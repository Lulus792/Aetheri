"""Check bounded publication and interrupted rename/fsync recovery in Linux."""
import copy
import errno
import hashlib
import json
import os
from pathlib import Path
import shutil
import stat
import subprocess
import sys
import tempfile

import runtime_contracts as reference


def main():
    executable, build = Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve()
    mounted = sys.argv[3:] == ["--mount"]
    frozen = json.loads((Path(__file__).resolve().parents[1] / "spec/vectors/runtime-v1.json").read_text())[0]
    temporary = Path(tempfile.mkdtemp(prefix="publish-", dir=build))
    assert temporary.resolve().parent == build
    cases = 0
    try:
        def setup():
            nonlocal cases
            cases += 1
            root = temporary / str(cases)
            slot, store = root / "slot", root / "store"
            stage, container = slot / "staging", slot / "publication"
            stage.mkdir(parents=True, mode=0o700)
            slot.chmod(0o700)
            store.mkdir(mode=0o700)
            container.mkdir(mode=0o700)
            manifest = copy.deepcopy(frozen["input"])
            encoded = reference.fixture(manifest)
            record = root / "manifest"
            record.write_bytes(encoded)
            payload = {"bin/probe": b"synthetic-not-an-executable\n", "share/message": b"hello\n"}
            for file in manifest["files"]:
                path = stage / file["path"]
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(payload[file["path"]])
                path.chmod(file["mode"])
            for path in (stage / "bin", stage / "share"):
                path.chmod(0o555)
            return dict(root=root, slot=slot, store=store, record=record, digest=hashlib.sha256(encoded).hexdigest(),
                        payload=payload, manifest=encoded, payload_inode=stage.stat().st_ino, container_inode=container.stat().st_ino)

        def target(item):
            return item["store"] / item["digest"]

        def location(item):
            # Expected identities were retained before any call, independently
            # from the candidate's content. These are test receipts, not public
            # root enrollment or an authority authentication implementation.
            container = target(item) if target(item).exists() else item["slot"] / "publication"
            payload = item["slot"] / "staging" if (item["slot"] / "staging").exists() else container / "payload"
            assert container.stat().st_ino == item["container_inode"]
            assert payload.stat().st_ino == item["payload_inode"]
            return "stored" if container == target(item) else "nested" if payload.parent == container else "stage"

        def invoke(item, fault="none", expected=0, mutation="none", at=None):
            result = subprocess.run([executable, item["root"], item["record"], item["digest"],
                                     location(item) if at is None else at, fault, mutation],
                                    capture_output=True, text=True, timeout=30)
            if fault.endswith("-exit"):
                assert result.returncode == 73, (result.returncode, result.stderr)
                return
            assert result.returncode == 0, result.stderr
            report = tuple(map(int, result.stdout.split()))
            assert (report[0] < 0 if expected is None else report[0] == expected), (fault, mutation, report, result.stderr)
            return report

        def checked(item):
            final = target(item)
            assert final.stat().st_ino == item["container_inode"]
            assert (final / "payload").stat().st_ino == item["payload_inode"]
            assert not (item["slot"] / "staging").exists() and not (item["slot"] / "publication").exists()
            assert {p.name for p in final.iterdir()} == {"manifest", "payload"}
            assert (final / "manifest").read_bytes() == item["manifest"]
            assert stat.S_IMODE((final / "manifest").stat().st_mode) == 0o444
            for name, contents in item["payload"].items():
                assert (final / "payload" / name).read_bytes() == contents

        if mounted:
            assert os.geteuid() == 0
            for name in ("store", "slot/publication", "slot/staging", "slot/staging/bin/probe"):
                item = setup()
                path = item["root"] / name
                before = path.stat()
                subprocess.run(["mount", "--bind", path, path], check=True)
                try:
                    assert path.stat().st_ino == before.st_ino and path.stat().st_dev == before.st_dev
                    invoke(item, expected=-errno.EXDEV)
                    assert not target(item).exists()
                    assert not (item["slot"] / "publication/manifest").exists()
                finally:
                    subprocess.run(["umount", path], check=True)
            print("Publication rejects same-inode parent/container/payload/file binds on distinct mounts.")
            return

        item = setup()
        report = invoke(item)
        assert report[-1] == 2
        checked(item)
        for sealed in (False, True):
            if sealed:
                target(item).chmod(0o555)
                (target(item) / "payload").chmod(0o555)
            report = invoke(item)
            # 2 payload files + manifest; payload/root and both old/new parents
            # are flushed even on a separately started process after visibility.
            assert report[1] == 3 and report[2] >= 2 and report[3] == 2 and report[4] == 1 and report[5] >= 2 and report[6] == 0
            checked(item)

        for fault in ("inner-before", "inner-after", "outer-before", "outer-after", "file-sync", "payload-sync",
                      "old-parent-sync", "container-sync", "container-parent-sync", "store-sync", "writer-close",
                      "write-zero", "write-io", "inner-exit", "outer-exit"):
            item = setup()
            invoke(item, fault=fault, expected=-errno.EIO)
            report = invoke(item)
            assert report[3] == 2 and report[4] == 1
            checked(item)
        for fault in ("write-eintr", "write-short"):
            item = setup()
            invoke(item, fault=fault)
            checked(item)

        for change in ("unknown-target", "target-symlink", "bad-manifest", "extra-entry", "manifest-symlink",
                       "manifest-hardlink", "manifest-xattr", "payload-content", "root-mode", "bad-name",
                       "rebound-stage", "rebound-container"):
            item = setup()
            container = item["slot"] / "publication"
            path = container / "manifest"
            if change == "unknown-target":
                shutil.copytree(container, target(item))
            elif change == "target-symlink": target(item).symlink_to(container, target_is_directory=True)
            elif change in ("bad-manifest", "manifest-hardlink", "manifest-xattr"):
                path.write_bytes(b"X" * len(item["manifest"]) if change == "bad-manifest" else item["manifest"])
                if change == "manifest-xattr": os.setxattr(path, "user.unwanted", b"metadata")
                path.chmod(0o444)
                if change == "manifest-hardlink": os.link(path, item["root"] / "alias")
            elif change == "manifest-symlink": path.symlink_to(item["record"])
            elif change == "extra-entry": (container / "junk").write_bytes(b"preserve")
            elif change == "payload-content":
                payload = item["slot"] / "staging/bin/probe"
                payload.chmod(0o755)
                payload.write_bytes(b"X" * len(item["payload"]["bin/probe"]))
                payload.chmod(0o555)
            elif change == "root-mode": container.chmod(0o755)
            before_target = target(item).lstat() if target(item).exists() or target(item).is_symlink() else None
            mutation = change if change in ("bad-name", "rebound-stage", "rebound-container") else "none"
            invoke(item, mutation=mutation, expected=None, at="stage")
            if before_target is not None: assert target(item).lstat() == before_target
            else: assert not target(item).exists()
            assert (item["slot"] / "staging").exists() and container.exists()

        # A visible final name cannot repair missing metadata or absorb a second
        # local payload. Such a state was not produced by this protocol.
        for change in ("missing-manifest", "stage-beside-final", "extra-final-entry"):
            item = setup()
            invoke(item)
            if change == "missing-manifest": (target(item) / "manifest").unlink()
            elif change == "stage-beside-final": (item["slot"] / "staging").mkdir(mode=0o700)
            else: (target(item) / "unexpected").mkdir()
            invoke(item, expected=None, at="stored")
            if change == "missing-manifest": assert not (target(item) / "manifest").exists()
        item = setup()
        invoke(item)
        shutil.copytree(target(item), item["root"] / "copied-store")
        replacement_inode = (item["root"] / "copied-store").stat().st_ino
        assert replacement_inode != item["container_inode"]
        invoke(item, mutation="copied-final", expected=-errno.ESTALE)
        assert target(item).stat().st_ino == replacement_inode
        assert (item["root"] / "detached-store").stat().st_ino == item["container_inode"]
        assert (target(item) / "manifest").read_bytes() == item["manifest"]
        for name, contents in item["payload"].items(): assert (target(item) / "payload" / name).read_bytes() == contents
        print(f"Native publication: {cases} fixtures; no-replace moves, identity, namespace conflicts, IO faults, process exit and full resync passed.")
    finally:
        for directory, _, _ in os.walk(temporary, followlinks=False):
            if not Path(directory).is_symlink(): Path(directory).chmod(0o700)
        shutil.rmtree(temporary)


if __name__ == "__main__":
    main()
