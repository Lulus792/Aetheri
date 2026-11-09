"""Host-only carrier isolation checks; no VM, installer, network or device write."""
import copy
import json
from pathlib import Path
import tempfile
from types import SimpleNamespace
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

    def test_hda_has_only_owned_wav_output_and_no_host_backend(self):
        module=lab.runtime();capture=module.VM/'hda'/('a'*32)/'capture.wav'
        with patch.object(lab,'load',return_value=self.ready()):
            plain=lab.command(module,False);audio=lab.command(module,False,capture)
            self.assertNotIn('-audiodev',plain)
            self.assertIn('none,id=audio,out.frequency=48000,out.channels=2,out.format=s16',audio)
            self.assertIn('-S',audio);self.assertIn('-no-shutdown',audio)
            self.assertNotIn('-S',plain);self.assertNotIn('-no-shutdown',plain)
            monitors=[audio[i+1] for i,v in enumerate(audio) if v=='-qmp']
            self.assertEqual(monitors,[f'unix:{module.VM / "qmp.sock"},server=on,wait=off',f'unix:{module.VM / "qmp-capture.sock"},server=on,wait=off'])
            self.assertIn('hda-output,bus=hda.0,audiodev=audio',audio)
            self.assertFalse(any('hda-duplex' in x or 'host_device' in x for x in audio))
            for wrong in (run_vm.VM/'capture.wav',module.VM/'capture.wav',module.VM/'hda'/'foreign'/'capture.wav'):
                with self.assertRaises(ValueError):lab.command(module,False,wrong)
            with self.assertRaises(ValueError):lab.command(module,True,capture)

    def test_file_replacement_witness_differs(self):
        with tempfile.TemporaryDirectory() as d:
            path=Path(d)/'image';path.write_bytes(b'a');before=lab.identity(path)
            path.rename(Path(d)/'original');path.write_bytes(b'a')
            self.assertNotEqual(lab.identity(path),before)

    def test_capture_finalizes_only_after_guest_poweroff(self):
        self.capture_case()

    def test_capture_diagnostic_preserves_original_without_cont_or_stop(self):
        self.capture_case(diagnostic=True)

    def capture_case(self,diagnostic=False):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);(root/'qmp-capture.sock').touch();capture=root/'capture.wav';capture.touch()
            audio=dict(state='prepared',object=lab.identity(capture));calls=[];saved=[]
            process=SimpleNamespace(poll=lambda:None,wait=lambda timeout:0)
            class Monitor:
                def __init__(self,*args):self.state=args[3];self.state['events']=[];self.listings=0;self.closed=False
                def execute(self,name,args=None):
                    calls.append((name,args));command=(args or {}).get('command-line','')
                    if command=='info capture':
                        self.listings+=1
                        if self.listings!=2:return ''
                        return 'unexpected capture' if diagnostic else '[0]: Capturing audio(48000,16,2) to '+capture.as_posix()+': 0 bytes\r\n'
                    if name=='query-status':return dict(running=False,status='shutdown')
                    return ''
                def poll(self,timeout):self.state['events'].append(dict(event='SHUTDOWN',data=dict(guest=True,reason='guest-shutdown')))
                def close(self):self.closed=True
            monitor=Monitor(None,None,None,{},None)
            with patch.object(lab.image_qmp,'Connection',return_value=monitor):
                # Connection normally receives and owns this same journal object.
                audio['qmp']=monitor.state
                if diagnostic:
                    with self.assertRaises(ValueError):lab.capture_session(SimpleNamespace(VM=root),process,'fixed-test-uuid',audio,capture,lambda:saved.append(audio['state']))
                    self.assertNotIn(('cont',None),calls)
                    self.assertFalse(any('stopcapture' in str(args) or name=='quit' for name,args in calls))
                else:
                    lab.capture_session(SimpleNamespace(VM=root),process,'fixed-test-uuid',audio,capture,lambda:saved.append(audio['state']))
                    self.assertEqual(audio['state'],'capture-finalized')
                    self.assertTrue(audio['quit_acknowledged'])
                    self.assertLess(calls.index(('query-status',None)),calls.index(('human-monitor-command',{'command-line':'stopcapture 0'})))
                    self.assertLess(calls.index(('human-monitor-command',{'command-line':'stopcapture 0'})),calls.index(('quit',None)))
                self.assertTrue(monitor.closed)

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
