"""Developer disk guards; host tests never execute QEMU or format a device."""
import json
from pathlib import Path
import subprocess
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch
import uuid

import development_storage as storage
import run_vm


class Storage(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.vm = SimpleNamespace(VM=self.root, FLAGS=0, IMAGE='qemu-img',
            owned=lambda p: p, config=lambda: {'uuid': '0a174bc7-f17b-4799-9331-145ba3a5800c'},
            private_path=lambda *a, **kw: None, write_state=self.write,
            file_guard=lambda path: self.guard())

    def guard(self):
        from contextlib import nullcontext
        return nullcontext()

    def write(self, path, record):
        path.write_text(json.dumps(record))

    def prepared(self):
        with patch.object(storage.subprocess, 'run') as effect, patch.object(storage, 'inspect_image'):
            record = storage.prepare(self.vm)
        self.assertEqual(effect.call_count, 1)
        return record

    def test_original_creation_and_exact_retry_do_not_repeat_image_effect(self):
        record = self.prepared()
        with patch.object(storage.subprocess, 'run', side_effect=AssertionError('repeat effect')):
            self.assertEqual(storage.prepare(self.vm), record)
        self.assertEqual(storage.load(self.vm), record)

    def test_lost_creation_reply_never_replays_or_attaches(self):
        with patch.object(storage.subprocess, 'run', side_effect=subprocess.TimeoutExpired('create', 1)):
            with self.assertRaises(subprocess.TimeoutExpired): storage.prepare(self.vm)
        self.assertEqual(storage.load(self.vm)['stage'], 'creating')
        with patch.object(storage.subprocess, 'run', side_effect=AssertionError('repeat effect')):
            with self.assertRaisesRegex(ValueError, 'uncertain'): storage.prepare(self.vm)
            with self.assertRaisesRegex(ValueError, 'uncertain'): storage.boot_arguments(self.vm, False)

    def test_replacement_image_is_not_adopted_from_equal_bytes(self):
        self.prepared()
        image = storage.paths(self.vm)[2]
        image.rename(image.with_name('original.qcow2'))
        image.write_bytes(b'')
        with self.assertRaisesRegex(ValueError, 'replaced'): storage.load(self.vm)

    def test_no_registration_and_foreign_files_refuse_creation(self):
        directory = storage.paths(self.vm)[0]; directory.mkdir()
        (directory / 'foreign').write_text('preserve')
        with patch.object(storage.subprocess, 'run', side_effect=AssertionError('effect')):
            with self.assertRaisesRegex(ValueError, 'unregistered'): storage.prepare(self.vm)
        self.assertEqual((directory/'foreign').read_text(), 'preserve')

    def test_insufficient_host_capacity_cannot_create_or_register_image(self):
        with patch.object(storage.shutil,'disk_usage',return_value=SimpleNamespace(free=storage.SIZE)):
            with self.assertRaisesRegex(ValueError,'capacity'): storage.prepare(self.vm)
        self.assertFalse(storage.paths(self.vm)[1].exists())
        self.assertFalse(storage.paths(self.vm)[2].exists())

    def test_wrong_vm_serial_size_and_fields_are_rejected(self):
        record = self.prepared()
        for key, value in (('vm_uuid', str(uuid.uuid4())), ('serial', 'AETHERI_P0_ONLY'),
                           ('bytes', 48*1024**3), ('bytes', True), ('extra', True)):
            with self.subTest(key=key, value=value):
                with self.assertRaises(ValueError): storage.validate(record | {key:value}, self.vm.config()['uuid'])

    def test_installer_never_receives_registered_developer_volume(self):
        self.prepared()
        with self.assertRaisesRegex(ValueError, 'reinstallation'): storage.boot_arguments(self.vm, True)

    def test_boot_explicit_format_and_json_filename_and_device_identity(self):
        record = self.prepared()
        with patch.object(storage, 'inspect_image') as observed:
            args = storage.boot_arguments(self.vm, False)
        observed.assert_called_once()
        block = json.loads(args[1])
        self.assertEqual(block['driver'], 'qcow2')
        self.assertEqual(block['file'], {'driver':'file', 'filename':str(storage.paths(self.vm)[2])})
        self.assertIn('serial='+record['serial'], args[3])
        self.assertNotIn('AETHERI_P0_ONLY', args[3])

    def test_external_backing_snapshots_encryption_and_size_are_rejected(self):
        record = self.prepared()
        original = {'format':'qcow2','virtual-size':storage.SIZE}
        for bad in ({'backing-filename':'other'}, {'snapshots':[{}]}, {'encrypted':True},
                    {'virtual-size':1}, {'format':'raw'}, {'format-specific':{'data':{'data-file':'external'}}}):
            with patch.object(storage.subprocess, 'check_output', return_value=json.dumps(original | bad).encode()):
                with self.assertRaises(ValueError): storage.inspect_image(self.vm, record)

    def test_pending_image_has_no_guest_format_profile(self):
        record = self.prepared()
        with self.assertRaises(ValueError): storage.guest_profile(record | {'stage':'creating'})
        self.assertEqual(set(storage.guest_profile(record)), {'format','vm_uuid','token','serial','bytes','fs_uuid'})

    def test_missing_or_foreign_attachment_never_dispatches_guest_code(self):
        self.prepared()
        self.vm.running=lambda: True
        image=storage.paths(self.vm)[2]
        for blocks in ([], [{'inserted':{'node-name':'p0disk','drv':'qcow2','file':str(image),'ro':False}}],
                       [{'inserted':{'node-name':'aetheri_development_work','drv':'qcow2','file':'foreign','ro':False}}]):
            self.vm.qmp=lambda *a: blocks
            with patch.object(storage.subprocess,'run',side_effect=AssertionError('guest effect')):
                with self.assertRaisesRegex(ValueError,'not attached'): storage.guest_action(self.vm,'initialize-guest')

    def test_verified_guest_dispatch_is_once_and_uses_stdin_not_shell_data(self):
        record=self.prepared(); image=storage.paths(self.vm)[2]
        self.vm.running=lambda: True
        self.vm.qmp=lambda command,*a: ([{'inserted':{'node-name':'aetheri_development_work','drv':'qcow2',
                                          'file':str(image),'ro':False}}] if command=='query-block' else record['serial'])
        for action in ('initialize-guest','mount-guest'):
            with patch.object(storage.subprocess,'run',side_effect=subprocess.TimeoutExpired('ssh',1)) as effect:
                with self.assertRaises(subprocess.TimeoutExpired): storage.guest_action(self.vm,action)
                self.assertEqual(effect.call_count,1)
                self.assertEqual(effect.call_args.args[0][-2:],['ssh','sudo -n python3 -'])
                compile(effect.call_args.kwargs['input'],'guest-envelope','exec')

    def test_no_additional_storage_leaves_ordinary_boot_unchanged(self):
        self.assertEqual(storage.boot_arguments(self.vm, False), [])
        self.assertEqual(storage.boot_arguments(self.vm, True), [])

    def test_existing_runner_uses_additional_attachment_only_after_review(self):
        with patch.object(run_vm, 'VM', self.root), patch.object(run_vm, 'config', self.vm.config), \
                patch.object(storage, 'boot_arguments', return_value=['-device','fixed-reviewed-disk']) as attachment:
            result = run_vm.command(False, offline=True)
        attachment.assert_called_once_with(run_vm, False)
        self.assertEqual(result[-2:], ['-device','fixed-reviewed-disk'])


if __name__ == '__main__':
    unittest.main()
