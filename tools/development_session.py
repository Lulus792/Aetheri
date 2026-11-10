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
import tempfile
import time
import uuid

TEMPORARY_PROFILE = "tmpfs-v1"
TEMPORARY_BYTES = 1024**3
TEMPORARY_FILESYSTEM = "/tmp:rw,nosuid,nodev,size=1G,mode=1777"

CONTROL = Path("/var/lib/aetheri-development-session")
USER_ROOT = Path("/home/aetheri/development-sessions")
SOURCE = Path("/home/aetheri/source/tools/development_session.py")
PROJECT = Path(__file__).resolve().parents[1]
UI = Path("/home/aetheri/deps/ui-gles-release-04/prefix")
TERMINAL_FONT = Path("/usr/share/fonts/liberation-mono-fonts/LiberationMono-Regular.ttf")
APP_SELECTORS = {b"S": "settings", b"T": "terminal", b"F": "files"}
APP_ROLES = ("files", "terminal", "settings")  # Files retains its document owner before later views retire.
PROFILES = ("release", "debug")
SETTINGS_UIS = ("sdl", "nexaui", "nexaui-owned")
FILES_UIS = ("sdl", "nexaui-owned")
TERMINAL_UIS = ("sdl", "nexaui-owned")
KEYBOARD_LAYOUTS = ("us", "de")
TERMINAL_BOLD_FONT = Path("/usr/share/fonts/liberation-mono-fonts/LiberationMono-Bold.ttf")
# Frozen shape of aetheri-settings-v1; new source sets need a new kind/validator.
OWN_SETTINGS_SOURCES = ("CMakeLists.txt", "src/settings/nexaui_main.c", "src/settings/nexaui_view.c",
                        "src/settings/nexaui_view.h", "src/settings/NEXAUI-NOTICE.txt")
# Frozen aetheri-files-v1 sets; include the compiled shared model dependencies.
FILES_MODEL_SOURCES = tuple("src/files/" + name + suffix for name in
    ("model", "session", "create", "rename", "launch", "layout") for suffix in (".c", ".h")) + (
    "src/editor/file.c", "src/editor/file.h", "src/editor/document.c", "src/editor/document.h",
    "src/text/grapheme.c", "src/text/grapheme.h", "src/text/grapheme_data.h")
OWN_FILES_SOURCES = ("CMakeLists.txt", "src/files/nexaui_main.c", "src/files/nexaui_view.c",
                    "src/files/nexaui_view.h", "src/settings/NEXAUI-NOTICE.txt")
# Frozen aetheri-terminal-v1; its SDL host and compiled I/O dependencies are own code.
TERMINAL_MODEL_SOURCES = tuple("src/terminal/" + name + suffix for name in
    ("model", "session", "pty", "clipboard", "paste", "composition") for suffix in (".c", ".h"))
OWN_TERMINAL_SOURCES = ("CMakeLists.txt", "src/terminal/main.c", "src/terminal/nexaui_view.c", "src/terminal/nexaui_view.h")
# Additive v2: preserve v1's historical source set and closed receipts.
TERMINAL_MULTIPLEX_SOURCES = TERMINAL_MODEL_SOURCES + tuple("src/terminal/" + name + suffix
    for name in ("spawn", "workspace") for suffix in (".c", ".h"))
TERMINAL_STARTER_SOURCES = ("src/terminal/pty_starter.c",)
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


def profile_fingerprint(profile):
    return hashlib.sha256(json.dumps(profile, sort_keys=True, separators=(",", ":")).encode()).hexdigest()

settings_fingerprint = profile_fingerprint  # Preserve the existing canonical binding.

def valid_nexaui_build_path(value, build_profile):
    """Developer directory shape only; selection still checks the SDK and cache."""
    if build_profile not in PROFILES or not isinstance(value, str): return False
    return re.fullmatch(r"/home/aetheri/deps/(?:nexaui-settings-" + build_profile +
                        r"-[0-9]+|nexaui-desktop-" + build_profile + r"-[0-9a-f]{12})", value) is not None


def validate_settings_profile(profile, build_profile, owned_view=False):
    keys = {"revision", "version", "build", "artifacts", "model", "metadata_sha256", "pin_sha256"}
    if owned_view: keys.add("presentation")
    if not isinstance(profile, dict) or set(profile) != keys:
        raise RuntimeError("invalid development Settings binding")
    if not valid_nexaui_build_path(profile["build"], build_profile):
        raise RuntimeError("invalid development Settings build")
    if not isinstance(profile["revision"], str) or re.fullmatch("[0-9a-f]{40}", profile["revision"]) is None:
        raise RuntimeError("invalid development Settings revision")
    if not isinstance(profile["version"], str) or re.fullmatch("[0-9]+\\.[0-9]+\\.[0-9]+", profile["version"]) is None:
        raise RuntimeError("invalid development Settings version")
    artifacts = profile["artifacts"]
    expected = {"prefix/lib/libnexaui.so." + profile["version"]}
    if not owned_view: expected.add("consumer-build/aetheri_model_view")
    if not isinstance(artifacts, dict) or set(artifacts) != expected:
        raise RuntimeError("invalid development Settings artifacts")
    model = profile["model"]
    if not isinstance(model, dict) or set(model) != {"src/settings/model.c", "src/settings/model.h"}:
        raise RuntimeError("invalid development Settings model")
    for value in (*artifacts.values(), *model.values(), profile["metadata_sha256"], profile["pin_sha256"]):
        if not isinstance(value, str) or re.fullmatch("[0-9a-f]{64}", value) is None:
            raise RuntimeError("invalid development Settings digest")
    if owned_view:
        view = profile["presentation"]
        if not isinstance(view, dict) or set(view) != {"kind", "binary", "sha256", "sources", "cache_sha256"} or view["kind"] != "aetheri-settings-v1":
            raise RuntimeError("invalid owned Settings presentation")
        if view["binary"] != (build_for(build_profile) / "aetheri-settings-nexaui").as_posix():
            raise RuntimeError("invalid owned Settings executable")
        if not isinstance(view["sources"], dict) or set(view["sources"]) != set(OWN_SETTINGS_SOURCES):
            raise RuntimeError("invalid owned Settings source binding")
        for value in (view["sha256"], view["cache_sha256"], *view["sources"].values()):
            if not isinstance(value, str) or re.fullmatch("[0-9a-f]{64}", value) is None:
                raise RuntimeError("invalid owned Settings digest")
    return profile


