"""Runtime image-copy adversaries. No host installation or VM start."""
import copy
import hashlib
import os
from pathlib import Path
import tempfile
import unittest

import desktop_payload as payload
import uefi_probe_image as media


class RuntimePayload(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.base=Path(self.temp.name);self.tree=self.base/'root';self.tree.mkdir()
        self.source=self.base/'input';self.source.write_bytes(b'pinned runtime bytes\n')
        self.entry=dict(source=str(self.source),destination='/usr/lib/aetheri/runtime/libfixture.so',size=self.source.stat().st_size,sha256=hashlib.sha256(self.source.read_bytes()).hexdigest(),mode=0o644,package='fixture-1',provenance=None)
        self.m=dict(format=1,files=[self.entry],symlinks={})

    def test_exact_copy_and_reuse_never_overwrite_an_incumbent(self):
        first=payload.install(self.tree,self.m)
        target=self.tree/'usr/lib/aetheri/runtime/libfixture.so'
        self.assertEqual(target.read_bytes(),self.source.read_bytes())
        witness=target.stat()
        second=payload.install(self.tree,self.m)
        self.assertEqual((first['copied'],second['reused']),(1,1))
        self.assertEqual(target.stat().st_ino,witness.st_ino)
        target.write_bytes(b'foreign same place')
        with self.assertRaises(ValueError):payload.install(self.tree,self.m)
        self.assertEqual(target.read_bytes(),b'foreign same place')

    def test_bad_paths_bounds_modes_hashes_and_duplicate_destinations(self):
        for path in ('/usr/../outside','/usr//lib/file','/tmp/foreign','usr/lib/file','/usr/lib/evil\nentry','/usr/lib/../lib/file'):
            m=copy.deepcopy(self.m);m['files'][0]['destination']=path
            with self.assertRaises(ValueError):payload.validate(m)
        for key,value in [('size',True),('size',-1),('size',257*1024**2),('sha256','0'*63),('sha256','G'*64),('mode',0o4755),('source','relative')]:
            m=copy.deepcopy(self.m);m['files'][0][key]=value
            with self.assertRaises(ValueError):payload.validate(m)
        m=copy.deepcopy(self.m);m['files']*=2
        with self.assertRaises(ValueError):payload.validate(m)

    def test_aliases_cannot_escape_clobber_or_become_ancestors(self):
        m=copy.deepcopy(self.m);m['symlinks']={'/usr/lib/aetheri/runtime/libfixture.so.0':'libfixture.so'}
        payload.validate(m)
        for target in ('/usr/lib/aetheri/runtime/libfixture.so','missing.so','../../../../../../usr/lib/aetheri/runtime/libfixture.so','./libfixture.so','dir/../libfixture.so'):
            changed=copy.deepcopy(m);changed['symlinks']['/usr/lib/aetheri/runtime/libfixture.so.0']=target
            with self.assertRaises(ValueError):payload.validate(changed)
        changed=copy.deepcopy(m);changed['symlinks']={'/usr/lib/aetheri/runtime':'runtime/libfixture.so'}
        with self.assertRaises(ValueError):payload.validate(changed)
        changed=copy.deepcopy(m);changed['symlinks']={self.entry['destination']:'libfixture.so'}
        with self.assertRaises(ValueError):payload.validate(changed)
        if os.name!='nt':
            payload.install(self.tree,m)
            self.assertEqual((self.tree/'usr/lib/aetheri/runtime/libfixture.so.0').read_bytes(),self.source.read_bytes())

    def test_source_change_preserves_the_failed_copy_and_refuses_replay(self):
        self.source.write_bytes(b'changed runtime byte\n')
        self.assertEqual(self.source.stat().st_size,self.entry['size'])
        with self.assertRaisesRegex(ValueError,'source bytes changed'):payload.install(self.tree,self.m)
        target=self.tree/'usr/lib/aetheri/runtime/libfixture.so'
        self.assertEqual(target.read_bytes(),self.source.read_bytes())
        with self.assertRaisesRegex(ValueError,'existing runtime bytes'):payload.install(self.tree,self.m)

    def test_existing_non_directory_parent_cannot_redirect_a_copy(self):
        (self.tree/'usr').write_text('foreign')
        with self.assertRaisesRegex(ValueError,'parent'):payload.install(self.tree,self.m)
        self.assertEqual((self.tree/'usr').read_text(),'foreign')
        if os.name!='nt':
            (self.tree/'usr').unlink();(self.tree/'usr').symlink_to(self.base,target_is_directory=True)
            with self.assertRaisesRegex(ValueError,'parent'):payload.install(self.tree,self.m)

    def test_root_geometry_is_explicit_and_profile_bound(self):
        self.assertEqual(media.root_geometry(dict(proof='readonly-root')),(512*1024**2,524288))
        self.assertEqual(media.root_geometry(dict(proof='writable-data')),(512*1024**2,524288))
        self.assertEqual(media.root_geometry(dict(proof='desktop-image')),(768*1024**2,1048576))
        for value in (None,'arbitrary','ram-root'):
            with self.assertRaises(ValueError):media.root_geometry(dict(proof=value))

    def test_only_independently_pinned_flat_base_copies_can_be_normalized(self):
        payload.install(self.tree,self.m)
        alias='/usr/lib/aetheri/runtime/libfixture.so.0'
        path=self.tree/alias.lstrip('/');path.write_bytes(self.source.read_bytes())
        m=copy.deepcopy(self.m);m['symlinks']={alias:'libfixture.so'}
        with self.assertRaisesRegex(ValueError,'unrecognized'):payload.normalize_base(self.tree,m,{})
        self.assertEqual(path.read_bytes(),self.source.read_bytes())
        path.write_bytes(b'foreign')
        with self.assertRaisesRegex(ValueError,'changed'):payload.normalize_base(self.tree,m,{alias:self.entry['sha256']})
        self.assertEqual(path.read_bytes(),b'foreign')
        path.write_bytes(self.source.read_bytes())
        self.assertEqual(payload.normalize_base(self.tree,m,{alias:self.entry['sha256']}),[alias.lstrip('/')])
        self.assertFalse(path.exists())


if __name__=='__main__':unittest.main(verbosity=2)
