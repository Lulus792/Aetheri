"""USB-root payload and independent result/command boundaries; no VM dispatch."""
import copy
import hashlib
from pathlib import Path
import tempfile
import unittest

import build_usb_root_probe as builder
import test_usb_root_probe as controller


class USBRootContract(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup)
        self.file=Path(self.tmp.name)/'payload';self.file.write_bytes(b'pinned USB runtime\n')
        self.pins=dict(format=1,kernel_release='6.19.10-300.fc44.x86_64',files=[dict(source=str(self.file),destination='/usr/lib/aetheri/runtime',size=self.file.stat().st_size,sha256=hashlib.sha256(self.file.read_bytes()).hexdigest(),mode=0o644,package='fixture')])
        self.manifest=dict(nonce='a'*32,kernel_release=self.pins['kernel_release'])
        self.identifier='11111111-2222-3333-4444-555555555555'
        n=self.manifest['nonce'];k=self.manifest['kernel_release'];b=self.identifier
        self.steps=[f'AETHERI_INITRD_DEVICE nonce={n} diskseq=2 boot={b}\n',f'AETHERI_INITRD_PASS nonce={n} usb=1 ext4_ro=1 boot={b}\n',f'AETHERI_INITRD_HANDOFF nonce={n} usb=1 ext4_ro=1 boot={b}\n',f'AETHERI_ROOT_PASS nonce={n} kernel={k} arch=x86_64 pid1=systemd uefi=1 ext4_ro=1 write_erofs=1 cgroup2=1 boot={b}\n']
        self.steps.insert(2,'aetheri-initrd-udev.service: Deactivated successfully.\n')
        self.steps.insert(4,'Switching root.\n')
        self.end='aetheri-root.service: Deactivated successfully.\nreboot: Power down\n'

    def test_payload_rejects_changed_content_paths_extents_and_duplicates(self):
        self.assertEqual(builder.checked_payloads(self.pins)[0][1],self.file.read_bytes())
        for key,value in [('destination','/usr/../outside'),('destination','/usr//double'),('destination','/dev/sda'),('size',True),('size',33*1024**2),('sha256','x'*64),('mode',0o4755)]:
            bad=copy.deepcopy(self.pins);bad['files'][0][key]=value
            with self.assertRaises(ValueError):builder.checked_payloads(bad)
        bad=copy.deepcopy(self.pins);bad['files']*=2
        with self.assertRaises(ValueError):builder.checked_payloads(bad)
        self.file.write_bytes(b'foreign')
        with self.assertRaises(ValueError):builder.checked_payloads(self.pins)

    def test_partition_device_unit_is_canonical_and_cannot_inject_unit_syntax(self):
        self.assertEqual(builder.device_unit(self.identifier),r'dev-disk-by\x2dpartuuid-11111111\x2d2222\x2d3333\x2d4444\x2d555555555555.device')
        for value in ('../../sda','a\nRequires=other','{'+self.identifier+'}',self.identifier+'x'):
            with self.assertRaises(ValueError):builder.device_unit(value)

    def test_success_requires_ordered_same_boot_initrd_handover_and_real_root(self):
        self.assertTrue(controller.evaluate(''.join(self.steps)+self.end,self.manifest,0)[0])
        for i in range(len(self.steps)):self.assertFalse(controller.evaluate(''.join(self.steps[:i]+self.steps[i+1:])+self.end,self.manifest,0)[0])
        for value in (''.join(reversed(self.steps))+self.end,''.join(self.steps)+self.end+self.steps[1],''.join(self.steps).replace('usb=1','usb=0')+self.end,''.join(self.steps)+self.end+'AETHERI_DATA_PASS foreign\n'):
            self.assertFalse(controller.evaluate(value,self.manifest,0)[0])

    def test_foreign_boot_failure_missing_shutdown_and_nonzero_exit_reject(self):
        good=''.join(self.steps)+self.end
        changed=self.steps.copy();changed[3]=changed[3].replace(self.identifier,'00000000-0000-0000-0000-000000000000')
        for body,exit_code,timeout in [(''.join(changed)+self.end,0,False),(good,1,False),(good,0,True),(''.join(self.steps),0,False),(good+'AETHERI_INITRD_FAIL reason=drift\n',0,False)]:
            self.assertFalse(controller.evaluate(body,self.manifest,exit_code,timeout)[0])

    def test_negative_is_specific_and_never_accepts_a_mounted_or_executing_root(self):
        for reason in ('transport','filesystem'):
            text=f"AETHERI_INITRD_FAIL reason={reason}\naetheri-initrd-device.service: Failed with result 'exit-code'.\nreboot: Power down\n"
            self.assertTrue(controller.evaluate(text,self.manifest,0,expect_failure=reason)[0])
            for bad in (text+self.steps[0],text+'Mounting sysroot.mount',text.replace(reason,'other',1)):
                self.assertFalse(controller.evaluate(bad,self.manifest,0,expect_failure=reason)[0])

    def test_usb_and_explicit_virtio_negative_commands_have_only_owned_readonly_media(self):
        normal=controller.command(Path('/image'),Path('/attempt'))
        rejected=controller.command(Path('/image'),Path('/attempt'),True)
        self.assertIn('qemu-xhci,id=xhci',normal);self.assertTrue(any(x.startswith('usb-storage,') and 'removable=on' in x for x in normal))
        self.assertFalse(any(x.startswith('usb-storage,') for x in rejected))
        self.assertTrue(any(x.startswith('virtio-blk-pci,') for x in rejected))
        for args in (normal,rejected):
            self.assertEqual(args.count('-drive'),3)
            self.assertIn('if=none,id=bootmedia,format=raw,readonly=on,file='+str(Path('/image')/'boot.raw'),args)
            for bad in ('-kernel','-initrd','-append','-qmp','-netdev','-fsdev','-virtfs'):self.assertNotIn(bad,args)


if __name__=='__main__':unittest.main(verbosity=2)
