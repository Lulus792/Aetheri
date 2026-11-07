"""Root-only setup/mount of the host-selected disposable developer work disk.

Never called by normal Aetheri apps. No root resizing, disk discovery by content,
automatic formatting on boot, production allocation or clone authentication.
"""
import argparse
from contextlib import contextmanager
import fcntl
import json
import os
from pathlib import Path
import stat
import struct
import subprocess
import uuid

STATE = Path("/var/lib/aetheri-development-storage")
MOUNT = Path("/mnt/aetheri")
SIZE = 8 * 1024**3


def run(*args, pass_fds=()):
    return subprocess.check_output(args, text=True, pass_fds=pass_fds).strip()


def validate_profile(profile):
    if (not isinstance(profile, dict) or set(profile) != {"format", "vm_uuid", "token", "serial", "bytes", "fs_uuid"}
            or type(profile["format"]) is not int or profile["format"] != 1
            or type(profile["bytes"]) is not int or profile["bytes"] != SIZE):
        raise ValueError("invalid developer storage profile")
    for key in ("vm_uuid", "token", "fs_uuid"):
        value = profile[key]
        if not isinstance(value, str) or str(uuid.UUID(value)) != value:
            raise ValueError("invalid developer storage identity")
    if (uuid.UUID(profile["token"]).version != 4 or uuid.UUID(profile["fs_uuid"]).version != 4
            or profile["serial"] != "AETHBLD_" + uuid.UUID(profile["token"]).hex[:12]):
        raise ValueError("invalid developer storage serial")
    return profile


def require_carrier(profile):
    if os.geteuid() != 0 or run("getenforce") != "Enforcing":
        raise RuntimeError("requires root in the enforcing disposable carrier")
    if Path("/sys/class/dmi/id/product_uuid").read_text().strip().lower() != profile["vm_uuid"]:
        raise RuntimeError("foreign VM identity")


def metadata(path, directory=False):
    info = path.lstat()
    if (path.is_symlink() or info.st_uid != 0 or info.st_gid != 0
            or stat.S_IMODE(info.st_mode) != (0o700 if directory else 0o600)
            or not (stat.S_ISDIR(info.st_mode) if directory else stat.S_ISREG(info.st_mode))
            or (not directory and info.st_nlink != 1)):
        raise RuntimeError("untrusted developer storage state")


@contextmanager
def locked():
    if os.geteuid() != 0:
        raise RuntimeError("root required")
    STATE.mkdir(mode=0o700, exist_ok=True)
    metadata(STATE, directory=True)
    fd = os.open(STATE / "writer.lock", os.O_RDWR | os.O_CREAT | os.O_NOFOLLOW | os.O_CLOEXEC, 0o600)
    try:
        metadata(STATE / "writer.lock")
        fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
        yield
    finally:
        os.close(fd)


def save(record):
    temporary = STATE / "registration.next"
    fd = os.open(temporary, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_CLOEXEC, 0o600)
    with os.fdopen(fd, "w") as output:
        json.dump(record, output, indent=2)
        output.write("\n"); output.flush(); os.fsync(output.fileno())
    os.replace(temporary, STATE / "registration.json")
    fd = os.open(STATE, os.O_RDONLY | os.O_DIRECTORY | os.O_CLOEXEC)
    try:
        os.fsync(fd)
    finally:
        os.close(fd)


def load():
    if (STATE / "registration.next").exists():
        raise RuntimeError("developer storage acknowledgement uncertain; preserve pending state")
    metadata(STATE / "registration.json")
    record = json.loads((STATE / "registration.json").read_text())
    if (set(record) != {"profile", "stage", "format_boot"}
            or record["stage"] not in ("formatting", "formatted", "ready")
            or str(uuid.UUID(record["format_boot"])) != record["format_boot"]):
        raise RuntimeError("invalid developer storage state")
    validate_profile(record["profile"])
    require_carrier(record["profile"])
    return record


