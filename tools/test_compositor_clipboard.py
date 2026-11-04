"""Clipboard data through separate native clients, with real focus and FD transfer."""
import json
import hashlib
import os
from pathlib import Path
import socket
import subprocess
import sys
import tempfile
import time


def main():
    server_binary, client_binary, build = [Path(p).resolve() for p in sys.argv[1:]]
    if os.getuid()==0: raise RuntimeError("unprivileged private clipboard test required")
    root=Path(tempfile.mkdtemp(prefix="compositor-clipboard-",dir=build))
    runtime=root/"runtime";runtime.mkdir(mode=0o700)
    env={**os.environ,"XDG_RUNTIME_DIR":str(runtime),"WAYLAND_DISPLAY":"clipboard-private"}
    for key in ("DISPLAY","WAYLAND_SOCKET"):env.pop(key,None)
    controller,issuer=socket.socketpair(socket.AF_UNIX,socket.SOCK_SEQPACKET);controller.settimeout(4)
    processes=[];streams=[];cases=[]
    def log(name):return (root/(name+".log")).read_text(errors="replace")
    def start(binary,name,*args,pass_fds=()):
        stream=(root/(name+".log")).open("wb");streams.append(stream)
        p=subprocess.Popen([str(binary),*map(str,args)],env=env,stdin=subprocess.PIPE,stdout=stream,stderr=subprocess.STDOUT,pass_fds=pass_fds)
        processes.append(p);return p
    def wait(predicate,label):
        end=time.monotonic()+8
        while not predicate():
            assert all(p.poll() is None for p in processes),(label,[(p.pid,p.returncode) for p in processes])
            assert time.monotonic()<end,(label,{n:log(n)[-2500:] for n in ("server","a","b") if (root/(n+".log")).exists()})
            time.sleep(.01)
    def event(message):controller.sendall(message.encode());assert controller.recv(32)==b"OK"
    def focus(name,x,y,index):
        before=log(name).count(f"KEYBOARD_ENTER {index}\n")
        event(f"M {x} {y}");event("B 272 1");event("B 272 0")
        wait(lambda:log(name).count(f"KEYBOARD_ENTER {index}\n")>before,"actual keyboard focus "+name)
    def command(p,name,c):
        before=log(name).count(f"CLIPBOARD_SYNC {c}\n")
        p.stdin.write(c.encode());p.stdin.flush()
        wait(lambda:log(name).count(f"CLIPBOARD_SYNC {c}\n")>before,"native command "+c)
    def read(p,name,expected,c="R"):
        before=log(name).count("CLIPBOARD_READ ")
        command(p,name,c)
        records=[line.split()[1:] for line in log(name).splitlines() if line.startswith("CLIPBOARD_READ ")]
        assert len(records)>before
        record=records[-1];actual=bytes(int(x,16) for x in record[1:]);assert int(record[0])==len(actual)
        assert actual==expected,(name,c,actual,expected)
    aa="Aetheri A Grüße\n".encode();bb="Aetheri B Grüße\n".encode()
    try:
        server=start(server_binary,"server","--headless",env["WAYLAND_DISPLAY"],"-",issuer.fileno(),pass_fds=(issuer.fileno(),));issuer.close()
        wait(lambda:"READY " in log("server"),"compositor ready")
        maps=Path(f"/proc/{server.pid}/maps").read_text()
        libraries={line.split(maxsplit=5)[5] for line in maps.splitlines() if "/libwlroots-0.20.so" in line}
        assert len(libraries)==1,libraries
        library=Path(libraries.pop())
        loaded={"path":str(library),"sha256":hashlib.sha256(library.read_bytes()).hexdigest()}
        (root/"loaded-maps.txt").write_text(maps)
        a=start(client_binary,"a","--clipboard-a")
        wait(lambda:"FRAME 1 " in log("a"),"first independent client mapped")
        b=start(client_binary,"b","--clipboard-b")
        wait(lambda:"FRAME 1 " in log("b"),"second independent client mapped")
        focus("a",2020,100,1);command(a,"a","C");read(a,"a",aa)
        focus("b",4580,100,0);read(b,"b",aa)
        cases.append("UTF-8 offer and actual pipe bytes across independent clients")
        read(a,"a",b"")
        command(a,"a","C");read(b,"b",aa)
        command(a,"a","E");read(b,"b",aa)
        cases.append("unfocused retained-offer read, replacement and clear rejected")
        command(b,"b","Z");read(b,"b",aa)
        command(b,"b","C");read(b,"b",bb)
        wait(lambda:"CLIPBOARD_CANCEL " in log("a"),"old source cancelled")
        cases.append("unknown serial rejected; focused replacement cancels old source")
        focus("a",2020,100,1)
        command(a,"a","S");read(a,"a",bb)
        command(a,"a","C");read(a,"a",aa)
        cases.append("old-focus serial rejected after genuine refocus")
        event("D")
        command(a,"a","E")
        enters=log("a").count("KEYBOARD_ENTER ")
        event("A")
        wait(lambda:log("a").count("KEYBOARD_ENTER ")>enters,"replacement keyboard focus")
        command(a,"a","E");read(a,"a",b"")
        command(a,"a","S");read(a,"a",b"")
        command(a,"a","C");read(a,"a",aa)
        cases.append("no-keyboard selection mutation rejected")
        cases.append("replaced keyboard cannot replay the old focus serial")
        def unmap(p,name):
            p.stdin.write(b"U");p.stdin.flush()
            wait(lambda:"CLIPBOARD_VIEWS_UNMAPPED\n" in log(name),"all client views unmapped")
        unmap(b,"b");unmap(a,"a")
        read(a,"a",b"")
        for p,name in ((b,"b"),(a,"a")):
            before=log(name).count("FRAME 1 ")
            p.stdin.write(b"N");p.stdin.flush()
            wait(lambda:log(name).count("FRAME 1 ")>before,"native views remapped")
        focus("b",4580,100,1);read(b,"b",aa)
        cases.append("old offer cannot read during empty focus; source survives window unmap/remap")
        command(b,"b","D");command(b,"b","C");read(b,"b",bb)
        cases.append("unstarted source-less drag rejected without a leaked grab")
        command(b,"b","H");read(b,"b",bb)
        command(b,"b","J");read(b,"b",bb)
        cases.append("rejected source-backed drag preserves current clipboard and cleans fresh source")
        command(b,"b","E");read(b,"b",b"")
        focus("a",100,100,0);command(a,"a","S");read(a,"a",b"")
        cases.append("old-focus publication denied even after the selection was cleared")
        focus("b",4580,100,1)
        command(b,"b","C");read(b,"b",bb)
        zeros=log("a").count("CLIPBOARD_SELECTION 0\n")
        b.stdin.write(b"q");b.stdin.flush();assert b.wait(timeout=6)==0,log("b");processes.remove(b)
        wait(lambda:log("a").count("CLIPBOARD_SELECTION 0\n")>zeros,"source departure clears current selection")
        read(a,"a",b"")
        a.stdin.write(b"q");a.stdin.flush();assert a.wait(timeout=6)==0,log("a");processes.remove(a)
        server.terminate();assert server.wait(timeout=6)==0,log("server");processes.remove(server)
        cases.append("explicit clear and producer exit leave no retained clipboard payload")
        result={"result":"passed","cases":cases,"loaded_wlroots":loaded,
                "scope":"private same-UID native clipboard; no app grant or FD revocation"}
        (root/"result.json").write_text(json.dumps(result,indent=2)+"\n");print(json.dumps({**result,"artifacts":str(root)}))
    finally:
        for p in reversed(processes):
            if p.poll() is None:
                p.terminate()
                try:p.wait(timeout=4)
                except subprocess.TimeoutExpired:p.kill();p.wait(timeout=4)
            if p.stdin:p.stdin.close()
        for f in streams:f.close()
        controller.close();issuer.close()
        print("Clipboard evidence:",root)


if __name__=="__main__":main()
