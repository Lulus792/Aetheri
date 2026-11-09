"""Controls for original SDK metadata and the path-only relocation boundary."""
import copy
from pathlib import Path
import unittest
from unittest.mock import patch

import prepare_desktop_sdk as sdk


class SDK(unittest.TestCase):
    def test_only_reviewed_path_assignments_change(self):
        raw = b'prefix=/usr\nlibdir=/usr/lib64\nincludedir=/usr/include\nVersion: 1.26.0\nRequires.private: libffi\nxkb_root=/usr/share/xkeyboard-config-2\nDescription: /usr is original\n'
        result = sdk.rebase(raw, Path('/private/sdk'))
        self.assertEqual(result, b'prefix=/private/sdk/usr\nlibdir=/private/sdk/usr/lib64\nincludedir=/private/sdk/usr/include\nVersion: 1.26.0\nRequires.private: libffi\nxkb_root=/usr/share/xkeyboard-config-2\nDescription: /usr is original\n')

    def test_prefix_references_and_no_final_newline_preserved(self):
        raw = b'prefix=/usr\nlibdir=${prefix}/lib64\nVersion: 3.5.2'
        self.assertEqual(sdk.rebase(raw, Path('/sdk')), b'prefix=/sdk/usr\nlibdir=${prefix}/lib64\nVersion: 3.5.2')

    def test_changed_original_metadata_not_relabelled(self):
        v, _ = sdk.profile()
        n = next(iter(v['pkgconfig']))
        v['pkgconfig'][n] += '# changed\n'
        with self.assertRaisesRegex(ValueError, 'original pkg-config body'):
            sdk.validate(v)

    def test_alias_cannot_escape_or_remain_unresolved(self):
        original, _ = sdk.profile()
        n = next(iter(original['symlinks']))
        for target in ('../foreign', 'missing.so'):
            v = copy.deepcopy(original)
            v['symlinks'][n] = target
            with self.subTest(target=target), self.assertRaises(ValueError):
                sdk.validate(v)

    def test_cycle_rejected(self):
        v, _ = sdk.profile()
        n = next(iter(v['symlinks']))
        v['symlinks'][n] = Path(n).name
        with self.assertRaisesRegex(ValueError, 'unresolved'):
            sdk.validate(v)

    def test_profile_and_native_generator_present(self):
        v, _ = sdk.profile()
        self.assertEqual(len(v['packages']), 10)
        self.assertIn('usr/bin/wayland-scanner', {e['path'] for e in v['files']})
        self.assertEqual(v['versions']['wayland-client'], '1.26.0')

    def test_unknown_line_endings_rejected(self):
        for raw in (b'prefix=/usr\r\n', b'prefix=/usr\0\n'):
            with self.assertRaises(ValueError):
                sdk.rebase(raw, Path('/sdk'))

    def test_root_prepare_refused_before_effects(self):
        with patch.object(sdk.sys, 'platform', 'linux'), patch.object(sdk.os, 'geteuid', return_value=0, create=True):
            with self.assertRaisesRegex(ValueError, 'ordinary native build account'):
                sdk.prepare('missing', 'missing')

    def test_sdl_profile_keeps_real_api_versions_and_meson_notice(self):
        v, _ = sdk.profile('sdl')
        self.assertEqual(len(v['packages']), 5)
        self.assertEqual(v['versions'], {'egl': '1.5', 'glesv2': '3.2'})
        names = {e['path'] for e in v['files']}
        self.assertIn('usr/include/EGL/egl.h', names)
        self.assertIn('usr/bin/meson', names)
        self.assertIn('usr/lib/python3.14/site-packages/meson-1.11.2.dist-info/licenses/COPYING', names)
        self.assertEqual(len(v['source_notices']), 1)
        self.assertEqual(v['source_notices'][0]['source_package'], 1)

    def test_source_notice_must_stay_source_data(self):
        v, _ = sdk.profile('sdl')
        v['source_notices'][0]['source_package'] = 0
        with self.assertRaisesRegex(ValueError, 'source notice data'):
            sdk.validate(v)

    def test_unselected_profile_refused(self):
        with self.assertRaisesRegex(ValueError, 'unsupported'):
            sdk.profile('unreviewed')


if __name__ == '__main__':
    unittest.main()
