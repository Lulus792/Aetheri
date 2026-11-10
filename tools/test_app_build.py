"""Independent component content pins and controlled ordinary app test scope."""
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import tempfile
import unittest
from types import SimpleNamespace

import build_desktop_apps as apps


class Build(unittest.TestCase):
    def test_private_desktop_host_binds_actual_reviewed_font_and_rejects_drift(self):
        path=apps.ROOT/'tests/development_session_fixture.py'
        spec=importlib.util.spec_from_file_location('app_build_fixture',path)
        fixture=importlib.util.module_from_spec(spec);spec.loader.exec_module(fixture)
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory).resolve();(root/'spec').mkdir()
            font=root/'prefix/share/aetheri-fonts/LiberationSans-Regular.ttf'
            font.parent.mkdir(parents=True);font.write_bytes(b'reviewed')
            (root/'spec/ui-inputs.json').write_text(json.dumps({'font':{'sha256':hashlib.sha256(b'reviewed').hexdigest()}}))
            row='AETHERI_SETTINGS_TEST_FONT:FILEPATH='+str(font)+'\n'
            (root/'CMakeCache.txt').write_text(row)
            session=SimpleNamespace(PROJECT=root,digest=lambda p:hashlib.sha256(p.read_bytes()).hexdigest())
            fixture.configure_inputs(session,root,path)
            self.assertEqual(session.UI,root/'prefix');self.assertEqual(session.SOURCE,path)
            font.write_bytes(b'different')
            with self.assertRaisesRegex(RuntimeError,'reviewed input'):fixture.configure_inputs(session,root,path)
            (root/'CMakeCache.txt').write_text(row+row)
            with self.assertRaisesRegex(RuntimeError,'one configured font'):fixture.configure_inputs(session,root,path)

    def test_digest_has_canonical_order(self):
        a = {'lib/a': {'size': 3, 'sha256': 'a'}, 'lib/b': {'symlink': 'a'}}
        b = {'lib/b': {'symlink': 'a'}, 'lib/a': {'sha256': 'a', 'size': 3}}
        self.assertEqual(apps.inventory_digest(a), apps.inventory_digest(b))

    def test_changed_component_body_cannot_adopt_its_own_record(self):
        with tempfile.TemporaryDirectory() as directory:
            build = Path(directory).resolve()
            (build/'prefix').mkdir()
            (build/'prefix/library').write_bytes(b'original')
            (build/'build.json').write_bytes(b'{}')
            pin = dict(record='build.json', record_sha256=hashlib.sha256(b'{}').hexdigest(),
                       tree_sha256=apps.inventory_digest({'library': {'size': 8, 'sha256': hashlib.sha256(b'original').hexdigest()}}),
                       regular_files=1, aliases=0, max_file_bytes=8, total_bytes=8)
            apps.check_component(build, pin)
            (build/'prefix/library').write_bytes(b'changed!')
            with self.assertRaisesRegex(ValueError, 'selected component tree'):
                apps.check_component(build, pin)

    def test_unexplained_component_file_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            build = Path(directory).resolve()
            (build/'prefix').mkdir()
            (build/'build.json').write_bytes(b'{}')
            pin = dict(record='build.json', record_sha256=hashlib.sha256(b'{}').hexdigest(),
                       tree_sha256=apps.inventory_digest({}), regular_files=0, aliases=0, max_file_bytes=8, total_bytes=8)
            (build/'prefix/unlisted').write_bytes(b'x')
            with self.assertRaisesRegex(ValueError, 'selected component tree'):
                apps.check_component(build, pin)

    def test_symlink_escape_rejected_without_following_it(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve()
            (root/'real').write_bytes(b'valid')
            try:(root/'link').symlink_to('../foreign')
            except OSError as error:self.skipTest(str(error))
            with self.assertRaisesRegex(ValueError, 'component alias'):
                apps.tree_inventory(root)

    def test_all_independently_reviewed_variants_and_no_admin_test_admission(self):
        value = apps.component_profile()
        self.assertEqual(len(value['components']), 6)
        self.assertTrue(re.search(apps.MODEL_TESTS, 'editor.history'))
        self.assertTrue(re.search(apps.MODEL_TESTS, 'terminal.pty'))
        self.assertFalse(re.search(apps.MODEL_TESTS, 'catalogue.units'))
        self.assertFalse(re.search(apps.MODEL_TESTS, 'authority.unit_custody'))


if __name__ == '__main__':
    unittest.main()
