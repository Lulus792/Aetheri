"""Encode the selected login deployment for offline release signing."""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import stat
import struct
import sys

from prepare_login_deployment import (
    DATA, DESKTOP_EXECUTABLES, FORMAT, LIBEXEC, MAX_INPUT, UNITS, digest, unique_object,
    LOCK_EXECUTABLE, LOCK_FORMAT, LOCK_PAM, SOUND_EXECUTABLE, SOUND_FORMAT,
)
from verify_login_accounts import verify as verify_accounts


# Wire order matches enum ae_login_selection_role. Only these fixed image
# paths may contribute a record, regardless of which files the receipt lists.
ROLES = (
    (f"/{UNITS}/aetheri-login.service", 65536),
    (f"/{UNITS}/aetheri-login-startup.service", 65536),
    (None, 65536),  # reviewed distribution service.d drop-in
    (f"/{UNITS}/aetheri-login.service.d/20-session-shutdown.conf", 65536),
    (f"/{LIBEXEC}/aetheri-login-backend", MAX_INPUT),
    (f"/{DATA}/greetd.toml", 65536),
    (f"/{LIBEXEC}/aetheri-login-greeter", MAX_INPUT),
    (f"/{LIBEXEC}/aetheri-login-display", MAX_INPUT),
    (f"/{DATA}/greeter.ttf", MAX_INPUT),
    ("/etc/pam.d/aetheri-login", 65536),
    ("/etc/pam.d/aetheri-greeter", 65536),
    (f"/{LIBEXEC}/aetheri-session", MAX_INPUT),
    (f"/{LIBEXEC}/aetheri-compositor", MAX_INPUT),
    (f"/{LIBEXEC}/aetheri-desktop", MAX_INPUT),
    (f"/{LIBEXEC}/aetheri-login-startup", MAX_INPUT),
    (None, MAX_INPUT),  # independently selected effective kernel policy bytes
    *((f"/{LIBEXEC}/{name}", MAX_INPUT) for name in DESKTOP_EXECUTABLES),
)


def read_regular(path: Path, limit: int) -> bytes:
    info = path.lstat()
    if not stat.S_ISREG(info.st_mode) or not 0 < info.st_size <= limit:
        raise ValueError(f"expected a bounded regular file: {path}")
    with path.open("rb") as stream:
        body = stream.read(info.st_size + 1)
    if len(body) != info.st_size:
        raise ValueError(f"file length changed: {path}")
    return body


def payload(stage: Path, dropin: Path, policy: Path, release: str, uid: int,
            gid: int, groups: list[int], account_root: Path | None = None) -> bytes:
    if not re.fullmatch("[0-9a-f]{64}", release) or not any(bytes.fromhex(release)):
        raise ValueError("deployment release must be a nonzero 32-byte lowercase hex value")
    if any(type(value) is not int or not 0 < value < 0xffffffff for value in (uid, gid, *groups)):
        raise ValueError("account and supplementary group IDs must be nonzero Linux IDs")
    if len(groups) > 16 or sorted(set(groups)) != groups:
        raise ValueError("expected up to 16 strictly increasing supplementary groups")
    if account_root is not None and verify_accounts(account_root, uid, gid)["groups"] != groups:
        raise ValueError("selected groups differ from the enrolled greeter")
    selection = json.loads(read_regular(stage / "receipt.json", 65536), object_pairs_hook=unique_object)
    if (not isinstance(selection, dict) or set(selection) != {"format", "files"}
            or type(selection["format"]) is not int
            or selection["format"] not in (FORMAT, SOUND_FORMAT, LOCK_FORMAT)
            or not isinstance(selection["files"], list)):
        raise ValueError("expected a format-2, format-3 or format-4 login staging receipt")
    files = {}
    for entry in selection["files"]:
        if (not isinstance(entry, dict) or set(entry) != {"path", "mode", "size", "sha256"}
                or not isinstance(entry["path"], str) or entry["path"] in files):
            raise ValueError("invalid or duplicate login receipt file")
        files[entry["path"]] = entry

    version = selection["format"]
    sound_path = f"/{LIBEXEC}/{SOUND_EXECUTABLE}"
    roles = (*ROLES, (sound_path, MAX_INPUT)) if version != FORMAT else ROLES
    if version == LOCK_FORMAT:
        roles = (*roles, (f"/{LIBEXEC}/{LOCK_EXECUTABLE}", MAX_INPUT), (f"/{LOCK_PAM}", 65536))
    body = bytearray(f"AETHLS{version:02d}".encode("ascii") + bytes.fromhex(release))
    body.extend(struct.pack(">IIB2x16I", uid, gid, len(groups), *groups, *([0] * (16 - len(groups)))))
    external = iter((dropin, policy))
    for name, limit in roles:
        if version == LOCK_FORMAT and name == sound_path and name not in files:
            try:
                (stage / "root" / sound_path[1:]).lstat()
            except FileNotFoundError:
                body.extend(bytes(36))
                continue
            raise ValueError(f"staged sound executable has no reviewed receipt: {name}")
        path = next(external) if name is None else stage / "root" / name[1:]
        value = read_regular(path, limit)
        hashed = digest(value)
        if name is not None:
            entry = files.get(name)
            mode = 0o555 if name.startswith(f"/{LIBEXEC}/") else 0o444
            if (entry is None or entry["mode"] != mode or entry["size"] != len(value)
                    or entry["sha256"] != hashed):
                raise ValueError(f"staged file differs from reviewed receipt: {name}")
        body.extend(struct.pack(">I32s", len(value), bytes.fromhex(hashed)))
    return bytes(body)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--stage", type=Path, required=True)
    parser.add_argument("--systemd-dropin", type=Path, required=True)
    parser.add_argument("--effective-policy", type=Path, required=True)
    parser.add_argument("--release", required=True)
    parser.add_argument("--greeter-uid", type=int, required=True)
    parser.add_argument("--greeter-gid", type=int, required=True)
    parser.add_argument("--group", action="append", type=int, default=[])
    parser.add_argument("--account-root", type=Path, help="verify this assembled image's enrolled greeter")
    parser.add_argument("--signature", type=Path, help="append the offline signer’s raw Ed25519 signature")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        body = payload(args.stage, args.systemd_dropin, args.effective_policy,
                       args.release, args.greeter_uid, args.greeter_gid, args.group, args.account_root)
        if args.signature is not None:
            signature = read_regular(args.signature, 64)
            if len(signature) != 64:
                raise ValueError("expected a 64-byte raw Ed25519 signature")
            body += signature
        with args.output.open("xb") as stream:
            stream.write(body)
        if args.signature is not None:
            args.output.chmod(0o444)
    except (OSError, ValueError) as error:
        print(f"login selection: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
