"""Verify PAM files and their ELF dependencies in an assembled Linux root."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import posixpath
import re
import stat
import subprocess
import sys

import desktop_payload
from prepare_login_pam_runtime import HELPER, INTERPRETER, PROGRAMS, SPEC, elf_references, load


PAM_DIR = "/etc/pam.d/"
PROFILES = ("aetheri-login", "aetheri-greeter", "other")
CONFIG_CONTEXT = "system_u:object_r:aetheri_login_config_t:s0"
MODULE = re.compile(
    r"(?:auth|account|password|session)\s+(?:required|requisite|sufficient|optional)"
    r"\s+(pam_[a-z0-9_]+\.so)(?:\s+.*)?"
)


def named(root: Path, name: str) -> tuple[Path, os.stat_result]:
    path = PurePosixPath(name)
    current = root
    for part in path.parts[1:-1]:
        current /= part
        directory = current.lstat()
        if (not stat.S_ISDIR(directory.st_mode) or directory.st_uid or directory.st_gid
                or directory.st_mode & 0o022):
            raise ValueError(f"unprotected parent of {name}: {current}")
    leaf = current / path.name
    info = leaf.lstat()
    if info.st_uid or info.st_gid:
        raise ValueError(f"unexpected ownership: {name}")
    return leaf, info


def link(root: Path, name: str, target: str) -> None:
    path, info = named(root, name)
    if not stat.S_ISLNK(info.st_mode) or os.readlink(path) != target:
        raise ValueError(f"unexpected link: {name}")


def file(root: Path, name: str, entry: dict, context: str | None = None) -> Path:
    path, info = named(root, name)
    if (not stat.S_ISREG(info.st_mode) or stat.S_IMODE(info.st_mode) != entry["mode"]
            or info.st_nlink != 1 or info.st_size != entry["size"]):
        raise ValueError(f"unexpected file metadata: {name}")
    with path.open("rb") as stream:
        if hashlib.file_digest(stream, "sha256").hexdigest() != entry["sha256"]:
            raise ValueError(f"unexpected file digest: {name}")
    if context and os.getxattr(path, "security.selinux").rstrip(b"\0") != context.encode():
        raise ValueError(f"unexpected SELinux context: {name}")
    return path


def offline_root(root: Path) -> None:
    if sys.platform != "linux":
        raise ValueError("inspect the assembled root on Linux")
    if root.resolve() == Path("/"):
        raise ValueError("select an offline image root")
    root_info = root.lstat()
    if not stat.S_ISDIR(root_info.st_mode) or root_info.st_uid or root_info.st_gid or root_info.st_mode & 0o022:
        raise ValueError("expected a protected root directory")


def verify(root: Path) -> dict:
    offline_root(root)
    supplement, supplement_sha = load("login-pam-runtime-inputs.json")
    desktop, desktop_sha = load("desktop-image-inputs.json")
    boot, boot_sha = load("boot-probe-inputs.json")
    desktop_payload.validate(supplement)
    desktop_payload.validate(desktop)
    files = {entry["destination"]: entry for manifest in (desktop, supplement)
             for entry in manifest["files"]}
    links = desktop["symlinks"] | supplement["symlinks"]
    loader = next(entry for entry in boot["files"] if entry["source"] == INTERPRETER)
    loader_name = "/usr" + INTERPRETER
    if loader["canonical"] != loader_name:
        raise ValueError("unexpected loader in boot inventory")
    files[loader_name] = loader | {"mode": 0o755}
    link(root, "/lib64", "usr/lib64")
    link(root, "/sbin", "usr/bin")
    link(root, "/usr/sbin", "bin")

    pending = {loader_name}
    for profile in PROFILES:
        body = (SPEC.parent / "data/pam.d" / profile).read_bytes().replace(b"\r\n", b"\n")
        entry = {"mode": 0o444, "size": len(body), "sha256": hashlib.sha256(body).hexdigest()}
        file(root, PAM_DIR + profile, entry, CONFIG_CONTEXT)
        for raw in body.decode("utf-8").splitlines():
            line = raw.partition("#")[0].strip()
            if line:
                module = MODULE.fullmatch(line)
                if module is None:
                    raise ValueError(f"unsupported PAM module reference in {profile}: {line}")
                pending.add("/usr/lib64/security/" + module[1])

    metadata = supplement["image_metadata"][HELPER]
    files[HELPER] = files[HELPER] | {"mode": metadata["mode"]}
    pending.update(entry["destination"] for entry in supplement["files"] if entry["mode"] == 0o755)
    checked, checked_links = {}, {}
    while pending:
        name = min(pending)
        pending.remove(name)
        if name in checked or name in checked_links:
            continue
        if name in links:
            target = links[name]
            link(root, name, target)
            checked_links[name] = target
            pending.add(posixpath.normpath(posixpath.join(posixpath.dirname(name), target)))
            continue
        if name not in files:
            raise ValueError(f"ELF dependency is absent from the image inventory: {name}")
        path = file(root, name, files[name], metadata["selinux"] if name == HELPER else None)
        needed, interpreter = elf_references(path)
        if interpreter not in (None, INTERPRETER) or (name in PROGRAMS and interpreter is None):
            raise ValueError(f"unexpected interpreter: {name}")
        checked[name] = {"sha256": files[name]["sha256"], "needed": needed, "interpreter": interpreter}
        pending.update(needed)
    return {
        "format": 1,
        "supplement_sha256": supplement_sha,
        "desktop_sha256": desktop_sha,
        "boot_sha256": boot_sha,
        "profiles": list(PROFILES),
        "elf_objects": checked,
        "aliases": checked_links,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, required=True)
    args = parser.parse_args()
    try:
        print(json.dumps(verify(args.root), indent=2))
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        print(f"login PAM root: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
