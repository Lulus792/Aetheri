"""Real terminal/PTY and independent libwayland peers; no injected SDL events."""
import gzip
import hashlib
import json
import os
from pathlib import Path
import re
import select
import signal
import socket
import subprocess
import sys
import tempfile
import time


def main():
    server_bin, terminal_bin, shell_bin, peer_bin, api_bin, font, build = map(lambda x: Path(x).resolve(), sys.argv[1:])
    assert os.getuid() != 0
    root = Path(tempfile.mkdtemp(prefix="terminal-clipboard-", dir=build))
    runtime, cache, captures = [root / n for n in ("runtime", "cache", "captures")]
    for p in (runtime, cache, captures):
        p.mkdir(mode=0o700)
    env = {**os.environ, "XDG_RUNTIME_DIR": str(runtime), "WAYLAND_DISPLAY": "terminal-clipboard",
           "LIBGL_ALWAYS_SOFTWARE": "1", "GALLIUM_DRIVER": "llvmpipe", "LP_NUM_THREADS": "2"}
    for k in ("DISPLAY", "WAYLAND_SOCKET", "SDL_VIDEO_DRIVER", "SDL_RENDER_DRIVER"):
        env.pop(k, None)
    controller, issuer = socket.socketpair(socket.AF_UNIX, socket.SOCK_SEQPACKET)
    controller.settimeout(4)
    children, streams, pins, cases, images = [], [], [], [], []
    binaries = {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in (server_bin, terminal_bin, shell_bin, peer_bin, api_bin)}

    def start(binary, name, *args, pass_fds=()):
        f = (root / (name + ".log")).open("wb")
        streams.append(f)
        p = subprocess.Popen([str(binary), *map(str, args)], cwd=root, env=env, stdin=subprocess.PIPE,
                             stdout=f, stderr=subprocess.STDOUT, pass_fds=pass_fds)
        children.append(p)
        pins.append(os.pidfd_open(p.pid))
        return p

    def log(name="terminal"):
        return (root / (name + ".log")).read_text(errors="replace")

    def wait(predicate, label, timeout=8):
        end = time.monotonic() + timeout
        while not predicate():
            assert all(p.poll() is None for p in children), (label, root, [(p.pid, p.returncode) for p in children])
            assert time.monotonic() < end, (label, root, log()[-3500:])
            time.sleep(.01)

    def event(c):
        controller.sendall(c.encode())
        assert controller.recv(32) == b"OK", c

    def key(c, down):
        event(f"K {c} {int(down)}")

    def tap(c):
        key(c, True)
        key(c, False)

    def control(c, shift=True):
        key(29, True)
        if shift:
            key(42, True)
        tap(c)
        if shift:
            key(42, False)
        key(29, False)

    def fields(prefix):
        rows = [x for x in log().splitlines() if x.startswith(prefix)]
        return {k: int(v) for k, v in re.findall(r"(\w+)=(\d+)", rows[-1])} if rows else {}

    def accepted():
        rows = re.findall(r"^INPUT (\d+)$", log(), re.M)
        return int(rows[-1]) if rows else 0

    codes = dict(zip("abcdefghijklmnopqrstuvwxyz", (30,48,46,32,18,33,34,35,23,36,37,38,50,49,24,25,16,19,31,20,22,47,17,45,21,44)))
    codes.update({" ":57, "-":12, ".":52, "/":53, "\n":28})
    codes.update(dict(zip("1234567890", range(2,12))))

    def typing(text):
        target = accepted() + len(text.encode())
        for c in text:
            tap(codes[c])
        wait(lambda: accepted() == target, "native typed command")

    def focus_terminal():
        event("M 500 200"); event("B 272 1"); event("B 272 0")
        wait(lambda: fields("CLIPBOARD state ").get("focused") == 1, "terminal real keyboard focus")

    def peer_command(p, name, c):
        count = log(name).count(f"CLIPBOARD_SYNC {c}\n")
        p.stdin.write(c.encode()); p.stdin.flush()
        wait(lambda: log(name).count(f"CLIPBOARD_SYNC {c}\n") > count, "peer native " + c)

    def focus_peer(name):
        event("M 4580 100"); event("B 272 1"); event("B 272 0")
        wait(lambda: log(name).rfind("KEYBOARD_ENTER 0\n") > log(name).rfind("KEYBOARD_LEAVE\n"), "peer actual focus")
        wait(lambda: fields("CLIPBOARD state ").get("focused") == 0, "terminal actual keyboard leave")

    def peer(name, payload=None, plain=False):
        if payload is None:
            p = start(peer_bin, name, "--clipboard-stall")
        else:
            path = root / (name + ".payload")
            path.write_bytes(payload)
            p = start(peer_bin, name, "--clipboard-plain" if plain else "--clipboard-file", path)
        wait(lambda: "FRAME 1 " in log(name), "independent peer mapped")
        p.stdin.write(b"df"); p.stdin.flush()
        wait(lambda: re.search(r"^FRAME 0 \d+ \d+ 1$", log(name), re.M), "peer fullscreen on third output")
        focus_peer(name)
        return p

    def close_peer(p):
        p.stdin.write(b"q"); p.stdin.flush()
        assert p.wait(timeout=6) == 0, root
        p.stdin.close(); children.remove(p)
        focus_terminal()

    def publish(p, name):
        focus_peer(name); peer_command(p, name, "C"); focus_terminal()

    def paste_review():
        control(47)
        wait(lambda: fields("VIEW ").get("review") == 1, "actual complete paste review")
        frame=int(re.findall(r"^VIEW (\d+) .*\breview=1\b",log(),re.M)[-1])
        # A log of the submitted view is not the compositor's frame callback.
        # Only a later native event can confirm this actually drawn review.
        wait(lambda: fields("VIEW ").get("review")==1 and
             any(int(n)>=frame for n in re.findall(r"^DRAWN (\d+)$",log(),re.M)),
             "matching native paste-review frame")

    def capture(label):
        wait(lambda: re.search(r"^DRAWN \d+$", log(), re.M), "native frame")
        sequence = len(images) // 3 + 1
        server.send_signal(signal.SIGUSR1)
        wait(lambda: all(f"CAPTURE {sequence} {i}\n" in log("server") for i in range(3)), label)
        for i in range(3):
            p = captures / f"capture-{sequence}-output-{i}.ppm"
            raw = p.read_bytes(); magic, geometry, peak, pixels = raw.split(b"\n", 3)
            w, h = map(int, geometry.split())
            assert magic == b"P6" and peak == b"255" and len(pixels) == w*h*3
            packed = p.with_suffix(".ppm.gz"); packed.write_bytes(gzip.compress(raw, mtime=0)); p.unlink()
            images.append({"case": label, "output": i, "file": packed.name, "sha256": hashlib.sha256(raw).hexdigest()})

    try:
        server = start(server_bin, "server", "--headless", env["WAYLAND_DISPLAY"], captures, issuer.fileno(), pass_fds=(issuer.fileno(),))
        issuer.close()
        wait(lambda: "READY " in log("server"), "compositor")
        api=start(api_bin,"api");assert api.wait(timeout=8)==0,log("api");children.remove(api)
        api.stdin.close()
        cases.append("real adapter rejects wrong-thread/reentrant API calls and retains borrowed objects through repeated exact FD cleanup")
        terminal = start(terminal_bin, "terminal", "--shell", shell_bin, "--font", font, "--cache", cache, "--diagnostics")
        wait(lambda: "READY " in log() and fields("VIEW ").get("sequence", 0) > 1 and fields("VIEW ").get("x")==9, "terminal and real shell prompt")
        shell_pid = int(re.search(r"READY .* shell=(\d+)", log()).group(1))
        shell_pin = os.pidfd_open(shell_pid); pins.append(shell_pin)
        # Only direct, pinned original programs. Test commands are finite and waited
        # by the original shell; no numeric SID rediscovery or arbitrary jobs.
        focus_terminal()
        left, top, cw, ch = map(float, re.findall(r" grid_left=([\d.]+) grid_top=([\d.]+) cell_width=([\d.]+) cell_height=([\d.]+)$", log(), re.M)[-1])
        prompt_row=fields("VIEW ")["y"]
        event(f"M {40+left+.1} {40+top+(prompt_row+.5)*ch}"); event("B 272 1")
        event(f"M {40+left+8*cw} {40+top+(prompt_row+.5)*ch}"); event("B 272 0")
        control(46)
        wait(lambda: "CLIPBOARD copy result=0" in log(), "native copy")
        p = peer("reader", b"unused")
        peer_command(p, "reader", "R")
        row = re.findall(r"^CLIPBOARD_READ (.+)$", log("reader"), re.M)[-1].split()
        actual = bytes(int(x,16) for x in row[1:])
        assert int(row[0]) == len(actual) and actual == b"aetheri>", actual
        cases.append("actual selected terminal text exported to independent libwayland receiver")
        close_peer(p)

        payload = b"printf pasted > paste-proof"
        p = peer("single", payload); publish(p, "single")
        before = accepted(); control(47)
        wait(lambda: accepted() == before+len(payload), "single-line paste queued literally")
        assert not (root/"paste-proof").exists()
        tap(28); wait(lambda: (root/"paste-proof").exists() and (root/"paste-proof").read_bytes()==b"pasted", "only fresh Return executes pasted command")
        assert (root/"paste-proof").read_bytes() == b"pasted"
        cases.append("single-line paste remains literal until separate native Return")
        close_peer(p)

        payload = b"printf first > first-proof\r\nprintf second > second-proof\n"
        p = peer("multi", payload); publish(p, "multi"); before = accepted()
        paste_review(); capture("multiline paste review before any PTY input")
        assert accepted() == before and not (root/"first-proof").exists()
        tap(1); wait(lambda: fields("VIEW ").get("review") == 0, "Escape cancels review")
        paste_review(); tap(28)
        wait(lambda: fields("VIEW ").get("review") == 0, "default Return cancels review")
        assert accepted() == before and not (root/"second-proof").exists()
        paste_review(); control(28, shift=False)
        wait(lambda: (root/"second-proof").exists() and (root/"second-proof").read_bytes()==b"second", "fresh confirmed multiline paste reaches real shell")
        assert (root/"first-proof").read_bytes() == b"first" and (root/"second-proof").read_bytes() == b"second"
        assert accepted() == before+len(payload.replace(b"\r\n",b"\n"))
        cases.append("complete CRLF-normalized paste requires review; Escape/default Return cancel; fresh Ctrl+Enter confirms once")
        close_peer(p)

        p=peer("long-review",b"printf visible\n"*125,plain=True); publish(p,"long-review"); before=accepted()
        paste_review(); tap(107)
        wait(lambda:fields("VIEW ").get("preview_scroll",0)>0,"full long preview scrolls to end")
        for points in (20,24,32):
            tap(66); wait(lambda:fields("VIEW ").get("points")==points,"review text size change")
        tap(64); tap(65)
        wait(lambda:fields("VIEW ").get("german")==1 and fields("VIEW ").get("contrast")==1,"German high-contrast review")
        capture("complete scrollable review at 200 percent German text")
        tap(1); wait(lambda:fields("VIEW ").get("review")==0,"long review cancels without input")
        assert accepted()==before
        tap(66);tap(64);tap(65)
        wait(lambda:fields("VIEW ").get("points")==16 and fields("VIEW ").get("german")==0,"restore native view preferences")
        cases.append("plain MIME fallback and full long-preview scrolling at 200 percent text; cancellation leaves PTY unchanged")
        close_peer(p)

        for name, payload, result in (("bad-utf8",b"a\xc0\xaf",3), ("nul",b"a\0b",4), ("escape",b"a\x1b[31m",4), ("bidi","a\u202eb".encode(),4)):
            p=peer(name,payload); publish(p,name); before=accepted(); count=log().count("CLIPBOARD validated=")
            control(47)
            wait(lambda: log().count("CLIPBOARD validated=")>count,"whole invalid payload rejected")
            assert fields("CLIPBOARD validated=").get("validated")==result and accepted()==before
            close_peer(p)
        cases.append("malformed UTF-8, embedded NUL, terminal escape and bidi controls reject atomically")
        p=peer("oversize",b"x"*4097); publish(p,"oversize"); before=accepted(); control(47)
        wait(lambda:"CLIPBOARD receive_end=4" in log(),"4097-byte transfer rejected before PTY input")
        assert accepted()==before; close_peer(p)
        cases.append("native 4096-byte receive bound rejects oversize without partial input")

        p=peer("slow"); publish(p,"slow"); before=accepted(); started=time.monotonic(); control(47)
        wait(lambda:"CLIPBOARD_STALLED" in log("slow"),"peer retains actual transfer FD")
        tap(64); wait(lambda:fields("VIEW ").get("german")==1,"F6 remains responsive during native receive")
        while "CLIPBOARD receive_end=5" not in log() and time.monotonic()-started<7:
            peer_command(p,"slow","T"); time.sleep(.12)
        elapsed=time.monotonic()-started
        assert "CLIPBOARD receive_end=5" in log() and 4.5<elapsed<7 and accepted()==before, (elapsed,root)
        peer_command(p,"slow","T")
        assert "CLIPBOARD_STALL_BROKEN" in log("slow")
        cases.append("trickled bytes do not extend total deadline; UI responds and timed-out receive closes original pipe")
        count=log("slow").count("CLIPBOARD_STALLED")
        control(47); wait(lambda:log("slow").count("CLIPBOARD_STALLED")>count,"second pending native transfer")
        focus_peer("slow"); peer_command(p,"slow","T")
        wait(lambda:fields("VIEW ").get("receiving")==0,"actual other-client focus cancels pending paste")
        assert accepted()==before
        cases.append("independent client focus loss cancels receive while pointer remains available")
        close_peer(p)

        # Test-owned finite programs supply content; shell/PTY and all native IO
        # remain the production binaries, not mocked clipboard functions.
        (root/"large.py").write_text("import os\nos.write(1,b'x'*150000+b'\\n')\n")
        typing("python3 large.py\n")
        wait(lambda:fields("VIEW ").get("history",0)>100,"large real PTY history")
        control(30); control(46)
        wait(lambda:fields("CLIPBOARD copy ").get("bytes",0)>65536,"large immutable exported snapshot")
        copied=fields("CLIPBOARD copy ")["bytes"]
        p=peer("blocked",b"unused")
        for _ in range(9): peer_command(p,"blocked","W")
        wait(lambda:fields("CLIPBOARD state ").get("sends")==8,"eight bounded outgoing transfers")
        assert fields("CLIPBOARD state ")["sources"]==1
        focus_terminal()
        (root/"mutate.py").write_text("import os\nos.write(1,b'\\x1b[3J\\x1b[2J\\x1b[HNEW\\n')\n")
        typing("python3 mutate.py\n")
        wait(lambda:fields("VIEW ").get("history")==0,"real model clears old history")
        control(30); control(46)
        wait(lambda:fields("CLIPBOARD state ").get("sources")==2,"old in-flight immutable snapshot survives source replacement")
        wait(lambda:fields("CLIPBOARD state ").get("sends")==0,"blocked send deadlines retire all original writers")
        assert fields("CLIPBOARD state ")["sources"]==1 and fields("CLIPBOARD state ")["bytes"]<1024
        focus_peer("blocked"); peer_command(p,"blocked","F")
        drains=[tuple(map(int,row)) for row in re.findall(r"^CLIPBOARD_DRAIN (\d+) (\d+) (\d+)$",log("blocked"),re.M)]
        assert len(drains)==9 and all(0<n<copied and eof==1 for _,n,eof in drains[:8]) and drains[8]==(8,0,1),drains
        peer_command(p,"blocked","R")
        row=re.findall(r"^CLIPBOARD_READ (.+)$",log("blocked"),re.M)[-1].split()
        current=bytes(int(x,16) for x in row[1:])
        assert b"NEW" in current and b"xxx" not in current,current
        cases.append("eight blocked exports share old immutable data; ninth rejected; deadlines release old source after replacement")
        close_peer(p)
        p=peer("close-pending"); publish(p,"close-pending"); control(47)
        wait(lambda:"CLIPBOARD_STALLED" in log("close-pending"),"native transfer pending at close")
        control(16)
        assert terminal.wait(timeout=8)==0 and select.select([shell_pin],[],[],0)[0]
        children.remove(terminal)
        peer_command(p,"close-pending","T")
        assert "CLIPBOARD_STALL_BROKEN" in log("close-pending")
        p.stdin.write(b"q");p.stdin.flush();assert p.wait(timeout=6)==0
        p.stdin.close();children.remove(p)
        cases.append("closing during pending receive joins the existing worker, closes transfer and waits original shell/PTY")
        server.terminate(); assert server.wait(timeout=8)==0; children.remove(server)
        assert not re.search(r"AddressSanitizer|LeakSanitizer|runtime error:","".join(log(p.stem) for p in root.glob("*.log")))
        assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest()==h for p,h in binaries.items())
        result={"result":"passed","cases":cases,"images":images,"binaries":binaries,"receive_deadline_seconds":elapsed,
                "scope":"private real Wayland/PTY clipboard; no production app admission or sensitive clipboard persistence"}
        (root/"result.json").write_text(json.dumps(result,indent=2)+"\n")
        print(json.dumps({**result,"artifacts":str(root)}))
    finally:
        # Signals target only enrolled original kernel objects, never a PID lookup.
        for fd in reversed(pins):
            try: signal.pidfd_send_signal(fd,signal.SIGKILL)
            except ProcessLookupError: pass
            finally: os.close(fd)
        for p in children:
            p.wait(timeout=5)
            if p.stdin: p.stdin.close()
        for f in streams: f.close()
        controller.close(); issuer.close()
        print("Terminal clipboard evidence:",root)


if __name__=="__main__":
    main()
