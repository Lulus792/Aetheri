"""Compile the installed login SELinux policy in a fresh offline image layer."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import stat
import subprocess
import sys


POLICY = Path(__file__).resolve().parents[1] / "policy"
MODULES = ("login-startup.cil", "login.cil", "login-graphical.cil", "login-apps.cil")
STORE = "var/lib/selinux/targeted/active"


def digest(path: Path) -> str:
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def inventory(root: Path) -> dict:
    if not stat.S_ISDIR(root.lstat().st_mode):
        raise ValueError(f"expected a regular policy directory: {root}")
    entries = {}
    for path in sorted(root.rglob("*")):
        info = path.lstat()
        if stat.S_ISDIR(info.st_mode):
            continue
        if not stat.S_ISREG(info.st_mode):
            raise ValueError(f"expected a regular policy file: {path}")
        entries[path.relative_to(root).as_posix()] = digest(path)
    return entries


def prepare(base: Path, output: Path, desktop_audio: bool = False,
            desktop_sound: bool = False) -> dict:
    if sys.platform != "linux":
        raise ValueError("run the offline policy compiler on Linux")
    if desktop_sound and not desktop_audio:
        raise ValueError("desktop Sound requires the desktop audio policy")
    base_config = base / "etc/selinux"
    base_store = base / STORE
    if any(output.resolve().is_relative_to(source.resolve()) for source in (base_config, base_store)):
        raise ValueError("select an output outside the source trees")
    configuration, modules = inventory(base_config), inventory(base_store)
    if not modules or "policy.kern" not in modules or "semanage.conf" not in configuration:
        raise ValueError("expected a complete targeted distribution policy selection")
    if any(name.split("/")[-2:-1] == ["aetheri_login"] for name in modules):
        raise ValueError("the baseline already contains the login policy module")

    names = (*MODULES, "login-audio.cil") if desktop_audio else MODULES
    if desktop_sound:
        names = (*names, "login-audio-control.cil")
    module_sources = {name: (POLICY / name).read_bytes().replace(b"\r\n", b"\n") for name in names}
    file_contexts = (POLICY / "login.fc").read_bytes().replace(b"\r\n", b"\n")
    if desktop_audio:
        file_contexts += b"\n" + (POLICY / "login-audio.fc").read_bytes().replace(b"\r\n", b"\n")
    if desktop_sound:
        file_contexts += b"\n" + (POLICY / "login-audio-control.fc").read_bytes().replace(b"\r\n", b"\n")
    output.mkdir()
    root = output / "root"
    target = root / "etc/selinux"
    store = root / STORE
    shutil.copytree(base_config, target)
    shutil.copytree(base_store, store)
    if inventory(target) != configuration or inventory(store) != modules:
        raise ValueError("selected inputs changed during staging")
    module = output / "aetheri_login.cil"
    module.write_bytes(b"".join(module_sources.values()))
    command = [
        "/usr/bin/semodule", "-s", "targeted", "-n", "-p", str(root),
        "-S", "/var/lib/selinux", "-g", str(target / "semanage.conf"),
        "-X", "400", "-i", str(module),
    ]
    run = subprocess.run(command, capture_output=True, timeout=90)
    (output / "compile.stderr").write_bytes(run.stderr)
    run.check_returncode()
    compiled = digest(store / "policy.kern")
    installed = [path for path in (target / "targeted/policy").iterdir()
                 if re.fullmatch(r"policy\.[0-9]+", path.name) and digest(path) == compiled]
    if len(installed) != 1 or compiled == modules["policy.kern"]:
        raise ValueError("offline store and installed policy did not produce one matching new policy")

    contexts = target / "targeted/contexts/files/file_contexts.local"
    if contexts.with_suffix(".local.bin").exists():
        raise ValueError("selected baseline has a compiled local context override")
    with contexts.open("ab") as stream:
        stream.write(b"\n" + file_contexts)
    (target / "config").write_text("SELINUX=enforcing\nSELINUXTYPE=targeted\n", encoding="ascii")
    # Retain the compiler store as evidence beside the installable layer.
    (root / "var/lib/selinux").rename(output / "store")
    (root / "var/lib").rmdir()
    (root / "var").rmdir()
    record = {
        "format": 1,
        "base_configuration": configuration,
        "base_store": modules,
        "module_sources": {name: hashlib.sha256(body).hexdigest() for name, body in module_sources.items()},
        "file_contexts": hashlib.sha256(file_contexts).hexdigest(),
        "compiled_policy": {"path": "/" + installed[0].relative_to(root).as_posix(), "sha256": compiled},
        "image_configuration": inventory(target),
    }
    with (output / "receipt.json").open("x", encoding="utf-8") as stream:
        json.dump(record, stream, indent=2)
        stream.write("\n")
    return record


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--desktop-audio", action="store_true")
    parser.add_argument("--desktop-sound", action="store_true")
    args = parser.parse_args()
    try:
        record = prepare(args.base, args.output.absolute(), args.desktop_audio, args.desktop_sound)
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        print(f"login policy: {error}", file=sys.stderr)
        return 1
    print(json.dumps(record["compiled_policy"]))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
