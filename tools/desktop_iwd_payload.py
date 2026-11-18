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


ROOT = Path(__file__).resolve().parents[1]
PROFILE = ROOT / "spec/desktop-iwd-inputs.json"
RESOLVER_PROFILE = ROOT / "spec/desktop-resolver-inputs.json"
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
SETUP_FILES = {
    "/usr/lib/systemd/system/aetheri-wlan.service": ROOT / "data/systemd/aetheri-wlan.service",
    "/usr/lib/udev/rules.d/90-aetheri-wlan-ipv6.rules": ROOT / "data/wlan/90-aetheri-wlan-ipv6.rules",
    "/usr/share/aetheri/wlan/main.conf": ROOT / "data/wlan/main.conf",
}
RESOLVER_PACKAGES = {
    "systemd-resolved-259.10-1.fc44.x86_64.rpm": {
        "/usr/lib/systemd/systemd-resolved",
        "/usr/lib/systemd/system/systemd-resolved.service",
    },
    "openssl-libs-3.5.9-1.fc44.x86_64.rpm": {
        "/usr/lib64/libssl.so.3.5.9",
        "/usr/share/licenses/openssl-libs/LICENSE.txt",
    },
}
RESOLVER_ALIAS = {"/usr/lib64/libssl.so.3": "libssl.so.3.5.9"}
RESOLVER_SERVICE = "/usr/lib/systemd/system/systemd-resolved.service"
RESOLVER_SOCKET_UNITS = b"systemd-resolved-varlink.socket systemd-resolved-monitor.socket"
RESOLVER_FILES = {
    "/usr/share/aetheri/wlan/main.conf": ROOT / "data/wlan/resolved-main.conf",
    "/usr/lib/systemd/system/aetheri-wlan.service.d/50-resolver.conf":
        ROOT / "data/systemd/aetheri-wlan-resolver.conf",
    "/usr/lib/systemd/system/systemd-resolved.service.d/50-aetheri.conf":
        ROOT / "data/systemd/aetheri-resolver-service.conf",
    "/usr/lib/systemd/resolved.conf.d/50-aetheri.conf": ROOT / "data/systemd/aetheri-resolved.conf",
    "/usr/share/dbus-1/system.d/org.freedesktop.resolve1-aetheri.conf":
        ROOT / "policy/org.freedesktop.resolve1-aetheri.conf",
}
RESOLV_TARGET = "../run/systemd/resolve/stub-resolv.conf"
RESOLVER_ACCOUNTS = {
    "passwd": (b"systemd-resolve:x:193:193:systemd Resolver:/:/usr/sbin/nologin", 7, (2, 3)),
    "group": (b"systemd-resolve:x:193:", 4, (2,)),
    "shadow": (b"systemd-resolve:!*::::::1:", 9, ()),
    "gshadow": (b"systemd-resolve:!*::", 4, ()),
}
CONTROL_FILES = {
    "/usr/lib/systemd/system/aetheri-wlan-control.service": ROOT / "data/systemd/aetheri-wlan-control.service",
    "/usr/share/dbus-1/system-services/org.aetheri.Wlan1.service": ROOT / "data/dbus-1/org.aetheri.Wlan1.service",
    "/usr/share/dbus-1/system.d/org.aetheri.Wlan1.conf": ROOT / "policy/org.aetheri.Wlan1.conf",
}
CONTROL_ELF = "/usr/libexec/aetheri/aetheri-wlan-control"
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


