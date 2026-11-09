"""Host-only source archive controls; no compiler, policy or installation."""
import copy
import io
import json
from pathlib import Path
import sys
import tarfile
import tempfile
import unittest

import build_libinput as candidate


class Source(unittest.TestCase):
    def test_official_source_inventory(self):
        files=candidate.source_members(ARCHIVE,PINS)
        self.assertEqual(len(files),525)
        self.assertIn('src/libinput-plugin-lua.c',files)
        self.assertIn('test/test-plugins-lua.c',files)
        self.assertIn('COPYING',files)

    def test_changed_source_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d)/'source';raw=bytearray(ARCHIVE.read_bytes());raw[-1]^=1;p.write_bytes(raw)
            with self.assertRaisesRegex(ValueError,'changed selected input'):candidate.source_members(p,PINS)

    def reject(self,name,kind=tarfile.REGTYPE,target=''):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d)/'source'
            with tarfile.open(p,'w:gz') as a:
                m=tarfile.TarInfo(name);m.type=kind;m.linkname=target;a.addfile(m,io.BytesIO(b''))
            pins=copy.deepcopy(PINS);pins['source'].update(size=p.stat().st_size,sha256=candidate.audio.sha(p.read_bytes()))
            with self.assertRaisesRegex(ValueError,'unsafe libinput source member'):candidate.source_members(p,pins)

    def test_parent_escape(self):self.reject('libinput-1.32.0/../outside')
    def test_unexpected_symlink(self):self.reject('libinput-1.32.0/src/other',tarfile.SYMTYPE,'../../outside')
    def test_different_root(self):self.reject('libinput-1.31.3/src/file')


if __name__=='__main__':
    ARCHIVE=Path(sys.argv.pop(1)) if len(sys.argv)>1 else candidate.ROOT/'local/libinput-review-2026-11-09/libinput-1.32.0.tar.gz'
    PINS=json.loads(candidate.PROFILE.read_bytes());unittest.main()
