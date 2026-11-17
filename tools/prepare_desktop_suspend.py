"""Extract the pinned systemd suspend files into a fresh offline staging tree."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import stat
import subprocess
import sys

import desktop_payload
import prepare_audio


SPEC = prepare_audio.ROOT / "spec"
PROFILE = SPEC / "desktop-suspend-inputs.json"
PACKAGE = "systemd-udev-259.10-1.fc44.x86_64"
SIGNER = "36f612dcf27f7d1a48a835e4dbfcf71c6d9f90a6"
BASE_FILES = (
    "/usr/lib/systemd/systemd",
    "/usr/lib/systemd/systemd-logind",
    "/usr/lib64/systemd/libsystemd-shared-259.10-1.fc44.so",
    "/usr/share/licenses/systemd/LICENSE.LGPL2.1",
)
PAYLOAD = {
    "/usr/lib/systemd/system/sleep.target": (0o644, "power_unit_file_t"),
    "/usr/lib/systemd/system/suspend.target": (0o644, "power_unit_file_t"),
    "/usr/lib/systemd/system/systemd-suspend.service": (0o644, "power_unit_file_t"),
    "/usr/lib/systemd/systemd-sleep": (0o755, "systemd_sleep_exec_t"),
}


def profile():
    raw = PROFILE.read_bytes()
    pins = json.loads(raw)
    rpm_path = SPEC / "audio-udev-inputs.json"
    rpm = json.loads(rpm_path.read_bytes())["rpm"]
    rows = pins["files"]
    if (pins["format"] != 1 or pins["rpm_profile"] != rpm_path.name
            or pins["package"] != PACKAGE or rpm["name"] != PACKAGE + ".rpm"
            or rpm["signer"] != SIGNER or len(rows) != len(PAYLOAD)
            or {row["destination"] for row in rows} != set(PAYLOAD)):
        raise ValueError("unreviewed suspend profile")
    for row in rows:
        mode, role = PAYLOAD[row["destination"]]
        if (row["mode"] != mode or row["selinux"] != f"system_u:object_r:{role}:s0"
                or type(row["size"]) is not int or not 0 < row["size"] <= 64 * 1024):
            raise ValueError("unreviewed suspend file metadata")
    return pins, rpm, prepare_audio.sha(raw)


def verify_base(tree):
    """Check the existing systemd ABI and the license in the offline image."""
    tree = tree.absolute()
    if tree == Path(tree.anchor) or tree.resolve(strict=True) != tree or not tree.is_dir():
        raise ValueError("select a real offline image tree")
    boot = json.loads((SPEC / "boot-probe-inputs.json").read_bytes())
    desktop = json.loads((SPEC / "desktop-image-inputs.json").read_bytes())
    selected = {row["canonical"]: row for row in boot["files"]}
    selected.update({row["destination"]: row for row in desktop["files"]})
    checked = {}
    for name in BASE_FILES:
        row = selected[name]
        path = tree / name.lstrip("/")
        if (path.resolve(strict=True) != path or not stat.S_ISREG(path.lstat().st_mode)
                or path.stat().st_size != row["size"]):
            raise ValueError(f"different suspend base file: {name}")
        with path.open("rb") as stream:
            digest = hashlib.file_digest(stream, "sha256").hexdigest()
        if digest != row["sha256"]:
            raise ValueError(f"different suspend base bytes: {name}")
        checked[name] = digest
    return checked


def prepare(rpm_source, output, base_root=None):
    if (sys.platform != "linux" or os.getuid() == 0 or os.getuid() != os.geteuid()
            or os.getgid() != os.getegid()):
        raise ValueError("use an ordinary Linux build account")
    pins, rpm, profile_digest = profile()
    output = output.absolute()
    if output.resolve() != output:
        raise ValueError("aliased suspend staging tree")
    base_files = verify_base(base_root) if base_root is not None else None
    with prepare_audio.sealed_package(rpm_source, rpm) as fd:
        archive_path = f"/proc/self/fd/{fd}"
        proof = subprocess.check_output(
            ["rpmkeys", "--checksig", "--verbose", archive_path], pass_fds=(fd,), text=True,
            env={**os.environ, "LC_ALL": "C"},
        ).lower()
        if f"{SIGNER}: ok" not in proof or "payload sha256 digest: ok" not in proof:
            raise ValueError("unverified suspend RPM")
        archive = subprocess.check_output(["rpm2cpio", archive_path], pass_fds=(fd,))

    wanted = {row["destination"].lstrip("/"): row for row in pins["files"]}
    members = {}
    for name, mode, data in prepare_audio.cpio_members(archive, selected=wanted):
        row = wanted[name]
        if (not stat.S_ISREG(mode) or stat.S_IMODE(mode) != row["mode"]
                or len(data) != row["size"] or prepare_audio.sha(data) != row["sha256"]):
            raise ValueError(f"different suspend archive member: {name}")
        members[name] = data
    if members.keys() != wanted.keys():
        raise ValueError("missing suspend archive members")

    output.mkdir(mode=0o700)
    tree = output / "root"
    tree.mkdir()
    for name, row in wanted.items():
        desktop_payload.parents(tree, desktop_payload.destination(row["destination"]))
        path = tree / name
        with path.open("xb") as stream:
            stream.write(members[name])
        path.chmod(row["mode"])
    receipt = {
        "format": 1, "profile_sha256": profile_digest,
        "rpm": rpm, "source_package": pins["source_package"], "license": pins["license"],
        "files": [{**row, "uid": 0, "gid": 0} for row in pins["files"]],
    }
    if base_files is not None:
        receipt["base_files"] = base_files
    with (output / "inputs.json").open("x", encoding="utf-8") as stream:
        stream.write(json.dumps(receipt, indent=2) + "\n")
    return receipt


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rpm", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--base-root", type=Path, help="also verify the selected offline image base")
    args = parser.parse_args()
    try:
        print(json.dumps(prepare(args.rpm, args.output, args.base_root), indent=2))
    except (OSError, ValueError, KeyError, subprocess.CalledProcessError) as error:
        print(f"desktop suspend: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
