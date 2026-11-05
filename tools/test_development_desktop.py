"""Actual development supervisor plus dock/search, Settings, terminal and shell."""
import gzip
import hashlib
import json
import os
from pathlib import Path
import re
import select
import signal
import socket
import stat
import subprocess
import sys
import tempfile
import time


def startup_failure(build,source,owner_source,root):
    fault=root/"startup-failure";fault.mkdir(mode=0o700)
    for name in ("sessions","sessions/fixture","sessions/fixture/runtime","sessions/fixture/cache", "sessions/fixture/documents"):
        (fault/name).mkdir(mode=0o700)
    reader,issuer=socket.socketpair(socket.AF_UNIX,socket.SOCK_SEQPACKET)
    with (fault/"owner.log").open("wb") as stream:
        child=subprocess.Popen([sys.executable,str(source/"tests/development_session_fixture.py"),str(build),str(fault),
                                str(issuer.fileno()),str(owner_source),"early-error"],cwd=fault,pass_fds=(issuer.fileno(),),
                               stdin=subprocess.DEVNULL,stdout=stream,stderr=subprocess.STDOUT)
        issuer.close();pin=os.pidfd_open(child.pid)
        try:
            assert child.wait(timeout=5)==1,"failed compositor must retire unused desktop endpoints"
            text=(fault/"owner.log").read_text();assert "SPAWN compositor " in text and "SPAWN desktop " not in text
            assert "SESSION_END " in text and not re.search(r"AddressSanitizer|LeakSanitizer|runtime error:",text)
        finally:
            if child.poll() is None:signal.pidfd_send_signal(pin,signal.SIGKILL);child.wait(timeout=5)
            os.close(pin);reader.close()


