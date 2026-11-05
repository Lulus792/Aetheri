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
    server_bin, files_bin, terminal_bin, editor_bin, font, mono, build, fault_bin = map(
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
    process_pins = {}; document_pins = []; files_log = "files"

    def start(binary, name, *args, pass_fds=(), extra_env=None):
        stream = (root/(name+".log")).open("wb"); streams.append(stream)
        p = subprocess.Popen([str(binary), *map(str, args)], cwd=root, env=extra_env or env,
                             stdin=subprocess.DEVNULL, stdout=stream, stderr=subprocess.STDOUT, pass_fds=pass_fds)
        children.append(p); pin=os.pidfd_open(p.pid); pins.append(pin); process_pins[p]=pin; return p

    def log(name=None):
        name = name or files_log
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
    codes.update({" ":57, "-":12, ".":52, "/":53})

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
        control(49, shift=True); wait(lambda: state(dialog=1, name_focus=0), "new folder dialog at minimum 200 percent")
        typing("scratch"); wait(lambda: state(name_bytes=7), "literal folder name accepted")
        capture("creation dialog remains usable at minimum German 200 percent")
        # Background navigation/filtering cannot replace the retained target or name.
        control(38); tap(108); wait(lambda: state(dialog=1, depth=0, name_bytes=7), "modal navigation fence")
        tap(15); wait(lambda: state(name_focus=1), "Cancel focus")
        event("K 42 1"); tap(15); event("K 42 0")
        wait(lambda: state(name_focus=0), "reverse Tab returns to name")
        tap(15); tap(15); wait(lambda: state(name_focus=2), "Create focus")
        tap(28)
        wait(lambda: (folder/"scratch").is_dir() and state(dialog=0, creating=0), "native directory creation completed")
        assert (folder/"scratch").stat().st_mode & 0o777 == 0o700
        tap(66); tap(64); tap(65)
        wait(lambda: state(scale="1.00", german=0, contrast=0), "preferences restored")
        # Opening/cancelling is effect-free; Cancel must not click through to rows.
        control(49); wait(lambda: state(dialog=1), "new file dialog")
        typing("cancel.txt"); tap(15); tap(28)
        wait(lambda: state(dialog=0), "keyboard cancellation")
        assert not (folder/"cancel.txt").exists()
        # A valid retained prefix must not become an unintended filename.
        control(49); wait(lambda: state(dialog=1), "overflow dialog")
        typing("x"*255); wait(lambda: state(name_bytes=255), "maximum name")
        capture("long name shows the editable tail")
        typing("x"); wait(lambda: state(name_bad=1), "whole name intent rejected")
        tap(28); time.sleep(.1); assert not (folder/("x"*255)).exists()
        control(22); wait(lambda: state(name_bad=0, name_bytes=0), "clear rejected name")
        typing("prefix/"); wait(lambda: state(name_bad=1), "slash rejected")
        tap(28); time.sleep(.1); assert not (folder/"prefix").exists()
        tap(1); wait(lambda: state(dialog=0), "cancel invalid name")
        # Existing contents are not overwritten, and an explicit corrected name is allowed.
        control(49); wait(lambda: state(dialog=1), "conflict dialog")
        typing("alpha.txt"); tap(28)
        wait(lambda: "CREATE attempt=2 result=2 kind=0" in log() and state(dialog=1, creating=0), "existing name conflict")
        assert (folder/"alpha.txt").read_bytes() == b"alpha\n"
        capture("existing file conflict preserves contents")
        control(22); typing("created.txt"); tap(28)
        wait(lambda: (folder/"created.txt").is_file() and state(dialog=0, creating=0), "native create-only file completed")
        assert (folder/"created.txt").read_bytes() == b"" and (folder/"created.txt").stat().st_mode & 0o777 == 0o600
        # Mismatched releases between the two creation buttons cannot open either dialog.
        event("M 455 110"); event("B 272 1"); event("M 580 110"); event("B 272 0")
        time.sleep(.1); assert state(dialog=0)
        click(455,110); wait(lambda: state(dialog=1), "actual new-file button")
        tap(1); wait(lambda: state(dialog=0), "pointer dialog cancellation")
        filter_to("created", 1); tap(28)
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
        wait(lambda: (folder/"created.txt").read_bytes() == b"hello world ", "new file edited and saved exactly")
        assert (folder/"alpha.txt").read_bytes() == b"alpha\n"
        capture("newly created file saved in owned editor terminal")
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
                      "new folder at German 200 percent minimum viewport", "modal target and keyboard focus retained",
                      "cancel does not create", "overflow and invalid names cannot create a retained prefix",
                      "existing-file conflict is create-only", "corrected explicit name creates an empty 0600 file",
                      "new file opens in own editor and saves exact bytes", "creation pointer release does not click through",
                      "overflow filter cannot open the retained matching prefix",
                      "actual selected-FD save", "Terminal close retains dirty-editor cancel",
                      "direct document Ctrl+Z does not strand a stopped editor",
                      "hostile literal filename never parsed by shell", "Files waits for original document child"])
        # The alternate test executable loses the acknowledgement AFTER a real mkdir.
        # Its worker waits on our own release file; ordinary Files contains no hook.
        files_log = "fault"
        fault = start(fault_bin, files_log, *args)
        wait(lambda: state(dialog=0), "fault fixture native Files")
        control(49, shift=True); wait(lambda: state(dialog=1), "fault creation dialog")
        typing("unknown-folder"); tap(28)
        wait(lambda: "FAULT_CREATED directory" in log() and state(creating=1), "real directory effect awaiting acknowledgement")
        before = view()["frame"]; tap(65); tap(1)
        wait(lambda: state(contrast=1, creating=1) and view()["frame"] > before, "worker leaves UI responsive; cancel cannot roll back")
        (root/"release-create").write_bytes(b"release\n")
        wait(lambda: state(creating=0, unknown=1, dialog=1), "lost acknowledgement remains unknown")
        assert (folder/"unknown-folder").is_dir()
        capture("unknown mkdir outcome blocks further creation")
        tap(28); wait(lambda: state(dialog=0, unknown=1), "Back retains unknown operation")
        control(49); wait(lambda: state(dialog=1, unknown=1, name_focus=1), "new creation opens inspection only")
        before_name = view()["name_bytes"]; control(22); typing("another"); tap(15)
        time.sleep(.1); assert state(unknown=1, name_bytes=before_name, name_focus=1)
        assert log().count("FAULT_CREATED directory") == 1 and not (folder/"another").exists()
        control(16); assert fault.wait(timeout=8) == 0; children.remove(fault)
        assert (folder/"unknown-folder").is_dir()
        # Closing during a known-in-flight effect waits for that original worker.
        (root/"release-create").unlink()
        pending_folder=root/"pending-documents"; pending_folder.mkdir(mode=0o700)
        files_log="pending-close"; pending_args=list(args); pending_args[1]=pending_folder
        pending=start(fault_bin,files_log,*pending_args)
        wait(lambda: state(dialog=0), "pending-close Files")
        control(49,shift=True); wait(lambda: state(dialog=1), "pending-close dialog")
        typing("unknown-folder"); tap(28)
        wait(lambda: "FAULT_CREATED directory" in log() and state(creating=1), "pending-close original effect")
        control(16); time.sleep(.1); assert pending.poll() is None
        (root/"release-create").write_bytes(b"release\n")
        assert pending.wait(timeout=8) == 0; children.remove(pending)
        assert "CREATE attempt=1 result=7 kind=1" in log() and "CLOSED files" in log()
        assert (pending_folder/"unknown-folder").is_dir()
        cases.extend(["real lost mkdir acknowledgement is unknown, never retried", "unknown inspection cannot edit or create",
                      "creation worker keeps UI responsive", "close waits for original in-flight creation and preserves its effect"])
        absent = start(files_bin, "absent", *args, extra_env={**env,"WAYLAND_DISPLAY":"absent","DISPLAY":":199"})
        assert absent.wait(timeout=8) == 1; children.remove(absent)
        server.terminate(); assert server.wait(timeout=8) == 0; children.remove(server)
        result = {"result":"passed", "scope":"ordinary-user development navigation and direct child ownership, not app/document admission",
                  "cases":cases, "images":images, "idle_seconds":1, "idle_cpu_ticks":idle,
                  "binaries":{str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in (server_bin,files_bin,terminal_bin,editor_bin,fault_bin)}}
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
