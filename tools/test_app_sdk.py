"""Core app SDK selection, real metadata and unchanged smaller-family bounds."""
import unittest

import prepare_desktop_sdk as sdk


class Apps(unittest.TestCase):
    def test_real_versions_and_crypto_only(self):
        v, _ = sdk.profile('apps')
        self.assertEqual(len(v['packages']), 8)
        self.assertEqual(v['versions']['libcrypto'], '3.5.9')
        files = {e['path']: e for e in v['files']}
        self.assertEqual(files['usr/lib64/libcrypto.so.3.5.9']['size'], 5783496)
        self.assertNotIn('usr/lib64/libssl.so.3.5.9', files)
        self.assertNotIn('usr/lib64/pkgconfig/libssl.pc', files)

    def test_original_private_requirements_are_metadata_not_new_runtime(self):
        v, _ = sdk.profile('apps')
        self.assertIn('Requires.private: libsepol libpcre2-8\n', v['pkgconfig']['usr/lib64/pkgconfig/libselinux.pc'])
        self.assertEqual(len(v['metadata_only_packages']), 2)
        for package in v['metadata_only_packages']:
            selected = [e for e in v['files'] if e['package'] == package]
            self.assertEqual(len(selected), 1)
            self.assertTrue(selected[0]['path'].endswith('.pc'))

    def test_crypto_specific_bound_does_not_relax_old_profiles(self):
        for kind in ('common', 'sdl', 'wlroots'):
            v, _ = sdk.profile(kind)
            entry = next(e for e in v['files'] if not e['path'].endswith('.pc'))
            entry['size'] = 3*1024**2
            with self.subTest(kind=kind), self.assertRaisesRegex(ValueError, 'SDK file'):
                sdk.validate(v)
        v, _ = sdk.profile('apps')
        entry = next(e for e in v['files'] if e['path'] == 'usr/lib64/libcrypto.so.3.5.9')
        entry['size'] = 9*1024**2
        with self.assertRaisesRegex(ValueError, 'SDK file'):
            sdk.validate(v)

    def test_metadata_version_cannot_be_changed(self):
        v, _ = sdk.profile('apps')
        v['pkgconfig']['usr/lib64/pkgconfig/libcrypto.pc'] = v['pkgconfig']['usr/lib64/pkgconfig/libcrypto.pc'].replace('3.5.9', '3.5.5')
        with self.assertRaisesRegex(ValueError, 'original pkg-config'):
            sdk.validate(v)


if __name__ == '__main__':
    unittest.main()
