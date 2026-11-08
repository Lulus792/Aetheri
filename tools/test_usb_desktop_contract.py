"""Single-medium layout, bounded image I/O and independent boot observations."""
import copy
import hashlib
import os
import ctypes
import struct
from pathlib import Path
import tempfile
import unittest

import build_usb_desktop as builder
import image_io
import test_usb_desktop as controller
import uefi_probe_image as media


class USBDesktopContract(unittest.TestCase):
    def setUp(self):
        base=Path(__file__).resolve().parents[1]/'local' if os.name=='nt' else None
        if base:base.mkdir(exist_ok=True)
        self.tmp=tempfile.TemporaryDirectory(dir=base);self.root=Path(self.tmp.name)
        if base:self.assertTrue(self.root.resolve().is_relative_to(base.resolve()))
        self.addCleanup(self.tmp.cleanup)
        self.nonce='a'*32;self.boot='11111111-2222-3333-4444-555555555555'
        end=(1310720+60127232)*512
        self.manifest=dict(proof='usb-desktop',nonce=self.nonce,disk_bytes=31457280000,usb_wrong_data_filesystem=False,data_partition=dict(start=1310720,sectors=60127232),outputs={'boot.raw':'0'*64},immutable_ranges=[dict(start=0,size=1310720*512,sha256='1'*64),dict(start=end,size=31457280000-end,sha256='2'*64)])

    def test_sparse_copy_and_extent_hash_read_back_actual_bytes(self):
        data=b'begin'+b'\0'*(2*1024**2)+b'end';source=self.root/'source';target=self.root/'target';source.write_bytes(data)
        with source.open('rb') as src,target.open('x+b') as dst:
            image_io.sparse_file(dst);self.assertEqual(image_io.copy_extent(src,dst,len(data)),hashlib.sha256(data).hexdigest());image_io.set_length(dst,len(data));dst.flush()
            with self.assertRaises(ValueError):image_io.sparse_file(dst)
        self.assertEqual(target.read_bytes(),data)
        self.assertEqual(image_io.range_sha(target,5,len(data)-8),hashlib.sha256(data[5:-3]).hexdigest())
        for start,size in [(-1,1),(0,-1),(len(data),1),(0,True)]:
            with self.assertRaises(ValueError):image_io.range_sha(target,start,size)
        with source.open('rb') as src,(self.root/'short').open('x+b') as dst:
            with self.assertRaises(ValueError):image_io.copy_extent(src,dst,len(data)+1)

    def test_full_geometry_regular_reader_stays_bounded_and_sparse(self):
        path=self.root/'large'
        with path.open('x+b') as f:
            image_io.sparse_file(f);image_io.set_length(f,31457280000);f.seek(-3,2);f.write(b'end');f.flush()
            if os.name=='nt':self.assertTrue(os.fstat(f.fileno()).st_file_attributes&0x200)
            else:self.assertLess(os.fstat(f.fileno()).st_blocks*512,1024**2)
            f.seek(450);f.write(b'\xee');f.seek(454);f.write(struct.pack('<II',1,31457280000//512-1));f.seek(510);f.write(b'\x55\xaa');f.flush()
        if os.name=='nt':
            from ctypes import wintypes
            call=ctypes.WinDLL('kernel32',use_last_error=True).GetCompressedFileSizeW
            call.argtypes=[wintypes.LPCWSTR,ctypes.POINTER(wintypes.DWORD)];call.restype=wintypes.DWORD;high=wintypes.DWORD();ctypes.set_last_error(0)
            low=call(str(path),ctypes.byref(high))
            if low==0xffffffff and ctypes.get_last_error():raise ctypes.WinError(ctypes.get_last_error())
            self.assertLess((high.value<<32)|low,1024**2)
        with path.open('rb') as f:
            data=media.RegularImage(f);self.assertEqual(len(data),31457280000);self.assertEqual(data[-3:],b'end');self.assertEqual(data[10:2],b'');self.assertEqual(data[0],0)
            self.assertEqual(data.range_digest(8192,4096),hashlib.sha256(b'\0'*4096).hexdigest())
            with self.assertRaisesRegex(ValueError,'oversized'):data[:]
            with self.assertRaises(ValueError):data.range_digest(31457280000,1)
            manifest=copy.deepcopy(self.manifest);manifest['root_partition']={}
            with self.assertRaisesRegex(ValueError,'invalid GPT header'):media.esp_extent(data,manifest)

    def test_geometry_and_mutable_range_admission_are_fixed(self):
        _,_,layout=builder.selected_layout();self.assertEqual(layout['disk_bytes'],31457280000)
        self.assertEqual(controller.admit(self.manifest)['size'],31457280000)
        variants=[]
        for key,value in [('disk_bytes',32*1024**3),('usb_wrong_data_filesystem',True)]:
            bad=copy.deepcopy(self.manifest);bad[key]=value;variants.append(bad)
        bad=copy.deepcopy(self.manifest);bad['data_partition']['start']-=1;variants.append(bad)
        bad=copy.deepcopy(self.manifest);bad['immutable_ranges'][0]['size']-=1;variants.append(bad)
        bad=copy.deepcopy(self.manifest);bad['immutable_ranges'][1]['sha256']='foreign';variants.append(bad)
        for bad in variants:
            with self.assertRaises(ValueError):controller.admit(bad)

    def test_command_replaces_both_legacy_disks_with_one_owned_usb(self):
        args=controller.command(Path('/image'),Path('/attempt'),Path('/owned/medium.raw'),self.boot)
        self.assertEqual(args.count('-drive'),3);self.assertTrue(any(v.startswith('usb-storage,') and 'removable=on' in v for v in args));self.assertIn('if=none,id=usbmedium,format=raw,file='+str(Path('/owned/medium.raw')),args)
        self.assertFalse(any('virtio-blk' in v or 'id=userdata' in v or 'tcp:' in v or str(Path('/image')/'boot.raw') in v for v in args))
        for bad in ('-kernel','-initrd','-append','-netdev','-fsdev','-virtfs'):self.assertNotIn(bad,args)

    def test_native_handover_and_same_parent_mount_sequence_cannot_be_mixed(self):
        n=self.nonce;b=self.boot
        lines=[f'AETHERI_INITRD_DEVICE nonce={n} diskseq=1 boot={b}\n',f'AETHERI_INITRD_PASS nonce={n} usb=1 ext4_ro=1 boot={b}\n','aetheri-initrd-udev.service: Deactivated successfully.\n',f'AETHERI_INITRD_HANDOFF nonce={n} usb=1 ext4_ro=1 boot={b}\n','Switching root.\n',f'AETHERI_ROOT_PASS nonce={n} ext4_ro=1 boot={b}\n',f'AETHERI_USB_DATA_PASS nonce={n} same_disk=1 root_ro=1 data_rw=0 boot={b}\n',f'AETHERI_USB_DATA_MOUNTED nonce={n} same_disk=1 root_ro=1 data_rw=1 boot={b}\n']
        good=''.join(lines);self.assertTrue(controller.check_boot(good,self.manifest,b))
        for bad in [good.replace(b,'00000000-0000-0000-0000-000000000000',1),''.join(reversed(lines)),good+lines[-1],good+'AETHERI_USB_DATA_FAIL reason=data-parent\n',good.replace('same_disk=1','same_disk=0')]+[''.join(lines[:i]+lines[i+1:]) for i in range(len(lines))]:
            with self.assertRaises(ValueError):controller.check_boot(bad,self.manifest,b)

    def test_shutdown_needs_both_unmounts_and_actual_ext4_unmount(self):
        lines=['Unmounted var.mount - /var.\n','Unmounted home.mount - /home.\n','EXT4-fs (sda3): unmounting filesystem\n']
        self.assertTrue(controller.unmounted(''.join(lines)))
        self.assertTrue(controller.unmounted(''.join(lines).replace('var.mount','\x1b[0;1;39mvar.mount\x1b[0m').replace('home.mount','\x1b[0;1;39mhome.mount\x1b[0m')))
        for i in range(3):self.assertFalse(controller.unmounted(''.join(lines[:i]+lines[i+1:])))


if __name__=='__main__':unittest.main(verbosity=2)
