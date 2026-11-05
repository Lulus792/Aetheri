"""Native text-input-v3 fixture -> actual SDL -> original shell/PTY.

The controlled producer is test-only, not installed compositor/IME acceptance.
"""
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
    server_bin, terminal_bin, shell_bin, peer_bin, font, build=map(lambda x:Path(x).resolve(),sys.argv[1:])
    assert os.getuid()!=0
    root=Path(tempfile.mkdtemp(prefix="terminal-compose-",dir=build))
    runtime,cache,captures=[root/n for n in ("runtime","cache","captures")]
    for p in (runtime,cache,captures):p.mkdir(mode=0o700)
    env={**os.environ,"XDG_RUNTIME_DIR":str(runtime),"WAYLAND_DISPLAY":"composition","LIBGL_ALWAYS_SOFTWARE":"1","GALLIUM_DRIVER":"llvmpipe","LP_NUM_THREADS":"2"}
    for k in ("DISPLAY","WAYLAND_SOCKET","SDL_VIDEO_DRIVER","SDL_RENDER_DRIVER"):env.pop(k,None)
    controller,issuer=socket.socketpair(socket.AF_UNIX,socket.SOCK_SEQPACKET);controller.settimeout(4)
    children,streams,pins,cases,images=[],[],[],[],[]
    binaries={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in (server_bin,terminal_bin,shell_bin,peer_bin)}
    def start(binary,name,*args,pass_fds=()):
        f=(root/(name+".log")).open("wb");streams.append(f)
        p=subprocess.Popen([str(binary),*map(str,args)],cwd=root,env=env,stdin=subprocess.PIPE,stdout=f,stderr=subprocess.STDOUT,pass_fds=pass_fds)
        children.append(p);pins.append(os.pidfd_open(p.pid));return p
    def log(name="terminal"):return (root/(name+".log")).read_text(errors="replace")
    def wait(predicate,label):
        end=time.monotonic()+8
        while not predicate():
            assert all(p.poll() is None for p in children),(label,root)
            assert time.monotonic()<end,(label,root,log()[-2500:]);time.sleep(.01)
    def event(c):controller.sendall(c.encode());assert controller.recv(32)==b"OK",(c,root)
    def key(c,down):event(f"K {c} {int(down)}")
    def tap(c):key(c,True);key(c,False)
    def control(c,shift=False):
        key(29,True)
        if shift:key(42,True)
        tap(c)
        if shift:key(42,False)
        key(29,False)
    def view():
        rows=re.findall(r"^VIEW (\d+) (.+)$",log(),re.M)
        if not rows:return {}
        frame,fields=rows[-1];return {"frame":int(frame),**{k:int(v) for k,v in re.findall(r"(\w+)=(\d+)",fields)}}
    def accepted():
        rows=re.findall(r"^INPUT (\d+)$",log(),re.M);return int(rows[-1]) if rows else 0
    codes=dict(zip("abcdefghijklmnopqrstuvwxyz",(30,48,46,32,18,33,34,35,23,36,37,38,50,49,24,25,16,19,31,20,22,47,17,45,21,44)))
    codes.update({" ":57,"-":12,"'":40,"\n":28,".":52})
    def typing(text):
        target=accepted()+len(text.encode())
        for c in text:
            if c==">":key(42,True);tap(52);key(42,False)
            else:tap(codes[c])
        wait(lambda:accepted()==target,"actual ordinary native text input")
    def draft(command,size):
        count=log("server").count(f"IME produced={command}\n");event(f"I {command}")
        wait(lambda:log("server").count(f"IME produced={command}\n")>count,"real native IME protocol event")
        wait(lambda:view().get("composition")==size,"native preedit snapshot displayed")
    def capture(label):
        wait(lambda:f"DRAWN {view().get('frame')}\n" in log(),"latest compositor-observed frame")
        seq=len(images)//3+1;server.send_signal(signal.SIGUSR1)
        wait(lambda:all(f"CAPTURE {seq} {i}\n" in log("server") for i in range(3)),label)
        for i in range(3):
            p=captures/f"capture-{seq}-output-{i}.ppm";raw=p.read_bytes();magic,geometry,peak,pixels=raw.split(b"\n",3)
            w,h=map(int,geometry.split());assert magic==b"P6" and peak==b"255" and len(pixels)==w*h*3
            packed=p.with_suffix(".ppm.gz");packed.write_bytes(gzip.compress(raw,mtime=0));p.unlink()
            images.append({"case":label,"output":i,"file":packed.name,"sha256":hashlib.sha256(raw).hexdigest()})
    try:
        server=start(server_bin,"server","--headless",env["WAYLAND_DISPLAY"],captures,issuer.fileno(),pass_fds=(issuer.fileno(),));issuer.close()
        wait(lambda:"READY " in log("server"),"compositor ready")
        event("I on")
        terminal=start(terminal_bin,"terminal","--shell",shell_bin,"--font",font,"--cache",cache,"--diagnostics")
        wait(lambda:"READY " in log() and "IME commit enabled=1" in log("server") and view().get("sequence",0)>1,"actual SDL v3 activation")
        shell=int(re.search(r"READY .* shell=(\d+)",log()).group(1));shell_pin=os.pidfd_open(shell);pins.append(shell_pin)
        typing("printf '");wait(lambda:view().get("x")==17,"original PTY echo before draft")
        before=accepted();sequence=view()["sequence"]
        draft(0,7);assert accepted()==before and view()["sequence"]==sequence
        assert "TEXT edit accepted=1 bytes=7 start=2 length=2" in log()
        capture("German native preedit and scalar selection before shell commit")
        for c in (28,14,105,106,15):tap(c)
        control(46);tap(64);wait(lambda:view().get("german")==1,"ordinary chrome works while composing")
        assert accepted()==before and view()["sequence"]==sequence
        event("I 1");wait(lambda:accepted()==before+2 and view().get("composition")==0,"native composed UTF-8 committed exactly once")
        typing("' > composed-output\n")
        wait(lambda:(root/"composed-output").exists() and (root/"composed-output").read_bytes()=="é".encode(),"real shell file effect contains exact composed bytes")
        cases.append("native v3 draft never reaches PTY; Enter/editing keys cannot execute; exact committed Unicode reaches original shell once")
        before=accepted();disabled=log("server").count("IME commit enabled=0")
        draft(0,7);tap(1);wait(lambda:view().get("composition")==0,"Escape clears draft")
        wait(lambda:log("server").count("IME commit enabled=0")>disabled,"Escape disables actual native text input")
        assert accepted()==before
        draft(6,3);capture("native combining-mark draft")
        draft(2,0);assert accepted()==before
        cases.append("Escape resets actual protocol state; native empty preedit clears without insertion; combining text remains a draft")
        draft(0,7);count=log().count("TEXT commit stale");event("I 7")
        wait(lambda:view().get("composition")==0 and log().count("TEXT commit stale")>count,"queued native commit rejected after Escape")
        assert accepted()==before
        cases.append("native Escape/commit batch cannot revive cancelled draft through already-queued text")
        draft(3,1000)
        for points in (20,24,32):tap(66);wait(lambda:view().get("points")==points,"composition text scale")
        tap(65);wait(lambda:view().get("contrast")==1,"higher contrast composition")
        capture("long horizontally bounded draft at 200 percent German text")
        areas=re.findall(r"^TEXT area=(\d+),(\d+),(\d+),(\d+)$",log(),re.M);x,y,w,h=map(int,areas[-1])
        assert 0<=x<view()["width"] and 0<=y<view()["height"] and w>0 and h>0
        tap(1);wait(lambda:view().get("composition")==0,"long draft cancelled")
        assert accepted()==before
        cases.append("1000-byte draft follows caret in bounded viewport; scale/contrast and candidate area stay inside window")
        count=log().count("TEXT edit accepted=0");event("I 4")
        wait(lambda:log().count("TEXT edit accepted=0")>count,"control-containing draft rejected")
        assert view().get("composition")==0 and accepted()==before
        count=log().count("TEXT commit accepted=0");event("I 5")
        wait(lambda:log().count("TEXT commit accepted=0")>count,"multiline/control commit rejected atomically")
        assert accepted()==before
        cases.append("native draft controls and multiline commit cannot partially enter or execute shell input")
        draft(0,7)
        peer=start(peer_bin,"peer","--clipboard-a");wait(lambda:"FRAME 1 " in log("peer"),"independent client mapped")
        event("M 4580 100");event("B 272 1");event("B 272 0")
        wait(lambda:"FOCUS 0\n" in log() and view().get("composition")==0,"actual other-client focus loss clears draft")
        assert accepted()==before
        peer.stdin.write(b"q");peer.stdin.flush();assert peer.wait(timeout=6)==0;peer.stdin.close();children.remove(peer)
        event("M 500 200");event("B 272 1");event("B 272 0")
        wait(lambda:log().rfind("FOCUS 1\n")>log().rfind("FOCUS 0\n"),"fresh original window focus")
        tap(64);wait(lambda:view().get("german")==0,"fresh owner event after focus")
        assert view().get("composition")==0 and accepted()==before
        cases.append("independent client focus loss retires draft; re-entry does not revive it")
        draft(0,7);control(16,True)
        assert terminal.wait(timeout=8)==0 and select.select([shell_pin],[],[],0)[0];children.remove(terminal)
        assert "shell_status=129 failed=0" in log()
        server.terminate();assert server.wait(timeout=8)==0;children.remove(server)
        cases.append("close during native composition waits original shell/PTY and retires producer resources")
        assert not re.search(r"AddressSanitizer|LeakSanitizer|runtime error:",log()+log("server")+log("peer"))
        assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest()==h for p,h in binaries.items())
        result={"result":"passed","cases":cases,"images":images,"binaries":binaries,
                "scope":"actual SDL, controlled native v3 producer and real PTY; no installed IME/provider or full shaping/accessibility acceptance"}
        (root/"result.json").write_text(json.dumps(result,indent=2)+"\n");print(json.dumps({**result,"artifacts":str(root)}))
    finally:
        for fd in reversed(pins):
            try:signal.pidfd_send_signal(fd,signal.SIGKILL)
            except ProcessLookupError:pass
            finally:os.close(fd)
        for p in children:
            p.wait(timeout=5)
            if p.stdin:p.stdin.close()
        for f in streams:f.close()
        controller.close();issuer.close();print("Terminal composition evidence:",root)


if __name__=="__main__":main()
