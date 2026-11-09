"""Projection/closure controls; native inputs are explicit test arguments only."""
import copy
import hashlib
import json
import os
from pathlib import Path
import shutil
import sys
import tempfile
import unittest
from unittest.mock import patch

import prepare_libinput_runtime as runtime
import build_desktop_probe as desktop

NATIVE_INPUTS=None
OLD_ABI=None


class Profile(unittest.TestCase):
    def test_complete_data_helpers_and_attribution(self):
        v=runtime.profile();self.assertEqual(len(v['files']),63)
        names={e['path'] for e in v['files']}
        self.assertEqual(sum(n.endswith('.quirks') for n in names),54)
        self.assertIn('usr/lib/udev/libinput-device-group',names)
        self.assertIn('usr/lib/udev/rules.d/90-libinput-fuzz-override.rules',names)
        self.assertIn('usr/share/licenses/libinput/COPYING',names)
        self.assertIn('usr/share/licenses/lua/COPYRIGHT',names)
        self.assertNotIn('usr/lib64/liblua-5.4.so',names)
        self.assertFalse(any('/include/' in n or '/man/' in n for n in names))

    def test_normal_default_remains_byte_exact(self):
        raw,v=desktop.runtime_selection()
        self.assertEqual(raw,runtime.BASE.read_bytes())
        self.assertNotIn('input_runtime',v)
        self.assertIn('/usr/lib64/liblua-5.4.so',{e['destination'] for e in v['files']})

    def test_unreviewed_base_or_missing_attribution_is_rejected(self):
        for change in ('base','notice','origin','counter','duplicate'):
            v=copy.deepcopy(runtime.profile())
            if change=='base':v['base_desktop_sha256']='0'*64
            elif change=='notice':v['components']['lua']['notices']=[]
            elif change=='origin':v['files'][0]['component']='foreign'
            elif change=='counter':v['files'][0]['input']='counter-build/foreign'
            else:v['files'][1]=copy.deepcopy(v['files'][0])
            with tempfile.TemporaryDirectory() as d:
                p=Path(d)/'profile.json';p.write_text(json.dumps(v))
                with patch.object(runtime,'PROFILE',p),self.assertRaises(ValueError):runtime.profile()


@unittest.skipUnless(os.name=='posix','Linux preparation, symlink and ELF boundary')
class Native(unittest.TestCase):
    def setUp(self):
        if NATIVE_INPUTS is None:self.skipTest('explicit native build/Lua/archive inputs required')
        self.tmp=tempfile.TemporaryDirectory(prefix='aetheri-input-runtime-');self.addCleanup(self.tmp.cleanup);self.root=Path(self.tmp.name).resolve()
        self.build,self.lua,self.archive=map(Path,NATIVE_INPUTS)
        self.output=self.root/'runtime'

    def prepare(self):return runtime.prepare(self.build,self.lua,self.archive,self.output)

    def test_actual_projection_and_desktop_overlay_preserve_other_owners(self):
        self.prepare();v=runtime.check(self.output);m=runtime.desktop_manifest(self.output)
        self.assertEqual(len(m['files']),1434)
        self.assertNotIn('/usr/lib64/liblua-5.4.so',{e['destination'] for e in m['files']})
        self.assertTrue(set(v['remove_packages']).isdisjoint(m['packages']))
        base=json.loads(runtime.BASE.read_bytes());kept=[e for e in base['files'] if e.get('package') not in v['remove_packages']]
        projected={e['destination']:e for e in m['files']}
        self.assertTrue(all(projected[e['destination']]==e for e in kept))
        self.assertEqual(desktop.runtime_selection(self.output)[1],m)
        marker=self.output/'keep';marker.write_bytes(b'original private tree')
        with self.assertRaises(FileExistsError):self.prepare()
        self.assertEqual(marker.read_bytes(),b'original private tree')

    def test_corrupted_code_data_notice_rules_and_receipt_cannot_pass(self):
        self.prepare()
        for name in ('usr/lib64/libinput.so.10.13.0','usr/share/libinput/50-system-hp.quirks','usr/share/licenses/lua/COPYRIGHT','usr/lib/udev/rules.d/80-libinput-device-groups.rules','inputs.json'):
            p=self.output/name;old=p.read_bytes();p.write_bytes(old+b'changed')
            with self.subTest(name=name),self.assertRaises((ValueError,json.JSONDecodeError)):runtime.check(self.output)
            p.write_bytes(old)
        extra=self.output/'unexplained';extra.mkdir()
        with self.assertRaisesRegex(ValueError,'unexplained'):runtime.check(self.output)

    def test_alias_and_mode_drift_are_rejected(self):
        self.prepare();alias=self.output/'usr/lib64/libinput.so.10';alias.unlink();alias.symlink_to('liblua-5.5.so')
        with self.assertRaisesRegex(ValueError,'alias'):runtime.check(self.output)
        alias.unlink();alias.symlink_to('libinput.so.10.13.0')
        code=self.output/'usr/lib64/libinput.so.10.13.0';code.chmod(0o777)
        with self.assertRaisesRegex(ValueError,'mode'):runtime.check(self.output)

    def test_changed_input_rejected_before_output(self):
        changed=self.root/'build';changed.mkdir()
        for e in runtime.profile()['files']:
            if e['source']=='libinput_build':
                p=changed/e['input'];p.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(self.build/e['input'],p)
        p=changed/'stage/usr/lib64/libinput.so.10.13.0';p.write_bytes(b'counter library')
        with self.assertRaisesRegex(ValueError,'different selected input'):runtime.prepare(changed,self.lua,self.archive,self.output)
        self.assertFalse(self.output.exists())

    def test_user_owned_output_is_not_protected_admission(self):
        self.prepare()
        with self.assertRaisesRegex(ValueError,'unprotected'):runtime.check_protected(self.output)

    def test_actual_static_elf_audit_and_old_abi_counter(self):
        self.prepare();v=runtime.check(self.output)
        files=[dict(destination='/'+e['path']) for e in v['files']]
        manifest=dict(files=files,input_runtime=dict(profile_sha256=runtime.lf_hash(runtime.PROFILE)))
        result=runtime.audit_elfs(self.output,manifest)
        self.assertEqual(len(result['needed']),5)
        self.assertIn('liblua-5.5.so',result['needed']['/usr/lib64/libinput.so.10.13.0'])
        # The caller supplies an independently selected original library as the
        # fourth argument; no fake ELF output or weakened reader is used.
        if OLD_ABI is not None:
            shutil.copyfile(OLD_ABI,self.output/'usr/lib64/libinput.so.10.13.0')
            with self.assertRaisesRegex(ValueError,'old Lua ABI'):runtime.audit_elfs(self.output,manifest)


if __name__=='__main__':
    OLD_ABI=None
    if len(sys.argv)>3:
        NATIVE_INPUTS=[sys.argv.pop(1) for _ in range(3)]
        if len(sys.argv)>1:OLD_ABI=Path(sys.argv.pop(1))
    unittest.main()
