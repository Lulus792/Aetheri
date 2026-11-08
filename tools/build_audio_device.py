"""Join real udev/ALSA monitoring and policy in a fresh diskless test image.

No package installation, module loading or host/device operation occurs here.
The preserved manual HDA image remains an independent control.
"""
import argparse
import gzip
import json
import os
from pathlib import Path
import shutil
import subprocess

import build_audio_hda as manual
import desktop_payload
import prepare_audio as audio
import prepare_audio_policy as policy

PINS=audio.ROOT/'spec/audio-device-inputs.json'


def build(output,nonce,sdk,hardware,manager):
    policy.check(manager);pins=json.loads(PINS.read_bytes());desktop_payload.validate(pins)
    output=Path(output).absolute()
    if output.is_symlink() or output.resolve()!=output:raise ValueError('aliased device output')
    # manual.build also enforces ordinary native x86-64 Linux before creation.
    if os.name!='posix' or os.getuid()==0 or os.uname().sysname!='Linux' or os.uname().machine!='x86_64':raise ValueError('ordinary x86-64 Linux builder required')
    output.mkdir(mode=0o700);manual.build(output/'manual',nonce,sdk,hardware)
    tree=output/'root';shutil.copytree(output/'manual/root',tree,symlinks=True)
    logs=output/'logs';logs.mkdir();base=json.loads((output/'manual/image/manifest.json').read_text())
    def replace(name,text):
        p=tree/name
        if p.is_symlink() or audio.sha(p.read_bytes())!=base['root'][name]['sha256']:raise ValueError('changed copied fixture input')
        p.write_text(text)
    def put(name,text):
        p=tree/name;p.parent.mkdir(parents=True,exist_ok=True)
        with p.open('x') as f:f.write(text)
        p.chmod(0o644)
    selected,_=policy.profile();entries=[]
    for e in selected['files']:
        entries.append(dict(source=str(Path(manager)/e['path']),destination='/'+e['path'],size=e['size'],sha256=e['sha256'],mode=e['mode'],package=e['package']))
    for e in selected['existing_runtime']['files']:
        entries.append({**e,'source':str(Path(manager)/e['destination'].lstrip('/'))})
    aliases={**{'/'+n:t for n,t in selected['symlinks'].items()},**selected['existing_runtime']['symlinks']}
    desktop_payload.install(tree,dict(format=1,files=entries,symlinks=aliases));desktop_payload.install(tree,pins)
    (tree/'usr/bin/systemd-udevd').symlink_to('udevadm')
    command=['gcc','-std=c17','-O2','-Wall','-Wextra','-Wpedantic','-Wconversion','-Wshadow','-Werror','-DAETHERI_AUDIO_POLICY',str(audio.ROOT/'src/boot/audio.c'),'-o',str(tree/'usr/libexec/aetheri-audio-policy-owner')]
    p=subprocess.run(command,capture_output=True);(logs/'owner-build.log').write_bytes(p.stdout+p.stderr);p.check_returncode()
    for name in ('usr/libexec/aetheri-audio-owner','etc/aetheri-audio/alsa.conf'):
        p=tree/name
        if p.is_symlink() or audio.sha(p.read_bytes())!=base['root'][name]['sha256']:raise ValueError('changed copied retired fixture file')
        p.unlink()
    # udev changes mode after changing ownership; FOWNER is needed for that
    # second operation. Only this disposable root supervisor retains it. Every
    # server/manager/player child still clears all capabilities before exec.
    unit=(tree/'etc/systemd/system/aetheri-audio.service').read_text().replace('ExecStart=/usr/libexec/aetheri-audio-owner','ExecStart=/usr/libexec/aetheri-audio-policy-owner').replace('TimeoutStartSec=45','TimeoutStartSec=90').replace('CAP_DAC_OVERRIDE','CAP_DAC_OVERRIDE CAP_NET_ADMIN CAP_FOWNER').replace('Description=Fixed QEMU HDA owner','Description=Aetheri discovered HDA policy test')
    replace('etc/systemd/system/aetheri-audio.service',unit)
    replace('etc/passwd',(tree/'etc/passwd').read_text()+'aetheri:x:1000:1000:Fixed audio test user:/run/aetheri-audio:/nonexistent\n')
    replace('etc/group',(tree/'etc/group').read_text()+'aetheri:x:1000:\n')
    common='context.spa-libs = { support.* = support/libspa-support audio.convert.* = audioconvert/libspa-audioconvert api.alsa.* = alsa/libspa-alsa }\n'
    replace('etc/aetheri-audio/client.conf',common+'''context.properties = { mem.allow-mlock = false support.dbus = false }
context.modules = [
 { name = libpipewire-module-protocol-native }
 { name = libpipewire-module-metadata }
 { name = libpipewire-module-client-node }
 { name = libpipewire-module-adapter }
]
''')
    replace('etc/aetheri-audio/server.conf',common+'''context.properties = { core.daemon = true core.name = pw-hda default.clock.rate = 48000 default.clock.quantum = 1024 default.clock.min-quantum = 1024 default.clock.max-quantum = 1024 mem.allow-mlock = false support.dbus = false }
context.modules = [
 { name = libpipewire-module-protocol-native args = { sockets = [ { name = pw-hda mode = 0600 } ] } }
 { name = libpipewire-module-client-node }
 { name = libpipewire-module-client-device }
 { name = libpipewire-module-adapter }
 { name = libpipewire-module-link-factory }
 { name = libpipewire-module-spa-node-factory }
 { name = libpipewire-module-metadata }
 { name = libpipewire-module-access args = { access.legacy = false access.socket = { pw-hda = unrestricted } } }
]
''')
    put('etc/aetheri-wireplumber/wireplumber.conf','context.properties = { mem.allow-mlock = false support.dbus = false }\n'+(Path(manager)/'usr/share/wireplumber/wireplumber.conf').read_text())
    put('etc/aetheri-wireplumber/wireplumber.conf.d/90-aetheri-hda-test.conf','''wireplumber.profiles = {
 aetheri-hda-test = {
  inherits = [ main-embedded ]
  monitor.alsa = required
  monitor.alsa-midi = disabled
  hardware.bluetooth = disabled
  hardware.video-capture = disabled
  support.dbus = disabled
  support.system-dbus = disabled
  api.notifications = disabled
 }
}
monitor.alsa.properties = { alsa.use-acp = true }
monitor.alsa.rules = [
 { matches = [ { device.name = "~alsa_card.*" } ] actions = { update-props = { api.alsa.use-acp = true api.alsa.use-ucm = false } } }
 { matches = [ { node.name = "~alsa_output.*" } ] actions = { update-props = {
    audio.format = S16LE audio.rate = 48000 audio.channels = 2
    api.alsa.period-size = 1024 api.alsa.disable-batch = true
    node.always-process = true node.pause-on-idle = false node.suspend-on-idle = false session.suspend-timeout-seconds = 0
 } } }
]
''')
    put('etc/udev/rules.d/90-aetheri-hda-test.rules','''SUBSYSTEM=="sound", KERNEL=="card0", ATTRS{vendor}=="0x8086", ATTRS{device}=="0x2668", IMPORT{builtin}="path_id", ENV{ID_SOUND}="1", ENV{ID_BUS}="pci", ENV{SOUND_INITIALIZED}="1"
SUBSYSTEM=="sound", KERNEL=="controlC0|pcmC0D0p|timer", OWNER:="1000", GROUP:="1000", MODE:="0600"
''')
    image=output/'image';image.mkdir()
    for name in ('vmlinuz','cmdline.txt'):shutil.copyfile(output/'manual/image'/name,image/name)
    paths=sorted(tree.rglob('*'),key=lambda p:p.relative_to(tree).as_posix())
    for p in [tree,*paths]:os.utime(p,(0,0),follow_symlinks=False)
    names=b'\0'.join(p.relative_to(tree).as_posix().encode() for p in paths)+b'\0'
    packed=subprocess.run(['cpio','--null','--create','--format=newc','--owner=0:0','--reproducible','--quiet'],input=names,cwd=tree,capture_output=True,check=True)
    (image/'initramfs.cpio.gz').write_bytes(gzip.compress(packed.stdout,mtime=0))
    objects={}
    for p in paths:
        name=p.relative_to(tree).as_posix()
        if p.is_symlink():objects[name]={'symlink':os.readlink(p)}
        elif p.is_file():objects[name]={'sha256':audio.sha(p.read_bytes()),'mode':oct(p.stat().st_mode&0o7777)}
    manifest={**base,'proof':'audio-device','purpose':'Diskless udev/ACP/ALSA policy test, not normal admission or physical audio','root':objects,'device_builder_sha256':audio.sha(Path(__file__).read_bytes()),'device_inputs_sha256':audio.sha(PINS.read_bytes()),'policy_inputs_sha256':audio.sha(policy.PROFILE.read_bytes()),'policy_preparer_sha256':audio.sha((audio.ROOT/'tools/prepare_audio_policy.py').read_bytes()),'outputs':{n:audio.sha((image/n).read_bytes()) for n in ('vmlinuz','initramfs.cpio.gz','cmdline.txt')}}
    (image/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n');return manifest


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('output',type=Path);p.add_argument('nonce');p.add_argument('sdk',type=Path);p.add_argument('hardware',type=Path);p.add_argument('manager',type=Path);a=p.parse_args();build(a.output,a.nonce,a.sdk,a.hardware,a.manager)
