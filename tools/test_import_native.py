"""Run real bounded payload staging, recovery and containment probes in Linux."""
import copy
import errno
import hashlib
import json
import os
from pathlib import Path
import shutil
import socket
import stat
import subprocess
import sys
import tempfile

import runtime_contracts as reference


def main():
    executable, build = Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve()
    mount_mode = sys.argv[3:] == ["--mount"]
    frozen = json.loads((Path(__file__).resolve().parents[1] / "spec/vectors/runtime-v1.json").read_text())[0]
    temporary = Path(tempfile.mkdtemp(prefix="import-", dir=build))
    if temporary.resolve().parent != build:
        raise ValueError("test directory outside build")
    cases = 0
    try:
        def setup(manifest=None, payload=None):
            nonlocal cases
            cases += 1
            root = temporary / str(cases)
            source, stage, record = root / "source", root / "stage", root / "manifest"
            source.mkdir(parents=True, mode=0o700)
            stage.mkdir(mode=0o700)
            manifest = copy.deepcopy(frozen["input"] if manifest is None else manifest)
            payload = {"bin/probe": b"synthetic-not-an-executable\n", "share/message": b"hello\n"} if payload is None else payload
            encoded = reference.fixture(manifest)
            record.write_bytes(encoded)
            for file in manifest["files"]:
                path = source / file["path"]
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(payload[file["path"]])
                path.chmod(file["mode"])
            for path, directories, _ in os.walk(source, topdown=False):
                Path(path).chmod(0o555)
            return source, stage, record, hashlib.sha256(encoded).hexdigest(), manifest, payload

        def invoke(item, action="stage", fault="none", expected=0):
            source, stage, record, digest, _, _ = item
            result = subprocess.run([executable, action, source, stage, record, digest, fault],
                                    check=True, capture_output=True, text=True, timeout=40)
            report = tuple(map(int, result.stdout.split()))
            if (expected is None and report[0] >= 0) or (expected is not None and report[0] != expected):
                raise AssertionError((action, fault, report, result.stderr))
            return report

        def snapshot(root):
            result = {}
            for directory, directories, files in os.walk(root, followlinks=False):
                for name in ["."] + directories + files:
                    path = Path(directory) / name
                    info = path.lstat()
                    contents = hashlib.sha256(path.read_bytes()).hexdigest() if stat.S_ISREG(info.st_mode) else None
                    result[path.relative_to(root).as_posix()] = (
                        info.st_dev, info.st_ino, info.st_uid, info.st_gid, info.st_mode, info.st_nlink,
                        info.st_size, info.st_mtime_ns, info.st_ctime_ns, contents)
            return result

        def checked(item):
            _, stage, _, _, manifest, payload = item
            assert stat.S_IMODE(stage.stat().st_mode) == 0o700
            expected_files = {file["path"] for file in manifest["files"]}
            actual_files = {path.relative_to(stage).as_posix() for path in stage.rglob("*") if path.is_file()}
            assert actual_files == expected_files
            for file in manifest["files"]:
                path = stage / file["path"]
                assert path.read_bytes() == payload[file["path"]]
                assert stat.S_IMODE(path.stat().st_mode) == file["mode"] and path.stat().st_nlink == 1
            assert all(stat.S_IMODE(path.stat().st_mode) == 0o555 for path in stage.rglob("*") if path.is_dir())

        if mount_mode:
            assert os.geteuid() == 0
            for relative in ("bin/probe", "bin"):
                item = setup()
                target = item[0] / relative
                original = target.stat()
                subprocess.run(["mount", "--bind", target, target], check=True)
                try:
                    assert target.stat().st_dev == original.st_dev and target.stat().st_ino == original.st_ino
                    invoke(item, expected=-errno.EXDEV)
                    assert not list(item[1].iterdir())
                finally:
                    subprocess.run(["umount", target], check=True)
            item = setup()
            path = item[0] / "bin/probe"
            path.parent.chmod(0o755)
            path.unlink()
            os.mknod(path, stat.S_IFCHR | 0o555, os.makedev(1, 3))
            path.parent.chmod(0o555)
            assert invoke(item, expected=-errno.EBADMSG)[3] == 0
            assert not list(item[1].iterdir())
            print("Same-inode file/directory bind mounts and listed device rejection passed.")
            return

        item = setup()
        assert item[2].read_bytes().hex() == frozen["hex"] and item[3] == frozen["sha256"]
        original_source = snapshot(item[0])
        invoke(item)
        assert snapshot(item[0]) == original_source
        checked(item)
        before = {path.relative_to(item[1]).as_posix(): path.stat().st_ino for path in item[1].rglob("*")}
        invoke(item, expected=-errno.EEXIST)
        assert before == {path.relative_to(item[1]).as_posix(): path.stat().st_ino for path in item[1].rglob("*")}
        assert invoke(item, action="sync")[1:3] == (2, 3)

        for change in ("copied-content", "extra-entry", "file-mode", "directory-mode", "file-symlink"):
            item = setup()
            invoke(item)
            root = item[1]
            path = root / "bin/probe"
            if change == "copied-content":
                path.chmod(0o755)
                path.write_bytes(b"X" * 28)
                path.chmod(0o555)
            elif change == "extra-entry": (root / "unlisted").write_bytes(b"extra")
            elif change == "file-mode": path.chmod(0o755)
            elif change == "directory-mode": path.parent.chmod(0o755)
            elif change == "file-symlink":
                path.parent.chmod(0o755)
                path.unlink()
                path.symlink_to("../share/message")
                path.parent.chmod(0o555)
            before_sync = snapshot(root)
            invoke(item, action="sync", expected=None)
            assert snapshot(root) == before_sync

        for fault in ("read-eintr", "read-short", "write-eintr", "write-short"):
            item = setup()
            invoke(item, fault=fault)
            checked(item)
        for fault, error in (("read-eof", errno.EBADMSG), ("read-io", errno.EIO),
                             ("write-zero", errno.EIO), ("write-io", errno.EIO),
                             ("file-sync", errno.EIO), ("root-sync", errno.EIO),
                             ("writer-close", errno.EIO), ("copy-corrupt", errno.EBADMSG),
                             ("source-rebind", errno.ESTALE)):
            if fault == "source-rebind":
                manifest = copy.deepcopy(frozen["input"])
                data = b"A" * 131073
                manifest["files"][0].update(size=len(data), sha256=hashlib.sha256(data).hexdigest())
                item = setup(manifest, {"bin/probe": data, "share/message": b"hello\n"})
            else:
                item = setup()
            invoke(item, fault=fault, expected=-error)
            assert list(item[1].iterdir()), "uncertain partial stage was removed"
            if fault == "source-rebind":
                assert (item[1] / "bin/probe").read_bytes() == item[5]["bin/probe"], "copy reopened substituted source path"
            invoke(item, expected=-errno.EEXIST)
            if fault == "root-sync":
                assert invoke(item, action="sync")[1:3] == (2, 3)
                checked(item)

        for change in ("extra-file", "extra-dir", "missing", "symlink", "dir-symlink", "fifo", "socket",
                       "hardlink", "source-writable", "root-writable", "dir-writable", "xattr",
                       "root-xattr", "directory-xattr", "content", "oversize", "wrong-id", "stage-entry", "stage-mode"):
            item = setup()
            source, stage, record, digest, manifest, payload = item
            path = source / "bin/probe"
            source.chmod(0o755)
            path.parent.chmod(0o755)
            if change == "extra-file": (source / "unlisted").write_bytes(b"unexpected")
            elif change == "extra-dir": (source / "empty").mkdir()
            elif change == "missing": path.unlink()
            elif change == "symlink":
                path.unlink()
                path.symlink_to("../share/message")
            elif change == "dir-symlink":
                path.unlink()
                path.parent.rmdir()
                path.parent.symlink_to("share", target_is_directory=True)
            elif change == "fifo":
                path.unlink()
                os.mkfifo(path, 0o555)
            elif change == "socket":
                path.unlink()
                with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as local_socket:
                    local_socket.bind(str(path))
            elif change == "hardlink": os.link(path, temporary / f"alias-{cases}")
            elif change == "source-writable": path.chmod(0o755)
            elif change == "xattr":
                path.chmod(0o755)
                os.setxattr(path, "user.unwanted", b"payload metadata")
                path.chmod(0o555)
            elif change == "root-xattr": os.setxattr(source, "user.unwanted", b"root metadata")
            elif change == "directory-xattr": os.setxattr(path.parent, "user.unwanted", b"directory metadata")
            elif change in ("content", "oversize"):
                path.chmod(0o755)
                path.write_bytes(b"X" * (28 if change == "content" else 29))
                path.chmod(0o555)
            elif change == "wrong-id": item = (source, stage, record, "00" * 32, manifest, payload)
            elif change == "stage-entry": (stage / "existing").write_bytes(b"preserve")
            elif change == "stage-mode": stage.chmod(0o755)
            if change != "dir-symlink": path.parent.chmod(0o755 if change == "dir-writable" else 0o555)
            source.chmod(0o755 if change == "root-writable" else 0o555)
            report = invoke(item, expected=None)
            if change in ("symlink", "dir-symlink", "fifo", "socket"):
                assert report[3] == 0, "nonregular object reached content open"
            if change == "content":
                assert list(stage.iterdir()), "bad copied content must remain unadmitted"
            elif change == "stage-entry": assert (stage / "existing").read_bytes() == b"preserve"
            else: assert not list(stage.iterdir()), change

        for paths, data in ((["empty"], b""), (["/".join(["a"] * 127 + ["f"])], b"deep"),
                            ([f"f{n:02d}" for n in range(32)], b"many"),
                            (["first", "second"], b"x" * (16 * 1024**2))):
            manifest = copy.deepcopy(frozen["input"])
            manifest["files"] = [dict(path=path, mode=0o444, size=len(data), sha256=hashlib.sha256(data).hexdigest()) for path in paths]
            manifest["actions"] = []
            item = setup(manifest, {path: data for path in paths})
            invoke(item)
            checked(item)
            invoke(item, action="sync")
        print(f"Native import: {cases} fixtures; inventory, copy/hash, limits, IO faults and separate-invocation resync passed.")
    finally:
        # Only this test's known build-local temporary tree. Never follow fixture
        # symlinks or make any path outside it writable during cleanup.
        for path, directories, _ in os.walk(temporary, followlinks=False):
            if not Path(path).is_symlink(): Path(path).chmod(0o700)
        shutil.rmtree(temporary)


if __name__ == "__main__":
    main()
