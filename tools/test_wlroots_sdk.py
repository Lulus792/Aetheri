"""Controls for genuine input build/header/metadata and package SDK selection."""
import copy
import unittest
from unittest.mock import patch
from pathlib import Path
from types import SimpleNamespace

from build_graphics import private_environment
import prepare_desktop_sdk as sdk
import prepare_wlroots_sdk as wlr
from test_wlroots_sdk_native import mapped_files


class SDK(unittest.TestCase):
    def test_original_package_versions_and_complete_input_bridge(self):
        v, _ = wlr.profile()
        self.assertEqual(len(v['packages']), 12)
        self.assertEqual(v['input_build']['version'], '1.32.0')
        self.assertEqual(v['versions']['libudev'], '259')
        self.assertIn('usr/share/hwdata/pnp.ids', {e['path'] for e in v['files']})

    def test_built_source_must_keep_exact_stage_paths(self):
        original, digest = sdk.profile('wlroots')
        for name in ('../foreign', 'counter-source/src/libinput.h', '/usr/include/libinput.h'):
            v = copy.deepcopy(original)
            v['input_build']['files'][0]['source_path'] = name
            with patch.object(wlr.sdk, 'profile', return_value=(v, digest)), self.assertRaises(ValueError):
                wlr.profile()

    def test_generated_version_is_not_relabelled(self):
        v, digest = sdk.profile('wlroots')
        v['input_build']['pkgconfig'] = v['input_build']['pkgconfig'].replace('1.32.0', '1.31.3')
        with patch.object(wlr.sdk, 'profile', return_value=(v, digest)), self.assertRaisesRegex(ValueError, 'original input pkg-config'):
            wlr.profile()

    def test_cannot_substitute_another_build_or_source(self):
        original, digest = sdk.profile('wlroots')
        for key in ('version', 'source'):
            v = copy.deepcopy(original)
            v['input_build'][key] = 'foreign'
            with patch.object(wlr.sdk, 'profile', return_value=(v, digest)), self.assertRaisesRegex(ValueError, 'SDK build'):
                wlr.profile()

    def test_original_aliases_and_notice_required(self):
        original, digest = sdk.profile('wlroots')
        v = copy.deepcopy(original)
        v['input_build']['symlinks']['usr/lib64/libinput.so'] = '../foreign'
        with patch.object(wlr.sdk, 'profile', return_value=(v, digest)), self.assertRaises(ValueError):
            wlr.profile()
        v = copy.deepcopy(original)
        v['upstream_notice']['sha256'] = '0'*64
        with patch.object(wlr.sdk, 'profile', return_value=(v, digest)), self.assertRaisesRegex(ValueError, 'reviewed libdrm'):
            wlr.profile()

    def test_only_paths_change_in_built_pkgconfig(self):
        v, _ = wlr.profile()
        entries = wlr.input_entries(v, Path('/sdk'))
        body = sdk.rebase(v['input_build']['pkgconfig'].encode(), Path('/sdk/input'))
        self.assertIn(b'Version: 1.32.0\n', body)
        self.assertIn(b'Requires.private: libudev\n', body)
        self.assertIn(b'plugindir=${libdir}/libinput/plugins\n', body)
        self.assertEqual(entries['usr/lib64/pkgconfig/libinput.pc']['sha256'], wlr.sha(body))

    def test_root_prepare_refuses_before_effects(self):
        with patch.object(wlr.sys, 'platform', 'linux'), patch.object(wlr.os, 'getuid', return_value=0, create=True):
            with self.assertRaisesRegex(ValueError, 'ordinary native'):
                wlr.prepare('missing', 'missing', 'missing', 'missing')

    def test_old_build_path_and_incomplete_private_selection(self):
        args = SimpleNamespace(common_sdk=None, session_sdk=None, sdl_sdk=None,
                               toolchain=None, original_runtime=None, session=True)
        self.assertEqual(private_environment(args), (None, None))
        args.session_sdk = Path('/private')
        with self.assertRaisesRegex(ValueError, 'all five'):
            private_environment(args)

    def test_original_release_command_is_required_even_for_legacy_record(self):
        built = {'build_profile_sha256': 'original'}
        library = {'size': 637664, 'sha256': 'selected'}
        record = {'profile_sha256': 'original', 'artifact': library,
                  'commands': [['meson', 'setup', '/old/candidate/build', '/old/candidate/source', '--buildtype=release']]}
        wlr.admit_record(record, built, library)
        for name in ('different-profile', 'debugoptimized', 'counter-artifact'):
            other = copy.deepcopy(record)
            if name == 'different-profile':other['profile_sha256'] = 'changed'
            elif name == 'debugoptimized':other['commands'][0][-1] = '--buildtype=debugoptimized'
            else:other['artifact']['sha256'] = 'counter'
            with self.subTest(name=name), self.assertRaisesRegex(ValueError, 'Release record'):
                wlr.admit_record(other, built, library)

    def test_only_known_nonexecuting_pixel_buffers_are_separate_from_files(self):
        line = 'MAP 1000-2000 rw-s 00000000 00:01 123 /dev/shm/wlroots-Abc123 (deleted)'
        files, buffers = mapped_files(line+'\nMAP 3000-4000 r-xp 00000000 00:01 456 /sdk/lib.so')
        self.assertEqual(files, {'/sdk/lib.so'})
        self.assertEqual(buffers, ['/dev/shm/wlroots-Abc123 (deleted)'])
        for wrong in (line.replace('rw-s', 'r-xs'), line.replace('wlroots-Abc123', 'foreign'),
                      line.replace('/dev/shm/wlroots-Abc123', '/tmp/libforeign.so')):
            with self.subTest(line=wrong), self.assertRaisesRegex(ValueError, 'mapping'):
                mapped_files(wrong)


if __name__ == '__main__':
    unittest.main()
