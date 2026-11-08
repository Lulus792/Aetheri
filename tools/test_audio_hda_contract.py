"""Independent WAV counterexamples; no VM, device or package effects."""
import io
import math
import struct
import unittest
import wave

import test_audio_hda as audio


def capture():
    pairs=[(0,0)]*1024
    pairs += [tuple(round(4096*math.sin(2*math.pi*f*i/48000)) for f in (440,880)) for i in range(96000)]
    return pairs+[(0,0)]*1024


def wav(pairs,rate=48000):
    output=io.BytesIO()
    with wave.open(output,'wb') as target:
        target.setnchannels(2);target.setsampwidth(2);target.setframerate(rate)
        target.writeframes(b''.join(struct.pack('<hh',*p) for p in pairs))
    return output.getvalue()


class Capture(unittest.TestCase):
    @classmethod
    def setUpClass(cls):cls.pairs=capture()

    def test_complete(self):
        actual=audio.validate_wav(wav(self.pairs))
        self.assertEqual(actual['signal_start'],1024)
        self.assertEqual(actual['signal_frames'],96000)
        self.assertLess(actual['max_error_lsb'],.501)

    def test_silence(self):
        with self.assertRaises(ValueError):audio.validate_wav(wav([(0,0)]*98048))

    def test_swapped_channels(self):
        with self.assertRaises(ValueError):audio.validate_wav(wav([(r,l) for l,r in self.pairs]))

    def test_lost_initial_right_quantum(self):
        p=self.pairs.copy()
        for i in range(1024,2048):p[i]=(p[i][0],0)
        with self.assertRaises(ValueError):audio.validate_wav(wav(p))

    def test_missing_tail(self):
        with self.assertRaises(ValueError):audio.validate_wav(wav(self.pairs[:95000]))

    def test_underrun_in_middle(self):
        p=self.pairs.copy();p[10000:11024]=[(0,0)]*1024
        with self.assertRaises(ValueError):audio.validate_wav(wav(p))

    def test_extra_signal(self):
        with self.assertRaises(ValueError):audio.validate_wav(wav(self.pairs+[(300,100)]))

    def test_wrong_rate(self):
        with self.assertRaises(ValueError):audio.validate_wav(wav(self.pairs,44100))

    def test_unfinalized_riff(self):
        raw=bytearray(wav(self.pairs));raw[4:8]=b'\x00'*4
        with self.assertRaises(ValueError):audio.validate_wav(raw)

    def test_half_cubic_gain(self):
        pairs=[(0,0)]*1024+[tuple(round(512*math.sin(2*math.pi*f*i/48000)) for f in (440,880)) for i in range(96000)]+[(0,0)]*1024
        self.assertLess(audio.validate_wav(wav(pairs),gain=.125)['max_error_lsb'],.501)
        with self.assertRaises(ValueError):audio.validate_wav(wav(pairs))

    def test_unity_cannot_replace_half(self):
        with self.assertRaises(ValueError):audio.validate_wav(wav(self.pairs),gain=.125)

    def test_muted_complete_zero(self):
        self.assertTrue(audio.validate_wav(wav([(0,0)]*97024),muted=True)['silent'])

    def test_muted_requires_duration(self):
        with self.assertRaises(ValueError):audio.validate_wav(wav([(0,0)]*100),muted=True)

    def test_muted_rejects_even_one_nonzero_sample(self):
        p=[(0,0)]*97024;p[40000]=(1,0)
        with self.assertRaises(ValueError):audio.validate_wav(wav(p),muted=True)