def unpack(package_path: Path, package: dict, scratch: Path, aliases: dict[str, str]) -> list[dict]:
    name = package_path.name
    with prepare_audio.sealed_package(package_path, package, description=name) as fd:
        archive_path = f"/proc/self/fd/{fd}"
        proof = subprocess.check_output(
            ["rpmkeys", "--checksig", "--verbose", archive_path], pass_fds=(fd,),
            text=True, env={**os.environ, "LC_ALL": "C"},
        ).lower()
        if f"{SIGNER}: ok" not in proof or "payload sha256 digest: ok" not in proof:
            raise ValueError(f"unverified WLAN package: {name}")
        archive = subprocess.check_output(["rpm2cpio", archive_path], pass_fds=(fd,))

    wanted = {entry["destination"][1:]: entry for entry in package["files"]}
    files = []
    selected = wanted.keys() | {path[1:] for path in aliases}
    missing = selected.copy()
    for member, mode, body in prepare_audio.cpio_members(archive, selected=selected):
        missing.remove(member)
        if member not in wanted:
            if not stat.S_ISLNK(mode) or bytes(body) != aliases["/" + member].encode("ascii"):
                raise ValueError(f"different WLAN runtime alias: {member}")
            continue
        entry = wanted[member]
        if (not stat.S_ISREG(mode) or stat.S_IMODE(mode) != entry["mode"]
                or len(body) != entry["size"] or prepare_audio.sha(body) != entry["sha256"]):
            raise ValueError(f"different WLAN archive member: {member}")
        target = scratch / member
        target.parent.mkdir(parents=True, exist_ok=True)
        with target.open("xb") as output:
            output.write(body)
        files.append({**entry, "package": name, "source": str(target)})
    if missing:
        raise ValueError(f"missing WLAN archive members: {name}")
    return files


def resolver_accounts(tree: Path) -> list[tuple[Path, bytes, bool]]:
    """Preflight local accounts in the builder-owned image before any writes."""
    etc = tree / "etc"
    if not stat.S_ISDIR(etc.lstat().st_mode):
        raise ValueError("expected a real offline account directory")
    updates = []
    for name, (record, fields, ids) in RESOLVER_ACCOUNTS.items():
        path = etc / name
        try:
            info = path.lstat()
        except FileNotFoundError:
            if ids:
                raise ValueError(f"missing offline account database: {name}") from None
            updates.append((path, record, True))
            continue
        if not stat.S_ISREG(info.st_mode) or info.st_nlink != 1 or info.st_size > 1024**2:
            raise ValueError(f"invalid offline account database: {name}")
        raw = path.read_bytes()
        if (raw and not raw.endswith(b"\n")) or b"\0" in raw or b"\r" in raw:
            raise ValueError(f"invalid offline account records: {name}")
        selected = []
        for line in raw.splitlines():
            if not line or line.startswith(b"#"):
                continue
            row = line.split(b":")
            if len(row) != fields:
                raise ValueError(f"invalid offline account fields: {name}")
            if row[0] == b"systemd-resolve" or any(row[i].lstrip(b"0") == b"193" for i in ids):
                selected.append(line)
        if selected and selected != [record]:
            raise ValueError(f"conflicting resolver account or ID: {name}")
        if not selected:
            updates.append((path, record, False))
    resolv = etc / "resolv.conf"
    if (resolv.exists() or resolv.is_symlink()) and (
        not resolv.is_symlink() or os.readlink(resolv) != RESOLV_TARGET
    ):
        raise ValueError("conflicting offline resolver configuration")
    return updates


def install_resolver_accounts(tree: Path, updates: list[tuple[Path, bytes, bool]]) -> None:
    for path, record, create in updates:
        if create:
            fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
            with os.fdopen(fd, "wb") as out:
                out.write(record + b"\n")
        else:
            fd = os.open(path, os.O_WRONLY | os.O_APPEND | os.O_NOFOLLOW)
            with os.fdopen(fd, "ab") as out:
                out.write(record + b"\n")
    resolv = tree / "etc/resolv.conf"
    if not resolv.is_symlink():
        resolv.symlink_to(RESOLV_TARGET)


def package_files(packages: Path, profile: dict, expected: dict, scratch: Path,
                  aliases: dict[str, str]) -> list[dict]:
    files = []
    for name, destinations in expected.items():
        package = profile["packages"][name]
        if (len(package["files"]) != len(destinations)
                or {entry["destination"] for entry in package["files"]} != destinations):
            raise ValueError(f"unreviewed WLAN inventory: {name}")
        selected_aliases = {
            path: target for path, target in aliases.items()
            if (Path(path).parent / target).as_posix() in destinations
        }
        files.extend(unpack(packages / name, package, scratch, selected_aliases))
    return files


def remove_resolver_socket_activation(entry: dict, scratch: Path) -> None:
    selected = Path(entry["source"]).read_bytes()
    for field in (b"Sockets", b"Also"):
        line = b"\n" + field + b"=" + RESOLVER_SOCKET_UNITS + b"\n"
        if selected.count(line) != 1:
            raise ValueError("unexpected resolver unit socket dependencies")
        selected = selected.replace(line, b"\n")
    destination = scratch / "aetheri-resolved.service"
    with destination.open("xb") as output:
        output.write(selected)
    provenance = entry.pop("package") + " unit without socket activation dependencies"
    entry.update(source=str(destination), size=len(selected), sha256=prepare_audio.sha(selected),
                 provenance=provenance)


