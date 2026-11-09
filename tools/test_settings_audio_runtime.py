"""Runtime projection tests; no bus, VM, policy or package installation."""
import copy
import hashlib
import json
import os
from pathlib import Path
import tempfile
import unittest

import prepare_settings_audio_lab as runtime


class Projection(unittest.TestCase):
    def setUp(self):
        self.temporary=tempfile.TemporaryDirectory(prefix='aetheri-sound-runtime-')
        self.addCleanup(self.temporary.cleanup);self.base=Path(self.temporary.name).resolve()
        self.inputs=self.base/'inputs';self.inputs.mkdir();self.tree=self.base/'tree'
        self.files=[]
        for name,role,data in [('library','library',b'bounded runtime'),('license','notice',b'original attribution')]:
            (self.inputs/name).write_bytes(data)
            self.files.append(dict(source=name,destination='/usr/'+('lib64/libfixture.so.0' if role=='library' else 'share/licenses/fixture/LICENSE'),
                size=len(data),sha256=hashlib.sha256(data).hexdigest(),mode=0o755 if role=='library' else 0o644,
                component='fixture',role=role,package=None,provenance='independently selected fixture package'))
        self.value=dict(format=1,purpose='settings-audio-lab',files=self.files,
            components=dict(fixture=dict(origin='independently selected fixture package',notices=[self.files[1]['destination']])))

    def test_copy_and_check_require_both_code_and_original_attribution(self):
        result=runtime.prepare(self.inputs,self.tree,self.value)
        self.assertEqual(result['files'],2);self.assertEqual(result['components'],1)
        for entry in self.files:self.assertEqual((self.tree/entry['destination'].lstrip('/')).read_bytes(),(self.inputs/entry['source']).read_bytes())
        self.assertEqual(runtime.check(self.tree,self.value),result)

    def test_missing_or_foreign_notice_is_rejected_before_output_creation(self):
        for mutate in ('missing','foreign','unlisted'):
            value=copy.deepcopy(self.value)
            if mutate=='missing':value['components']['fixture']['notices']=[]
            elif mutate=='foreign':value['components']['fixture']['notices']=[self.files[0]['destination']]
            else:value['files'].pop()
            with self.assertRaises(ValueError):runtime.prepare(self.inputs,self.tree,value)
            self.assertFalse(self.tree.exists())

    def test_changed_notice_and_unexplained_output_are_detected(self):
        runtime.prepare(self.inputs,self.tree,self.value)
        notice=self.tree/self.files[1]['destination'].lstrip('/')
        notice.write_bytes(b'altered attribution')
        with self.assertRaises(ValueError):runtime.check(self.tree,self.value)
        notice.write_bytes((self.inputs/'license').read_bytes());notice.chmod(0o644)
        (self.tree/'foreign').write_bytes(b'foreign file')
        with self.assertRaisesRegex(ValueError,'unexplained'):runtime.check(self.tree,self.value)
        (self.tree/'foreign').unlink();(self.tree/'foreign-directory').mkdir()
        with self.assertRaisesRegex(ValueError,'unexplained'):runtime.check(self.tree,self.value)

    def test_failed_input_copy_preserves_partial_output_and_refuses_retry(self):
        (self.inputs/'library').write_bytes(b'changed runtime')
        with self.assertRaises(ValueError):runtime.prepare(self.inputs,self.tree,self.value)
        self.assertTrue((self.tree/'usr/lib64/libfixture.so.0').exists())
        with self.assertRaises(FileExistsError):runtime.prepare(self.inputs,self.tree,self.value)

    def test_canonical_paths_and_destination_ownership_are_required(self):
        for key,value in [('source','.'),('source','../library'),('source','./library'),('destination','/usr/../foreign'),('destination','/etc/profile'),('mode',0o4755),('size',True)]:
            changed=copy.deepcopy(self.value);changed['files'][0][key]=value
            with self.assertRaises(ValueError):runtime.profile(changed)
        for key in ('source','destination'):
            changed=copy.deepcopy(self.value);changed['files'][1][key]=changed['files'][0][key]
            with self.assertRaises(ValueError):runtime.profile(changed)

    def test_existing_foreign_output_is_not_adopted(self):
        self.tree.mkdir();(self.tree/'foreign').write_bytes(b'keep')
        with self.assertRaises(FileExistsError):runtime.prepare(self.inputs,self.tree,self.value)
        self.assertEqual((self.tree/'foreign').read_bytes(),b'keep')

    def test_file_cannot_claim_a_different_component_origin(self):
        for key,value in [('provenance','foreign source'),('package','other-1-1.x86_64')]:
            changed=copy.deepcopy(self.value);changed['files'][0][key]=value
            with self.assertRaisesRegex(ValueError,'origin'):runtime.prepare(self.inputs,self.tree,changed)
            self.assertFalse(self.tree.exists())

    def test_input_output_overlap_is_rejected(self):
        for tree in (self.inputs,self.inputs/'nested',self.base):
            with self.assertRaises(ValueError):runtime.prepare(self.inputs,tree,self.value)

    @unittest.skipIf(os.name=='nt','Native Linux alias and file-mode checks')
    def test_input_and_output_parent_aliases_and_mode_drift_are_rejected(self):
        (self.inputs/'real').mkdir();(self.inputs/'alias').symlink_to('real',target_is_directory=True)
        changed=copy.deepcopy(self.value);changed['files'][0]['source']='alias/library'
        with self.assertRaisesRegex(ValueError,'parent'):runtime.prepare(self.inputs,self.tree,changed)
        self.assertFalse(self.tree.exists())
        runtime.prepare(self.inputs,self.tree,self.value)
        library=self.tree/'usr/lib64/libfixture.so.0';library.chmod(0o777)
        with self.assertRaisesRegex(ValueError,'mode'):runtime.check(self.tree,self.value)
        library.chmod(0o755)
        foreign=self.base/'foreign';(self.tree/'usr/lib64').rename(foreign)
        (self.tree/'usr/lib64').symlink_to(foreign,target_is_directory=True)
        with self.assertRaisesRegex(ValueError,'parent'):runtime.check(self.tree,self.value)


if __name__=='__main__':unittest.main()