def checked_nexaui_artifact(path, expected=None):
    path = Path(path); current = path.lstat()
    if path.resolve() != path or not stat.S_ISREG(current.st_mode) or current.st_uid != 1000 or stat.S_IMODE(current.st_mode) != 0o755 or current.st_nlink != 1 or current.st_size > 16*1024**2:
        raise RuntimeError("NexaUI artifact changed")
    value = digest(path)
    if expected is not None and value != expected: raise RuntimeError("NexaUI artifact changed")
    return value


def nexaui_candidate(build_profile):
    pin_path = PROJECT / "spec/nexaui-inputs.json"
    with pin_path.open("rb") as stream: pin_bytes = stream.read(65537)
    if len(pin_bytes) > 65536: raise RuntimeError("NexaUI pin exceeded bound")
    pin = json.loads(pin_bytes)
    build = Path(pin["development_builds"][build_profile])
    if build.resolve() != build: raise RuntimeError("aliased NexaUI build")
    owned(build, 1000, 0o700, directory=True)
    metadata = build / "build.json"; owned(metadata, 1000, 0o644)
    with metadata.open("rb") as stream: metadata_bytes = stream.read(65537)
    if len(metadata_bytes) > 65536: raise RuntimeError("NexaUI metadata exceeded bound")
    record = json.loads(metadata_bytes)
    if record["revision"] != pin["revision"] or record["version"] != pin["version"] or record["atspi_compiled"] is not True or record["sanitizers"] is not (build_profile == "debug"):
        raise RuntimeError("NexaUI candidate does not match selected build")
    return pin, pin_bytes, build, record, metadata_bytes


def nexaui_view_cache(build_profile, sdk):
    cache = build_for(build_profile) / "CMakeCache.txt"; owned(cache, 1000, 0o644)
    with cache.open("rb") as stream: data = stream.read(65537)
    if len(data) > 65536: raise RuntimeError("NexaUI view build cache exceeded bound")
    required = ("AETHERI_NEXAUI_PREFIX:PATH=" + str(sdk / "prefix"),
                "NexaUI_DIR:PATH=" + str(sdk / "prefix/lib/cmake/NexaUI"),
                "AETHERI_SANITIZERS:BOOL=" + ("ON" if build_profile == "debug" else "OFF"))
    lines = data.decode("utf-8").splitlines()
    if any(lines.count(line) != 1 for line in required):
        raise RuntimeError("owned view configuration does not match selected SDK/profile")
    return hashlib.sha256(data).hexdigest()


def settings_selection(build_profile, ui):
    """Read-only developer consistency check, never production code admission.

    The ordinary build account controls these mutable inputs. Hashing them is
    not a sealed store, publisher trust, atomic exec pin or volume authentication.
    """
    if ui == "sdl": return None
    if ui not in ("nexaui", "nexaui-owned") or build_profile not in PROFILES:
        raise RuntimeError("invalid Settings UI/profile")
    try:
        pin, pin_bytes, build, record, metadata_bytes = nexaui_candidate(build_profile)
        names = ("prefix/lib/libnexaui.so." + pin["version"],)
        if ui == "nexaui": names += ("consumer-build/aetheri_model_view",)
        selected = dict(revision=pin["revision"], version=pin["version"], build=str(build),
                        artifacts={n: record["artifacts"][n] for n in names}, model=record["model"],
                        metadata_sha256=hashlib.sha256(metadata_bytes).hexdigest(), pin_sha256=hashlib.sha256(pin_bytes).hexdigest())
        if ui == "nexaui-owned":
            # The own app links its own model. The unused upstream consumer's
            # copied model must not force an SDK rebuild after an app edit.
            selected["model"] = {n:digest(PROJECT / n) for n in ("src/settings/model.c", "src/settings/model.h")}
            own_build = build_for(build_profile)
            binary = own_build / "aetheri-settings-nexaui"
            selected["presentation"] = dict(kind="aetheri-settings-v1", binary=str(binary),
                sha256=checked_nexaui_artifact(binary), sources={n:digest(PROJECT / n) for n in OWN_SETTINGS_SOURCES},
                cache_sha256=nexaui_view_cache(build_profile, build))
        validate_settings_profile(selected, build_profile, ui == "nexaui-owned")
        for name, expected in selected["model"].items():
            if digest(PROJECT / name) != expected: raise RuntimeError("Settings model changed")
        for name, expected in selected["artifacts"].items():
            checked_nexaui_artifact(build / name, expected)
        return selected
    except (OSError, KeyError, TypeError, ValueError) as error:
        raise RuntimeError("Settings candidate check failed") from error


