"""GPT-only fixture changes and actual wrong-parent result classification."""
from pathlib import Path
import struct
import tempfile
import unittest
from unittest import mock
import uuid
import zlib

import image_io
import prepare_usb_parent_negative as prepare
import test_usb_parent_negative as negative


class USBParentContract(unittest.TestCase):
    def test_space_preflight_needs_full_pair_and_headroom_before_effects(self):
        required=2*31457280000+8*1024**3;output=Path('/owned/new-attempt')
        with mock.patch.object(prepare.shutil,'disk_usage') as usage:
            usage.return_value.free=required
            prepare.require_space(output);usage.assert_called_once_with(output.parent)
            usage.return_value.free=required-1
            with self.assertRaises(OSError) as error:prepare.require_space(output)
            self.assertEqual(error.exception.errno,28)

    def test_guid_mutation_changes_only_both_guid_and_crc_fields(self):
        with tempfile.TemporaryDirectory() as temporary:
            path=Path(temporary)/'fixture';size=8*1024**2;last=size//512-1
            old=uuid.UUID('11111111-2222-3333-4444-555555555555');new=uuid.UUID('66666666-7777-8888-9999-aaaaaaaaaaaa')
            entries=bytearray(16384);entries[2*128+16:2*128+32]=old.bytes_le
            with path.open('x+b') as stream:
                image_io.sparse_file(stream);image_io.set_length(stream,size)
                for current,other,array in ((1,last,2),(last,1,last-32)):
                    header=bytearray(92);header[:8]=b'EFI PART';struct.pack_into('<II',header,8,0x10000,92);struct.pack_into('<QQQQ',header,24,current,other,2048,last-33);header[56:72]=old.bytes_le;struct.pack_into('<QIII',header,72,array,128,128,zlib.crc32(entries));struct.pack_into('<I',header,16,zlib.crc32(header))
                    stream.seek(current*512);stream.write(header);stream.seek(array*512);stream.write(entries)
                stream.flush();before=path.read_bytes();prepare.replace_guid(stream,3,old,new);after=path.read_bytes()
                allowed=set()
                for current,other,array in ((1,last,2),(last,1,last-32)):
                    for start,length in ((current*512+16,4),(current*512+88,4),(array*512+2*128+16,16)):allowed.update(range(start,start+length))
                    header=bytearray(after[current*512:current*512+92]);crc=struct.unpack_from('<I',header,16)[0];struct.pack_into('<I',header,16,0);self.assertEqual(zlib.crc32(header),crc)
                    changed=after[array*512:array*512+16384];self.assertEqual(zlib.crc32(changed),struct.unpack_from('<I',header,88)[0]);self.assertEqual(uuid.UUID(bytes_le=changed[2*128+16:2*128+32]),new)
                self.assertEqual(len(before),len(after));self.assertTrue(any(a!=b for a,b in zip(before,after)))
                self.assertTrue(all(a==b or i in allowed for i,(a,b) in enumerate(zip(before,after))))
                with self.assertRaises(ValueError):prepare.replace_guid(stream,3,old,new)
                self.assertEqual(path.read_bytes(),after)

    def test_failure_needs_actual_handover_specific_reason_and_no_mount(self):
        nonce='a'*32;boot='11111111-2222-3333-4444-555555555555';m=dict(nonce=nonce,kernel_release='6.19.10-300.fc44.x86_64')
        lines=[f'AETHERI_INITRD_DEVICE nonce={nonce} diskseq=1 boot={boot}\n',f'AETHERI_INITRD_PASS nonce={nonce} usb=1 ext4_ro=1 boot={boot}\n','aetheri-initrd-udev.service: Deactivated successfully.\n',f'AETHERI_INITRD_HANDOFF nonce={nonce} usb=1 ext4_ro=1 boot={boot}\n','Switching root.\n',f'AETHERI_ROOT_PASS nonce={nonce} kernel={m["kernel_release"]} arch=x86_64 pid1=systemd uefi=1 ext4_ro=1 write_erofs=1 cgroup2=1 boot={boot}\n','AETHERI_USB_DATA_FAIL reason=data-parent\n',"aetheri-data-check.service: Failed with result 'exit-code'.\n",'reboot: Power down\n']
        good=''.join(lines);self.assertTrue(negative.evaluate(good,m,0,expect_failure='data-parent')[0])
        for bad in [good.replace(boot,'00000000-0000-0000-0000-000000000000',1),''.join(reversed(lines)),good+'Mounting var.mount\n',good+'Mounting \x1b[0;1;39mvar.mount\x1b[0m\n',good+'AETHERI_USB_DATA_PASS foreign\n',good.replace('reason=data-parent','reason=filesystem'),good+'AETHERI_DESKTOP_READY foreign\n']+[''.join(lines[:i]+lines[i+1:]) for i in range(len(lines))]:
            self.assertFalse(negative.evaluate(bad,m,0,expect_failure='data-parent')[0])
        self.assertFalse(negative.evaluate(good,m,1,expect_failure='data-parent')[0]);self.assertFalse(negative.evaluate(good,m,0,True,'data-parent')[0])

    def test_negative_command_has_exactly_two_owned_usb_media_and_no_control_listener(self):
        args=negative.command(Path('/image'),Path('/attempt'),Path('/owned/primary.raw'),Path('/owned/external.raw'),'11111111-2222-3333-4444-555555555555')
        self.assertEqual(args.count('-drive'),4);self.assertEqual(sum(value.startswith('usb-storage,') for value in args),2)
        self.assertFalse(any('tcp:' in value or 'virtio-blk' in value for value in args))
        for bad in ('-kernel','-initrd','-append','-qmp','-netdev','-virtfs','-fsdev'):self.assertNotIn(bad,args)


if __name__=='__main__':unittest.main(verbosity=2)
