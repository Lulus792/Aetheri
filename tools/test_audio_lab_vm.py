"""Host-only carrier isolation checks; no VM, installer, network or device write."""
import copy
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import audio_lab_vm as lab
import run_vm


class Isolation(unittest.TestCase):
    def test_context_does_not_retarget_original(self):
        original=(run_vm.VM,run_vm.SSH_PORT,run_vm.HTTP_PORT)
        module=lab.runtime()
        self.assertEqual(original,(run_vm.VM,run_vm.SSH_PORT,run_vm.HTTP_PORT))
        self.assertNotEqual(module.VM,run_vm.VM)
        self.assertEqual(module.VM,lab.ROOT/'vm/audio-lab')
        self.assertEqual((module.SSH_PORT,module.HTTP_PORT),(19223,19447))

    def ready(self):
        return dict(format=1,profile='audio-lab',uuid='11111111-1111-4111-8111-111111111111',disk_serial=lab.SERIAL,disk_bytes=lab.DISK,stage='ready',
            template_sha256='0'*64,kickstart_sha256='0'*64,objects={n:dict(device=1,inode=i+1) for i,n in enumerate(('system.qcow2','nvram.fd','id_ed25519','id_ed25519.pub','http/carrier.ks'))})

    def test_foreign_registration_and_geometry_rejected(self):
        value=self.ready();self.assertEqual(lab.validate(value),value)
        for key,wrong in [('format',True),('profile','p0'),('disk_bytes',48*1024**3),('disk_serial','AETHERI_P0_ONLY'),('uuid','11111111-1111-1111-8111-111111111111'),('stage','adopt')]:
            with self.subTest(key=key),self.assertRaises(ValueError):lab.validate({**value,key:wrong})
        bad=copy.deepcopy(value);bad['objects']['system.qcow2']['inode']=0
        with self.assertRaises(ValueError):lab.validate(bad)

    def test_boot_has_only_own_disk_and_local_qmp(self):
        module=lab.runtime()
        with patch.object(lab,'load',return_value=self.ready()):args=lab.command(module,False)
        self.assertEqual(args[args.index('-qmp')+1],f'unix:{module.VM / "qmp.sock"},server=on,wait=off')
        self.assertEqual(args[args.index('-m')+1],'4096M')
        disks=[args[i+1] for i,x in enumerate(args) if x=='-drive']
        self.assertEqual(len(disks),3)
        self.assertTrue(any(str(module.VM/'system.qcow2') in x for x in disks))
        self.assertFalse(any(str(run_vm.VM) in x for x in disks))
        self.assertIn('restrict=on',args[args.index('-netdev')+1])
        self.assertNotIn('tcp:',args[args.index('-qmp')+1])
        self.assertFalse(any('host_device' in x or '\\\\.\\PhysicalDrive' in x or 'usb-host' in x for x in args))

    def test_template_guards_match_own_geometry_and_no_wheel(self):
        text=(lab.ROOT/'spec/audio-lab.ks.in').read_text()
        for required in ('34359738368','virtio-AETH_AUDIO_LAB','Aetheri audio lab VM','selinux --enforcing','CapabilityBoundingSet=','NoNewPrivileges=yes','ignoredisk --only-use=vda'):
            self.assertIn(required,text)
        self.assertNotIn('--groups=wheel',text)
        self.assertNotIn('AETHERI_P0_ONLY',text)

    def test_file_replacement_witness_differs(self):
        with tempfile.TemporaryDirectory() as d:
            path=Path(d)/'image';path.write_bytes(b'a');before=lab.identity(path)
            path.rename(Path(d)/'original');path.write_bytes(b'a')
            self.assertNotEqual(lab.identity(path),before)

    def test_installer_exit_and_complete_console_required(self):
        identifier='11111111-1111-4111-8111-111111111111'
        record=dict(uuid=identifier,install=True,state='exited',exit=0)
        serial='Running post-installation scripts\r\n\x1b[1;24r\r\nComplete!\r\nreboot: Power down\r\n'
        self.assertTrue(lab.installation_complete(record,serial,identifier))
        for key,value in [('uuid','other'),('install',False),('state','spawned'),('exit',1),('exit',False)]:
            with self.subTest(key=key,value=value):self.assertFalse(lab.installation_complete({**record,key:value},serial,identifier))
        for broken in ('',serial.replace('Complete!','error'),serial.replace('reboot: Power down',''),serial.replace('Running post-installation scripts','')):
            self.assertFalse(lab.installation_complete(record,broken,identifier))


if __name__=='__main__':unittest.main()
