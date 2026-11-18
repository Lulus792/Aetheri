"""Rebuild the pinned iwd resolver daemon from offline inputs on Linux."""

import argparse
import io
import json
import os
from pathlib import Path
import stat
import subprocess
import sys
import tarfile
import tempfile

import desktop_iwd_payload as iwd
import desktop_wlan_payload
import prepare_audio


ELL_RUNTIME = "libell-0.83-1.fc44.x86_64.rpm"


def pinned_bytes(path: Path, pin: dict) -> bytes:
    with path.open("rb") as stream:
        body = stream.read(pin["size"] + 1)
    if len(body) != pin["size"] or prepare_audio.sha(body) != pin["sha256"]:
        raise ValueError(f"changed iwd build input: {path.name}")
    return body


def stage_ell(cache: Path, profile: dict, output: Path) -> None:
    iwd.unpack(cache / ELL_RUNTIME, profile["packages"][ELL_RUNTIME], output, iwd.ALIAS)
    for path, target in iwd.ALIAS.items():
        (output / path[1:]).symlink_to(target)
    pin = profile["resolver_daemon"]["headers_rpm"]
    archive = iwd.verified_archive(cache / pin["filename"], pin)
    headers = output / "usr/include/ell"
    headers.mkdir(parents=True)
    for name, mode, content in prepare_audio.cpio_members(archive):
        path = Path(name)
        if path.parent == Path("usr/include/ell") and path.suffix == ".h":
            if not stat.S_ISREG(mode):
                raise ValueError(f"invalid libell header: {name}")
            with (headers / path.name).open("xb") as stream:
                stream.write(content)
    if not (headers / "ell.h").is_file():
        raise ValueError("missing libell public headers")


def build(cache: Path) -> dict:
    if (sys.platform != "linux" or os.uname().machine != "x86_64" or os.getuid() == 0
            or os.getuid() != os.geteuid() or os.getgid() != os.getegid()):
        raise ValueError("use an ordinary x86-64 Linux build account")
    cache = desktop_wlan_payload.canonical_directory(cache)
    raw = iwd.PROFILE.read_bytes()
    profile = json.loads(raw)
    selected = profile["resolver_daemon"]
    if profile["format"] != 2 or profile["signer"] != iwd.SIGNER or selected["filename"] != iwd.RESOLVER_DAEMON:
        raise ValueError("unreviewed iwd build profile")
    destination = cache / iwd.RESOLVER_DAEMON
    source_pin = selected["source_archive"]
    source = pinned_bytes(cache / source_pin["filename"], source_pin)
    patch = pinned_bytes(iwd.ROOT / selected["patch"]["path"], selected["patch"])

    with tempfile.TemporaryDirectory(prefix="aetheri-iwd-build-", dir=cache) as directory:
        scratch = Path(directory)
        ell = scratch / "ell"
        stage_ell(cache, profile, ell)
        with tarfile.open(fileobj=io.BytesIO(source), mode="r:xz") as archive:
            archive.extractall(scratch, filter="data")
        tree = scratch / "iwd-3.12"
        env = {
            "PATH": os.environ["PATH"], "LC_ALL": "C", "SOURCE_DATE_EPOCH": "1773409620",
            "ELL_CFLAGS": f"-I{ell / 'usr/include'}",
            "ELL_LIBS": f"-L{ell / 'usr/lib64'} -Wl,-l:libell.so.0",
            "CFLAGS": f"-O2 -g0 -ffile-prefix-map={tree}=. -fstack-protector-strong -D_FORTIFY_SOURCE=3",
        }
        subprocess.run(["patch", "--batch", "--forward", "--fuzz=0", "-p1"], input=patch,
                       cwd=tree, env=env, stdout=sys.stderr, check=True)
        subprocess.run(["./configure", "--enable-external-ell", "--enable-pie", "--disable-client",
                        "--disable-monitor", "--disable-systemd-service", "--disable-dbus-policy",
                        "--disable-manual-pages", "--disable-dependency-tracking", "--prefix=/usr",
                        "--sysconfdir=/etc", "--localstatedir=/var"], cwd=tree, env=env,
                       stdout=sys.stderr, check=True)
        subprocess.run(["make", "-j2", "src/iwd"], cwd=tree, env=env, stdout=sys.stderr, check=True)
        candidate = scratch / iwd.RESOLVER_DAEMON
        subprocess.run(["strip", "--strip-unneeded", "-o", str(candidate), "src/iwd"], cwd=tree,
                       env=env, stdout=sys.stderr, check=True)
        iwd.reviewed_resolver_daemon(profile, scratch)
        candidate.chmod(0o755)
        os.link(candidate, destination)

    return {"profile_sha256": prepare_audio.sha(raw), "output": str(destination),
            "sha256": selected["sha256"]}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--packages", required=True, type=Path)
    args = parser.parse_args()
    try:
        print(json.dumps(build(args.packages), indent=2))
    except (OSError, ValueError, KeyError, subprocess.CalledProcessError) as error:
        print(f"build iwd: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
