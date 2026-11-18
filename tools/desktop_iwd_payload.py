"""Stage the signed iwd daemon and its runtime without enabling Wi-Fi."""

import argparse
import json
import os
from pathlib import Path
import stat
import subprocess
import sys
import tempfile

import desktop_payload
import desktop_wlan_payload
import prepare_audio


PROFILE = Path(__file__).resolve().parents[1] / "spec/desktop-iwd-inputs.json"
SIGNER = "36f612dcf27f7d1a48a835e4dbfcf71c6d9f90a6"
PACKAGES = {
    "iwd-3.12-1.fc44.x86_64.rpm": {"/usr/libexec/iwd", "/usr/share/licenses/iwd/COPYING"},
    "libell-0.83-1.fc44.x86_64.rpm": {"/usr/lib64/libell.so.0.0.2", "/usr/share/licenses/libell/COPYING"},
}
ALIAS = {"/usr/lib64/libell.so.0": "libell.so.0.0.2"}
MODULES = desktop_wlan_payload.MODULES
CRYPTO_MODULES = {f"{MODULES}/kernel/lib/crypto/libdes.ko.xz",
                  f"{MODULES}/kernel/crypto/des_generic.ko.xz"}
BUS_POLICY = "/usr/share/dbus-1/system.d/aetheri-iwd.conf"
BUS_RULES = b'''<!DOCTYPE busconfig PUBLIC "-//freedesktop//DTD D-BUS Bus Configuration 1.0//EN"
 "http://www.freedesktop.org/standards/dbus/1.0/busconfig.dtd">
<busconfig>
  <policy context="default">
    <deny own="net.connman.iwd"/>
    <deny send_destination="net.connman.iwd"/>
  </policy>
  <policy user="root">
    <allow own="net.connman.iwd"/>
    <allow send_destination="net.connman.iwd"/>
  </policy>
</busconfig>
'''


def unpack(package_path: Path, package: dict, scratch: Path) -> list[dict]:
    name = package_path.name
    with prepare_audio.sealed_package(package_path, package, description=name) as fd:
        archive_path = f"/proc/self/fd/{fd}"
        proof = subprocess.check_output(
            ["rpmkeys", "--checksig", "--verbose", archive_path], pass_fds=(fd,),
            text=True, env={**os.environ, "LC_ALL": "C"},
        ).lower()
        if f"{SIGNER}: ok" not in proof or "payload sha256 digest: ok" not in proof:
            raise ValueError(f"unverified iwd package: {name}")
        archive = subprocess.check_output(["rpm2cpio", archive_path], pass_fds=(fd,))

    wanted = {entry["destination"][1:]: entry for entry in package["files"]}
    files = []
    selected = wanted.keys() | {path[1:] for path in ALIAS if name.startswith("libell-")}
    missing = selected.copy()
    for member, mode, body in prepare_audio.cpio_members(archive, selected=selected):
        missing.remove(member)
        if member not in wanted:
            if not stat.S_ISLNK(mode) or bytes(body) != ALIAS["/" + member].encode("ascii"):
                raise ValueError(f"different iwd runtime alias: {member}")
            continue
        entry = wanted[member]
        if (not stat.S_ISREG(mode) or stat.S_IMODE(mode) != entry["mode"]
                or len(body) != entry["size"] or prepare_audio.sha(body) != entry["sha256"]):
            raise ValueError(f"different iwd archive member: {member}")
        target = scratch / member
        target.parent.mkdir(parents=True, exist_ok=True)
        with target.open("xb") as output:
            output.write(body)
        files.append({**entry, "package": name, "source": str(target)})
    if missing:
        raise ValueError(f"missing iwd archive members: {name}")
    return files


def install(tree: Path, kernel_root: Path, packages: Path) -> dict:
    if sys.platform != "linux" or os.getuid() == 0 or os.getuid() != os.geteuid() or os.getgid() != os.getegid():
        raise ValueError("use an ordinary Linux account to stage iwd")
    tree = tree.absolute()
    if tree == Path(tree.anchor) or tree.resolve(strict=True) != tree or not tree.is_dir():
        raise ValueError("select a canonical offline image tree")
    kernel_root = desktop_wlan_payload.canonical_directory(kernel_root)
    raw = PROFILE.read_bytes()
    profile = json.loads(raw)
    modules = profile["kernel_modules"]
    if (profile["format"] != 1 or profile["signer"] != SIGNER
            or profile["packages"].keys() != PACKAGES.keys()
            or profile["kernel_release"] != desktop_wlan_payload.KERNEL_RELEASE
            or len(modules) != len(CRYPTO_MODULES)
            or {entry["destination"] for entry in modules} != CRYPTO_MODULES):
        raise ValueError("unreviewed iwd profile")
    with tempfile.TemporaryDirectory(prefix="aetheri-iwd-") as directory:
        scratch = Path(directory)
        files = [desktop_wlan_payload.pinned_source(kernel_root / entry["destination"][1:], entry)
                 for entry in modules]
        for name, expected in PACKAGES.items():
            package = profile["packages"][name]
            if (len(package["files"]) != len(expected)
                    or {entry["destination"] for entry in package["files"]} != expected):
                raise ValueError(f"unreviewed iwd inventory: {name}")
            files.extend(unpack(packages / name, package, scratch))
        policy = scratch / "aetheri-iwd.conf"
        policy.write_bytes(BUS_RULES)
        files.append({"destination": BUS_POLICY, "source": str(policy), "provenance": "Aetheri",
                      "size": len(BUS_RULES), "sha256": prepare_audio.sha(BUS_RULES), "mode": 0o644})
        inventory = desktop_payload.install(tree, {"format": 1, "files": files, "symlinks": ALIAS})
    return {
        "profile_sha256": prepare_audio.sha(raw), "kernel_release": profile["kernel_release"],
        "inventory": inventory,
        "files": {entry["destination"]: entry["sha256"] for entry in files}, "symlinks": ALIAS,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--image-root", required=True, type=Path)
    parser.add_argument("--kernel-root", required=True, type=Path)
    parser.add_argument("--packages", required=True, type=Path)
    args = parser.parse_args()
    try:
        print(json.dumps(install(args.image_root, args.kernel_root, args.packages), indent=2))
    except (OSError, ValueError, KeyError, subprocess.CalledProcessError) as error:
        print(f"desktop iwd: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
