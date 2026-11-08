"""Inspect actual prepared EFI bytes; no VM, device or installed service."""
import argparse
import json
import hashlib
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

import boot_manager_profile as profile
import build_boot_manager as builder
import uefi_probe_image as media

IMAGE=PREPARED=None


class BootManagerContract(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.raw,cls.manifest,cls.uki,cls.entry=builder.source_profile(IMAGE)
        cls.esp=(PREPARED/'esp.raw').read_bytes()

    def test_preparation_matches_independent_source_and_fixed_files(self):
        raw,manifest,files,esp=builder.checked_preparation(IMAGE,PREPARED)
        self.assertEqual((raw,manifest,esp),(self.raw,self.manifest,self.esp))
        self.assertEqual(files['system.efi'],self.uki)
        self.assertEqual(media.digest(files['BOOTX64.EFI']),profile.MANAGER_SHA256)
        self.assertEqual(files['aetheri.conf'],self.entry)
        self.assertEqual(files['loader.conf'],profile.LOADER)

    def test_full_options_match_the_unchanged_embedded_profile(self):
        args=profile.arguments(self.manifest).encode()
        self.assertEqual(profile.entry(self.manifest,args),profile.entry(self.manifest,args+b'\n'))
        self.assertEqual(args.count(b'aetheri.boot_nonce='),1)
        self.assertEqual(args.count(b'root=PARTUUID='),1)
        for bad in (args+b' aetheri.boot_nonce='+self.manifest['nonce'].encode(),
                    args.replace(b'aetheri.boot_nonce=',b'other='),args+b'\noptions extra',args+b'\0'):
            with self.assertRaises(ValueError):profile.entry(self.manifest,bad)

    def test_wrong_profile_refuses_before_native_preparation(self):
        for change in ({'usb_console_fallback':False},{'proof':'ram-root'}, {'nonce':'bad'}, {'kernel_release':'other'}):
            m={**self.manifest,**change}
            with self.assertRaises(ValueError):profile.arguments(m)
        with patch.object(builder,'source_profile',side_effect=ValueError('foreign source')),patch.object(builder.subprocess,'run') as run:
            with self.assertRaises(ValueError):builder.assemble(IMAGE,PREPARED,PREPARED/'must-not-exist')
            run.assert_not_called();self.assertFalse((PREPARED/'must-not-exist').exists())

    def test_managed_tree_is_an_explicit_profile_not_an_old_fallback(self):
        with self.assertRaises(ValueError):media.boot_file(self.esp)
        changed=bytearray(self.esp);at=changed.find(b'BOOTX64 EFI');self.assertGreater(at,0)
        changed[at]=ord('X')
        with self.assertRaises(ValueError):media.boot_file(changed,managed=True)

    def test_lfn_checksums_and_names_are_required(self):
        found=False
        for at in range(0,len(self.esp)-32,32):
            e=self.esp[at:at+32]
            if e[0]==0x41 and e[11]==0x0f and e[1:11]==b'l\0o\0a\0d\0e\0':
                found=True
                for offset in (13,1):
                    changed=bytearray(self.esp);changed[at+offset]^=1
                    with self.assertRaises(ValueError):media.boot_file(changed,managed=True)
        self.assertTrue(found)

    def test_files_cannot_share_the_managers_cluster_chain(self):
        changed=bytearray(self.esp)
        a=changed.find(b'BOOTX64 EFI');b=changed.find(b'SYSTEM  EFI')
        self.assertGreater(a,0);self.assertGreater(b,0)
        for offset,count in ((20,2),(26,2)):changed[b+offset:b+offset+count]=changed[a+offset:a+offset+count]
        with self.assertRaisesRegex(ValueError,'cross-linked'):media.boot_file(changed,managed=True)

    def test_changed_preparation_cannot_rebind_its_source(self):
        actual=builder.regular
        def changed(path,limit):
            raw=actual(path,limit)
            if path==PREPARED/'manifest.json':
                m=json.loads(raw);m['source_manifest_sha256']='0'*64;return json.dumps(m).encode()
            return raw
        with patch.object(builder,'regular',side_effect=changed):
            with self.assertRaisesRegex(ValueError,'prepared source'):builder.checked_preparation(IMAGE,PREPARED)

    def test_assembly_io_replaces_zeros_and_preserves_other_bytes(self):
        # Bounded real regular-file I/O. Profile admission has the separate
        # actual-ESP tests above; this exercises the native sparse/length API.
        with tempfile.TemporaryDirectory(dir=builder.ROOT/'local') as temporary:
            root=Path(temporary);base=root/'base';base.mkdir();prepared=root/'prepared';prepared.mkdir()
            original=b'\x7f'*4096;esp=b'X'*256+b'\0'*256
            (base/'boot.raw').write_bytes(original);(prepared/'manifest.json').write_bytes(b'{}\n')
            sha=lambda b:hashlib.sha256(b).hexdigest()
            manifest=dict(disk_bytes=4096,esp_start=1,outputs={'boot.raw':sha(original)},
                immutable_ranges=[dict(start=0,size=2048,sha256=sha(original[:2048])),dict(start=3072,size=1024,sha256=sha(original[3072:]))])
            files={'BOOTX64.EFI':b'manager','system.efi':b'uki'}
            def inspected(path):
                return manifest if path==base else json.loads((path/'manifest.json').read_text())
            with patch.object(builder,'checked_preparation',return_value=(b'base',manifest,files,esp)),patch.object(builder.media,'inspect',side_effect=inspected),patch.object(builder.shutil,'disk_usage',return_value=SimpleNamespace(free=9*1024**3)) as space:
                result=builder.assemble(base,prepared,root/'output')
                self.assertEqual((root/'output/boot.raw').read_bytes(),original[:512]+esp+original[1024:])
                self.assertEqual((base/'boot.raw').read_bytes(),original)
                self.assertEqual(result['outputs']['boot.raw'],sha(original[:512]+esp+original[1024:]))
                with self.assertRaises(FileExistsError):builder.assemble(base,prepared,root/'output')
                space.return_value.free=0
                with self.assertRaises(OSError):builder.assemble(base,prepared,root/'low-space')
                self.assertFalse((root/'low-space').exists())


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--image',type=Path,required=True);p.add_argument('--prepared',type=Path,required=True)
    a,rest=p.parse_known_args();IMAGE=a.image.resolve();PREPARED=a.prepared.resolve();unittest.main(argv=[__file__,*rest],verbosity=2)