def validate_files_profile(profile, build_profile):
    keys = {"revision", "version", "build", "artifacts", "model", "metadata_sha256", "pin_sha256", "presentation"}
    if not isinstance(profile, dict) or set(profile) != keys: raise RuntimeError("invalid Files profile")
    if not valid_nexaui_build_path(profile["build"], build_profile):
        raise RuntimeError("invalid Files SDK build")
    if not isinstance(profile["revision"], str) or re.fullmatch("[0-9a-f]{40}", profile["revision"]) is None:
        raise RuntimeError("invalid Files SDK revision")
    if not isinstance(profile["version"], str) or re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+", profile["version"]) is None:
        raise RuntimeError("invalid Files SDK version")
    artifacts, model, view = profile["artifacts"], profile["model"], profile["presentation"]
    if not isinstance(artifacts, dict) or set(artifacts) != {"prefix/lib/libnexaui.so." + profile["version"]}:
        raise RuntimeError("invalid Files SDK artifacts")
    if not isinstance(model, dict) or set(model) != set(FILES_MODEL_SOURCES): raise RuntimeError("invalid Files model binding")
    if not isinstance(view, dict) or set(view) != {"kind", "binary", "sha256", "sources", "cache_sha256"} or view["kind"] != "aetheri-files-v1":
        raise RuntimeError("invalid Files presentation")
    if view["binary"] != (build_for(build_profile) / "aetheri-files-nexaui").as_posix():
        raise RuntimeError("invalid Files executable")
    if not isinstance(view["sources"], dict) or set(view["sources"]) != set(OWN_FILES_SOURCES):
        raise RuntimeError("invalid Files presentation sources")
    for value in (*artifacts.values(), *model.values(), profile["metadata_sha256"], profile["pin_sha256"], view["sha256"], view["cache_sha256"], *view["sources"].values()):
        if not isinstance(value, str) or re.fullmatch("[0-9a-f]{64}", value) is None: raise RuntimeError("invalid Files digest")
    return profile


def files_selection(build_profile, ui):
    # Developer consistency only: no sealed code, atomic exec pin or publisher trust.
    if ui == "sdl": return None
    if ui != "nexaui-owned" or build_profile not in PROFILES: raise RuntimeError("invalid Files UI/profile")
    try:
        pin, pin_bytes, sdk, record, metadata_bytes = nexaui_candidate(build_profile)
        library = "prefix/lib/libnexaui.so." + pin["version"]
        binary = build_for(build_profile) / "aetheri-files-nexaui"
        selected = dict(revision=pin["revision"], version=pin["version"], build=str(sdk),
            artifacts={library:checked_nexaui_artifact(sdk / library, record["artifacts"][library])},
            model={n:digest(PROJECT / n) for n in FILES_MODEL_SOURCES},
            metadata_sha256=hashlib.sha256(metadata_bytes).hexdigest(), pin_sha256=hashlib.sha256(pin_bytes).hexdigest(),
            presentation=dict(kind="aetheri-files-v1", binary=str(binary), sha256=checked_nexaui_artifact(binary),
                sources={n:digest(PROJECT / n) for n in OWN_FILES_SOURCES}, cache_sha256=nexaui_view_cache(build_profile, sdk)))
        return validate_files_profile(selected, build_profile)
    except (OSError, KeyError, TypeError, ValueError) as error:
        raise RuntimeError("Files candidate check failed") from error


def validate_terminal_profile(profile, build_profile):
    keys = {"revision", "version", "build", "artifacts", "model", "metadata_sha256", "pin_sha256",
            "presentation", "assets", "ui_pin_sha256"}
    if not isinstance(profile, dict) or set(profile) != keys: raise RuntimeError("invalid Terminal profile")
    if not valid_nexaui_build_path(profile["build"], build_profile):
        raise RuntimeError("invalid Terminal SDK build")
    if not isinstance(profile["revision"], str) or re.fullmatch("[0-9a-f]{40}", profile["revision"]) is None:
        raise RuntimeError("invalid Terminal SDK revision")
    if not isinstance(profile["version"], str) or re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+", profile["version"]) is None:
        raise RuntimeError("invalid Terminal SDK version")
    artifacts, model, view = profile["artifacts"], profile["model"], profile["presentation"]
    if not isinstance(artifacts, dict) or set(artifacts) != {"prefix/lib/libnexaui.so." + profile["version"]}:
        raise RuntimeError("invalid Terminal SDK artifacts")
    if not isinstance(view, dict) or view.get("kind") not in ("aetheri-terminal-v1", "aetheri-terminal-v2"):
        raise RuntimeError("invalid Terminal presentation")
    multiplex = view["kind"] == "aetheri-terminal-v2"
    view_keys = {"kind", "binary", "sha256", "sources", "cache_sha256"}
    if multiplex: view_keys.add("starter")
    if set(view) != view_keys: raise RuntimeError("invalid Terminal presentation")
    model_sources = TERMINAL_MULTIPLEX_SOURCES if multiplex else TERMINAL_MODEL_SOURCES
    if not isinstance(model, dict) or set(model) != set(model_sources): raise RuntimeError("invalid Terminal model binding")
    if view["binary"] != (build_for(build_profile) / "aetheri-terminal-nexaui").as_posix(): raise RuntimeError("invalid Terminal executable")
    if not isinstance(view["sources"], dict) or set(view["sources"]) != set(OWN_TERMINAL_SOURCES): raise RuntimeError("invalid Terminal presentation sources")
    starter_digests = ()
    if multiplex:
        starter = view["starter"]
        if not isinstance(starter, dict) or set(starter) != {"binary", "sha256", "sources"}:
            raise RuntimeError("invalid Terminal starter binding")
        if starter["binary"] != (build_for(build_profile) / "aetheri-terminal-pty-starter").as_posix():
            raise RuntimeError("invalid Terminal starter executable")
        if not isinstance(starter["sources"], dict) or set(starter["sources"]) != set(TERMINAL_STARTER_SOURCES):
            raise RuntimeError("invalid Terminal starter sources")
        starter_digests = (starter["sha256"], *starter["sources"].values())
    assets = profile["assets"]
    if not isinstance(assets, dict) or set(assets) != {str(TERMINAL_FONT), str(TERMINAL_BOLD_FONT), "/usr/share/licenses/liberation-mono-fonts/LICENSE"}:
        raise RuntimeError("invalid Terminal assets")
    for value in (*artifacts.values(), *model.values(), *assets.values(), profile["metadata_sha256"], profile["pin_sha256"],
                  profile["ui_pin_sha256"], view["sha256"], view["cache_sha256"], *view["sources"].values(), *starter_digests):
        if not isinstance(value, str) or re.fullmatch("[0-9a-f]{64}", value) is None: raise RuntimeError("invalid Terminal digest")
    return profile


