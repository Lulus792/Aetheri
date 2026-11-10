"""Current runtime byte/notice/source guards; no service, device or image effects."""
import copy
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

import prepare_current_desktop_runtime as runtime


def fixture():
    def entry(name, raw, role):
        return dict(provider='input', input=name, destination='/usr/'+name,
                    size=len(raw), sha256=hashlib.sha256(raw).hexdigest(), mode=0o755 if role=='library' else 0o644,
                    component='example', role=role)
    return dict(format=1, purpose=runtime.PURPOSE, variant='release',
                files=[entry('lib64/example.so', b'original', 'library'), entry('share/licenses/example/LICENSE', b'license', 'notice')],
                components={'example':dict(origin='independent fixed test inputs',notices=['/usr/share/licenses/example/LICENSE'])},
                symlinks={})


class Runtime(unittest.TestCase):
    def sources(self, root, value):
        source=root/'input';source.mkdir()
        for e,raw in zip(value['files'],(b'original',b'license')):
            p=source/e['input'];p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(raw)
        return dict(input=str(source))

    def test_real_profile_has_nine_apps_and_separate_test_client_with_notices(self):
        value=runtime.profile();programs={Path(e['destination']).name for e in value['files'] if e['role']=='program'}
        self.assertEqual(len(programs),10)
        self.assertIn('aetheri-text-editor',programs);self.assertIn('test_compositor_client',programs)
        self.assertFalse(any('liblua-5.4' in e['destination'] for e in value['files']))
        self.assertEqual(sum(e['destination'].endswith('.quirks') for e in value['files']),54)

    def test_prepare_check_and_existing_output_is_never_adopted(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory).resolve();value=fixture();sources=self.sources(root,value);target=root/'output'
            runtime.prepare(sources,target,value)
            self.assertEqual(runtime.check(target,value)['files'],2)
            before=(target/'runtime.json').read_bytes()
            with self.assertRaises(FileExistsError):runtime.prepare(sources,target,value)
            self.assertEqual((target/'runtime.json').read_bytes(),before)

    def test_changed_source_rejected_before_destination_creation(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory).resolve();value=fixture();sources=self.sources(root,value)
            (root/'input/lib64/example.so').write_bytes(b'changed!')
            with self.assertRaisesRegex(ValueError,'selected runtime bytes'):runtime.prepare(sources,root/'output',value)
            self.assertFalse((root/'output').exists())

    def test_self_record_cannot_authorize_changed_body_or_unexplained_empty_directory(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory).resolve();value=fixture();sources=self.sources(root,value);target=root/'output'
            runtime.prepare(sources,target,value)
            body=target/'usr/lib64/example.so';body.write_bytes(b'changed!')
            with self.assertRaisesRegex(ValueError,'selected runtime bytes'):runtime.check(target,value)
            body.write_bytes(b'original');(target/'unexplained').mkdir()
            with self.assertRaisesRegex(ValueError,'unexplained'):runtime.check(target,value)

    def test_foreign_notice_and_escaping_alias_are_rejected(self):
        for mutate in (lambda v:v['components']['example']['notices'].append('/usr/share/licenses/foreign/LICENSE'),
                       lambda v:v['symlinks'].update({'/usr/lib64/alias.so':'../../outside'})):
            value=fixture();mutate(value)
            with self.assertRaises(ValueError):runtime.validate(value)

    def test_different_provider_or_overlapping_output_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory).resolve();value=fixture();sources=self.sources(root,value)
            with self.assertRaisesRegex(ValueError,'provider set'):runtime.prepare({},root/'output',value)
            with self.assertRaisesRegex(ValueError,'overlapping'):runtime.prepare(sources,root/'input/nested',value)
            self.assertFalse((root/'input/nested').exists())

    def test_original_alias_roundtrip_and_changed_target_rejection(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory).resolve();value=fixture();sources=self.sources(root,value)
            probe=root/'probe'
            try:probe.symlink_to('unused')
            except OSError as error:self.skipTest(str(error))
            probe.unlink();value['symlinks']={'/usr/lib64/alias.so':'example.so'};target=root/'output'
            runtime.prepare(sources,target,value)
            self.assertEqual(runtime.check(target,value)['aliases'],1)
            alias=target/'usr/lib64/alias.so';alias.unlink();alias.symlink_to('../foreign')
            with self.assertRaisesRegex(ValueError,'runtime alias'):runtime.check(target,value)

    @unittest.skipUnless(sys.platform=='linux','native FIFO source class')
    def test_fifo_is_rejected_without_waiting_for_a_writer(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory).resolve();value=fixture();sources=self.sources(root,value)
            p=root/'input/lib64/example.so';p.unlink();os.mkfifo(p)
            (root/'profile.json').write_text(json.dumps(value));(root/'sources.json').write_text(json.dumps(sources))
            result=subprocess.run([sys.executable,str(Path(runtime.__file__).resolve()),'prepare',str(root/'output'),
                                   '--sources',str(root/'sources.json'),'--profile',str(root/'profile.json')],capture_output=True,timeout=5)
            self.assertNotEqual(result.returncode,0);self.assertIn(b'source class/size',result.stderr)
            self.assertFalse((root/'output').exists())


if __name__=='__main__':unittest.main()
