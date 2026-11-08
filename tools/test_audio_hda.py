"""Cold-boot a separate diskless HDA guest and verify its host WAV capture.

No host audio, physical disk, network, carrier controller or USB is used. Keep
the original process and one host-local AF_UNIX QMP connection throughout.
"""
import argparse
import hashlib
import io
import math
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import time
import uuid
import wave

import image_qmp
import image_state
import run_vm as vm
import test_boot_probe as boot
from test_laptop_no_serial import retire_failed


def validate_wav(raw,gain=1.0,muted=False):
    if not math.isfinite(gain) or not 0<gain<=1:raise ValueError('capture gain bound')
    if not 44 <= len(raw) <= 8*1024**2:
        raise ValueError('WAV size bound')
    if raw[:4] != b'RIFF' or raw[8:12] != b'WAVE' or struct.unpack_from('<I',raw,4)[0] != len(raw)-8:
        raise ValueError('incomplete RIFF capture')
    with wave.open(io.BytesIO(raw),'rb') as source:
        if (source.getnchannels(),source.getsampwidth(),source.getframerate(),source.getcomptype()) != (2,2,48000,'NONE'):
            raise ValueError('different capture format')
        count=source.getnframes();pcm=source.readframes(count)
        if len(pcm)!=count*4:raise ValueError('truncated capture')
    samples=list(struct.iter_unpack('<hh',pcm))
    if muted:
        if count<96000 or any(v for pair in samples for v in pair):raise ValueError('missing duration or nonzero muted capture')
        return dict(captured_frames=count,silent=True,sha256=hashlib.sha256(raw).hexdigest())
    first=next((i for i,pair in enumerate(samples) if max(map(abs,pair))>2),None)
    if first is None or first<1:raise ValueError('missing phase-zero signal start')
    start=first-1;frames=96000
    if len(samples)<start+frames:raise ValueError('truncated signal')
    if any(abs(v)>2 for pair in samples[:start] for v in pair):raise ValueError('unexpected prefix')
    peak=0.0
    for i in range(frames):
        for channel,frequency in enumerate((440,880)):
            expected=gain*4096*math.sin(2*math.pi*frequency*i/48000)
            peak=max(peak,abs(samples[start+i][channel]-expected))
    if peak>2:raise ValueError('wrong/missing/reordered samples: '+str(peak))
    if any(abs(v)>2 for pair in samples[start+frames:] for v in pair):raise ValueError('unexpected suffix')
    return dict(captured_frames=count,signal_start=start,signal_frames=frames,max_error_lsb=peak,sha256=hashlib.sha256(raw).hexdigest())


def control_settings(manifest):
    if manifest.get('proof') not in ('audio-control-hda','audio-session'):return 1.0,False
    cases={'unity':(1.0,False),'half':(.5,False),'muted':(1.0,True)}
    expected=cases.get(manifest.get('control_case'))
    if manifest.get('control_mixer')!='qemu-software' or expected is None or type(manifest.get('control_volume')) is not float or type(manifest.get('control_muted')) is not bool or (manifest['control_volume'],manifest['control_muted'])!=expected:
        raise ValueError('invalid native HDA case')
    return expected


def session_settings(manifest):
    if manifest.get('proof')!='audio-session':return None
    case=manifest.get('session_case')
    if case not in ('playback','vt-loss','server-loss','policy-loss'):raise ValueError('invalid audio-session case')
    return case


