"""Backing-chain admission and real pinned qemu-img content comparisons."""
import copy
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

import image_io
import test_usb_parent_overlay as overlay


class OverlayContract(unittest.TestCase):
    def test_chain_requires_exact_formats_objects_and_equal_fixed_sizes(self):
        a=Path('/owned/new.qcow2').resolve();b=Path('/owned/base.raw').resolve();size=31457280000
        chain=[dict(filename=str(a),format='qcow2',**{'virtual-size':size,'full-backing-filename':str(b),'backing-filename-format':'raw'}),dict(filename=str(b),format='raw',**{'virtual-size':size})]
        overlay.chain_check(chain,a,b,size)
        variants=[[],chain[:1],chain+[chain[1]]]
        for index,key,value in [(0,'format','raw'),(1,'format','qcow2'),(0,'virtual-size',size+512),(1,'virtual-size',size-512),(0,'filename','/foreign/overlay'),(1,'filename','/foreign/base'),(0,'full-backing-filename','/foreign/base'),(0,'backing-filename-format','qcow2'),(1,'backing-filename','extra'),(0,'encrypted',True)]:
            bad=copy.deepcopy(chain);bad[index][key]=value;variants.append(bad)
        for bad in variants:
            with self.assertRaises(ValueError):overlay.chain_check(bad,a,b,size)

    def test_only_two_explicit_cow_drives_replace_the_owned_raw_media(self):
        a=Path('/owned/primary.qcow2');b=Path('/owned/external.qcow2')
        args=overlay.command(Path('/image'),Path('/owned'),a,b,'11111111-2222-3333-4444-555555555555')
        drives=[args[i+1] for i,v in enumerate(args[:-1]) if v=='-drive']
        self.assertEqual(len(drives),4);self.assertEqual(sum('format=qcow2,' in v for v in drives),2)
        self.assertTrue(any('format=qcow2,file='+str(a) in v for v in drives));self.assertTrue(any('format=qcow2,file='+str(b) in v for v in drives))
        for forbidden in ('-qmp','-netdev','-kernel','-initrd','-append','-virtfs','-fsdev'):self.assertNotIn(forbidden,args)

    @unittest.skipUnless(os.name=='nt','real executable check is for the pinned Windows QEMU extraction')
    def test_real_overlay_integrity_equal_content_and_different_content(self):
        tool,_=overlay.image_tool();parent=Path(__file__).resolve().parents[1]/'local'
        with tempfile.TemporaryDirectory(dir=parent) as directory:
            root=Path(directory).resolve();base=root/'base.raw';other=root/'other.raw';cow=root/'fresh.qcow2';size=8*1024**2
            for path,text in [(base,b'base'),(other,b'else')]:
                with path.open('x+b') as f:image_io.sparse_file(f);image_io.set_length(f,size);f.write(text)
            def run(args):return subprocess.run([str(tool),*args],capture_output=True,timeout=30)
            self.assertEqual(run(['create','-q','-f','qcow2','-F','raw','-b',str(base),str(cow)]).returncode,0)
            result=run(['info','-f','qcow2','--output=json','--backing-chain',str(cow)]);self.assertEqual(result.returncode,0)
            overlay.chain_check(json.loads(result.stdout),cow,base,size)
            self.assertEqual(run(['check','-f','qcow2','--output=json',str(cow)]).returncode,0)
            self.assertEqual(run(['compare','-q','-f','qcow2','-F','raw',str(cow),str(base)]).returncode,0)
            self.assertEqual(run(['compare','-q','-f','qcow2','-F','raw',str(cow),str(other)]).returncode,1)


if __name__=='__main__':unittest.main(verbosity=2)
