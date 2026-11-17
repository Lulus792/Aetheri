"""Exercise the Settings executable and its inherited channel on a private Wayland seat."""
import hashlib
import json
import os
from pathlib import Path
import re
import socket
import struct
import subprocess
import sys
import tempfile
import time


def header(kind, request=0):
    return struct.pack(">4sBBHQ", b"ASND", 1, kind, 0, request)


def snapshot(revision, muted=False):
    target = (1, 27, 4)
    command = target if muted else (0, 0, 0)
    fields = struct.pack(">QQQQIIHBBBBH", revision, int(muted), *command, 0, 1,
                         1, 0, 2 if muted else 0, int(muted), 0)
    output = struct.pack(">QQIB3xd256s512s", *target, 7 if muted else 3, 0.45,
                         b"alsa_output.analog", b"Line out")
    return header(1) + fields + output


def main():
    server_binary, app_binary, font, artifacts = (Path(arg).resolve() for arg in sys.argv[1:])
    assert os.getuid() != 0
    inputs = Path(__file__).resolve().parents[1] / "spec/ui-inputs.json"
    assert hashlib.sha256(font.read_bytes()).hexdigest() == json.loads(inputs.read_text())["font"]["sha256"]
    root = Path(tempfile.mkdtemp(prefix="settings-sound-", dir=artifacts))
    print(root, flush=True)
    runtime_dir = tempfile.TemporaryDirectory(prefix="ae-sound-")
    runtime, captures = Path(runtime_dir.name), root / "captures"
    captures.mkdir(mode=0o700)
    env = {**os.environ, "XDG_RUNTIME_DIR": str(runtime), "WAYLAND_DISPLAY": "settings-sound-private",
           "NEXAUI_PLATFORM": "wayland", "DBUS_SESSION_BUS_ADDRESS": "unix:path=" + str(runtime / "absent-bus")}
    for name in ("DISPLAY", "WAYLAND_SOCKET", "AT_SPI_BUS_ADDRESS"):
        env.pop(name, None)
    processes = []

    def start(name, args, fd):
        with (root / (name + ".log")).open("xb") as stream:
            process = subprocess.Popen(args, env=env, pass_fds=(fd,), stdin=subprocess.DEVNULL,
                                       stdout=stream, stderr=subprocess.STDOUT)
        processes.append(process)
        return process

    def log(name="app"):
        return (root / (name + ".log")).read_text(errors="replace")

    def wait(predicate, label):
        deadline = time.monotonic() + 12
        while not predicate():
            failures = [p.returncode for p in processes if p.poll() is not None]
            assert not failures, (label, failures, log("server")[-2000:],
                                  log()[-3000:] if len(processes) > 1 else "")
            assert time.monotonic() < deadline, (label, log()[-3000:], log("server")[-2000:])
            time.sleep(0.01)

    def event(controller, command):
        controller.sendall(command.encode())
        assert controller.recv(32) == b"OK", command

    def click(controller, identifier):
        controls = re.findall(rf"^CONTROL {identifier} ([\d.-]+) ([\d.-]+) ([\d.-]+) ([\d.-]+) \d+$",
                              log(), re.M)
        assert controls, identifier
        x, y, w, h = map(float, controls[-1])
        event(controller, f"M {40+x+w/2} {40+y+h/2}")
        event(controller, "B 272 1")
        event(controller, "B 272 0")

    controller, issuer = socket.socketpair(socket.AF_UNIX, socket.SOCK_SEQPACKET)
    owner, child = socket.socketpair(socket.AF_UNIX, socket.SOCK_SEQPACKET)
    try:
        controller.settimeout(4)
        owner.settimeout(4)
        start("server", [server_binary, "--headless", env["WAYLAND_DISPLAY"], captures,
                         str(issuer.fileno())], issuer.fileno())
        issuer.close()
        wait(lambda: "READY " in log("server"), "private compositor ready")
        app_args = [str(app_binary), "--sound-channel", "--font", str(font), "--diagnostics"]
        assert subprocess.run(app_args, env=env, timeout=4, check=False).returncode == 2

        # Keep the original creator as the app's parent across exec.
        launcher = ("import os,sys; fd=int(sys.argv[1]); "
                    "os.dup2(fd,3,inheritable=True); "
                    "os.close(fd) if fd!=3 else None; os.execv(sys.argv[2],sys.argv[2:])")
        app = start("app", [sys.executable, "-I", "-c", launcher, str(child.fileno()), *app_args],
                    child.fileno())
        child.close()
        wait(lambda: "page=0" in log() and "MAP 0\n" in log("server"), "Settings visible")
        click(controller, 53)
        wait(lambda: re.search(r"^SETTLED .* page=3 ", log(), re.M), "Sound page idle")
        owner.sendall(snapshot(3))
        mute = 0x200000002
        wait(lambda: f"CONTROL {mute} " in log() and "revision=3 count=1" in log(),
             "socket wakes idle window and renders output controls")
        click(controller, mute)
        assert owner.recv(64) == header(2, 1) + struct.pack(">QQQIBB2xd", 3, 1, 27, 4, 1, 1, 0.0)
        owner.sendall(header(3, 1) + b"\0")
        wait(lambda: "revision=3 count=1 feedback=2" in log(), "submission shown")
        output = f"AUDIO_OUTPUT id={mute-2} default=1 available=1 volume=0.450000000 muted="
        assert output + "0\n" in log() and output + "1\n" not in log()
        owner.sendall(snapshot(4, muted=True))
        wait(lambda: "revision=4 count=1 feedback=3" in log() and output + "1\n" in log(),
             "readback confirms mute")
        owner.close()
        wait(lambda: "fenced=1" in log(), "connection loss displayed while window remains alive")
        after_loss = len(log())
        click(controller, 50)
        wait(lambda: re.search(r"^SETTLED .* page=0 ", log()[after_loss:], re.M),
             "Displays still navigable after sound loss")
        event(controller, "K 1 1")
        event(controller, "K 1 0")
        assert app.wait(timeout=5) == 0
        print("Settings rendered the graph update, confirmed a mute readback and survived channel loss.")
    finally:
        for endpoint in (issuer, controller, owner, child):
            endpoint.close()
        for process in reversed(processes):
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait(timeout=5)
        runtime_dir.cleanup()


if __name__ == "__main__":
    main()
