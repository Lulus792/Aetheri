"""Actual nested wlroots presentation and seat forwarding through a private parent."""
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
    parent_binary, compositor, client_binary, build = map(lambda s: Path(s).resolve(), sys.argv[1:])
    if os.getuid() == 0:
        raise RuntimeError("nested test is unprivileged")
    root = Path(tempfile.mkdtemp(prefix="compositor-nested-", dir=build))
    runtime, captures = root / "runtime", root / "captures"
    runtime.mkdir(mode=0o700); captures.mkdir(mode=0o700)
    env = {**os.environ, "XDG_RUNTIME_DIR": str(runtime), "WAYLAND_DISPLAY": "nested-child"}
    for key in ("DISPLAY", "WAYLAND_SOCKET"): env.pop(key, None)
    controller, issuer = socket.socketpair(socket.AF_UNIX, socket.SOCK_SEQPACKET)
    controller.settimeout(4)
    processes, streams = [], []

    def start(binary, name, *args, pass_fds=()):
        f = (root / (name + ".log")).open("wb"); streams.append(f)
        p = subprocess.Popen([str(binary), *map(str, args)], env=env, stdin=subprocess.PIPE,
                             stdout=f, stderr=subprocess.STDOUT, pass_fds=pass_fds)
        processes.append(p)
        return p

    def log(name): return (root / (name + ".log")).read_text(errors="replace")

    def wait(predicate, label, live):
        deadline = time.monotonic() + 8
        while not predicate():
            assert all(p.poll() is None for p in live), (label, log("child"))
            assert time.monotonic() < deadline, (label, log("child"), log("client") if (root / "client.log").exists() else "")
            time.sleep(0.01)

    def event(text):
        controller.sendall(text.encode()); assert controller.recv(32) == b"OK"

    try:
        parent = start(parent_binary, "parent", "--headless", "nested-parent", captures, issuer.fileno(), pass_fds=(issuer.fileno(),))
        issuer.close()
        wait(lambda: "READY " in log("parent"), "parent ready", (parent,))
        child = start(compositor, "child", "--nested", env["WAYLAND_DISPLAY"], "-", "nested-parent")
        wait(lambda: "READY " in log("child"), "nested native outputs ready", (parent, child))
        client = start(client_binary, "client", "--input")
        wait(lambda: "FRAME 1 " in log("client") and "KEYMAP\n" in log("client"), "native child windows", (parent, child, client))
        event("M 140 140"); event("B 272 1"); event("B 272 0")
        event("K 30 1"); event("K 30 0")
        wait(lambda: "KEY 0 30 1 97\n" in log("client"), "nested pointer/keyboard first window", (parent, child, client))
        event("M 2060 140"); event("B 272 1"); event("B 272 0")
        event("K 48 1"); event("K 48 0")
        wait(lambda: "KEY 1 48 1 98\n" in log("client"), "nested second-output input", (parent, child, client))
        parent.send_signal(signal.SIGUSR1)
        wait(lambda: all(f"CAPTURE 1 {i}\n" in log("parent") for i in range(3)), "parent composition", (parent, child, client))
        colors = [(41, 99, 196), (38, 180, 120), (16, 20, 28)]
        images = []
        for i, color in enumerate(colors):
            path = captures / f"capture-1-output-{i}.ppm"
            raw = path.read_bytes(); magic, dims, peak, pixels = raw.split(b"\n", 3)
            width, height = map(int, dims.split())
            assert magic == b"P6" and peak == b"255" and len(pixels) == width * height * 3
            offset = (85 * width + 85) * 3
            assert tuple(pixels[offset:offset + 3]) == color, (i, pixels[offset:offset + 3])
            packed = path.with_suffix(".ppm.gz")
            with packed.open("xb") as f: f.write(gzip.compress(raw, mtime=0))
            images.append({"file": packed.name, "sha256": hashlib.sha256(raw).hexdigest()})
            path.unlink()
        client.stdin.write(b"q"); client.stdin.flush()
        assert client.wait(timeout=6) == 0, log("client")
        parent.terminate()
        assert parent.wait(timeout=6) == 0, log("parent")
        # Losing the selected parent must end the child, never choose a fallback.
        assert child.wait(timeout=6) == 1, log("child")
        assert not (runtime / env["WAYLAND_DISPLAY"]).exists()
        result = {"result": "passed", "cases": ["native nested output pixels on three parent outputs",
            "real nested pointer and keyboard forwarding on two outputs", "selected parent loss exits without fallback"],
            "images": images, "scope": "private parent; physical display/input remain untested"}
        (root / "result.json").write_text(json.dumps(result, indent=2) + "\n")
        print(json.dumps({**result, "artifacts": str(root)}))
    finally:
        for p in reversed(processes):
            if p.poll() is None:
                p.terminate()
                try: p.wait(timeout=3)
                except subprocess.TimeoutExpired: p.kill(); p.wait(timeout=3)
            p.stdin.close()
        for f in streams: f.close()
        controller.close(); issuer.close()
        print("Nested evidence:", root)


if __name__ == "__main__": main()
