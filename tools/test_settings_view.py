"""Exercise Aetheri-owned Settings presentation through native Wayland input.

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
from xml.sax.saxutils import escape

ROOT = Path(__file__).resolve().parents[1]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    if len(sys.argv) not in (5, 6):
        raise ValueError("expected server, app, font, output and optional private AT-SPI fixture")
    server_binary, app_binary, font, artifacts = [Path(p).resolve() for p in sys.argv[1:5]]
    fixture = Path(sys.argv[5]).resolve() if len(sys.argv) == 6 else None
    if os.getuid() == 0:
        raise ValueError("use an unprivileged disposable test account")
    assert digest(font) == json.loads((ROOT / "spec/ui-inputs.json").read_text())["font"]["sha256"]
    root = Path(tempfile.mkdtemp(prefix="settings-view-", dir=artifacts))
    runtime, captures = root / "runtime", root / "captures"
    runtime.mkdir(mode=0o700)
    captures.mkdir(mode=0o700)
    env = {**os.environ, "XDG_RUNTIME_DIR": str(runtime), "WAYLAND_DISPLAY": "nexaui-settings-private",
           "NEXAUI_PLATFORM": "wayland", "DBUS_SESSION_BUS_ADDRESS": "unix:path=" + str(root / "absent-bus"),
           "ASAN_OPTIONS": "detect_leaks=1:halt_on_error=1", "UBSAN_OPTIONS": "halt_on_error=1:print_stacktrace=1"}
    for name in ("DISPLAY", "WAYLAND_SOCKET", "AT_SPI_BUS_ADDRESS"):
        env.pop(name, None)
    client_binary = root / "atspi-client"
    if fixture:
        assets = json.loads((fixture / "inputs.json").read_text())
        for name, value in assets["files"].items():
            assert digest(fixture / name.lstrip("/")) == value, name
        # Reuse verified private headers/runtime, never overwrite the earlier peer.
        sdk = Path("/home/aetheri/deps/nexaui-settings-release-02/dbus-sdk")
        command = ["cc", "-std=c17", "-Wall", "-Wextra", "-Wpedantic", "-Werror", "-O2",
                   str(ROOT / "tests/settings_view_atspi.c"), "-o", str(client_binary)]
        command += ["-I" + str(fixture / p) for p in ("usr/include/at-spi-2.0", "usr/include/glib-2.0", "usr/lib64/glib-2.0/include")]
        command += ["-I" + str(sdk / p) for p in ("include/dbus-1.0", "lib64/dbus-1.0/include")]
        command += [str(fixture / "usr/lib64/libatspi.so.0.0.1"), "-l:libgobject-2.0.so.0", "-l:libglib-2.0.so.0",
                    "-Wl,-rpath," + str(fixture / "usr/lib64"), "-Wl,-rpath-link," + str(fixture / "usr/lib64")]
        compiled = subprocess.run(command, capture_output=True, text=True)
        (root / "client-build.log").write_text(compiled.stdout + compiled.stderr)
        assert compiled.returncode == 0, compiled.stderr
        address = "unix:path=" + str(runtime / "bus")
        configuration = root / "bus.conf"
        configuration.write_text("<busconfig><type>session</type><listen>" + escape(address) + "</listen>"
            "<auth>EXTERNAL</auth><policy context=\"default\"><allow user=\"" + str(os.getuid()) + "\"/>"
            "<allow own=\"*\"/><allow send_destination=\"*\"/><allow receive_sender=\"*\"/>"
            "</policy></busconfig>\n")
        env.update(DBUS_SESSION_BUS_ADDRESS=address, AT_SPI_BUS_ADDRESS=address,
                   LD_LIBRARY_PATH=str(fixture / "usr/lib64"), GSETTINGS_BACKEND="memory")
        for name in ("NO_AT_BRIDGE", "DBUS_STARTER_ADDRESS", "DBUS_STARTER_BUS_TYPE"):
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
        values = re.findall(r"^MODEL theme=(\d+) german=(\d+) text=(\d+) page=(\d+)$", log(), re.M)
        return tuple(map(int, values[-1])) if values else None

    def tab(target):
        key(15)
        wait(lambda: focus() == target, "native Tab to " + str(target))

    def capture(label):
        def settled():
            values = re.findall(r"^SETTLED theme=(\d+) german=(\d+) text=(\d+) page=(\d+) (\d+) (\d+)$", log(), re.M)
            frames = re.findall(r"^FRAME (\d+) (\d+) ", log(), re.M)
            return values and frames and tuple(map(int, values[-1][:4])) == model() and values[-1][4:] == frames[-1]
        wait(settled, "presented stable " + label)
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
                assert len({pixel(x, y) for y in range(0, height, 2) for x in range(0, width, 2)}) > 10
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

    def click(identifier):
        controls = re.findall(r"^CONTROL " + str(identifier) + r" ([\d.-]+) ([\d.-]+) ([\d.-]+) ([\d.-]+) (\d+)$", log(), re.M)
        assert controls, identifier
        x, y, w, h = map(float, controls[-1][:4])
        event(f"M {40+x+w/2} {40+y+h/2}")
        event("B 272 1")
        event("B 272 0")

    try:
        if fixture:
            bus = start(fixture / "usr/bin/dbus-daemon", "bus", "--nofork", "--config-file=" + str(configuration))
            wait(lambda: (runtime / "bus").is_socket(), "private accessibility bus")
            registry = start(fixture / "usr/libexec/at-spi2-registryd", "registry")
            def registry_ready():
                p = subprocess.run(["busctl", "--address=" + address, "--timeout=1", "call", "org.freedesktop.DBus",
                    "/org/freedesktop/DBus", "org.freedesktop.DBus", "NameHasOwner", "s", "org.a11y.atspi.Registry"],
                    env=env, capture_output=True, text=True, timeout=2)
                return p.returncode == 0 and p.stdout.strip() == "b true"
            wait(registry_ready, "private accessibility registry")
        server = start(server_binary, "server", "--headless", env["WAYLAND_DISPLAY"], captures,
                       issuer.fileno(), pass_fds=(issuer.fileno(),))
        issuer.close()
        wait(lambda: "READY " in log("server"), "compositor ready")
        app = start(app_binary, "app", "--font", font, "--diagnostics")
        wait(lambda: model() == (0, 0, 100, 0) and "MAP 0\n" in log("server"), "initial Settings view")
        outputs = re.findall(r"^OUTPUT (\d+) (\d+) (\d+) (\d+) (\d+) (\d+)$", log(), re.M)
        assert {(int(row[1]), int(row[2])) for row in outputs} == {(1920, 1080), (2560, 1440), (3840, 2160)}
        maps = Path(f"/proc/{app.pid}/maps").read_text()
        assert "/nexaui-settings-release-02/prefix/lib/libnexaui.so" in maps or "/nexaui-settings-debug-01/prefix/lib/libnexaui.so" in maps
        assert "libdbus-1.so.3" in maps
        loaded = {Path(line.split()[-1]).resolve() for line in maps.splitlines() if "/libnexaui.so" in line}
        assert len(loaded) == 1
        library = loaded.pop()
        candidate = library.parents[2]
        pin = json.loads((ROOT / "spec/nexaui-inputs.json").read_text())
        metadata = json.loads((candidate / "build.json").read_text())
        assert metadata["revision"] == pin["revision"] and metadata["version"] == pin["version"] and metadata["atspi_compiled"]
        assert digest(library) == metadata["artifacts"][library.relative_to(candidate).as_posix()]
        assert not any(name in maps for name in ("libSDL", "libvulkan", "libEGL", "libGLX", "libX11"))
        (root / "loaded-maps.txt").write_text(maps)
        capture("own Settings display page")
        click(51)
        wait(lambda: model() == (0, 0, 100, 1), "pointer opens Appearance")
        capture("own Appearance dark")
        click(101)
        wait(lambda: model() == (1, 0, 100, 1), "pointer selects Light")
        capture("own Appearance light")
        key(15)
        wait(lambda: focus() == 102, "Tab to high contrast")
        key(57)
        wait(lambda: model() == (2, 0, 100, 1), "keyboard selects contrast")
        for target in (110, 111, 112):
            tab(target)
        key(57)
        wait(lambda: model() == (2, 0, 200, 1), "keyboard selects 200 percent")
        capture("own Appearance contrast 200 percent")
        # Wrap through the real control order to Language.
        # This private compositor does not advertise minimize; disabled native
        # operations must be skipped by keyboard traversal, not made available.
        assert re.findall(r"^CONTROL 12 .* (\d+)$", log(), re.M)[-1] == "1"
        for target in (10, 11, 50, 51, 52):
            tab(target)
        key(57)
        wait(lambda: model() == (2, 0, 200, 2), "keyboard opens Language")
        tab(120)
        tab(121)
        key(57)
        wait(lambda: model() == (2, 1, 200, 2), "keyboard selects German")
        capture("own German language 200 percent")
        tab(10)
        tab(11)
        key(57)
        wait(lambda: "FRAME 1920 1080 " in log(), "maximize")
        event("O 0 320 240")
        wait(lambda: "FRAME 320 240 " in log(), "minimum native configure")
        assert model() == (2, 1, 200, 2)
        tab(50)
        tab(51)
        key(57)
        wait(lambda: model() == (2, 1, 200, 1), "narrow keyboard navigation")
        tab(52)
        for target in (100, 101, 102, 110, 111, 112):
            tab(target)
        capture("own minimum German contrast 200 percent")
        def footer_y():
            return float(re.findall(r"^FOOTER ([\d.-]+) ", log(), re.M)[-1])
        before_footer = footer_y()
        key(107)
        wait(lambda: footer_y() < before_footer, "keyboard End reveals help")
        capture("own minimum keyboard reading")
        before_footer = footer_y()
        key(104)
        wait(lambda: footer_y() > before_footer, "keyboard PageUp preserves reading overlap")
        key(102)
        assert model() == (2, 1, 200, 1) and focus() == 112
        event("O 0 1920 1080")
        wait(lambda: re.findall(r"^FRAME (\d+) (\d+) ", log(), re.M)[-1] == ("1920", "1080"), "restore")
        capture("own restored view")
        tab(10)
        tab(11)
        key(57)
        wait(lambda: re.findall(r"^FRAME (\d+) (\d+) ", log(), re.M)[-1] == ("1000", "720"), "own restore request")
        capture("own restored normal window")
        key(57)
        wait(lambda: re.findall(r"^FRAME (\d+) (\d+) ", log(), re.M)[-1] == ("1920", "1080"), "own remaximize request")
        time.sleep(0.3)
        before = log().count("FRAME ")
        def ticks():
            fields = Path(f"/proc/{app.pid}/stat").read_text().rsplit(")", 1)[1].split()
            return int(fields[11]) + int(fields[12])
        initial = ticks()
        time.sleep(1.2)
        delta = ticks() - initial
        assert log().count("FRAME ") == before and delta <= 2, (delta, log()[-2000:])
        if fixture:
            peer = start(client_binary, "client", app.pid, "3. HEADLESS-1")
            assert peer.wait(timeout=12) == 0, log("client")
            processes.remove(peer)
        else:
            key(1)
        assert app.wait(timeout=8) == 0, log()
        processes.remove(app)
        wait(lambda: "UNMAP 0\n" in log("server"), "native view retired")
        if fixture:
            negative = start(app_binary, "negative-app", "--font", font, "--diagnostics", extra_env={**env, "NO_AT_BRIDGE": "1"})
            wait(lambda: "SETTLED " in log("negative-app"), "visible disabled bridge")
            rejected = start(client_binary, "negative-client", negative.pid, "3. HEADLESS-1")
            assert rejected.wait(timeout=10) == 1 and "target app was not registered" in log("negative-client"), log("negative-client")
            processes.remove(rejected)
            negative.terminate()
            assert negative.wait(timeout=8) == 0, log("negative-app")
            processes.remove(negative)
        for name, options in (("absent-display", {"WAYLAND_DISPLAY": "absent", "DISPLAY": ":199"}),
                              ("missing-font", {})):
            failed = start(app_binary, name, "--font", root / "absent.ttf" if name == "missing-font" else font,
                           extra_env={**env, **options})
            assert failed.wait(timeout=8) == 1, log(name)
            processes.remove(failed)
        server.terminate()
        assert server.wait(timeout=8) == 0, log("server")
        processes.remove(server)
        if fixture:
            for child in (registry, bus):
                child.terminate()
                assert child.wait(timeout=8) in (0, -signal.SIGTERM)
                processes.remove(child)
        for stream in streams:
            stream.flush()
        for diagnostic in root.glob("*.log"):
            assert not re.search(r"AddressSanitizer|LeakSanitizer|runtime error:|GLib-GObject-CRITICAL", diagnostic.read_text(errors="replace")), diagnostic
        result = {"result": "passed", "app_sha256": digest(app_binary),
                  "sdk_revision": metadata["revision"], "sdk_version": metadata["version"],
                  "binaries": {str(p): digest(p) for p in (server_binary, app_binary, library)},
                  "peer_sha256": digest(client_binary) if fixture else None,
                  "sources": {name: digest(ROOT / name) for name in (
                      "src/settings/model.c", "src/settings/model.h", "src/settings/nexaui_view.c",
                      "src/settings/nexaui_view.h", "src/settings/nexaui_main.c",
                      "tests/test_settings_view.c", "tests/settings_view_atspi.c", "tools/test_settings_view.py")},
                  "cases": cases, "images": images, "idle_cpu_ticks": delta, "idle_seconds": 1.2,
                  "atspi": "independent private registry/client positive and disabled-bridge negative" if fixture else "compiled/late-loaded; unavailable private bus only",
                  "scope": "Aetheri-owned opaque Settings view on private compositor; no default adoption or physical acceptance"}
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
        print("Aetheri Settings view evidence:", root)


if __name__ == "__main__":
    main()