def evaluate(serial,manifest,exit_code,events,missing_device=False):
    if 'AETHERI_AUDIO_COMMAND_TIMEOUT' in serial:return False
    preflight,_,_=boot.evaluate(serial,manifest,exit_code)
    if not preflight or not image_qmp.guest_poweroff(events):return False
    passes=re.findall(r'^AETHERI_AUDIO_PASS .*$',serial,re.MULTILINE)
    failures=re.findall(r'^AETHERI_AUDIO_FAIL reason=(\S+) errno=\d+\s*$',serial,re.MULTILINE)
    session=manifest.get('proof')=='audio-session'
    try:session_case=session_settings(manifest)
    except ValueError:return False
    if missing_device and session:
        return not passes and not failures and 'AETHERI_AUDIO_SESSION_PASS' not in serial and 'AETHERI_AUDIO_USER_BUS_FAIL' not in serial and re.findall(r'^AETHERI_AUDIO_SESSION_FAIL reason=(\S+) errno=\d+\s*$',serial,re.MULTILINE)==['device'] and 'AETHERI_AUDIO_ACTOR' not in serial and "aetheri-audio.service: Failed with result 'exit-code'." in serial
    if missing_device:
        return not passes and failures==['device'] and 'AETHERI_AUDIO_ACTOR' not in serial and "aetheri-audio.service: Failed with result 'exit-code'." in serial
    expected='AETHERI_AUDIO_PASS nonce='+manifest['nonce']+' frames=96000 rate=48000 channels=2 server_end=1 player_end=1'
    if session:
        expected='AETHERI_AUDIO_SESSION_PASS nonce='+manifest['nonce']+' user_end=1 runtime_removed=1 udev_end=1'
        passes=re.findall(r'^AETHERI_AUDIO_SESSION_PASS .*$',serial,re.MULTILINE)
        if 'AETHERI_AUDIO_SESSION_FAIL' in serial or 'AETHERI_AUDIO_USER_BUS_FAIL' in serial:return False
    native=manifest.get('proof') in ('audio-control-hda','audio-session')
    device=native or manifest.get('proof')=='audio-device'
    actors=re.findall(r'^AETHERI_AUDIO_ACTOR role=(server|policy|control|player) uid=1000 cap_zero=1 nnp=1\s*$',serial,re.MULTILINE)
    access=('AETHERI_AUDIO_SESSION_REVOKED before=denied active=allowed after=denied owner=0 group=63 mode=0660' if session else 'AETHERI_AUDIO_UDEV_ACCESS uid=1000 gid=1000 mode=0600 verified=1')
    ended=('AETHERI_AUDIO_USER_END originals=2 session_target_stopped=1' if session else 'AETHERI_AUDIO_POLICY alsa_monitor=1 acp=1 autolink=1 policy_end=1 udev_end=1')
    policy=(not device or ('Failed to set owner/mode' not in serial and 'Failed to resolve user' not in serial
             and serial.count('AETHERI_AUDIO_UDEV initialized=1 database=1')==1
             and serial.count(access)==1
             and serial.count(ended)==1
             and serial.count('AETHERI_AUDIO_ACTOR role=udev uid=0 cap_zero=0 nnp=1')==1
             and 'POLICY_SINK\n' in serial and 'node.name = "alsa_output.' in serial
             and 'POLICY_CARD\n' in serial and 'device.api = "alsa"' in serial and 'device.enum.api = "udev"' in serial
             and 'api.alsa.use-acp = "true"' in serial and 'api.alsa.card = "0"' in serial
             and 'POLICY_PROFILE\n' in serial and re.search(r'^\s*\*\s+[0-9]+\s+Analog Stereo Output \[available\]\s+prio=6500\s*$',serial,re.MULTILINE)))
    if session:
        owners=re.findall(r'^AETHERI_AUDIO_USER_MANAGER owner=(:[0-9]+\.[0-9]+)\s*$',serial,re.MULTILINE)
        retained=re.findall(r'^AETHERI_AUDIO_USER_REFS owner=(:[0-9]+\.[0-9]+) retained=4\s*$',serial,re.MULTILINE)
        policy=policy and len(owners)==1 and retained==owners
        units=re.findall(r'^AETHERI_AUDIO_USER_UNIT name=(aetheri-(?:pipewire|wireplumber)\.service) pid=([1-9][0-9]*) image=(/usr/bin/(?:pipewire|wireplumber))\s*$',serial,re.MULTILINE)
        policy=policy and len(units)==2 and units[0][0]=='aetheri-pipewire.service' and units[0][2]=='/usr/bin/pipewire' and units[1][0]=='aetheri-wireplumber.service' and units[1][2]=='/usr/bin/wireplumber' and units[0][1]!=units[1][1]
        policy=policy and len(re.findall(r'^AETHERI_AUDIO_LOCAL_SESSION id=[A-Za-z0-9_-]+ uid=1000 seat=seat0 active=1 remote=0 type=wayland class=user\s*$',serial,re.MULTILINE))==1
        policy=policy and serial.count('AETHERI_AUDIO_SESSION_ACCESS allowed=0 uid=1000 groups=0 cap_zero=1')==2 and serial.count('AETHERI_AUDIO_SESSION_ACCESS allowed=1 uid=1000 groups=1 cap_zero=1')==1
        for phase in ('before','after'):
            for path in ('/dev/snd/controlC0','/dev/snd/pcmC0D0p'):
                policy=policy and serial.count(f'AETHERI_AUDIO_SESSION_NODE phase={phase} path={path} uid=0 gid=63 mode=0660')==1
        if policy and passes==[expected] and serial.count('DRAINED frames=96000')==1:
            denied=[m.start() for m in re.finditer('AETHERI_AUDIO_SESSION_ACCESS allowed=0 uid=1000 groups=0 cap_zero=1',serial)]
            policy=(denied[0]<serial.index('AETHERI_AUDIO_LOCAL_SESSION ')<serial.index('AETHERI_AUDIO_SESSION_ACCESS allowed=1')<serial.index('AETHERI_AUDIO_USER_MANAGER ')<serial.index('AETHERI_AUDIO_USER_REFS ')<serial.index('AETHERI_AUDIO_USER_UNIT ')
                    <serial.index('DRAINED frames=96000')<serial.index(ended)<denied[1]<serial.index(access)<serial.index(expected))
        else:policy=False
        if session_case=='playback':policy=policy and 'AETHERI_AUDIO_LOSS_' not in serial and 'AETHERI_AUDIO_SESSION_LOSS ' not in serial
        else:
            observations=re.findall(r'^AETHERI_AUDIO_LOSS_OBSERVED case=(vt-loss|server-loss|policy-loss) mask=([1-7]) latched=1\s*$',serial,re.MULTILINE)
            completed=f'AETHERI_AUDIO_SESSION_LOSS case={session_case} originals=2 endpoint_removed=1 restart=0 reactivated={int(session_case=="vt-loss")}'
            bit={'vt-loss':1,'server-loss':2,'policy-loss':4}[session_case]
            policy=policy and len(observations)==1 and observations[0][0]==session_case and int(observations[0][1])&bit!=0 and serial.count(completed)==1
            if policy:
                observed='AETHERI_AUDIO_LOSS_OBSERVED case='
                policy=serial.index('DRAINED frames=96000')<serial.index(observed)<serial.index(completed)<serial.index(ended)
                if session_case=='vt-loss':
                    away='AETHERI_AUDIO_LOSS_INJECT kind=vt target=1';back='AETHERI_AUDIO_LOSS_INJECT kind=vt target=8'
                    inactive_access='AETHERI_AUDIO_SESSION_ACCESS allowed=0 uid=1000 groups=1 cap_zero=1'
                    policy=policy and serial.count(away)==serial.count(back)==serial.count(inactive_access)==1
                    if policy:policy=serial.index('DRAINED frames=96000')<serial.index(away)<serial.index(observed)<serial.index(inactive_access)<serial.index(back)<serial.index(completed)
                else:
                    injected=f'AETHERI_AUDIO_LOSS_INJECT kind={session_case.removesuffix("-loss")} original_pidfd=1'
                    policy=policy and serial.count(injected)==1 and serial.index('DRAINED frames=96000')<serial.index(injected)<serial.index(observed)
    mixer=serial.count('AETHERI_AUDIO_MIXER volume=74 switch=1 channels=2 verified=1')==1
    if native:
        try:volume,muted=control_settings(manifest)
        except ValueError:return False
        applied=re.findall(r'^AETHERI_AUDIO_NATIVE_PASS case=(unity|half|muted) node=([A-Za-z0-9._:-]+) id=([0-9]+) serial=([0-9]+) volume=([0-9.]+) muted=([01]) observed=1\s*$',serial,re.MULTILINE)
        section=serial.split('POLICY_SINK\n',1)[-1].split('POLICY_CARD\n',1)[0]
        selected=re.search(r'^id ([0-9]+), type PipeWire:Interface:Node\s*$',section,re.MULTILINE)
        names=re.findall(r'^\s*\*?\s*node.name = "([A-Za-z0-9._:-]+)"\s*$',section,re.MULTILINE)
        ids=re.findall(r'^\s*\*?\s*object.serial = "([0-9]+)"\s*$',section,re.MULTILINE)
        policy=policy and len(applied)==1 and selected is not None and len(names)==len(ids)==1
        if policy:
            case,node,bound,serial_id,level,mute=applied[0]
            policy=(case==manifest['control_case'] and node==names[0] and bound==selected.group(1) and serial_id==ids[0] and int(serial_id)>0 and level==f'{volume:.6f}' and int(mute)==int(muted))
        expected_groups=1 if session else 0
        policy=policy and serial.count('AETHERI_AUDIO_NATIVE_CLOSED retired=1')==1 and serial.count(f'AETHERI_AUDIO_NATIVE_ACTOR uid=1000 gid=1000 cap_zero=1 nnp=1 groups={expected_groups}')==1 and 'AETHERI_AUDIO_NATIVE_FAIL' not in serial
        card=serial.split('POLICY_CARD\n',1)[-1].split('POLICY_PROFILE\n',1)[0]
        policy=policy and len(re.findall(r'^\s*\*?\s*api.alsa.soft-mixer = "true"\s*$',card,re.MULTILINE))==1
        baseline='AETHERI_AUDIO_NATIVE_BASELINE hardware_unity=1 before_server=1'
        policy=policy and serial.count(baseline)==1 and serial.index(baseline)<serial.find('AETHERI_AUDIO_ACTOR role=server')
        mixer=serial.count('AETHERI_AUDIO_MIXER_OBSERVED volume=74 switch=1 channels=2 verified=1')==1
    return (passes==[expected] and not failures and actors==(['server','policy','control','player'] if native else ['server','policy','player'] if device else ['server','player']) and policy
            and serial.count('AETHERI_AUDIO_DEVICE pci=8086:2668 card=0 playback=1')==1
            and mixer
            and serial.count('DRAINED frames=96000')==1
            and 'aetheri-audio.service: Deactivated successfully.' in serial)


