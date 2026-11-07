"""Build a readonly ext4 system-root proof in a new regular GPT image file.

Ordinary Linux build user only; no mount, device or installed-service operation.
This first profile uses the selected kernel's builtin virtio/ext4 root mounting.
"""
import argparse
import gzip
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
import uuid

import build_uefi_probe as uefi
import test_boot_probe as boot

ROOT = Path(__file__).resolve().parents[1]
DISK_BYTES = 512*1024**2
ROOT_START, ROOT_SECTORS = 262144, 524288


def digest(data):
    return hashlib.sha256(data).hexdigest()


def archive_entries(data):
    """Only consume a previously admitted original boot archive."""
    at = 0
    while True:
        fields = [int(data[i:i+8], 16) for i in range(at+6, at+110, 8)]
        mode, size, namesize = fields[1], fields[6], fields[11]
        at += 110
        name = data[at:at+namesize-1].decode()
        at = (at+namesize+3)&~3
        body = data[at:at+size]
        at = (at+size+3)&~3
        if name == "TRAILER!!!":
            return
        yield name, mode, body


def build(base, packages, output, wrong_nonce=False, prepare=None):
    if sys.platform != "linux" or os.uname().machine != "x86_64" or os.geteuid() == 0:
        raise ValueError("use an ordinary x86-64 Linux build user")
    original = boot.inspect(base)
    if original.get("proof", "ram-root") != "ram-root":
        raise ValueError("expected the original RAM-root input")
    pins = json.loads((ROOT/"spec/boot-probe-inputs.json").read_text())
    config_pin = next(p for p in pins["files"] if p["source"].endswith("/config"))
    config = Path(config_pin["source"]).read_bytes()
    if digest(config) != config_pin["sha256"]:
        raise ValueError("selected kernel configuration changed")
    for feature in ("EXT4_FS", "VIRTIO_PCI", "VIRTIO_BLK", "DEVTMPFS"):
        if ("CONFIG_"+feature+"=y\n").encode() not in config:
            raise ValueError("required builtin driver absent: "+feature)
    output.mkdir(mode=0o700)
    logs = output/"logs"
    logs.mkdir()
    configuration = Path("/etc/mke2fs.conf").read_bytes()
    (output/"mke2fs.conf").write_bytes(configuration)
    environment = {**os.environ, "LC_ALL":"C", "MKE2FS_CONFIG":str(output/"mke2fs.conf")}

    def command(name, args, data=None):
        p = subprocess.run(args, input=data, capture_output=True, env=environment)
        (logs/(name+".stdout")).write_bytes(p.stdout)
        (logs/(name+".stderr")).write_bytes(p.stderr)
        p.check_returncode()
        return p.stdout

    tree = output/"root"
    tree.mkdir()
    links = []
    old_probe = None
    for name, mode, data in archive_entries(gzip.decompress((base/"initramfs.cpio.gz").read_bytes())):
        path = tree/name
        kind = mode & 0o170000
        if kind == 0o040000:
            path.mkdir(parents=True, exist_ok=True)
            path.chmod(mode & 0o7777)
        elif kind == 0o120000:
            links.append((path, data.decode()))
        else:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
            path.chmod(mode & 0o7777)
            if name == "usr/libexec/aetheri-boot-probe":
                old_probe = data
    for path, target in links:
        path.symlink_to(target)
    for name in ("etc/aetheri-boot-proof", "etc/systemd/system/aetheri-boot.target", "etc/systemd/system/aetheri-boot.service", "usr/libexec/aetheri-boot-probe", "init"):
        (tree/name).unlink()
    compiler = ["/usr/bin/gcc", "-std=c17", "-D_GNU_SOURCE", "-Wall", "-Wextra", "-Wpedantic", "-Wconversion", "-Wshadow", "-Werror", "-O2", "-g0", "-Wl,--build-id=none"]
    command("old-probe", [*compiler, str(ROOT/"src/boot/probe.c"), "-o", str(output/"old-probe")])
    if (output/"old-probe").read_bytes() != old_probe:
        raise ValueError("original RAM diagnostic binary changed")
    command("root-probe", [*compiler, "-DAETHERI_ROOT_PROBE", "-fanalyzer", str(ROOT/"src/boot/probe.c"), "-o", str(tree/"usr/libexec/aetheri-root-probe")])
    nonce = original["nonce"]
    namespace = uuid.UUID("e64786b8-e09e-496c-8175-731a1debcf8f")
    root_guid = str(uuid.uuid5(namespace, nonce+"/root-partition"))
    fs_uuid = str(uuid.uuid5(namespace, nonce+"/root-filesystem"))
    (tree/"etc/os-release").write_text('NAME="Aetheri system root proof"\nID=aetheri-root-probe\nPRETTY_NAME="Aetheri readonly system root proof"\n')
    (tree/"etc/aetheri-root-proof").write_text(("0"*32 if wrong_nonce else nonce)+"\n"+original["kernel_release"]+"\n")
    (tree/"etc/systemd/system/default.target").symlink_to("aetheri-root.target")
    (tree/"etc/systemd/system/aetheri-root.target").write_text("[Unit]\nDescription=Aetheri readonly system root proof\nDefaultDependencies=no\nRequires=aetheri-root.service\nAfter=aetheri-root.service\n")
    (tree/"etc/systemd/system/aetheri-root.service").write_text("[Unit]\nDescription=Aetheri root observations\nDefaultDependencies=no\nSuccessAction=poweroff-immediate\nFailureAction=poweroff-immediate\n[Service]\nType=oneshot\nExecStart=/usr/libexec/aetheri-root-probe\nStandardOutput=tty\nStandardError=tty\nTTYPath=/dev/console\nTimeoutStartSec=20\n")
    addition = prepare(tree, nonce) if prepare else {}
    target = addition.get("target", "aetheri-root.target")
    proof = addition.get("proof", "readonly-root")
    root_image = output/"root.ext4"
    with root_image.open("xb") as stream:
        stream.truncate(ROOT_SECTORS*512)
    command("mkfs", ["/usr/bin/mkfs.ext4", "-q", "-F", "-U", fs_uuid, "-L", "AETHERI_SYSTEM", "-E", "root_owner=0:0", "-d", str(tree), str(root_image)])
    paths = ["/", *["/"+p.relative_to(tree).as_posix() for p in sorted(tree.rglob("*"))]]
    if any(not re.fullmatch(r"/[A-Za-z0-9._+/-]*", p) or ".." in Path(p).parts for p in paths):
        raise ValueError("unsupported bootstrap inode path")
    batch = output/"owners.debugfs"
    batch.write_text("".join(f'set_inode_field "{p}" uid 0\nset_inode_field "{p}" gid 0\n' for p in paths))
    command("owners", ["/usr/bin/debugfs", "-w", "-f", str(batch), str(root_image)])
    batch = output/"verify.debugfs"
    batch.write_text("".join(f'stat "{p}"\n' for p in paths))
    result = command("inodes", ["/usr/bin/debugfs", "-f", str(batch), str(root_image)]).decode()
    owners = re.findall(r"User:\s*(\d+)\s+Group:\s*(\d+)", result)
    if len(owners) != len(paths) or any(pair != ("0", "0") for pair in owners):
        raise ValueError("root inode ownership verification failed")
    command("fsck", ["/usr/bin/e2fsck", "-f", "-n", str(root_image)])

    # No /init: the selected builtin kernel root path mounts ext4 itself.
    thin = output/"kernel-inputs"
    thin.mkdir()
    marker = output/"thin"
    marker.mkdir()
    text = b"Aetheri builtin-kernel ext4 root mounting profile\n"
    (marker/".aetheri-kernel-root").write_bytes(text)
    (marker/".aetheri-kernel-root").chmod(0o644)
    (marker/"dev").mkdir()
    os.utime(marker/"dev", (0, 0))
    os.utime(marker/".aetheri-kernel-root", (0, 0))
    raw = subprocess.check_output(["/usr/bin/cpio", "--quiet", "--null", "--create", "--format=newc", "--owner=0:0", "--reproducible"], input=b"dev\0.aetheri-kernel-root\0", cwd=marker)
    (thin/"initramfs.cpio.gz").write_bytes(gzip.compress(raw, mtime=0))
    shutil.copyfile(base/"vmlinuz", thin/"vmlinuz")
    cmdline = f"console=ttyS0,115200n8 panic=-1 root=PARTUUID={root_guid} rootwait=20 ro rootfstype=ext4 rootflags=noload init=/usr/lib/systemd/systemd systemd.unit={target} systemd.log_target=console systemd.show_status=yes aetheri.boot_nonce={nonce}\n"
    (thin/"cmdline.txt").write_text(cmdline)
    thin_manifest = dict(format=1, proof=proof, nonce=nonce, kernel_release=original["kernel_release"], outputs={n:boot.sha(thin/n) for n in ("vmlinuz", "initramfs.cpio.gz", "cmdline.txt")}, root={".aetheri-kernel-root":{"sha256":digest(text),"mode":"0o644"}})
    (thin/"manifest.json").write_text(json.dumps(thin_manifest, indent=2)+"\n")
    uefi.build(thin, packages, output/"efi")
    manifest = json.loads((output/"efi/manifest.json").read_text())
    disk_id = str(uuid.uuid5(namespace, nonce+"/system-disk"))
    final = output/"boot.raw"
    with final.open("xb") as stream:
        stream.truncate(DISK_BYTES)
    layout = f'label: gpt\nlabel-id: {disk_id}\nunit: sectors\nfirst-lba: 2048\n\nstart=2048,size=258048,type=U,uuid={manifest["esp_guid"]},name="Aetheri boot proof"\nstart={ROOT_START},size={ROOT_SECTORS},type=4f68bce3-e8cd-4db1-96e7-fbcaf984b709,uuid={root_guid},attrs="GUID:60",name="Aetheri system root"\n'
    command("gpt", ["/usr/bin/sfdisk", "--quiet", "--no-reread", "--no-tell-kernel", str(final)], layout.encode())
    with final.open("r+b") as target, (output/"efi/boot.raw").open("rb") as source:
        target.seek(2048*512)
        source.seek(2048*512)
        left = 258048*512
        while left:
            data = source.read(min(left, 1024**2))
            if not data:
                raise ValueError("truncated ESP source")
            if data.strip(b"\0"):
                target.write(data)
            else:
                target.seek(len(data), 1)
            left -= len(data)
        target.seek(ROOT_START*512)
        with root_image.open("rb") as source:
            while data := source.read(1024**2):
                if data.strip(b"\0"):
                    target.write(data)
                else:
                    target.seek(len(data), 1)
    shutil.copyfile(output/"efi/BOOTX64.EFI", output/"BOOTX64.EFI")
    root_tools = {}
    for name in ("gcc", "mkfs.ext4", "debugfs", "e2fsck"):
        path = Path("/usr/bin")/name
        root_tools[str(path)] = dict(sha256=boot.sha(path), package=subprocess.check_output(["/usr/bin/rpm", "-qf", "--qf", "%{NEVRA}", str(path)], text=True))
    manifest.update(
        disk_bytes=DISK_BYTES,
        disk_guid=disk_id,
        root_builder_sha256=boot.sha(Path(__file__)),
        root_probe_source_sha256=boot.sha(ROOT/"src/boot/probe.c"),
        root_partition=dict(guid=root_guid, filesystem_uuid=fs_uuid, start=ROOT_START, sectors=ROOT_SECTORS, sha256=boot.sha(root_image)),
        root_tools=root_tools,
        mke2fs_configuration_sha256=digest(configuration),
        root_inode_count=len(paths),
        root_inventory={p.relative_to(tree).as_posix(): ({"symlink":os.readlink(p)} if p.is_symlink() else {"sha256":boot.sha(p)}) for p in tree.rglob("*") if p.is_symlink() or p.is_file()},
        outputs={"boot.raw":boot.sha(final),"BOOTX64.EFI":boot.sha(output/"BOOTX64.EFI")},
    )
    manifest.update(addition)
    (output/"manifest.json").write_text(json.dumps(manifest,indent=2)+"\n")
    print(json.dumps(dict(output=str(output),root_inodes=len(paths),disk_bytes=DISK_BYTES)))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--boot-image",type=Path,required=True)
    parser.add_argument("--packages",type=Path,required=True)
    parser.add_argument("--output",type=Path,required=True)
    parser.add_argument("--wrong-nonce",action="store_true",help="separate diagnostic-only negative fixture")
    args=parser.parse_args()
    build(args.boot_image.resolve(),args.packages.resolve(),args.output.absolute(),args.wrong_nonce)
