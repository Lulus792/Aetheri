"""Controls for the private developer-tool archive and prefix boundary."""
import hashlib
from pathlib import Path
import stat
import tempfile
import unittest
from unittest.mock import patch

import prepare_desktop_toolchain as tools


def archive(rows):
    out = bytearray()
    for name, mode, body, ino, links in [*rows, ('TRAILER!!!', 0, b'', 0, 1)]:
        name = name.encode()+b'\0'
        fields = [ino, mode, 0, 0, links, 0, len(body), 0, 0, 0, 0, len(name), 0]
        out.extend(b'070701'+b''.join(('%08x'%v).encode() for v in fields)+name)
        out.extend(b'\0'*(-len(out)%4))
        out.extend(body)
        out.extend(b'\0'*(-len(out)%4))
    return bytes(out)


class Archive(unittest.TestCase):
    def setUp(self):
        self.body = b'original body'
        self.entry = dict(mode=0o755, size=len(self.body), sha256=hashlib.sha256(self.body).hexdigest())

    def test_original_hardlink_body_copied_without_links(self):
        raw = archive([('usr/bin/selected', stat.S_IFREG|0o755, b'', 42, 2),
                       ('usr/bin/other', stat.S_IFREG|0o755, self.body, 42, 2)])
        self.assertEqual(tools.select(raw, {'usr/bin/selected': self.entry}, {}), {'usr/bin/selected': self.body})

    def test_different_inode_cannot_supply_a_missing_body(self):
        raw = archive([('usr/bin/selected', stat.S_IFREG|0o755, b'', 42, 2),
                       ('usr/bin/other', stat.S_IFREG|0o755, self.body, 43, 2)])
        with self.assertRaisesRegex(ValueError, 'digest'):
            tools.select(raw, {'usr/bin/selected': self.entry}, {})

    def test_changed_hardlink_header_rejected(self):
        raw = archive([('usr/bin/selected', stat.S_IFREG|0o755, b'', 42, 2),
                       ('usr/bin/other', stat.S_IFREG|0o644, self.body, 42, 2)])
        with self.assertRaisesRegex(ValueError, 'header class'):
            tools.select(raw, {'usr/bin/selected': self.entry}, {})

    def test_duplicate_bodies_rejected(self):
        raw = archive([('usr/bin/selected', stat.S_IFREG|0o755, self.body, 42, 2),
                       ('usr/bin/other', stat.S_IFREG|0o755, self.body, 42, 2)])
        with self.assertRaisesRegex(ValueError, 'multiple hardlink'):
            tools.select(raw, {'usr/bin/selected': self.entry}, {})

    def test_digest_and_alias_are_independent(self):
        raw = archive([('usr/bin/selected', stat.S_IFREG|0o755, self.body, 42, 1),
                       ('usr/lib64/lib.so', stat.S_IFLNK|0o777, b'foreign.so', 43, 1)])
        with self.assertRaisesRegex(ValueError, 'alias'):
            tools.select(raw, {'usr/bin/selected': self.entry}, {'usr/lib64/lib.so': 'original.so'})
        changed = {**self.entry, 'sha256': '0'*64}
        with self.assertRaisesRegex(ValueError, 'digest'):
            tools.select(raw, {'usr/bin/selected': changed}, {})

    def test_names_and_tail_are_not_ignored(self):
        for name in ('../escape', '/absolute', 'usr/../escape', 'usr//double', 'usr/./dot'):
            with self.subTest(name=name), self.assertRaisesRegex(ValueError, 'noncanonical'):
                list(tools.members(archive([(name, stat.S_IFREG|0o644, b'', 1, 1)])))
        raw = archive([('usr/a', stat.S_IFREG|0o644, b'', 1, 1)]*2)
        with self.assertRaisesRegex(ValueError, 'duplicate'):
            list(tools.members(raw))
        with self.assertRaisesRegex(ValueError, 'after tool archive'):
            list(tools.members(archive([])+b'x'))

    def test_truncated_body_rejected(self):
        raw = archive([('usr/a', stat.S_IFREG|0o644, self.body, 1, 1)])
        with self.assertRaises(ValueError):
            list(tools.members(raw[:120]))


class Prefix(unittest.TestCase):
    def test_real_profile_is_bounded(self):
        v, _ = tools.profile()
        self.assertEqual(len(v['packages']), 9)
        self.assertIn('usr/include/c++/16/vector', {e['path'] for e in v['files']})
        self.assertNotIn('usr/bin/cpack', {e['path'] for e in v['files']})

    def test_unlisted_file_cannot_be_a_valid_prefix(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            (root/'unlisted').write_bytes(b'x')
            with self.assertRaisesRegex(ValueError, 'unlisted tool file'):
                tools.check(root)

    def test_privileged_prepare_rejected_before_reading_inputs(self):
        with patch.object(tools.sys, 'platform', 'linux'), patch.object(tools.os, 'geteuid', return_value=0, create=True):
            with self.assertRaisesRegex(ValueError, 'ordinary native build account'):
                tools.prepare('missing', 'missing', 'missing')


if __name__ == '__main__':
    unittest.main()
