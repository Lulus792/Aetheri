"""Actual compositor provider + physical key path -> SDL -> original shell/PTY."""
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
    server_bin,terminal_bin,shell_bin,peer_bin,font,build=map(lambda x:Path(x).resolve(),sys.argv[1:])
    assert os.getuid()!=0
    root=Path(tempfile.mkdtemp(prefix="compose-provider-",dir=build))
    runtime,cache,captures=[root/n for n in ("runtime","cache","captures")]
    for p in (runtime,cache,captures):p.mkdir(mode=0o700)
    env={**os.environ,"XDG_RUNTIME_DIR":str(runtime),"WAYLAND_DISPLAY":"compose","LIBGL_ALWAYS_SOFTWARE":"1","GALLIUM_DRIVER":"llvmpipe","LP_NUM_THREADS":"2"}
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
            assert time.monotonic()<end,(label,root,log()[-2200:]);time.sleep(.01)
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
        wait(lambda:accepted()==target,"ordinary actual native input")
    def proof(name,text):wait(lambda:(root/name).exists() and (root/name).read_bytes()==text,"real original shell effect "+name)
    def compose():
        tap(127);wait(lambda:view().get("composition")==2,"real Menu/Compose key starts native preedit")
    def capture(label):
        wait(lambda:f"DRAWN {view().get('frame')}\n" in log(),"latest actual frame")
        seq=len(images)//3+1;server.send_signal(signal.SIGUSR1)
        wait(lambda:all(f"CAPTURE {seq} {i}\n" in log("server") for i in range(3)),label)
        for i in range(3):
            p=captures/f"capture-{seq}-output-{i}.ppm";raw=p.read_bytes();magic,geometry,peak,pixels=raw.split(b"\n",3)
            w,h=map(int,geometry.split());assert magic==b"P6" and peak==b"255" and len(pixels)==w*h*3
            packed=p.with_suffix(".ppm.gz");packed.write_bytes(gzip.compress(raw,mtime=0));p.unlink()
            images.append({"case":label,"output":i,"file":packed.name,"sha256":hashlib.sha256(raw).hexdigest()})
    try:
        server=start(server_bin,"server","--headless",env["WAYLAND_DISPLAY"],captures,issuer.fileno(),"--compose",pass_fds=(issuer.fileno(),));issuer.close()
        wait(lambda:"READY " in log("server"),"actual provider compositor")
        terminal=start(terminal_bin,"terminal","--shell",shell_bin,"--font",font,"--cache",cache,"--diagnostics")
        wait(lambda:"READY " in log() and "COMPOSE active=1" in log("server") and view().get("sequence",0)>1,"actual SDL native text input activated")
        shell=int(re.search(r"READY .* shell=(\d+)",log()).group(1));shell_pin=os.pidfd_open(shell);pins.append(shell_pin)
        typing("printf '");wait(lambda:view().get("x")==17,"original PTY echo")
        event("L de");before=accepted();tap(13)
        wait(lambda:view().get("composition")==2,"actual German acute dead key")
        assert accepted()==before;capture("real German dead-key preedit before shell input")
        tap(18);wait(lambda:accepted()==before+2 and view().get("composition")==0,"actual dead key plus e produces Unicode once")
        event("L us");typing("' > acute-proof\n");proof("acute-proof","é".encode())
        cases.append("ordinary production provider consumes actual DE dead-key press/release and commits exact UTF-8 once")
        typing("printf '");before=accepted();compose()
        key(42,True);tap(40);key(42,False)
        wait(lambda:view().get("composition")==3,"actual multi-key sequence advances draft")
        tap(30);wait(lambda:accepted()==before+2 and view().get("composition")==0,"actual Compose quote plus a")
        typing("' > umlaut-proof\n");proof("umlaut-proof","ä".encode())
        cases.append("Menu/Compose sequence uses embedded standard data and actual native v3, not fixed test-producer text")
        typing("printf '");before=accepted();compose();tap(40);tap(36)
        wait(lambda:accepted()==before+3 and view().get("composition")==0,"native multi-scalar Compose commit")
        typing("' > combining-proof\n");proof("combining-proof","j́".encode())
        cases.append("a multi-scalar result preserves base plus combining mark bytes through original PTY and shell")
        before=accepted();compose();tap(1);wait(lambda:view().get("composition")==0,"native Escape cancels provider")
        assert accepted()==before
        typing("printf plain > plain-proof\n");proof("plain-proof",b"plain")
        cases.append("provider cancellation cannot forward Escape or revive a prefix in subsequent ordinary input")
        before=accepted();compose()
        for points in (20,24,32):tap(66);wait(lambda:view().get("points")==points,"real provider draft survives font scale")
        tap(64);tap(65);wait(lambda:view().get("german")==1 and view().get("contrast")==1,"actual German high-contrast draft")
        assert view().get("composition")==2 and accepted()==before
        capture("real provider composition at 200 percent German text")
        tap(1);wait(lambda:view().get("composition")==0,"cancel scaled provider draft")
        tap(66);tap(64);tap(65)
        cases.append("function/chrome controls and candidate-position commits preserve original provider state without duplicate input")
        before=accepted();compose();event("L de");wait(lambda:view().get("composition")==0,"same keyboard keymap change cancels original prefix")
        event("L us");assert accepted()==before
        typing("printf keymap > keymap-proof\n");proof("keymap-proof",b"keymap")
        cases.append("same-keyboard keymap replacement resets provider before ordinary input")
        compose();event("D");wait(lambda:view().get("composition")==0,"original device removal retires draft")
        event("A");wait(lambda:log().rfind("FOCUS 1\n")>log().rfind("FOCUS 0\n"),"replacement native keyboard focus")
        typing("printf device > device-proof\n");proof("device-proof",b"device")
        cases.append("new keyboard cannot inherit old keyboard composition")
        before=accepted();compose()
        peer=start(peer_bin,"peer","--clipboard-a");wait(lambda:"FRAME 1 " in log("peer"),"independent native client")
        event("M 4580 100");event("B 272 1");event("B 272 0")
        wait(lambda:view().get("composition")==0 and log().rfind("FOCUS 0\n")>log().rfind("FOCUS 1\n"),"other actual client focus retires provider draft")
        assert accepted()==before
        peer.stdin.write(b"q");peer.stdin.flush();assert peer.wait(timeout=6)==0;peer.stdin.close();children.remove(peer)
        event("M 500 200");event("B 272 1");event("B 272 0")
        wait(lambda:log().rfind("FOCUS 1\n")>log().rfind("FOCUS 0\n"),"fresh original client focus")
        typing("printf focused > focus-proof\n");proof("focus-proof",b"focused")
        cases.append("actual client departure/re-entry cannot retarget or restore old composition")
        compose();control(16,True)
        assert terminal.wait(timeout=8)==0 and select.select([shell_pin],[],[],0)[0];children.remove(terminal)
        server.terminate();assert server.wait(timeout=8)==0;children.remove(server)
        assert not re.search(r"AddressSanitizer|LeakSanitizer|runtime error:",log()+log("server")+log("peer"))
        assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest()==h for p,h in binaries.items())
        result={"result":"passed","cases":cases,"images":images,"binaries":binaries,
                "scope":"actual built-in provider and native key/text/PTY path; no external IME, installed session, full script or hardware acceptance"}
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
        controller.close();issuer.close();print("Compose provider evidence:",root)


if __name__=="__main__":main()
