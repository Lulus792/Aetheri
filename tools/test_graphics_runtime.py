"""Original archive bytes, hardlinks, bounds and graphics projection controls."""
import copy
import hashlib
import io
import stat
import unittest
from unittest.mock import patch

import prepare_graphics_runtime as graphics


def member(name, body=b'', mode=stat.S_IFREG | 0o644, inode=1, links=1):
    name = name.encode()+b'\0'
    fields = [inode, mode, 0, 0, links, 0, len(body), 0, 0, 0, 0, len(name), 0]
    raw = b'070701'+b''.join(f'{v:08x}'.encode() for v in fields)+name
    raw += b'\0'*((-len(raw)) % 4)
    return raw+body+b'\0'*((-len(body)) % 4)


def archive(*entries):
    return b''.join(entries)+member('TRAILER!!!')


def expected(body, mode=0o644):
    return dict(size=len(body), mode=mode, sha256=hashlib.sha256(body).hexdigest())


class ShortReads(io.BytesIO):
    def read(self, n=-1):
        if n > graphics.CHUNK:
            raise AssertionError('unbounded read')
        return super().read(min(n, 17))


class Graphics(unittest.TestCase):
    def test_streamed_body_exact_with_short_reads(self):
        body = b'graphics\0'*400000
        chunks = []
        data = archive(member('usr/lib64/test.so', body))
        observed = graphics.scan(ShortReads(data), lambda n, v: chunks.append)
        self.assertEqual(b''.join(chunks), body)
        self.assertLessEqual(max(map(len, chunks)), graphics.CHUNK)
        self.assertEqual(graphics.select(observed, {'usr/lib64/test.so': expected(body)}, {}),
                         {'usr/lib64/test.so': ['usr/lib64/test.so']})

    def test_wrong_digest_and_class_rejected(self):
        n = 'usr/lib64/test.so'
        for data in (member(n, b'wrong'), member(n, b'good', stat.S_IFLNK | 0o777)):
            with self.subTest(data=data[:40]), self.assertRaises(ValueError):
                graphics.select(graphics.scan(io.BytesIO(archive(data))), {n: expected(b'good')}, {})

    def test_missing_and_wrong_mode_rejected(self):
        n = 'usr/lib64/test.so'
        for data in (archive(), archive(member(n, b'', stat.S_IFREG | 0o755))):
            with self.assertRaises(ValueError):
                graphics.select(graphics.scan(io.BytesIO(data)), {n: expected(b'')}, {})

    def test_partial_hardlink_set_copies_original_other_body(self):
        n, other = 'usr/share/licenses/a', 'usr/share/licenses/b'
        data = archive(member(n, links=3), member(other, b'notice', links=3))
        observed = graphics.scan(io.BytesIO(data))
        self.assertEqual(graphics.select(observed, {n: expected(b'notice')}, {}), {other: [n]})

    def test_multiple_hardlink_bodies_and_different_headers_rejected(self):
        n, other = 'usr/share/licenses/a', 'usr/share/licenses/b'
        for data in (archive(member(n, b'a', links=2), member(other, b'a', links=2)),
                     archive(member(n, links=2), member(other, b'a', mode=stat.S_IFREG | 0o755, links=2))):
            with self.assertRaisesRegex(ValueError, 'hardlink'):
                graphics.select(graphics.scan(io.BytesIO(data)), {n: expected(b'a')}, {})

    def test_empty_hardlink_body(self):
        n = 'usr/share/licenses/empty'
        observed = graphics.scan(io.BytesIO(archive(member(n, links=2))))
        self.assertEqual(graphics.select(observed, {n: expected(b'')}, {}), {n: [n]})

    def test_link_checked_as_original_data(self):
        n = 'usr/lib64/dri/swrast_dri.so'
        observed = graphics.scan(io.BytesIO(archive(member(n, b'libdril_dri.so', stat.S_IFLNK | 0o777))))
        graphics.select(observed, {}, {n: 'libdril_dri.so'})
        with self.assertRaisesRegex(ValueError, 'alias'):
            graphics.select(observed, {}, {n: 'foreign.so'})

    def test_truncation_invalid_header_and_nonzero_tail_rejected(self):
        valid = archive(member('usr/a', b'hello'))
        for data in (valid[:10], valid[:-8], b'070702'+valid[6:], valid+b'x', valid+b'\0'*(graphics.TAIL_LIMIT+1)):
            with self.subTest(size=len(data)), self.assertRaises(ValueError):
                graphics.scan(io.BytesIO(data))
        with self.assertRaisesRegex(ValueError, 'after graphics archive'):
            graphics.scan(ShortReads(valid+b'\0'*40+b'x'))
        self.assertEqual(graphics.scan(ShortReads(valid+b'\0'*9880)), graphics.scan(io.BytesIO(valid)))

    def test_duplicates_escape_and_device_nodes_rejected(self):
        for data in (archive(member('usr/a'), member('usr/a')), archive(member('../escape')),
                     archive(member('/absolute')), archive(member('usr/a', mode=stat.S_IFCHR | 0o600))):
            with self.assertRaises(ValueError):
                graphics.scan(io.BytesIO(data))

    def test_member_and_total_bounds_are_enforced(self):
        data = archive(member('usr/a', b'0123456789'))
        with patch.object(graphics, 'MEMBER_LIMIT', 9), self.assertRaisesRegex(ValueError, 'member bound'):
            graphics.scan(io.BytesIO(data))
        with patch.object(graphics, 'ARCHIVE_LIMIT', 128), self.assertRaisesRegex(ValueError, 'archive bound'):
            graphics.scan(io.BytesIO(data))

    def test_only_selected_body_reaches_sink(self):
        copied = []
        data = archive(member('usr/ignore', b'no'), member('usr/keep', b'yes'))
        graphics.scan(io.BytesIO(data), lambda n, v: copied.append if n == 'usr/keep' else None)
        self.assertEqual(copied, [b'yes'])

    def test_root_prepare_refused_before_effects(self):
        with patch.object(graphics.sys, 'platform', 'linux'), patch.object(graphics.os, 'geteuid', return_value=0, create=True):
            with self.assertRaisesRegex(ValueError, 'ordinary native build account'):
                graphics.prepare('missing', 'missing')

    def test_profile_preserves_complete_mesa_and_real_large_llvm(self):
        v, _ = graphics.profile()
        files = {e['path']: e for e in v['files']}
        self.assertGreater(files['usr/lib64/libLLVM.so.22.1']['size'], 128*1024**2)
        self.assertIn('usr/lib64/gbm/dri_gbm.so', files)
        self.assertIn('usr/lib64/dri/iris_dri.so', v['symlinks'])
        self.assertIn('usr/lib64/dri/nouveau_dri.so', v['symlinks'])

    def test_profile_rejects_escape_cycle_and_runtime_source_relabelling(self):
        original, _ = graphics.profile()
        n = next(iter(original['symlinks']))
        for target in ('../../../../outside', n.rsplit('/', 1)[1]):
            v = copy.deepcopy(original)
            v['symlinks'][n] = target
            with self.assertRaises(ValueError):
                graphics.validate(v)
        v = copy.deepcopy(original)
        next(e for e in v['notices'] if 'source_package' in e)['source_package'] = 0
        with self.assertRaisesRegex(ValueError, 'source data'):
            graphics.validate(v)


if __name__ == '__main__':
    unittest.main()
