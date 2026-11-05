"""Exercise the pinned original-model NexaUI consumer on Aetheri's compositor.

This unprivileged private-seat test is not installed app, physical display or
screenreader acceptance. Its inherited input channel exists only in the test server.
"""
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

ROOT = Path(__file__).resolve().parents[1]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    server_binary, candidate, font, artifacts = [Path(p).resolve() for p in sys.argv[1:]]
    if os.getuid() == 0:
        raise ValueError("use an unprivileged disposable test account")
    config = json.loads((ROOT / "spec/nexaui-inputs.json").read_text())
    record = json.loads((candidate / "build.json").read_text())
    assert record["revision"] == config["revision"] and record["atspi_compiled"]
    for name, value in record["artifacts"].items():
        assert digest(candidate / name) == value, name
    for name, value in record["model"].items():
        assert digest(ROOT / name) == value, name
    assert digest(font) == json.loads((ROOT / "spec/ui-inputs.json").read_text())["font"]["sha256"]
    root = Path(tempfile.mkdtemp(prefix="nexaui-settings-", dir=artifacts))
    runtime, captures = root / "runtime", root / "captures"
    runtime.mkdir(mode=0o700)
    captures.mkdir(mode=0o700)
    env = {**os.environ, "XDG_RUNTIME_DIR": str(runtime), "WAYLAND_DISPLAY": "nexaui-settings-private",
           "NEXAUI_PLATFORM": "wayland", "DBUS_SESSION_BUS_ADDRESS": "unix:path=" + str(root / "absent-bus")}
    for name in ("DISPLAY", "WAYLAND_SOCKET", "AT_SPI_BUS_ADDRESS"):
        env.pop(name, None)
    controller, issuer = socket.socketpair(socket.AF_UNIX, socket.SOCK_SEQPACKET)
    controller.settimeout(4)
    processes, streams, images, cases = [], [], [], []

    def start(binary, name, *args, pass_fds=(), extra_env=None):
        stream = (root / (name + ".log")).open("xb")
        streams.append(stream)
        p = subprocess.Popen([str(binary), *map(str, args)], env=extra_env or env,
                             stdout=stream, stderr=subprocess.STDOUT, stdin=subprocess.DEVNULL, pass_fds=pass_fds)
        processes.append(p)
        return p

    def log(name="app"):
        return (root / (name + ".log")).read_text(errors="replace")

    def wait(predicate, label):
        deadline = time.monotonic() + 12
        while not predicate():
            assert all(p.poll() is None for p in processes), (label, log()[-4000:], log("server")[-2000:])
            assert time.monotonic() < deadline, (label, log()[-4000:], log("server")[-2000:])
            time.sleep(0.01)

    def event(command):
        controller.sendall(command.encode())
        assert controller.recv(32) == b"OK", command

    def key(code):
        event(f"K {code} 1")
        event(f"K {code} 0")

    def focus():
        values = re.findall(r"^FOCUS (\d+)$", log(), re.M)
        return int(values[-1]) if values else None

    def model():
        values = re.findall(r"^MODEL theme=(\d+) german=(\d+) text=(\d+)$", log(), re.M)
        return tuple(map(int, values[-1])) if values else None

    def tab(target):
        key(15)
        wait(lambda: focus() == target, "native Tab to " + str(target))

    def capture(label):
        sequence = len(cases) + 1
        server.send_signal(signal.SIGUSR1)
        wait(lambda: all(f"CAPTURE {sequence} {i}\n" in log("server") for i in range(3)), label)
        sample = None
        for i in range(3):
            path = captures / f"capture-{sequence}-output-{i}.ppm"
            raw = path.read_bytes()
            magic, dimensions, peak, pixels = raw.split(b"\n", 3)
            width, height = map(int, dimensions.split())
            assert magic == b"P6" and peak == b"255" and len(pixels) == width * height * 3
            def pixel(x, y):
                return tuple(pixels[(y * width + x) * 3:(y * width + x) * 3 + 3])
            if i == 0:
                # Initial/restored app begins at 40,40; maximized begins at 0,0.
                sample = pixel(60, 60)
                assert sample != (16, 20, 28), (label, sample)
                assert len({pixel(x, y) for y in range(64, 150) for x in range(64, min(width, 300))}) > 10
            else:
                assert pixel(80, 80) == (16, 20, 28)
            packed = path.with_suffix(".ppm.gz")
            packed.write_bytes(gzip.compress(raw, mtime=0))
            assert gzip.decompress(packed.read_bytes()) == raw
            path.unlink()
            images.append({"case": label, "output": i, "file": packed.name,
                           "raw_sha256": hashlib.sha256(raw).hexdigest(), "file_sha256": digest(packed)})
        cases.append(label)
        return sample

    app_binary = candidate / "consumer-build/aetheri_model_view"
    try:
        scene = start(candidate / "consumer-build/aetheri_model_view_test", "scene", font)
        assert scene.wait(timeout=30) == 0, log("scene")
        processes.remove(scene)
        server = start(server_binary, "server", "--headless", env["WAYLAND_DISPLAY"], captures,
                       issuer.fileno(), pass_fds=(issuer.fileno(),))
        issuer.close()
        wait(lambda: "READY " in log("server"), "compositor ready")
        app = start(app_binary, "app", "--font", font, "--diagnostics")
        wait(lambda: model() == (0, 0, 100) and "MAP 0\n" in log("server"), "initial original-model view")
        outputs = re.findall(r"^OUTPUT (\d+) (\d+) (\d+) (\d+) (\d+) (\d+)$", log(), re.M)
        assert {(int(row[1]), int(row[2])) for row in outputs} == {(1920, 1080), (2560, 1440), (3840, 2160)}
        maps = Path(f"/proc/{app.pid}/maps").read_text()
        assert str(candidate / "prefix/lib/libnexaui.so") in maps
        assert "libdbus-1.so.3" in maps
        assert not any(name in maps for name in ("libSDL", "libvulkan", "libEGL", "libGLX", "libX11"))
        (root / "loaded-maps.txt").write_text(maps)
        capture("actual three-output software view")
        # Independently captured initial marks: app origin 40,40, Light at
        # local 132..172/90..132 and Dark at 24..64/90..132.
        for x, expected in ((190, (1, 0, 100)), (85, (0, 0, 100))):
            event(f"M {x} 155")
            event("B 272 1")
            event("B 272 0")
            wait(lambda: model() == expected, "native pointer preference selection")
        assert focus() == 10
        tab(11)
        key(57)
        wait(lambda: model() == (1, 0, 100), "native Light selection")
        tab(12)
        key(57)
        wait(lambda: model() == (2, 0, 100), "native high contrast selection")
        tab(13)
        tab(14)
        key(57)
        wait(lambda: model() == (2, 1, 100), "native German selection")
        tab(16)
        tab(17)
        tab(18)
        key(57)
        wait(lambda: model() == (2, 1, 200), "native 200 percent selection")
        capture("German high contrast and 200 percent")
        event("K 125 1")
        key(103)
        event("K 125 0")
        wait(lambda: "FRAME 1920 1080 " in log(), "actual maximize configure")
        event("O 0 320 320")
        wait(lambda: "FRAME 320 320 " in log(), "narrow native configure")
        assert model() == (2, 1, 200) and focus() == 18
        # Cycle the entire control list while native focus-following scroll is active.
        display_controls = [0x100000000 + int(row[0]) for row in outputs]
        for target in (*display_controls, 19, 10, 11, 12, 13, 14, 16, 17, 18):
            tab(target)
        capture("320 by 320 retains preferences and keyboard focus")
        event("O 0 1920 1080")
        wait(lambda: re.findall(r"^FRAME (\d+) (\d+) ", log(), re.M)[-1] == ("1920", "1080"), "native restore")
        assert model() == (2, 1, 200) and focus() == 18
        capture("restored original view and preferences")
        time.sleep(0.3)
        before = log().count("FRAME ")
        def ticks():
            fields = Path(f"/proc/{app.pid}/stat").read_text().rsplit(")", 1)[1].split()
            return int(fields[11]) + int(fields[12])
        initial = ticks()
        time.sleep(1.2)
        delta = ticks() - initial
        assert log().count("FRAME ") == before and delta <= 2, (delta, log()[-2000:])
        key(1)
        assert app.wait(timeout=8) == 0, log()
        processes.remove(app)
        wait(lambda: "UNMAP 0\n" in log("server"), "native view retired")
        for name, options in (("absent-display", {"WAYLAND_DISPLAY": "absent", "DISPLAY": ":199"}),
                              ("missing-font", {})):
            failed = start(app_binary, name, "--font", root / "absent.ttf" if name == "missing-font" else font,
                           extra_env={**env, **options})
            assert failed.wait(timeout=8) == 1, log(name)
            processes.remove(failed)
        server.terminate()
        assert server.wait(timeout=8) == 0, log("server")
        processes.remove(server)
        result = {"result": "passed", "revision": config["revision"], "sanitizers": record["sanitizers"],
                  "cases": cases, "images": images, "idle_cpu_ticks": delta, "idle_seconds": 1.2,
                  "atspi": "compiled/late-loaded; unavailable private bus only, no native accessibility acceptance",
                  "scope": "original-model software view on Aetheri private compositor; no default adoption or physical acceptance"}
        (root / "result.json").write_text(json.dumps(result, indent=2) + "\n")
        print(json.dumps({**result, "artifacts": str(root)}))
    finally:
        for p in reversed(processes):
            if p.poll() is None:
                p.terminate()
                try:
                    p.wait(timeout=4)
                except subprocess.TimeoutExpired:
                    p.kill()
                    p.wait(timeout=4)
        for stream in streams:
            stream.close()
        controller.close()
        issuer.close()
        print("NexaUI Settings evidence:", root)


if __name__ == "__main__":
    main()
