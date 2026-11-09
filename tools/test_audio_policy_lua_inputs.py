"""Host-only archive controls for the private module source recipe."""
import copy
import io
from pathlib import Path
import sys
import tarfile
import tempfile
import unittest

import build_audio_policy_lua as build


class Inputs(unittest.TestCase):
    def test_reviewed_source(self):
        members=build.source_members(ARCHIVE,build.profile())
        self.assertEqual(len(members),546)
        self.assertIn('modules/module-lua-scripting/wplua/wplua.c',members)
        self.assertNotIn('tests/wplua/settings',members)

    def test_changed_bytes(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d)/'source';raw=bytearray(ARCHIVE.read_bytes());raw[-1]^=1;p.write_bytes(raw)
            with self.assertRaisesRegex(ValueError,'changed WirePlumber archive'):build.source_members(p,build.profile())

    def synthetic(self,name,kind=tarfile.REGTYPE,target=''):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d)/'source'
            with tarfile.open(p,'w:gz') as a:
                m=tarfile.TarInfo(name);m.type=kind;m.linkname=target
                a.addfile(m,io.BytesIO(b''))
            pins=copy.deepcopy(build.profile());pins['source'].update(size=p.stat().st_size,sha256=build.audio.sha(p.read_bytes()))
            with self.assertRaisesRegex(ValueError,'unsafe WirePlumber'):build.source_members(p,pins)

    def test_escaping_name(self):self.synthetic('wireplumber-0.5.19/../outside')
    def test_unreviewed_link(self):self.synthetic('wireplumber-0.5.19/modules/x',tarfile.SYMTYPE,'../../outside')
    def test_changed_reviewed_alias(self):self.synthetic('wireplumber-0.5.19/CLAUDE.md',tarfile.SYMTYPE,'outside')


if __name__=='__main__':
    ARCHIVE=Path(sys.argv.pop(1)) if len(sys.argv)>1 else build.ROOT/'local/audio-lua-review-2026-11-09/wireplumber-0.5.19-source.tar.gz'
    unittest.main()