def selected_device(profile, exclusive=False):
    """Hold the independently selected virtio block object, never an lsblk guess."""
    path = Path("/dev/disk/by-id") / ("virtio-" + profile["serial"])
    fd = os.open(path, os.O_RDWR | os.O_CLOEXEC | (os.O_EXCL if exclusive else 0))
    try:
        info = os.fstat(fd)
        if not stat.S_ISBLK(info.st_mode):
            raise RuntimeError("developer storage is not a block device")
        sysfs = Path("/sys/dev/block") / (str(os.major(info.st_rdev)) + ":" + str(os.minor(info.st_rdev)))
        actual = sysfs.resolve(strict=True)
        if (actual / "partition").exists() or (actual / "serial").read_text().strip() != profile["serial"]:
            raise RuntimeError("developer storage serial/topology differs")
        root = os.stat("/").st_dev
        root_sysfs = (Path("/sys/dev/block") / (str(os.major(root)) + ":" + str(os.minor(root)))).resolve(strict=True)
        if actual == root_sysfs or actual in root_sysfs.parents:
            raise RuntimeError("developer disk cannot contain the carrier root")
        if any((actual / "holders").iterdir()) or any(p.name.startswith(actual.name) for p in actual.iterdir() if (p / "partition").exists()):
            raise RuntimeError("developer disk has holders or partitions")
        size = struct.unpack("Q", fcntl.ioctl(fd, 0x80081272, b"\0" * 8))[0]  # BLKGETSIZE64
        if size != profile["bytes"]:
            raise RuntimeError("developer storage size differs")
        return fd, path, info.st_rdev
    except BaseException:
        os.close(fd)
        raise


def filesystem(fd):
    probe = subprocess.run(["blkid", "-p", "-o", "export", "/proc/self/fd/" + str(fd)],
                           pass_fds=(fd,), capture_output=True, text=True)
    if probe.returncode not in (0, 2):
        raise RuntimeError("developer filesystem probe failed: " + probe.stderr)
    return dict(line.split("=", 1) for line in probe.stdout.splitlines() if "=" in line)


def mount_selected(record):
    profile = record["profile"]
    if record["stage"] not in ("formatted", "ready"):
        raise RuntimeError("format acknowledgement unknown; formatting must not be replayed")
    fd, path, device = selected_device(profile)
    try:
        found = filesystem(fd)
        if found.get("TYPE") != "ext4" or found.get("UUID") != profile["fs_uuid"]:
            raise RuntimeError("developer filesystem differs from independently selected UUID")
        MOUNT.mkdir(mode=0o755, exist_ok=True)
        info = MOUNT.lstat()
        if MOUNT.is_symlink() or not stat.S_ISDIR(info.st_mode) or info.st_uid != 0 or stat.S_IMODE(info.st_mode) != 0o755:
            raise RuntimeError("untrusted developer mountpoint")
        if not os.path.ismount(MOUNT):
            if any(MOUNT.iterdir()):
                raise RuntimeError("developer mountpoint is not empty")
            # Keep the selected object held; mount the held block FD, not a
            # second symlink lookup. This action never formats a filesystem.
            run("mount", "-t", "ext4", "-o", "rw,nosuid,nodev", "/proc/self/fd/" + str(fd), str(MOUNT), pass_fds=(fd,))
        mounted = json.loads(run("findmnt", "--json", "--mountpoint", str(MOUNT), "-o", "TARGET,FSTYPE,OPTIONS,MAJ:MIN"))["filesystems"]
        expected_device = str(os.major(device)) + ":" + str(os.minor(device))
        if len(mounted) != 1 or mounted[0]["maj:min"] != expected_device or mounted[0]["fstype"] != "ext4":
            raise RuntimeError("developer mount is on another device")
        options = set(mounted[0]["options"].split(","))
        if not {"rw", "nosuid", "nodev"} <= options or "noexec" in options or os.stat(MOUNT).st_dev != device:
            raise RuntimeError("developer mount options/identity differ")
        work = MOUNT / "work"
        if not work.exists():
            work.mkdir(mode=0o700)
            os.chown(work, 1000, 1000)
        info = work.lstat()
        if (work.is_symlink() or not stat.S_ISDIR(info.st_mode) or info.st_dev != device
                or info.st_uid != 1000 or info.st_gid != 1000 or stat.S_IMODE(info.st_mode) != 0o700):
            raise RuntimeError("developer work directory differs")
        run("restorecon", "-v", str(MOUNT), str(work))
        for directory in (work, MOUNT):
            directory_fd = os.open(directory, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW | os.O_CLOEXEC)
            try:
                os.fsync(directory_fd)
            finally:
                os.close(directory_fd)
        os.fsync(fd)
        if record["stage"] != "ready":
            record["stage"] = "ready"
            save(record)
        space = os.statvfs(work)
        return dict(profile=profile, mount=str(MOUNT), work=str(work), filesystem=found,
                    available=space.f_bavail * space.f_frsize, enforcing=run("getenforce"))
    finally:
        os.close(fd)


