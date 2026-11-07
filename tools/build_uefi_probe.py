"""Assemble an unsigned UKI and GPT/FAT32 boot-probe file as a Linux build user.

Only creates a new output directory and regular image file. Never accepts a
device, installs a package, mounts a filesystem or modifies firmware settings.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import uuid

import test_boot_probe as boot

ROOT = Path(__file__).resolve().parents[1]
PINS = ROOT / "spec/uefi-probe-inputs.json"
DISK_BYTES = 128 * 1024**2
ESP_START, ESP_SECTORS = 2048, 258048


def sha(data):
    return hashlib.sha256(data).hexdigest()


def build(image, packages, output):
    if sys.platform != "linux" or os.uname().machine != "x86_64" or os.geteuid() == 0:
        raise ValueError("use an ordinary x86-64 Linux build user")
    original = boot.inspect(image)
    pin_bytes = PINS.read_bytes()
    pin = json.loads(pin_bytes)
    if pin["format"] != 1:
        raise ValueError("unsupported UEFI probe inputs")
    archives = {}
    for entry in pin["packages"]:
        name = entry["name"]
        if Path(name).name != name or name in archives:
            raise ValueError("invalid package input")
        data = (packages/name).read_bytes()
        if len(data) != entry["size"] or sha(data) != entry["sha256"]:
            raise ValueError("changed package: " + name)
        archives[name] = data
    output.mkdir(mode=0o700)
    prefix = output / "tools"
    prefix.mkdir()
    logs = output / "logs"
    logs.mkdir()
    env = {**os.environ, "PYTHONDONTWRITEBYTECODE": "1", "SOURCE_DATE_EPOCH": "0", "LC_ALL": "C", "MTOOLSRC": str(output/"empty.conf")}
    (output/"empty.conf").write_bytes(b"")

    def command(name, args, data=None):
        result = subprocess.run(args, input=data, capture_output=True, env=env)
        (logs/(name+".stdout")).write_bytes(result.stdout)
        (logs/(name+".stderr")).write_bytes(result.stderr)
        result.check_returncode()
        return result.stdout

    for entry in pin["packages"]:
        name = entry["name"]
        rpm = output/name
        with rpm.open("xb") as stream:
            stream.write(archives[name])
        signature = command(name+"-signature", ["/usr/bin/rpmkeys", "--checksig", "--verbose", str(rpm)]).decode()
        if pin["fedora_signer"].lower()+": OK" not in signature:
            raise ValueError("unexpected package signer")
        payload = command(name+"-cpio", ["/usr/bin/rpm2cpio", str(rpm)])
        for relative, expected in entry["payloads"].items():
            path = Path(relative)
            if path.is_absolute() or ".." in path.parts:
                raise ValueError("invalid pinned payload path")
            data = command(name+"-"+relative.replace("/", "_"), ["/usr/bin/cpio", "--quiet", "--extract", "--to-stdout", "./"+relative], payload)
            if len(data) != expected["size"] or sha(data) != expected["sha256"]:
                raise ValueError("changed package payload: " + relative)
            target = prefix/path
            target.parent.mkdir(parents=True, exist_ok=True)
            with target.open("xb") as stream:
                stream.write(data)

    env["PYTHONPATH"] = str(prefix/"usr/lib/python3.14/site-packages")
    osrel = b'NAME="Aetheri boot proof"\nID=aetheri-boot-probe\nPRETTY_NAME="Aetheri UEFI media proof"\n'
    if original.get("proof") in ("readonly-root", "writable-data"):
        osrel = b'NAME="Aetheri system root proof"\nID=aetheri-root-probe\nPRETTY_NAME="Aetheri readonly system root proof"\n'
    (output/"os-release").write_bytes(osrel)
    uki = output/"BOOTX64.EFI"
    ukify = ["/usr/bin/python3", str(prefix/"usr/bin/ukify"), "--config="+str(output/"empty.conf"), "--efi-arch=x64", "--stub="+str(prefix/"usr/lib/systemd/boot/efi/linuxx64.efi.stub"), "--linux="+str(image/"vmlinuz"), "--initrd="+str(image/"initramfs.cpio.gz"), "--cmdline=@"+str(image/"cmdline.txt"), "--os-release=@"+str(output/"os-release"), "--uname="+original["kernel_release"], "--output="+str(uki), "build"]
    command("ukify", ukify)
    image_file = output/"boot.raw"
    with image_file.open("xb") as stream:
        stream.truncate(DISK_BYTES)
    # All formatter/partitioner targets are this newly created regular file.
    namespace = uuid.UUID("b8d727ab-5f3d-489c-a24b-bb10b987c29a")
    disk_id = str(uuid.uuid5(namespace, original["nonce"]+"/disk"))
    esp_id = str(uuid.uuid5(namespace, original["nonce"]+"/esp"))
    layout = f'label: gpt\nlabel-id: {disk_id}\nunit: sectors\nfirst-lba: {ESP_START}\n\nstart={ESP_START}, size={ESP_SECTORS}, type=U, uuid={esp_id}, name="Aetheri boot proof"\n'
    command("gpt", ["/usr/bin/sfdisk", "--quiet", "--no-reread", "--no-tell-kernel", str(image_file)], layout.encode())
    command("fat", ["/usr/bin/mkfs.fat", "--invariant", "-F", "32", "-i", original["nonce"][:8], "-n", "AETHERI", "-h", str(ESP_START), "--offset="+str(ESP_START), str(image_file), str(ESP_SECTORS//2)])
    media = str(image_file)+"@@"+str(ESP_START*512)
    command("efi-directory", ["/usr/bin/mmd", "-i", media, "::/EFI"])
    command("boot-directory", ["/usr/bin/mmd", "-i", media, "::/EFI/BOOT"])
    os.utime(uki, (315532800, 315532800))  # FAT's earliest representable date.
    command("copy-uki", ["/usr/bin/mcopy", "-m", "-i", media, str(uki), "::/EFI/BOOT/BOOTX64.EFI"])
    readback = output/"readback.efi"
    command("readback", ["/usr/bin/mcopy", "-i", media, "::/EFI/BOOT/BOOTX64.EFI", str(readback)])
    if readback.read_bytes() != uki.read_bytes():
        raise ValueError("FAT copy does not match original UKI")
    sections = {".linux": original["outputs"]["vmlinuz"], ".initrd": original["outputs"]["initramfs.cpio.gz"], ".cmdline": original["outputs"]["cmdline.txt"], ".osrel": sha(osrel), ".uname": sha(original["kernel_release"].encode())}
    build_tools = {}
    for name in ("rpmkeys", "rpm2cpio", "cpio", "sfdisk", "mkfs.fat", "mmd", "mcopy"):
        path = Path("/usr/bin")/name
        build_tools[str(path)] = {
            "sha256": boot.sha(path),
            "package": subprocess.check_output(["/usr/bin/rpm", "-qf", "--qf", "%{NEVRA}", str(path)], text=True),
        }
    result = dict(
        format=1,
        purpose="unsigned standalone UEFI media boot proof, not a production OS image",
        nonce=original["nonce"],
        proof=original.get("proof", "ram-root"),
        kernel_release=original["kernel_release"],
        boot_manifest_sha256=boot.sha(image/"manifest.json"),
        pins_sha256=sha(pin_bytes),
        builder_sha256=boot.sha(Path(__file__)),
        disk_bytes=DISK_BYTES,
        disk_guid=disk_id,
        esp_guid=esp_id,
        esp_start=ESP_START,
        esp_sectors=ESP_SECTORS,
        sections=sections,
        outputs={"boot.raw": boot.sha(image_file), "BOOTX64.EFI": boot.sha(uki)},
        ukify_command=ukify,
        build_tools=build_tools,
    )
    (output/"manifest.json").write_text(json.dumps(result, indent=2)+"\n")
    print(json.dumps(dict(output=str(output), uki_bytes=uki.stat().st_size, disk_bytes=DISK_BYTES)))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--boot-image", type=Path, required=True)
    parser.add_argument("--packages", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    build(args.boot_image.resolve(), args.packages.resolve(), args.output.absolute())
