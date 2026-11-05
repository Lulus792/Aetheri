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
    server_bin,terminal_bin,shell_bin,peer_bin,font,build=map(lambda x:Path(x).resolve(),sys.argv[1:7])
    editor_bin=Path(sys.argv[7]).resolve() if len(sys.argv)==8 else None
    assert os.getuid()!=0
    root=Path(tempfile.mkdtemp(prefix="compose-provider-",dir=build))
    runtime,cache,captures=[root/n for n in ("runtime","cache","captures")]
    for p in (runtime,cache,captures):p.mkdir(mode=0o700)
    env={**os.environ,"XDG_RUNTIME_DIR":str(runtime),"WAYLAND_DISPLAY":"compose","LIBGL_ALWAYS_SOFTWARE":"1","GALLIUM_DRIVER":"llvmpipe","LP_NUM_THREADS":"2"}
    for k in ("DISPLAY","WAYLAND_SOCKET","SDL_VIDEO_DRIVER","SDL_RENDER_DRIVER"):env.pop(k,None)
    controller,issuer=socket.socketpair(socket.AF_UNIX,socket.SOCK_SEQPACKET);controller.settimeout(4)
    children,streams,pins,cases,images=[],[],[],[],[]
    binaries={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in (server_bin,terminal_bin,shell_bin,peer_bin)}
    if editor_bin:binaries[str(editor_bin)]=hashlib.sha256(editor_bin.read_bytes()).hexdigest()
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
    def input_key(code,size):
        before=accepted();tap(code);wait(lambda:accepted()==before+size,"actual native editor key bytes")
    def view():
        rows=re.findall(r"^VIEW (\d+) (.+)$",log(),re.M)
        if not rows:return {}
        frame,fields=rows[-1];return {"frame":int(frame),**{k:int(v) for k,v in re.findall(r"(\w+)=(\d+)",fields)}}
    def accepted():
        rows=re.findall(r"^INPUT (\d+)$",log(),re.M);return int(rows[-1]) if rows else 0
    codes=dict(zip("abcdefghijklmnopqrstuvwxyz",(30,48,46,32,18,33,34,35,23,36,37,38,50,49,24,25,16,19,31,20,22,47,17,45,21,44)))
    codes.update({" ":57,"-":12,"'":40,"\n":28,".":52})
    codes.update({"/":53,**dict(zip("1234567890",range(2,12)))})
    def typing(text):
        target=accepted()+len(text.encode())
        for c in text:
            if c==">":key(42,True);tap(52);key(42,False)
            elif c==":":key(42,True);tap(39);key(42,False)
            else:tap(codes[c])
        wait(lambda:accepted()==target,"ordinary actual native input")
    def proof(name,text):
        wait(lambda:(root/name).exists() and (root/name).read_bytes()==text,"real original shell effect "+name)
        wait(lambda:view().get('x')==9 and view().get('composition')==0,"original own-editor prompt after command effect")
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
        typing("printf '");before=accepted();compose();tap(40);tap(36)
        wait(lambda:accepted()==before+3 and view().get("composition")==0,"native combining text reaches own line editor")
        typing("x");input_key(105,3);input_key(14,1);input_key(107,3)
        typing("' > cluster-delete-proof\n");proof("cluster-delete-proof",b"x")
        cases.append("actual Compose result moves and deletes as one extended grapheme in the own shell editor")
        typing("printf 'abxc'");input_key(102,3)
        for _ in range(10):input_key(106,3)
        input_key(111,4);input_key(107,3);typing(" > delete-proof\n");proof("delete-proof",b"abc")
        (root/"delete-proof").rename(root/"delete-original")
        input_key(103,3);input_key(28,1);proof("delete-proof",b"abc")
        cases.append("actual Home/Delete/End edit and explicit history Up/Enter retain exact original command effects")
        typing("printf 'ac'");input_key(105,3);input_key(105,3);typing("b")
        before=accepted();control(44,True);wait(lambda:accepted()==before+1,"actual graphical undo chord")
        before=accepted();control(21,True);wait(lambda:accepted()==before+1,"actual graphical redo chord")
        input_key(107,3);typing(" > undo-proof\n");proof("undo-proof",b"abc")
        cases.append("Ctrl+Shift+Z/Y through real native keyboard input undo/redo a midline edit without stopping the shell")
        (root/"tab-name").write_bytes(b"completed");typing("cat tab-n");input_key(15,1)
        typing(" > tab-proof\n");proof("tab-proof",b"completed")
        cases.append("actual Tab completes a literal file token and waits for explicit Enter before command effects")
        (root/"undo-proof").rename(root/"undo-original")
        before=accepted();control(19);wait(lambda:accepted()==before+1,"actual graphical history search chord")
        typing("undo-proof");wait(lambda:view().get("x")==len("find> undo-proof"),"actual query caret and search view")
        input_key(28,1);wait(lambda:view().get("x")==9+len("printf 'abc' > undo-proof"),"selected original command restored to normal draft")
        assert not (root/"undo-proof").exists()
        input_key(28,1);proof("undo-proof",b"abc")
        cases.append("actual Ctrl+R/query renders history, first Enter only accepts, second Enter executes the original selected command")
        if editor_bin:
            typing(str(editor_bin)+" --new editor-proof\n")
            wait(lambda:view().get("x")==8 and view().get("y")==1,"actual full-screen modal editor")
            children_ids=Path(f"/proc/{shell}/task/{shell}/children").read_text().split();assert len(children_ids)==1
            editor_pid=int(children_ids[0]);editor_pin=os.pidfd_open(editor_pid);pins.append(editor_pin)
            assert Path(f"/proc/{editor_pid}/exe").resolve()==editor_bin and os.stat(f"/proc/{editor_pid}").st_uid==os.getuid()
            input_key(23,1);typing("hello ");compose();tap(40);tap(36)
            wait(lambda:view().get("composition")==0 and view().get("x")==15,"actual Compose cluster inside Insert mode")
            typing("\nsecond line");tap(1)
            wait(lambda:view().get("y")==2 and view().get("x")==18,"modal Escape retains original document/caret")
            capture("actual own modal editor at 100 percent")
            assert not (root/"editor-proof").exists()
            control(16);wait(lambda:view().get("y")==view().get("rows",0)-1 and view().get("x")==0,"native dirty-close dialog focus")
            capture("actual unsaved-close choices before file effects")
            tap(1);wait(lambda:view().get("y")==2,"Escape restores original document focus")
            for points in (20,24,32):tap(66);wait(lambda:view().get("points")==points,"actual modal editor text scaling")
            tap(65);wait(lambda:view().get("contrast")==1,"actual high-contrast modal view")
            capture("actual modal editor at 200 percent high contrast")
            before=view().get('sequence',0);typing('u');wait(lambda:view().get('sequence',0)>before and view().get('x')==8 and view().get('y')==2,"actual Normal undo after native Compose typing")
            control(19);wait(lambda:view().get('x')==18 and view().get('y')==2,"actual editor Ctrl+R restores original typed group")
            # Existing original bytes are not saved by undo/redo itself.
            assert not (root/"editor-proof").exists()
            cases.append("actual modal document undo/redo preserves the native Compose bytes and has no file effect")
            control(31);wait(lambda:(root/"editor-proof").exists() and (root/"editor-proof").read_bytes()=="hello j́\nsecond line".encode(),"real modal save exact grapheme/newline bytes")
            typing(":q\n");wait(lambda:select.select([editor_pin],[],[],0)[0] and view().get("x")==9,"same original editor exits back to shell")
            tap(66);tap(65)
            cases.extend(["actual original modal executable edits native Compose graphemes and newline in the full-screen terminal",
                "dirty-close dialog has actual footer focus, no file effect, and Escape returns to document",
                "200-percent high-contrast editor preserves exact save bytes and restores the original shell"])
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
