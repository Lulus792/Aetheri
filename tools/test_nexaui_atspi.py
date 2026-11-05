"""Independent AT-SPI actions on the real NexaUI Settings/compositor connection.

The bus and registry are private, unprivileged test children. No system service,
desktop configuration, normal app identity or privileged authority is changed.
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
from xml.sax.saxutils import escape

ROOT = Path(__file__).resolve().parents[1]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    server_binary, candidate, font, fixture, output = [Path(p).resolve() for p in sys.argv[1:]]
    if os.getuid() == 0:
        raise ValueError("private native test must be unprivileged")
    record = json.loads((candidate / "build.json").read_text())
    pin = json.loads((ROOT / "spec/nexaui-inputs.json").read_text())
    assert record["revision"] == pin["revision"] and record["atspi_compiled"]
    for name, value in record["artifacts"].items():
        assert digest(candidate / name) == value, name
    for name, value in record["model"].items():
        assert digest(ROOT / name) == value, name
    assets = json.loads((fixture / "inputs.json").read_text())
    assert digest(ROOT / "tests/nexaui_settings_atspi.c") == assets["source_sha256"]
    assert digest(fixture / "settings-atspi-client") == assets["client_sha256"]
    for name, value in assets["files"].items():
        assert digest(fixture / name.lstrip("/")) == value, name
    assert digest(font) == json.loads((ROOT / "spec/ui-inputs.json").read_text())["font"]["sha256"]
    root = Path(tempfile.mkdtemp(prefix="nexaui-atspi-", dir=output))
    runtime, captures = root / "runtime", root / "captures"
    runtime.mkdir(mode=0o700)
    captures.mkdir(mode=0o700)
    address = "unix:path=" + str(runtime / "bus")
    env = {**os.environ, "XDG_RUNTIME_DIR": str(runtime), "WAYLAND_DISPLAY": "aetheri-atspi-private",
           "NEXAUI_PLATFORM": "wayland", "DBUS_SESSION_BUS_ADDRESS": address, "AT_SPI_BUS_ADDRESS": address,
           "GSETTINGS_BACKEND": "memory", "LD_LIBRARY_PATH": str(fixture / "usr/lib64"),
           "ASAN_OPTIONS": "detect_leaks=1:halt_on_error=1",
           "UBSAN_OPTIONS": "halt_on_error=1:print_stacktrace=1"}
    for key in ("DISPLAY", "WAYLAND_SOCKET", "NO_AT_BRIDGE", "DBUS_STARTER_ADDRESS", "DBUS_STARTER_BUS_TYPE"):
        env.pop(key, None)
    configuration = root / "bus.conf"
    configuration.write_text("<busconfig><type>session</type><listen>" + escape(address) + "</listen>"
        "<auth>EXTERNAL</auth><policy context=\"default\"><allow user=\"" + str(os.getuid()) + "\"/>"
        "<allow own=\"*\"/><allow send_destination=\"*\"/><allow receive_sender=\"*\"/>"
        "</policy></busconfig>\n")
    processes, streams, images = [], [], []
    controller, issuer = socket.socketpair(socket.AF_UNIX, socket.SOCK_SEQPACKET)
    controller.settimeout(4)
    parent, peer = socket.socketpair(socket.AF_UNIX, socket.SOCK_SEQPACKET)
    parent.settimeout(12)

    def start(binary, name, *args, pass_fds=(), extra_env=None):
        stream = (root / (name + ".log")).open("xb")
        streams.append(stream)
        p = subprocess.Popen([str(binary), *map(str, args)], env=extra_env or env, pass_fds=pass_fds,
                             stdin=subprocess.DEVNULL, stdout=stream, stderr=subprocess.STDOUT)
        processes.append(p)
        return p

    def log(name="app"):
        p = root / (name + ".log")
        return p.read_text(errors="replace") if p.exists() else ""

    def wait(predicate, label):
        deadline = time.monotonic() + 12
        while not predicate():
            assert all(p.poll() is None for p in processes), (label, [(p.pid, p.returncode) for p in processes], log()[-4000:], log("registry"), log("bus"))
            assert time.monotonic() < deadline, (label, log()[-4000:], log("client")[-2000:], log("registry"))
            time.sleep(0.01)

    def registry_ready():
        p = subprocess.run(["busctl", "--address=" + address, "--timeout=1", "call", "org.freedesktop.DBus",
                            "/org/freedesktop/DBus", "org.freedesktop.DBus", "NameHasOwner", "s",
                            "org.a11y.atspi.Registry"], env=env, capture_output=True, text=True, timeout=2)
        return p.returncode == 0 and p.stdout.strip() == "b true"

    def event(command):
        controller.sendall(command.encode())
        assert controller.recv(32) == b"OK", command

    def key(code):
        event(f"K {code} 1")
        event(f"K {code} 0")

    def model():
        values = re.findall(r"^MODEL theme=(\d+) german=(\d+) text=(\d+)$", log(), re.M)
        return tuple(map(int, values[-1])) if values else None

    def capture(label):
        sequence = len(images) // 3 + 1
        server.send_signal(signal.SIGUSR1)
        wait(lambda: all(f"CAPTURE {sequence} {i}\n" in log("server") for i in range(3)), label)
        for i in range(3):
            path = captures / f"capture-{sequence}-output-{i}.ppm"
            raw = path.read_bytes()
            magic, dimensions, peak, pixels = raw.split(b"\n", 3)
            width, height = map(int, dimensions.split())
            assert magic == b"P6" and peak == b"255" and len(pixels) == width * height * 3
            if i == active_output:
                assert len(set(pixels)) > 16
                # A presented view contains more than the uniform compositor background.
                assert pixels != bytes((16, 20, 28)) * (width * height)
            packed = path.with_suffix(".ppm.gz")
            packed.write_bytes(gzip.compress(raw, mtime=0))
            path.unlink()
            images.append({"case": label, "output": i, "file": packed.name,
                           "raw_sha256": hashlib.sha256(raw).hexdigest(), "file_sha256": digest(packed)})

    app_binary = candidate / "consumer-build/aetheri_model_view"
    try:
        bus = start(fixture / "usr/bin/dbus-daemon", "bus", "--nofork", "--config-file=" + str(configuration), "--print-address=1")
        wait(lambda: (runtime / "bus").is_socket(), "private bus")
        registry = start(fixture / "usr/libexec/at-spi2-registryd", "registry")
        wait(registry_ready, "real registry owner")
        server = start(server_binary, "server", "--headless", env["WAYLAND_DISPLAY"], captures,
                       issuer.fileno(), pass_fds=(issuer.fileno(),))
        issuer.close()
        wait(lambda: "READY " in log("server"), "Aetheri compositor ready")
        # The identical independent client must reject a real window whose
        # native bridge is disabled; a readable/model-only view cannot pass.
        negative = start(app_binary, "negative-app", "--font", font, "--diagnostics", extra_env={**env, "NO_AT_BRIDGE": "1"})
        wait(lambda: "MODEL theme=0 german=0 text=100" in log("negative-app"), "negative native window")
        rejected = start(fixture / "settings-atspi-client", "negative-client", negative.pid, peer.fileno(), 1920, 1080, pass_fds=(peer.fileno(),))
        assert rejected.wait(timeout=10) == 1 and "target app was not registered" in log("negative-client"), log("negative-client")
        processes.remove(rejected)
        assert "MODEL theme=2" not in log("negative-app")
        negative.terminate()
        assert negative.wait(timeout=8) == 0, log("negative-app")
        processes.remove(negative)
        wait(lambda: "UNMAP 0\n" in log("server"), "negative window retired")
        app = start(app_binary, "app", "--font", font, "--diagnostics")
        wait(lambda: model() == (0, 0, 100), "original model default view")
        # The compositor allocates a new ordinary output slot after the first
        # window retires. Test current placement rather than assuming slot zero.
        active_output = int(re.findall(r"^MAP (\d+)$", log("server"), re.M)[-1])
        assert active_output == 1
        full_width, full_height = 2560, 1440
        client = start(fixture / "settings-atspi-client", "client", app.pid, peer.fileno(),
                       full_width, full_height, pass_fds=(peer.fileno(),))
        peer.close()
        assert parent.recv(32) == b"SMALL", log("client")
        wait(lambda: model() == (2, 1, 200), "AT-SPI changed actual model")
        capture("independent AT-SPI changes German contrast and 200 percent")
        event("K 125 1")
        key(103)
        event("K 125 0")
        wait(lambda: f"FRAME {full_width} {full_height} " in log(), "actual maximize")
        event(f"O {active_output} 320 320")
        wait(lambda: "FRAME 320 320 " in log(), "native small view")
        parent.sendall(b"K")
        assert parent.recv(32) == b"LARGE", log("client")
        wait(lambda: model() == (2, 1, 150) and "SELECTED_OUTPUT 7\n" in log(), "small native AT-SPI preference/display actions")
        capture("small native view after independent 150 percent and display action")
        event(f"O {active_output} {full_width} {full_height}")
        wait(lambda: re.findall(r"^FRAME (\d+) (\d+) ", log(), re.M)[-1] == (str(full_width), str(full_height)), "native restore")
        parent.sendall(b"K")
        assert parent.recv(32) == b"CLOSE", log("client")
        assert model() == (2, 1, 150)
        capture("restored native accessible model")
        maps = {"app": Path(f"/proc/{app.pid}/maps").read_text(), "client": Path(f"/proc/{client.pid}/maps").read_text()}
        assert str(candidate / "prefix/lib/libnexaui.so") in maps["app"]
        assert not any(s in maps["app"] for s in ("libatspi", "libSDL", "libvulkan", "libX11"))
        assert str(fixture / "usr/lib64/libatspi.so.0.0.1") in maps["client"] and "libnexaui" not in maps["client"]
        for name, value in maps.items():
            (root / (name + "-maps.txt")).write_text(value)
        parent.sendall(b"K")
        assert client.wait(timeout=8) == 0, log("client")
        processes.remove(client)
        assert app.wait(timeout=8) == 0, log()
        processes.remove(app)
        wait(lambda: f"UNMAP {active_output}\n" in log("server"), "positive accessible view retired")
        for process in (server, registry, bus):
            process.terminate()
            assert process.wait(timeout=8) in (0, -signal.SIGTERM)
            processes.remove(process)
        for name in ("app", "client", "negative-app", "negative-client", "server", "registry", "bus"):
            assert not re.search(r"AddressSanitizer|LeakSanitizer|runtime error:|GLib-GObject-CRITICAL", log(name)), (name, log(name)[-4000:])
        result = {"result": "passed", "revision": pin["revision"], "sanitizers": record["sanitizers"],
                  "native_peer": "independent C17/libatspi with no NexaUI or model linkage",
                  "negative": "same client rejects visible NO_AT_BRIDGE window without model effects",
                  "images": images, "scope": "real private AT-SPI registry/actions on Aetheri compositor; no human screenreader, physical or normal session acceptance"}
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
        parent.close()
        peer.close()
        print("NexaUI AT-SPI evidence:", root)


if __name__ == "__main__":
    main()
