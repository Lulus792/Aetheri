"""Build a diskless systemd/initramfs boot proof from explicit carrier pins.

Run as an ordinary Linux user in a new private workspace. No installation,
mount, package download, device access or production-base selection is performed.
"""
import argparse
import gzip
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
PINS = ROOT / "spec/boot-probe-inputs.json"


def digest(data):
    return hashlib.sha256(data).hexdigest()


def checked_inputs(pin):
    if pin["format"] != 1 or pin["architecture"] != "x86_64":
        raise ValueError("unsupported boot-proof profile")
    result = {}
    for entry in pin["files"]:
        name = entry["source"]
        if not name.startswith("/") or ".." in Path(name).parts or name in result:
            raise ValueError("invalid or duplicate input")
        path = Path(name)
        if str(path.resolve()) != entry["canonical"]:
            raise ValueError("input canonical path changed: " + name)
        data = path.read_bytes()
        if len(data) != entry["size"] or digest(data) != entry["sha256"]:
            raise ValueError("input content changed: " + name)
        result[name] = data
    return result


def build(output, nonce):
    if sys.platform != "linux" or os.uname().machine != "x86_64" or os.geteuid() == 0:
        raise ValueError("use an ordinary x86-64 Linux build user")
    if not re.fullmatch("[0-9a-f]{32}", nonce):
        raise ValueError("nonce must be 32 lowercase hexadecimal characters")
    pin_data = PINS.read_bytes()
    pin = json.loads(pin_data)
    inputs = checked_inputs(pin)
    # Validate everything before reserving a new output. Never reuse an attempt.
    output.mkdir(mode=0o700)
    tree = output / "root"
    tree.mkdir()

    def put(name, data, mode=0o644):
        path = tree / name
        path.parent.mkdir(parents=True, exist_ok=True)
        with path.open("xb") as stream:
            stream.write(data)
        path.chmod(mode)

    for name, data in inputs.items():
        if "/modules/" in name:
            continue
        target = ("/usr" + name) if name.startswith("/lib64/") else name
        put(target.lstrip("/"), data, 0o644 if name.startswith("/usr/share/licenses/") else 0o755)
    for name in ("dev", "proc", "sys", "run", "tmp", "etc", "usr/libexec"):
        (tree / name).mkdir(parents=True, exist_ok=True)
    (tree / "tmp").chmod(0o1777)
    (tree / "lib64").symlink_to("usr/lib64")
    (tree / "init").symlink_to("usr/lib/systemd/systemd")
    put("etc/os-release", b'NAME="Aetheri boot proof"\nID=aetheri-boot-probe\nPRETTY_NAME="Aetheri diskless boot proof"\n')
    put("etc/machine-id", b"")
    put("etc/passwd", b"root:x:0:0:Boot proof:/root:/nonexistent\n")
    put("etc/group", b"root:x:0:\n")
    put("etc/aetheri-boot-proof", (nonce + "\n" + pin["kernel_release"] + "\n").encode())
    put("etc/systemd/system/aetheri-boot.target", b"[Unit]\nDescription=Aetheri diskless boot proof\nDefaultDependencies=no\nRequires=aetheri-boot.service\nAfter=aetheri-boot.service\n")
    put("etc/systemd/system/aetheri-boot.service", b"[Unit]\nDescription=Aetheri boot observations\nDefaultDependencies=no\nSuccessAction=poweroff-immediate\nFailureAction=poweroff-immediate\n[Service]\nType=oneshot\nExecStart=/usr/libexec/aetheri-boot-probe\nStandardInput=null\nStandardOutput=tty\nStandardError=tty\nTTYPath=/dev/console\nTimeoutStartSec=20\n")
    source = ROOT / "src/boot/probe.c"
    command = ["/usr/bin/gcc", "-std=c17", "-D_GNU_SOURCE", "-Wall", "-Wextra", "-Wpedantic", "-Wconversion", "-Wshadow", "-Werror", "-O2", "-g0", "-Wl,--build-id=none", str(source), "-o", str(tree / "usr/libexec/aetheri-boot-probe")]
    compiler = subprocess.run(command, capture_output=True, check=True)
    (output / "compiler.log").write_bytes(compiler.stdout + compiler.stderr)
    # Fixed metadata + stable ordering. cpio records root ownership without chown.
    paths = sorted(tree.rglob("*"))
    for path in [tree, *paths]:
        os.utime(path, (0, 0), follow_symlinks=False)
    names = b"\0".join(p.relative_to(tree).as_posix().encode() for p in paths) + b"\0"
    archive = subprocess.run(["/usr/bin/cpio", "--null", "--create", "--format=newc", "--owner=0:0", "--reproducible", "--quiet"], input=names, cwd=tree, capture_output=True, check=True)
    initrd = gzip.compress(archive.stdout, mtime=0)
    (output / "initramfs.cpio.gz").write_bytes(initrd)
    kernel = inputs["/lib/modules/" + pin["kernel_release"] + "/vmlinuz"]
    (output / "vmlinuz").write_bytes(kernel)
    cmdline = "console=ttyS0,115200n8 panic=-1 rdinit=/init systemd.unit=aetheri-boot.target systemd.log_target=console systemd.show_status=yes aetheri.boot_nonce=" + nonce
    (output / "cmdline.txt").write_text(cmdline + "\n")
    objects = {}
    for path in paths:
        if path.is_symlink():
            objects[path.relative_to(tree).as_posix()] = {"symlink": os.readlink(path)}
        elif path.is_file():
            objects[path.relative_to(tree).as_posix()] = {
                "sha256": digest(path.read_bytes()),
                "mode": oct(path.stat().st_mode & 0o7777),
            }
    manifest = dict(
        format=1,
        purpose="diskless UEFI/systemd boot proof; not a laptop image or production base",
        nonce=nonce,
        kernel_release=pin["kernel_release"],
        pins_sha256=digest(pin_data),
        probe_source_sha256=digest(source.read_bytes()),
        builder_sha256=digest(Path(__file__).read_bytes()),
        tool_versions={name: subprocess.check_output(["/usr/bin/" + name, "--version"], text=True).splitlines()[0] for name in ("gcc", "ld", "as", "cpio")},
        command=command,
        outputs={n: digest((output/n).read_bytes()) for n in ("vmlinuz", "initramfs.cpio.gz", "cmdline.txt")},
        root=objects,
    )
    (output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(json.dumps({"output": str(output), "nonce": nonce, "initramfs_bytes": len(initrd)}))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--nonce", required=True)
    args = parser.parse_args()
    build(args.output.absolute(), args.nonce)