class BootEvidence(unittest.TestCase):
    manifest={'nonce':'0'*32,'kernel_release':'test-kernel'}
    events=[{'event':'SHUTDOWN','data':{'guest':True,'reason':'guest-shutdown'}}]
    base=('AETHERI_BOOT_PASS nonce='+'0'*32+' kernel=test-kernel arch=x86_64 pid1=systemd uefi=1 ram_root=1 cgroup2=1 boot=aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee\n'
          'aetheri-boot.service: Deactivated successfully.\nreboot: Power down\n')
    positive=('AETHERI_AUDIO_DEVICE pci=8086:2668 card=0 playback=1\n'
              'AETHERI_AUDIO_MIXER volume=74 switch=1 channels=2 verified=1\n'
              'AETHERI_AUDIO_ACTOR role=server uid=1000 cap_zero=1 nnp=1\n'
              'AETHERI_AUDIO_ACTOR role=player uid=1000 cap_zero=1 nnp=1\nDRAINED frames=96000\n'
              'AETHERI_AUDIO_PASS nonce='+'0'*32+' frames=96000 rate=48000 channels=2 server_end=1 player_end=1\n'
              'aetheri-audio.service: Deactivated successfully.\n')

    def test_full_evidence(self):self.assertTrue(audio.evaluate(self.base+self.positive,self.manifest,0,self.events))

    def test_preflight_alone(self):self.assertFalse(audio.evaluate(self.base,self.manifest,0,self.events))

    def test_mixer_missing(self):
        serial=self.base+self.positive.replace('AETHERI_AUDIO_MIXER volume=74 switch=1 channels=2 verified=1\n','')
        self.assertFalse(audio.evaluate(serial,self.manifest,0,self.events))

    def test_privileged_player(self):
        serial=self.base+self.positive.replace('role=player uid=1000 cap_zero=1','role=player uid=0 cap_zero=0')
        self.assertFalse(audio.evaluate(serial,self.manifest,0,self.events))

    def test_host_shutdown(self):
        events=[{'event':'SHUTDOWN','data':{'guest':False,'reason':'host-qmp-quit'}}]
        self.assertFalse(audio.evaluate(self.base+self.positive,self.manifest,0,events))

    def test_expected_device_failure_is_not_playback(self):
        serial=self.base+"AETHERI_AUDIO_FAIL reason=device errno=2\naetheri-audio.service: Failed with result 'exit-code'.\n"
        self.assertTrue(audio.evaluate(serial,self.manifest,0,self.events,missing_device=True))
        self.assertFalse(audio.evaluate(serial,self.manifest,0,self.events))


class DeviceEvidence(unittest.TestCase):
    manifest={**BootEvidence.manifest,'proof':'audio-device'}
    extra=('AETHERI_AUDIO_UDEV initialized=1 database=1\n'
           'AETHERI_AUDIO_UDEV_ACCESS uid=1000 gid=1000 mode=0600 verified=1\n'
           'AETHERI_AUDIO_ACTOR role=udev uid=0 cap_zero=0 nnp=1\n'
           'POLICY_SINK\nnode.name = "alsa_output.pci-test.analog-stereo"\n'
           'POLICY_CARD\ndevice.api = "alsa"\ndevice.enum.api = "udev"\napi.alsa.use-acp = "true"\napi.alsa.card = "0"\nPOLICY_PROFILE\n * 1  Analog Stereo Output [available]  prio=6500\n'
           'AETHERI_AUDIO_POLICY alsa_monitor=1 acp=1 autolink=1 policy_end=1 udev_end=1\n')
    joined=BootEvidence.base+BootEvidence.positive.replace('AETHERI_AUDIO_ACTOR role=player','AETHERI_AUDIO_ACTOR role=policy uid=1000 cap_zero=1 nnp=1\nAETHERI_AUDIO_ACTOR role=player')+extra

    def test_joined(self):self.assertTrue(audio.evaluate(self.joined,self.manifest,0,BootEvidence.events))

    def test_manual_hda_is_not_joined_policy(self):
        self.assertFalse(audio.evaluate(BootEvidence.base+BootEvidence.positive,self.manifest,0,BootEvidence.events))

    def test_missing_udev_evidence(self):
        self.assertFalse(audio.evaluate(self.joined.replace('AETHERI_AUDIO_UDEV initialized=1 database=1\n',''),self.manifest,0,BootEvidence.events))

    def test_missing_policy_end(self):
        self.assertFalse(audio.evaluate(self.joined.replace('policy_end=1','policy_end=0'),self.manifest,0,BootEvidence.events))

    def test_virtual_sink_cannot_replace_discovered_alsa(self):
        self.assertFalse(audio.evaluate(self.joined.replace('node.name = "alsa_output.','node.name = "aetheri.virtual.'),self.manifest,0,BootEvidence.events))

    def test_udev_grant_is_required(self):
        self.assertFalse(audio.evaluate(self.joined.replace('AETHERI_AUDIO_UDEV_ACCESS uid=1000 gid=1000 mode=0600 verified=1\n',''),self.manifest,0,BootEvidence.events))

    def test_off_profile_is_not_playback(self):
        self.assertFalse(audio.evaluate(self.joined.replace('Analog Stereo Output [available]','Off [available]'),self.manifest,0,BootEvidence.events))

    def test_failed_udev_mode_update_is_not_success(self):
        self.assertFalse(audio.evaluate(self.joined+'timer: Failed to set owner/mode: Operation not permitted\n',self.manifest,0,BootEvidence.events))

    def test_unresolved_udev_user_is_not_success(self):
        self.assertFalse(audio.evaluate(self.joined+'Failed to resolve user 1000\n',self.manifest,0,BootEvidence.events))


