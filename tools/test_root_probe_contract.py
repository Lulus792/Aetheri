"""Readonly-root artifact and result regressions; never starts a VM."""
import copy
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch
import uuid

import test_boot_probe as boot
import test_uefi_probe as runner
import uefi_probe_image as media

IMAGE = None


class RootContract(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.manifest = media.inspect(IMAGE)
        cls.disk = (IMAGE/"boot.raw").read_bytes()

    def test_actual_root_partition_and_efi_resources(self):
        m = self.manifest
        self.assertEqual(m['proof'], 'readonly-root')
        self.assertEqual(m['disk_bytes'], 512*1024**2)
        esp = media.esp_extent(self.disk, m)
        self.assertEqual(media.digest(media.boot_file(esp)), m['outputs']['BOOTX64.EFI'])

    def test_root_filesystem_uuid_and_content_are_separately_checked(self):
        changed = copy.deepcopy(self.manifest)
        changed['root_partition']['filesystem_uuid'] = str(uuid.UUID(int=1))
        with self.assertRaisesRegex(ValueError, 'filesystem'):
            media.esp_extent(self.disk, changed)
        changed = copy.deepcopy(self.manifest)
        changed['root_partition']['sha256'] = '0'*64
        with self.assertRaisesRegex(ValueError, 'changed root'):
            media.esp_extent(self.disk, changed)

    def test_root_cannot_be_selected_by_changed_extent_or_identity(self):
        for key, value in [('start', 1), ('sectors', 1), ('guid', str(uuid.UUID(int=1)))]:
            m = copy.deepcopy(self.manifest)
            m['root_partition'][key] = value
            with self.assertRaises(ValueError):
                media.esp_extent(self.disk, m)

    def test_no_initrd_init_or_direct_qemu_payload_for_this_profile(self):
        sections = media.pe_sections((IMAGE/'BOOTX64.EFI').read_bytes())
        cmdline = sections['.cmdline'].decode()
        self.assertIn('root=PARTUUID='+self.manifest['root_partition']['guid'], cmdline)
        self.assertIn(' ro rootfstype=ext4 rootflags=noload ', cmdline)
        self.assertNotIn('rdinit=', cmdline)
        out = boot.ROOT/'out'
        out.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='root-contract-',dir=out) as directory:
            target = Path(directory)/'attempt'
            with patch.object(boot,'host_tools',return_value={}),patch.object(boot,'observe') as observe,patch.object(runner.shutil,'copyfile'):
                runner.run(IMAGE,target)
                command = observe.call_args.args[1]
                self.assertIn('virtio-blk-pci,drive=bootmedia,serial=AETHERI_ROOT_PROBE,bootindex=1',command)
                self.assertTrue(all(x not in command for x in ('-kernel','-initrd','-append','-netdev','-qmp','-fsdev')))

    def test_ram_root_success_cannot_satisfy_readonly_root(self):
        m = self.manifest
        body = f"nonce={m['nonce']} kernel={m['kernel_release']} arch=x86_64 pid1=systemd uefi=1 "
        tail = ' cgroup2=1 boot=11111111-1111-1111-1111-111111111111\n'
        end = 'aetheri-root.service: Deactivated successfully.\nreboot: Power down\n'
        good = 'AETHERI_ROOT_PASS '+body+'ext4_ro=1 write_erofs=1'+tail
        self.assertTrue(boot.evaluate(good+end,m,0)[0])
        for line in ['AETHERI_BOOT_PASS '+body+'ram_root=1'+tail,good.replace('write_erofs=1','write_erofs=0'),good.replace('ext4_ro=1','ext4_ro=0')]:
            self.assertFalse(boot.evaluate(line+end,m,0)[0])
        self.assertFalse(boot.evaluate(good+end+good,m,0)[0])
        self.assertFalse(boot.evaluate(good+end+'AETHERI_BOOT_FAIL reason=nonce\n',m,0)[0])
        negative = "AETHERI_ROOT_FAIL reason=nonce\naetheri-root.service: Failed with result 'exit-code'.\nreboot: Power down\n"
        self.assertTrue(boot.evaluate(negative,m,0,expect_failure=True)[0])


if __name__ == '__main__':
    IMAGE = Path(sys.argv.pop(1)).resolve()
    unittest.main(verbosity=2)
