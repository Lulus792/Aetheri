"""Explicit dedicated-carrier UI session. Development infrastructure, not app admission.

Windows: start/status/stop via the verified P0 SSH connection.
Linux --guest: root-only controller; --supervise: ordinary-user child supervisor.
No installed service, public API, catalogue writer or automatic retry.
"""
import argparse
from contextlib import contextmanager
import hashlib
import json
import os
from pathlib import Path
import re
import selectors
import signal
import socket
import stat
import subprocess
import sys
import time
import uuid

CONTROL = Path("/var/lib/aetheri-development-session")
USER_ROOT = Path("/home/aetheri/development-sessions")
SOURCE = Path("/home/aetheri/source/tools/development_session.py")
PROJECT = Path(__file__).resolve().parents[1]
UI = Path("/home/aetheri/deps/ui-gles-release-02/prefix")
TERMINAL_FONT = Path("/usr/share/fonts/liberation-mono-fonts/LiberationMono-Regular.ttf")
APP_SELECTORS = {b"S": "settings", b"T": "terminal", b"F": "files"}
APP_ROLES = ("files", "terminal", "settings")  # Files retains its document owner before later views retire.
PROFILES = ("release", "debug")
SETTINGS_UIS = ("sdl", "nexaui")
PHASES = ("preparing", "starting", "running", "stopping", "exited", "closed")


def build_for(profile):
    return Path("/home/aetheri/build/session-" + profile)


def run(*args, timeout=25):
    return subprocess.check_output(args, text=True, stderr=subprocess.PIPE, timeout=timeout).strip()


def digest(path):
    with Path(path).open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def terminal_assets():
    font = json.loads((SOURCE.parents[1] / "spec/ui-inputs.json").read_text())["terminal_font"]
    license_path = Path("/usr/share/licenses/liberation-mono-fonts/LICENSE")
    if font["path"] != str(TERMINAL_FONT) or font["license_path"] != str(license_path):
        raise RuntimeError("unexpected fixed terminal asset paths")
    values = {str(TERMINAL_FONT): digest(TERMINAL_FONT), str(license_path): digest(license_path)}
    if values[str(TERMINAL_FONT)] != font["sha256"] or values[str(license_path)] != font["license_sha256"]:
        raise RuntimeError("reviewed terminal assets changed")
    return values


def settings_fingerprint(profile):
    return hashlib.sha256(json.dumps(profile, sort_keys=True, separators=(",", ":")).encode()).hexdigest()


def validate_settings_profile(profile, build_profile):
    if not isinstance(profile, dict) or set(profile) != {"revision", "version", "build", "artifacts", "model", "metadata_sha256", "pin_sha256"}:
        raise RuntimeError("invalid development Settings binding")
    if not isinstance(profile["build"], str) or re.fullmatch("/home/aetheri/deps/nexaui-settings-" + build_profile + "-[0-9]+", profile["build"]) is None:
        raise RuntimeError("invalid development Settings build")
    if not isinstance(profile["revision"], str) or re.fullmatch("[0-9a-f]{40}", profile["revision"]) is None:
        raise RuntimeError("invalid development Settings revision")
    if not isinstance(profile["version"], str) or re.fullmatch("[0-9]+\\.[0-9]+\\.[0-9]+", profile["version"]) is None:
        raise RuntimeError("invalid development Settings version")
    artifacts = profile["artifacts"]
    expected = {"consumer-build/aetheri_model_view", "prefix/lib/libnexaui.so." + profile["version"]}
    if not isinstance(artifacts, dict) or set(artifacts) != expected:
        raise RuntimeError("invalid development Settings artifacts")
    model = profile["model"]
    if not isinstance(model, dict) or set(model) != {"src/settings/model.c", "src/settings/model.h"}:
        raise RuntimeError("invalid development Settings model")
    for value in (*artifacts.values(), *model.values(), profile["metadata_sha256"], profile["pin_sha256"]):
        if not isinstance(value, str) or re.fullmatch("[0-9a-f]{64}", value) is None:
            raise RuntimeError("invalid development Settings digest")
    return profile


