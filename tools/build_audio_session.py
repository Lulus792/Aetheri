"""Build a fresh diskless PAM/logind/user-manager audio integration image."""
import argparse
import gzip
import json
import os
from pathlib import Path
import shutil
import subprocess

import audio_session_runtime
import audio_session_units
import build_audio_control_hda as base
import desktop_payload
import prepare_audio as audio


def build(output,nonce,sdk,hardware,manager,headers):
    if os.name!='posix' or os.getuid()==0 or os.uname().machine!='x86_64':raise ValueError('ordinary native builder required')
    output=Path(output).absolute()
    if output.is_symlink() or output.resolve()!=output:raise ValueError('aliased session output')
    output.mkdir(mode=0o700);original=base.build(output/'native',nonce,sdk,hardware,manager,headers,'half')
    tree=output/'root';shutil.copytree(output/'native/root',tree,symlinks=True);logs=output/'logs';logs.mkdir()
    def put(name,text):
        p=tree/name;p.parent.mkdir(parents=True,exist_ok=True)
        with p.open('x') as f:f.write(text)
        p.chmod(0o644)
    def replace(name,text):
        p=tree/name
        if p.is_symlink() or audio.sha(p.read_bytes())!=original['root'][name]['sha256']:raise ValueError('changed copied session input: '+name)
        p.write_text(text)
    runtime,pins_hash=audio_session_runtime.select(audio.ROOT/'spec/desktop-image-inputs.json',tree)
    baseline={'/'+n:e['sha256'] for n,e in original['root'].items() if 'sha256' in e}
    normalized=desktop_payload.normalize_base(tree,runtime,baseline);receipt=desktop_payload.install(tree,runtime)
    (output/'session-runtime.json').write_text(json.dumps(dict(profile=runtime,receipt=receipt,normalized=normalized),indent=2)+'\n')
    args=['gcc','-std=c17','-O2','-g','-Wall','-Wextra','-Wpedantic','-Wconversion','-Wshadow','-Werror',str(audio.ROOT/'src/boot/audio_session.c'),str(audio.ROOT/'src/audio/session_guard.c'),'-o',str(tree/'usr/libexec/aetheri-audio-session-check'),'-lsystemd']
    p=subprocess.run(args,capture_output=True);(logs/'session-build.log').write_bytes(p.stdout+p.stderr);p.check_returncode()
    def link(name,target):
        p=tree/name;p.parent.mkdir(parents=True,exist_ok=True);p.symlink_to(target)
    link('sbin','usr/bin');link('usr/sbin','bin')
    link('usr/lib/systemd/system/dbus.service','dbus-broker.service')
    link('usr/lib/systemd/system/dbus-org.freedesktop.login1.service','systemd-logind.service')
    link('usr/lib/systemd/user/dbus.service','dbus-broker.service')
    put('etc/systemd/user/default.target',(tree/'usr/lib/systemd/user/default.target').read_text()+'\n[Unit]\nWants=dbus.socket\n')
    replace('etc/passwd',(tree/'etc/passwd').read_text().replace(':/run/aetheri-audio:',':/home/aetheri:')+'dbus:x:81:81:D-Bus:/nonexistent:/nonexistent\n')
    replace('etc/group',(tree/'etc/group').read_text()+'dbus:x:81:\ntty:x:5:\ndisk:x:6:\nlp:x:7:\ncdrom:x:11:\ndialout:x:18:\ntape:x:33:\nkvm:x:36:\naudio:x:63:\nsgx:x:995:\ninput:x:998:\nvideo:x:997:\nrender:x:996:\nclock:x:103:\nkmem:x:9:\n')
    replace('etc/udev/rules.d/90-aetheri-hda-test.rules','SUBSYSTEM=="sound", KERNEL=="card0", ATTRS{vendor}=="0x8086", ATTRS{device}=="0x2668", IMPORT{builtin}="path_id", ENV{ID_SOUND}="1", ENV{ID_BUS}="pci", ENV{SOUND_INITIALIZED}="1"\n')
    put('etc/pam.d/aetheri-audio-test','auth required pam_permit.so\naccount required pam_permit.so\nsession required pam_systemd.so type=wayland class=user\n')
    put('etc/pam.d/systemd-user','auth required pam_permit.so\naccount required pam_permit.so\nsession required pam_systemd.so class=manager\n')
    put('etc/pam.d/other','auth required pam_deny.so\naccount required pam_deny.so\npassword required pam_deny.so\nsession required pam_deny.so\n')
    put('etc/systemd/logind.conf.d/aetheri-test.conf','[Login]\nNAutoVTs=0\nReserveVT=0\nUserStopDelaySec=0\n')
    put('etc/systemd/journald.conf.d/aetheri-test.conf','[Journal]\nStorage=volatile\nRuntimeMaxUse=8M\nForwardToConsole=yes\nTTYPath=/dev/ttyS0\nMaxLevelConsole=debug\n')
    for unit in ('dbus-broker.service','systemd-logind.service','user-runtime-dir@.service','user@.service','systemd-user-sessions.service'):
        stream='file:/dev/ttyS0' if unit=='user@.service' else 'journal'
        # PID1 can enforce the ordinary manager's capability boundary. An
        # unprivileged child manager cannot lower its own bounding mask, and
        # pam_systemd otherwise seeds CAP_WAKE_ALARM into inherited state.
        boundary='CapabilityBoundingSet=\nAmbientCapabilities=\nNoNewPrivileges=yes\n' if unit=='user@.service' else ''
        put('etc/systemd/system/'+unit+'.d/aetheri-test.conf','[Service]\nRestart=no\nStandardOutput='+stream+'\nStandardError='+stream+'\n'+boundary)
    put('etc/systemd/system/dbus.socket.d/aetheri-test.conf','[Unit]\nDefaultDependencies=no\n')
    put('etc/systemd/user/dbus-broker.service.d/aetheri-test.conf','[Service]\nRestart=no\n')
    for name,body in audio_session_units.units().items():put('etc/systemd/user/'+name,body)
    for name in ('aetheri-pipewire.service','aetheri-wireplumber.service'):
        # Keep failure logs in this user's runtime; no serial-open permission
        # or persistent log tree is granted to the ordinary audio processes.
        path='%t/'+name+'.log'
        put('etc/systemd/user/'+name+'.d/aetheri-test.conf','[Service]\nStandardOutput=append:'+path+'\nStandardError=append:'+path+'\n')
    put('etc/systemd/user/aetheri-session.target','[Unit]\nDescription=Fixed test login session\n')
    put('etc/systemd/system/aetheri-audio-login.service','''[Unit]
Description=Fixed PAM audio-session test
Requires=systemd-logind.service
After=systemd-logind.service
[Service]
Type=oneshot
User=aetheri
Group=aetheri
PAMName=aetheri-audio-test
TTYPath=/dev/tty8
StandardInput=tty
StandardOutput=file:/dev/ttyS0
StandardError=file:/dev/ttyS0
NoNewPrivileges=yes
CapabilityBoundingSet=
AmbientCapabilities=
Environment=XDG_SESSION_TYPE=wayland XDG_SESSION_CLASS=user XDG_SEAT=seat0 XDG_VTNR=8
Environment=DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/1000/bus
ExecStart=/usr/libexec/aetheri-audio-session-check --user
TimeoutStartSec=90
Restart=no
''')
    unit=(tree/'etc/systemd/system/aetheri-audio.service').read_text().replace('ExecStart=/usr/libexec/aetheri-audio-policy-owner','ExecStart=/usr/libexec/aetheri-audio-session-check').replace('TimeoutStartSec=90','TimeoutStartSec=140').replace('CAP_FOWNER','CAP_FOWNER CAP_SYS_TTY_CONFIG')
    replace('etc/systemd/system/aetheri-audio.service',unit)
    sources={'session_builder_sha256':'tools/build_audio_session.py','session_units_sha256':'tools/audio_session_units.py','session_runtime_sha256':'tools/audio_session_runtime.py','session_source_sha256':'src/boot/audio_session.c','session_guard_source_sha256':'src/audio/session_guard.c','session_guard_header_sha256':'src/audio/session_guard.h'}
    put('etc/aetheri-audio/session-case','playback\n')
    for case in ('playback','vt-loss','server-loss','policy-loss'):
        # Repack the same independently built root; only this fixed selector
        # differs. No actor/input is selected from a runtime reply or marker.
        (tree/'etc/aetheri-audio/session-case').write_text(case+'\n')
        image=output/('image' if case=='playback' else case+'/image');image.mkdir(parents=True)
        for name in ('vmlinuz','cmdline.txt'):shutil.copyfile(output/'native/image'/name,image/name)
        paths=sorted(tree.rglob('*'),key=lambda p:p.relative_to(tree).as_posix())
        for p in [tree,*paths]:os.utime(p,(0,0),follow_symlinks=False)
        names=b'\0'.join(p.relative_to(tree).as_posix().encode() for p in paths)+b'\0'
        packed=subprocess.run(['cpio','--null','--create','--format=newc','--owner=0:0','--reproducible','--quiet'],input=names,cwd=tree,capture_output=True,check=True)
        (image/'initramfs.cpio.gz').write_bytes(gzip.compress(packed.stdout,mtime=0))
        objects={}
        for p in paths:
            n=p.relative_to(tree).as_posix()
            if p.is_symlink():objects[n]={'symlink':os.readlink(p)}
            elif p.is_file():objects[n]={'sha256':audio.sha(p.read_bytes()),'mode':oct(p.stat().st_mode&0o7777)}
        manifest={**original,'proof':'audio-session','session_case':case,'purpose':'Isolated PAM/logind/user-manager audio fixture, not normal app/device admission','root':objects,'session_inputs_sha256':pins_hash,**{k:audio.sha((audio.ROOT/n).read_bytes()) for k,n in sources.items()},'outputs':{n:audio.sha((image/n).read_bytes()) for n in ('vmlinuz','initramfs.cpio.gz','cmdline.txt')}}
        (image/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    return json.loads((output/'image/manifest.json').read_text())


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('output','sdk','hardware','manager','headers'):p.add_argument(name,type=Path)
    p.add_argument('--nonce',required=True);a=p.parse_args();build(a.output,a.nonce,a.sdk,a.hardware,a.manager,a.headers)
