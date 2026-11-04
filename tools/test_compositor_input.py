"""Native seat/focus/drag checks using a separately compiled inherited-FD fixture."""
import gzip
import hashlib
import json
import os
from pathlib import Path
import signal
import socket
import subprocess
import sys
import tempfile
import time


def main():
    server_binary, client_binary, build = map(lambda s: Path(s).resolve(), sys.argv[1:])
    if os.getuid() == 0:
        raise RuntimeError("private input test must be unprivileged")
    root = Path(tempfile.mkdtemp(prefix="compositor-input-", dir=build))
    runtime, captures = root / "runtime", root / "captures"
    runtime.mkdir(mode=0o700); captures.mkdir(mode=0o700)
    env = {**os.environ, "XDG_RUNTIME_DIR": str(runtime), "WAYLAND_DISPLAY": "input-private"}
    for name in ("DISPLAY", "WAYLAND_SOCKET"):
        env.pop(name, None)
    controller, issuer = socket.socketpair(socket.AF_UNIX, socket.SOCK_SEQPACKET)
    controller.settimeout(4)
    processes, streams = [], []

    def start(binary, name, *args, pass_fds=()):
        stream = (root / (name + ".log")).open("wb"); streams.append(stream)
        p = subprocess.Popen([str(binary), *map(str, args)], env=env, stdin=subprocess.PIPE,
                             stdout=stream, stderr=subprocess.STDOUT, pass_fds=pass_fds)
        processes.append(p)
        return p

    def log(name="client"):
        return (root / (name + ".log")).read_text(errors="replace")

    def wait(predicate, label):
        end = time.monotonic() + 7
        while not predicate():
            assert all(p.poll() is None for p in processes), (label, log(), log("server"))
            assert time.monotonic() < end, (label, log(), log("server"))
            time.sleep(0.01)

    def event(text):
        controller.sendall(text.encode())
        assert controller.recv(32) == b"OK", text

    def key(code, down):
        event(f"K {code} {int(down)}")

    def click(x, y):
        event(f"M {x} {y}"); event("B 272 1"); event("B 272 0")

    def command(c, expected):
        before = log().count(expected)
        client.stdin.write(c.encode()); client.stdin.flush()
        wait(lambda: log().count(expected) > before, expected)

    sequence, images, cases = 0, [], []

    def snapshot(label, points):
        nonlocal sequence
        sequence += 1
        server.send_signal(signal.SIGUSR1)
        wait(lambda: all(f"CAPTURE {sequence} {i}\n" in log("server") for i in range(3)), label)
        for i in range(3):
            path = captures / f"capture-{sequence}-output-{i}.ppm"
            raw = path.read_bytes(); magic, dimensions, peak, pixels = raw.split(b"\n", 3)
            width, height = map(int, dimensions.split())
            assert magic == b"P6" and peak == b"255" and len(pixels) == width * height * 3
            for x, y, color in points.get(i, []):
                offset = (y * width + x) * 3
                assert tuple(pixels[offset:offset + 3]) == color, (label, i, x, y, pixels[offset:offset + 3])
            digest = hashlib.sha256(raw).hexdigest()
            packed = path.with_suffix(".ppm.gz")
            with packed.open("xb") as f:
                f.write(gzip.compress(raw, mtime=0))
            path.unlink()
            images.append({"file": packed.name, "sha256": digest})
        cases.append(label)

    blue, green, bg = (41, 99, 196), (38, 180, 120), (16, 20, 28)
    try:
        server = start(server_binary, "server", "--headless", env["WAYLAND_DISPLAY"], captures, issuer.fileno(), pass_fds=(issuer.fileno(),))
        issuer.close()
        wait(lambda: "READY " in log("server"), "server readiness")
        client = start(client_binary, "client", "--input")
        wait(lambda: "CAPS 3\n" in log() and "FRAME 1 " in log() and "KEYBOARD_ENTER 1\n" in log(), "seat and mapped windows")
        click(100, 100)
        wait(lambda: "KEYBOARD_ENTER 0\n" in log(), "click focus")
        key(30, True); key(30, False)
        wait(lambda: "KEY 0 30 1 97\n" in log(), "unmodified a reaches first window")
        event("S 15")
        wait(lambda: "AXIS 0 15.00\n" in log(), "scroll reaches hovered window")
        click(2020, 100)
        key(42, True); key(30, True); key(30, False); key(42, False)
        wait(lambda: "KEY 1 30 1 65\n" in log(), "shifted A reaches second window")
        key(56, True); key(15, True); key(15, False); key(56, False)
        key(48, True); key(48, False)
        wait(lambda: "KEY 0 48 1 98\n" in log(), "Alt+Tab selects first window")
        assert not any(line.startswith("KEY ") and line.split()[2] == "15" for line in log().splitlines())
        cases.append("click focus, keymap/modifiers, scrolling and consumed Alt+Tab press/release")
        click(100, 100)
        command("g", "ARM_MOVE\n")
        syncs = log().count("INPUT_SYNC\n")
        event("B 272 1")
        wait(lambda: log().count("INPUT_SYNC\n") > syncs, "admitted move request")
        event("M 160 140"); event("B 272 0")
        snapshot("serial-bound move", {0: [(45, 45, bg), (105, 85, blue)], 1: [(45, 45, green)]})
        command("z", "ARM_RESIZE\n")
        syncs = log().count("INPUT_SYNC\n")
        event("B 272 1")
        wait(lambda: log().count("INPUT_SYNC\n") > syncs, "admitted resize request")
        event("M 220 180")
        wait(lambda: "FRAME 0 420 260 0\n" in log(), "resize actually presented")
        event("B 272 0")
        snapshot("serial-bound resize", {0: [(510, 325, blue), (525, 345, bg)]})
        command("t", "INPUT_SYNC\n")
        event("M 300 220")
        snapshot("released serial cannot move window", {0: [(105, 85, blue), (525, 345, bg)]})
        command("h", "ARM_FOREIGN\n")
        syncs = log().count("INPUT_SYNC\n")
        event("B 272 1")
        wait(lambda: log().count("INPUT_SYNC\n") > syncs, "foreign-window request processed")
        enters = log().count("POINTER_ENTER 1 ")
        event("M 2020 100")
        wait(lambda: "MOTION 0 1920.00 20.00\n" in log(), "held grab retains original local coordinates")
        assert log().count("POINTER_ENTER 1 ") == enters
        event("B 272 0")
        wait(lambda: log().count("POINTER_ENTER 1 ") > enters, "hover changes only after release")
        snapshot("foreign-window move rejected; implicit grab retained", {0: [(105, 85, blue)], 1: [(45, 45, green)]})
        event("B 272 1")
        wait(lambda: log().count("BUTTON 1 272 1\n") >= 2, "second window press")
        command("d", "DESTROY 1\n")
        wait(lambda: "UNMAP 1\n" in log("server"), "unmap held origin")
        releases = log().count("BUTTON 0 272 0\n")
        event("M 200 180"); event("B 272 0")
        wait(lambda: log().count("POINTER_ENTER 0 ") >= 3, "pointer recovers after origin gone")
        assert log().count("BUTTON 0 272 0\n") == releases
        snapshot("unmap held origin does not redirect release", {0: [(105, 85, blue)], 1: [(45, 45, bg)]})
        key(42, True)
        event("D")
        wait(lambda: "CAPS 0\n" in log(), "device removal withdraws capabilities")
        maps = log().count("KEYMAP\n")
        enters = log().count("KEYBOARD_ENTER 0\n")
        event("A")
        wait(lambda: log().count("KEYMAP\n") > maps and log().count("KEYBOARD_ENTER 0\n") > enters,
             "fresh keyboard resource actually bound and entered")
        before = log().count("KEY 0 30 1 97\n")
        key(30, True); key(30, False)
        wait(lambda: log().count("KEY 0 30 1 97\n") > before, "replacement keyboard has no stuck Shift")
        cases.append("device removal and fresh keyboard without stale modifiers")
        maps = log().count("KEYMAP\n")
        enters = log().count("KEYBOARD_ENTER 0\n")
        event("D"); event("G")
        wait(lambda: log().count("KEYMAP\n") > maps and log().count("KEYBOARD_ENTER 0\n") > enters,
             "German keyboard resource actually bound and entered")
        key(21, True); key(21, False)
        wait(lambda: "KEY 0 21 1 122\n" in log(), "German Z at the US Y physical key")
        cases.append("German keymap retained instead of replaced with US")
        client.stdin.write(b"q"); client.stdin.flush()
        assert client.wait(timeout=6) == 0, log()
        server.terminate()
        assert server.wait(timeout=6) == 0, log("server")
        result = {"result": "passed", "scope": "native input fixture; no hardware or trusted session", "cases": cases, "images": images}
        (root / "result.json").write_text(json.dumps(result, indent=2) + "\n")
        print(json.dumps({**result, "artifacts": str(root)}))
    finally:
        for p in reversed(processes):
            if p.poll() is None:
                p.terminate()
                try: p.wait(timeout=3)
                except subprocess.TimeoutExpired: p.kill(); p.wait(timeout=3)
            p.stdin.close()
        for stream in streams: stream.close()
        controller.close(); issuer.close()
        print("Input evidence:", root)


if __name__ == "__main__":
    main()