def settings_selection(build_profile, ui):
    """Read-only developer consistency check, never production code admission.

    The ordinary build account controls these mutable inputs. Hashing them is
    not a sealed store, publisher trust, atomic exec pin or volume authentication.
    """
    if ui == "sdl": return None
    if ui != "nexaui" or build_profile not in PROFILES:
        raise RuntimeError("invalid Settings UI/profile")
    try:
        pin_path = PROJECT / "spec/nexaui-inputs.json"
        with pin_path.open("rb") as stream: pin_bytes = stream.read(65537)
        if len(pin_bytes) > 65536: raise RuntimeError("Settings pin exceeded bound")
        pin = json.loads(pin_bytes)
        build = Path(pin["development_builds"][build_profile])
        if build.resolve() != build: raise RuntimeError("aliased Settings build")
        owned(build, 1000, 0o700, directory=True)
        metadata = build / "build.json"; owned(metadata, 1000, 0o644)
        with metadata.open("rb") as stream: metadata_bytes = stream.read(65537)
        if len(metadata_bytes) > 65536: raise RuntimeError("Settings metadata exceeded bound")
        record = json.loads(metadata_bytes)
        if record["revision"] != pin["revision"] or record["version"] != pin["version"] or record["atspi_compiled"] is not True or record["sanitizers"] is not (build_profile == "debug"):
            raise RuntimeError("Settings candidate does not match selected build")
        names = ("consumer-build/aetheri_model_view", "prefix/lib/libnexaui.so." + pin["version"])
        selected = dict(revision=pin["revision"], version=pin["version"], build=str(build),
                        artifacts={n: record["artifacts"][n] for n in names}, model=record["model"],
                        metadata_sha256=hashlib.sha256(metadata_bytes).hexdigest(), pin_sha256=hashlib.sha256(pin_bytes).hexdigest())
        validate_settings_profile(selected, build_profile)
        for name, expected in selected["model"].items():
            if digest(PROJECT / name) != expected: raise RuntimeError("Settings model changed")
        for name, expected in selected["artifacts"].items():
            path = build / name; current = path.lstat()
            if path.resolve() != path or not stat.S_ISREG(current.st_mode) or current.st_uid != 1000 or stat.S_IMODE(current.st_mode) != 0o755 or current.st_nlink != 1 or current.st_size > 16*1024**2 or digest(path) != expected:
                raise RuntimeError("Settings artifact changed")
        return selected
    except (OSError, KeyError, TypeError, ValueError) as error:
        raise RuntimeError("Settings candidate check failed") from error


def application_command(role, build, cache, settings_profile=None):
    if role == "settings":
        if settings_profile is not None:
            return [str(Path(settings_profile["build"]) / "consumer-build/aetheri_model_view"),
                    "--font", str(UI / "share/aetheri-fonts/LiberationSans-Regular.ttf"), "--cache", str(cache), "--diagnostics"]
        return [str(build / "aetheri-settings"), "--font", str(UI / "share/aetheri-fonts/LiberationSans-Regular.ttf"),
                "--cache", str(cache), "--diagnostics"]
    if role == "terminal":
        return [str(build / "aetheri-terminal"), "--shell", str(build / "aetheri-shell"),
                "--font", str(TERMINAL_FONT), "--cache", str(cache), "--diagnostics"]
    if role == "files":
        return [str(build / "aetheri-files"), "--root", str(cache.parent / "documents"),
                "--font", str(UI / "share/aetheri-fonts/LiberationSans-Regular.ttf"),
                "--terminal-font", str(TERMINAL_FONT), "--cache", str(cache),
                "--terminal", str(build / "aetheri-terminal"), "--editor", str(build / "aetheri-editor"), "--diagnostics"]
    raise ValueError("unknown fixed development application")


def compositor_launch(build, shell_server, compose=False):
    args = [str(build / "aetheri-compositor"), "--drm", "aetheri-development", "-", "/dev/dri/card0"]
    if shell_server is not None:
        args += ["--shell-fd", str(shell_server.fileno())]
    if compose: args.append("--compose")
    return args, (shell_server.fileno(),) if shell_server is not None else ()


def emit_child_line(role, line):
    if not line.startswith((b"VIEW ", b"CONTROL ", b"LAYOUT ", b"DRAWN ", b"INPUT ", b"FOCUS ")):
        print(role + ": " + line.decode(errors="replace"), flush=True)


def owned(path, uid, mode, directory=False):
    s = Path(path).lstat()
    kind = stat.S_ISDIR if directory else stat.S_ISREG
    if not kind(s.st_mode) or s.st_uid != uid or stat.S_IMODE(s.st_mode) != mode:
        raise RuntimeError("unexpected development object owner/type/mode: " + str(path))
    if not directory and (s.st_nlink != 1 or s.st_size > 65536):
        raise RuntimeError("unexpected development record bounds")


def save(record):
    # A preexisting .next is an unresolved write, never disposable retry debris.
    temporary = CONTROL / "active.next"
    data = (json.dumps(record, indent=2) + "\n").encode()
    if len(data) > 65536:
        raise RuntimeError("development record exceeded its bound")
    fd = os.open(temporary, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW, 0o600)
    with os.fdopen(fd, "wb") as stream:
        stream.write(data); stream.flush(); os.fsync(stream.fileno())
    os.replace(temporary, CONTROL / "active.json")
    fd = os.open(CONTROL, os.O_RDONLY | os.O_DIRECTORY)
    try: os.fsync(fd)
    finally: os.close(fd)


