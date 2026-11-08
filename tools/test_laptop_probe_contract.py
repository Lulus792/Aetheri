"""Pinned non-executable additions and console setup independent of a UART."""
import json
from pathlib import Path
import tempfile
import unittest

import build_laptop_probe as laptop
import desktop_payload
import build_usb_root_probe


class LaptopContract(unittest.TestCase):
    def test_desktop_console_never_requests_an_absent_uart(self):
        self.assertEqual(build_usb_root_probe.console_option(True),'console=tty0')
        self.assertEqual(build_usb_root_probe.console_option(False),'console=ttyS0,115200n8')
        for value in (None,0,1,'true'):
            with self.assertRaises(ValueError):build_usb_root_probe.console_option(value)

    def fixture(self,root):
        a=root/'etc/systemd/journald.conf.d/aetheri-development.conf';b=root/'etc/systemd/system/aetheri-graphics.service'
        for p in (a,b):p.parent.mkdir(parents=True,exist_ok=True)
        a.write_text('[Journal]\nStorage=volatile\nTTYPath=/dev/ttyS0\n')
        b.write_text('[Service]\nUser=aetheri\nNoNewPrivileges=yes\nStandardOutput=file:/dev/ttyS0\nStandardError=file:/dev/ttyS0\n')
        return a,b

    def test_console_uses_kernel_console_and_graphics_uses_bounded_journal(self):
        with tempfile.TemporaryDirectory() as temporary:
            a,b=self.fixture(Path(temporary));laptop.configure_console(Path(temporary))
            self.assertIn('Storage=volatile\nTTYPath=/dev/console\n',a.read_text())
            self.assertIn('StandardOutput=journal\nStandardError=journal\n',b.read_text())
            self.assertIn('User=aetheri\nNoNewPrivileges=yes\n',b.read_text())
            before=(a.read_bytes(),b.read_bytes())
            with self.assertRaises(ValueError):laptop.configure_console(Path(temporary))
            self.assertEqual((a.read_bytes(),b.read_bytes()),before)

    def test_foreign_or_ambiguous_input_is_rejected_before_either_edit(self):
        for bad in ('missing','duplicate'):
            with tempfile.TemporaryDirectory() as temporary:
                a,b=self.fixture(Path(temporary))
                b.write_text(b.read_text().replace('StandardOutput=file:/dev/ttyS0','StandardOutput=null') if bad=='missing' else b.read_text()+b.read_text())
                before=(a.read_bytes(),b.read_bytes())
                with self.assertRaises(ValueError):laptop.configure_console(Path(temporary))
                self.assertEqual((a.read_bytes(),b.read_bytes()),before)

    def test_additions_are_same_kernel_modules_one_dmc_and_its_license(self):
        pins=json.loads((laptop.ROOT/'spec/laptop-probe-inputs.json').read_text())
        self.assertLess(desktop_payload.validate(pins),3*1024**2)
        self.assertEqual(pins['kernel_release'],'6.19.10-300.fc44.x86_64')
        modules=[];other=[]
        for entry in pins['files']:
            self.assertEqual(entry['mode'],0o644)
            if '/lib/modules/' in entry['destination']:
                self.assertTrue(entry['destination'].startswith('/usr/lib/modules/'+pins['kernel_release']+'/kernel/'))
                self.assertTrue(entry['destination'].endswith('.ko.xz'));modules.append(entry)
            else:other.append(entry['destination'])
        self.assertEqual(len(modules),18)
        self.assertEqual(set(other),{'/usr/lib/firmware/i915/kbl_dmc_ver1_04.bin.xz','/usr/share/licenses/intel-gpu-firmware/LICENSE.i915'})


if __name__=='__main__':unittest.main(verbosity=2)
