"""Audio profile and extraction-data counterexamples; no daemon/device use."""
import stat
import os
from pathlib import Path
import tempfile
import unittest
import prepare_audio as audio


def archive(entries):
    result = bytearray()
    for name, mode, data in [*entries, ("TRAILER!!!", 0, b"")]:
        name = name.encode() + b"\0"
        values = [1, mode, 1000, 1000, 1, 0, len(data), 0, 0, 0, 0, len(name), 0]
        result += b"070701" + b"".join(f"{v:08x}".encode() for v in values) + name
        result += b"\0" * (-len(result) % 4)
        result += data
        result += b"\0" * (-len(result) % 4)
    result += b"\0" * (-len(result) % 512)
    return bytes(result)


class Inputs(unittest.TestCase):
    @unittest.skipUnless(hasattr(os, 'memfd_create'), 'Linux sealed snapshots')
    def test_snapshot_survives_source_replacement_and_refuses_writes(self):
        with tempfile.TemporaryDirectory() as directory:
            p = Path(directory)/'input.rpm';p.write_bytes(b'original')
            pin = {'size': 8, 'sha256': audio.sha(b'original')}
            with audio.sealed_package(p, pin) as fd:
                self.assertEqual(stat.S_IMODE(os.fstat(fd).st_mode), 0o600)
                p.write_bytes(b'replaced')
                self.assertEqual(os.pread(fd, 8, 0), b'original')
                with self.assertRaises(PermissionError): os.pwrite(fd, b'X', 0)
                with self.assertRaises(PermissionError): os.ftruncate(fd, 0)
            with self.assertRaises(OSError): os.fstat(fd)

    def test_actual_profile_has_exact_packages_and_separate_test_roles(self):
        pins, digest = audio.profile()
        self.assertEqual(len(digest), 64)
        self.assertEqual(len(pins["packages"]), 4)
        self.assertEqual(len(pins["files"]), 252)
        roles = {e["path"]: e["role"] for e in pins["files"]}
        self.assertEqual(roles["usr/bin/pw-link"], "test-tool")
        self.assertNotIn("usr/lib64/spa-0.2/bluez5/libspa-bluez5.so", roles)

    def test_canonical_rejects_escape_alias_and_absolute_member(self):
        for path in ("/usr/a", "usr/../etc/a", "usr//a", "usr/./a", "usr/a\\b", "usr/a\0b"):
            with self.subTest(path=path), self.assertRaises(ValueError):
                audio.canonical(path)

    def test_regular_directory_and_link_are_only_borrowed_data(self):
        entries = [("./usr", stat.S_IFDIR | 0o755, b""), ("./usr/a", stat.S_IFREG | 0o644, b"payload"), ("./usr/link", stat.S_IFLNK | 0o777, b"/outside")]
        actual = [(name, mode, bytes(data)) for name, mode, data in audio.cpio_members(archive(entries))]
        self.assertEqual(actual, [(n.removeprefix("./"), m, d) for n, m, d in entries])
        # The signed Fedora archive also contains an unselected locale name.
        member = archive([('usr/share/locale/sr@latin/LC_MESSAGES/pipewire.mo', stat.S_IFREG, b'locale')])
        self.assertEqual(len(list(audio.cpio_members(member))), 1)

    def test_duplicate_or_escaping_archive_entries_fail(self):
        for entries in ([('usr/a', stat.S_IFREG, b'a')]*2, [('usr/../escape', stat.S_IFREG, b'a')], [('/absolute', stat.S_IFREG, b'a')]):
            with self.subTest(entries=entries), self.assertRaises(ValueError):
                list(audio.cpio_members(archive(entries)))

    def test_missing_trailer_truncation_and_nonzero_suffix_fail(self):
        raw = archive([('usr/a', stat.S_IFREG, b'abc')])
        for value in (raw[:30], raw[:117], raw[:116] + b'x', raw + b'junk'):
            with self.subTest(size=len(value)), self.assertRaises(ValueError):
                list(audio.cpio_members(value))

    def test_header_and_name_bounds_fail_before_payload_iteration(self):
        raw = bytearray(archive([('usr/a', stat.S_IFREG, b'abc')]))
        for offset, value in ((6, b'zzzzzzzz'), (94, b'ffffffff'), (54, b'ffffffff')):
            changed = raw.copy(); changed[offset:offset + 8] = value
            with self.subTest(offset=offset), self.assertRaises(ValueError):
                list(audio.cpio_members(bytes(changed)))


if __name__ == '__main__':
    unittest.main()