def validate(record, carrier):
    if record.get("format") != 1 or record.get("carrier") != carrier or record.get("profile") not in PROFILES:
        raise RuntimeError("foreign development record/profile")
    token = record.get("token", "")
    if not isinstance(token, str) or re.fullmatch("[0-9a-f]{32}", token) is None:
        raise RuntimeError("invalid development token")
    if record.get("unit") != "aetheri-development-" + token + ".service" or record.get("phase") not in PHASES:
        raise RuntimeError("invalid development unit/state")
    if str(uuid.UUID(record["boot"])) != record["boot"]:
        raise RuntimeError("invalid development boot")
    if record.get("invocation") is not None and re.fullmatch("[0-9a-f]{32}", record["invocation"]) is None:
        raise RuntimeError("invalid original development invocation")
    if not isinstance(record.get("desktop", False), bool):
        raise RuntimeError("invalid development desktop profile")
    if type(record.get("compose", False)) is not bool:
        raise RuntimeError("invalid development Compose profile")
    ui = record.get("settings_ui", "sdl")
    selected = record.get("settings_profile")
    binding = record.get("settings_binding")
    if ui not in SETTINGS_UIS or (ui == "sdl" and (selected is not None or binding is not None)):
        raise RuntimeError("invalid development Settings selection")
    if ui == "nexaui":
        if not record.get("desktop", False): raise RuntimeError("NexaUI selection requires desktop")
        validate_settings_profile(selected, record["profile"])
        if binding != settings_fingerprint(selected): raise RuntimeError("development Settings binding changed")
    protocol = record.get("launch_protocol", 1)
    if type(protocol) is not int or protocol not in (1, 2, 3) or (not record.get("desktop", False) and protocol != 1):
        raise RuntimeError("invalid development launch protocol")
    return record


def load(carrier, token=None):
    active = CONTROL / "active.json"
    if not active.exists():
        return None
    owned(active, 0, 0o600)
    record = validate(json.loads(active.read_text()), carrier)
    if token is not None and token != record["token"]:
        path = CONTROL / "history" / (token + ".json")
        owned(path, 0, 0o600)
        record = validate(json.loads(path.read_text()), carrier)
        if record["token"] != token or record["phase"] != "closed":
            raise RuntimeError("nonterminal development history")
    return record


def properties(unit):
    names = ("LoadState", "ActiveState", "SubState", "MainPID", "ExecMainPID", "ExecMainStatus",
             "ExecMainCode", "InvocationID", "ControlGroup", "Result", "Type", "RemainAfterExit", "User")
    return dict(line.split("=", 1) for line in run("systemctl", "show", unit,
                *["--property=" + name for name in names]).splitlines())


def require_original(record, props):
    if Path("/proc/sys/kernel/random/boot_id").read_text().strip() != record["boot"]:
        raise RuntimeError("different boot: development session needs manual reconciliation")
    if props["InvocationID"] != record["invocation"] or props["ExecMainPID"] != str(record["pid"]):
        raise RuntimeError("original development unit was lost or replaced")
    if props["MainPID"] not in ("0", str(record["pid"])):
        raise RuntimeError("development main process changed")
    if props["RemainAfterExit"] != "yes" or props["Type"] != "notify" or props["User"] != "aetheri":
        raise RuntimeError("development unit configuration drift")


def free_seat():
    for row in json.loads(run("loginctl", "list-sessions", "--json=short")):
        if row.get("seat") == "seat0":
            raise RuntimeError("an existing local session owns seat0")
    text = run("systemd-analyze", "cat-config", "systemd/logind.conf")
    values = {"NAutoVTs": 6, "ReserveVT": 6}
    for name, value in re.findall(r"^\s*(NAutoVTs|ReserveVT)\s*=\s*(\d+)\s*$", text, re.M):
        values[name] = int(value)
    if values["NAutoVTs"] >= 8 or values["ReserveVT"] == 8:
        raise RuntimeError("tty8 is in this carrier's automatic VT range")
    if Path("/sys/class/tty/tty0/active").read_text().strip() != "tty1":
        raise RuntimeError("start requires the inspected tty1 baseline")
    if subprocess.run(["systemctl", "is-active", "--quiet", "getty@tty8.service"]).returncode == 0:
        raise RuntimeError("tty8 has a login service")
    tty = os.stat("/dev/tty8").st_rdev
    for p in Path("/proc").iterdir():
        if not p.name.isdigit(): continue
        try: current = int((p / "stat").read_text().rsplit(")", 1)[1].split()[4])
        except (FileNotFoundError, ProcessLookupError): continue
        if current == tty:
            raise RuntimeError("tty8 already has a controlling process")


