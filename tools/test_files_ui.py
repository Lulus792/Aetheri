"""Actual Files -> owned Terminal -> selected-FD editor on the private compositor."""
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
    server_bin, files_bin, terminal_bin, editor_bin, font, mono, build = map(
        lambda p: Path(p).resolve(), sys.argv[1:])
    assert os.getuid() != 0
    inputs = json.loads((Path(__file__).resolve().parents[1]/"spec/ui-inputs.json").read_text())
    assert hashlib.sha256(font.read_bytes()).hexdigest() == inputs["font"]["sha256"]
    assert hashlib.sha256(mono.read_bytes()).hexdigest() == inputs["terminal_font"]["sha256"]
    root = Path(tempfile.mkdtemp(prefix="files-ui-", dir=build))
    runtime, cache, captures, folder = [root/n for n in ("runtime", "cache", "captures", "documents")]
    for path in (runtime, cache, captures, folder): path.mkdir(mode=0o700)
    (folder/"docs").mkdir(mode=0o700)
    (folder/"docs"/"note.txt").write_text("inside\n")
    (folder/"alpha.txt").write_text("alpha\n")
    (folder/".hidden").write_text("hidden\n")
    hostile = "literal ' $(touch SHOULD-NOT-EXIST).txt"
    (folder/hostile).write_text("literal\n")
    (folder/("long-"+"x"*250)).write_text("long\n")
    (folder/"escape").symlink_to("/")
    os.mkfifo(folder/"pipe", 0o600)
    for i in range(16): (folder/f"z-{i:02}.txt").write_text("row\n")
    env = {**os.environ, "XDG_RUNTIME_DIR": str(runtime), "WAYLAND_DISPLAY": "files-private",
           "LIBGL_ALWAYS_SOFTWARE": "1", "GALLIUM_DRIVER": "llvmpipe", "LP_NUM_THREADS": "2"}
    for k in ("DISPLAY", "WAYLAND_SOCKET", "SDL_VIDEO_DRIVER", "SDL_RENDER_DRIVER"): env.pop(k, None)
    controller, issuer = socket.socketpair(socket.AF_UNIX, socket.SOCK_SEQPACKET)
    controller.settimeout(4)
    children, streams, pins, images, cases = [], [], [], [], []
    process_pins = {}; document_pins = []

    def start(binary, name, *args, pass_fds=(), extra_env=None):
        stream = (root/(name+".log")).open("wb"); streams.append(stream)
        p = subprocess.Popen([str(binary), *map(str, args)], cwd=root, env=extra_env or env,
                             stdin=subprocess.DEVNULL, stdout=stream, stderr=subprocess.STDOUT, pass_fds=pass_fds)
        children.append(p); pin=os.pidfd_open(p.pid); pins.append(pin); process_pins[p]=pin; return p

    def log(name="files"):
        p=root/(name+".log"); return p.read_text(errors="replace") if p.exists() else ""

    def wait(predicate, label):
        end = time.monotonic()+12
        while not predicate():
            assert all(p.poll() is None for p in children), (label, root, log()[-2500:])
            assert time.monotonic() < end, (label, root, log()[-3000:])
            time.sleep(.01)

    def event(command):
        controller.sendall(command.encode()); assert controller.recv(32) == b"OK", command

    def tap(code): event(f"K {code} 1"); event(f"K {code} 0")

    def control(code, shift=False):
        event("K 29 1")
        if shift: event("K 42 1")
        tap(code)
        if shift: event("K 42 0")
        event("K 29 0")

    def view():
        rows = re.findall(r"^VIEW frame=(\d+) (.+)$", log(), re.M)
        return {"frame": int(rows[-1][0]), **dict(re.findall(r"(\w+)=([^ ]+)", rows[-1][1]))} if rows else {}

    def state(**expected): return bool(view()) and all(view().get(k) == str(v) for k, v in expected.items())

    def terminal_view():
        rows = re.findall(r"^VIEW (\d+) (.+)$", log()[log().rfind("READY wayland opengles2 editor="):], re.M)
        return {"frame": int(rows[-1][0]), **dict(re.findall(r"(\w+)=([^ ]+)", rows[-1][1]))} if rows else {}

    def accepted():
        rows = re.findall(r"^INPUT (\d+)$", log()[log().rfind("READY wayland opengles2 editor="):], re.M)
        return int(rows[-1]) if rows else 0

    codes = dict(zip("abcdefghijklmnopqrstuvwxyz", (30,48,46,32,18,33,34,35,23,36,37,38,50,49,24,25,16,19,31,20,22,47,17,45,21,44)))
    codes.update({" ":57, "-":12, ".":52})

    def typing(text, editor=False):
        target = accepted()+len(text.encode())
        for c in text: tap(codes[c])
        if editor: wait(lambda: accepted() == target, "actual editor input acceptance")

    def filter_to(query, count):
        control(38); wait(lambda: state(filter=1), "filter focus")
        typing(query); wait(lambda: state(count=count), "actual filtered rows")

    def capture(label):
        time.sleep(.10)
        seq = len(images)//3+1; server.send_signal(signal.SIGUSR1)
        wait(lambda: all(f"CAPTURE {seq} {i}\n" in log("server") for i in range(3)), label)
        for i in range(3):
            path = captures/f"capture-{seq}-output-{i}.ppm"
            raw = path.read_bytes(); magic, geometry, peak, pixels = raw.split(b"\n", 3)
            w, h = map(int, geometry.split())
            assert magic == b"P6" and peak == b"255" and len(pixels) == w*h*3
            if i == 0:
                bg = (0,0,0) if state(contrast=1) else (16,20,28)
                assert tuple(pixels[(50*w+50)*3:(50*w+50)*3+3]) == bg
                ink = sum(tuple(pixels[(y*w+x)*3:(y*w+x)*3+3]) != bg
                          for y in range(64,100) for x in range(65,240))
                assert ink > 150, (label, ink)
            packed = path.with_suffix(".ppm.gz"); packed.write_bytes(gzip.compress(raw, mtime=0))
            assert gzip.decompress(packed.read_bytes()) == raw; path.unlink()
            images.append({"case":label, "output":i, "file":packed.name, "sha256":hashlib.sha256(raw).hexdigest()})
        cases.append(label)

    def click(x, y):
        event(f"M {x} {y}"); event("B 272 1"); event("B 272 0")

    def ended(pin): return bool(select.select([pin], [], [], 0)[0])

    args = ["--root", folder, "--font", font, "--cache", cache, "--terminal", terminal_bin,
            "--editor", editor_bin, "--terminal-font", mono, "--diagnostics"]
    try:
        server = start(server_bin, "server", "--headless", env["WAYLAND_DISPLAY"], captures, issuer.fileno(),
                       pass_fds=(issuer.fileno(),), extra_env={**env, "WAYLAND_DEBUG":"server"})
        issuer.close(); wait(lambda: "READY " in log("server"), "private compositor")
        app = start(files_bin, "files", *args)
        wait(lambda: state(depth=0, count=22, total=23), "complete visible root")
        capture("root and clipped long literal name")
        tap(28); wait(lambda: state(depth=1, count=1), "held directory enter")
        tap(14); wait(lambda: state(depth=0, count=22), "held parent return")
        control(35); wait(lambda: state(count=23), "hidden keyboard toggle")
        # A mismatched release must not activate the hidden control.
        event("M 200 160"); event("B 272 1"); event("M 900 650"); event("B 272 0")
        time.sleep(.1); assert state(count=23)
        control(35); wait(lambda: state(count=22), "hidden restored")
        filter_to("no-match", 0); tap(28); time.sleep(.1); assert "OPEN " not in log()
        tap(1); filter_to("escape", 1); tap(28)
        wait(lambda: "VIEW frame=" in log() and state(depth=0, count=1), "symlink refusal")
        time.sleep(.1); assert "OPEN " not in log()
        tap(1); filter_to("long", 1); capture("long filename is bounded before rasterization")
        tap(1)
        filter_to("long-"+"x"*250, 1); typing("a")
        wait(lambda: state(rejected=1), "overflow rejects the whole filter intent")
        tap(28); time.sleep(.1); assert "OPEN " not in log() and state(depth=0)
        tap(1); wait(lambda: state(rejected=0, count=22), "cancel rejected filter")
        time.sleep(.15)
        before=view()["frame"]
        def ticks():
            fields=Path(f"/proc/{app.pid}/stat").read_text().rsplit(")",1)[1].split()
            return int(fields[11])+int(fields[12])
        initial=ticks(); time.sleep(1); idle=ticks()-initial
        assert view()["frame"]==before and idle<=2, (idle, before, view())
        # Page/end selection must be visible, not a hidden activation target.
        tap(107); wait(lambda: int(view().get("selected",0)) == 21, "last row selected")
        assert int(view()["top"]) > 0
        tap(102); wait(lambda: state(selected=0, top=0), "home selection visible")
        tap(64); tap(65)
        for _ in range(3): tap(66)
        wait(lambda: state(german=1, contrast=1, scale="2.00"), "German contrast 200 percent")
        capture("German contrast at 200 percent")
        # Use actual client resize request and configure; no SDL event injection.
        resizes = log("server").count(".resize(")
        event("M 1037 717"); event("B 272 1")
        wait(lambda: log("server").count(".resize(") > resizes, "Files native resize request")
        event("M 677 457"); wait(lambda: state(width=640, height=420), "minimum size configured")
        event("B 272 0"); capture("minimum viewport refuses invisible entry activation")
        assert state(rows=0)
        tap(28); time.sleep(.1); assert "OPEN " not in log() and state(depth=0)
        tap(66); tap(64); tap(65)
        wait(lambda: state(scale="1.00", german=0, contrast=0), "preferences restored")
        filter_to("alpha", 1); tap(28)
        wait(lambda: "READY wayland opengles2 editor=" in log(), "owned Terminal and actual editor")
        terminal_pid = int(re.findall(r"^OPEN pid=(\d+)", log(), re.M)[-1])
        editor_pid = int(re.findall(r"READY .* editor=(\d+)", log())[-1])
        terminal_pin, editor_pin = os.pidfd_open(terminal_pid), os.pidfd_open(editor_pid)
        pins.extend((terminal_pin, editor_pin)); document_pins.extend((terminal_pin, editor_pin))
        # The second client maps on output 1, and the server focuses it.
        wait(lambda: int(terminal_view().get("sequence", "0")) > 1 and "FOCUS 1\n" in log()
             and f"DRAWN {terminal_view().get('frame')}\n" in log(), "editor rendered and native keyboard focus admitted")
        typing("ihello ", editor=True)
        control(44)  # A direct document has no shell that could resume a stopped job.
        time.sleep(.15)
        assert Path(f"/proc/{editor_pid}/stat").read_text().rsplit(")",1)[1].split()[0] not in ("T","t")
        control(16, shift=True)  # Terminal close requests Editor Ctrl+Q, preserving dirty choices.
        time.sleep(.2); assert not ended(editor_pin) and not ended(terminal_pin)
        capture("Terminal close presents the dirty-editor choice")
        tap(1); time.sleep(.16); typing("world ", editor=True)
        control(31)
        wait(lambda: (folder/"alpha.txt").read_bytes() == b"hello world alpha\n", "exact selected-document save")
        capture("saved selected file in owned editor terminal")
        control(16)
        wait(lambda: ended(editor_pin) and ended(terminal_pin) and "DOCUMENT_END status=0" in log(), "original document pair ended")
        wait(lambda: state(child=-1), "Files observes owned document end and refreshes")
        # Fresh literal hostile filename; no filename interpretation by a shell.
        click(150,200); filter_to("literal", 1); tap(28)
        wait(lambda: log().count("READY wayland opengles2 editor=") == 2, "literal filename second document")
        second_start=log().rfind("READY wayland opengles2 editor=")
        wait(lambda: "FOCUS 1\n" in log()[second_start:] and int(terminal_view().get("sequence","0"))>1
             and f"DRAWN {terminal_view().get('frame')}\n" in log()[second_start:], "second document actual focus and frame")
        ep = os.pidfd_open(int(re.findall(r"READY .* editor=(\d+)",log())[-1])); pins.append(ep); document_pins.append(ep)
        tp = os.pidfd_open(int(re.findall(r"^OPEN pid=(\d+)",log(),re.M)[-1])); pins.append(tp); document_pins.append(tp)
        typing("ix", editor=True); control(31)
        wait(lambda: (folder/hostile).read_bytes() == b"xliteral\n", "literal filename save without shell")
        assert not (folder/"SHOULD-NOT-EXIST").exists() and not (root/"SHOULD-NOT-EXIST").exists()
        # Closing Files while its document is alive waits for that original child.
        click(150,200); control(16); time.sleep(.2)
        assert app.poll() is None and not ended(tp)
        # Third mapped client is on output 2 at (4480+40,40).
        click(4600,200); control(16)
        assert app.wait(timeout=12) == 0, log()[-3000:]; children.remove(app)
        assert ended(ep) and ended(tp) and log().count("DOCUMENT_END status=0") == 2
        assert "CLOSED files" in log() and "failed=0" in log().split("CLOSED files")[-1]
        cases.extend(["mismatched pointer release cancelled", "links and no-match do not dispatch",
                      "overflow filter cannot open the retained matching prefix",
                      "actual selected-FD save", "Terminal close retains dirty-editor cancel",
                      "direct document Ctrl+Z does not strand a stopped editor",
                      "hostile literal filename never parsed by shell", "Files waits for original document child"])
        absent = start(files_bin, "absent", *args, extra_env={**env,"WAYLAND_DISPLAY":"absent","DISPLAY":":199"})
        assert absent.wait(timeout=8) == 1; children.remove(absent)
        server.terminate(); assert server.wait(timeout=8) == 0; children.remove(server)
        result = {"result":"passed", "scope":"ordinary-user development navigation and direct child ownership, not app/document admission",
                  "cases":cases, "images":images, "idle_seconds":1, "idle_cpu_ticks":idle,
                  "binaries":{str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in (server_bin,files_bin,terminal_bin,editor_bin)}}
        (root/"result.json").write_text(json.dumps(result,indent=2)+"\n")
        print(json.dumps({**result,"artifacts":str(root)}))
    finally:
        # Pin every harness-owned original. No numeric-PID rediscovery or signals.
        for pin in reversed(document_pins):
            if not ended(pin): signal.pidfd_send_signal(pin, signal.SIGHUP)
        for p in reversed(children):
            pin=process_pins[p]
            if p.poll() is None:
                signal.pidfd_send_signal(pin, signal.SIGTERM)
                try: p.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    signal.pidfd_send_signal(pin, signal.SIGKILL); p.wait(timeout=5)
        for stream in streams: stream.close()
        for pin in pins: os.close(pin)
        controller.close(); issuer.close()
        print("Files evidence:", root)


if __name__ == "__main__": main()