def install(tree: Path, kernel_root: Path, packages: Path, *, control_executable: Path | None = None,
            with_resolver: bool = False) -> dict:
    if sys.platform != "linux" or os.getuid() == 0 or os.getuid() != os.geteuid() or os.getgid() != os.getegid():
        raise ValueError("use an ordinary Linux account to stage iwd")
    tree = tree.absolute()
    if tree == Path(tree.anchor) or tree.resolve(strict=True) != tree or not tree.is_dir():
        raise ValueError("select a canonical offline image tree")
    kernel_root = desktop_wlan_payload.canonical_directory(kernel_root)
    setup = dict(SETUP_FILES)
    aliases = dict(ALIAS)
    if with_resolver:
        resolver_raw = RESOLVER_PROFILE.read_bytes()
        resolver_profile = json.loads(resolver_raw)
        if (resolver_profile["format"] != 1 or resolver_profile["signer"] != SIGNER
                or resolver_profile["packages"].keys() != RESOLVER_PACKAGES.keys()
                or resolver_profile["symlinks"] != RESOLVER_ALIAS):
            raise ValueError("unreviewed resolver profile")
        account_updates = resolver_accounts(tree)
        setup.update(RESOLVER_FILES)
        aliases.update(RESOLVER_ALIAS)
    if control_executable is not None:
        control_executable = control_executable.absolute()
        if (control_executable.resolve(strict=True) != control_executable
                or not control_executable.is_file() or not 64 <= control_executable.stat().st_size <= 16 * 1024**2):
            raise ValueError("expected a bounded regular WLAN control executable")
        with control_executable.open("rb") as stream:
            header = stream.read(20)
        if (header[:7] != b"\x7fELF\x02\x01\x01" or header[16:18] not in (b"\x02\x00", b"\x03\x00")
                or header[18:20] != b"\x3e\x00"):
            raise ValueError("expected an x86-64 WLAN control ELF")
        setup.update(CONTROL_FILES)
        setup[CONTROL_ELF] = control_executable
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
        files.extend(package_files(packages, profile, PACKAGES, scratch, ALIAS))
        if with_resolver:
            resolver_files = package_files(packages, resolver_profile, RESOLVER_PACKAGES, scratch, RESOLVER_ALIAS)
            service = next(entry for entry in resolver_files if entry["destination"] == RESOLVER_SERVICE)
            remove_resolver_socket_activation(service, scratch)
            files.extend(resolver_files)
        policy = scratch / "aetheri-iwd.conf"
        policy.write_bytes(BUS_RULES)
        for destination, source in {**setup, BUS_POLICY: policy}.items():
            files.append({"destination": destination, "source": str(source), "provenance": "Aetheri",
                          "size": source.stat().st_size, "sha256": desktop_wlan_payload.digest(source),
                          "mode": 0o755 if destination == CONTROL_ELF else 0o644})
        inventory = desktop_payload.install(tree, {"format": 1, "files": files, "symlinks": aliases})
        if with_resolver:
            install_resolver_accounts(tree, account_updates)
    result = {
        "profile_sha256": prepare_audio.sha(raw), "kernel_release": profile["kernel_release"],
        "inventory": inventory,
        "files": {entry["destination"]: entry["sha256"] for entry in files}, "symlinks": aliases,
    }
    if with_resolver:
        result["resolver"] = {"profile_sha256": prepare_audio.sha(resolver_raw), "uid": 193, "gid": 193,
                              "resolv_conf": RESOLV_TARGET}
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--image-root", required=True, type=Path)
    parser.add_argument("--kernel-root", required=True, type=Path)
    parser.add_argument("--packages", required=True, type=Path)
    parser.add_argument("--control-executable", type=Path)
    parser.add_argument("--with-resolver", action="store_true")
    args = parser.parse_args()
    try:
        print(json.dumps(install(args.image_root, args.kernel_root, args.packages,
                                 control_executable=args.control_executable,
                                 with_resolver=args.with_resolver), indent=2))
    except (OSError, ValueError, KeyError, subprocess.CalledProcessError) as error:
        print(f"desktop iwd: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
