"""Prepare the pinned private PipeWire SDK/runtime; never install an RPM.

Package signatures and exact bytes precede data-only extraction. Only named
regular members reach a fresh prefix; scripts and archive links never run.
This is developer tooling, not an untrusted package installer or audio broker.
"""
import argparse
import contextlib
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import stat
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
PROFILE = ROOT / "spec/audio-inputs.json"


def sha(raw):
    return hashlib.sha256(raw).hexdigest()


@contextlib.contextmanager
def sealed_package(path, package):
    """Hash/signature/parser share one immutable data object, not repeated paths."""
    import fcntl
    path = Path(path)
    if path.is_symlink() or not path.is_file() or path.stat().st_size != package["size"]:
        raise ValueError("changed audio RPM")
    raw = path.read_bytes()
    if len(raw) != package["size"] or sha(raw) != package["sha256"]:
        raise ValueError("changed audio RPM bytes")
    fd = os.memfd_create("aetheri-audio-rpm", os.MFD_CLOEXEC | os.MFD_ALLOW_SEALING)
    try:
        os.fchmod(fd, 0o600)
        pending = memoryview(raw)
        while pending:
            written = os.write(fd, pending)
            if written <= 0:
                raise OSError("audio RPM snapshot write")
            pending = pending[written:]
        seals = fcntl.F_SEAL_WRITE | fcntl.F_SEAL_GROW | fcntl.F_SEAL_SHRINK | fcntl.F_SEAL_SEAL
        fcntl.fcntl(fd, fcntl.F_ADD_SEALS, seals)
        if fcntl.fcntl(fd, fcntl.F_GET_SEALS) != seals:
            raise ValueError("audio RPM snapshot not sealed")
        yield fd
    finally:
        os.close(fd)


def canonical(name):
    if not isinstance(name, str) or not re.fullmatch(r"usr/[A-Za-z0-9_./+@-]+", name):
        raise ValueError("invalid audio member")
    path = PurePosixPath(name)
    if str(path) != name or ".." in path.parts:
        raise ValueError("escaping audio member")
    return path


def profile():
    raw = PROFILE.read_bytes()
    value = json.loads(raw)
    if value.get("format") != 1 or value.get("version") != "1.6.11" or value.get("signer") != "36f612dcf27f7d1a48a835e4dbfcf71c6d9f90a6":
        raise ValueError("unreviewed audio profile")
    packages = value["packages"]
    expected = {p + "-1.6.11-1.fc44.x86_64.rpm" for p in ("pipewire", "pipewire-libs", "pipewire-devel", "pipewire-utils")}
    if len(packages) != 4 or {p["name"] for p in packages} != expected:
        raise ValueError("unreviewed audio packages")
    for p in packages:
        if not re.fullmatch("[0-9a-f]{64}", p["sha256"]) or type(p["size"]) is not int or not 0 < p["size"] <= 16 * 1024**2:
            raise ValueError("invalid audio package pin")
    if not 1 <= len(value["files"]) <= 512:
        raise ValueError("audio inventory bound")
    seen = set()
    for entry in value["files"]:
        canonical(entry["path"])
        if entry["path"] in seen or entry["package"] not in expected or not re.fullmatch("[0-9a-f]{64}", entry["sha256"]) or type(entry["size"]) is not int or not 0 <= entry["size"] <= 4 * 1024**2 or entry["mode"] not in (0o644, 0o755):
            raise ValueError("invalid audio payload pin")
        seen.add(entry["path"])
    if sum(e["size"] for e in value["files"]) > 32 * 1024**2:
        raise ValueError("audio payload budget")
    links = {"usr/lib64/libpipewire-0.3.so.0": "libpipewire-0.3.so.0.1611.0", "usr/lib64/libpipewire-0.3.so": "libpipewire-0.3.so.0"}
    if value["symlinks"] != links or seen & links.keys():
        raise ValueError("different audio aliases")
    return value, sha(raw)


def cpio_members(raw):
    """Bounded newc data iterator. No member ever selects an extraction path."""
    if len(raw) > 64 * 1024**2:
        raise ValueError("audio archive budget")
    at = 0
    seen = set()
    for _ in range(4096):
        if at + 110 > len(raw) or raw[at:at + 6] != b"070701":
            raise ValueError("invalid audio newc header")
        fields = raw[at + 6:at + 110]
        if not re.fullmatch(b"[0-9a-fA-F]{104}", fields):
            raise ValueError("invalid audio newc fields")
        values = [int(fields[i:i + 8], 16) for i in range(0, 104, 8)]
        mode, size, namesize = values[1], values[6], values[11]
        at += 110
        if not 1 <= namesize <= 4096 or size > 16 * 1024**2 or at + namesize > len(raw):
            raise ValueError("audio archive entry bound")
        name = raw[at:at + namesize]
        if name[-1:] != b"\0" or b"\0" in name[:-1]:
            raise ValueError("invalid audio archive name")
        name = name[:-1].decode("utf-8", errors="strict")
        at = (at + namesize + 3) & ~3
        end = at + size
        if end > len(raw):
            raise ValueError("truncated audio archive")
        data = memoryview(raw)[at:end]
        at = (end + 3) & ~3
        if name == "TRAILER!!!":
            if size or any(raw[end:]):
                raise ValueError("data after audio trailer")
            return
        if name.startswith("./"):
            name = name[2:]
        if name == "." and stat.S_ISDIR(mode):
            continue
        path = PurePosixPath(name)
        if not re.fullmatch(r"[A-Za-z0-9_./+@-]+", name) or path.is_absolute() or str(path) != name or ".." in path.parts:
            raise ValueError("escaping audio archive name")
        if name in seen:
            raise ValueError("duplicate audio archive entry")
        seen.add(name)
        yield name, mode, data
    raise ValueError("audio archive entry count")


