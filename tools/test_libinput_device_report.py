"""Host-only result controls; never creates devices or starts a service."""
import unittest
from test_libinput_devices import parse_summary,library_selection


TEXT='\nsummary:\n  completed: 293\n  pass: 262\n  na: 30\n  fail: 0\n  skip: 1\n  status: PASS\n'


class Report(unittest.TestCase):
    def test_explicit_library_profiles(self):
        self.assertEqual(library_selection(dict(library_directory='release')),('release','release'))
        self.assertEqual(library_selection(dict(library_profile='instrumented',library_directory='candidate/build')),('instrumented','candidate/build'))

    def test_counter_and_unbound_profiles_rejected(self):
        for profile,directory in [('release','candidate/counter-build'),('instrumented','candidate/counter-build'),
                                  ('instrumented','release'),('release','candidate/build'),('unknown','candidate/build')]:
            with self.subTest(profile=profile,directory=directory),self.assertRaises(ValueError):
                library_selection(dict(library_profile=profile,library_directory=directory))

    def test_hardware_na_and_skip_remain_visible(self):
        r=parse_summary(TEXT,0)
        self.assertTrue(r['upstream_passed']);self.assertFalse(r['coverage_complete'])
        self.assertEqual(r['counts'],dict(completed=293,**{'pass':262},na=30,fail=0,skip=1))

    def test_all_cases_execute(self):
        r=parse_summary(TEXT.replace('293','50').replace('262','50').replace('na: 30','na: 0').replace('skip: 1','skip: 0'),0)
        self.assertTrue(r['upstream_passed']);self.assertTrue(r['coverage_complete'])

    def test_native_failure_cannot_be_hidden(self):
        r=parse_summary(TEXT,6)
        self.assertFalse(r['upstream_passed']);self.assertFalse(r['coverage_complete'])
    def test_missing_summary_cannot_pass(self):self.assertFalse(parse_summary('upstream exited 0',0)['upstream_passed'])

    def test_inconsistent_or_incomplete_counts(self):
        for text in (TEXT.replace('293','294'),TEXT.replace('  na: 30\n',''),TEXT+'  pass: 262\n'):
            with self.subTest(text=text):self.assertFalse(parse_summary(text,0)['upstream_passed'])

    def test_failures_and_empty_run_cannot_pass(self):
        for text in (TEXT.replace('pass: 262','pass: 261').replace('fail: 0','fail: 1'),
                     TEXT.replace('293','0').replace('262','0').replace('na: 30','na: 0').replace('skip: 1','skip: 0')):
            with self.subTest(text=text):self.assertFalse(parse_summary(text,0)['upstream_passed'])


if __name__=='__main__':unittest.main()
