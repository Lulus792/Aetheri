"""Boot the fixed read-only GPT/FAT32 artifact through UEFI removable-media lookup.

No -kernel/-initrd/-append, physical device, data disk, network, QMP or shared
directory is supported. Every attempt owns a new NVRAM and original child handle.
"""
import argparse
from pathlib import Path
import shutil

import test_boot_probe as boot
import uefi_probe_image as media


def run(image, output, expect_failure=False, timeout=120):
    if any("," in str(path) for path in (image, output, boot.TOOLS)):
        raise ValueError("QEMU media paths must not contain option delimiters")
    manifest = media.inspect(image)
    hashes = boot.host_tools()
    output.mkdir(mode=0o700)
    nvram = output/"nvram.fd"
    shutil.copyfile(boot.TOOLS/"share/edk2-i386-vars.fd", nvram)
    command = [
        str(boot.TOOLS/"qemu-system-x86_64.exe"),
        "-name", "aetheri-uefi-media-proof",
        "-machine", "pc-q35-11.1,accel=tcg", "-cpu", "max",
        "-smp", "2", "-m", "768M", "-no-user-config", "-nodefaults",
        "-display", "none", "-monitor", "none", "-nic", "none", "-no-reboot",
        "-drive", "if=pflash,format=raw,readonly=on,file="+str(boot.TOOLS/"share/edk2-x86_64-code.fd"),
        "-drive", "if=pflash,format=raw,file="+str(nvram),
        "-serial", "file:"+str(output/"serial.log"),
    ]
    command += ["-drive", "if=none,id=bootmedia,format=raw,readonly=on,file="+str(image/"boot.raw")]
    if "root_partition" in manifest:
        command += ["-device", "virtio-blk-pci,drive=bootmedia,serial=AETHERI_ROOT_PROBE,bootindex=1"]
    else:
        command += ["-device", "qemu-xhci,id=xhci", "-device", "usb-storage,bus=xhci.0,drive=bootmedia,removable=on,bootindex=1"]
    boot.observe(output, command, manifest, hashes, boot.sha(image/"manifest.json"), expect_failure, timeout)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--image", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--expect-nonce-failure", action="store_true")
    args = parser.parse_args()
    run(args.image.resolve(), args.output.absolute(), args.expect_nonce_failure)