def main():
    build = Path(sys.argv[1]).resolve()
    if os.getuid() != 1000:
        raise RuntimeError("explicit disposable-carrier integration test requires UID1000")
    source = Path(__file__).resolve().parents[1]
    owner_source=Path(sys.argv[2]).resolve() if len(sys.argv)>2 else source/"tools/development_session.py"
    compose=len(sys.argv)>3 and sys.argv[3]=='--compose'
    source_hash=hashlib.sha256(owner_source.read_bytes()).hexdigest()
    binaries={str(build/n):hashlib.sha256((build/n).read_bytes()).hexdigest() for n in
              ("test_compositor_server","aetheri-desktop","aetheri-settings","aetheri-terminal","aetheri-shell","aetheri-files","aetheri-editor")}
    root = Path(tempfile.mkdtemp(prefix="desktop-owner-", dir=build))
    for name in ("captures", "sessions", "sessions/fixture", "sessions/fixture/runtime", "sessions/fixture/cache", "sessions/fixture/documents"):
        (root/name).mkdir(mode=0o700)
    (root/"wait_job.py").write_text("import signal,time\nfrom pathlib import Path\nsignal.signal(signal.SIGHUP,signal.SIG_IGN)\nPath('job-ready').write_text('ready')\ntime.sleep(30)\n")
    document=root/"sessions/fixture/documents/alpha.txt";document.write_bytes(b"alpha\n")
    startup_failure(build,source,owner_source,root)
    controller, issuer = socket.socketpair(socket.AF_UNIX, socket.SOCK_SEQPACKET)
    controller.settimeout(4)
    stream=(root/"owner.log").open("wb")
    owner=subprocess.Popen([sys.executable,str(source/"tests/development_session_fixture.py"),str(build),str(root),str(issuer.fileno()),str(owner_source),*(['compose'] if compose else [])],
                           cwd=root,pass_fds=(issuer.fileno(),),stdin=subprocess.DEVNULL,stdout=stream,stderr=subprocess.STDOUT)
    issuer.close()
    pins={};roles={};images=[];cases=["failed compositor before desktop creation retires its unused launch/private endpoints"];holders=[];seen_spawns=0
    owner_pin=os.pidfd_open(owner.pid);pins[(owner.pid,os.fstat(owner_pin).st_ino)]=owner_pin

    def rawlog(): return (root/"owner.log").read_text(errors="replace")
    def log(role): return "\n".join(line[len(role)+2:] for line in rawlog().splitlines() if line.startswith(role+": "))+"\n"
    def live(fd): return not select.select([fd],[],[],0)[0]
    def pin(pid,parent):
        fd=os.pidfd_open(pid)
        try:
            assert live(fd) and int(Path(f"/proc/{pid}/stat").read_text().rsplit(")",1)[1].split()[1])==parent
            key=(pid,os.fstat(fd).st_ino)
            if key in pins: os.close(fd);return pins[key]
            pins[key]=fd;return fd
        except BaseException:
            os.close(fd);raise
    def enroll():
        nonlocal seen_spawns
        if not live(owner_pin):return
        spawns=re.findall(r"^SPAWN (\w+) (\d+)$",rawlog(),re.M)
        fresh=spawns[seen_spawns:];seen_spawns=len(spawns)
        for role,pid in fresh:
            pid=int(pid)
            try: roles[role]=(pid,pin(pid,owner.pid))
            except (ProcessLookupError,FileNotFoundError):pass
    def wait(predicate,label):
        end=time.monotonic()+9
        while not predicate():
            enroll()
            assert owner.poll() is None,(label,root,rawlog()[-1000:])
            assert time.monotonic()<end,(label,root)
            time.sleep(.01)
        enroll()
    def event(text):
        controller.sendall(text.encode());assert controller.recv(32)==b"OK"
    def key(code,down):event(f"K {code} {int(down)}")
    def tap(code):key(code,True);key(code,False)
    codes=dict(zip("abcdefghijklmnopqrstuvwxyz",(30,48,46,32,18,33,34,35,23,36,37,38,50,49,24,25,16,19,31,20,22,47,17,45,21,44)))
    codes.update(dict(zip("1234567890",range(2,12))))
    codes.update({" ":57,"-":12,".":52,"\n":28})
    def typing(text):
        for c in text:
            if c in (">","_"):key(42,True);tap(52 if c==">" else 12);key(42,False)
            else:tap(codes[c])
    def shortcut():
        count=log("desktop").count("FOCUS 0\n")
        stable=log("desktop").count("STABLE 0 1920x1080\n")
        event("M 100 100");key(125,True);tap(57);key(125,False)
        wait(lambda:log("desktop").count("FOCUS 0\n")>count and log("desktop").count("STABLE 0 1920x1080\n")>stable,"actual global search")
    def click_control(kind):
        rows=re.findall(r"VIEW (\d+) output=0 search=([01]) size=(\d+)x(\d+)",log("desktop"))
        frame,expanded,_,height=map(int,rows[-1])
        x,y=map(float,re.findall(r"CONTROL "+str(frame)+r" 0 "+str(kind)+r" 0 ([\d.]+) ([\d.]+)",log("desktop"))[0])
        event(f"M {x} {y if expanded else 1080-height+y}");event("B 272 1");event("B 272 0")
    def launch_from_search(query):
        shortcut();typing(query)
        wait(lambda:bool(re.search(r"VIEW .* query="+query+r" pending=",log("desktop"))),"native search query")
        tap(28)
    def terminal_shell():
        values=re.findall(r"READY .* shell=(\d+)",log("terminal"))
        assert values
        pid=int(values[-1]);return pid,pin(pid,roles["terminal"][0])
    def snapshot(label):
        sequence=len(images)//3+1
        # Wait for the latest settled dock frame on each actual output.
        for output in range(3):
            def drawn():
                lines=re.findall(r"VIEW (\d+) output="+str(output)+r" .*",log("desktop"))
                return bool(lines) and f"DRAWN {lines[-1]} output={output} " in log("desktop")
            wait(drawn,"compositor observed dock frame")
        signal.pidfd_send_signal(roles["compositor"][1],signal.SIGUSR1)
        wait(lambda:all(f"CAPTURE {sequence} {i}\n" in log("compositor") for i in range(3)),"native composed capture")
        for i in range(3):
            p=root/"captures"/f"capture-{sequence}-output-{i}.ppm";raw=p.read_bytes()
            magic,dimensions,maximum,pixels=raw.split(b"\n",3);w,h=map(int,dimensions.split())
            assert magic==b"P6" and maximum==b"255" and len(pixels)==w*h*3
            assert len({pixels[n:n+3] for n in range(0,len(pixels),3)})>20
            packed=p.with_suffix(".ppm.gz");packed.write_bytes(gzip.compress(raw,mtime=0));p.unlink()
            images.append({"case":label,"output":i,"file":packed.name,"sha256":hashlib.sha256(raw).hexdigest()})
        cases.append(label)
    try:
        wait(lambda:"STABLE 2 3840x84\n" in log("desktop") and "FIXTURE_NOTIFY READY=1" in rawlog(),"real supervisor desktop ready")
        snapshot("actual supervisor dock has Settings Terminal and Files on three outputs")
        click_control(5)
        wait(lambda:"READY files " in log("files") and "LAUNCH_REPLY A F\n" in log("desktop") and "VIEW frame=" in log("files"),"dock launches actual Files")
        first_files=roles["files"]
        before=log("desktop").count("LAUNCH_REPLY A F\n");click_control(5)
        wait(lambda:log("desktop").count("LAUNCH_REPLY A F\n")>before,"reuse original Files")
        assert len(re.findall(r"^SPAWN files ",rawlog(),re.M))==1 and live(first_files[1])
        key(29,True);tap(16);key(29,False)
        wait(lambda:"LAUNCH_REPLY X F\n" in log("desktop"),"Files original process and pipe ended")
        assert not live(first_files[1])
        launch_from_search("files")
        wait(lambda:len(re.findall(r"^SPAWN files ",rawlog(),re.M))==2 and log("files").count("READY files ")==2,"search reopens Files in original session workspace")
        current_files=roles["files"];assert current_files!=first_files
        wait(lambda:len(set(re.findall(r"WINDOW id=(\d+) app=org.aetheri.Files active=1",log("desktop"))))>=2 and log("files").rfind("VIEW frame=")>log("files").rfind("READY files "),"reopened Files actual focus and rendered view")
        tap(28)
        wait(lambda:"READY wayland opengles2 editor=" in log("files") and "FOCUS 1\n" in log("files"),"Files selected document has native focus")
        document_terminal=int(re.findall(r"^OPEN pid=(\d+)",log("files"),re.M)[-1])
        document_editor=int(re.findall(r"READY .* editor=(\d+)",log("files"))[-1])
        document_terminal_pin=pin(document_terminal,current_files[0]);document_editor_pin=pin(document_editor,document_terminal)
        wait(lambda:bool(re.search(r"VIEW \d+ .* sequence=[2-9]\d* ",log("files"))),"actual original editor frame")
        typing("ihello ")
        wait(lambda:log("files").count("TEXT commit accepted=1 bytes=1")==7,"complete dirty document native input")
        assert document.read_bytes()==b"alpha\n"
        snapshot("Files owns a dirty document before normal Terminal launch")
        click_control(4)
        wait(lambda:"LAUNCH_REPLY A T\n" in log("desktop") and "READY " in log("terminal"),"dock launches actual terminal")
        shell,shell_pin=terminal_shell()
        wait(lambda:bool(re.search(r"VIEW .* sequence=[2-9]\d* ",log("terminal"))) and "FOCUS 1\n" in log("terminal"),"original shell prompt and native focus")
        first_terminal=roles["terminal"]
        terminal_window=int(re.findall(r"WINDOW id=(\d+) app=org.aetheri.Terminal ",log("desktop"))[-1])
        editor_window=int(re.findall(r"WINDOW id=(\d+) app=org.aetheri.Editor ",log("desktop"))[-1])
        assert editor_window!=terminal_window and live(document_editor_pin)
        if compose:
            wait(lambda:'COMPOSE active=1' in log('compositor'),'owner enabled real provider')
            def composition_size():
                values=re.findall(r'VIEW .* composition=(\d+) ',log('terminal'));return int(values[-1]) if values else -1
            tap(127);wait(lambda:composition_size()==2,'actual supervised terminal Compose')
            tap(1);wait(lambda:composition_size()==0,'supervised composition cancellation')
            cases.append('explicit supervisor Compose profile reaches actual terminal before original shell command')
        typing("printf session > dock-proof\n")
        wait(lambda:(root/"dock-proof").is_file() and (root/"dock-proof").read_text()=="session","actual own shell command from dock")
        before=log("desktop").count("LAUNCH_REPLY A T\n");click_control(4)
        wait(lambda:log("desktop").count("LAUNCH_REPLY A T\n")>before,"reuse original live terminal")
        assert len(re.findall(r"^SPAWN terminal ",rawlog(),re.M))==1 and live(first_terminal[1])
        launch_from_search("settings")
        wait(lambda:"LAYOUT " in log("settings") and "LAUNCH_REPLY A S\n" in log("desktop"),"global search starts actual Settings")
        assert live(first_terminal[1]) and live(shell_pin)
        snapshot("actual Settings and terminal coexist under original owner")
        focus_before=log("terminal").count("FOCUS 1\n")
        shortcut();typing("terminal")
        wait(lambda:"query=terminal " in log("desktop"),"filter the original Terminal independently of editor windows")
        activated=log("desktop").count(f"ACTIVATED {terminal_window} 1\n")
        tap(15);tap(28)
        wait(lambda:log("desktop").count(f"ACTIVATED {terminal_window} 1\n")>activated,"activate normal Terminal without selecting the earlier document window")
        wait(lambda:log("terminal").count("FOCUS 1\n")>focus_before,"actual terminal focus after task activation")
        key(29,True);key(42,True);tap(16);key(42,False);key(29,False)
        wait(lambda:"LAUNCH_REPLY X T\n" in log("desktop"),"terminal process and output pipe retired")
        assert not live(first_terminal[1]) and not live(shell_pin) and live(roles["settings"][1])
        focus_before=log("terminal").count("FOCUS 1\n")
        launch_from_search("terminal")
        wait(lambda:len(re.findall(r"^SPAWN terminal ",rawlog(),re.M))==2 and len(re.findall(r"READY .* shell=",log("terminal")))==2,"reopen a fresh terminal after full retirement")
        assert roles["terminal"]!=first_terminal
        wait(lambda:log("terminal").count("FOCUS 1\n")>focus_before,"fresh terminal native focus")
        shell,shell_pin=terminal_shell()
        typing("python3 wait_job.py\n")
        wait(lambda:(root/"job-ready").is_file(),"trusted hangup-ignoring original job")
        job=None
        for p in Path("/proc").iterdir():
            if p.name.isdecimal():
                try:
                    if int((p/"stat").read_text().rsplit(")",1)[1].split()[1])==shell:
                        job=pin(int(p.name),shell);break
                except (FileNotFoundError,ProcessLookupError):pass
        assert job is not None
        # Retain the actual original terminal's output writer while its held
        # pidfd is live. This is test-only process inspection, never admission
        # or a production mechanism for adopting an arbitrary /proc/PID/fd.
        assert live(roles["terminal"][1])
        holder=os.open(f"/proc/{roles['terminal'][0]}/fd/1",os.O_WRONLY|os.O_CLOEXEC|os.O_NONBLOCK)
        holders.append(holder)
        assert stat.S_ISFIFO(os.fstat(holder).st_mode) and live(roles["terminal"][1])
        shortcut();key(29,True);tap(16);key(29,False)
        # Files is first in shutdown. The dirty document must keep every later
        # role/display alive, while existing-window search remains available.
        wait(lambda:"STOPPING\n" in rawlog(),"original owner applied its shutdown fence")
        time.sleep(.15)
        assert all(live(roles[r][1]) for r in ("files","terminal","settings","desktop","compositor")) and live(document_terminal_pin) and live(document_editor_pin)
        assert not re.search(r"VIEW .* closing=1 ",log("terminal").rsplit("READY wayland opengles2 shell=",1)[-1]),"normal Terminal must not retire before Files/document end"
        focused=log("files").count("FOCUS 1\n")
        typing("editor")  # Shutdown was requested in this already-open search.
        wait(lambda:"query=editor " in log("desktop"),"existing editor remains searchable after the fence")
        tap(28)
        wait(lambda:log("files").count("FOCUS 1\n")>focused,"closing desktop still activates its existing document")
        key(29,True);tap(31);key(29,False)
        wait(lambda:document.read_bytes()==b"hello alpha\n","exact selected document save during ordered shutdown")
        key(29,True);tap(16);key(29,False)
        wait(lambda:not live(document_terminal_pin) and not live(document_editor_pin) and not live(current_files[1]),"original Files/document pair ends before later roles")
        wait(lambda:bool(re.search(r"VIEW .* closing=1 ",log("terminal").rsplit("READY wayland opengles2 shell=",1)[-1])),"owner requests terminal close while original job remains")
        assert all(live(roles[r][1]) for r in ("terminal","settings","desktop","compositor")) and live(shell_pin) and live(job)
        time.sleep(.15)
        assert all(live(roles[r][1]) for r in ("terminal","settings","desktop","compositor"))
        signal.pidfd_send_signal(job,signal.SIGTERM)
        wait(lambda:not live(roles["terminal"][1]),"original terminal exits while its writer is retained")
        time.sleep(.15)
        assert all(live(roles[r][1]) for r in ("settings","desktop","compositor")),"pipe holder must keep later roles alive"
        assert live(owner_pin),"pipe holder must retain original supervisor"
        os.close(holder);holders.remove(holder)
        assert owner.wait(timeout=9)==0,rawlog()
        assert all(not live(fd) for fd in pins.values())
        result=json.loads(re.search(r"^SESSION_END (.+)$",rawlog(),re.M).group(1))
        assert all(status==0 for status in result["children"].values()),result
        assert result["app_history"]==[{"role":"files","pid":first_files[0],"status":0},{"role":"terminal","pid":first_terminal[0],"status":0}],result
        text=rawlog();assert text.rfind("files: CLOSED files ")<text.rfind("terminal: CLOSED ")<text.rfind("settings: CLOSED ")<text.rfind("desktop: CLOSED ")
        assert not re.search(r"AddressSanitizer|LeakSanitizer|runtime error:",text)
        cases.extend(("separate launch slots, reuse, actual task activation and fresh terminal reopen",
                      "Files dock/search reuse and fresh reopen retain the session document workspace",
                      "distinct Editor window identity cannot hijack normal Terminal selection",
                      "ordered shutdown preserves the dirty document and permits existing-window activation/save/close",
                      "original blocking job keeps Settings/desktop/compositor alive until explicit test release",
                      "exited terminal with a retained actual output writer keeps later roles alive until pipe EOF",
                      "shutdown reaps each original process and drains its pipe before retiring the next role"))
        assert hashlib.sha256(owner_source.read_bytes()).hexdigest()==source_hash
        assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest()==h for p,h in binaries.items())
        report={"result":"passed","scope":"actual development owner and native apps on test compositor, not PAM/DRM or production admission","cases":cases,"images":images,"end":result,"binaries":binaries,
                "owner_source":str(owner_source),"owner_source_sha256":source_hash,"artifacts":str(root)}
        (root/"result.json").write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
    finally:
        enroll()
        for fd in holders:os.close(fd)
        if owner.poll() is None:owner.terminate()
        # Only independently enrolled original pidfds; no SID/PID rediscovery
        # after owner loss. This cleanup is test-only, never a session policy.
        for fd in reversed(list(pins.values())):
            try:signal.pidfd_send_signal(fd,signal.SIGKILL)
            except ProcessLookupError:pass
            finally:os.close(fd)
        if owner.poll() is None:owner.wait(timeout=5)
        stream.close();controller.close()
        print("Development owner evidence:",root)


if __name__=="__main__":main()