def check(prefix):
    pins, digest = profile()
    if sys.platform != "linux":
        raise ValueError("audio prefix targets the reviewed native Linux development carrier")
    for package, expected in pins["development_runtime"].items():
        actual = subprocess.check_output(["rpm", "-q", "--qf", "%{NAME}-%{VERSION}-%{RELEASE}.%{ARCH}", package], text=True)
        if actual != expected:
            raise ValueError("unreviewed development audio dependency: " + package)
    prefix = Path(prefix).absolute()
    if prefix.is_symlink() or prefix.resolve() != prefix or not prefix.is_dir():
        raise ValueError("aliased audio prefix")
    metadata = prefix / "inputs.json"
    if metadata.is_symlink() or not metadata.is_file():
        raise ValueError("aliased audio prefix record")
    record = json.loads(metadata.read_text())
    if record["profile_sha256"] != digest or record["version"] != pins["version"] or record["files"] != len(pins["files"]):
        raise ValueError("audio prefix from another profile")
    expected = {"inputs.json"}
    for entry in pins["files"]:
        p = prefix / entry["path"]
        if p.is_symlink() or not p.is_file() or stat.S_IMODE(p.stat().st_mode) != entry["mode"] or p.stat().st_size != entry["size"] or sha(p.read_bytes()) != entry["sha256"]:
            raise ValueError("changed audio prefix member: " + entry["path"])
        expected.add(entry["path"])
    for name, target in pins["symlinks"].items():
        p = prefix / name
        if not p.is_symlink() or os.readlink(p) != target:
            raise ValueError("changed audio prefix alias")
        expected.add(name)
    actual = {p.relative_to(prefix).as_posix() for p in prefix.rglob("*") if p.is_file() or p.is_symlink()}
    if actual != expected:
        raise ValueError("unknown/missing audio prefix members")
    return record


def prepare(cache, prefix):
    if sys.platform != "linux" or os.getuid() == 0 or os.getuid() != os.geteuid() or os.getgid() != os.getegid():
        raise ValueError("use ordinary Linux build account")
    pins, digest = profile()
    prefix = Path(prefix).absolute()
    if prefix.is_symlink() or prefix.resolve() != prefix:
        raise ValueError("aliased audio destination")
    # Authenticate every immutable snapshot before creating an output prefix.
    proofs = []
    with contextlib.ExitStack() as snapshots:
        packages = []
        for package in pins["packages"]:
            fd = snapshots.enter_context(sealed_package(Path(cache) / package["name"], package))
            path = "/proc/self/fd/" + str(fd)
            proof = subprocess.check_output(["rpmkeys", "--checksig", "--verbose", path], pass_fds=(fd,), text=True)
            if pins["signer"] + ": ok" not in proof.lower() or "payload sha256 digest: ok" not in proof.lower():
                raise ValueError("unverified audio RPM signer/payload")
            proofs.append({"name": package["name"], "signature": proof})
            packages.append((package, fd, path))
        prefix.mkdir(mode=0o700)
        for package, fd, path in packages:
            wanted = {e["path"]: e for e in pins["files"] if e["package"] == package["name"]}
            archive = subprocess.check_output(["rpm2cpio", path], pass_fds=(fd,))
            seen = set()
            for name, mode, data in cpio_members(archive):
                if name not in wanted:
                    continue
                entry = wanted[name]
                if not stat.S_ISREG(mode) or len(data) != entry["size"] or sha(data) != entry["sha256"]:
                    raise ValueError("changed audio payload")
                target = prefix / name
                target.parent.mkdir(parents=True, exist_ok=True)
                with target.open("xb") as stream:
                    stream.write(data)
                target.chmod(entry["mode"])
                seen.add(name)
            if seen != wanted.keys():
                raise ValueError("missing audio payload")
    for name, target in pins["symlinks"].items():
        (prefix / name).symlink_to(target)
    record = {"profile_sha256": digest, "packages": proofs, "version": pins["version"], "files": len(pins["files"]), "scope": pins["purpose"]}
    with (prefix / "inputs.json").open("x") as stream:
        json.dump(record, stream, indent=2)
        stream.write("\n")
    check(prefix)
    return record


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("prepare", "check"))
    parser.add_argument("prefix", type=Path)
    parser.add_argument("--cache", type=Path)
    args = parser.parse_args()
    if args.action == "prepare" and args.cache is None:
        parser.error("prepare needs --cache")
    result = prepare(args.cache, args.prefix) if args.action == "prepare" else check(args.prefix)
    print(json.dumps({"result": "checked", "version": result["version"], "files": result["files"]}))


if __name__ == "__main__":
    main()
