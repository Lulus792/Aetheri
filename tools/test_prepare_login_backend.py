"""Offline input admission and fresh-output regressions; no Rust execution."""
import copy
import io
import json
from pathlib import Path
import tarfile
import tempfile
import unittest

import prepare_login_backend as subject


def archive(members):
    output = io.BytesIO()
    with tarfile.open(fileobj=output, mode='w:gz') as stream:
        for name, body in members:
            item = tarfile.TarInfo(name)
            if isinstance(body, tuple):
                item.type = body[0]
                item.linkname = body[1]
                stream.addfile(item)
            else:
                item.size = len(body)
                stream.addfile(item, io.BytesIO(body))
    return output.getvalue()


class Preparation(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.cache = self.root / 'cache'
        self.cache.mkdir()
        self.source = self.root / 'source.tar.gz'
        self.source.write_bytes(archive([
            ('greetd-0.10.3/Cargo.toml', b'[workspace]\nmembers = ["inish", "greetd_ipc", "greetd", "agreety", "fakegreet"]\n'),
            ('greetd-0.10.3/greetd/Cargo.toml', b'[dependencies]\ntokio = { version = "1", features = ["rt"] }\n'),
            ('greetd-0.10.3/LICENSE', b'fixture license'),
            ('greetd-0.10.3/greetd_ipc/LICENSE', (tarfile.SYMTYPE, '../LICENSE'))]))
        crate = self.cache / 'fixture-1.0.0.crate'
        crate.write_bytes(archive([('fixture-1.0.0/Cargo.toml', b'[package]\nname="fixture"\nversion="1.0.0"\n')]))
        self.crate = crate
        entry = lambda p: {'size': p.stat().st_size, 'sha256': subject.digest(p.read_bytes())}
        self.lock = self.root / 'Cargo.lock'
        self.lock.write_text('version = 4\n[[package]]\nname="fixture"\nversion="1.0.0"\nsource="registry+https://github.com/rust-lang/crates.io-index"\nchecksum="'+entry(crate)['sha256']+'"\n')
        self.pins = {'source': entry(self.source), 'lock': entry(self.lock),
                     'crates': [dict(name='fixture', version='1.0.0', **entry(crate))]}
        self.output = self.root / 'prepared'

    def prepare(self):
        return subject.prepare(self.source, self.cache, self.lock, self.output, self.pins)

    def test_offline_license_fixed_pin_and_checksums(self):
        receipt = self.prepare()
        self.assertFalse(receipt['runtime_admitted'])
        self.assertEqual((self.output/'source/greetd_ipc/LICENSE').read_bytes(), b'fixture license')
        self.assertFalse((self.output/'source/greetd_ipc/LICENSE').is_symlink())
        self.assertIn(b'=1.53.3', (self.output/'source/greetd/Cargo.toml').read_bytes())
        self.assertEqual((self.output/'source/Cargo.lock').read_bytes(), self.lock.read_bytes())
        checksum = json.loads((self.output/'vendor/fixture-1.0.0/.cargo-checksum.json').read_bytes())
        self.assertEqual(checksum['package'], self.pins['crates'][0]['sha256'])
        self.assertEqual(checksum['files']['Cargo.toml'], subject.digest((self.output/'vendor/fixture-1.0.0/Cargo.toml').read_bytes()))

    def test_changed_archive_rejects_before_output(self):
        self.crate.write_bytes(b'changed')
        with self.assertRaises(ValueError): self.prepare()
        self.assertFalse(self.output.exists())

    def test_valid_archive_with_wrong_frozen_digest_rejects(self):
        old = self.pins['crates'][0]['sha256']
        self.pins['crates'][0]['sha256'] = '0' * 64
        self.lock.write_text(self.lock.read_text().replace(old, '0' * 64))
        self.pins['lock'] = {'size': self.lock.stat().st_size, 'sha256': subject.digest(self.lock.read_bytes())}
        with self.assertRaises(ValueError): self.prepare()
        self.assertFalse(self.output.exists())

    def test_changed_lock_rejects_before_output(self):
        self.lock.write_bytes(b'changed')
        with self.assertRaises(ValueError): self.prepare()
        self.assertFalse(self.output.exists())

    def test_missing_crate_rejects_before_output(self):
        self.crate.unlink()
        with self.assertRaises(FileNotFoundError): self.prepare()
        self.assertFalse(self.output.exists())

    def test_existing_output_is_preserved(self):
        self.output.mkdir()
        (self.output/'keep').write_bytes(b'unchanged')
        with self.assertRaises(FileExistsError): self.prepare()
        self.assertEqual(list(self.output.iterdir()), [self.output/'keep'])
        self.assertEqual((self.output/'keep').read_bytes(), b'unchanged')

    def test_failed_write_retains_partial_output_and_refuses_reuse(self):
        from unittest.mock import patch
        original = Path.open

        def injected(path, *args, **kwargs):
            if path == self.output/'source/greetd/Cargo.toml':
                raise OSError('injected write failure')
            return original(path, *args, **kwargs)

        with patch.object(Path, 'open', injected), self.assertRaises(OSError): self.prepare()
        before = {p.relative_to(self.output).as_posix(): p.read_bytes() for p in self.output.rglob('*') if p.is_file()}
        self.assertTrue(before)
        self.assertFalse((self.output/'prepared.json').exists())
        with self.assertRaises(FileExistsError): self.prepare()
        self.assertEqual(before, {p.relative_to(self.output).as_posix(): p.read_bytes() for p in self.output.rglob('*') if p.is_file()})

    def test_lock_graph_mismatch(self):
        self.pins['crates'][0]['version'] = '2.0.0'
        with self.assertRaises(ValueError): self.prepare()
        self.assertFalse(self.output.exists())

    def test_duplicate_graph_members(self):
        self.pins['crates'].append(copy.deepcopy(self.pins['crates'][0]))
        with self.assertRaises(ValueError): self.prepare()
        self.assertFalse(self.output.exists())

    def test_archive_names_types_duplicates_and_collisions(self):
        for members in [
            [('crate/../escape', b'x')], [('/crate/escape', b'x')],
            [('crate/a\\b', b'x')], [('crate/C:escape', b'x')],
            [('crate/a', b'x'), ('crate/a', b'y')],
            [('crate/a', b'x'), ('crate/a/b', b'y')],
            [('crate/a', (tarfile.LNKTYPE, 'crate/other'))],
            [('crate/a', (tarfile.SYMTYPE, '../../escape'))],
            [('crate/a', (tarfile.FIFOTYPE, ''))],
            [('other/a', b'x')],
        ]:
            with self.subTest(members=members), self.assertRaises(ValueError):
                subject.unpack(archive(members), 'crate')

    def test_license_alias_is_exact(self):
        for target in ('../../LICENSE', '/LICENSE', '../other'):
            with self.subTest(target=target), self.assertRaises(ValueError):
                subject.unpack(archive([('greetd-0.10.3/greetd_ipc/LICENSE', (tarfile.SYMTYPE, target))]), 'greetd-0.10.3', True)

    def test_trailing_slash_cannot_alias_regular_member(self):
        with self.assertRaises(ValueError):
            subject.unpack(archive([('crate/a/', b'x'), ('crate/a', b'y')]), 'crate')
        with self.assertRaises(ValueError):
            subject.unpack(archive([('crate/a//', (tarfile.DIRTYPE, '')),
                                    ('crate/a', (tarfile.DIRTYPE, ''))]), 'crate')

    def test_member_and_aggregate_limits(self):
        from unittest.mock import patch
        data = archive([('crate/a', b'12345')])
        with patch.object(subject, 'MAX_MEMBER', 4), self.assertRaises(ValueError):
            subject.unpack(data, 'crate')
        with patch.object(subject, 'MAX_TREE', 4), self.assertRaises(ValueError):
            subject.unpack(data, 'crate')


if __name__ == '__main__':
    unittest.main()
