"""Native SDL buffers/input and real shell/PTY; no synthetic SDL text events."""
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
    server_binary, terminal_binary, shell_binary, failure_binary, font, build = [Path(x).resolve() for x in sys.argv[1:]]
    assert os.getuid() != 0
    pins = json.loads((Path(__file__).resolve().parents[1] / "spec/ui-inputs.json").read_text())["terminal_font"]
    assert hashlib.sha256(font.read_bytes()).hexdigest() == pins["sha256"]
    assert hashlib.sha256(Path(pins["license_path"]).read_bytes()).hexdigest() == pins["license_sha256"]
    binaries={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in (server_binary,terminal_binary,shell_binary,failure_binary)}
    root = Path(tempfile.mkdtemp(prefix="terminal-ui-", dir=build))
    runtime, cache, captures = [root / x for x in ("runtime", "cache", "captures")]
    for p in (runtime, cache, captures):
        p.mkdir(mode=0o700)
    env = {**os.environ, "XDG_RUNTIME_DIR": str(runtime), "WAYLAND_DISPLAY": "terminal-ui", "LIBGL_ALWAYS_SOFTWARE": "1", "GALLIUM_DRIVER": "llvmpipe", "LP_NUM_THREADS": "2"}
    for k in ("DISPLAY", "WAYLAND_SOCKET", "SDL_VIDEO_DRIVER", "SDL_RENDER_DRIVER"):
        env.pop(k, None)
    controller, issuer = socket.socketpair(socket.AF_UNIX, socket.SOCK_SEQPACKET)
    controller.settimeout(4)
    processes, streams, held, pictures, cases = [], [], {}, [], []
    current = None
    shell_pid = None
    shell_pin = None

    def start(binary, name, args=(), pass_fds=()):
        f = (root / (name + ".log")).open("wb")
        streams.append(f)
        p = subprocess.Popen([str(binary), *map(str, args)], cwd=root, env=env, stdin=subprocess.DEVNULL, stdout=f, stderr=subprocess.STDOUT, pass_fds=pass_fds)
        processes.append(p)
        pin = os.pidfd_open(p.pid)
        held[(p.pid, os.fstat(pin).st_ino)] = pin
        return p

    def log(name="terminal"):
        return (root / (name + ".log")).read_text(errors="replace")

    def enroll():
        if shell_pin is None or select.select([shell_pin], [], [], 0)[0]:
            return
        # Only while the independently pinned original shell is live; never
        # rediscover a new session by a numeric SID after that process ends.
        for p in Path("/proc").iterdir():
            if not p.name.isdecimal():
                continue
            fd = None
            try:
                pid = int(p.name)
                if os.getsid(pid) != shell_pid:
                    continue
                fd = os.pidfd_open(pid)
                if os.getsid(pid) != shell_pid:
                    os.close(fd)
                    continue
                key = (pid, os.fstat(fd).st_ino)
                if key in held:
                    os.close(fd)
                else:
                    held[key] = fd
            except (ProcessLookupError, FileNotFoundError, PermissionError):
                if fd is not None:
                    os.close(fd)

    def wait(predicate, label):
        deadline = time.monotonic() + 8
        while not predicate():
            enroll()
            assert server.poll() is None and (current is None or current.poll() is None), (label, root)
            assert time.monotonic() < deadline, (label, root)
            time.sleep(.01)
        enroll()

    def event(command):
        controller.sendall(command.encode())
        assert controller.recv(32) == b"OK", command

    def key(code, down):
        event(f"K {code} {int(down)}")

    def tap(code):
        key(code, True)
        key(code, False)

    codes = dict(zip("abcdefghijklmnopqrstuvwxyz", (30,48,46,32,18,33,34,35,23,36,37,38,50,49,24,25,16,19,31,20,22,47,17,45,21,44)))
    codes.update(dict(zip("1234567890", range(2,12))))
    codes.update({" ":57,"-":12,"=":13,"[":26,"]":27,"\\":43,";":39,"'":40,",":51,".":52,"/":53,"`":41,"\n":28})
    shifted = dict(zip("!@#$%^&*()_+{}|:\"<>?~", "1234567890-=[]\\;',./`"))

    def accepted():
        rows = re.findall(r"^INPUT (\d+)$",log(),re.M)
        return int(rows[-1]) if rows else 0

    def typing(text):
        target = accepted() + len(text.encode())
        for c in text:
            shift = c.isupper() or c in shifted
            base = shifted.get(c, c.lower())
            if shift:
                key(42, True)
            tap(codes[base])
            if shift:
                key(42, False)
        wait(lambda: accepted() == target,"native text accepted by terminal")

    def layout(german):
        lost=log().count("FOCUS 0\n");gained=log().count("FOCUS 1\n")
        event("D")
        wait(lambda:log().count("FOCUS 0\n")>lost,"actual client keyboard leave")
        event("G" if german else "A")
        wait(lambda:log().count("FOCUS 1\n")>gained,"actual client keyboard enter")

    def control(code, shift=False):
        key(29, True)
        if shift:
            key(42, True)
        tap(code)
        if shift:
            key(42, False)
        key(29, False)

    def view():
        rows = re.findall(r"^VIEW (\d+) (.+)$", log(), re.M)
        if not rows:
            return {}
        frame, fields = rows[-1]
        return {"frame": int(frame), **{k:int(v) for k,v in re.findall(r"(\w+)=(\d+)", fields)}}

    def start_terminal(name, binary=terminal_binary):
        nonlocal current, shell_pid, shell_pin
        current = start(binary, name, ("--shell", shell_binary, "--font", font, "--cache", cache, "--diagnostics"))
        wait(lambda: "READY " in log(name), "native terminal ready")
        shell_pid = int(re.search(r"READY .* shell=(\d+)", log(name)).group(1))
        shell_pin = os.pidfd_open(shell_pid)
        fields = Path(f"/proc/{shell_pid}/stat").read_text().rsplit(")",1)[1].split()
        assert int(fields[1]) == current.pid and int(fields[3]) == shell_pid
        held[(shell_pid, os.fstat(shell_pin).st_ino)] = shell_pin
        return current

    sequence = 0

    def snapshot(label, green=False, selected=False):
        nonlocal sequence
        wait(lambda: view() and f"DRAWN {view()['frame']}\n" in log(), "latest compositor-observed frame")
        sequence += 1
        server.send_signal(signal.SIGUSR1)
        wait(lambda: all(f"CAPTURE {sequence} {i}\n" in log("server") for i in range(3)), label)
        for i in range(3):
            p = captures / f"capture-{sequence}-output-{i}.ppm"
            raw = p.read_bytes()
            magic, dimensions, peak, pixels = raw.split(b"\n",3)
            w,h = map(int, dimensions.split())
            assert magic == b"P6" and peak == b"255" and len(pixels) == w*h*3
            if i == 0:
                assert len({pixels[n:n+3] for n in range(0,len(pixels),3)}) > 20
                if selected:
                    count=sum(pixels[n:n+3]==b"\x30\x4e\x78" for n in range(0,len(pixels),3))
                    assert count>200,(label,"actual selected cell background",count)
                if green:
                    count = sum(pixels[n+1] > pixels[n]+25 and pixels[n+1] > pixels[n+2]+10 for n in range(0,len(pixels),3))
                    assert count > 80, (label,"actual green terminal output",count)
            packed = p.with_suffix(".ppm.gz")
            packed.write_bytes(gzip.compress(raw,mtime=0))
            p.unlink()
            pictures.append({"case":label,"output":i,"file":packed.name,"sha256":hashlib.sha256(raw).hexdigest()})
        cases.append(label)

    try:
        server = start(server_binary, "server", ("--headless",env["WAYLAND_DISPLAY"],captures,issuer.fileno()), (issuer.fileno(),))
        issuer.close()
        wait(lambda: "READY " in log("server"), "compositor")
        start_terminal("terminal")
        wait(lambda: view().get("sequence",0)>1 and view().get("x")==9, "actual original shell prompt processed")
        snapshot("native terminal and original shell")
        nexaui="PRESENTATION nexaui " in log()
        if nexaui:
            def zoomed():
                rows=re.findall(r"^CHROME maximized=(\d+) header=([\d.]+)$",log(),re.M)
                return int(rows[-1][0]) if rows else -1
            assert re.findall(r"^CHROME maximized=0 header=32.0$",log(),re.M)
            normal_cols=view()["cols"];before_input=accepted()
            event("M 103 56");event("B 272 1");event("B 272 0")
            wait(lambda:zoomed()==1 and view()["cols"]>normal_cols,"left native zoom button and actual configure")
            snapshot("compact left controls and observed work-area zoom")
            control(50,shift=True)
            wait(lambda:zoomed()==0 and view()["cols"]==normal_cols,"keyboard restores actual normal geometry")
            # Original-press ownership: mismatched releases and Escape cannot
            # turn a chrome gesture into another action or shell input.
            for x,y in ((59,103),(103,59)):
                before=view()["frame"]
                event(f"M {x} 56");event("B 272 1");event(f"M {y} 56");event("B 272 0")
                wait(lambda:view()["frame"]>before,"mismatched chrome release processed")
                assert current.poll() is None and zoomed()==0
            before=view()["frame"]
            event("M 103 56");event("B 272 1");tap(1);event("B 272 0")
            wait(lambda:view()["frame"]>before,"Escape cancels original chrome press")
            assert zoomed()==0 and accepted()==before_input
            event("M 81 56");event("B 272 1");event("B 272 0")
            event("M 500 200");event("B 272 1");event("M 103 56");event("B 272 0")
            wait(lambda:view().get("selection",0)>0,"body-origin selection remains a selection")
            tap(1);wait(lambda:view().get("selection")==0,"clear selection without shell Escape")
            assert zoomed()==0 and accepted()==before_input
            cases.append("compact chrome: observed pointer zoom/keyboard restore, mismatched/escaped/body-origin release refusal, explicit disabled minimize and no PTY shortcut input")
        typing("printf '\\033[32mAETHERI_NATIVE\\033[0m\\n'\n")
        typing("printf done > proof\n")
        wait(lambda: (root/"proof").is_file(), "typed command actual file effect")
        snapshot("actual ANSI output from native key input",green=True)
        before_input=accepted()
        control(30,shift=True)
        wait(lambda:view().get("selection",0)>1,"keyboard selects actual retained terminal content")
        snapshot("keyboard selection of actual terminal output",selected=True)
        control(46,shift=True);wait(lambda:"CLIPBOARD copy result=0" in log(),"native clipboard publication")
        control(47,shift=True)
        wait(lambda:view().get("review")==1 and view().get("paste",0)>1,"self clipboard transfer reaches review without shell input")
        assert accepted()==before_input,"clipboard chords must not become terminal interrupt/literal-next"
        snapshot("native multiline clipboard review before shell input")
        tap(1);wait(lambda:view().get("review")==0,"Escape cancels paste review")
        cases.append("actual copied text returns through native clipboard and remains unsubmitted until review")
        tap(1)
        wait(lambda:view().get("selection")==0,"Escape clears selection without PTY input")
        assert accepted()==before_input
        grid=tuple(map(float,re.findall(r" grid_left=([\d.]+) grid_top=([\d.]+) cell_width=([\d.]+) cell_height=([\d.]+)$",log(),re.M)[-1]))
        left,top,cw,ch=grid
        # The private compositor's first normal-window placement is 40,40.
        prompt_row=view()["y"]
        event(f"M {40+left+.1} {40+top+(prompt_row+.5)*ch}");event("B 272 1")
        wait(lambda:view().get("selecting")==1,"native selection drag starts in grid")
        event(f"M {40+left+8*cw} {40+top+(prompt_row+.5)*ch}");event("B 272 0")
        wait(lambda:view().get("selection")==9 and view().get("selecting")==0,"eight native selected prompt cells")
        snapshot("pointer cell selection and exact UTF-8 byte extent",selected=True)
        control(105,shift=True)
        wait(lambda:view().get("selection")==8,"keyboard contracts the same pointer selection")
        assert accepted()==before_input
        tap(1);wait(lambda:view().get("selection")==0,"selection escape")
        event(f"M {40+left+cw} {40+top+ch/2}");event("B 272 1")
        wait(lambda:view().get("selecting")==1,"second actual drag")
        lost=log().count("FOCUS 0\n");event("D")
        wait(lambda:log().count("FOCUS 0\n")>lost and view().get("selecting")==0,"focus loss cancels drag")
        gained=log().count("FOCUS 1\n");event("A")
        wait(lambda:log().count("FOCUS 1\n")>gained,"fresh keyboard restores actual terminal focus")
        tap(1);wait(lambda:view().get("selection")==0,"clear empty anchor after focus return")
        assert accepted()==before_input
        cases.append("native keyboard/pointer selection, retained UTF-8 extent, focus-loss drag cancellation and Escape never become shell input")
        typing("printf '\\033[32m")
        layout(True)
        target=accepted()+len("Grüße".encode())
        key(42,True);tap(34);key(42,False);tap(19);tap(26);tap(12);tap(18)
        wait(lambda:accepted()==target,"actual German committed text")
        layout(False)
        typing("\\033[0m\\n' > unicode-output\n")
        wait(lambda:(root/"unicode-output").is_file(),"UTF-8 command output")
        wait(lambda:(root/"unicode-output").read_bytes()==b"\x1b[32mGr\xc3\xbc\xc3\x9fe\x1b[0m\n","exact UTF-8 bytes from actual input")
        typing("cat unicode-output\n")
        before=view()["sequence"]
        typing("printf unicode > unicode-proof\n")
        wait(lambda: (root/"unicode-proof").is_file() and view().get("sequence",0)>before, "DE/US native keymap change and UTF-8 output")
        snapshot("German input and streaming UTF-8",green=True)
        typing("printf '%s' '")
        layout(True)
        target=accepted()+8
        key(100,True)
        for code in (16,8,9,10,11,12,27,86):tap(code)
        wait(lambda:accepted()==target,"German AltGr committed text while modifier held")
        key(100,False)
        layout(False)
        typing("' > altgr-proof\n")
        wait(lambda:(root/"altgr-proof").is_file(),"German AltGr command effect")
        wait(lambda:(root/"altgr-proof").read_bytes()==b"@{[]}\\~|","exact native AltGr text")
        cases.append("German AltGr punctuation is committed text, independent of shortcut modifiers")
        typing("seq 1 80\n")
        wait(lambda: view().get("history",0)>10, "bounded scroll history")
        key(42,True);tap(104);key(42,False)
        wait(lambda: view().get("view",0)>0, "native history key")
        snapshot("keyboard scrollback")
        key(42,True);tap(109);key(42,False)
        wait(lambda: view().get("view")==0, "return to live output")
        typing("sleep 30\n")
        def sleeper():
            for p in Path("/proc").iterdir():
                if p.name.isdecimal():
                    try:
                        if os.getsid(int(p.name))==shell_pid and (p/"comm").read_text().strip()=="sleep":
                            return int(p.name)
                    except (ProcessLookupError,FileNotFoundError,PermissionError):
                        pass
            return None
        wait(lambda: sleeper() is not None,"foreground command from terminal")
        pid=sleeper();pin=os.pidfd_open(pid);held[(pid,os.fstat(pin).st_ino)]=pin
        control(44)
        wait(lambda: Path(f"/proc/{pid}/stat").read_text().rsplit(")",1)[1].split()[0]=="T","native Ctrl+Z")
        typing("fg\n")
        wait(lambda: int(Path(f"/proc/{shell_pid}/stat").read_text().rsplit(")",1)[1].split()[5])==pid,"fg returned terminal ownership")
        control(46)
        wait(lambda:bool(select.select([pin],[],[],0)[0]),"native Ctrl+C ends original command")
        before=view().get("cols")
        key(125,True);tap(103);key(125,False)
        wait(lambda:view().get("cols")!=before,"window/grid/PTY resize")
        snapshot("maximized terminal after real job control")
        tap(64);tap(65)
        wait(lambda:view().get("german")==1 and view().get("contrast")==1,"language and contrast frame applied")
        snapshot("German high-contrast terminal")
        control(30,shift=True);wait(lambda:view().get("selection",0)>1,"select before text resize")
        tap(66);tap(66);tap(66)
        wait(lambda:view().get("points")==32,"200 percent text size")
        assert view().get("selection")==0,"grid resize clears old selection geometry"
        snapshot("200 percent terminal text and cell resize")
        control(30,shift=True);wait(lambda:view().get("selection",0)>1,"selection in current 200 percent geometry")
        snapshot("German high-contrast selection at 200 percent text",selected=True)
        tap(1);wait(lambda:view().get("selection")==0,"clear selection before idle measurement")
        def cpu_ticks():
            fields=Path(f"/proc/{current.pid}/stat").read_text().rsplit(")",1)[1].split()
            return int(fields[11])+int(fields[12])
        count=log().count("VIEW ");ticks=cpu_ticks();idle_start=time.monotonic()
        time.sleep(2)
        assert log().count("VIEW ")==count
        status=Path(f"/proc/{current.pid}/status").read_text()
        metrics={"idle_seconds":time.monotonic()-idle_start,"idle_cpu_seconds":(cpu_ticks()-ticks)/os.sysconf("SC_CLK_TCK"),
                 "idle_new_frames":log().count("VIEW ")-count,"rss_kib":int(re.search(r"VmRSS:\s+(\d+)",status).group(1)),
                 "threads":int(re.search(r"Threads:\s+(\d+)",status).group(1)),"text_points":32}
        typing("exit 0\n")
        assert current.wait(timeout=8)==0,log()
        assert "shell_status=0 failed=0" in log()
        cases.append("original shell end and PTY EOF both observed before clean terminal close")
        current=None;shell_pin=None;shell_pid=None
        if nexaui:
            start_terminal("chrome-close")
            wait(lambda:"NEXAUI_FRAME " in log("chrome-close") and "FOCUS 1\n" in log("chrome-close"),"fresh native close window")
            home=int(re.findall(r"^MAP (\d+)$",log("server"),re.M)[-1]);assert home in (0,1,2)
            event(f"M {(0,1920,4480)[home]+59} 56");event("B 272 1");event("B 272 0")
            assert current.wait(timeout=8)==0,log("chrome-close")
            assert "shell_status=129 failed=0" in log("chrome-close") and select.select([shell_pin],[],[],0)[0]
            cases.append("left close button waits for its original shell and PTY")
            current=None;shell_pin=None;shell_pid=None
        gate=root/"event-failure-gate"
        env["AETHERI_TERMINAL_EVENT_FAILURE_GATE"]=str(gate)
        start_terminal("event-failure",failure_binary)
        wait(lambda:"FOCUS 1\n" in log("event-failure"),"fault consumer native focus")
        gate.write_bytes(b"owned native fixture\n")
        tap(28)
        assert current.wait(timeout=8)==1,log("event-failure")
        assert "PTY event bridge" in log("event-failure") and "shell_status=129 failed=1" in log("event-failure")
        assert select.select([shell_pin],[],[],0)[0],"original shell must be terminal after event failure"
        cases.append("event-queue failure retains owner until original shell and PTY end")
        current=None;shell_pin=None;shell_pid=None
        env.pop("AETHERI_TERMINAL_EVENT_FAILURE_GATE")
        gate=root/"wait-failure-gate"
        env["AETHERI_TERMINAL_WAIT_FAILURE_GATE"]=str(gate)
        start_terminal("wait-failure",failure_binary)
        wait(lambda:"FOCUS 1\n" in log("wait-failure"),"wait-failure consumer native focus")
        gate.write_bytes(b"owned native fixture\n");tap(28)
        assert current.wait(timeout=8)==1,log("wait-failure")
        assert "terminal: event wait:" in log("wait-failure") and "shell_status=129 failed=1" in log("wait-failure")
        assert select.select([shell_pin],[],[],0)[0],"original shell must end after event-wait failure"
        cases.append("persistent event-wait failure uses synchronous owned cleanup without retry loop")
        current=None;shell_pin=None;shell_pid=None
        server.terminate();assert server.wait(timeout=8)==0,log("server")
        assert not re.search(r"AddressSanitizer|LeakSanitizer|runtime error:",log()+log("server")+log("event-failure")+log("wait-failure")+(log("chrome-close") if nexaui else ""))
        assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest()==h for p,h in binaries.items())
        result={"result":"passed","scope":"native SDL/Wayland input/pixels and original shell/PTY; no production app admission","cases":cases,"images":pictures,"binaries":binaries,"metrics":metrics,"artifacts":str(root)}
        (root/"result.json").write_text(json.dumps(result,indent=2));print(json.dumps(result))
    except BaseException:
        try:
            if current is not None and current.poll() is None and server.poll() is None:
                snapshot("failure diagnostic")
        except BaseException as diagnostic:
            print("Failure capture unavailable:",repr(diagnostic))
        raise
    finally:
        enroll()
        for fd in held.values():
            try:
                signal.pidfd_send_signal(fd,signal.SIGKILL)
            except ProcessLookupError:
                pass
            finally:
                os.close(fd)
        for p in processes:
            if p.poll() is None:
                p.wait(timeout=5)
        for f in streams:
            f.close()
        controller.close();issuer.close()
        print("Terminal UI evidence:",root)


if __name__ == "__main__":
    main()
