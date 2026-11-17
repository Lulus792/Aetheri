"""Stage the fixed Aetheri login layout for image assembly."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import stat
import sys


ROOT = Path(__file__).resolve().parents[1]
LIBEXEC = "usr/libexec/aetheri"
DATA = "usr/share/aetheri/login"
UNITS = "usr/lib/systemd/system"
MAX_INPUT = 16 * 1024 * 1024
FORMAT = 2

DESKTOP_EXECUTABLES = (
    "aetheri-settings-nexaui", "aetheri-files-nexaui", "aetheri-terminal-nexaui",
    "aetheri-terminal-pty-starter", "aetheri-shell", "aetheri-editor",
)

EXECUTABLES = (
    "aetheri-login-backend", "aetheri-login-startup", "aetheri-login-greeter",
    "aetheri-login-display", "aetheri-session", "aetheri-compositor",
    "aetheri-desktop", *DESKTOP_EXECUTABLES,
)
ARTIFACTS = {name: (f"{LIBEXEC}/{name}", 0o555) for name in EXECUTABLES}
ARTIFACTS.update({
    "greeter-font": (f"{DATA}/greeter.ttf", 0o444),
    "greeter-font-license": ("usr/share/licenses/aetheri-login/font.txt", 0o444),
    "backend-license": ("usr/share/licenses/aetheri-login/backend.txt", 0o444),
    "backend-dependency-notices": ("usr/share/licenses/aetheri-login/backend-dependencies.txt", 0o444),
})

SUPPORT = {
    "data/login/greetd.toml": f"{DATA}/greetd.toml",
    "data/login/ld.so.conf": "etc/ld.so.conf",
    "data/login/nsswitch.conf": "etc/nsswitch.conf",
    "data/pam.d/aetheri-login": "etc/pam.d/aetheri-login",
    "data/pam.d/aetheri-greeter": "etc/pam.d/aetheri-greeter",
    "data/pam.d/other": "etc/pam.d/other",
    "data/sysusers.d/aetheri-login.conf": "usr/lib/sysusers.d/aetheri-login.conf",
    "data/tmpfiles.d/aetheri-login.conf": "usr/lib/tmpfiles.d/aetheri-login.conf",
    "data/systemd/aetheri-login.service": f"{UNITS}/aetheri-login.service",
    "data/systemd/aetheri-login-startup.service.in": f"{UNITS}/aetheri-login-startup.service",
    "data/systemd/aetheri-login.service.d/20-session-shutdown.conf":
        f"{UNITS}/aetheri-login.service.d/20-session-shutdown.conf",
    "data/wayland-sessions/aetheri.desktop": "usr/share/wayland-sessions/aetheri.desktop",
    **{f"policy/{name}": f"{DATA}/policy/{name}" for name in (
        "login-startup.cil", "login.cil", "login-graphical.cil", "login-apps.cil", "login.fc")},
}


def digest(body: bytes) -> str:
    return hashlib.sha256(body).hexdigest()


def unique_object(pairs: list[tuple[str, object]]) -> dict:
    result = {}
    for name, value in pairs:
        if name in result:
            raise ValueError(f"duplicate manifest field: {name}")
        result[name] = value
    return result


def read_artifact(manifest: Path, name: str, selection: object) -> bytes:
    if not isinstance(selection, dict) or set(selection) != {"path", "size", "sha256"}:
        raise ValueError(f"invalid artifact selection: {name}")
    path, size, expected = (selection[key] for key in ("path", "size", "sha256"))
    if (not isinstance(path, str) or not path or "\0" in path
            or type(size) is not int or not 0 < size <= MAX_INPUT
            or not isinstance(expected, str) or not re.fullmatch("[0-9a-f]{64}", expected)):
        raise ValueError(f"invalid artifact metadata: {name}")
    source = manifest.parent / path
    if not stat.S_ISREG(source.lstat().st_mode):
        raise ValueError(f"artifact must be a regular file: {name}")
    with source.open("rb") as stream:
        body = stream.read(size + 1)
    if len(body) != size or digest(body) != expected:
        raise ValueError(f"artifact differs from its selection: {name}")
    if name in EXECUTABLES:
        # Full executable and live-process admission belong to the native verifier.
        if (len(body) < 64 or body[:7] != b"\x7fELF\x02\x01\x01"
                or body[16:18] not in (b"\x02\x00", b"\x03\x00") or body[18:20] != b"\x3e\x00"):
            raise ValueError(f"artifact must be an x86-64 ELF: {name}")
    elif name == "greeter-font" and body[:4] != b"\x00\x01\x00\x00":
        raise ValueError("greeter-font must be a TrueType file")
    elif name != "greeter-font":
        if b"\0" in body or not body.decode("utf-8").strip():
            raise ValueError(f"license or notice must be nonempty UTF-8 text: {name}")
    return body


def stage(manifest: Path, destination: Path) -> dict:
    with manifest.open("rb") as stream:
        data = stream.read(65537)
    if len(data) > 65536:
        raise ValueError("login artifact selection exceeds 64 KiB")
    selected = json.loads(data, object_pairs_hook=unique_object)
    if (not isinstance(selected, dict) or set(selected) != {"format", "artifacts"}
            or type(selected["format"]) is not int or selected["format"] != FORMAT
            or not isinstance(selected["artifacts"], dict)
            or set(selected["artifacts"]) != set(ARTIFACTS)):
        raise ValueError("expected the complete format-2 login artifact selection")
    payload = {
        target: (read_artifact(manifest, name, selected["artifacts"][name]), mode)
        for name, (target, mode) in ARTIFACTS.items()
    }
    for source, target in SUPPORT.items():
        body = (ROOT / source).read_bytes().replace(b"\r\n", b"\n")
        if source.endswith(".service.in"):
            placeholder = b"@AETHERI_LOGIN_STARTUP_CONTEXT@"
            if body.count(placeholder) != 1:
                raise ValueError("unexpected startup context template")
            body = body.replace(placeholder, b"system_u:system_r:aetheri_login_startup_t:s0")
        payload[target] = body, 0o444

    record = {"format": FORMAT, "files": [
        {"path": "/" + target, "mode": mode, "size": len(body), "sha256": digest(body)}
        for target, (body, mode) in sorted(payload.items())]}
    destination.mkdir()
    for target, (body, mode) in sorted(payload.items()):
        dest = destination / "root" / target
        dest.parent.mkdir(parents=True, exist_ok=True)
        with dest.open("xb") as stream:
            stream.write(body)
        os.chmod(dest, mode)
    with (destination / "receipt.json").open("x", encoding="utf-8", newline="\n") as stream:
        json.dump(record, stream, indent=2)
        stream.write("\n")
    return record


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        record = stage(args.inputs, args.output)
    except (OSError, ValueError) as error:
        print(f"login deployment: {error}", file=sys.stderr)
        return 1
    print(f"Staged {len(record['files'])} login files in {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
