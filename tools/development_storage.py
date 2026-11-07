"""Prepare the fixed additional developer disk; never resize the carrier root.

Run on the Windows VM host. An interrupted creation remains pending and is not
replayed or adopted from an image header. Guest formatting is a separate action.
This registration assumes the same trusted host user and private VM directory;
file IDs are replacement guards, not volume/clone authentication.
"""
import argparse
import base64
import json
import os
from pathlib import Path
import shutil
import stat
import subprocess
import sys
import uuid


SIZE = 8 * 1024**3
DIRECTORY = "development-storage"
RECORD = "registration.json"
IMAGE = "work.qcow2"


def paths(vm):
    directory = vm.owned(vm.VM / DIRECTORY)
    # Reject a redirected ancestor too, including Windows junctions.
    if directory.resolve() != directory.absolute():
        raise ValueError("redirected developer storage directory")
    if directory.exists() and not directory.is_dir():
        raise ValueError("developer storage directory is not a directory")
    return directory, directory / RECORD, directory / IMAGE


def file_identity(path):
    info = path.lstat()
    if (path.is_symlink() or not stat.S_ISREG(info.st_mode) or info.st_nlink != 1
            or info.st_ino == 0):
        raise ValueError("developer image must be an original singly linked regular file")
    return {"device": info.st_dev, "inode": info.st_ino}


def validate(record, vm_uuid):
    keys = {"format", "vm_uuid", "token", "serial", "bytes", "fs_uuid", "stage", "image_identity"}
    if (not isinstance(record, dict) or set(record) != keys or type(record["format"]) is not int
            or record["format"] != 1 or record["vm_uuid"] != vm_uuid
            or type(record["bytes"]) is not int or record["bytes"] != SIZE
            or record["stage"] not in ("creating", "ready")):
        raise ValueError("invalid developer storage registration")
    for name in ("token", "fs_uuid"):
        value = record[name]
        if not isinstance(value, str) or str(uuid.UUID(value)) != value or uuid.UUID(value).version != 4:
            raise ValueError("invalid developer storage identity")
    if record["serial"] != "AETHBLD_" + uuid.UUID(record["token"]).hex[:12]:
        raise ValueError("developer storage serial differs from original registration")
    identity = record["image_identity"]
    if (not isinstance(identity, dict) or set(identity) != {"device", "inode"}
            or any(type(n) is not int or n < 0 for n in identity.values()) or identity["inode"] == 0):
        raise ValueError("invalid developer image object identity")
    return record


def load(vm, required=False):
    directory, record_path, image = paths(vm)
    if not record_path.exists():
        if directory.exists() and any(directory.iterdir()):
            raise ValueError("unregistered developer storage; inspect without adoption")
        if required:
            raise ValueError("developer storage has not been prepared")
        return None
    file_identity(record_path)
    record = validate(json.loads(record_path.read_text(encoding="utf-8")), vm.config()["uuid"])
    if file_identity(image) != record["image_identity"]:
        raise ValueError("developer storage image was replaced")
    return record


def inspect_image(vm, record):
    _, _, image = paths(vm)
    if file_identity(image) != record["image_identity"]:
        raise ValueError("developer storage image was replaced")
    info = json.loads(subprocess.check_output(
        [str(vm.IMAGE), "info", "--output=json", str(image)], creationflags=vm.FLAGS))
    if (info.get("format") != "qcow2" or info.get("virtual-size") != SIZE
            or info.get("backing-filename") or info.get("encrypted", False)
            or info.get("snapshots") or info.get("format-specific", {}).get("data", {}).get("data-file")):
        raise ValueError("unexpected developer disk format, size or external dependency")
    if file_identity(image) != record["image_identity"]:
        raise ValueError("developer storage image changed during inspection")
    return info


