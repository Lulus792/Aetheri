"""Stage the packaged login's additional PAM runtime in a fresh Linux tree."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys

import desktop_payload


SPEC = Path(__file__).resolve().parents[1] / "spec"
HELPER = "/usr/bin/unix_chkpwd"
NOLOGIN = "/usr/bin/nologin"
PROGRAMS = {HELPER, NOLOGIN}
INTERPRETER = "/lib64/ld-linux-x86-64.so.2"
BASE_MODULES = {"/usr/lib64/security/" + name + ".so"
                for name in ("pam_deny", "pam_permit", "pam_systemd")}


def load(name: str) -> tuple[dict, str]:
    body = (SPEC / name).read_bytes()
    return json.loads(body), hashlib.sha256(body).hexdigest()


def names(inventory: dict) -> set[str]:
    return {entry["destination"] for entry in inventory["files"]} | inventory["symlinks"].keys()


def elf_references(path: Path) -> tuple[list[str], str | None]:
    output = subprocess.run(
        ["readelf", "-dW", "-lW", str(path)], check=True, capture_output=True, text=True,
        env={**os.environ, "LC_ALL": "C"}, timeout=10,
    ).stdout
    needed = re.findall(r"\(NEEDED\).*Shared library: \[([^\]]+)\]", output)
    if any(not re.fullmatch(r"[A-Za-z0-9_.+-]+", name) or name in (".", "..")
           for name in needed):
        raise ValueError(f"unexpected ELF dependency in {path}")
    interpreter = re.search(r"\[Requesting program interpreter: ([^\]]+)\]", output)
    return ["/usr/lib64/" + name for name in needed], interpreter[1] if interpreter else None


def stage(output: Path) -> dict:
    if sys.platform != "linux" or not os.getuid() or os.getuid() != os.geteuid():
        raise ValueError("run as an ordinary Linux build user")
    supplement, supplement_sha = load("login-pam-runtime-inputs.json")
    desktop, desktop_sha = load("desktop-image-inputs.json")
    boot, boot_sha = load("boot-probe-inputs.json")
    desktop_payload.validate(supplement)
    desktop_payload.validate(desktop)
    additions, base = names(supplement), names(desktop)
    if additions & base or not BASE_MODULES <= base:
        raise ValueError("PAM supplement conflicts with or lacks the selected desktop base")
    if not PROGRAMS <= additions:
        raise ValueError("missing a required login program")
    if not any(entry["source"] == INTERPRETER and entry["canonical"] == "/usr" + INTERPRETER
               for entry in boot["files"]):
        raise ValueError("missing password helper interpreter in the boot inventory")
    if supplement["image_metadata"] != {
        HELPER: {"mode": 0o4755, "uid": 0, "gid": 0, "selinux": "system_u:object_r:chkpwd_exec_t:s0"}
    } or not any(entry["destination"] == HELPER and entry["mode"] == 0o755
                 for entry in supplement["files"]):
        raise ValueError("invalid password helper metadata")

    output.mkdir()
    root = output / "root"
    root.mkdir()
    copied = desktop_payload.install(root, supplement)
    references = {}
    required_base = BASE_MODULES.copy()
    for entry in supplement["files"]:
        name = entry["destination"]
        if entry["mode"] != 0o755:
            continue
        needed, interpreter = elf_references(root / name.lstrip("/"))
        missing = set(needed) - additions - base
        if missing:
            raise ValueError(f"unpinned dependencies for {name}: {', '.join(sorted(missing))}")
        if interpreter != (INTERPRETER if name in PROGRAMS else None):
            raise ValueError(f"unexpected interpreter for {name}")
        required_base.update(set(needed) & base)
        references[name] = {"needed": needed, "interpreter": interpreter}
    receipt = {
        "format": 1,
        "supplement_sha256": supplement_sha,
        "desktop_sha256": desktop_sha,
        "boot_sha256": boot_sha,
        "copied": copied,
        "elf_references": references,
        "required_desktop_paths": sorted(required_base),
        "required_boot_path": INTERPRETER,
        "image_metadata": supplement["image_metadata"],
    }
    with (output / "receipt.json").open("x", encoding="utf-8") as stream:
        json.dump(receipt, stream, indent=2)
        stream.write("\n")
    return receipt


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        stage(args.output)
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        print(f"login PAM runtime: {error}", file=sys.stderr)
        return 1
    print(f"Staged the login PAM supplement in {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
