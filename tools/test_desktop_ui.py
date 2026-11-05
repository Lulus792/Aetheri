"""Native SDL desktop input/pixels with actual Settings and a controlled launch owner."""
import gzip
import hashlib
import json
import os
from pathlib import Path
import re
import signal
import socket
import subprocess
import sys
import tempfile
import time


def main():
    server_binary, desktop_binary, settings_binary, font, build = [Path(x).resolve() for x in sys.argv[1:]]
    assert os.getuid() != 0
    pins = json.loads((Path(__file__).resolve().parents[1] / "spec/ui-inputs.json").read_text())
    assert hashlib.sha256(font.read_bytes()).hexdigest() == pins["font"]["sha256"]
    root = Path(tempfile.mkdtemp(prefix="desktop-ui-", dir=build))
    runtime, cache, captures = [root / x for x in ("runtime", "cache", "captures")]
    for p in (runtime, cache, captures): p.mkdir(mode=0o700)
    env = {**os.environ, "XDG_RUNTIME_DIR": str(runtime), "WAYLAND_DISPLAY": "desktop-ui", "LIBGL_ALWAYS_SOFTWARE": "1",
           "GALLIUM_DRIVER": "llvmpipe", "LP_NUM_THREADS": "2"}
    for key in ("DISPLAY", "WAYLAND_SOCKET", "SDL_VIDEO_DRIVER", "SDL_RENDER_DRIVER"): env.pop(key, None)
    control, issuer = socket.socketpair(socket.AF_UNIX, socket.SOCK_SEQPACKET)
    server_fd, client_fd = socket.socketpair()
    launch_owner, launch_client = socket.socketpair(socket.AF_UNIX, socket.SOCK_SEQPACKET)
    control.settimeout(5); launch_owner.settimeout(7)
    processes, streams, images, cases = [], [], [], []
    output_heights=[1080,1440,2160]

    def start(binary, name, args, pass_fds=(), child_env=None):
        f = (root / (name + ".log")).open("wb"); streams.append(f)
        p = subprocess.Popen([str(binary), *map(str, args)], env=child_env or env, pass_fds=pass_fds,
                             stdin=subprocess.DEVNULL, stdout=f, stderr=subprocess.STDOUT)
        processes.append(p); return p

    def log(name="desktop"): return (root / (name + ".log")).read_text(errors="replace")

    def wait(predicate, label, live=()):
        deadline = time.monotonic() + 9
        while not predicate():
            assert all(p.poll() is None for p in live), (label, root)
            assert time.monotonic() < deadline, (label, root)
            time.sleep(0.01)

    def event(value):
        control.sendall(value.encode()); assert control.recv(32) == b"OK"

    def key(code, down): event(f"K {code} {int(down)}")
    def tap(code): key(code, True); key(code, False)

    def shortcut():
        before = log().count("FOCUS 0\n")
        stable_line = f"STABLE 0 1920x{output_heights[0]}\n"
        stable = log().count(stable_line)
        event("M 100 100"); key(125, True); tap(57); key(125, False)
        wait(lambda: log().count("FOCUS 0\n") > before,"new search focus",(server,desktop))
        wait(lambda: log().count(stable_line) > stable,"new search allocation",(server,desktop))

    def typing(value):
        codes = dict(zip("abcdefghijklmnopqrstuvwxyz", (30,48,46,32,18,33,34,35,23,36,37,38,50,49,24,25,16,19,31,20,22,47,17,45,21,44)))
        for c in value: tap(codes[c])

    def click(x, y):
        event(f"M {x} {y}"); event("B 272 1"); event("B 272 0")

    def click_control(output, kind):
        rows = re.findall(r"VIEW (\d+) output="+str(output)+r" search=([01]) size=(\d+)x(\d+)",log())
        frame, expanded, _, height = map(int,rows[-1])
        x, y = map(float,re.findall(r"CONTROL "+str(frame)+" "+str(output)+" "+str(kind)+r" 0 ([\d.]+) ([\d.]+)",log())[0])
        gx=(0,1920,4480)[output]; gy=0 if expanded else output_heights[output]-height
        click(x+gx,y+gy)

    sequence = 0

    def frames_done():
        value=log()
        for output in range(3):
            views=re.findall(r"VIEW (\d+) output="+str(output)+" ",value)
            frames=re.findall(r"DRAWN (\d+) output="+str(output)+" ",value)
            if not views or not frames or int(frames[-1])<int(views[-1]):return False
        return True

    def snapshot(label, dock=True):
        nonlocal sequence
        wait(frames_done,"compositor frame observations: "+label,(server,desktop))
        sequence += 1; server.send_signal(signal.SIGUSR1)
        wait(lambda: all(f"CAPTURE {sequence} {i}\n" in log("server") for i in range(3)), label, (server, desktop))
        for i in range(3):
            p = captures / f"capture-{sequence}-output-{i}.ppm"
            raw = p.read_bytes(); magic, dimensions, peak, pixels = raw.split(b"\n", 3); w, h = map(int, dimensions.split())
            assert magic == b"P6" and peak == b"255" and len(pixels) == w*h*3
            # The real UI must contain foreground text/geometry, not a solid strip.
            if i == 0:
                assert len({pixels[j:j+3] for j in range(0, len(pixels), 3)}) > 20
                if dock:
                    offset=((h-60)*w+760)*3
                    assert tuple(pixels[offset:offset+3]) in ((38,46,63),(44,72,108),(0,0,0)), (label,"missing dock",tuple(pixels[offset:offset+3]))
                    outside=((h-60)*w+10)*3
                    assert tuple(pixels[outside:outside+3]) == (16,20,28), (label,"transparent dock/background alpha",tuple(pixels[outside:outside+3]))
            packed = p.with_suffix(".ppm.gz"); packed.write_bytes(gzip.compress(raw, mtime=0)); p.unlink()
            images.append({"case": label, "output": i, "file": packed.name, "sha256": hashlib.sha256(raw).hexdigest()})
        cases.append(label)

    try:
        server = start(server_binary, "server", ("--headless", env["WAYLAND_DISPLAY"], captures, issuer.fileno(), "--shell-fd", server_fd.fileno()),
                       (issuer.fileno(), server_fd.fileno()))
        issuer.close(); server_fd.close()
        wait(lambda: "READY " in log("server"), "compositor", (server,))
        desktop = start(desktop_binary, "desktop", ("--font", font, "--cache", cache, "--launcher-fd", launch_client.fileno(), "--diagnostics"),
                        (client_fd.fileno(), launch_client.fileno()), {**env, "WAYLAND_SOCKET": str(client_fd.fileno())})
        client_fd.close(); launch_client.close()
        wait(lambda: "STABLE 2 3840x84\n" in log(), "three native SDL dock surfaces", (server, desktop))
        snapshot("native compact dock on three outputs")
        before=log().count("FOCUS 1\n");click_control(1,1)
        wait(lambda: log().count("FOCUS 1\n")>before and "STABLE 1 2560x1440\n" in log(),"pointer opens second-output search",(server,desktop))
        before=log().count("FOCUS 0\n");old_stable=log().count("STABLE 1 2560x84\n");click_control(0,1)
        wait(lambda: log().count("FOCUS 0\n")>before and log().count("STABLE 1 2560x84\n")>old_stable,"pointer moves search between outputs",(server,desktop))
        snapshot("pointer search moves between actual outputs")
        count=log().count("STABLE 0 1920x84\n");click(5,5)
        wait(lambda: log().count("STABLE 0 1920x84\n")>count,"outside click closes search",(server,desktop))
        shortcut()
        wait(lambda: "output=0 search=1 size=1920x1080" in log(), "global search", (server, desktop))
        typing("zzzz")
        wait(lambda: "query=zzzz " in log(), "actual search text input", (server, desktop))
        snapshot("empty search result")
        for _ in range(4): tap(14)
        typing("settings")
        wait(lambda: "query=settings " in log(), "filter Settings", (server, desktop))
        snapshot("Settings search result")
        output_heights[0]=800
        event("O 0 1920 800")
        wait(lambda:"STABLE 0 1920x800\n" in log(),"short output before app creation",(server,desktop))
        tap(28)
        assert launch_owner.recv(16) == b"L3S"
        # Hold the reply: repeated Enter must not repeat an uncertain request.
        tap(28); launch_owner.settimeout(0.2)
        try: launch_owner.recv(16); raise AssertionError("duplicate pending launch")
        except TimeoutError: pass
        launch_owner.settimeout(7)
        settings = start(settings_binary, "settings", ("--font", font, "--cache", cache, "--diagnostics"))
        stable = log().count("STABLE 0 1920x84\n")
        launch_owner.sendall(b"A3S")
        wait(lambda: "windows=1" in log() and "LAYOUT " in log("settings"), "actual Settings window", (server, desktop, settings))
        wait(lambda:bool(re.search(r"VIEW .* size=1100x636\n",log("settings"))),
             "initial Settings respects dock and placement bounds",(server,desktop,settings))
        wait(lambda:all(re.findall(r"VIEW \d+ output="+str(i)+r" .* windows=(\d+)",log())[-1]=='1' for i in range(3)),
             "all dock window observations painted",(server,desktop,settings))
        wait(lambda: log().rfind("search=0") > log().rfind("search=1"), "collapse search", (server, desktop, settings))
        wait(lambda: log().count("STABLE 0 1920x84\n") > stable,"new compact allocation",(server,desktop,settings))
        snapshot("Settings and dock coexist")
        key(125,True);tap(103);key(125,False)
        wait(lambda:bool(re.search(r"VIEW .* size=1920x716\n",log("settings"))),
             "global keyboard maximize uses work area",(server,desktop,settings))
        snapshot("keyboard maximized Settings leaves dock visible")
        key(125,True);tap(108);key(125,False)
        wait(lambda:len(re.findall(r"VIEW .* size=1100x636\n",log("settings")))>=2,
             "keyboard restore retains normal client size",(server,desktop,settings))
        snapshot("keyboard restored Settings retains initial bounds")
        shortcut(); tap(15); tap(15); tap(15); tap(28)
        wait(lambda: "ACTIVATED 1 1\n" in log(), "keyboard task activation", (server, desktop, settings))
        key(29,True);tap(16);key(29,False)
        assert settings.wait(timeout=7) == 0, log("settings")
        launch_owner.sendall(b"X3S")
        wait(lambda: "LAUNCH_REPLY X S\n" in log(), "tracked process exit", (server, desktop))
        shortcut(); tap(64)
        wait(lambda: "search=1" in log(), "search again", (server, desktop))
        tap(28); assert launch_owner.recv(16) == b"L3S"; launch_owner.sendall(b"E3S")
        wait(lambda: "LAUNCH_REPLY E S\n" in log(), "definite launch failure", (server, desktop))
        tap(65)
        wait(lambda:bool(re.search(r"VIEW \d+ output=0 search=1 .* pending=0 windows=0 de=1 contrast=1",log())),
             "German high-contrast error frame",(server,desktop))
        snapshot("German search with opaque high contrast and launch error")
        count=log().count("STABLE 0 1920x84\n");tap(1)
        wait(lambda:log().count("STABLE 0 1920x84\n")>count,"close before output resize",(server,desktop))
        output_heights[0]=320
        event("O 0 320 320")
        wait(lambda: "STABLE 0 320x84\n" in log(),"compact dock reflow",(server,desktop))
        before=log().count("FOCUS 0\n");click_control(0,1)
        wait(lambda: log().count("FOCUS 0\n")>before and "STABLE 0 320x320\n" in log(),"small output search",(server,desktop))
        snapshot("German search reflows on a 320px output",dock=False)
        count=log().count("STABLE 0 1920x84\n")
        tap(1);output_heights[0]=1080;event("O 0 1920 1080")
        wait(lambda: log().count("STABLE 0 1920x84\n")>count,"restored output dock",(server,desktop))
        time.sleep(0.3); before = log().count("VIEW "); time.sleep(0.4); assert log().count("VIEW ") == before
        cases.append("event-driven idle, keyboard search/edit/selection, actual Settings start/activation/close, pending duplicate suppression and explicit launch failure")
        shortcut();key(29,True);key(42,True);tap(16);key(42,False);key(29,False)
        launch_owner.settimeout(.15)
        try:launch_owner.recv(16);raise AssertionError("terminal close chord must not close desktop search")
        except TimeoutError:pass
        assert desktop.poll() is None and server.poll() is None
        launch_owner.settimeout(7)
        cases.append("Ctrl+Shift+Q does not alias the desktop Ctrl+Q action")
        key(29,True);tap(16);key(29,False)
        assert launch_owner.recv(16)==b"Q30"
        assert desktop.poll() is None and server.poll() is None
        tap(28);launch_owner.settimeout(.15)
        try:launch_owner.recv(16);raise AssertionError("new launch after close fence")
        except TimeoutError:pass
        cases.append("user close requests owner shutdown once and fences new launches before retirement")
        desktop.terminate(); assert desktop.wait(timeout=7) == 0, log()
        assert server.wait(timeout=7) == 0, log("server")
        for name in ("desktop", "server", "settings"):
            assert not re.search(r"AddressSanitizer|LeakSanitizer|runtime error:", log(name)), log(name)
        for case in ("eof", "fd", "old-version"):
            for s in (control,issuer,server_fd,client_fd,launch_owner,launch_client):s.close()
            control,issuer=socket.socketpair(socket.AF_UNIX,socket.SOCK_SEQPACKET);control.settimeout(5)
            server_fd,client_fd=socket.socketpair();launch_owner,launch_client=socket.socketpair(socket.AF_UNIX,socket.SOCK_SEQPACKET)
            s=start(server_binary,case+"-server",("--headless",env["WAYLAND_DISPLAY"],"-",issuer.fileno(),"--shell-fd",server_fd.fileno()),(issuer.fileno(),server_fd.fileno()))
            issuer.close();server_fd.close()
            wait(lambda:"READY " in log(case+"-server"),"denial fixture server",(s,))
            d=start(desktop_binary,case+"-desktop",("--font",font,"--cache",cache,"--launcher-fd",launch_client.fileno(),"--diagnostics"),
                    (client_fd.fileno(),launch_client.fileno()),{**env,"WAYLAND_SOCKET":str(client_fd.fileno())})
            client_fd.close();launch_client.close()
            wait(lambda:"STABLE 2 3840x84\n" in log(case+"-desktop"),"denial fixture UI",(s,d))
            if case=="eof":launch_owner.close()
            elif case=="old-version":launch_owner.sendall(b"A2S")
            else:
                import array
                fds=os.pipe()
                try:launch_owner.sendmsg([b"A3S"],[(socket.SOL_SOCKET,socket.SCM_RIGHTS,array.array("i",[fds[0]]))])
                finally:
                    for fd in fds:os.close(fd)
            assert d.wait(timeout=7)==1 and s.wait(timeout=7)==1
            assert "launcher connection lost or invalid" in log(case+"-desktop")
            assert not re.search(r"AddressSanitizer|LeakSanitizer|runtime error:",log(case+"-desktop")+log(case+"-server"))
        cases.append("lost launcher and incoming ancillary FD fail closed without reconnect or launch replay")
        result = {"result": "passed", "scope": "native private SDL/Wayland and actual Settings; controlled test launch owner, not production session admission",
                  "cases": cases, "images": images, "artifacts": str(root)}
        (root / "result.json").write_text(json.dumps(result,indent=2)+"\n");print(json.dumps(result))
    finally:
        for p in reversed(processes):
            if p.poll() is None:
                p.terminate()
                try: p.wait(timeout=3)
                except subprocess.TimeoutExpired: p.kill();p.wait(timeout=3)
        for f in streams: f.close()
        for s in (control, issuer, server_fd, client_fd, launch_owner, launch_client): s.close()
        print("Desktop UI evidence:",root)


if __name__ == "__main__":main()
