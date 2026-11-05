"""Actual private shell roles, pixels, input and foreign-connection denials."""
import gzip
import hashlib
import json
import os
from pathlib import Path
import re
import signal
import socket
import subprocess
import sys
import tempfile
import time


def main():
    server_bin, desktop_bin, client_bin, build = map(lambda s: Path(s).resolve(), sys.argv[1:])
    assert os.getuid() != 0, "run without root"
    root = Path(tempfile.mkdtemp(prefix="desktop-", dir=build))
    runtime, captures = root / "runtime", root / "captures"
    runtime.mkdir(mode=0o700); captures.mkdir(mode=0o700)
    env = {**os.environ, "XDG_RUNTIME_DIR": str(runtime), "WAYLAND_DISPLAY": "desktop-private"}
    for key in ("DISPLAY", "WAYLAND_SOCKET"):
        env.pop(key, None)
    processes, streams, sockets, cases, images = [], [], [], [], []

    def start(binary, name, args=(), pass_fds=(), child_env=None):
        stream = (root / (name + ".log")).open("wb"); streams.append(stream)
        p = subprocess.Popen([str(binary), *map(str, args)], env=child_env or env,
                             stdin=subprocess.PIPE, stdout=stream, stderr=subprocess.STDOUT,
                             pass_fds=pass_fds)
        processes.append(p)
        return p

    def log(name):
        return (root / (name + ".log")).read_text(errors="replace")

    def wait(predicate, label, live=()):
        deadline = time.monotonic() + 7
        while not predicate():
            assert all(p.poll() is None for p in live), (label, [(p.pid, p.returncode) for p in live], root)
            assert time.monotonic() < deadline, (label, root)
            time.sleep(0.01)

    def command(p, c):
        p.stdin.write(c.encode()); p.stdin.flush()

    def begin(name, mode=()):
        control, issuer = socket.socketpair(socket.AF_UNIX, socket.SOCK_SEQPACKET)
        server_fd, client_fd = socket.socketpair(socket.AF_UNIX, socket.SOCK_STREAM)
        sockets.extend((control, issuer, server_fd, client_fd)); control.settimeout(4)
        server = start(server_bin, name + "-server", ("--headless", env["WAYLAND_DISPLAY"],
            captures if name == "main" else "-", issuer.fileno(), "--shell-fd", server_fd.fileno()),
            (issuer.fileno(), server_fd.fileno()))
        issuer.close(); server_fd.close()
        wait(lambda: "READY " in log(name + "-server"), "compositor start", (server,))
        desktop = start(desktop_bin, name + "-desktop", mode, (client_fd.fileno(),),
            {**env, "WAYLAND_SOCKET": str(client_fd.fileno())})
        client_fd.close()
        return server, desktop, control

    sequence = 0

    def snapshot(label, points):
        nonlocal sequence
        sequence += 1; server.send_signal(signal.SIGUSR1)
        wait(lambda: all(f"CAPTURE {sequence} {i}\n" in log("main-server") for i in range(3)), label, (server, desktop))
        for i in range(3):
            path = captures / f"capture-{sequence}-output-{i}.ppm"
            raw = path.read_bytes(); magic, dimensions, peak, pixels = raw.split(b"\n", 3)
            width, height = map(int, dimensions.split())
            assert magic == b"P6" and peak == b"255" and len(pixels) == width * height * 3
            for x, y, color in points.get(i, []):
                offset = (y * width + x) * 3
                assert tuple(pixels[offset:offset+3]) == color, (label, i, x, y, tuple(pixels[offset:offset+3]))
            packed = path.with_suffix(".ppm.gz")
            packed.write_bytes(gzip.compress(raw, mtime=0)); path.unlink()
            images.append({"file": packed.name, "sha256": hashlib.sha256(raw).hexdigest()})
        cases.append(label)

    def event(value):
        control.sendall(value.encode()); assert control.recv(32) == b"OK"

    def key(code, down):
        event(f"K {code} {int(down)}")

    def stats():
        before = log("main-server").count("STATS ")
        server.send_signal(signal.SIGUSR2)
        wait(lambda: log("main-server").count("STATS ") > before, "stats", (server,))
        return re.findall(r"STATS windows=(\d+) commits=(\d+)", log("main-server"))[-1]

    try:
        server, desktop, control = begin("main")
        wait(lambda: "PAINT 2 3840 84\n" in log("main-desktop"), "three dock surfaces", (server, desktop))
        control_id, layer_id = re.search(r"GLOBALS (\d+) (\d+)", log("main-desktop")).groups()
        for name, args, expected in (
            ("ordinary", ("--ordinary",), "PRIVATE_GLOBALS_HIDDEN"),
            ("forge-control", ("--forge", control_id, "control"), "FORGED_BIND_DENIED"),
            ("forge-layer", ("--forge", layer_id, "layer"), "FORGED_BIND_DENIED"),
        ):
            p = start(desktop_bin, name, args)
            assert p.wait(timeout=7) == 0 and expected in log(name), log(name)
        cases.append("same-UID ordinary connection cannot discover or bind either private global, even by exact known ID")
        client = start(client_bin, "app", ("--input",))
        wait(lambda: "KEYBOARD_ENTER 1\n" in log("app") and "WINDOW 2 " in log("main-desktop"), "real app windows", (server, desktop, client))
        wait(lambda: "BOUNDS 0 1840 916\n" in log("app") and "BOUNDS 1 2480 1276\n" in log("app"),
             "normal bounds include dock and placement margins",(server,desktop,client))
        event("M 100 100");event("B 272 1");event("B 272 0")
        key(125,True);key(103,True);key(103,False);key(125,False)
        wait(lambda:"FRAME 0 1920 996 0\n" in log("app"),"native keyboard maximize",(server,desktop,client))
        restored=log("app").count("FRAME 0 360 220 0\n")
        key(125,True);key(108,True);key(108,False);key(125,False)
        wait(lambda:log("app").count("FRAME 0 360 220 0\n")>restored,"native keyboard restore",(server,desktop,client))
        assert not re.search(r"KEY 0 (103|108) [01] ",log("app")),"compositor shortcut leaked to client"
        cases.append("advisory normal bounds reserve dock/margins; native Super+Up/Down maximizes/restores without forwarding either arrow event")
        command(client, "u")
        wait(lambda: "Grüße " + "ä" * 124 in log("main-desktop"), "bounded Unicode metadata", (server, desktop, client))
        title = next(line.split(" ", 3)[3].split(" | ", 1)[0] for line in log("main-desktop").splitlines() if "Grüße " in line)
        assert len(title.encode()) == 256 and title == "Grüße " + "ä" * 124
        cases.append("descriptive metadata preserves whole UTF-8 characters, replaces controls and bounds wire strings to 256 bytes")
        dock, search, blue, bg = (52, 64, 90), (32, 42, 64), (41, 99, 196), (16, 20, 28)
        snapshot("dock on all three outputs", {0: [(45, 45, blue), (100, 1070, dock)],
            1: [(100, 1430, dock)], 2: [(100, 2150, dock), (100, 100, bg)]})
        maximized = log("app").count("FRAME 0 1920 996 0\n")
        command(client, "m")
        wait(lambda: log("app").count("FRAME 0 1920 996 0\n") > maximized, "maximized window excludes dock", (server, desktop, client))
        snapshot("maximized app respects reserved dock", {0: [(1900, 990, blue), (100, 1070, dock)]})
        event("M 100 100"); key(125, True); key(57, True); key(57, False); key(125, False)
        wait(lambda: "TOGGLE 0\n" in log("main-desktop"), "global search shortcut", (server, desktop, client))
        assert not re.search(r"^KEY \d+ 57 [01]", log("app"), re.M)
        command(desktop, "e")
        wait(lambda: "PAINT 0 1920 1080\n" in log("main-desktop") and "ENTER 0\n" in log("main-desktop"), "search focus", (server, desktop, client))
        snapshot("search overlays maximized app", {0: [(100, 100, search), (1900, 990, search)]})
        key(30, True); key(30, False)
        wait(lambda: "KEY 30 1\n" in log("main-desktop"), "text routed to search", (server, desktop, client))
        enters = log("main-desktop").count("ENTER 0\n")
        event("D"); event("A")
        wait(lambda: log("main-desktop").count("ENTER 0\n") > enters, "replacement keyboard returns to search", (server, desktop, client))
        before_keys = len(re.findall(r"^KEY ", log("app"), re.M))
        key(56, True); key(15, True); key(15, False); key(56, False)
        key(125,True);key(108,True);key(108,False);key(125,False)
        command(desktop, "a")
        wait(lambda: "ACTIVATED 1 0\n" in log("main-desktop"), "activation cannot bypass exclusive search", (server, desktop, client))
        assert len(re.findall(r"^KEY ", log("app"), re.M)) == before_keys
        command(desktop, "n")
        wait(lambda: log("main-desktop").count("PAINT 0 1920 84\n") >= 2 and "LEAVE\n" in log("main-desktop"), "collapse and restore", (server, desktop, client))
        command(desktop, "a")
        wait(lambda: "ACTIVATED 1 1\n" in log("main-desktop"), "activate exact window", (server, desktop, client))
        key(48, True); key(48, False)
        wait(lambda: "KEY 0 48 1 98\n" in log("app"), "normal input restored", (server, desktop, client))
        command(desktop, "s")
        wait(lambda: "ACTIVATED 4294967295 0\n" in log("main-desktop"), "stale window denied", (server, desktop, client))
        snapshot("collapsed search restores app and dock", {0: [(100, 100, blue), (1900, 990, blue), (100, 1070, dock)]})
        event("O 0 1600 900")
        wait(lambda: "PAINT 0 1600 84\n" in log("main-desktop") and "FRAME 0 1600 816 0\n" in log("app"),
             "output resize reconfigures dock and maximized app", (server, desktop, client))
        snapshot("resized output retains correct work area", {0: [(1500, 810, blue), (100, 890, dock)]})
        restored = log("app").count("FRAME 0 1920 996 0\n")
        paints = log("main-desktop").count("PAINT 0 1920 84\n")
        event("O 0 1920 1080")
        wait(lambda: log("app").count("FRAME 0 1920 996 0\n") > restored and
             log("main-desktop").count("PAINT 0 1920 84\n") > paints, "original output size restored", (server, desktop, client))
        snapshot("output size restoration reconfigures both surfaces", {0: [(1900, 990, blue), (100, 1070, dock)]})
        command(desktop, "u")
        wait(lambda: "FRAME 0 1920 1080 0\n" in log("app"), "unmapped dock releases reserved space", (server, desktop, client))
        snapshot("unmapped dock restores full work area", {0: [(100, 1070, blue)]})
        paints = log("main-desktop").count("PAINT 0 1920 84\n")
        restored = log("app").count("FRAME 0 1920 996 0\n")
        command(desktop, "r")
        wait(lambda: log("main-desktop").count("PAINT 0 1920 84\n") > paints, "dock remaps through fresh configure", (server, desktop, client))
        wait(lambda: log("app").count("FRAME 0 1920 996 0\n") > restored, "remapped dock reserves again", (server, desktop, client))
        snapshot("remapped dock reserves work area again", {0: [(100, 990, blue), (100, 1070, dock)]})
        # A buffer-only repaint must not trigger an unbounded configure cycle.
        paints = log("main-desktop").count("PAINT ")
        command(desktop, "p")
        wait(lambda: "SYNC p\n" in log("main-desktop"), "buffer repaint", (server, desktop, client))
        time.sleep(0.15)
        assert log("main-desktop").count("PAINT ") == paints + 1
        before = stats(); time.sleep(0.35); assert stats() == before
        cases.append("global shortcut pair consumed, exclusive search focus and Alt+Tab fence, exact activation, stale ID denial, no configure/redraw loop")
        command(client,"f")
        wait(lambda:"FRAME 0 3840 2160 1\n" in log("app"),"fullscreen on selected third output",(server,desktop,client))
        key(125,True);key(108,True);key(108,False);key(125,False)
        event("O 2 3520 1980")
        wait(lambda:"FRAME 0 3520 1980 1\n" in log("app") and "PAINT 2 3520 84\n" in log("main-desktop"),
             "selected output resize updates fullscreen despite original home",(server,desktop,client))
        snapshot("fullscreen follows selected output size with dock overlay",{2:[(3500,1800,blue),(100,1970,dock)]})
        full_frames=log("app").count("FRAME 0 3840 2160 1\n")
        event("O 2 3840 2160")
        wait(lambda:log("app").count("FRAME 0 3840 2160 1\n")>full_frames,"selected fullscreen output restored",(server,desktop,client))
        restored=log("app").count("FRAME 0 1920 996 0\n")
        command(client,"r")
        wait(lambda:log("app").count("FRAME 0 1920 996 0\n")>restored,
             "fullscreen exit preserves underlying maximize despite ignored shortcut",(server,desktop,client))
        cases.append("output changes follow current selected output; Super+Down cannot change the underlying fullscreen/maximize state")
        command(client, "d")
        wait(lambda: "CLOSED_WINDOW 2\n" in log("main-desktop"), "close event", (server, desktop, client))
        command(client, "c")
        wait(lambda: "WINDOW 3 " in log("main-desktop"), "non-reused ID", (server, desktop, client))
        enters = log("main-desktop").count("ENTER 0\n")
        command(desktop, "e")
        wait(lambda: log("main-desktop").count("ENTER 0\n") > enters, "search before app exit", (server, desktop, client))
        command(client, "q"); assert client.wait(timeout=7) == 0, log("app")
        wait(lambda: "CLOSED_WINDOW 1\n" in log("main-desktop") and "CLOSED_WINDOW 3\n" in log("main-desktop"), "all apps gone during search", (server, desktop))
        count = log("main-desktop").count("SYNC n\n"); command(desktop, "n")
        wait(lambda: log("main-desktop").count("SYNC n\n") > count, "search restores empty focus safely", (server, desktop))
        command(desktop, "q")
        assert desktop.wait(timeout=7) == 0, log("main-desktop")
        assert server.wait(timeout=7) == 0, log("main-server")
        assert not (runtime / env["WAYLAND_DISPLAY"]).exists()
        cases.append("window IDs never reused; acknowledged desktop departure ends compositor cleanly")
        for name, mode in (("lost", ()), ("invalid", ("--invalid",)), ("duplicate", ("--duplicate",))):
            s, d, _ = begin(name, mode)
            if name == "lost":
                wait(lambda: "PAINT 2 " in log(name + "-desktop"), "live desktop", (s, d))
                d.kill(); d.wait(timeout=7)
            else:
                assert d.wait(timeout=7) != 0
            assert s.wait(timeout=7) == 1, log(name + "-server")
            assert not (runtime / env["WAYLAND_DISPLAY"]).exists()
        cases.append("unexpected shell loss, invalid exclusive-zone profile and repeated control binding fail the isolated session without replacement")
        control, issuer = socket.socketpair(socket.AF_UNIX, socket.SOCK_SEQPACKET)
        wrong, other = socket.socketpair(socket.AF_UNIX, socket.SOCK_SEQPACKET)
        sockets.extend((control, issuer, wrong, other))
        bad = start(server_bin, "wrong-fd", ("--headless", env["WAYLAND_DISPLAY"], "-", issuer.fileno(), "--shell-fd", wrong.fileno()),
                    (issuer.fileno(), wrong.fileno()))
        issuer.close(); wrong.close()
        assert bad.wait(timeout=7) == 1 and "READY " not in log("wrong-fd")
        assert not (runtime / env["WAYLAND_DISPLAY"]).exists()
        cases.append("a non-stream shell FD is rejected before socket publication")
        for name in ("main-server", "main-desktop", "app", "lost-server", "invalid-server", "duplicate-server", "wrong-fd"):
            assert not re.search(r"AddressSanitizer|runtime error:|LeakSanitizer", log(name)), log(name)
        result = {"result": "passed", "scope": "unprivileged private headless Wayland; protocol probe, no product shell/admission",
                  "cases": cases, "images": images, "artifacts": str(root)}
        (root / "result.json").write_text(json.dumps(result, indent=2) + "\n")
        print(json.dumps(result))
    finally:
        for p in reversed(processes):
            if p.poll() is None:
                p.terminate()
                try: p.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    p.kill(); p.wait(timeout=3)
            if p.stdin: p.stdin.close()
        for stream in streams: stream.close()
        for sock in sockets: sock.close()
        print("Desktop evidence:", root)


if __name__ == "__main__":
    main()
