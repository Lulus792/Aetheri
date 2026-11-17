"""Verify the enrolled greeter and local account lookup in an offline image."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import stat
import sys

from prepare_login_pam_runtime import NOLOGIN, SPEC, load
from verify_login_pam_root import file, link, named, offline_root


GREETER = b"aetheri-greeter"
HOME = "/var/empty"
SHELL = "/usr/sbin/nologin"
MAX_DATABASE = 1024 * 1024


def database(root: Path, name: str, fields: int, modes: tuple[int, ...]) -> list[list[bytes]]:
    path, info = named(root, "/etc/" + name)
    if (not stat.S_ISREG(info.st_mode) or stat.S_IMODE(info.st_mode) not in modes
            or info.st_nlink != 1 or not 0 < info.st_size <= MAX_DATABASE):
        raise ValueError(f"unexpected account database metadata: {name}")
    with path.open("rb") as stream:
        body = stream.read(info.st_size + 1)
    if len(body) != info.st_size or b"\0" in body or b"\r" in body or not body.endswith(b"\n"):
        raise ValueError(f"invalid account database: {name}")
    rows = [line.split(b":") for line in body.split(b"\n") if line and not line.startswith(b"#")]
    if any(len(row) != fields or not row[0] or row[0].startswith((b"+", b"-"))
           or row[0].split() != [row[0]] for row in rows):
        raise ValueError(f"invalid local records: {name}")
    return rows


def linux_id(value: bytes) -> int:
    if not 1 <= len(value) <= 10 or not value.isdigit() or int(value) >= 0xffffffff:
        raise ValueError("invalid local account ID")
    return int(value)


def account(rows: list[list[bytes]], name: str) -> list[bytes]:
    matches = [row for row in rows if row[0] == GREETER]
    if len(matches) != 1:
        raise ValueError(f"expected exactly one greeter record in {name}")
    return matches[0]


def lists_greeter(members: bytes) -> bool:
    return any(member.strip() == GREETER for member in members.split(b","))


def verify(root: Path, uid: int, gid: int) -> dict:
    offline_root(root)
    if any(not 0 < value < 0xffffffff or value == 65535 for value in (uid, gid)):
        raise ValueError("select valid nonzero greeter IDs")

    nss = (SPEC.parent / "data/login/nsswitch.conf").read_bytes().replace(b"\r\n", b"\n")
    file(root, "/etc/nsswitch.conf", {
        "mode": 0o444, "size": len(nss), "sha256": hashlib.sha256(nss).hexdigest(),
    })
    path, info = named(root, HOME)
    if not stat.S_ISDIR(info.st_mode) or stat.S_IMODE(info.st_mode) != 0o755 or any(path.iterdir()):
        raise ValueError("expected an empty Root-owned greeter home")
    link(root, "/usr/sbin", "bin")
    manifest, _ = load("login-pam-runtime-inputs.json")
    executable = next(entry for entry in manifest["files"] if entry["destination"] == NOLOGIN)
    file(root, NOLOGIN, executable)

    passwd = database(root, "passwd", 7, (0o444, 0o644))
    group = database(root, "group", 4, (0o444, 0o644))
    shadow = database(root, "shadow", 9, (0o000, 0o400, 0o600))
    gshadow = database(root, "gshadow", 4, (0o000, 0o400, 0o600))
    user = account(passwd, "passwd")
    primary = account(group, "group")
    protected_user = account(shadow, "shadow")
    protected_group = account(gshadow, "gshadow")
    if (user[1] != b"x" or linux_id(user[2]) != uid or linux_id(user[3]) != gid
            or user[5:] != [HOME.encode(), SHELL.encode()]):
        raise ValueError("greeter account differs from the selected identity")
    if primary[1] != b"x" or linux_id(primary[2]) != gid or primary[3]:
        raise ValueError("greeter group differs from the selected identity")
    if any(row is not user and (linux_id(row[2]) == uid or linux_id(row[3]) == gid) for row in passwd):
        raise ValueError("greeter UID or primary group is shared with another user")
    if any(row is not primary and linux_id(row[2]) == gid for row in group):
        raise ValueError("greeter GID is shared with another group")
    if any(lists_greeter(row[3]) for row in group):
        raise ValueError("greeter has a supplementary membership in group")
    if protected_user[1] != b"!*" or protected_user[7] != b"1":
        raise ValueError("greeter password or account is not fully locked")
    if protected_group[1:] != [b"!*", b"", b""]:
        raise ValueError("greeter group shadow record is not locked and empty")
    if any(lists_greeter(row[2]) or lists_greeter(row[3]) for row in gshadow):
        raise ValueError("greeter has a group shadow role")
    return {"format": 1, "account": GREETER.decode(), "uid": uid, "gid": gid, "groups": [gid]}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--greeter-uid", type=int, required=True)
    parser.add_argument("--greeter-gid", type=int, required=True)
    args = parser.parse_args()
    try:
        print(json.dumps(verify(args.root, args.greeter_uid, args.greeter_gid)))
    except (OSError, ValueError) as error:
        print(f"login account root: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