def start(record, carrier, profile, desktop=False, compose=False, settings_ui="sdl"):
    if record and record["phase"] != "closed":
        raise RuntimeError("an existing development intent must be observed/stopped first")
    if settings_ui not in SETTINGS_UIS or (settings_ui == "nexaui" and not desktop):
        raise RuntimeError("Settings UI requires its selected desktop profile")
    selected = settings_selection(profile, settings_ui)
    binding = settings_fingerprint(selected) if selected is not None else None
    free_seat()
    if record:
        history = CONTROL / "history" / (record["token"] + ".json")
        if history.exists():
            owned(history, 0, 0o600)
            if history.read_bytes() != (CONTROL / "active.json").read_bytes():
                raise RuntimeError("development history conflicts")
        else:
            fd = os.open(history, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW, 0o600)
            with os.fdopen(fd, "wb") as stream:
                stream.write((CONTROL / "active.json").read_bytes()); stream.flush(); os.fsync(stream.fileno())
            fd = os.open(history.parent, os.O_RDONLY | os.O_DIRECTORY)
            try: os.fsync(fd)
            finally: os.close(fd)
    token = uuid.uuid4().hex
    build = build_for(profile)
    names = ("aetheri-compositor", "aetheri-settings", "aetheri-desktop", "aetheri-terminal", "aetheri-shell", "aetheri-files", "aetheri-editor") if desktop else ("aetheri-compositor", "aetheri-settings")
    if selected is not None: names = tuple(n for n in names if n != "aetheri-settings")
    binaries = {str(build / name): digest(build / name) for name in names}
    if selected is not None: binaries.update({str(Path(selected["build"]) / n): h for n, h in selected["artifacts"].items()})
    assets = terminal_assets() if desktop else {}
    record = dict(format=1, token=token, carrier=carrier, profile=profile,
                  boot=Path("/proc/sys/kernel/random/boot_id").read_text().strip(),
                  unit="aetheri-development-" + token + ".service", phase="preparing",
                  invocation=None, pid=None, binaries=binaries, assets=assets, desktop=desktop,
                  launch_protocol=3 if desktop else 1, compose=compose, created_unix=time.time(), exit=None)
    record.update(settings_ui=settings_ui, settings_profile=selected, settings_binding=binding)
    save(record)
    if not USER_ROOT.exists(): USER_ROOT.mkdir(mode=0o700); os.chown(USER_ROOT, 1000, 1000)
    owned(USER_ROOT, 1000, 0o700, directory=True)
    base = USER_ROOT / token; base.mkdir(mode=0o700); os.chown(base, 1000, 1000)
    for name in (("runtime", "cache", "documents") if desktop else ("runtime", "cache")):
        path = base / name; path.mkdir(mode=0o700); os.chown(path, 1000, 1000)
    record["phase"] = "starting"; save(record)
    args = ["systemd-run", "--unit=" + record["unit"], "--service-type=notify",
            "--property=NotifyAccess=main", "--property=RemainAfterExit=yes", "--property=User=aetheri",
            "--property=Group=aetheri", "--property=PAMName=login", "--property=TTYPath=/dev/tty8",
            "--property=StandardInput=tty", "--property=StandardOutput=journal", "--property=StandardError=journal",
            "--property=TTYReset=yes", "--property=NoNewPrivileges=yes", "--property=CapabilityBoundingSet=",
            "--property=AmbientCapabilities=", "--property=Restart=no", "--property=KillMode=control-group",
            "--property=SendSIGKILL=no", "--property=TimeoutStartSec=15", "--property=TimeoutStopSec=10",
            "--property=Conflicts=getty@tty8.service", "--property=After=getty@tty8.service",
            "--setenv=XDG_SESSION_TYPE=wayland", "--setenv=XDG_SESSION_CLASS=user",
            "--setenv=XDG_SEAT=seat0", "--setenv=XDG_VTNR=8", "--setenv=LIBSEAT_BACKEND=logind",
            "/usr/bin/python3", str(SOURCE), "--supervise", "--profile", profile, "--token", token]
    if desktop: args.append("--desktop")
    if compose: args.append("--compose")
    if selected is not None: args += ["--settings-ui", settings_ui, "--settings-binding", binding]
    # Start the unit without waiting for READY, then activate its inspected VT.
    # No second start is permitted if either dispatch acknowledgement is lost.
    args.insert(1, "--no-block")
    run(*args); run("chvt", "8")
    deadline = time.monotonic() + 16
    while True:
        props = properties(record["unit"])
        if props["ActiveState"] == "active" and props["SubState"] == "running": break
        if props["ActiveState"] == "failed" or time.monotonic() >= deadline:
            raise RuntimeError("development startup did not become ready; preserve the pending intent")
        time.sleep(0.05)
    invocation = props["InvocationID"]
    if re.fullmatch("[0-9a-f]{32}", invocation) is None or int(props["MainPID"]) <= 1:
        raise RuntimeError("missing actual development invocation/process")
    record.update(invocation=invocation, pid=int(props["MainPID"]), phase="running")
    require_original(record, props)
    pin = os.pidfd_open(record["pid"])
    try:
        import select
        poller = select.poll(); poller.register(pin, select.POLLIN)
        if poller.poll(0): raise RuntimeError("development supervisor already exited")
        proc = Path("/proc") / str(record["pid"])
        status = dict(line.split(":", 1) for line in (proc / "status").read_text().splitlines())
        if any(set(status[key].split()) != {"1000"} for key in ("Uid", "Gid")) or status["NoNewPrivs"].strip() != "1":
            raise RuntimeError("development supervisor credentials differ")
        if any(int(status[key], 16) for key in ("CapInh", "CapPrm", "CapEff", "CapBnd", "CapAmb")):
            raise RuntimeError("development supervisor retained capabilities")
        record["observed_label"] = (proc / "attr/current").read_bytes().rstrip(b"\0").decode()
        record["observed_start_ticks"] = (proc / "stat").read_text().rsplit(")", 1)[1].split()[19]
        if poller.poll(0): raise RuntimeError("development supervisor exited during observation")
        require_original(record, properties(record["unit"]))
    finally: os.close(pin)
    save(record)
    return record


