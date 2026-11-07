"""Explicit root-only loop-device test in a disposable enforcing P0 mount namespace.

Not a default host test. Set AETHERI_TEST_DEVELOPMENT_STORAGE=1 and invoke through
unshare --mount --propagation private. Only a new /run fixture is formatted.
"""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
import uuid

ENABLED = sys.platform == 'linux' and os.environ.get('AETHERI_TEST_DEVELOPMENT_STORAGE') == '1'
if sys.platform == 'linux':
    import development_storage_guest as guest


@unittest.skipUnless(ENABLED, 'explicit disposable Linux carrier test only')
class GuestStorage(unittest.TestCase):
    def test_original_format_mount_reopen_and_negative_guards(self):
        self.assertEqual(os.geteuid(), 0)
        self.assertEqual(guest.run('getenforce'), 'Enforcing')
        self.assertEqual(Path('/sys/class/dmi/id/product_name').read_text().strip(), 'Aetheri P0 disposable VM')
        self.assertNotEqual(os.readlink('/proc/self/ns/mnt'), os.readlink('/proc/1/ns/mnt'))
        # Fixed 64 MiB private image; this test adapter never selects a guest or
        # hardware disk by name. Production selection is tested separately below.
        with tempfile.TemporaryDirectory(prefix='aetheri-storage-check-', dir='/run') as directory:
            root = Path(directory)
            root.chmod(0o755)
            image = root/'blank.raw'
            with image.open('xb') as output: output.truncate(64*1024**2)
            loop = guest.run('losetup', '--find', '--show', '--nooverlap', str(image))
            self.assertRegex(loop, r'^/dev/loop[0-9]+$')
            def select(profile, exclusive=False):
                fd = os.open(loop, os.O_RDWR | os.O_CLOEXEC | (os.O_EXCL if exclusive else 0))
                return fd, Path(loop), os.fstat(fd).st_rdev
            token=uuid.uuid4()
            profile=dict(format=1, vm_uuid=Path('/sys/class/dmi/id/product_uuid').read_text().strip().lower(),
                         token=str(token), serial='AETHBLD_'+token.hex[:12], bytes=64*1024**2, fs_uuid=str(uuid.uuid4()))
            try:
                with patch.object(guest,'SIZE',64*1024**2), patch.object(guest,'STATE',root/'state'), \
                        patch.object(guest,'MOUNT',root/'mount'), patch.object(guest,'selected_device',side_effect=select):
                    with open(loop, 'r+b', buffering=0) as changed: changed.write(b'nonblank')
                    with self.assertRaisesRegex(RuntimeError,'entirely zero'): guest.initialize(profile)
                    self.assertFalse((root/'state/registration.json').exists())
                    with open(loop, 'r+b', buffering=0) as changed: changed.write(b'\0'*8)
                    result = guest.initialize(profile)
                    self.assertEqual(result['filesystem']['UUID'],profile['fs_uuid'])
                    self.assertGreater(result['available'],32*1024**2)
                    self.assertEqual(guest.load()['stage'],'ready')
                    work = root/'mount/work'
                    saved=work/'retain.txt'; saved.write_bytes(b'original developer bytes\n'); os.chown(saved,1000,1000)
                    # A normal UID1000 build can write and execute here without
                    # privilege or a new SELinux allow rule.
                    guest.run('sudo','-n','-u','aetheri','--','env','TMPDIR='+str(work),'sh','-c',
                              'printf "int main(void){return 0;}\\n" > "$1/probe.c"; cc "$1/probe.c" -o "$1/probe" && "$1/probe"',
                              'storage-test',str(work))
                    with self.assertRaisesRegex(RuntimeError,'existing enrollment'): guest.initialize(profile)
                    guest.run('umount',str(root/'mount'))
                    result=guest.mount_selected(guest.load())
                    self.assertEqual(saved.read_bytes(),b'original developer bytes\n')
                    self.assertEqual(result['enforcing'],'Enforcing')
                    for field,value in (('serial','AETHERI_P0_ONLY'),('bytes',True),('vm_uuid',str(uuid.uuid4()))):
                        changed=profile|{field:value}
                        with self.assertRaises((ValueError,RuntimeError)):
                            guest.require_carrier(guest.validate_profile(changed))
                    for state in ('formatting','unknown'):
                        with self.assertRaisesRegex(RuntimeError,'must not be replayed'):
                            guest.mount_selected(guest.load()|{'stage':state})
                    wrong=guest.load(); wrong['profile']=profile|{'fs_uuid':str(uuid.uuid4())}
                    with self.assertRaisesRegex(RuntimeError,'filesystem differs'):guest.mount_selected(wrong)
                    self.assertEqual(saved.read_bytes(),b'original developer bytes\n')
            finally:
                if os.path.ismount(root/'mount'): guest.run('umount',str(root/'mount'))
                guest.run('losetup','--detach',loop)

    def test_real_selector_cannot_select_regular_file_or_missing_serial(self):
        token=uuid.uuid4()
        profile=dict(format=1,vm_uuid=Path('/sys/class/dmi/id/product_uuid').read_text().strip().lower(),
            token=str(token),serial='AETHBLD_'+token.hex[:12],bytes=guest.SIZE,fs_uuid=str(uuid.uuid4()))
        with self.assertRaises(FileNotFoundError): guest.selected_device(profile)
        # Read-only admission logic also rejects the actual carrier root before
        # any filesystem probe or format permission. No bytes are written.
        with self.assertRaisesRegex(RuntimeError,'carrier root'):
            guest.selected_device(profile|{'serial':'AETHERI_P0_ONLY'})
        with tempfile.NamedTemporaryFile(dir='/run') as file:
            actual_open=os.open
            with patch.object(guest.os,'open',side_effect=lambda *a,**kw: actual_open(file.name,os.O_RDWR)):
                with self.assertRaisesRegex(RuntimeError,'not a block device'): guest.selected_device(profile)


if __name__ == '__main__':
    unittest.main()
