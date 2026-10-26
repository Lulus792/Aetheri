"""Cache integrity and no-overwrite behavior; no network or VM execution."""
import hashlib
import io
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import prepare_vm as vm


class CacheInputs(unittest.TestCase):
    def setUp(self):
        root = vm.ROOT / "out"
        root.mkdir(exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(prefix="vm-inputs-", dir=root)
        self.cache = Path(self.temp.name)
        if self.cache.resolve().parent != root.resolve():
            raise ValueError("temporary directory outside workspace")
        self.addCleanup(self.temp.cleanup)
        self.record = dict(name="fixture.bin", size=6, algorithm="sha256",
                           digest=hashlib.sha256(b"hello\n").hexdigest(), url="https://invalid.example/")

    def test_valid_cache_is_reused_without_network(self):
        (self.cache / "fixture.bin").write_bytes(b"hello\n")
        with patch("urllib.request.urlopen", side_effect=AssertionError("network")):
            self.assertEqual(vm.fetch(self.cache, self.record), "already verified")

    def test_changed_same_size_and_short_inputs_are_rejected(self):
        path = self.cache / "fixture.bin"
        for data in (b"jello\n", b"hello"):
            path.write_bytes(data)
            with self.assertRaises(ValueError):
                vm.verify(path, self.record)

    def test_partial_download_is_not_overwritten(self):
        part = self.cache / "fixture.bin.part"
        part.write_bytes(b"previous evidence")
        with self.assertRaises(FileExistsError):
            vm.fetch(self.cache, self.record)
        self.assertEqual(part.read_bytes(), b"previous evidence")

    def test_path_escape_is_rejected(self):
        for name in ("../fixture", "/fixture", "a/b", "a\\b", ".", ".."):
            with self.assertRaises(ValueError):
                vm.fetch(self.cache, {**self.record, "name": name})

    def test_download_is_published_only_after_verification(self):
        with patch("urllib.request.urlopen", return_value=io.BytesIO(b"hello\n")):
            self.assertEqual(vm.fetch(self.cache, self.record), "downloaded and verified")
        self.assertEqual((self.cache / "fixture.bin").read_bytes(), b"hello\n")
        self.assertFalse((self.cache / "fixture.bin.part").exists())

    def test_corrupt_download_stays_unpublished(self):
        with patch("urllib.request.urlopen", return_value=io.BytesIO(b"jello\n")):
            with self.assertRaises(ValueError):
                vm.fetch(self.cache, self.record)
        self.assertFalse((self.cache / "fixture.bin").exists())
        self.assertEqual((self.cache / "fixture.bin.part").read_bytes(), b"jello\n")

    def test_oversized_download_stays_unpublished(self):
        with patch("urllib.request.urlopen", return_value=io.BytesIO(b"hello\nextra")):
            with self.assertRaises(ValueError):
                vm.fetch(self.cache, self.record)
        self.assertFalse((self.cache / "fixture.bin").exists())


if __name__ == "__main__":
    unittest.main(verbosity=2)