def stop(record):
    if record["phase"] == "closed": return record  # Historical; never act on a later session.
    if record["phase"] not in ("running",):
        raise RuntimeError("pending/uncertain development effect needs manual reconciliation")
    props = properties(record["unit"]); require_original(record, props)
    if props["MainPID"] != "0":
        pin = os.pidfd_open(int(props["MainPID"]))
        try:
            import select
            poller = select.poll(); poller.register(pin, select.POLLIN)
            record["phase"] = "stopping"; save(record)
            # RemainAfterExit retains the original status; stop only its main actor.
            run("systemctl", "kill", "--kill-whom=main", "--signal=SIGTERM", record["unit"])
            deadline = time.monotonic() + 12
            while True:
                props = properties(record["unit"]); require_original(record, props)
                if props["MainPID"] == "0": break
                if time.monotonic() >= deadline: raise RuntimeError("development supervisor still alive")
                time.sleep(0.05)
            if not poller.poll(0): raise RuntimeError("original development process pin is not terminal")
        finally: os.close(pin)
    else:
        record["phase"] = "stopping"; save(record)
    if props["ControlGroup"]:
        events = Path("/sys/fs/cgroup" + props["ControlGroup"]) / "cgroup.events"
        if "populated 1" in events.read_text(): raise RuntimeError("development service still populated")
    sessions = json.loads(run("loginctl", "list-sessions", "--json=short"))
    if any(row.get("seat") == "seat0" for row in sessions):
        raise RuntimeError("development PAM session still present")
    record.update(phase="exited", exit=dict(code=int(props["ExecMainCode"]), status=int(props["ExecMainStatus"]), result=props["Result"]))
    save(record)
    # Capture status before explicit unit retirement can trigger garbage collection.
    run("systemctl", "stop", record["unit"])
    run("chvt", "1")
    record["phase"] = "closed"; save(record)
    return record


@contextmanager
def locked(create=False):
    import fcntl
    if create and not CONTROL.exists(): CONTROL.mkdir(mode=0o700)
    owned(CONTROL, 0, 0o700, directory=True)
    if any(p.name not in ("history", "writer.lock", "active.json", "active.next") for p in CONTROL.iterdir()):
        raise RuntimeError("unknown development control entry")
    history = CONTROL / "history"
    if create and not history.exists(): history.mkdir(mode=0o700)
    owned(history, 0, 0o700, directory=True)
    fd = os.open(CONTROL / "writer.lock", os.O_RDWR | (os.O_CREAT if create else 0) | os.O_NOFOLLOW, 0o600)
    try:
        owned(CONTROL / "writer.lock", 0, 0o600)
        fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
        if (CONTROL / "active.next").exists(): raise RuntimeError("unacknowledged development record write")
        yield
    finally: os.close(fd)


def notify(message):
    name = os.environ.get("NOTIFY_SOCKET")
    if not name: raise RuntimeError("development supervisor requires its notify unit")
    with socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM) as sock:
        sock.connect("\0" + name[1:] if name.startswith("@") else name)
        sock.sendall(message.encode())


def observation(record):
    if record is None: return dict(kind="absent")
    if record["phase"] == "closed": return dict(kind="historical_closed")
    props = properties(record["unit"])
    if record["invocation"] is None:
        return dict(kind="pending_unbound", candidate_unit=props)
    try: require_original(record, props)
    except RuntimeError as error: return dict(kind="recovery_required", reason=str(error), candidate_unit=props)
    kind = "original_exit" if props["MainPID"] == "0" else "live"
    return dict(kind=kind, original_unit=props)