def terminal_selection(build_profile, ui):
    # Developer consistency only, with the same mutable-source limits as Files.
    if ui == "sdl": return None
    if ui != "nexaui-owned" or build_profile not in PROFILES: raise RuntimeError("invalid Terminal UI/profile")
    try:
        pin, pin_bytes, sdk, record, metadata_bytes = nexaui_candidate(build_profile)
        with (PROJECT / "spec/ui-inputs.json").open("rb") as stream: ui_bytes = stream.read(65537)
        if len(ui_bytes) > 65536: raise RuntimeError("Terminal asset pin exceeded bound")
        font = json.loads(ui_bytes)["terminal_font"]; bold = font["bold"]
        license_path = "/usr/share/licenses/liberation-mono-fonts/LICENSE"
        if font["path"] != str(TERMINAL_FONT) or bold["path"] != str(TERMINAL_BOLD_FONT) or font["license_path"] != license_path or bold["license_path"] != license_path or font["license_sha256"] != bold["license_sha256"]:
            raise RuntimeError("unexpected fixed Terminal asset paths/license")
        assets = {str(TERMINAL_FONT):font["sha256"], str(TERMINAL_BOLD_FONT):bold["sha256"], license_path:font["license_sha256"]}
        for path, expected in assets.items():
            if digest(path) != expected: raise RuntimeError("reviewed Terminal assets changed")
        library = "prefix/lib/libnexaui.so." + pin["version"]; binary = build_for(build_profile) / "aetheri-terminal-nexaui"
        selected = dict(revision=pin["revision"], version=pin["version"], build=str(sdk),
            artifacts={library:checked_nexaui_artifact(sdk / library, record["artifacts"][library])},
            model={n:digest(PROJECT / n) for n in TERMINAL_MULTIPLEX_SOURCES}, assets=assets,
            metadata_sha256=hashlib.sha256(metadata_bytes).hexdigest(), pin_sha256=hashlib.sha256(pin_bytes).hexdigest(),
            ui_pin_sha256=hashlib.sha256(ui_bytes).hexdigest(),
            presentation=dict(kind="aetheri-terminal-v2", binary=str(binary), sha256=checked_nexaui_artifact(binary),
                sources={n:digest(PROJECT / n) for n in OWN_TERMINAL_SOURCES}, cache_sha256=nexaui_view_cache(build_profile, sdk),
                starter=dict(binary=str(build_for(build_profile) / "aetheri-terminal-pty-starter"),
                    sha256=checked_nexaui_artifact(build_for(build_profile) / "aetheri-terminal-pty-starter"),
                    sources={n:digest(PROJECT / n) for n in TERMINAL_STARTER_SOURCES})))
        return validate_terminal_profile(selected, build_profile)
    except (OSError, KeyError, TypeError, ValueError) as error:
        raise RuntimeError("Terminal candidate check failed") from error


def application_command(role, build, cache, settings_profile=None, files_profile=None, terminal_profile=None,
                        *, ui=None, terminal_font=None, terminal_bold_font=None):
    ui = UI if ui is None else ui
    terminal_font = TERMINAL_FONT if terminal_font is None else terminal_font
    terminal_bold_font = TERMINAL_BOLD_FONT if terminal_bold_font is None else terminal_bold_font
    if role == "settings":
        if settings_profile is not None:
            executable = settings_profile["presentation"]["binary"] if "presentation" in settings_profile else str(Path(settings_profile["build"]) / "consumer-build/aetheri_model_view")
            return [executable,
                    "--font", str(ui / "share/aetheri-fonts/LiberationSans-Regular.ttf"), "--cache", str(cache), "--diagnostics"]
        return [str(build / "aetheri-settings"), "--font", str(ui / "share/aetheri-fonts/LiberationSans-Regular.ttf"),
                "--cache", str(cache), "--diagnostics"]
    if role == "terminal":
        executable = terminal_profile["presentation"]["binary"] if terminal_profile is not None else str(build / "aetheri-terminal")
        command = [executable, "--shell", str(build / "aetheri-shell"), "--font", str(terminal_font), "--cache", str(cache), "--diagnostics"]
        if terminal_profile is not None: command += ["--bold-font", str(terminal_bold_font)]
        if terminal_profile is not None and "starter" in terminal_profile["presentation"]:
            command += ["--pty-starter", terminal_profile["presentation"]["starter"]["binary"]]
        return command
    if role == "files":
        executable = files_profile["presentation"]["binary"] if files_profile is not None else str(build / "aetheri-files")
        command = [executable, "--root", str(cache.parent / "documents"),
                "--font", str(ui / "share/aetheri-fonts/LiberationSans-Regular.ttf"),
                "--terminal-font", str(terminal_font), "--cache", str(cache),
                "--terminal", terminal_profile["presentation"]["binary"] if terminal_profile is not None else str(build / "aetheri-terminal"), "--editor", str(build / "aetheri-editor"), "--diagnostics"]
        if terminal_profile is not None: command += ["--terminal-bold-font", str(terminal_bold_font)]
        return command
    raise ValueError("unknown fixed development application")