def initialize(profile):
    validate_profile(profile)
    require_carrier(profile)
    with locked():
        if (STATE / "registration.json").exists() or (STATE / "registration.next").exists():
            raise RuntimeError("existing enrollment; use mount/check, never initialize again")
        fd, _, _ = selected_device(profile, exclusive=True)
        try:
            if filesystem(fd):
                raise RuntimeError("developer disk is not blank")
            # The host supplied a newly created private image. Verify every
            # byte before committing permission for this one format attempt.
            os.lseek(fd, 0, os.SEEK_SET)
            remaining = profile["bytes"]
            while remaining:
                chunk = os.read(fd, min(4 * 1024**2, remaining))
                if not chunk or chunk.count(0) != len(chunk):
                    raise RuntimeError("developer disk is not entirely zero-filled")
                remaining -= len(chunk)
            record = dict(profile=profile, stage="formatting", format_boot=Path("/proc/sys/kernel/random/boot_id").read_text().strip())
            save(record)
            # mke2fs must acquire its own exclusive block claim. Keep the same
            # object pinned through a nonexclusive FD while relinquishing our
            # scan claim; do not bypass its mounted/in-use guard with -F.
            replacement = os.open("/proc/self/fd/" + str(fd), os.O_RDWR | os.O_CLOEXEC)
            original_info, next_info = os.fstat(fd), os.fstat(replacement)
            if (original_info.st_dev, original_info.st_ino, original_info.st_rdev) != (next_info.st_dev, next_info.st_ino, next_info.st_rdev):
                os.close(replacement)
                raise RuntimeError("developer block object changed during format handoff")
            os.close(fd)
            fd = replacement
            run("mkfs.ext4", "-q", "-U", profile["fs_uuid"], "-L", "AetheriWork",
                "-m", "0", "-E", "lazy_itable_init=0,lazy_journal_init=0",
                "/proc/self/fd/" + str(fd), pass_fds=(fd,))
            os.fsync(fd)
            found = filesystem(fd)
            if found.get("TYPE") != "ext4" or found.get("UUID") != profile["fs_uuid"]:
                raise RuntimeError("formatted developer disk differs")
            record["stage"] = "formatted"
            save(record)
        finally:
            os.close(fd)
        return mount_selected(record)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("initialize", "mount"))
    parser.add_argument("--profile", type=Path)
    args = parser.parse_args()
    if args.action == "initialize":
        if args.profile is None:
            parser.error("initialize requires the protected host-selected profile")
        metadata(args.profile)
        result = initialize(json.loads(args.profile.read_text()))
    else:
        if args.profile is not None:
            parser.error("mount uses only the original protected registration")
        with locked():
            result = mount_selected(load())
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
