"""Corruption/no-spawn checks against a built image; does not start a VM."""
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch
import zlib

import test_boot_probe as boot
import test_uefi_probe as runner
import uefi_probe_image as media

IMAGE = None


class MediaContract(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.manifest = media.inspect(IMAGE)
        cls.disk = (IMAGE/"boot.raw").read_bytes()
        cls.efi = (IMAGE/"BOOTX64.EFI").read_bytes()
        cls.volume = media.esp_extent(cls.disk, cls.manifest)

    def test_exact_gpt_fat_fallback_and_uki_match(self):
        self.assertEqual(media.boot_file(self.volume), self.efi)
        self.assertEqual(media.digest(media.pe_sections(self.efi)[".linux"]), self.manifest["sections"][".linux"])

    def test_both_gpt_checksums_are_required(self):
        for offset in (512+16, len(self.disk)-512+16):
            changed = bytearray(self.disk)
            changed[offset] ^= 1
            with self.assertRaisesRegex(ValueError, "checksum"):
                media.esp_extent(changed, self.manifest)

    def test_rechecksummed_wrong_partition_is_rejected(self):
        changed = bytearray(self.disk)
        last = len(changed)//512-1
        for header, array in ((1, 2), (last, last-32)):
            struct.pack_into("<Q", changed, array*512+32, 4096)
            entries = changed[array*512:array*512+128*128]
            struct.pack_into("<I", changed, header*512+88, zlib.crc32(entries))
            struct.pack_into("<I", changed, header*512+16, 0)
            struct.pack_into("<I", changed, header*512+16, zlib.crc32(changed[header*512:header*512+92]))
        with self.assertRaisesRegex(ValueError, "extent"):
            media.esp_extent(changed, self.manifest)

    def test_fat_mirrors_and_cycles_reject(self):
        reserved = struct.unpack_from("<H", self.volume, 14)[0]
        sectors = struct.unpack_from("<I", self.volume, 36)[0]
        root = struct.unpack_from("<I", self.volume, 44)[0]
        changed = bytearray(self.volume)
        changed[reserved*512+root*4] ^= 1
        with self.assertRaisesRegex(ValueError, "copies"):
            media.boot_file(changed)
        for start in (reserved, reserved+sectors):
            struct.pack_into("<I", changed, start*512+root*4, root)
        with self.assertRaisesRegex(ValueError, "cyclic"):
            media.boot_file(changed)

    def test_wrong_pe_target_duplicate_and_overlapping_sections_reject(self):
        pe = struct.unpack_from("<I", self.efi, 60)[0]
        table = pe+24+struct.unpack_from("<H", self.efi, pe+20)[0]
        variants = []
        changed = bytearray(self.efi)
        struct.pack_into("<H", changed, pe+4, 0x14c)
        variants.append(changed)
        changed = bytearray(self.efi)
        changed[table+40:table+48] = changed[table:table+8]
        variants.append(changed)
        changed = bytearray(self.efi)
        changed[table+40+20:table+40+24] = changed[table+20:table+24]
        variants.append(changed)
        for changed in variants:
            with self.assertRaises(ValueError):
                media.pe_sections(changed)

    def test_fixed_command_has_only_readonly_boot_media_and_no_direct_kernel(self):
        out = boot.ROOT/"out"
        out.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="uefi-contract-", dir=out) as temporary:
            target = Path(temporary)/"new"
            with patch.object(boot, "host_tools", return_value={}), patch.object(boot, "observe") as observe, patch.object(runner.shutil, "copyfile") as copy:
                runner.run(IMAGE, target)
                copy.assert_called_once_with(boot.TOOLS/"share/edk2-i386-vars.fd", target/"nvram.fd")
                command = observe.call_args.args[1]
                self.assertTrue(all(item not in command for item in ("-kernel", "-initrd", "-append", "-netdev", "-qmp", "-fsdev")))
                self.assertIn("usb-storage,bus=xhci.0,drive=bootmedia,removable=on,bootindex=1", command)
                self.assertIn("if=none,id=bootmedia,format=raw,readonly=on,file="+str(IMAGE/"boot.raw"), command)
                observe.reset_mock()
                with self.assertRaises(FileExistsError):
                    runner.run(IMAGE, target)
                observe.assert_not_called()

    def test_result_requires_exact_native_success_and_service_poweroff(self):
        m = self.manifest
        line = f"AETHERI_BOOT_PASS nonce={m['nonce']} kernel={m['kernel_release']} arch=x86_64 pid1=systemd uefi=1 ram_root=1 cgroup2=1 boot=11111111-1111-1111-1111-111111111111\n"
        end = "aetheri-boot.service: Deactivated successfully.\nreboot: Power down\n"
        self.assertTrue(boot.evaluate(line+end, m, 0)[0])
        for serial, code, timeout in ((end, 0, False), (line, 0, False), (line+end, 1, False), (line+end, 0, True), (line+line+end, 0, False), (line.replace(m['nonce'], "0"*32)+end, 0, False), (line+end+"AETHERI_BOOT_FAIL reason=nonce\n", 0, False)):
            self.assertFalse(boot.evaluate(serial, m, code, timeout)[0])
        negative = "AETHERI_BOOT_FAIL reason=nonce\naetheri-boot.service: Failed with result 'exit-code'.\nreboot: Power down\n"
        self.assertTrue(boot.evaluate(negative, m, 0, expect_failure=True)[0])
        self.assertFalse(boot.evaluate(line+negative, m, 0, expect_failure=True)[0])


if __name__ == "__main__":
    IMAGE = Path(sys.argv.pop(1)).resolve()
    unittest.main(verbosity=2)
