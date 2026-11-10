"""Current-runtime supervisor selection/commands; no service or device dispatch.

Tree protection is mocked only for portable projection/early-rejection tests.
Real body/type/alias controls live in test_current_desktop_runtime; actual native
protected-tree/session evidence must be recorded separately.
"""
import copy
from contextlib import ExitStack
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import development_runtime as runtime
import development_session as session


class Selection(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name).resolve()
        self.value = runtime.current.profile()
        self.binding = dict(kind=runtime.KIND, root=str(self.root),
                            profile_sha256=runtime.current.fingerprint(self.value))

    def selected(self, **changes):
        args = dict(binding=self.binding, profile='release', desktop=True,
                    settings_ui='nexaui-owned', files_ui='nexaui-owned', terminal_ui='nexaui-owned')
        args.update(changes)
        return runtime.Runtime(**args)

    def test_native_test_owner_is_explicit_and_cannot_select_foreign_code_or_documents(self):
        from test_current_desktop_session import native_owner
        value=dict(program_root='/run/a-ci-012345abcdef',program_files={'aetheri-desktop-owner':dict(sha256='a'*64)})
        self.assertIsNone(native_owner(value))
        selected=dict(kind='aetheri-native-desktop-owner-v1',binary=value['program_root']+'/aetheri-desktop-owner',
                      sha256='a'*64,documents='/run/a-native-doc-012345abcdef')
        self.assertEqual(native_owner({**value,'owner_profile':selected}),selected)
        for change in (dict(kind='other'),dict(binary='/foreign/owner'),dict(sha256='b'*64),
                       dict(documents='/home/aetheri/Documents'),dict(documents='/run/a-native-doc-012345abcdef/../other'),dict(extra=True)):
            with self.subTest(change=change),self.assertRaises(ValueError):native_owner({**value,'owner_profile':{**selected,**change}})
        value['program_files']['aetheri-session']=dict(sha256='b'*64)
        entry={**selected,'kind':'aetheri-native-session-entry-v1','binary':value['program_root']+'/aetheri-session','sha256':'b'*64}
        self.assertEqual(native_owner({**value,'owner_profile':entry}),entry)
        for change in (dict(kind=[]),dict(binary=selected['binary']),dict(sha256='a'*64)):
            with self.subTest(entry_change=change),self.assertRaises(ValueError):native_owner({**value,'owner_profile':{**entry,**change}})

    def test_foreign_or_ambiguous_binding_rejects_before_tree_access(self):
        cases = [None, {}, {**self.binding, 'kind':'other'}, {**self.binding, 'extra':True},
                 {**self.binding, 'profile_sha256':'0'*64}, {**self.binding, 'root':str(self.root/'..')},
                 {**self.binding, 'root':123}, {**self.binding, 'root':'relative'}]
        with patch.object(runtime.current, 'check_protected') as check:
            for binding in cases:
                with self.subTest(binding=binding), self.assertRaises(ValueError):
                    self.selected(binding=binding)
            check.assert_not_called()

    def test_partial_debug_or_legacy_view_does_not_select_current_runtime(self):
        with patch.object(runtime.current, 'check_protected') as check:
            for change in (dict(profile='debug'), dict(desktop=False), dict(desktop=1),
                           dict(settings_ui='nexaui'), dict(files_ui='sdl'), dict(terminal_ui='sdl')):
                with self.subTest(change=change), self.assertRaises(ValueError): self.selected(**change)
            check.assert_not_called()

    def test_protected_check_failure_propagates_without_role_selection(self):
        with patch.object(runtime.current, 'check_protected', side_effect=ValueError('changed body')):
            with self.assertRaisesRegex(ValueError, 'changed body'): self.selected()

    def test_projections_use_new_kind_and_cannot_reinterpret_legacy_sdk_receipts(self):
        with patch.object(runtime.current, 'check_protected'):
            selected = self.selected()
        for role, validator in (('settings', session.validate_settings_profile),
                                ('files', session.validate_files_profile), ('terminal', session.validate_terminal_profile)):
            value = selected.selection(role)
            self.assertEqual(value['kind'], runtime.KIND)
            self.assertEqual(value['runtime'], self.binding)
            with self.assertRaises(RuntimeError): validator(value, 'release')
            value['runtime']['root'] = '/foreign'
            self.assertEqual(selected.selection(role)['runtime'], self.binding)
        with self.assertRaises(ValueError): selected.selection('browser')

    def test_all_app_and_nested_editor_paths_stay_in_one_runtime(self):
        with patch.object(runtime.current, 'check_protected'): selected = self.selected()
        profiles = [selected.selection(r) for r in ('settings', 'files', 'terminal')]
        cache = Path('/private/cache')
        for role in ('settings', 'files', 'terminal'):
            args = session.application_command(role, selected.build, cache, *profiles,
                ui=selected.ui, terminal_font=selected.font, terminal_bold_font=selected.bold_font)
            self.assertTrue(Path(args[0]).is_relative_to(self.root))
            for flag in ('--font', '--bold-font', '--terminal-font', '--terminal-bold-font', '--shell', '--pty-starter', '--terminal', '--editor'):
                if flag in args:
                    self.assertTrue(Path(args[args.index(flag)+1]).is_relative_to(self.root), (role, flag, args))
            if role == 'terminal': self.assertIn('--pty-starter', args)
            if role == 'files':
                self.assertEqual(Path(args[args.index('--root')+1]), cache.parent/'documents')
                self.assertIn('--terminal-bold-font', args)

    def test_library_steering_is_replaced_without_losing_original_session_channels(self):
        with patch.object(runtime.current, 'check_protected'): selected = self.selected()
        original = dict(LD_PRELOAD='/foreign.so', LD_AUDIT='/audit.so', LD_LIBRARY_PATH='/foreign',
            SDL_DYNAMIC_API='/foreign.so', SDL_VIDEO_DRIVER='foreign', MESA_LOADER_DRIVER_OVERRIDE='foreign', GALLIUM_DRIVER='foreign',
            __EGL_VENDOR_LIBRARY_FILENAMES='/foreign.json', LIBINPUT_QUIRKS_DIR='/foreign',
            WAYLAND_SOCKET='13', NOTIFY_SOCKET='/original/notify', XDG_RUNTIME_DIR='/original/runtime',
            XDG_CONFIG_HOME='/checked/wacom/usr/share', PATH='/usr/bin:/bin')
        before = copy.deepcopy(original); env = selected.environment(original)
        self.assertEqual(original, before)
        for key in ('LD_PRELOAD', 'LD_AUDIT', 'SDL_DYNAMIC_API', 'SDL_VIDEO_DRIVER', 'MESA_LOADER_DRIVER_OVERRIDE'):
            self.assertNotIn(key, env)
        for key in ('WAYLAND_SOCKET', 'NOTIFY_SOCKET', 'XDG_RUNTIME_DIR', 'XDG_CONFIG_HOME', 'PATH'):
            self.assertEqual(env[key], before[key])
        self.assertEqual(env['LD_LIBRARY_PATH'], str(self.root/'usr/lib64'))
        self.assertEqual(env['LIBINPUT_QUIRKS_DIR'], str(self.root/'usr/share/libinput'))

    def test_missing_role_cannot_be_filled_by_an_unselected_carrier_program(self):
        value = copy.deepcopy(self.value)
        value['files'] = [e for e in value['files'] if not e['destination'].endswith('/aetheri-editor')]
        binding = {**self.binding, 'profile_sha256':runtime.current.fingerprint(value)}
        with patch.object(runtime.current, 'profile', return_value=value), \
                patch.object(runtime.current, 'check_protected') as check:
            with self.assertRaisesRegex(ValueError, 'missing current desktop role'): self.selected(binding=binding)
            check.assert_not_called()

    def test_original_per_role_binding_still_gates_supervisor_before_any_spawn(self):
        with patch.object(runtime.current, 'check_protected'): selected = self.selected()
        kwargs = dict(settings_ui='nexaui-owned', files_ui='nexaui-owned', terminal_ui='nexaui-owned',
                      current_runtime=self.binding, desktop=True)
        for role in ('settings', 'files', 'terminal'):
            kwargs[role+'_binding'] = session.profile_fingerprint(selected.selection(role))
        with ExitStack() as stack:
            stack.enter_context(patch.object(os, 'getuid', return_value=1000, create=True))
            stack.enter_context(patch.object(os, 'geteuid', return_value=1000, create=True))
            stack.enter_context(patch.object(runtime.current, 'check_protected'))
            spawn = stack.enter_context(patch.object(session.subprocess, 'Popen', side_effect=AssertionError('unexpected spawn')))
            owned = stack.enter_context(patch.object(session, 'owned', side_effect=AssertionError('unexpected user-tree access')))
            for role in ('settings', 'files', 'terminal'):
                changed = {**kwargs, role+'_binding':'0'*64}
                with self.subTest(role=role), self.assertRaisesRegex(RuntimeError, 'selection changed before supervision'):
                    session.supervise('release', 'a'*32, **changed)
            spawn.assert_not_called(); owned.assert_not_called()


if __name__ == '__main__': unittest.main()
