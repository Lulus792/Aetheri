"""Real private Wayland clients and composed-pixel checks; no root or host display."""
import gzip
import hashlib
import json
import os
from pathlib import Path
import re
import signal
import shutil
import subprocess
import sys
import tempfile
import time


def main():
    compositor, consumer, build = map(lambda s: Path(s).resolve(), sys.argv[1:])
    if os.getuid() == 0:
        raise RuntimeError("run the private compositor test without root")
    root = Path(tempfile.mkdtemp(prefix="compositor-", dir=build))
    runtime, captures = root / "runtime", root / "captures"
    runtime.mkdir(mode=0o700)
    captures.mkdir(mode=0o700)
    env = {**os.environ, "XDG_RUNTIME_DIR": str(runtime), "WAYLAND_DISPLAY": "aetheri-private"}
    # No inherited desktop selection can send either executable to a real session.
    for key in ("DISPLAY", "WAYLAND_SOCKET"):
        env.pop(key, None)
    processes, streams = [], []

    def start(binary, name, *args):
        stream = (root / (name + ".log")).open("wb")
        streams.append(stream)
        process = subprocess.Popen([str(binary), *map(str, args)], env=env,
                                   stdin=subprocess.PIPE, stdout=stream, stderr=subprocess.STDOUT)
        processes.append(process)
        return process

    def log(name):
        return (root / (name + ".log")).read_text(errors="replace")

    def wait(predicate, description, live=()):
        deadline = time.monotonic() + 8
        while not predicate():
            if any(p.poll() is not None for p in live):
                raise AssertionError("process exited: " + description)
            if time.monotonic() >= deadline:
                raise AssertionError("timeout: " + description)
            time.sleep(0.01)

    def command(process, value):
        process.stdin.write(value.encode("ascii"))
        process.stdin.flush()

    def stats():
        before = log("server").count("STATS ")
        server.send_signal(signal.SIGUSR2)
        wait(lambda: log("server").count("STATS ") > before, "statistics", (server,))
        return tuple(map(int, re.findall(r"STATS windows=(\d+) commits=(\d+)", log("server"))[-1]))

    dimensions = [(1920, 1080), (2560, 1440), (3840, 2160)]
    sequence = 0
    evidence = []
    images = []

    def snapshot(label, expected):
        nonlocal sequence
        sequence += 1
        server.send_signal(signal.SIGUSR1)
        wait(lambda: all(f"CAPTURE {sequence} {i}\n" in log("server") for i in range(3)),
             "composited images: " + label, (server,))
        for i, points in enumerate(expected):
            path = captures / f"capture-{sequence}-output-{i}.ppm"
            with path.open("rb") as source:
                assert source.readline() == b"P6\n"
                assert tuple(map(int, source.readline().split())) == dimensions[i]
                assert source.readline() == b"255\n"
                pixels = source.read()
            width, height = dimensions[i]
            assert len(pixels) == width * height * 3
            for x, y, color in points:
                offset = (y * width + x) * 3
                assert tuple(pixels[offset:offset + 3]) == color, (label, i, x, y, pixels[offset:offset + 3], color)
            with path.open("rb") as source:
                digest = hashlib.file_digest(source, "sha256").hexdigest()
                source.seek(0)
                packed = path.with_suffix(".ppm.gz")
                with packed.open("xb") as target, gzip.GzipFile(fileobj=target, mode="wb", mtime=0) as zipped:
                    shutil.copyfileobj(source, zipped)
            images.append({"capture": sequence, "output": i, "sha256": digest, "file": packed.name})
            path.unlink()  # Exact test-created file; lossless original bytes remain in gzip.
        evidence.append(label)

    bg, blue, green, inset = (16, 20, 28), (41, 99, 196), (38, 180, 120), (233, 237, 245)
    empty = [[(60, 60, bg), (0, 0, bg)] for _ in range(3)]
    try:
        # Argument/runtime guards run before opening a display or render backend.
        for args, alternate in [(["--headless", "../bad", "-"], env),
                                (["--headless", "bad", "-"], {**env, "XDG_RUNTIME_DIR": str(root / "absent")})]:
            rejected = subprocess.run([str(compositor), *args], env=alternate, capture_output=True, timeout=8)
            assert rejected.returncode == 2, rejected.stderr
        server = start(compositor, "server", "--headless", env["WAYLAND_DISPLAY"], captures)
        wait(lambda: "READY " in log("server"), "private compositor ready", (server,))
        collision = subprocess.run([str(compositor), "--headless", env["WAYLAND_DISPLAY"], "-"],
                                   env=env, capture_output=True, timeout=8)
        (root / "collision.log").write_bytes(collision.stdout + collision.stderr)
        assert collision.returncode == 1
        client = start(consumer, "client")
        wait(lambda: "FRAME 0 360 220 0\n" in log("client") and "FRAME 1 360 220 0\n" in log("client"), "two native mapped windows", (server, client))
        for w, h in dimensions:
            assert f"OUTPUT_MODE {w} {h}\n" in log("client")
        assert "CAPABILITIES 0\n" in log("client") and "CAPABILITIES 1\n" in log("client")
        snapshot("two independent windows on separate outputs", [
            [(0, 0, bg), (45, 45, blue), (60, 60, inset), (401, 261, bg)],
            [(0, 0, bg), (45, 45, green), (60, 60, inset)], empty[2]])
        before = stats()
        command(client, "p")
        wait(lambda: log("client").count("FRAME 0 360 220 0\n") >= 2, "callback without damage", (server, client))
        after_callback = stats()
        assert after_callback[0] == before[0] and 0 <= after_callback[1] - before[1] <= 1
        evidence.append("frame callback without surface damage; at most one requested output commit")
        command(client, "m")
        wait(lambda: "CONFIG 0 1920 1080 0\n" in log("client"), "maximize configure", (server, client))
        # A configure log precedes server commit; wait for its matching frame.
        wait(lambda: "FRAME 0 1920 1080 0\n" in log("client"), "maximize presented", (server, client))
        snapshot("maximize", [[(0, 0, blue), (1919, 1079, blue)], [(45, 45, green)], empty[2]])
        command(client, "n")
        wait(lambda: log("client").count("CONFIG 0 360 220 0\n") >= 2, "unmaximize", (server, client))
        wait(lambda: log("client").count("FRAME 0 360 220 0\n") >= 3, "unmaximize presented", (server, client))
        command(client, "f")
        wait(lambda: "CONFIG 0 3840 2160 1\n" in log("client"), "fullscreen selected third output", (server, client))
        wait(lambda: "FRAME 0 3840 2160 1\n" in log("client"), "fullscreen presented", (server, client))
        snapshot("fullscreen moves to 4K output", [empty[0], [(45, 45, green)], [(0, 0, blue), (3839, 2159, blue), (20, 20, inset)]])
        command(client, "r")
        wait(lambda: log("client").count("CONFIG 0 360 220 0\n") >= 3, "fullscreen restore", (server, client))
        wait(lambda: stats()[0] == 2, "two windows remain", (server, client))
        # Roundtrip capture observes the restore commit queued before the client's frame reply.
        wait(lambda: log("client").count("FRAME 0 360 220 0\n") >= 4, "restore presented", (server, client))
        snapshot("restore", [[(45, 45, blue)], [(45, 45, green)], empty[2]])
        command(client, "d")
        wait(lambda: "UNMAP 1\n" in log("server"), "destroy one window", (server, client))
        snapshot("independent window close", [[(45, 45, blue)], empty[1], empty[2]])
        command(client, "c")
        wait(lambda: log("client").count("FRAME 1 360 220 0\n") >= 2, "new window", (server, client))
        snapshot("recreate on third output", [[(45, 45, blue)], empty[1], [(45, 45, green)]])
        before = stats()
        time.sleep(0.3)
        assert stats() == before
        evidence.append("mapped unchanged windows have no repeated commits")
        # Another process negotiates xdg-shell v3 while the v5 consumer stays live.
        crashed = start(consumer, "crash", "--legacy")
        wait(lambda: "FRAME 1 360 220 0\n" in log("crash"), "concurrent legacy consumer", (server, client, crashed))
        assert stats()[0] == 4
        crashed.kill()
        crashed.wait(timeout=8)
        wait(lambda: stats()[0] == 2, "crash preserves other process windows", (server, client))
        snapshot("legacy process crash preserves original windows", [[(45, 45, blue)], empty[1], [(45, 45, green)]])
        command(client, "q")
        assert client.wait(timeout=8) == 0, log("client")
        wait(lambda: stats()[0] == 0, "all cleanly closed", (server,))
        snapshot("clean disconnect clears surfaces", empty)
        before = stats()
        time.sleep(0.3)
        after = stats()
        assert before == after, ("idle commits", before, after)
        evidence.append("unchanged scene has no repeated commits")
        server.terminate()
        assert server.wait(timeout=8) == 0, log("server")
        assert not (runtime / env["WAYLAND_DISPLAY"]).exists()
        report = {"result": "passed", "scope": "private headless pixman; no seat or trusted session",
                  "cases": evidence, "dimensions": dimensions, "images": images}
        (root / "result.json").write_text(json.dumps(report, indent=2) + "\n")
        print(json.dumps({**report, "artifacts": str(root)}))
    finally:
        for process in reversed(processes):
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait(timeout=3)
            if process.stdin:
                process.stdin.close()
        for stream in streams:
            stream.close()
        # Preserve all outputs on failure as well as the successful original evidence.
        print("Compositor evidence:", root)


if __name__ == "__main__":
    main()
