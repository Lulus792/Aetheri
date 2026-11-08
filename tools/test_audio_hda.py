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


def validate_wav(raw):
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
    first=next((i for i,pair in enumerate(samples) if max(map(abs,pair))>2),None)
    if first is None or first<1:raise ValueError('missing phase-zero signal start')
    start=first-1;frames=96000
    if len(samples)<start+frames:raise ValueError('truncated signal')
    if any(abs(v)>2 for pair in samples[:start] for v in pair):raise ValueError('unexpected prefix')
    peak=0.0
    for i in range(frames):
        for channel,frequency in enumerate((440,880)):
            expected=4096*math.sin(2*math.pi*frequency*i/48000)
            peak=max(peak,abs(samples[start+i][channel]-expected))
    if peak>2:raise ValueError('wrong/missing/reordered samples: '+str(peak))
    if any(abs(v)>2 for pair in samples[start+frames:] for v in pair):raise ValueError('unexpected suffix')
    return dict(captured_frames=count,signal_start=start,signal_frames=frames,max_error_lsb=peak,sha256=hashlib.sha256(raw).hexdigest())


def evaluate(serial,manifest,exit_code,events,missing_device=False):
    preflight,_,_=boot.evaluate(serial,manifest,exit_code)
    if not preflight or not image_qmp.guest_poweroff(events):return False
    passes=re.findall(r'^AETHERI_AUDIO_PASS .*$',serial,re.MULTILINE)
    failures=re.findall(r'^AETHERI_AUDIO_FAIL reason=(\S+) errno=\d+\s*$',serial,re.MULTILINE)
    if missing_device:
        return not passes and failures==['device'] and 'AETHERI_AUDIO_ACTOR' not in serial and "aetheri-audio.service: Failed with result 'exit-code'." in serial
    expected='AETHERI_AUDIO_PASS nonce='+manifest['nonce']+' frames=96000 rate=48000 channels=2 server_end=1 player_end=1'
    actors=re.findall(r'^AETHERI_AUDIO_ACTOR role=(server|player) uid=1000 cap_zero=1 nnp=1\s*$',serial,re.MULTILINE)
    return (passes==[expected] and not failures and actors==['server','player']
            and serial.count('AETHERI_AUDIO_DEVICE pci=8086:2668 card=0 playback=1')==1
            and serial.count('AETHERI_AUDIO_MIXER volume=74 switch=1 channels=2 verified=1')==1
            and serial.count('DRAINED frames=96000')==1
            and 'aetheri-audio.service: Deactivated successfully.' in serial)


def capture_reply(reply,command):
    # Exact pinned-tool warning is retained in state, never treated as a generic
    # success. All other diagnostics still fail their command-specific check.
    prefix="warning: '"+command+"' is deprecated since v10.2, to be removed\r\n"
    return reply[len(prefix):] if reply.startswith(prefix) else reply


def run(image,output,missing_device=False,timeout=150):
    image=Path(image).resolve();output=vm.owned(output)
    if len(str(output/'qmp.sock').encode())>=108 or any(',' in str(p) for p in (image,output,boot.TOOLS)):
        raise ValueError('unsupported QEMU path')
    manifest=boot.inspect(image)
    if manifest.get('proof')!='audio-hda':raise ValueError('not the RAM HDA test image')
    sources={'hda_inputs_sha256':'spec/audio-hda-inputs.json','audio_inputs_sha256':'spec/audio-inputs.json',
             'builder_sha256':'tools/build_audio_hda.py','owner_sha256':'src/boot/audio.c'}
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
            monitor.execute('quit');process.wait(timeout=10)
            serial=(output/'serial.log').read_text(errors='replace').replace('\r\n','\n')
            if not evaluate(serial,manifest,process.returncode,state['guest_events'],missing_device):
                raise ValueError('guest/device/actor/shutdown evidence failed; preserve serial log')
            if not missing_device:state['pcm']=validate_wav((output/'capture.wav').read_bytes())
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
    a=p.parse_args();run(a.image,a.output,a.missing_device)
