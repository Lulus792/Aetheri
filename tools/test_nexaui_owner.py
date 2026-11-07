"""Real development owner/dock with a bound NexaUI Settings selection.

The inherited input channel and copied source pin are private test fixtures.
Never mutate the shared SDK or source tree to simulate candidate drift.
"""
import gzip
import hashlib
import json
import os
from pathlib import Path
import re
import select
import signal
import socket
import subprocess
import sys
import tempfile
import time

import development_session as session
from test_development_desktop import control_position,role_output


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    build = Path(sys.argv[1]).resolve()
    ui = sys.argv[2] if len(sys.argv) == 3 else "nexaui"
    assert ui in ("nexaui", "nexaui-owned")
    if os.getuid() != 1000:
        raise RuntimeError("explicit disposable carrier account required")
    source = Path(__file__).resolve().parents[1]
    profile = "debug" if "AETHERI_SANITIZERS:BOOL=ON" in (build / "CMakeCache.txt").read_text() else "release"
    session.build_for = lambda selected_profile: build
    selected = session.settings_selection(profile, ui)
    # Leave space for the fixed runtime/socket suffix within sockaddr_un.
    root = Path(tempfile.mkdtemp(prefix="nx-own-" if ui == "nexaui-owned" else "nexaui-owner-", dir=build))
    for name in ("captures", "sessions", "sessions/fixture", "sessions/fixture/runtime", "sessions/fixture/cache", "sessions/fixture/documents", "source/spec", "source/src/settings"):
        (root / name).mkdir(mode=0o700, parents=True, exist_ok=True)
    copied_sources = ("spec/nexaui-inputs.json", "src/settings/model.c", "src/settings/model.h")
    if ui == "nexaui-owned": copied_sources += session.OWN_SETTINGS_SOURCES
    for name in copied_sources:
        (root / "source" / name).write_bytes((source / name).read_bytes())
    pin_path = root / "source/spec/nexaui-inputs.json"
    original_pin = pin_path.read_bytes()
    env = {**os.environ, "AETHERI_TEST_SETTINGS_UI": ui, "AETHERI_TEST_SETTINGS_SOURCE": str(root / "source")}
    controller, issuer = socket.socketpair(socket.AF_UNIX, socket.SOCK_SEQPACKET)
    controller.settimeout(4)
    stream = (root / "owner.log").open("wb")
    owner = subprocess.Popen([sys.executable, str(source / "tests/development_session_fixture.py"), str(build), str(root),
                              str(issuer.fileno())], pass_fds=(issuer.fileno(),), cwd=root,
                             env=env, stdin=subprocess.DEVNULL, stdout=stream, stderr=subprocess.STDOUT)
    issuer.close()
    owner_pin = os.pidfd_open(owner.pid)
    pins, roles, images, cases = [owner_pin], {}, [], []
    seen_spawns = 0

    def rawlog():
        return (root / "owner.log").read_text(errors="replace")

    def log(role):
        return role_output(rawlog(),role)

    def live(fd):
        return not select.select([fd], [], [], 0)[0]

    def enroll():
        nonlocal seen_spawns
        spawns = re.findall(r"^SPAWN (\w+) (\d+)$", rawlog(), re.M)
        fresh = spawns[seen_spawns:]; seen_spawns = len(spawns)
        for role, value in fresh:
            pid = int(value)
            fd = None
            try:
                fd = os.pidfd_open(pid)
                assert int(Path(f"/proc/{pid}/stat").read_text().rsplit(")", 1)[1].split()[1]) == owner.pid
            except (ProcessLookupError, FileNotFoundError):
                if fd is not None: os.close(fd)
                continue
            except BaseException:
                if fd is not None: os.close(fd)
                raise
            pins.append(fd)
            roles[role] = (pid, fd)

    def wait(predicate, label):
        deadline = time.monotonic() + 12
        while not predicate():
            enroll()
            assert owner.poll() is None and time.monotonic() < deadline, (label, str(root), rawlog()[-6000:])
            time.sleep(0.01)
        enroll()

    def event(command):
        controller.sendall(command.encode())
        assert controller.recv(32) == b"OK", command

    def key(code, down):
        event(f"K {code} {int(down)}")

    def tap(code):
        key(code, True)
        key(code, False)

    codes = dict(zip("abcdefghijklmnopqrstuvwxyz", (30,48,46,32,18,33,34,35,23,36,37,38,50,49,24,25,16,19,31,20,22,47,17,45,21,44)))

    def launch_search():
        before = log("desktop").count("STABLE 0 1920x1080\n")
        event("M 100 100")
        key(125, True)
        tap(57)
        key(125, False)
        wait(lambda: log("desktop").count("STABLE 0 1920x1080\n") > before, "global search ready")
        for char in "settings":
            tap(codes[char])
        wait(lambda: "query=settings " in log("desktop"), "native Settings query")
        tap(28)

    def click_control(kind):
        point=[]
        def observed():
            p=control_position(log('desktop'),kind)
            if p is None:return False
            point[:]=p;return True
        wait(observed,'complete presented desktop control')
        x,y=point;event(f"M {x} {y}")
        event("B 272 1")
        event("B 272 0")

    def current_settings():
        chunk = rawlog().rsplit("SPAWN settings ", 1)[-1]
        return "\n".join(s for s in chunk.splitlines() if s.startswith("settings: "))

    def model():
        rows = re.findall(r"MODEL theme=(\d+) german=(\d+) text=(\d+)", current_settings())
        return tuple(map(int, rows[-1])) if rows else None

    def tab_to(target):
        def focus():
            rows = re.findall(r"FOCUS (\d+)", current_settings())
            return int(rows[-1]) if rows else None
        for _ in range(16):
            if focus() == target: return
            previous = focus(); tap(15)
            wait(lambda: focus() != previous, "native owned Settings Tab")
        raise AssertionError((target, focus(), current_settings()[-3000:]))

    def snapshot(label):
        sequence = len(cases) + 1
        signal.pidfd_send_signal(roles["compositor"][1], signal.SIGUSR1)
        wait(lambda: all(f"CAPTURE {sequence} {i}\n" in log("compositor") for i in range(3)), label)
        for i in range(3):
            path = root / "captures" / f"capture-{sequence}-output-{i}.ppm"
            raw = path.read_bytes()
            magic, dimensions, peak, pixels = raw.split(b"\n", 3)
            width, height = map(int, dimensions.split())
            assert magic == b"P6" and peak == b"255" and len(pixels) == width*height*3
            assert len(set(pixels)) > 16
            packed = path.with_suffix(".ppm.gz")
            packed.write_bytes(gzip.compress(raw, mtime=0))
            path.unlink()
            images.append({"case":label, "output":i, "file":packed.name, "raw_sha256":hashlib.sha256(raw).hexdigest(), "file_sha256":digest(packed)})
        cases.append(label)

    try:
        wait(lambda: "FIXTURE_NOTIFY READY=1" in rawlog() and "STABLE 2 3840x84\n" in log("desktop"), "original owner/desktop ready")
        launch_search()
        wait(lambda: "FRAME " in log("settings") and "LAUNCH_REPLY A S\n" in log("desktop"), "search starts selected native view")
        first = roles["settings"]
        maps = Path(f"/proc/{first[0]}/maps").read_text()
        assert str(Path(selected["build"]) / "prefix/lib/libnexaui.so.") in maps and "libSDL3" not in maps
        expected_image = selected["presentation"]["sha256"] if ui == "nexaui-owned" else selected["artifacts"]["consumer-build/aetheri_model_view"]
        assert digest(Path(f"/proc/{first[0]}/exe")) == expected_image
        (root / "first-maps.txt").write_text(maps)
        snapshot("search-selected NexaUI Settings with original desktop")
        before = log("desktop").count("LAUNCH_REPLY A S\n")
        click_control(2)
        wait(lambda: log("desktop").count("LAUNCH_REPLY A S\n") > before, "dock reuses original live Settings")
        assert len(re.findall(r"^SPAWN settings ", rawlog(), re.M)) == 1 and live(first[1])
        if ui == "nexaui-owned":
            tab_to(51); tap(57)
            wait(lambda: "SETTLED theme=0 german=0 text=100 page=1" in current_settings(), "own Appearance page")
            tab_to(101)
        else:
            tap(15); tap(15)
        tap(57)
        wait(lambda: model() == (1,0,100), "native keyboard changes original model")
        tap(1)
        wait(lambda: "LAUNCH_REPLY X S\n" in log("desktop"), "Settings process and pipe retired")
        assert not live(first[1])
        # Mutate only the private source-pin copy. Actual SDK and repository
        # inputs remain immutable; no new process or SDL fallback may result.
        changed = json.loads(original_pin)
        changed["adoption"] += " controlled test drift"
        pin_path.write_text(json.dumps(changed) + "\n")
        click_control(2)
        wait(lambda: "SETTINGS_PROFILE_REJECTED " in rawlog() and "LAUNCH_REPLY E S\n" in log("desktop"), "profile drift rejects a fresh launch")
        assert len(re.findall(r"^SPAWN settings ", rawlog(), re.M)) == 1
        assert all(live(roles[r][1]) for r in ("desktop", "compositor"))
        pin_path.write_bytes(original_pin)
        if ui == "nexaui-owned":
            view_source = root / "source/src/settings/nexaui_view.c"
            original_view = view_source.read_bytes()
            view_source.write_bytes(original_view + b"\n/* private test drift */\n")
            errors = log("desktop").count("LAUNCH_REPLY E S\n")
            click_control(2)
            wait(lambda: log("desktop").count("LAUNCH_REPLY E S\n") > errors, "owned source drift refuses fresh Settings")
            assert len(re.findall(r"^SPAWN settings ", rawlog(), re.M)) == 1
            assert all(live(roles[r][1]) for r in ("desktop", "compositor"))
            view_source.write_bytes(original_view)
            model_source = root / "source/src/settings/model.c"
            original_model = model_source.read_bytes()
            model_source.write_bytes(original_model + b"\n/* independent app model fixture */\n")
            # A fresh own-app selection can bind its model without rebuilding
            # the SDK/unused consumer. This already-running owner cannot adopt it.
            original_project = session.PROJECT
            try:
                session.PROJECT = root / "source"
                fresh_selection = session.settings_selection(profile, ui)
            finally:
                session.PROJECT = original_project
            assert fresh_selection["model"] != selected["model"]
            assert fresh_selection["artifacts"] == selected["artifacts"]
            assert fresh_selection["presentation"]["sha256"] == selected["presentation"]["sha256"]
            errors = log("desktop").count("LAUNCH_REPLY E S\n")
            click_control(2)
            wait(lambda: log("desktop").count("LAUNCH_REPLY E S\n") > errors, "retained owner refuses a different app model")
            assert len(re.findall(r"^SPAWN settings ", rawlog(), re.M)) == 1
            model_source.write_bytes(original_model)
        # A fresh explicit request can now select only the original same bytes.
        click_control(2)
        wait(lambda: len(re.findall(r"^SPAWN settings ", rawlog(), re.M)) == 2 and
                     "settings: FRAME " in current_settings() and model() == (0,0,100), "fresh original Settings after exact restoration")
        second = roles["settings"]
        assert second[0] != first[0] and live(second[1])
        click_control(4)
        wait(lambda: "READY " in log("terminal"), "independent original terminal coexists")
        click_control(5)
        wait(lambda: "READY files " in log("files"), "independent original Files coexists")
        assert live(second[1])
        snapshot("fresh NexaUI Settings coexists with original Terminal and Files")
        owner.terminate()
        assert owner.wait(timeout=12) == 0, rawlog()
        assert all(not live(fd) for fd in pins)
        end = json.loads(re.search(r"^SESSION_END (.+)$", rawlog(), re.M)[1])
        assert all(value == 0 for value in end["children"].values()), end
        assert end["app_history"] == [{"role":"settings", "pid":first[0], "status":0}]
        assert rawlog().rfind("files: CLOSED files ") < rawlog().rfind("terminal: CLOSED ") < rawlog().rfind("settings: SHUTDOWN signal") < rawlog().rfind("desktop: CLOSED ")
        assert not re.search(r"AddressSanitizer|LeakSanitizer|runtime error:", rawlog())
        assert pin_path.read_bytes() == original_pin
        for name, expected in selected["artifacts"].items():
            assert digest(Path(selected["build"]) / name) == expected
        for name in copied_sources:
            assert (root / "source" / name).read_bytes() == (source / name).read_bytes()
        result = {"result":"passed", "profile":profile, "settings_ui":ui, "selection":selected, "binding":session.settings_fingerprint(selected),
                  "cases":cases, "images":images, "end":end,
                  "scope":"real owner/native desktop with explicit NexaUI selection; no PAM/DRM, production admission or complete removal"}
        (root / "result.json").write_text(json.dumps(result, indent=2) + "\n")
        print(json.dumps({**result, "artifacts":str(root)}))
    finally:
        enroll()
        if owner.poll() is None:
            owner.terminate()
        for fd in reversed(pins):
            try:
                signal.pidfd_send_signal(fd, signal.SIGKILL)
            except ProcessLookupError:
                pass
            finally:
                os.close(fd)
        if owner.poll() is None:
            owner.wait(timeout=5)
        stream.close()
        controller.close()
        print("NexaUI owner evidence:", root)


if __name__ == "__main__":
    main()
