"""Native seat/focus/drag checks using a separately compiled inherited-FD fixture."""
import gzip
import hashlib
import json
import os
from pathlib import Path
import select
import signal
import socket
import subprocess
import struct
import sys
import tempfile
import time


def main():
    server_binary, client_binary, build = map(lambda s: Path(s).resolve(), sys.argv[1:4])
    options = sys.argv[4:]
    lock_view = len(options) == 3 and options[0] == "--lock-view"
    managed = options == ["--managed"] or lock_view
    assert managed or not options
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
    lock, lock_peer = socket.socketpair(socket.AF_UNIX, socket.SOCK_SEQPACKET)
    lock.settimeout(4)
    private_sockets = []
    if lock_view:
        display, display_peer = socket.socketpair(socket.AF_UNIX, socket.SOCK_STREAM)
        credential, credential_peer = socket.socketpair(socket.AF_UNIX, socket.SOCK_SEQPACKET)
        credential.settimeout(4)
        private_sockets = [display, display_peer, credential, credential_peer]
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

    def lock_message(action, generation):
        return struct.pack("!5sBQ", b"AELK2", action, generation)

    def lock_expect(action, generation):
        assert lock.recv(64) == lock_message(action, generation), (action, generation, log("server"))

    sequence, images, cases = 0, [], []

    def snapshot(label, points, *, covered=False, black_outputs=(), wait_for_pixels=False):
        nonlocal sequence
        sequence += 1
        server.send_signal(signal.SIGUSR1)
        wait(lambda: all(f"CAPTURE {sequence} {i}\n" in log("server") for i in range(3)), label)
        missing = []
        for i in range(3):
            path = captures / f"capture-{sequence}-output-{i}.ppm"
            raw = path.read_bytes(); magic, dimensions, peak, pixels = raw.split(b"\n", 3)
            width, height = map(int, dimensions.split())
            assert magic == b"P6" and peak == b"255" and len(pixels) == width * height * 3
            if lock_view:
                assert (width, height) == ((1920, 1080), (2560, 1440), (3840, 2160))[i], (
                    label, i, width, height)
            if covered or i in black_outputs:
                assert pixels == bytes(len(pixels)), (label, i)
            for x, y, color in points.get(i, []):
                offset = (y * width + x) * 3
                if tuple(pixels[offset:offset + 3]) != color:
                    missing.append((i, x, y, width, height, tuple(pixels[offset:offset + 3])))
            digest = hashlib.sha256(raw).hexdigest()
            packed = path.with_suffix(".ppm.gz")
            with packed.open("xb") as f:
                f.write(gzip.compress(raw, mtime=0))
            path.unlink()
            images.append({"file": packed.name, "sha256": digest})
        if not wait_for_pixels:
            assert not missing, (label, missing)
        if not missing:
            cases.append(label)
        return missing

    def check_lock_view(view):
        accent = (179, 190, 255)
        field = (15, 19, 33)
        app_events = tuple(log().count(event) for event in ("KEY ", "BUTTON "))
        event("hold-cover")
        lock.sendall(lock_message(1, 0))
        wait(lambda: "TEST_COVER_PRESENT_HELD" in log("server"), "third display still pending")
        key(30, True); key(30, False)
        assert not select.select([lock, credential], [], [], 0.2)[0]
        snapshot("dialog stays hidden before all displays present", {}, covered=True)
        event("release-cover")
        lock_expect(3, 1)
        # A fresh, focused, visibly masked draft provides an observable readiness
        # witness without adding a production credential or focus test hook.
        for _ in range(12):
            key(30, True); key(30, False)
            time.sleep(0.03)
            key(28, True); key(28, False)
            if select.select([credential], [], [], 0.04)[0]:
                assert credential.recv(64) == b"AELV1a"
                break
        else:
            raise AssertionError("private view never received keyboard focus")
        credential.sendall(b"AELV1\x01")
        time.sleep(0.10)
        key(30, True); key(30, False)

        def wait_pixels(label, points):
            for _ in range(6):
                missing = snapshot(label, {0: points}, black_outputs=(1, 2), wait_for_pixels=True)
                if not missing:
                    return
                time.sleep(0.04)
            raise AssertionError((label, missing))

        wait_pixels("private focus and masked password on the first display",
                    [(0, 0, (0, 0, 0)), (775, 531, accent), (790, 551, accent)])
        key(1, True); key(1, False)
        wait_pixels("Escape clears the private draft", [(790, 551, field)])
        click(100, 100)
        command("t", "INPUT_SYNC\n")
        assert tuple(log().count(event) for event in ("KEY ", "BUTTON ")) == app_events, (
            "ordinary client received lock input")

        def commits():
            old = log("server").count("STATS ")
            server.send_signal(signal.SIGUSR2)
            wait(lambda: log("server").count("STATS ") > old, "idle output statistics")
            return int(log("server").strip().split("commits=")[-1].split()[0])

        # Avoid the legitimate minute tick near the sampling window.
        if 1 <= time.localtime().tm_sec <= 57:
            before = commits()
            time.sleep(0.25)
            assert commits() == before, "idle dialog repeatedly redraws"
        credential.close()
        assert view.wait(timeout=4) == 0, log("view")
        processes.remove(view)
        assert lock.recv(64) == b"", "loss of original view left the release channel live"
        snapshot("original view loss leaves all displays covered", {}, covered=True)

    blue, green, bg = (41, 99, 196), (38, 180, 120), (16, 20, 28)
    try:
        view_args = ("--lock-view-fd", display.fileno()) if lock_view else ()
        extra_fds = (display.fileno(),) if lock_view else ()
        server = start(server_binary, "server", "--headless", env["WAYLAND_DISPLAY"], captures,
                       issuer.fileno(), "--session-lock-fd" if managed else "--lock-fd", lock_peer.fileno(),
                       *view_args, pass_fds=(issuer.fileno(), lock_peer.fileno(), *extra_fds))
        issuer.close()
        lock_peer.close()
        wait(lambda: "READY " in log("server"), "server readiness")
        client = start(client_binary, "client", "--input")
        wait(lambda: "CAPS 3\n" in log() and "FRAME 1 " in log() and "KEYBOARD_ENTER 1\n" in log(), "seat and mapped windows")
        if lock_view:
            display.close()
            view_fds = (display_peer.fileno(), credential_peer.fileno())
            view = start(options[1], "view", *view_fds, options[2], "fixture", "en", pass_fds=view_fds)
            display_peer.close(); credential_peer.close()
            check_lock_view(view)
            view.stdin.close()
            client.stdin.write(b"q"); client.stdin.flush()
            assert client.wait(timeout=6) == 0, log()
            server.terminate()
            assert server.wait(timeout=6) == 0, log("server")
            (root / "result.json").write_text(json.dumps({"result": "passed", "cases": cases, "images": images}, indent=2))
            return
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
        key(125,True);key(103,True);key(103,False);key(125,False)
        event("M 160 140"); event("B 272 0")
        snapshot("serial-bound move", {0: [(45, 45, bg), (105, 85, blue)], 1: [(45, 45, green)]})
        assert "CONFIG 0 1920 1080 0\n" not in log(),"maximize bypassed active move grab"
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
        clicks = log().count("BUTTON 0 272 1\n")
        event("M 200 180")
        event("B 272 1")
        wait(lambda: log().count("BUTTON 0 272 1\n") > clicks, "held pointer before cover")
        leaves = log().count("KEYBOARD_LEAVE")
        pointer_leaves = log().count("POINTER_LEAVE")
        event("hold-cover")
        lock.sendall(lock_message(1, 0) if managed else b"AELK1")
        wait(lambda: "TEST_COVER_PRESENT_HELD\n" in log("server"), "third display presentation withheld")
        assert not select.select([lock], [], [], 0.25)[0], "acknowledged with one output pending"
        if managed:
            lock.sendall(lock_message(2, 1))
            lock_expect(5, 1)
        event("stale-cover")
        assert not select.select([lock], [], [], 0.25)[0], "acknowledged a stale frame"
        event("release-cover")
        if managed:
            lock_expect(3, 1)
        else:
            assert lock.recv(64) == b"AELK1 COVERED", log("server")
            assert lock.recv(64) == b"", "cover channel remained open after acknowledgement"
            lock.close()
        wait(lambda: log().count("KEYBOARD_LEAVE") > leaves and
             log().count("POINTER_LEAVE") > pointer_leaves, "normal clients lost focus")
        mark = len(log())
        if managed:
            lock.sendall(lock_message(2, 0))
            lock_expect(5, 0)
            key(42, True)
            lock.sendall(lock_message(2, 1))
            assert not select.select([lock], [], [], 0.25)[0], "released with held keyboard and pointer"
            key(42, False)
            assert not select.select([lock], [], [], 0.25)[0], "released with held pointer"
            command("t", "INPUT_SYNC\n")
            denied = ("KEY ", "KEYBOARD_ENTER ", "POINTER_ENTER ", "BUTTON ", "AXIS ", "MOTION ")
            assert not any(line.startswith(denied) for line in log()[mark:].splitlines()), log()[mark:]
            event("B 272 0")
            lock_expect(4, 1)
            key(30, True); key(30, False)
            wait(lambda: "KEY 0 30 1 97\n" in log()[mark:], "fresh key to restored original window")
            snapshot("same window restored after neutral input", {0: [(105, 85, blue)]})

            event("B 272 1")
            lock.sendall(lock_message(1, 1))
            lock_expect(3, 2)
            lock.sendall(lock_message(2, 1))
            lock_expect(5, 1)
            key(28, True)
            lock.sendall(lock_message(2, 2))
            assert not select.select([lock], [], [], 0.25)[0], "released with held Enter"
            other = os.fork()
            if other == 0:
                try:
                    lock.sendall(lock_message(2, 2))
                except OSError:
                    os._exit(1)
                os._exit(0)
            assert os.waitpid(other, 0) == (other, 0)
            assert lock.recv(64) == b"", "copied descriptor accepted a foreign sender"
            lock.close()
            mark = len(log())
            key(28, False)
        event("B 272 0")
        key(30, True); key(30, False)
        click(210, 190)
        event("S 30")
        event("D"); event("G")
        command("t", "INPUT_SYNC\n")
        denied = ("KEY ", "KEYBOARD_ENTER ", "POINTER_ENTER ", "BUTTON ", "AXIS ", "MOTION ")
        assert not any(line.startswith(denied) for line in log()[mark:].splitlines()), log()[mark:]
        client.stdin.write(b"c"); client.stdin.flush()
        wait(lambda: log("server").count("MAP 2\n") == 1, "new ordinary window while covered")
        snapshot("cover survives closed owner channel, held pointer and device replacement", {}, covered=True)
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
        lock.close(); lock_peer.close()
        for sock in private_sockets: sock.close()
        print("Input evidence:", root)


if __name__ == "__main__":
    main()
