"""Corruption stays bounded to a new regular image; failures require handover."""
from pathlib import Path
import tempfile
import unittest
import uuid

import image_io
import test_usb_data_filesystem as corruption
import test_usb_parent_negative as negative


class CorruptionContract(unittest.TestCase):
    def test_only_sixteen_uuid_bytes_change_and_repeat_is_refused(self):
        with tempfile.TemporaryDirectory() as temporary:
            path=Path(temporary)/'new.raw';identifier=uuid.UUID('11111111-2222-3333-4444-555555555555')
            part=dict(start=1310720,sectors=60127232,filesystem_uuid=str(identifier));at=671088640+1024
            block=bytearray(1024);block[56:58]=b'\x53\xef';block[104:120]=identifier.bytes
            with path.open('x+b') as held:
                image_io.sparse_file(held);image_io.set_length(held,31457280000)
                held.seek(at);held.write(block);held.flush()
                receipt=corruption.corrupt_uuid(held,part);held.seek(at);after=held.read(1024)
                self.assertEqual(receipt['offset'],at+104);self.assertEqual(receipt['bytes'],16)
                self.assertEqual(after[:104],block[:104]);self.assertEqual(after[120:],block[120:])
                self.assertEqual(after[104:120],bytes([identifier.bytes[0]^1])+identifier.bytes[1:])
                self.assertEqual(held.seek(0,2),31457280000)
                with self.assertRaises(ValueError):corruption.corrupt_uuid(held,part)
                held.seek(at);self.assertEqual(held.read(1024),after)

    def test_requires_corresponding_data_failure_before_mount_and_clean_exit(self):
        nonce='a'*32;identifier='11111111-2222-3333-4444-555555555555';manifest=dict(nonce=nonce,kernel_release='6.19.10-300.fc44.x86_64')
        lines=[f'AETHERI_INITRD_DEVICE nonce={nonce} diskseq=1 boot={identifier}\n',f'AETHERI_INITRD_PASS nonce={nonce} usb=1 ext4_ro=1 boot={identifier}\n','aetheri-initrd-udev.service: Deactivated successfully.\n',f'AETHERI_INITRD_HANDOFF nonce={nonce} usb=1 ext4_ro=1 boot={identifier}\n','Switching root.\n',f'AETHERI_ROOT_PASS nonce={nonce} kernel={manifest["kernel_release"]} arch=x86_64 pid1=systemd uefi=1 ext4_ro=1 write_erofs=1 cgroup2=1 boot={identifier}\n','AETHERI_USB_DATA_FAIL reason=filesystem\n',"aetheri-data-check.service: Failed with result 'exit-code'.\n",'reboot: Power down\n']
        good=''.join(lines);self.assertTrue(negative.evaluate(good,manifest,0,expect_failure='filesystem')[0])
        bads=[good.replace('reason=filesystem','reason=data-parent'),good+'Mounted \x1b[0;1;39mvar.mount\x1b[0m\n',good+'AETHERI_DESKTOP_READY\n',good.replace(identifier,'00000000-0000-0000-0000-000000000000',1)]
        bads+=[''.join(lines[:i]+lines[i+1:]) for i in range(len(lines))]
        for bad in bads:self.assertFalse(negative.evaluate(bad,manifest,0,expect_failure='filesystem')[0])
        self.assertFalse(negative.evaluate(good,manifest,1,expect_failure='filesystem')[0])
        self.assertFalse(negative.evaluate(good,manifest,0,True,'filesystem')[0])

    def test_command_has_one_owned_usb_and_no_control_or_network_listener(self):
        args=corruption.command(Path('/image'),Path('/attempt'),Path('/owned/new.raw'),'11111111-2222-3333-4444-555555555555')
        self.assertEqual(sum(v.startswith('usb-storage,') for v in args),1)
        self.assertFalse(any('tcp:' in v or 'virtio-blk' in v for v in args))
        for value in ('-qmp','-kernel','-initrd','-append','-netdev','-virtfs','-fsdev'):self.assertNotIn(value,args)


if __name__=='__main__':unittest.main(verbosity=2)
