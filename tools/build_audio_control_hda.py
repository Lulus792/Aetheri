"""Build a fresh RAM guest using native library controls on the discovered HDA."""
import argparse
import gzip
import json
import os
from pathlib import Path
import shutil
import subprocess

import build_audio_device as device
import prepare_audio as audio
import prepare_audio_control as control


def build(output,nonce,sdk,hardware,manager,headers,test='unity'):
    if test not in ('unity','half','muted'):raise ValueError('unsupported native HDA case')
    if os.name!='posix' or os.getuid()==0 or os.uname().sysname!='Linux' or os.uname().machine!='x86_64':raise ValueError('ordinary x86-64 Linux builder required')
    control.check(headers);output=Path(output).absolute()
    if output.is_symlink() or output.resolve()!=output:raise ValueError('aliased native HDA output')
    output.mkdir(mode=0o700)
    base=device.build(output/'device',nonce,sdk,hardware,manager)
    tree=output/'root';shutil.copytree(output/'device/root',tree,symlinks=True)
    logs=output/'logs';logs.mkdir();native=output/'native-build'
    def run(name,args):
        p=subprocess.run(args,stdout=subprocess.PIPE,stderr=subprocess.STDOUT);(logs/(name+'.log')).write_bytes(p.stdout);p.check_returncode()
    run('configure',['cmake','-S',str(audio.ROOT),'-B',str(native),'-G','Ninja','-DCMAKE_BUILD_TYPE=Release','-DCMAKE_SKIP_RPATH=ON','-DAETHERI_AUDIO=ON','-DAETHERI_AUDIO_CONTROL=ON','-DAETHERI_AUDIO_PREFIX='+str(sdk),'-DAETHERI_AUDIO_POLICY_PREFIX='+str(manager),'-DAETHERI_AUDIO_CONTROL_PREFIX='+str(headers)])
    run('probe-build',['cmake','--build',str(native),'-j2','--target','aetheri-audio-control-probe'])
    shutil.copyfile(native/'aetheri-audio-control-probe',tree/'usr/libexec/aetheri-audio-control-probe');(tree/'usr/libexec/aetheri-audio-control-probe').chmod(0o755)
    owner=tree/'usr/libexec/aetheri-audio-policy-owner'
    if owner.is_symlink() or audio.sha(owner.read_bytes())!=base['root']['usr/libexec/aetheri-audio-policy-owner']['sha256']:raise ValueError('changed copied owner')
    run('owner-build',['gcc','-std=c17','-O2','-Wall','-Wextra','-Wpedantic','-Wconversion','-Wshadow','-Werror','-DAETHERI_AUDIO_POLICY','-DAETHERI_AUDIO_NATIVE_CONTROL',str(audio.ROOT/'src/boot/audio.c'),'-o',str(owner)])
    # This immutable RAM-fixture selector is not a normal configuration/API.
    with (tree/'etc/aetheri-audio/native-case').open('x') as f:f.write(test+'\n')
    (tree/'etc/aetheri-audio/native-case').chmod(0o644)
    # QEMU 11.1 advertises dB amplifier steps but applies gain linearly. Keep
    # its emulated mixer enabled; use maintained software volume/mute only for
    # this exact disposable card. Do not export this quirk to physical devices.
    with (tree/'etc/aetheri-wireplumber/wireplumber.conf.d/91-native-qemu-mixer.conf').open('x') as f:
        f.write('''monitor.alsa.rules = [
 { matches = [ { device.name = "alsa_card.pci-0000_00_01.0" } ]
   actions = { update-props = { api.alsa.soft-mixer = true } } }
]
''')
    (tree/'etc/aetheri-wireplumber/wireplumber.conf.d/91-native-qemu-mixer.conf').chmod(0o644)
    image=output/'image';image.mkdir()
    for name in ('vmlinuz','cmdline.txt'):shutil.copyfile(output/'device/image'/name,image/name)
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
    sources={'native_builder_sha256':'tools/build_audio_control_hda.py','control_inputs_sha256':'spec/audio-control-inputs.json','control_preparer_sha256':'tools/prepare_audio_control.py','control_source_sha256':'src/audio/control.c','control_header_sha256':'src/audio/control.h','remote_source_sha256':'src/audio/remote.c','remote_header_sha256':'src/audio/remote.h','probe_source_sha256':'src/audio/control_probe.c','player_source_sha256':'src/audio/check.c','cmake_sha256':'CMakeLists.txt'}
    manifest={**base,'proof':'audio-control-hda','purpose':'Native library volume/mute on discovered virtual HDA; not normal audio admission','control_mixer':'qemu-software','control_case':test,'control_volume':.5 if test=='half' else 1.0,'control_muted':test=='muted','root':objects,**{key:audio.sha((audio.ROOT/name).read_bytes()) for key,name in sources.items()},'outputs':{n:audio.sha((image/n).read_bytes()) for n in ('vmlinuz','initramfs.cpio.gz','cmdline.txt')}}
    (image/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n');return manifest


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('output',type=Path);p.add_argument('nonce')
    for n in ('sdk','hardware','manager','headers'):p.add_argument(n,type=Path)
    p.add_argument('--case',choices=('unity','half','muted'),default='unity');a=p.parse_args();build(a.output,a.nonce,a.sdk,a.hardware,a.manager,a.headers,a.case)
