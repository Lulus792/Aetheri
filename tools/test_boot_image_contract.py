"""Independent malformed-image and no-overwrite checks; never launch a VM."""
import gzip
import hashlib
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import test_boot_probe as boot


def newc(entries):
    result = bytearray()
    for index, (name, body, mode, uid) in enumerate([*entries, ("TRAILER!!!", b"", 0, 0)], 1):
        encoded = name.encode() + b"\0"
        values = (index, mode, uid, 0, 1, 0, len(body), 0, 0, 0, 0, len(encoded), 0)
        result.extend(b"070701" + "".join(f"{value:08x}" for value in values).encode())
        result.extend(encoded)
        result.extend(b"\0" * (-len(result) % 4))
        result.extend(body)
        result.extend(b"\0" * (-len(result) % 4))
    return bytes(result)


class BootImageContract(unittest.TestCase):
    def setUp(self):
        out = boot.ROOT / "out"
        out.mkdir(exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(prefix="boot-contract-", dir=out)
        self.addCleanup(self.temp.cleanup)
        self.image = Path(self.temp.name)
        self.entries = [("init", b"usr/lib/systemd/systemd", 0o120777, 0), ("usr/lib/systemd/systemd", b"fixture, not an ELF", 0o100755, 0)]

    def write_image(self, entries=None, tail=b""):
        (self.image / "vmlinuz").write_bytes(b"fixture, not a kernel")
        (self.image / "cmdline.txt").write_text("fixture\n")
        (self.image / "initramfs.cpio.gz").write_bytes(gzip.compress(newc(self.entries if entries is None else entries) + tail, mtime=0))
        m = dict(format=1, nonce="a" * 32, kernel_release="fixture", root={"init": {"symlink": "usr/lib/systemd/systemd"}, "usr/lib/systemd/systemd": {"sha256": hashlib.sha256(self.entries[1][1]).hexdigest(), "mode": "0o755"}}, outputs={n: boot.sha(self.image/n) for n in ("vmlinuz", "initramfs.cpio.gz", "cmdline.txt")})
        (self.image / "manifest.json").write_text(json.dumps(m))
        return m

    def test_valid_archive_is_inspected_without_execution(self):
        self.write_image()
        self.assertEqual(boot.inspect(self.image)["nonce"], "a" * 32)

    def test_changed_kernel_initramfs_and_commandline_reject(self):
        for name in ("vmlinuz", "initramfs.cpio.gz", "cmdline.txt"):
            self.write_image()
            with (self.image/name).open("ab") as stream:
                stream.write(b"changed")
            with self.assertRaises(ValueError):
                boot.inspect(self.image)

    def test_bad_archive_names_duplicates_links_ownership_and_types_reject(self):
        invalid = [
            [("/absolute", b"", 0o100644, 0)],
            [("../escape", b"", 0o100644, 0)],
            [*self.entries, self.entries[0]],
            [("dir", b"", 0o040755, 0), ("dir", b"", 0o040755, 0)],
            [("init", b"/outside", 0o120777, 0)],
            [("init", b"../outside", 0o120777, 0)],
            [("init", b"", 0o100755, 1000)],
            [("init", b"", 0o020600, 0)],
        ]
        for entries in invalid:
            self.write_image(entries)
            with self.assertRaises(ValueError):
                boot.inspect(self.image)

    def test_unexamined_archive_tail_and_manifest_disagreement_reject(self):
        self.write_image(tail=b"unexamined")
        with self.assertRaises(ValueError):
            boot.inspect(self.image)
        self.write_image([self.entries[0], (self.entries[1][0], b"different", 0o100755, 0)])
        with self.assertRaises(ValueError):
            boot.inspect(self.image)

    def test_attempt_directory_is_never_reused_or_launched(self):
        self.write_image()
        attempt = self.image / "attempt"
        attempt.mkdir()
        (attempt / "original").write_text("keep")
        with patch.object(boot, "host_tools", return_value={}), patch.object(boot.subprocess, "Popen") as spawn:
            with self.assertRaises(FileExistsError):
                boot.run(self.image, attempt)
            spawn.assert_not_called()
        self.assertEqual((attempt / "original").read_text(), "keep")


if __name__ == "__main__":
    unittest.main(verbosity=2)
