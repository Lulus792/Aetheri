"""Host-only input/replay controls; never executes RPM or changes a guest."""
import copy
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import prepare_audio_lab_runtime as runtime


class RuntimeInputs(unittest.TestCase):
    def test_reviewed_profile_contains_only_eight_selected_packages(self):
        value,digest=runtime.profile()
        self.assertEqual({p['name'] for p in value['packages']},runtime.NAMES)
        self.assertEqual(len(value['packages']),8)
        self.assertEqual(len(digest),64)
        self.assertTrue(all(p['metadata'][1] and p['metadata'][2].endswith('.src.rpm') for p in value['packages']))

    def test_changed_family_duplicate_and_invalid_evidence_rejected(self):
        original=runtime.profile()[0]
        variants=[]
        for field,bad in (('name','unreviewed-1-1.x86_64.rpm'),('size',True),('sha256','x'*64),('scripts_sha256','0'*63)):
            value=copy.deepcopy(original);value['packages'][0][field]=bad;variants.append(value)
        value=copy.deepcopy(original);value['packages'][-1]=copy.deepcopy(value['packages'][0]);variants.append(value)
        value=copy.deepcopy(original);value['signer']='0'*40;variants.append(value)
        value=copy.deepcopy(original);value['format']=True;variants.append(value)
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);(root/'spec').mkdir()
            for index,value in enumerate(variants):
                (root/'spec/audio-lab-runtime.json').write_text(json.dumps(value))
                with self.subTest(index=index),patch.object(runtime,'ROOT',root),self.assertRaises(ValueError):runtime.profile()

    def test_nonroot_guard_precedes_guest_paths_and_effects(self):
        with patch.object(runtime.os,'geteuid',return_value=1000,create=True),patch.object(runtime,'output',side_effect=AssertionError('unexpected subprocess')):
            with self.assertRaisesRegex(ValueError,'root-only'):runtime.execute('apply',Path('/not-a-guest-input'))

    def test_existing_terminal_or_unknown_intent_never_replays(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);journal=root/'transaction.json'
            guard_inode=None
            for state in ('pending','needs-inspection','applied-reboot-required'):
                raw=json.dumps(dict(state=state)).encode();journal.write_bytes(raw)
                with patch.object(runtime,'guard'),patch.object(runtime,'inventory',side_effect=AssertionError('replay inventory')),patch.object(runtime.prepare_audio,'sealed_package',side_effect=AssertionError('replay package')):
                    with self.assertRaisesRegex(ValueError,'without replay'):runtime.execute('apply',root)
                self.assertEqual(journal.read_bytes(),raw)
                inode=(root/'provisioning.guard').stat().st_ino
                if guard_inode is None:guard_inode=inode
                self.assertEqual(inode,guard_inode)


if __name__=='__main__':unittest.main()