def development_binaries(build, desktop, settings_profile=None, files_profile=None, terminal_profile=None):
    names = ("aetheri-compositor", "aetheri-settings", "aetheri-desktop", "aetheri-terminal", "aetheri-shell", "aetheri-files", "aetheri-editor") if desktop else ("aetheri-compositor", "aetheri-settings")
    if settings_profile is not None: names = tuple(n for n in names if n != "aetheri-settings")
    if files_profile is not None: names = tuple(n for n in names if n != "aetheri-files")
    if terminal_profile is not None: names = tuple(n for n in names if n != "aetheri-terminal")
    binaries = {str(build / name):digest(build / name) for name in names}
    for selected in (settings_profile, files_profile, terminal_profile):
        if selected is None: continue
        for name, value in selected["artifacts"].items():
            path = str(Path(selected["build"]) / name)
            if path in binaries and binaries[path] != value: raise RuntimeError("conflicting selected SDK image")
            binaries[path] = value
        if "presentation" in selected:
            view = selected["presentation"]; binaries[view["binary"]] = view["sha256"]
            if "starter" in view:
                starter = view["starter"]; binaries[starter["binary"]] = starter["sha256"]
    return binaries


def keyboard_fingerprint(layout):
    if layout is None: return None
    if type(layout) is not str or layout not in KEYBOARD_LAYOUTS:
        raise ValueError("unsupported development keyboard layout")
    return profile_fingerprint(dict(kind="aetheri-keyboard-v1", layout=layout))


def compositor_ready(line, keyboard_layout=None):
    keyboard_fingerprint(keyboard_layout)
    if keyboard_layout is None: return line.startswith(b"READY ")
    # Original child diagnostics are development consistency evidence, not app
    # admission. Never accept another/missing/ambiguous selected layout.
    expected = rb"READY aetheri-development outputs=[1-3] renderer=pixman keyboard_layout=" + keyboard_layout.encode()
    return re.fullmatch(expected, line) is not None


def compositor_launch(build, shell_server, compose=False, keyboard_layout=None):
    keyboard_fingerprint(keyboard_layout)
    args = [str(build / "aetheri-compositor"), "--drm", "aetheri-development", "-", "/dev/dri/card0"]
    if shell_server is not None:
        args += ["--shell-fd", str(shell_server.fileno())]
    if compose: args.append("--compose")
    if keyboard_layout is not None: args += ["--keyboard-layout", keyboard_layout]
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


def temporary_area():
    """Observe this developer unit's tmpfs; not production app admission."""
    fd = os.open("/tmp", os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW | os.O_CLOEXEC)
    try:
        info, space = os.fstat(fd), os.fstatvfs(fd)
        flags = os.ST_NOSUID | os.ST_NODEV
        if (info.st_uid != 0 or stat.S_IMODE(info.st_mode) != 0o1777 or
                space.f_blocks * space.f_frsize != TEMPORARY_BYTES or space.f_flag & flags != flags):
            raise RuntimeError("development temporary filesystem differs")
        with Path("/proc/self/mountinfo").open() as stream: mounts = stream.read(1024**2 + 1)
        if len(mounts) > 1024**2: raise RuntimeError("development mount table exceeded bound")
        selected = [line.split(" - ") for line in mounts.splitlines()
                    if len(line.split()) > 5 and line.split()[4] == "/tmp"]
        if len(selected) != 1 or len(selected[0]) != 2:
            raise RuntimeError("development temporary mount is ambiguous")
        native = selected[0][1].split()
        if len(native) != 3 or native[0] != "tmpfs" or any(
                option in native[2].split(",") for option in ("usrquota", "grpquota", "prjquota")):
            raise RuntimeError("development requires its separate temporary tmpfs")
        # Sparse ftruncate/mmap success does not prove writable backing pages.
        with tempfile.TemporaryFile(dir="/tmp") as probe:
            os.posix_fallocate(probe.fileno(), 0, 4 * 1024**2)
        return dict(profile=TEMPORARY_PROFILE, bytes=TEMPORARY_BYTES,
                    available=space.f_bavail * space.f_frsize, device=info.st_dev)
    finally: os.close(fd)


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
    if "keyboard_layout" in record or "keyboard_binding" in record:
        layout = record.get("keyboard_layout")
        if layout is None or record.get("keyboard_binding") != keyboard_fingerprint(layout):
            raise RuntimeError("invalid development keyboard binding")
    ui = record.get("settings_ui", "sdl")
    selected = record.get("settings_profile")
    binding = record.get("settings_binding")
    if ui not in SETTINGS_UIS or (ui == "sdl" and (selected is not None or binding is not None)):
        raise RuntimeError("invalid development Settings selection")
    if ui != "sdl":
        if not record.get("desktop", False): raise RuntimeError("NexaUI selection requires desktop")
        validate_settings_profile(selected, record["profile"], ui == "nexaui-owned")
        if binding != settings_fingerprint(selected): raise RuntimeError("development Settings binding changed")
    files_ui = record.get("files_ui", "sdl")
    files_profile, files_binding = record.get("files_profile"), record.get("files_binding")
    if files_ui not in FILES_UIS or (files_ui == "sdl" and (files_profile is not None or files_binding is not None)):
        raise RuntimeError("invalid development Files selection")
    if files_ui != "sdl":
        if not record.get("desktop", False): raise RuntimeError("Files selection requires desktop")
        validate_files_profile(files_profile, record["profile"])
        if files_binding != profile_fingerprint(files_profile): raise RuntimeError("development Files binding changed")
    terminal_ui = record.get("terminal_ui", "sdl")
    terminal_profile, terminal_binding = record.get("terminal_profile"), record.get("terminal_binding")
    if terminal_ui not in TERMINAL_UIS or (terminal_ui == "sdl" and (terminal_profile is not None or terminal_binding is not None)):
        raise RuntimeError("invalid development Terminal selection")
    if terminal_ui != "sdl":
        if not record.get("desktop", False): raise RuntimeError("Terminal selection requires desktop")
        validate_terminal_profile(terminal_profile, record["profile"])
        if terminal_binding != profile_fingerprint(terminal_profile): raise RuntimeError("development Terminal binding changed")
    if "temporary_profile" in record and record["temporary_profile"] != TEMPORARY_PROFILE:
        raise RuntimeError("invalid development temporary profile")
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
             "ExecMainCode", "InvocationID", "ControlGroup", "Result", "Type", "RemainAfterExit", "User", "TemporaryFileSystem")
    return dict(line.split("=", 1) for line in run("systemctl", "show", unit,
                *["--property=" + name for name in names]).splitlines())


