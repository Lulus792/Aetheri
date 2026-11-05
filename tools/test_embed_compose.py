"""Build input rejection and byte-exact generated-data/license preservation."""
import hashlib
from pathlib import Path
import re
import tempfile
import unittest

import embed_compose


class ComposeEmbedding(unittest.TestCase):
    def test_reviewed_bytes_survive_generated_c_and_license(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);table=root/'table';license_path=root/'license';out=root/'out'/'data.c'
            raw='<dead_acute> <e> : "é"\n'.encode();license_bytes=b'Copyright example\nPermission notice.\n'
            table.write_bytes(raw);license_path.write_bytes(license_bytes)
            config=dict(size=len(raw),sha256=hashlib.sha256(raw).hexdigest(),license_size=len(license_bytes),license_sha256=hashlib.sha256(license_bytes).hexdigest())
            embed_compose.embed(out,table,license_path,config)
            generated=out.read_bytes();body=generated.split(b'{',1)[1].split(b'}',1)[0]
            self.assertEqual(bytes(map(int,re.findall(rb'\d+',body))),raw)
            self.assertEqual(out.with_name('compose-libX11-COPYING').read_bytes(),license_bytes)
            for path in (table,license_path):
                original=path.read_bytes();path.write_bytes(bytes([original[0]^1])+original[1:])
                with self.assertRaises(ValueError):embed_compose.embed(out,table,license_path,config)
                self.assertEqual(out.read_bytes(),generated);path.write_bytes(original)

    def test_even_pinned_invalid_data_cannot_enable_runtime_lookup(self):
        for raw in (b'include "%H/.XCompose"\n',b' \tinclude"/tmp/table"\n',b'include <locale>\n',b'abc\0hidden',b'\xff',b'',b'a'*(1024*1024+1)):
            with self.subTest(raw=raw[:32]),tempfile.TemporaryDirectory() as d:
                root=Path(d);table=root/'table';license_path=root/'license';out=root/'out'/'data.c'
                table.write_bytes(raw);license_path.write_bytes(b'license')
                config=dict(size=len(raw),sha256=hashlib.sha256(raw).hexdigest(),license_size=7,license_sha256=hashlib.sha256(b'license').hexdigest())
                with self.assertRaises(ValueError):embed_compose.embed(out,table,license_path,config)
                self.assertFalse(out.parent.exists())

    def test_symlink_or_wrong_size_is_not_reviewed_input(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);table=root/'table';table.write_bytes(b'data');link=root/'link';link.symlink_to(table)
            for path,size in ((link,4),(root,4),(table,3),(table,5)):
                with self.subTest(path=path,size=size),self.assertRaises(ValueError):
                    embed_compose.checked(path,size,hashlib.sha256(b'data').hexdigest())


if __name__=='__main__':unittest.main()
