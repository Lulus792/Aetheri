"""Unprivileged Linux controls for the live Sound test driver; no services."""
from pathlib import Path
import sys
import tempfile
import unittest

if sys.platform=='linux':
    from test_settings_audio_live import Sound,check_binaries,dump_diagnostics


@unittest.skipUnless(sys.platform=='linux','Linux fixture imports; no host provisioning')
class Controls(unittest.TestCase):
    def setUp(self):
        self.view=object.__new__(Sound)
        self.view.output=Path('/protected/evidence') # The inherited fixture field.
        self.view.gui_log=lambda:'''CONTROL 12 36.00 3.00 26.00 26.00 1
CONTROL 53 12.00 194.00 186.00 40.00 528
CONTROL 8589934592 234.00 170.65 742.00 52.00 2
CONTROL 8589934593 250.00 294.74 710.00 40.00 512
AUDIO_OUTPUT id=8589934592 default=1 available=1 volume=1.000000000 muted=0
'''

    def test_output_observation_does_not_shadow_the_evidence_path(self):
        self.assertEqual(self.view.observed_output(),(8589934592,1,1,1.0,0))
        self.assertEqual(self.view.output,Path('/protected/evidence'))

    def test_checked_selected_and_focused_controls_remain_enabled(self):
        for identifier in (53,8589934592,8589934593):
            self.assertTrue(self.view.enabled(identifier))
            self.assertEqual(self.view.rect(identifier)[1],0)
        self.assertFalse(self.view.enabled(12));self.assertEqual(self.view.rect(12)[1],1)
        self.assertFalse(self.view.enabled(999))

    def test_latest_observation_and_disabled_flag_win(self):
        old=self.view.gui_log()
        self.view.gui_log=lambda:old+'CONTROL 8589934593 250 294 710 40 513\nAUDIO_OUTPUT id=8589934592 default=1 available=1 volume=0.49 muted=1\n'
        self.assertFalse(self.view.enabled(8589934593));self.assertEqual(self.view.observed_output()[3:],(.49,1))

    def test_missing_helpers_fail_without_a_fixture(self):
        with tempfile.TemporaryDirectory() as temporary:
            with self.assertRaisesRegex(ValueError,'missing native test program: probe_audio_mac'):
                check_binaries(Path(temporary).resolve())

    def test_only_exact_optional_parameter_diagnostic_is_accepted(self):
        line='[E][13:52:37.904052] default      | [       pw-dump.c: 1509 on_core_error()] error id:22 seq:103 res:-2 (No such file or directory): enum params id:16 (Spa:Enum:ParamId:ProcessLatency) failed\n'
        self.assertTrue(dump_diagnostics(''));self.assertTrue(dump_diagnostics(line))
        for rejected in (line.replace('ProcessLatency','Props'),line.replace('id:16','id:2'),line.replace('res:-2','res:-13'),line+'unrelated error\n'):
            self.assertFalse(dump_diagnostics(rejected))

    def test_shared_input_environment_is_explicit(self):
        env=dict(LD_LIBRARY_PATH='/original/gui/usr/lib64',UNCHANGED='yes')
        self.view.input_runtime=None
        self.assertIs(self.view.with_input(env),env)
        self.view.input_runtime=Path('/protected/input')
        selected=self.view.with_input(env)
        self.assertEqual(selected['LD_LIBRARY_PATH'],'/protected/input/usr/lib64:/original/gui/usr/lib64')
        self.assertEqual(selected['LIBINPUT_QUIRKS_DIR'],'/protected/input/usr/share/libinput')
        self.assertEqual(selected['UNCHANGED'],'yes');self.assertNotIn('LIBINPUT_QUIRKS_DIR',env)

    def test_shared_input_refuses_original_policy_before_bus_or_effects(self):
        with self.assertRaisesRegex(ValueError,'candidate policy'):
            self.view.setup(None,None,None,None,None,None,input_runtime=Path('/unadmitted'))

    def test_source_loss_keeps_history_but_removes_current_controls(self):
        self.view.row=8589934592
        old='FRAME 1000 720 1.00\n'+self.view.gui_log()
        lost=old+'FRAME 1000 720 1.00\nAUDIO state=1 revision=6 count=1 feedback=5 stale=0 fenced=1\n'
        self.view.gui_log=lambda:lost
        self.assertFalse(self.view.enabled(self.view.row+1));self.assertTrue(self.view.fenced(lost))
        self.assertFalse(self.view.fenced(lost+'CONTROL 8589934594 1 1 1 1 0\n'))
        self.assertFalse(self.view.fenced(old))


if __name__=='__main__':unittest.main()