def require_original(record, props):
    if Path("/proc/sys/kernel/random/boot_id").read_text().strip() != record["boot"]:
        raise RuntimeError("different boot: development session needs manual reconciliation")
    if props["InvocationID"] != record["invocation"] or props["ExecMainPID"] != str(record["pid"]):
        raise RuntimeError("original development unit was lost or replaced")
    if props["MainPID"] not in ("0", str(record["pid"])):
        raise RuntimeError("development main process changed")
    expected_tmp = TEMPORARY_FILESYSTEM if record.get("temporary_profile") == TEMPORARY_PROFILE else ""
    if props.get("TemporaryFileSystem", "") != expected_tmp:
        raise RuntimeError("development temporary unit configuration drift")
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


def start(record, carrier, profile, desktop=False, compose=False, settings_ui="sdl", files_ui="sdl", terminal_ui="sdl", keyboard_layout=None):
    keyboard_binding = keyboard_fingerprint(keyboard_layout)
    if record and record["phase"] != "closed":
        raise RuntimeError("an existing development intent must be observed/stopped first")
    if (PROJECT / ".aetheri-promotion.pending").exists():
        raise RuntimeError("development source/build promotion is incomplete")
    if settings_ui not in SETTINGS_UIS or (settings_ui != "sdl" and not desktop):
        raise RuntimeError("Settings UI requires its selected desktop profile")
    selected = settings_selection(profile, settings_ui)
    binding = settings_fingerprint(selected) if selected is not None else None
    if files_ui not in FILES_UIS or (files_ui != "sdl" and not desktop):
        raise RuntimeError("Files UI requires its selected desktop profile")
    files_profile = files_selection(profile, files_ui)
    files_binding = profile_fingerprint(files_profile) if files_profile is not None else None
    if terminal_ui not in TERMINAL_UIS or (terminal_ui != "sdl" and not desktop):
        raise RuntimeError("Terminal UI requires its selected desktop profile")
    terminal_profile = terminal_selection(profile, terminal_ui)
    terminal_binding = profile_fingerprint(terminal_profile) if terminal_profile is not None else None
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
    binaries = development_binaries(build, desktop, selected, files_profile, terminal_profile)
    assets = terminal_assets() if desktop else {}
    if terminal_profile is not None:
        for path, value in terminal_profile["assets"].items():
            if path in assets and assets[path] != value: raise RuntimeError("conflicting Terminal assets")
            assets[path] = value
    record = dict(format=1, token=token, carrier=carrier, profile=profile,
                  boot=Path("/proc/sys/kernel/random/boot_id").read_text().strip(),
                  unit="aetheri-development-" + token + ".service", phase="preparing",
                  invocation=None, pid=None, binaries=binaries, assets=assets, desktop=desktop,
                  launch_protocol=3 if desktop else 1, compose=compose, temporary_profile=TEMPORARY_PROFILE,
                  created_unix=time.time(), exit=None)
    record.update(settings_ui=settings_ui, settings_profile=selected, settings_binding=binding)
    record.update(files_ui=files_ui, files_profile=files_profile, files_binding=files_binding)
    record.update(terminal_ui=terminal_ui, terminal_profile=terminal_profile, terminal_binding=terminal_binding)
    if keyboard_layout is not None:
        record.update(keyboard_layout=keyboard_layout, keyboard_binding=keyboard_binding)
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
            "--property=TemporaryFileSystem=" + TEMPORARY_FILESYSTEM,
            "--property=AmbientCapabilities=", "--property=Restart=no", "--property=KillMode=control-group",
            "--property=SendSIGKILL=no", "--property=TimeoutStartSec=15", "--property=TimeoutStopSec=10",
            "--property=Conflicts=getty@tty8.service", "--property=After=getty@tty8.service",
            "--setenv=XDG_SESSION_TYPE=wayland", "--setenv=XDG_SESSION_CLASS=user",
            "--setenv=XDG_SEAT=seat0", "--setenv=XDG_VTNR=8", "--setenv=LIBSEAT_BACKEND=logind",
            "/usr/bin/python3", str(SOURCE), "--supervise", "--profile", profile, "--token", token,
            "--temporary-profile", TEMPORARY_PROFILE]
    if desktop: args.append("--desktop")
    if compose: args.append("--compose")
    if keyboard_layout is not None: args += ["--keyboard-layout", keyboard_layout, "--keyboard-binding", keyboard_binding]
    if selected is not None: args += ["--settings-ui", settings_ui, "--settings-binding", binding]
    if files_profile is not None: args += ["--files-ui", files_ui, "--files-binding", files_binding]
    if terminal_profile is not None: args += ["--terminal-ui", terminal_ui, "--terminal-binding", terminal_binding]
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


