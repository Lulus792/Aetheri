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
        sdk = Path(json.loads((ROOT / "spec/nexaui-inputs.json").read_text())["development_builds"]["release"]) / "dbus-sdk"
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

    def capture(label, checks=()):
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
                if not checks:
                    assert sample != (16, 20, 28), (label, sample)
                assert len({pixel(x, y) for y in range(0, height, 2) for x in range(0, width, 2)}) > 10
                for x, y, expected in checks:
                    assert pixel(x, y) == expected, (label, x, y, pixel(x,y), expected)
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
                       issuer.fileno(), pass_fds=(issuer.fileno(),), extra_env={**env,"WAYLAND_DEBUG":"server"})
        issuer.close()
        wait(lambda: "READY " in log("server"), "compositor ready")
        app = start(app_binary, "app", "--font", font, "--diagnostics")
        wait(lambda: model() == (0, 0, 100, 0) and "MAP 0\n" in log("server"), "initial Settings view")
        outputs = re.findall(r"^OUTPUT (\d+) (\d+) (\d+) (\d+) (\d+) (\d+)$", log(), re.M)
        assert {(int(row[1]), int(row[2])) for row in outputs} == {(1920, 1080), (2560, 1440), (3840, 2160)}
        maps = Path(f"/proc/{app.pid}/maps").read_text()
        assert "libdbus-1.so.3" in maps
        loaded = {Path(line.split()[-1]).resolve() for line in maps.splitlines() if "/libnexaui.so" in line}
        assert len(loaded) == 1
        library = loaded.pop()
        candidate = library.parents[2]
        pin = json.loads((ROOT / "spec/nexaui-inputs.json").read_text())
        assert str(candidate) in pin["development_builds"].values()
        assert library.name == "libnexaui.so." + pin["version"]
        metadata = json.loads((candidate / "build.json").read_text())
        assert metadata["revision"] == pin["revision"] and metadata["version"] == pin["version"] and metadata["atspi_compiled"]
        assert digest(library) == metadata["artifacts"][library.relative_to(candidate).as_posix()]
        assert not any(name in maps for name in ("libSDL", "libvulkan", "libEGL", "libGLX", "libX11"))
        (root / "loaded-maps.txt").write_text(maps)
        capture("own Settings display page")
        click(53)
        wait(lambda: model() == (0,0,100,3), "native Sound navigation")
        capture("native Sound unavailable without an admitted audio owner")
        click(50)
        wait(lambda: model() == (0,0,100,0), "native return to Displays")
        # Native title dragging waits for the app to own the original press,
        # then for the actual server RPC before advancing the compositor grab.
        moves = log("server").count(".move(")
        titles = log().count("CHROME_TITLE down\n")
        event("M 220 56"); event("B 272 1")
        wait(lambda: log().count("CHROME_TITLE down\n") > titles, "native title press")
        event("M 228 56")
        wait(lambda: log("server").count(".move(") > moves, "native move request")
        event("M 308 116"); event("B 272 0")
        capture("native title drag moves the actual surface", ((50,80,(16,20,28)),(130,160,(28,31,40))))
        resizes = log("server").count(".resize(")
        event("M 1118 818"); event("B 272 1")
        wait(lambda: log("server").count(".resize(") > resizes, "native bottom-right resize request")
        event("M 1298 918")
        wait(lambda: "FRAME 1180 820 " in log(), "native larger configure")
        event("B 272 0")
        capture("native lower-right resize", ((1250,900,(40,44,54)),))
        resizes = log("server").count(".resize(")
        event("M 1298 918"); event("B 272 1")
        wait(lambda: log("server").count(".resize(") > resizes, "native return resize request")
        event("M 1118 818")
        wait(lambda: re.findall(r"^FRAME (\d+) (\d+) ", log(), re.M)[-1] == ("1000","720"), "native original size")
        event("B 272 0")
        moves = log("server").count(".move(")
        titles = log().count("CHROME_TITLE down\n")
        event("M 300 116"); event("B 272 1")
        wait(lambda: log().count("CHROME_TITLE down\n") > titles, "native return title press")
        event("M 308 116")
        wait(lambda: log("server").count(".move(") > moves, "native return move request")
        event("M 228 56"); event("B 272 0")
        capture("native title and size restored", ((50,80,(28,31,40)),(1100,800,(16,20,28))))
        def edge_drag(x,y,w,h,left,right,top,bottom,dx,dy):
            px=x+2 if left else x+w-2 if right else x+w/2
            py=y+2 if top else y+h-2 if bottom else y+h/2
            before=log("server").count(".resize(")
            event(f"M {px} {py}");event("B 272 1")
            wait(lambda:log("server").count(".resize(")>before,"native edge admission")
            event(f"M {px+dx} {py+dy}")
            expected=(str(int(w+(-dx if left else dx if right else 0))),str(int(h+(-dy if top else dy if bottom else 0))))
            wait(lambda:re.findall(r"^FRAME (\d+) (\d+) ",log(),re.M)[-1]==expected,"native edge configure "+str(expected))
            event("B 272 0")
            return x+(dx if left else 0),y+(dy if top else 0),float(expected[0]),float(expected[1])
        for left,right,top,bottom in ((1,0,1,0),(0,0,1,0),(0,1,1,0),(0,1,0,0),
                                      (0,1,0,1),(0,0,0,1),(1,0,0,1),(1,0,0,0)):
            dx=-20 if left else 20 if right else 0;dy=-15 if top else 15 if bottom else 0
            geometry=edge_drag(40,40,1000,720,left,right,top,bottom,dx,dy)
            restored=edge_drag(*geometry,left,right,top,bottom,-dx,-dy)
            assert restored==(40,40,1000,720)
        capture("all eight native resize edges restore their original geometry", ((50,80,(28,31,40)),(1100,800,(16,20,28))))
        # Escape cancels an app-owned pending caption press, including repeats.
        moves=log("server").count(".move(");titles=log().count("CHROME_TITLE down\n")
        cancelled=log().count("CHROME_TITLE cancel\n")
        event("M 220 56");event("B 272 1")
        wait(lambda:log().count("CHROME_TITLE down\n")>titles,"pending caption before Escape")
        event("K 1 1")
        wait(lambda:log().count("CHROME_TITLE cancel\n")>cancelled,"pending caption cancellation")
        event("K 1 1");event("K 1 0");event("M 308 116");event("B 272 0")
        assert log("server").count(".move(")==moves and app.poll() is None
        capture("Escape cancels pending caption without closing", ((50,80,(28,31,40)),(1100,800,(16,20,28))))
        # Once the compositor owns the move, it consumes Escape and restores
        # the original geometry. Further motion under the held press is inert.
        titles=log().count("CHROME_TITLE down\n");cancelled=log("server").count("GRAB_CANCEL restored=1\n")
        event("M 220 56");event("B 272 1")
        wait(lambda:log().count("CHROME_TITLE down\n")>titles,"moving caption before Escape")
        event("M 228 56")
        wait(lambda:log("server").count(".move(")>moves,"move admission before Escape")
        event("M 308 116")
        capture("native move before cancellation", ((50,80,(16,20,28)),(130,160,(28,31,40))))
        event("K 1 1")
        wait(lambda:log("server").count("GRAB_CANCEL restored=1\n")>cancelled,"compositor cancels original move")
        event("K 1 1");event("K 1 0");event("M 388 176");event("B 272 0")
        capture("Escape restores native move and consumes repeat", ((50,80,(28,31,40)),(1100,800,(16,20,28))))
        resizes=log("server").count(".resize(");cancelled=log("server").count("GRAB_CANCEL restored=1\n")
        event("M 1038 758");event("B 272 1")
        wait(lambda:log("server").count(".resize(")>resizes,"resize admission before Escape")
        event("M 1138 808")
        wait(lambda:re.findall(r"^FRAME (\d+) (\d+) ",log(),re.M)[-1]==("1100","770"),"larger size before Escape")
        event("K 1 1")
        wait(lambda:log("server").count("GRAB_CANCEL restored=1\n")>cancelled,"compositor cancels original resize")
        event("K 1 1");event("K 1 0");event("M 1238 858");event("B 272 0")
        wait(lambda:re.findall(r"^FRAME (\d+) (\d+) ",log(),re.M)[-1]==("1000","720"),"original size after Escape")
        capture("Escape restores native resize without continuing held gesture", ((50,80,(28,31,40)),(1100,800,(16,20,28))))
        assert model()==(0,0,100,0)
        # A title-origin gesture released over Close never becomes a button click.
        moves = log("server").count(".move(")
        titles = log().count("CHROME_TITLE down\n")
        event("M 220 56"); event("B 272 1")
        wait(lambda: log().count("CHROME_TITLE down\n") > titles, "title owns release")
        event("M 61 56")
        wait(lambda: log("server").count(".move(") > moves, "native crossing gesture")
        event("B 272 0")
        wait(lambda: log().count("FRAME ") > 5, "original title gesture finished")
        assert app.poll() is None and model() == (0,0,100,0)
        # A body control's press/release cannot be adopted as a title gesture.
        nav = re.findall(r"^CONTROL 51 ([\d.-]+) ([\d.-]+) ([\d.-]+) ([\d.-]+) ",log(),re.M)[-1]
        x,y,w,h = map(float,nav)
        moves = log("server").count(".move(")
        event(f"M {40+x+w/2} {40+y+h/2}"); event("B 272 1")
        event("M 220 56"); event("B 272 0")
        time.sleep(0.05)
        assert model() == (0,0,100,0) and log("server").count(".move(") == moves
        # Two ordinary title clicks zoom/restore; each is a real native pair.
        for expected in (("1920","1080"),("1000","720")):
            event("M 220 56" if expected[0]=="1920" else "M 180 16")
            for _ in range(2):
                event("B 272 1");event("B 272 0")
            wait(lambda: re.findall(r"^FRAME (\d+) (\d+) ",log(),re.M)[-1] == expected, "native title double click " + str(expected))
        capture("native title double-click restore")
        titles=log().count("CHROME_TITLE down\n")
        event("M 220 56");event("B 272 1")
        wait(lambda:log().count("CHROME_TITLE down\n")>titles,"title press before native device loss")
        frames=log().count("FRAME ")
        event("D")
        wait(lambda:log().count("FRAME ")>frames,"native focus/capture cancellation")
        def pointer_enters():
            return len(re.findall(r"-> wl_pointer#\d+\.enter\(",log("server")))
        enters=pointer_enters()
        event("A");event("M 220 56")
        wait(lambda:pointer_enters()>enters,"client binds the replacement native pointer")
        titles=log().count("CHROME_TITLE release\n")
        event("B 272 1");event("B 272 0")
        wait(lambda:log().count("CHROME_TITLE release\n")>titles,"fresh native caption click after device replacement")
        assert re.findall(r"^FRAME (\d+) (\d+) ",log(),re.M)[-1]==("1000","720")
        assert model()==(0,0,100,0)
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
        tab(53)
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
        tab(53)
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
        event("O 0 240 180")
        wait(lambda: re.findall(r"^FRAME (\d+) (\d+) ",log(),re.M)[-1] == ("240","180"), "below-minimum native configure")
        current = log().rsplit("FRAME ",1)[-1]
        assert re.findall(r"^CONTROL (\d+) ",current,re.M) == ["10"]
        assert model() == (2,1,200,1)
        capture("below-minimum view has only a working close control")
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