class NativeDeviceEvidence(unittest.TestCase):
    manifest={**DeviceEvidence.manifest,'proof':'audio-control-hda','control_mixer':'qemu-software','control_case':'unity','control_volume':1.0,'control_muted':False}
    joined=DeviceEvidence.joined.replace('AETHERI_AUDIO_ACTOR role=server','AETHERI_AUDIO_NATIVE_BASELINE hardware_unity=1 before_server=1\nAETHERI_AUDIO_ACTOR role=server').replace('POLICY_CARD\n','POLICY_CARD\napi.alsa.soft-mixer = "true"\n').replace('POLICY_SINK\n','POLICY_SINK\nid 25, type PipeWire:Interface:Node\nobject.serial = "31"\n').replace('AETHERI_AUDIO_MIXER volume=74 switch=1','AETHERI_AUDIO_MIXER_OBSERVED volume=74 switch=1').replace('AETHERI_AUDIO_ACTOR role=player','AETHERI_AUDIO_ACTOR role=control uid=1000 cap_zero=1 nnp=1\nAETHERI_AUDIO_ACTOR role=player')+(
        'AETHERI_AUDIO_NATIVE_ACTOR uid=1000 gid=1000 cap_zero=1 nnp=1 groups=0\n'
        'AETHERI_AUDIO_NATIVE_PASS case=unity node=alsa_output.pci-test.analog-stereo id=25 serial=31 volume=1.000000 muted=0 observed=1\n'
        'AETHERI_AUDIO_NATIVE_CLOSED retired=1\n')

    def test_joined_native(self):self.assertTrue(audio.evaluate(self.joined,self.manifest,0,BootEvidence.events))

    def test_cli_control_cannot_replace_native(self):
        self.assertFalse(audio.evaluate(DeviceEvidence.joined,self.manifest,0,BootEvidence.events))

    def test_requires_callback_retirement(self):
        self.assertFalse(audio.evaluate(self.joined.replace('AETHERI_AUDIO_NATIVE_CLOSED retired=1',''),self.manifest,0,BootEvidence.events))

    def test_wrong_node_is_not_the_inspected_sink(self):
        self.assertFalse(audio.evaluate(self.joined.replace('node=alsa_output.pci-test.analog-stereo','node=alsa_output.foreign'),self.manifest,0,BootEvidence.events))

    def test_wrong_id(self):
        self.assertFalse(audio.evaluate(self.joined.replace(' id=25 serial=31',' id=26 serial=31'),self.manifest,0,BootEvidence.events))

    def test_wrong_serial(self):
        self.assertFalse(audio.evaluate(self.joined.replace(' id=25 serial=31',' id=25 serial=32'),self.manifest,0,BootEvidence.events))

    def test_wrong_effective_level(self):
        self.assertFalse(audio.evaluate(self.joined.replace('volume=1.000000 muted=0','volume=0.500000 muted=0'),self.manifest,0,BootEvidence.events))

    def test_native_failure_cannot_be_hidden_by_a_pass(self):
        self.assertFalse(audio.evaluate(self.joined+'AETHERI_AUDIO_NATIVE_FAIL reason=readback step=2\n',self.manifest,0,BootEvidence.events))

    def test_case_metadata_types(self):
        for change in ({'control_mixer':'hardware'},{'control_case':'foreign'},{'control_volume':True},{'control_muted':0},{'control_volume':.5}):
            self.assertFalse(audio.evaluate(self.joined,{**self.manifest,**change},0,BootEvidence.events))

    def test_requires_live_software_mixer_property(self):
        self.assertFalse(audio.evaluate(self.joined.replace('api.alsa.soft-mixer = "true"','api.alsa.soft-mixer = "false"'),self.manifest,0,BootEvidence.events))

    def test_requires_unchanged_hardware_level(self):
        self.assertFalse(audio.evaluate(self.joined.replace('MIXER_OBSERVED volume=74 switch=1','MIXER_OBSERVED volume=56 switch=1'),self.manifest,0,BootEvidence.events))

    def test_requires_baseline_before_server(self):
        line='AETHERI_AUDIO_NATIVE_BASELINE hardware_unity=1 before_server=1\n'
        self.assertFalse(audio.evaluate(self.joined.replace(line,''),self.manifest,0,BootEvidence.events))
        self.assertFalse(audio.evaluate(self.joined.replace(line,'')+line,self.manifest,0,BootEvidence.events))