def supervise(profile, token, desktop=False, compose=False, settings_ui="sdl", settings_binding=None):
    if os.getuid() != 1000 or os.getuid() != os.geteuid():
        raise RuntimeError("development supervisor requires the ordinary carrier account")
    if settings_ui not in SETTINGS_UIS or (settings_ui == "nexaui" and not desktop):
        raise RuntimeError("Settings UI requires its selected desktop profile")
    selected = settings_selection(profile, settings_ui)
    expected_binding = settings_fingerprint(selected) if selected is not None else None
    if settings_binding != expected_binding:
        raise RuntimeError("original Settings selection changed before supervision")
    base = USER_ROOT / token
    for path in (base, base / "runtime", base / "cache"): owned(path, 1000, 0o700, directory=True)
    if desktop: owned(base / "documents", 1000, 0o700, directory=True)
    build = build_for(profile)
    env = {**os.environ, "XDG_RUNTIME_DIR": str(base / "runtime"), "LIBSEAT_BACKEND": "logind",
           "WAYLAND_DISPLAY": "aetheri-development", "LIBGL_ALWAYS_SOFTWARE": "1",
           "GALLIUM_DRIVER": "llvmpipe", "LP_NUM_THREADS": "2"}
    for key in ("WAYLAND_SOCKET", "DISPLAY", "SDL_VIDEO_DRIVER", "SDL_RENDER_DRIVER"): env.pop(key, None)
    selector = selectors.DefaultSelector()
    reader, writer = socket.socketpair(); reader.setblocking(False); writer.setblocking(False)
    old_wakeup = signal.set_wakeup_fd(writer.fileno())
    stopping = False; failed = False; children = {}; buffers = {}; ready = set(); terminated = set(); notified = False; fence_logged = False
    def signalled(number, frame):
        nonlocal stopping
        if number in (signal.SIGTERM, signal.SIGINT): stopping = True
    for number in (signal.SIGTERM, signal.SIGINT, signal.SIGCHLD): signal.signal(number, signalled)
    selector.register(reader, selectors.EVENT_READ, "signal")
    launch_owner = launch_client = shell_server = shell_client = None
    launches = {role: 0 for role in APP_ROLES}; app_history = []
    if desktop:
        shell_server, shell_client = socket.socketpair()
        launch_owner, launch_client = socket.socketpair(socket.AF_UNIX, socket.SOCK_SEQPACKET)
        launch_owner.setblocking(False)
        selector.register(launch_owner, selectors.EVENT_READ, "launch")
    def spawn(role, args, pass_fds=(), child_env=None):
        if role in children: raise RuntimeError("previous development child is not fully drained")
        child = subprocess.Popen(args, env=child_env or env, pass_fds=pass_fds, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        children[role] = child; buffers[role] = b""
        print("SPAWN " + role + " " + str(child.pid), flush=True)
        try:
            os.set_blocking(child.stdout.fileno(), False)
            selector.register(child.stdout, selectors.EVENT_READ, role)
        except BaseException:
            child.stdout.close()  # Keep/reap the original Popen even if pipe setup failed.
            raise
    def reply(value):
        if launch_owner is None or launch_owner.send(value) != len(value):
            raise RuntimeError("uncertain development launch reply")
    def recycle_app(role):
        child = children.get(role)
        if not child or child.poll() is None or not child.stdout.closed: return False
        if child.returncode != 0: raise RuntimeError(role + " failed during desktop use")
        app_history.append(dict(role=role, pid=child.pid, status=child.returncode))
        del children[role]; del buffers[role]; ready.discard(role); terminated.discard(role)
        if not stopping and launch_owner is not None: reply(b"X3" + next(s for s,r in APP_SELECTORS.items() if r == role))
        return True
    def retirement_step():
        for role in (*APP_ROLES, "desktop", "compositor"):
            child = children.get(role)
            if child is None: continue
            if child.poll() is None:
                if role not in terminated: child.terminate(); terminated.add(role)
                return False
            if not child.stdout.closed: return False
        return True
    deadline = time.monotonic() + 12
    try:
        compositor_args, compositor_fds = compositor_launch(build, shell_server, compose)
        spawn("compositor", compositor_args, compositor_fds)
        if shell_server: shell_server.close()
        while True:
            if not stopping and not notified and time.monotonic() >= deadline: failed = True; stopping = True
            if stopping:
                if not fence_logged:
                    print("STOPPING", flush=True); fence_logged = True
                # These parent copies may still be unused if the compositor
                # failed before desktop creation. They must not keep the owner
                # channel alive while waiting for its EOF. Issued child copies
                # remain independently owned by the original desktop process.
                for unused in (launch_client, shell_client):
                    if unused is not None: unused.close()
                launch_client = shell_client = None
                drained = retirement_step()
                if all(child.poll() is not None for child in children.values()):
                    # A requested stop may produce 0 or SIGTERM, but never hide
                    # a sanitizer/cleanup failure. Drain each original pipe too.
                    if any(child.returncode not in (0, -signal.SIGTERM) for child in children.values()): failed = True
                    if drained and all(key.data == "signal" for key in selector.get_map().values()): break
            timeout = max(0, deadline - time.monotonic()) if not stopping and not notified else None
            for key, _ in selector.select(timeout):
                role = key.data
                if role == "signal":
                    try: reader.recv(4096)
                    except BlockingIOError: pass
                    continue
                if role == "launch":
                    import array
                    value, ancillary, flags, _ = launch_owner.recvmsg(8, socket.CMSG_SPACE(4*4), socket.MSG_CMSG_CLOEXEC)
                    for level, kind, payload in ancillary:
                        if level == socket.SOL_SOCKET and kind == socket.SCM_RIGHTS:
                            fds = array.array("i"); fds.frombytes(payload[:len(payload)-len(payload)%fds.itemsize])
                            for fd in fds: os.close(fd)
                    if not value and not ancillary:
                        selector.unregister(launch_owner); launch_owner.close(); launch_owner = None
                        if not stopping: failed = True; stopping = True
                        continue
                    if value not in (b"L3S", b"L3T", b"L3F", b"Q30") or ancillary or flags & (socket.MSG_TRUNC | socket.MSG_CTRUNC):
                        raise RuntimeError("invalid development launch packet")
                    if value == b"Q30": stopping = True; continue
                    if stopping: continue  # No new effects after the shutdown fence.
                    role = APP_SELECTORS[value[2:]]; suffix = b"3" + value[2:]
                    recycle_app(role)
                    child = children.get(role)
                    if child and child.poll() is None: reply(b"A" + suffix); continue
                    if child or launches[role] >= 128: reply(b"E" + suffix); continue
                    launches[role] += 1
                    if role == "settings" and selected is not None:
                        try:
                            if settings_selection(profile, settings_ui) != selected:
                                raise RuntimeError("original Settings selection changed before spawn")
                        except RuntimeError as error:
                            print("SETTINGS_PROFILE_REJECTED " + str(error), flush=True)
                            reply(b"E" + suffix); continue
                    try:
                        spawn(role, application_command(role, build, base / "cache", selected))
                    except OSError:
                        # Popen reports an exec failure after reaping its failed child.
                        if role in children: raise  # Later pipe setup failed after a real spawn.
                        reply(b"E" + suffix); continue
                    reply(b"A" + suffix); continue
                data = os.read(key.fileobj.fileno(), 8192)
                if not data:
                    if buffers[role]:
                        print(role + ": " + buffers[role].decode(errors="replace"), flush=True)
                        buffers[role] = b""
                    selector.unregister(key.fileobj); key.fileobj.close(); continue
                buffers[role] += data
                if len(buffers[role]) > 65536: raise RuntimeError("development child output exceeded bound")
                while b"\n" in buffers[role]:
                    line, buffers[role] = buffers[role].split(b"\n", 1)
                    emit_child_line(role, line)
                    if line.startswith(b"READY ") and role not in ready:
                        ready.add(role)
                        if role == "compositor" and not stopping:
                            if desktop:
                                spawn("desktop", [str(build / "aetheri-desktop"), "--font", str(UI / "share/aetheri-fonts/LiberationSans-Regular.ttf"),
                                                  "--cache", str(base / "cache"), "--launcher-fd", str(launch_client.fileno()), "--diagnostics"],
                                      (shell_client.fileno(), launch_client.fileno()), {**env, "WAYLAND_SOCKET": str(shell_client.fileno())})
                                shell_client.close(); launch_client.close()
                            else:
                                spawn("settings", application_command("settings", build, base / "cache"))
                    first = "desktop" if desktop else "settings"
                    if role == first and line.startswith(b"LAYOUT ") and {"compositor", first} <= ready and not notified:
                        notify("READY=1\nSTATUS=" + first.capitalize() + " submitted its first development frame")
                        notified = True
            if desktop and not stopping:
                for role in APP_ROLES: recycle_app(role)
            for role, child in list(children.items()):
                code = child.poll()
                if code is not None and role not in terminated:
                    if desktop and role in APP_ROLES and code == 0 and notified: continue
                    if role != ("desktop" if desktop else "settings") or code != 0 or not notified:
                        if not (desktop and role == "compositor" and code == 0 and notified): failed = True
                    stopping = True
        print("SESSION_END " + json.dumps({"children": {role: child.returncode for role, child in children.items()}, "app_history": app_history}), flush=True)
        return 1 if failed else 0
    except BaseException as error:
        print("SUPERVISOR_ERROR " + str(error), flush=True)
        if launch_owner is not None:
            selector.unregister(launch_owner); launch_owner.close(); launch_owner = None
        # Retain original child handles on a stuck shutdown, rather than leaving
        # them behind or escalating to an unreviewed force-kill policy.
        while not retirement_step():
            for key, _ in selector.select():
                if key.data == "signal":
                    try: reader.recv(4096)
                    except BlockingIOError: pass
                else:
                    data = os.read(key.fileobj.fileno(), 8192)
                    if not data: selector.unregister(key.fileobj); key.fileobj.close()
        return 1
    finally:
        signal.set_wakeup_fd(old_wakeup)
        selector.close(); reader.close(); writer.close()
        for channel in (launch_owner, launch_client, shell_server, shell_client):
            if channel is not None: channel.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("start", "status", "stop"), nargs="?", default="status")
    parser.add_argument("--profile", choices=PROFILES, default="release")
    parser.add_argument("--token")
    parser.add_argument("--desktop", action="store_true", help="explicit native dock/search development session")
    parser.add_argument("--compose", action="store_true", help="opt-in built-in Compose provider (requires its reviewed build)")
    parser.add_argument("--settings-ui", choices=SETTINGS_UIS, default="sdl", help="explicit development desktop Settings view")
    parser.add_argument("--settings-binding", help=argparse.SUPPRESS)
    parser.add_argument("--guest", action="store_true", help=argparse.SUPPRESS)
    parser.add_argument("--supervise", action="store_true", help=argparse.SUPPRESS)
    parser.add_argument("--carrier-uuid", help=argparse.SUPPRESS)
    args = parser.parse_args()
    if args.compose and args.action != "start" and not args.supervise:
        raise ValueError("--compose selects a new start profile only")
    if args.settings_ui != "sdl" and (not args.desktop or (args.action != "start" and not args.supervise)):
        raise ValueError("--settings-ui nexaui requires a new --desktop start")
    if args.settings_binding is not None and (not args.supervise or re.fullmatch("[0-9a-f]{64}", args.settings_binding) is None):
        raise ValueError("Settings binding belongs to the original supervisor only")
    if args.token is not None and re.fullmatch("[0-9a-f]{32}", args.token) is None: raise ValueError("invalid development token")
    if args.supervise:
        if args.token is None: raise ValueError("supervision requires its selected development token")
        return supervise(args.profile, args.token, args.desktop, args.compose, args.settings_ui, args.settings_binding)
    if not args.guest:
        import shlex
        import run_vm as vm
        if not vm.running(): raise RuntimeError("dedicated VM identity is not verified")
        command = ["sudo", "-n", "python3", SOURCE.as_posix(), args.action, "--guest", "--carrier-uuid", vm.config()["uuid"], "--profile", args.profile]
        if args.desktop: command.append("--desktop")
        if args.compose: command.append("--compose")
        if args.settings_ui != "sdl": command += ["--settings-ui", args.settings_ui]
        if args.token: command += ["--token", args.token]
        elif args.action == "stop":
            probe = [*command]; probe[4] = "status"
            result = subprocess.check_output([sys.executable, str(Path(__file__).parent / "run_vm.py"), "ssh", shlex.join(probe)], stderr=subprocess.PIPE, creationflags=vm.FLAGS)
            token = (json.loads(result).get("record") or {}).get("token")
            if not token: raise RuntimeError("no current development token")
            command += ["--token", token]
        return subprocess.call([sys.executable, str(Path(__file__).parent / "run_vm.py"), "ssh", shlex.join(command)],
                               stdin=sys.stdin, stdout=sys.stdout, stderr=sys.stderr, creationflags=vm.FLAGS)
    if os.getuid() != 0 or args.carrier_uuid is None: raise RuntimeError("explicit root-only disposable carrier command required")
    carrier = str(uuid.UUID(args.carrier_uuid))
    if Path("/sys/class/dmi/id/product_uuid").read_text().strip().lower() != carrier or Path("/sys/class/dmi/id/product_name").read_text().strip() != "Aetheri P0 disposable VM":
        raise RuntimeError("not the independently selected disposable carrier")
    if run("getenforce") != "Enforcing": raise RuntimeError("carrier must remain enforcing")
    if not CONTROL.exists() and args.action == "status": print(json.dumps(dict(format=1, record=None))); return 0
    with locked(create=args.action == "start"):
        record = load(carrier, args.token)
        if args.action == "start": record = start(record, carrier, args.profile, args.desktop, args.compose, args.settings_ui)
        elif args.action == "stop":
            if record is None or args.token is None: raise RuntimeError("stop requires an original development token")
            record = stop(record)
        result = dict(format=1, record=record)
        result["observation"] = observation(record)
        print(json.dumps(result, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
