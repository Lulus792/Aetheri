"""Actual Settings consumer/input/pixels on the private compositor; no SDL event injection."""
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
    server_binary, app_binary, font, build = [Path(p).resolve() for p in sys.argv[1:]]
    if os.getuid()==0:
        raise RuntimeError("private native UI test must be unprivileged")
    config=json.loads((Path(__file__).resolve().parents[1]/"spec/ui-inputs.json").read_text())
    assert hashlib.sha256(font.read_bytes()).hexdigest()==config["font"]["sha256"]
    root=Path(tempfile.mkdtemp(prefix="settings-ui-",dir=build))
    runtime,captures=root/"runtime",root/"captures"
    runtime.mkdir(mode=0o700); captures.mkdir(mode=0o700)
    cache=root/"shader-cache"; cache.mkdir(mode=0o700)
    env={**os.environ,"XDG_RUNTIME_DIR":str(runtime),"WAYLAND_DISPLAY":"settings-private",
         "LIBGL_ALWAYS_SOFTWARE":"1","GALLIUM_DRIVER":"llvmpipe","LP_NUM_THREADS":"2"}
    for key in ("DISPLAY","WAYLAND_SOCKET","SDL_VIDEO_DRIVER","SDL_RENDER_DRIVER"):
        env.pop(key,None)
    controller,issuer=socket.socketpair(socket.AF_UNIX,socket.SOCK_SEQPACKET)
    controller.settimeout(4)
    processes,streams,images,cases=[],[],[],[]

    def start(binary,name,*args,pass_fds=(),extra_env=None):
        stream=(root/(name+".log")).open("wb"); streams.append(stream)
        p=subprocess.Popen([str(binary),*map(str,args)],env=extra_env or env,stdout=stream,
                           stderr=subprocess.STDOUT,stdin=subprocess.DEVNULL,pass_fds=pass_fds)
        processes.append(p); return p

    def log(name="app"):
        return (root/(name+".log")).read_text(errors="replace")

    def wait(predicate,label):
        end=time.monotonic()+12
        while not predicate():
            assert all(p.poll() is None for p in processes), (label,[(p.pid,p.returncode) for p in processes],log()[-6000:],log("server")[-3000:])
            assert time.monotonic()<end,(label,log()[-6000:],log("server")[-3000:])
            time.sleep(0.01)

    def event(command):
        controller.sendall(command.encode()); assert controller.recv(32)==b"OK",command

    def key(code):
        event(f"K {code} 1"); event(f"K {code} 0")

    def view():
        content=log()
        frames=re.findall(r"^LAYOUT (\d+)$",content,re.M)
        if not frames: return None
        frame=frames[-1]
        match=re.search(r"^VIEW "+frame+r" (.+)$",content,re.M)
        state=dict(item.split("=") for item in match[1].split())
        state["frame"]=int(frame)
        state["controls"]={int(m[0]):tuple(map(float,m[1:])) for m in re.findall(
            r"^CONTROL "+frame+r" (\d+) ([\d.-]+) ([\d.-]+) ([\d.-]+) ([\d.-]+)$",content,re.M)}
        return state

    def state(**expected):
        v=view()
        return v is not None and all(v.get(k)==str(value) for k,value in expected.items())

    def click_control(control):
        before=view()["frame"]
        x,y,low,high=view()["controls"][control]
        assert high>low, (control,view())
        # First mapped window is at (40,40) in the compositor, independently captured below.
        event(f"M {40+x} {40+y}"); event("B 272 1"); event("B 272 0")
        wait(lambda:view()["frame"]>before,"actual pointer delivery")

    def capture(label,bg):
        sequence=len(cases)+1
        server.send_signal(signal.SIGUSR1)
        wait(lambda:all(f"CAPTURE {sequence} {i}\n" in log("server") for i in range(3)),label)
        for i in range(3):
            path=captures/f"capture-{sequence}-output-{i}.ppm"
            raw=path.read_bytes(); magic,dimensions,peak,pixels=raw.split(b"\n",3)
            w,h=map(int,dimensions.split())
            assert magic==b"P6" and peak==b"255" and len(pixels)==w*h*3
            def pixel(x,y): return tuple(pixels[(y*w+x)*3:(y*w+x)*3+3])
            assert pixel(10,10)==(16,20,28)
            if i==0:
                assert pixel(50,50)==bg,(label,pixel(50,50),bg)
                # Actual antialiased title ink, not merely an empty app rectangle.
                ink=sum(pixel(x,y)!=bg for y in range(64,110) for x in range(65,300))
                assert ink>200,(label,ink)
            else: assert pixel(80,80)==(16,20,28)
            packed=path.with_suffix(".ppm.gz")
            with packed.open("xb") as f: f.write(gzip.compress(raw,mtime=0))
            assert gzip.decompress(packed.read_bytes())==raw
            path.unlink()
            images.append({"case":label,"output":i,"file":packed.name,"sha256":hashlib.sha256(raw).hexdigest()})
        cases.append(label)

    try:
        server=start(server_binary,"server","--headless",env["WAYLAND_DISPLAY"],captures,issuer.fileno(),
                     pass_fds=(issuer.fileno(),),extra_env={**env,"WAYLAND_DEBUG":"server"})
        issuer.close()
        wait(lambda:"READY " in log("server"),"compositor ready")
        app=start(app_binary,"app","--font",font,"--cache",cache,"--diagnostics")
        wait(lambda:state(page=0) and "MAP 0\n" in log("server"),"first actual native view")
        observed=re.findall(r"^DISPLAY (\d+) (\d+) (\d+) ([\d.]+) ([\d.]+) (\d+)$",log(),re.M)
        assert {(int(r[1]),int(r[2])) for r in observed}=={(1920,1080),(2560,1440),(3840,2160)},observed
        assert "OBSERVATION 1 3 0\n" in log()
        maps=Path(f"/proc/{app.pid}/maps").read_text()
        for library in ("libSDL3.so","libSDL3_ttf.so","libfreetype.so","libharfbuzz.so"):
            assert any(library in line and str(font.parent.parent.parent/"lib") in line for line in maps.splitlines()),library
        assert "READY wayland opengles2 " in log() and "GL_RENDERER llvmpipe" in log(),log()
        assert "libEGL.so" in maps and "libvulkan.so" not in maps
        (root/"loaded-maps.txt").write_text(maps)
        capture("actual three-output observation",(19,23,31))
        click_control(102); wait(lambda:state(selected=2),"third display selection")
        before=log().count("OBSERVATION "); key(63)
        wait(lambda:log().count("OBSERVATION ")>before,"F5 refresh")
        assert state(selected=2)
        # Tab from the selected third row to refresh, then wrap through close and tabs.
        key(15); wait(lambda:state(focus=4),"keyboard focus next")
        key(15); wait(lambda:state(focus=5),"keyboard wrap")
        key(15); wait(lambda:state(focus=1),"display tab focus")
        key(15); key(28); wait(lambda:state(page=1,focus=2),"keyboard opens appearance")
        click_control(11); wait(lambda:state(theme=1),"real light theme")
        capture("light theme through native pointer",(242,244,248))
        click_control(10); wait(lambda:state(theme=0),"dark theme")
        # Reach 200% text with keyboard, including automatic focused-control scrolling.
        for _ in range(5): key(15)
        wait(lambda:state(focus=22),"text-size control keyboard traversal")
        key(28); wait(lambda:state(text=200),"200 percent text")
        assert float(view()["scroll"])>0
        click_control(3); wait(lambda:state(page=2),"language navigation")
        click_control(31); wait(lambda:state(de=1),"German labels")
        capture("German and 200 percent text",(19,23,31))
        # Native bottom/right resize request: actual serial, compositor configure and SDL surface.
        resizes=log("server").count(".resize(")
        event("M 1137 797"); event("B 272 1")
        # The driver ACK flushes input, not the client's later native resize RPC.
        # Observe actual server receipt before moving the admitted resize grab.
        wait(lambda:log("server").count(".resize(")>resizes,"native resize request received")
        event("M 797 517")
        wait(lambda:state(size="760x480"),"native minimum-size configure")
        event("B 272 0")
        capture("minimum window at 200 percent",(19,23,31))
        click_control(2); wait(lambda:state(page=1),"appearance after resize")
        # All controls remain keyboard reachable, including below the viewport.
        for _ in range(7): key(15)
        wait(lambda:state(focus=22),"last preference reachable at minimum size")
        assert float(view()["scroll"])>0
        # Quiescence: explicit bounded idle observation, not a constant render loop.
        time.sleep(0.3)
        before=view()["frame"]
        def ticks():
            fields=Path(f"/proc/{app.pid}/stat").read_text().rsplit(")",1)[1].split()
            return int(fields[11])+int(fields[12])
        initial=ticks(); time.sleep(1)
        assert view()["frame"]==before,(before,view())
        delta=ticks()-initial
        assert delta<=2,delta
        event("K 29 1"); key(16); event("K 29 0")
        assert app.wait(timeout=8)==0,log()
        processes.remove(app)
        assert "CLOSED frames=" in log()
        wait(lambda:"UNMAP 0\n" in log("server"),"native view retired")
        # No X11/offscreen fallback with an unavailable explicit Wayland display.
        unavailable=start(app_binary,"unavailable","--font",font,"--cache",cache,extra_env={**env,"WAYLAND_DISPLAY":"absent","DISPLAY":":199"})
        assert unavailable.wait(timeout=8)==1,log("unavailable")
        processes.remove(unavailable)
        bad_font=start(app_binary,"bad-font","--font",root/"missing-font.ttf","--cache",cache)
        assert bad_font.wait(timeout=8)==1,log("bad-font")
        processes.remove(bad_font)
        bad_cache=start(app_binary,"bad-cache","--font",font,"--cache",root/"missing-cache")
        assert bad_cache.wait(timeout=8)==2,log("bad-cache")
        processes.remove(bad_cache)
        server.terminate(); assert server.wait(timeout=8)==0,log("server")
        processes.remove(server)
        result={"result":"passed","scope":"private native development view; no normal app or system-setting admission",
                "cases":cases,"checks":["three actual outputs","retained selection on refresh","native keyboard/pointer",
                "per-window preferences","minimum size/200%/scroll","event-driven idle","clean close","no display fallback","font failure cleanup"],
                "idle_cpu_ticks":delta,"idle_seconds":1,
                "shader_cache_files":sum(p.is_file() for p in cache.rglob("*")),
                "shader_cache_bytes":sum(p.stat().st_size for p in cache.rglob("*") if p.is_file()),"images":images}
        (root/"result.json").write_text(json.dumps(result,indent=2)+"\n")
        print(json.dumps({**result,"artifacts":str(root)}))
    finally:
        for p in reversed(processes):
            if p.poll() is None:
                p.terminate()
                try:p.wait(timeout=4)
                except subprocess.TimeoutExpired:p.kill();p.wait(timeout=4)
        for stream in streams:stream.close()
        controller.close();issuer.close()
        print("Settings evidence:",root)


if __name__=="__main__":
    main()