class PoweredOffRetirement(unittest.TestCase):
    class Monitor:
        def __init__(self,error=None):self.error=error;self.requests=[]
        def execute(self,name):
            self.requests.append(name)
            if self.error:raise self.error

    class Process:
        def __init__(self,code=0):self.code=code;self.waits=0
        def wait(self,timeout):self.waits+=1;return self.code

    def state(self):return dict(guest_status=dict(running=False,status='shutdown'),guest_events=BootEvidence.events.copy())

    def test_acknowledged_quit(self):
        s=self.state();m=self.Monitor();p=self.Process();audio.finish_powered_off(m,p,s)
        self.assertTrue(s['quit_acknowledged']);self.assertEqual(m.requests,['quit']);self.assertEqual(p.waits,1)

    def test_reset_keeps_unknown_acknowledgement_without_replay(self):
        s=self.state();m=self.Monitor(ConnectionResetError('closed'));p=self.Process();audio.finish_powered_off(m,p,s)
        self.assertFalse(s['quit_acknowledged']);self.assertEqual(s['quit_transport_error'],'closed');self.assertEqual(m.requests,['quit'])

    def test_eof_keeps_unknown_acknowledgement_without_replay(self):
        s=self.state();m=self.Monitor(EOFError('closed'));p=self.Process();audio.finish_powered_off(m,p,s)
        self.assertFalse(s['quit_acknowledged']);self.assertEqual(m.requests,['quit'])

    def test_timeout_remains_failed(self):
        m=self.Monitor(TimeoutError('unknown'));p=self.Process()
        with self.assertRaises(TimeoutError):audio.finish_powered_off(m,p,self.state())
        self.assertEqual(m.requests,['quit']);self.assertEqual(p.waits,0)

    def test_nonzero_exit_remains_failed(self):
        with self.assertRaises(ValueError):audio.finish_powered_off(self.Monitor(EOFError()),self.Process(1),self.state())

    def test_running_guest_cannot_use_terminal_exception(self):
        s=self.state();s['guest_status']['running']=True;m=self.Monitor();p=self.Process()
        with self.assertRaises(ValueError):audio.finish_powered_off(m,p,s)
        self.assertEqual(m.requests,[]);self.assertEqual(p.waits,0)

    def test_host_shutdown_cannot_use_terminal_exception(self):
        s=self.state();s['guest_events']=[dict(event='SHUTDOWN',data=dict(guest=False,reason='host-qmp-quit'))];m=self.Monitor();p=self.Process()
        with self.assertRaises(ValueError):audio.finish_powered_off(m,p,s)
        self.assertEqual(m.requests,[])


if __name__=='__main__':unittest.main()
