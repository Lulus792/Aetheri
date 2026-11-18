"""Stage the pinned HP WLAN driver, firmware and regulatory data offline."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import stat
import subprocess
import sys
import tempfile

import desktop_payload
import prepare_audio


PROFILE = Path(__file__).resolve().parents[1] / "spec/desktop-wlan-inputs.json"
KERNEL_RELEASE = "6.19.10-300.fc44.x86_64"
MODULES = f"/usr/lib/modules/{KERNEL_RELEASE}"
PCI_DEVICE = "8086:24fd"
REGDB_SIGNER = "36f612dcf27f7d1a48a835e4dbfcf71c6d9f90a6"
FIRMWARE_FILES = {
    "/usr/lib/firmware/iwlwifi-8265-36.ucode", "/usr/lib/firmware/iwlwifi-8000C-36.ucode",
    "/usr/share/licenses/linux-firmware/LICENCE.iwlwifi_firmware",
}
REGDB_FILES = {
    "/usr/lib/firmware/regulatory.db", "/usr/lib/firmware/regulatory.db.p7s",
    "/usr/share/licenses/wireless-regdb/LICENSE",
}
RULE_PATH = "/usr/lib/udev/rules.d/80-aetheri-hp-wlan.rules"
RULE = (b'ACTION=="add", SUBSYSTEM=="pci", ATTR{vendor}=="0x8086", '
        b'ATTR{device}=="0x24fd", DRIVER=="", ENV{MODALIAS}=="?*", '
        b'RUN{builtin}+="kmod load"\n')


def canonical_directory(path: Path) -> Path:
    path = path.absolute()
    if path.resolve(strict=True) != path or not path.is_dir():
        raise ValueError(f"expected a canonical directory: {path}")
    return path


def digest(path: Path) -> str:
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def pinned_source(path: Path, entry: dict) -> dict:
    info = path.lstat()
    if (path.resolve(strict=True) != path or not stat.S_ISREG(info.st_mode)
            or info.st_size != entry["size"] or digest(path) != entry["sha256"]):
        raise ValueError(f"different WLAN input: {path}")
    return {**entry, "source": str(path)}


def regulatory_files(package_path: Path, package: dict, files: list[dict]) -> dict[str, bytes]:
    if sys.platform != "linux" or os.getuid() == 0 or os.getuid() != os.geteuid() or os.getgid() != os.getegid():
        raise ValueError("use an ordinary Linux account to stage WLAN")
    with prepare_audio.sealed_package(package_path, package, description="wireless-regdb RPM") as fd:
        archive_path = f"/proc/self/fd/{fd}"
        proof = subprocess.check_output(
            ["rpmkeys", "--checksig", "--verbose", archive_path], pass_fds=(fd,),
            text=True, env={**os.environ, "LC_ALL": "C"},
        ).lower()
        if f"{REGDB_SIGNER}: ok" not in proof or "payload sha256 digest: ok" not in proof:
            raise ValueError("unverified wireless-regdb RPM")
        archive = subprocess.check_output(["rpm2cpio", archive_path], pass_fds=(fd,))
    wanted = {entry["destination"][1:]: entry for entry in files}
    selected = {}
    for name, mode, body in prepare_audio.cpio_members(archive, selected=wanted):
        entry = wanted[name]
        if (not stat.S_ISREG(mode) or stat.S_IMODE(mode) != entry["mode"]
                or len(body) != entry["size"] or prepare_audio.sha(body) != entry["sha256"]):
            raise ValueError(f"different wireless-regdb archive member: {name}")
        selected["/" + name] = bytes(body)
    if len(selected) != len(files):
        raise ValueError("missing wireless-regdb archive member")
    return selected


def install(tree: Path, kernel_root: Path, firmware_dir: Path, regdb_rpm: Path) -> dict:
    tree = canonical_directory(tree)
    if tree == Path(tree.anchor):
        raise ValueError("select an offline image tree")
    modules = canonical_directory(tree / MODULES[1:])
    for name in ("modules.order", "modules.builtin", "modules.builtin.modinfo"):
        if not stat.S_ISREG((modules / name).lstat().st_mode):
            raise ValueError("image is missing the selected kernel module inventory")
    kernel_root = canonical_directory(kernel_root)
    firmware_dir = canonical_directory(firmware_dir)
    raw = PROFILE.read_bytes()
    profile = json.loads(raw)
    if (profile["format"] != 1 or profile["kernel_release"] != KERNEL_RELEASE
            or profile["pci_device"] != PCI_DEVICE or profile["regdb_rpm"]["signer"] != REGDB_SIGNER
            or {entry["destination"] for entry in profile["firmware"]} != FIRMWARE_FILES
            or {entry["destination"] for entry in profile["regdb_files"]} != REGDB_FILES):
        raise ValueError("unreviewed desktop WLAN profile")

    files = []
    for entry in profile["kernel_modules"]:
        name = desktop_payload.destination(entry["destination"])
        if not entry["destination"].startswith(f"{MODULES}/kernel/") or not name.name.endswith(".ko.xz"):
            raise ValueError("unexpected WLAN module destination")
        files.append(pinned_source(kernel_root / str(name)[1:], entry))
    for entry in profile["firmware"]:
        name = desktop_payload.destination(entry["destination"])
        files.append(pinned_source(firmware_dir / name.name, entry))

    bodies = regulatory_files(regdb_rpm, profile["regdb_rpm"], profile["regdb_files"])
    with tempfile.TemporaryDirectory(prefix="aetheri-wlan-") as directory:
        inputs = Path(directory)
        for entry in profile["regdb_files"]:
            name = entry["destination"]
            path = inputs / desktop_payload.destination(name).name
            with path.open("xb") as stream:
                stream.write(bodies[name])
            files.append({**entry, "source": str(path)})
        path = inputs / Path(RULE_PATH).name
        path.write_bytes(RULE)
        files.append({"destination": RULE_PATH, "source": str(path), "provenance": "Aetheri",
                      "size": len(RULE), "sha256": prepare_audio.sha(RULE), "mode": 0o644})
        installed = desktop_payload.install(tree, {"format": 1, "files": files, "symlinks": {}})
    return {"kernel_release": KERNEL_RELEASE, "pci_device": PCI_DEVICE, "inventory": installed,
            "profile_sha256": prepare_audio.sha(raw),
            "files": {entry["destination"]: entry["sha256"] for entry in files}}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--image-root", required=True, type=Path)
    parser.add_argument("--kernel-root", required=True, type=Path)
    parser.add_argument("--firmware-dir", required=True, type=Path)
    parser.add_argument("--regdb-rpm", required=True, type=Path)
    args = parser.parse_args()
    try:
        print(json.dumps(install(args.image_root, args.kernel_root, args.firmware_dir, args.regdb_rpm), indent=2))
    except (OSError, ValueError, KeyError, subprocess.CalledProcessError) as error:
        print(f"desktop WLAN: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