def supervise(profile, token, desktop=False, compose=False, settings_ui="sdl", settings_binding=None,
              files_ui="sdl", files_binding=None, terminal_ui="sdl", terminal_binding=None, temporary_profile=None,
              keyboard_layout=None, keyboard_binding=None, current_runtime=None):
    if os.getuid() != 1000 or os.getuid() != os.geteuid():
        raise RuntimeError("development supervisor requires the ordinary carrier account")
    if keyboard_binding != keyboard_fingerprint(keyboard_layout):
        raise RuntimeError("original keyboard selection changed before supervision")
    runtime = None
    if current_runtime is not None:
        import development_runtime
        runtime = development_runtime.Runtime(current_runtime, profile, desktop, settings_ui, files_ui, terminal_ui)
    if settings_ui not in SETTINGS_UIS or (settings_ui != "sdl" and not desktop):
        raise RuntimeError("Settings UI requires its selected desktop profile")
    if temporary_profile not in (None, TEMPORARY_PROFILE):
        raise RuntimeError("invalid supervised temporary profile")
    if temporary_profile is not None:
        print("TEMPORARY " + json.dumps(temporary_area()), flush=True)
    selected = runtime.selection('settings') if runtime else settings_selection(profile, settings_ui)
    expected_binding = settings_fingerprint(selected) if selected is not None else None
    if settings_binding != expected_binding:
        raise RuntimeError("original Settings selection changed before supervision")
    if files_ui not in FILES_UIS or (files_ui != "sdl" and not desktop):
        raise RuntimeError("Files UI requires its selected desktop profile")
    files_profile = runtime.selection('files') if runtime else files_selection(profile, files_ui)
    expected_files = profile_fingerprint(files_profile) if files_profile is not None else None
    if files_binding != expected_files: raise RuntimeError("original Files selection changed before supervision")
    if terminal_ui not in TERMINAL_UIS or (terminal_ui != "sdl" and not desktop):
        raise RuntimeError("Terminal UI requires its selected desktop profile")
    terminal_profile = runtime.selection('terminal') if runtime else terminal_selection(profile, terminal_ui)
    expected_terminal = profile_fingerprint(terminal_profile) if terminal_profile is not None else None
    if terminal_binding != expected_terminal: raise RuntimeError("original Terminal selection changed before supervision")
    base = USER_ROOT / token
    for path in (base, base / "runtime", base / "cache"): owned(path, 1000, 0o700, directory=True)
    if desktop: owned(base / "documents", 1000, 0o700, directory=True)
    build = runtime.build if runtime else build_for(profile)
    ui = runtime.ui if runtime else UI
    assets = dict(ui=ui, terminal_font=runtime.font, terminal_bold_font=runtime.bold_font) if runtime else {}
    env = {**os.environ, "XDG_RUNTIME_DIR": str(base / "runtime"), "LIBSEAT_BACKEND": "logind",
           "WAYLAND_DISPLAY": "aetheri-development", "LIBGL_ALWAYS_SOFTWARE": "1",
           "GALLIUM_DRIVER": "llvmpipe", "LP_NUM_THREADS": "2"}
    for key in ("WAYLAND_SOCKET", "DISPLAY", "SDL_VIDEO_DRIVER", "SDL_RENDER_DRIVER"): env.pop(key, None)
    if runtime: env = runtime.environment(env)
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
        compositor_args, compositor_fds = compositor_launch(build, shell_server, compose, keyboard_layout)
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
                    if runtime:
                        try: runtime.recheck()
                        except (OSError, ValueError) as error:
                            print("CURRENT_RUNTIME_REJECTED " + role + " " + str(error), flush=True)
                            reply(b"E" + suffix); continue
                    if not runtime and role == "settings" and selected is not None:
                        try:
                            if settings_selection(profile, settings_ui) != selected:
                                raise RuntimeError("original Settings selection changed before spawn")
                        except RuntimeError as error:
                            print("SETTINGS_PROFILE_REJECTED " + str(error), flush=True)
                            reply(b"E" + suffix); continue
                    if not runtime and role == "files" and files_profile is not None:
                        try:
                            if files_selection(profile, files_ui) != files_profile:
                                raise RuntimeError("original Files selection changed before spawn")
                        except RuntimeError as error:
                            print("FILES_PROFILE_REJECTED " + str(error), flush=True)
                            reply(b"E" + suffix); continue
                    if not runtime and role in ("terminal", "files") and terminal_profile is not None:
                        try:
                            if terminal_selection(profile, terminal_ui) != terminal_profile:
                                raise RuntimeError("original Terminal selection changed before spawn")
                        except RuntimeError as error:
                            print("TERMINAL_PROFILE_REJECTED " + role + " " + str(error), flush=True)
                            reply(b"E" + suffix); continue
                    try:
                        spawn(role, application_command(role, build, base / "cache", selected, files_profile, terminal_profile, **assets))
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
                        if role == "compositor" and not compositor_ready(line, keyboard_layout):
                            raise RuntimeError("original compositor reported a different keyboard layout")
                        ready.add(role)
                        if role == "compositor" and not stopping:
                            if desktop:
                                spawn("desktop", [str(build / "aetheri-desktop"), "--font", str(ui / "share/aetheri-fonts/LiberationSans-Regular.ttf"),
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
    parser.add_argument("--keyboard-layout", choices=KEYBOARD_LAYOUTS, help="explicit startup keyboard map; requires a matching compositor build")
    parser.add_argument("--keyboard-binding", help=argparse.SUPPRESS)
    parser.add_argument("--settings-ui", choices=SETTINGS_UIS, default="sdl", help="explicit development desktop Settings view")
    parser.add_argument("--settings-binding", help=argparse.SUPPRESS)
    parser.add_argument("--files-ui", choices=FILES_UIS, default="sdl", help="explicit development desktop Files view")
    parser.add_argument("--files-binding", help=argparse.SUPPRESS)
    parser.add_argument("--terminal-ui", choices=TERMINAL_UIS, default="sdl", help="explicit development desktop Terminal/editor view")
    parser.add_argument("--terminal-binding", help=argparse.SUPPRESS)
    parser.add_argument("--guest", action="store_true", help=argparse.SUPPRESS)
    parser.add_argument("--supervise", action="store_true", help=argparse.SUPPRESS)
    parser.add_argument("--temporary-profile", choices=(TEMPORARY_PROFILE,), help=argparse.SUPPRESS)
    parser.add_argument("--carrier-uuid", help=argparse.SUPPRESS)
    args = parser.parse_args()
    if args.compose and args.action != "start" and not args.supervise:
        raise ValueError("--compose selects a new start profile only")
    if args.keyboard_layout is not None and args.action != "start" and not args.supervise:
        raise ValueError("--keyboard-layout selects a new start profile only")
    if args.keyboard_binding is not None and (not args.supervise or args.keyboard_layout is None or re.fullmatch("[0-9a-f]{64}", args.keyboard_binding) is None):
        raise ValueError("keyboard binding belongs to the original supervisor only")
    if args.settings_ui != "sdl" and (not args.desktop or (args.action != "start" and not args.supervise)):
        raise ValueError("a NexaUI Settings selection requires a new --desktop start")
    if args.settings_binding is not None and (not args.supervise or re.fullmatch("[0-9a-f]{64}", args.settings_binding) is None):
        raise ValueError("Settings binding belongs to the original supervisor only")
    if args.files_ui != "sdl" and (not args.desktop or (args.action != "start" and not args.supervise)):
        raise ValueError("a NexaUI Files selection requires a new --desktop start")
    if args.files_binding is not None and (not args.supervise or re.fullmatch("[0-9a-f]{64}", args.files_binding) is None):
        raise ValueError("Files binding belongs to the original supervisor only")
    if args.terminal_ui != "sdl" and (not args.desktop or (args.action != "start" and not args.supervise)):
        raise ValueError("a NexaUI Terminal selection requires a new --desktop start")
    if args.terminal_binding is not None and (not args.supervise or re.fullmatch("[0-9a-f]{64}", args.terminal_binding) is None):
        raise ValueError("Terminal binding belongs to the original supervisor only")
    if args.token is not None and re.fullmatch("[0-9a-f]{32}", args.token) is None: raise ValueError("invalid development token")
    if args.temporary_profile is not None and not args.supervise:
        raise ValueError("temporary profile belongs to the original supervisor only")
    if args.supervise:
        if args.token is None: raise ValueError("supervision requires its selected development token")
        return supervise(args.profile, args.token, args.desktop, args.compose, args.settings_ui, args.settings_binding,
                         args.files_ui, args.files_binding, args.terminal_ui, args.terminal_binding, args.temporary_profile,
                         args.keyboard_layout, args.keyboard_binding)
    if not args.guest:
        import shlex
        import run_vm as vm
        if not vm.running(): raise RuntimeError("dedicated VM identity is not verified")
        command = ["sudo", "-n", "python3", SOURCE.as_posix(), args.action, "--guest", "--carrier-uuid", vm.config()["uuid"], "--profile", args.profile]
        if args.desktop: command.append("--desktop")
        if args.compose: command.append("--compose")
        if args.keyboard_layout is not None: command += ["--keyboard-layout", args.keyboard_layout]
        if args.settings_ui != "sdl": command += ["--settings-ui", args.settings_ui]
        if args.files_ui != "sdl": command += ["--files-ui", args.files_ui]
        if args.terminal_ui != "sdl": command += ["--terminal-ui", args.terminal_ui]
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
        if args.action == "start": record = start(record, carrier, args.profile, args.desktop, args.compose, args.settings_ui, args.files_ui, args.terminal_ui, args.keyboard_layout)
        elif args.action == "stop":
            if record is None or args.token is None: raise RuntimeError("stop requires an original development token")
            record = stop(record)
        result = dict(format=1, record=record)
        result["observation"] = observation(record)
        print(json.dumps(result, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
