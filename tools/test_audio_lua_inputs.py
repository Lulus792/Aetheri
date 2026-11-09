"""Source/archive/patch controls only: no compiler, bus, VM or installation."""
import copy
import io
from pathlib import Path
import sys
import tarfile
import tempfile
import unittest

import build_audio_lua as lua


class Inputs(unittest.TestCase):
    def setUp(self):
        self.temporary=tempfile.TemporaryDirectory();self.addCleanup(self.temporary.cleanup)
        self.root=Path(self.temporary.name);self.pins=lua.profile()[0]

    def archive(self,names):
        data=io.BytesIO()
        with tarfile.open(fileobj=data,mode='w:gz') as archive:
            for name,kind in names:
                item=tarfile.TarInfo(name);item.type=kind
                if kind==tarfile.REGTYPE:item.size=1;archive.addfile(item,io.BytesIO(b'x'))
                else:item.linkname='outside';archive.addfile(item)
        raw=data.getvalue();path=self.root/'source.tar.gz';path.write_bytes(raw)
        pins=copy.deepcopy(self.pins);pins['source'].update(size=len(raw),sha256=lua.sha(raw));return path,pins

    def test_exact_source_bytes_are_required(self):
        path,pins=self.archive([('lua-5.5.1/a.c',tarfile.REGTYPE)])
        self.assertEqual(lua.source_members(path,pins),{'a.c':b'x'})
        raw=path.read_bytes();path.write_bytes(raw[:-1]+bytes([raw[-1]^1]))
        with self.assertRaisesRegex(ValueError,'different Lua source bytes'):lua.source_members(path,pins)

    def test_links_traversal_aliases_and_duplicates_are_rejected(self):
        for names in ([('lua-5.5.1/a',tarfile.SYMTYPE)], [('lua-5.5.1/../a',tarfile.REGTYPE)], [('lua-5.5.1//a',tarfile.REGTYPE)], [('lua-5.5.1/a',tarfile.REGTYPE)]*2):
            path,pins=self.archive(names)
            with self.assertRaisesRegex(ValueError,'unsafe Lua source member'):lua.source_members(path,pins)

    def test_patch_inventory_and_bytes_are_bound(self):
        pins,digest=lua.profile();self.assertEqual(len(digest),64)
        self.assertEqual(lua.sha((lua.ROOT/pins['patch']['path']).read_bytes()),pins['patch']['sha256'])
        self.assertEqual(len(pins['patch']['files']),6)


if __name__=='__main__':unittest.main()