def capture_reply(reply,command):
    # Exact pinned-tool warning is retained in state, never treated as a generic
    # success. All other diagnostics still fail their command-specific check.
    prefix="warning: '"+command+"' is deprecated since v10.2, to be removed\r\n"
    return reply[len(prefix):] if reply.startswith(prefix) else reply


def finish_powered_off(monitor,process,state):
    # QEMU can close its original transport before the quit response reaches
    # Windows. Never replay quit or fabricate its acknowledgement. The already
    # observed guest shutdown and original process exit independently retire
    # this diskless test; an uncertain reply remains in the QMP journal.
    status=state.get('guest_status',{})
    if status.get('running') is not False or status.get('status')!='shutdown' or not image_qmp.guest_poweroff(state.get('guest_events',[])):
        raise ValueError('quit requires original guest poweroff')
    state['quit_acknowledged']=False
    try:
        monitor.execute('quit');state['quit_acknowledged']=True
    except (EOFError,ConnectionResetError) as error:
        state['quit_transport_error']=str(error)[:1024]
    if process.wait(timeout=10)!=0:raise ValueError('original powered-off VM exit failed')


def run(image,output,missing_device=False,timeout=150,icount=False):
    if type(icount) is not bool:raise ValueError('instruction-clock selection must be explicit boolean')
    image=Path(image).resolve();output=vm.owned(output)
    if len(str(output/'qmp.sock').encode())>=108 or any(',' in str(p) for p in (image,output,boot.TOOLS)):
        raise ValueError('unsupported QEMU path')
    manifest=boot.inspect(image)
    if manifest.get('proof') not in ('audio-hda','audio-device','audio-control-hda','audio-session'):raise ValueError('not the RAM HDA test image')
    volume,muted=control_settings(manifest)
    session_settings(manifest)
    sources={'hda_inputs_sha256':'spec/audio-hda-inputs.json','audio_inputs_sha256':'spec/audio-inputs.json',
             'builder_sha256':'tools/build_audio_hda.py','owner_sha256':'src/boot/audio.c'}
    if manifest['proof'] in ('audio-device','audio-control-hda','audio-session'):
        sources.update(device_builder_sha256='tools/build_audio_device.py',device_inputs_sha256='spec/audio-device-inputs.json',policy_inputs_sha256='spec/audio-policy-inputs.json',policy_preparer_sha256='tools/prepare_audio_policy.py')
    if manifest['proof'] in ('audio-control-hda','audio-session'):
        sources.update(native_builder_sha256='tools/build_audio_control_hda.py',control_inputs_sha256='spec/audio-control-inputs.json',control_preparer_sha256='tools/prepare_audio_control.py',control_source_sha256='src/audio/control.c',control_header_sha256='src/audio/control.h',remote_source_sha256='src/audio/remote.c',remote_header_sha256='src/audio/remote.h',probe_source_sha256='src/audio/control_probe.c',player_source_sha256='src/audio/check.c',cmake_sha256='CMakeLists.txt')
    if manifest['proof']=='audio-session':
        sources.update(session_builder_sha256='tools/build_audio_session.py',session_units_sha256='tools/audio_session_units.py',session_runtime_sha256='tools/audio_session_runtime.py',session_source_sha256='src/boot/audio_session.c',session_guard_source_sha256='src/audio/session_guard.c',session_guard_header_sha256='src/audio/session_guard.h',session_inputs_sha256='spec/desktop-image-inputs.json')
    for key,path in sources.items():
        # Source archives use normalized LF on both build and host.
        if manifest[key]!=hashlib.sha256((boot.ROOT/path).read_bytes().replace(b'\r\n',b'\n')).hexdigest():
            raise ValueError('image source changed: '+path)
    tools=boot.host_tools();output.mkdir(mode=0o700);vm.private_path(output,directory=True)
    shutil.copyfile(boot.TOOLS/'share/edk2-i386-vars.fd',output/'nvram.fd')
    identifier=str(uuid.uuid4())
    args=[str(boot.TOOLS/'qemu-system-x86_64.exe'),'-name','aetheri-diskless-hda','-uuid',identifier,
          '-machine','pc-q35-11.1,accel=tcg','-cpu','max','-smp','2','-m','768M',
          '-no-user-config','-nodefaults','-display','none','-monitor','none','-nic','none','-no-reboot','-no-shutdown','-S',
          '-drive','if=pflash,format=raw,readonly=on,file='+str(boot.TOOLS/'share/edk2-x86_64-code.fd'),
          '-drive','if=pflash,format=raw,file='+str(output/'nvram.fd'),
          '-kernel',str(image/'vmlinuz'),'-initrd',str(image/'initramfs.cpio.gz'),
          '-append',(image/'cmdline.txt').read_text().strip(),'-serial','file:'+str(output/'serial.log'),
          '-parallel','none','-qmp','unix:'+str(output/'qmp.sock')+',server=on,wait=off']
    if not missing_device:
        args+=['-audiodev','none,id=audio,out.frequency=48000,out.channels=2,out.format=s16',
               '-device','intel-hda,id=hda','-device','hda-output,bus=hda.0,audiodev=audio']
    if icount:args+=['-icount','shift=0,align=off,sleep=off','-rtc','clock=vm']
    state=dict(state='dispatch_pending',uuid=identifier,command=args,missing_device=missing_device,
               manifest_sha256=boot.sha(image/'manifest.json'),host_tools=tools,qmp={},
               controller_sha256=boot.sha(Path(__file__)),qmp_sha256=boot.sha(Path(image_qmp.__file__)))
    def save():image_state.write(output/'result.json',state)
    save();monitor=None
    with (output/'qemu.log').open('xb') as log:
        process=subprocess.Popen(args,stdin=subprocess.DEVNULL,stdout=log,stderr=log,
                                 creationflags=subprocess.CREATE_NO_WINDOW if os.name=='nt' else 0)
        try:
            state.update(state='running',pid=process.pid);save();start=time.monotonic()
            while not (output/'qmp.sock').exists():
                if process.poll() is not None or time.monotonic()-start>30:raise RuntimeError('original QMP did not appear')
                time.sleep(.1)
            monitor=image_qmp.Connection(output/'qmp.sock',identifier,process,state['qmp'],save)
            if not missing_device:
                capture_path=(output/'capture.wav').as_posix()
                if not re.fullmatch('[A-Za-z0-9:/_.-]+',capture_path):raise ValueError('unsupported capture path')
                # Pinned QEMU 11.1 test-only capture. This deprecated monitor API
                # is kept out of Aetheri runtime; explicit stop finalizes RIFF.
                reply=monitor.execute('human-monitor-command',{'command-line':'wavcapture "'+capture_path+'" audio 48000 16 2'})
                state['capture_start']=reply;save()
                if capture_reply(reply,'wavcapture'):raise ValueError('capture start returned a diagnostic')
                reply=monitor.execute('human-monitor-command',{'command-line':'info capture'})
                state['capture_active']=reply;save()
                if capture_reply(reply,'info capture')!='[0]: Capturing audio(48000,16,2) to '+capture_path+': 0 bytes\r\n':raise ValueError('capture not independently listed')
            monitor.execute('cont')
            while not image_qmp.guest_poweroff(state['qmp']['events']):
                if any(e.get('event')=='SHUTDOWN' and e.get('data',{}).get('reason')=='guest-reset' for e in state['qmp']['events']):
                    raise RuntimeError('original guest reset instead of powering off')
                if time.monotonic()-start>timeout:raise TimeoutError('original audio guest deadline')
                if process.poll() is not None:raise RuntimeError('VM ended before guest shutdown')
                monitor.poll(.2)
            state['guest_events']=list(state['qmp']['events'])
            status=monitor.execute('query-status');state['guest_status']=status;save()
            if status.get('running') is not False or status.get('status')!='shutdown':raise ValueError('guest did not remain powered off')
            if not missing_device:
                reply=monitor.execute('human-monitor-command',{'command-line':'stopcapture 0'})
                state['capture_stop']=reply;save()
                if capture_reply(reply,'stopcapture') or monitor.execute('human-monitor-command',{'command-line':'info capture'}):raise ValueError('capture not retired')
            finish_powered_off(monitor,process,state)
            serial=(output/'serial.log').read_text(errors='replace').replace('\r\n','\n')
            if not evaluate(serial,manifest,process.returncode,state['guest_events'],missing_device):
                raise ValueError('guest/device/actor/shutdown evidence failed; preserve serial log')
            if not missing_device:state['pcm']=validate_wav((output/'capture.wav').read_bytes(),volume**3,muted)
            if boot.inspect(image)!=manifest:raise ValueError('source image changed')
            state.update(state='passed',exit=process.returncode,elapsed_seconds=time.monotonic()-start);save()
        except BaseException as error:
            state.update(state='failed_preserved',error=str(error)[:1024])
            issues=retire_failed(process,monitor,state,save);monitor=None
            if issues:print('Audio fixture retirement issues: '+repr(issues),flush=True)
            raise
        finally:
            if monitor:monitor.close()
    return state


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--image',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--missing-device',action='store_true')
    p.add_argument('--icount',action='store_true',help='Explicit instruction-clock test profile; not real-time evidence')
    a=p.parse_args();run(a.image,a.output,a.missing_device,icount=a.icount)
