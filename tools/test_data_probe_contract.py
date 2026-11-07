"""Adversarial result/identity/dispatch checks, with no VM launch."""
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import json

import test_boot_probe as boot
import test_data_probe as runner


class DataContract(unittest.TestCase):
    def setUp(self):
        self.m=dict(proof='writable-data', nonce='a'*32, kernel_release='fixture')
        self.identifier='12345678-1234-1234-1234-123456789abc'
        self.root=f"AETHERI_ROOT_PASS nonce={'a'*32} kernel=fixture arch=x86_64 pid1=systemd uefi=1 ext4_ro=1 write_erofs=1 cgroup2=1 boot={self.identifier}\n"
        self.check=f"AETHERI_DATA_CHECK nonce={'a'*32}\n"
        self.user=f"AETHERI_DATA_USER nonce={'a'*32} counter=1 user=1000 cap_zero=1 tmp=1 runtime=1 boot={self.identifier}\n"
        self.result=f"AETHERI_DATA_PASS nonce={'a'*32} counter=1 user=1000 cap_zero=1 persisted=1 unmounted=1 boot={self.identifier}\n"
        self.end="aetheri-data-user.service: Deactivated successfully.\naetheri-data-finish.service: Deactivated successfully.\nreboot: Power down\n"
        self.good=self.root+self.check+self.user+self.result+self.end

    def test_requires_complete_same_boot_user_and_clean_unmount(self):
        self.assertTrue(boot.evaluate(self.good,self.m,0)[0])
        for part in (self.root,self.check,self.user,self.result,self.end):
            self.assertFalse(boot.evaluate(self.good.replace(part,''),self.m,0)[0])
        for text in ('user=1000','cap_zero=1','tmp=1','runtime=1','persisted=1','unmounted=1','ext4_ro=1','write_erofs=1'):
            self.assertFalse(boot.evaluate(self.good.replace(text,text+'0'),self.m,0)[0],text)
        self.assertFalse(boot.evaluate(self.good,self.m,1)[0])
        self.assertFalse(boot.evaluate(self.good,self.m,0,timed_out=True)[0])

    def test_mixed_foreign_duplicate_counter_and_boot_markers_reject(self):
        for changed in (self.good+self.root,self.good+self.user,self.good+self.result,self.good+'AETHERI_BOOT_PASS foreign\n',self.good+'AETHERI_ROOT_FAIL reason=nonce\n',self.good.replace(self.user,self.user.replace('counter=1','counter=2')),self.good.replace(self.result,self.result.replace(self.identifier,'0'*36)),self.good.replace(self.user,self.user.replace('a'*32,'b'*32))):
            self.assertFalse(boot.evaluate(changed,self.m,0)[0])

    def test_volume_rejection_has_no_user_write_or_success(self):
        failure="AETHERI_DATA_FAIL reason=volume-context\naetheri-data-check.service: Failed with result 'exit-code'.\nreboot: Power down\n"
        self.assertTrue(boot.evaluate(self.root+failure,self.m,0,expect_failure='volume-context')[0])
        for changed in (failure,self.root+failure+self.user,self.root+self.check+failure,self.root+failure+self.result,self.root+failure+failure):
            self.assertFalse(boot.evaluate(changed,self.m,0,expect_failure='volume-context')[0])
        self.assertFalse(boot.evaluate(self.root+failure,self.m,0)[0])

    def test_original_handle_rejects_name_replacement_size_drift_and_symlink(self):
        with tempfile.TemporaryDirectory() as d:
            path=Path(d)/'data.raw'
            with path.open('x+b') as held:
                held.write(b'original');held.flush()
                expected=runner.identity(os.fstat(held.fileno()))
                runner.check_held(held,path,expected)
                held.truncate(2)
                with self.assertRaises(ValueError):runner.check_held(held,path,expected)
                held.truncate(8)
                # Windows deliberately prevents unlinking an open ordinary file;
                # renamed/replaced-path behavior is exercised where available.
                if os.name!='nt':
                    path.rename(Path(d)/'retained')
                    path.write_bytes(b'original')
                    with self.assertRaises(ValueError):runner.check_held(held,path,expected)
                    path.unlink();path.symlink_to('retained')
                    with self.assertRaises(ValueError):runner.check_held(held,path,expected)

    def test_exact_two_disks_without_direct_kernel_network_or_qmp(self):
        args=runner.command(Path('/image'),Path('/attempt'),Path('/owned/data.raw'),'AETHDATA123456789abc')
        for forbidden in ('-kernel','-initrd','-append','-netdev','-qmp','-fsdev','-virtfs'):
            self.assertNotIn(forbidden,args)
        self.assertIn('if=none,id=bootmedia,format=raw,readonly=on,file='+str(Path('/image')/'boot.raw'),args)
        self.assertIn('if=none,id=userdata,format=raw,file='+str(Path('/owned/data.raw')),args)
        self.assertIn('virtio-blk-pci,drive=userdata,serial=AETHDATA123456789abc',args)
        self.assertEqual(args.count('-drive'),4)

    def test_uncertain_second_dispatch_never_replays_or_reinitializes(self):
        with tempfile.TemporaryDirectory() as d:
            image=Path(d)/'image';image.mkdir();output=Path(d)/'sequence'
            with (image/'data.raw').open('xb') as f:f.truncate(256*1024**2)
            m={**self.m, 'data_volume':dict(size=256*1024**2,serial='AETHDATA123456789abc',initial_sha256='0'*64)}
            def sparse_copy(source,target,limit):target.truncate(256*1024**2)
            def observe(attempt,*args):
                if attempt.name=='boot-2':raise RuntimeError('unknown dispatch')
                (attempt/'serial.log').write_text(self.result)
                return dict(boot_ids=[self.identifier])
            with patch.object(runner.media,'inspect',return_value=m),patch.object(boot,'host_tools',return_value={}),patch.object(boot,'sha',return_value='0'*64),patch.object(runner.shutil,'copyfileobj',side_effect=sparse_copy),patch.object(runner.shutil,'copyfile'),patch.object(boot,'observe',side_effect=observe) as dispatch:
                with self.assertRaisesRegex(RuntimeError,'unknown dispatch'):runner.run(image,output)
                self.assertEqual(dispatch.call_count,2)
                state=json.loads((output/'sequence.json').read_text())
                self.assertEqual(state['state'],'dispatch_pending')
                self.assertEqual(state['pending'],2)
                self.assertEqual(len(state['attempts']),1)
                self.assertTrue((output/'data.raw').is_file())
                with self.assertRaises(FileExistsError):runner.run(image,output)
                self.assertEqual(dispatch.call_count,2)


if __name__=='__main__':unittest.main(verbosity=2)