def prepare(vm):
    # The VM launch lock is held for the entire running VM lifetime. New image
    # creation has a separate lock and never alters any attached image.
    with vm.file_guard(vm.VM / "development-storage.lock"):
        previous = load(vm)
        if previous is not None:
            if previous["stage"] != "ready":
                raise ValueError("developer image creation is uncertain; never replay creation")
            # A running guest may hold it open; idempotent preparation does not
            # bypass that lock with qemu-img -U or change the original object.
            return previous
        directory, record_path, image = paths(vm)
        if shutil.disk_usage(vm.VM).free < SIZE + 1024**3:
            raise ValueError("insufficient host capacity for developer disk plus 1 GiB headroom")
        directory.mkdir(exist_ok=True)
        vm.private_path(directory, directory=True)
        fd = os.open(image, os.O_CREAT | os.O_EXCL | os.O_RDWR, 0o600)
        try:
            os.fsync(fd)
        finally:
            os.close(fd)
        vm.private_path(image)
        token = uuid.uuid4()
        record = dict(format=1, vm_uuid=vm.config()["uuid"], token=str(token),
                      serial="AETHBLD_" + token.hex[:12], bytes=SIZE,
                      fs_uuid=str(uuid.uuid4()), stage="creating", image_identity=file_identity(image))
        vm.write_state(record_path, record)
        vm.private_path(record_path)
        subprocess.run([str(vm.IMAGE), "create", "-f", "qcow2", str(image), str(SIZE)],
                       check=True, creationflags=vm.FLAGS)
        inspect_image(vm, record)
        with image.open("r+b") as output:
            os.fsync(output.fileno())
        record["stage"] = "ready"
        vm.write_state(record_path, record)
        vm.private_path(record_path)
        return record


def boot_arguments(vm, install):
    record = load(vm)
    if record is None:
        return []
    if install:
        raise ValueError("registered developer disk must be detached explicitly before carrier reinstallation")
    if record["stage"] != "ready":
        raise ValueError("developer disk creation is uncertain; boot attachment refused")
    inspect_image(vm, record)
    _, _, image = paths(vm)
    # JSON avoids QEMU's comma-separated drive option parsing for host paths.
    block = {"driver": "qcow2", "node-name": "aetheri_development_work",
             "file": {"driver": "file", "filename": str(image)}}
    return ["-blockdev", json.dumps(block, separators=(",", ":")), "-device",
            "virtio-blk-pci,id=aetheri_development_work,drive=aetheri_development_work,serial=" + record["serial"]]


def guest_profile(record):
    if record["stage"] != "ready":
        raise ValueError("developer image is not ready for guest enrollment")
    return {k: record[k] for k in ("format", "vm_uuid", "token", "serial", "bytes", "fs_uuid")}


def guest_action(vm, action):
    if action not in ("initialize-guest", "mount-guest"):
        raise ValueError("unknown developer storage guest action")
    record = load(vm, required=True)
    profile = guest_profile(record)
    if not vm.running():
        raise ValueError("developer guest identity is not verified")
    _, _, image = paths(vm)
    blocks = vm.qmp("query-block")
    matches = [block for block in blocks if block.get("inserted", {}).get("node-name") == "aetheri_development_work"]
    if (len(matches) != 1 or matches[0]["inserted"].get("drv") != "qcow2"
            or matches[0]["inserted"].get("ro") is not False
            or Path(matches[0]["inserted"].get("file", "")).resolve() != image.resolve()):
        raise ValueError("original developer disk is not attached to this VM; no guest action")
    actual_serial = vm.qmp("qom-get", {"path":"/machine/peripheral/aetheri_development_work", "property":"serial"})
    if actual_serial != record["serial"]:
        raise ValueError("attached developer disk serial differs")
    source = (Path(__file__).parent / "development_storage_guest.py").read_bytes()
    # Send code/data through the existing protected SSH stdin. No dynamic shell
    # arguments, host mounts, files from a candidate guest or retry loop.
    program = "import base64,json\nns={'__name__':'aetheri_storage_tool'}\n"
    program += "exec(compile(base64.b64decode(" + repr(base64.b64encode(source).decode()) + "),'development_storage_guest.py','exec'),ns)\n"
    program += "expected=json.loads(" + repr(json.dumps(profile)) + ")\n"
    if action == "initialize-guest":
        program += "result=ns['initialize'](expected)\n"
    else:
        program += ("with ns['locked']():\n record=ns['load']()\n"
                    " if record['profile'] != expected: raise RuntimeError('guest registration differs from original host profile')\n"
                    " result=ns['mount_selected'](record)\n")
    program += "print(json.dumps(result,indent=2))\n"
    return subprocess.run([sys.executable, str(Path(__file__).parent / "run_vm.py"), "ssh", "sudo -n python3 -"],
                          input=program, text=True, check=True, creationflags=vm.FLAGS)


def main():
    import run_vm as vm
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("prepare", "status", "guest-profile", "initialize-guest", "mount-guest"))
    args = parser.parse_args()
    if args.action in ("initialize-guest", "mount-guest"):
        guest_action(vm, args.action)
        return
    if args.action == "prepare":
        result = prepare(vm)
    else:
        result = load(vm, required=True)
        if args.action == "guest-profile":
            result = guest_profile(result)
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
