"""Build a fresh diskless QEMU-HDA test root; never touch a physical device.

Extends a copied RAM-only systemd proof, preserving its original output. This
profile is not the desktop, an installed authority or production device policy.
"""
import argparse
import gzip
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

import build_boot_probe as base
import desktop_payload
import prepare_audio as audio
import prepare_audio_hda as hda


def build(output, nonce, sdk, hardware):
    if sys.platform != 'linux' or os.getuid()==0 or os.uname().machine!='x86_64':
        raise ValueError('ordinary native x86-64 Linux builder required')
    audio.check(sdk);hda.check(hardware);pins,pin_hash=hda.profile()
    output=Path(output).absolute()
    if output.is_symlink() or output.resolve()!=output:raise ValueError('aliased HDA output')
    output.mkdir(mode=0o700)
    base.build(output/'base',nonce)
    tree=output/'root';shutil.copytree(output/'base/root',tree,symlinks=True)
    logs=output/'logs';logs.mkdir()
    def run(name,args):
        p=subprocess.run(args,capture_output=True);(logs/(name+'.log')).write_bytes(p.stdout+p.stderr);p.check_returncode();return p.stdout
    def put(name,text,mode=0o644):
        p=tree/name;p.parent.mkdir(parents=True,exist_ok=True)
        with p.open('x') as f:f.write(text)
        p.chmod(mode)
    # The separately checked SDK stays immutable. Copy only runtime, notices
    # and the one fixture linker; headers and the FIFO sink are not in this root.
    sdk_pins,_=audio.profile();selected=[]
    for e in sdk_pins['files']:
        if e['role'] in ('runtime','notice') or e['path']=='usr/bin/pw-link':
            p=Path(sdk)/e['path'];target=tree/e['path'];target.parent.mkdir(parents=True,exist_ok=True)
            if target.exists() or target.is_symlink():raise ValueError('audio image collision')
            shutil.copyfile(p,target);target.chmod(e['mode']);selected.append(e['path'])
    for alias,target in sdk_pins['symlinks'].items():(tree/alias).symlink_to(target)
    for e in pins['files']:
        p=tree/e['path'];p.parent.mkdir(parents=True,exist_ok=True)
        if p.exists() or p.is_symlink():raise ValueError('HDA image collision')
        shutil.copyfile(Path(hardware)/e['path'],p);p.chmod(e['mode'])
    for alias,target in pins['symlinks'].items():(tree/alias).symlink_to(target)
    desktop_payload.install(tree,pins['base_runtime'])
    module_profile=dict(format=1,files=[{**e,'destination':'/'+e['destination']} for e in pins['kernel_modules']],symlinks={})
    desktop_payload.install(tree,module_profile)
    # kmod resolves its compiled /lib/modules prefix in this private root.
    (tree/'lib').symlink_to('usr/lib')
    (tree/'usr/bin/modprobe').symlink_to('kmod')
    run('depmod',['depmod','-C','/dev/null','-b',str(tree),'-a',pins['kernel_release']])
    compile=['gcc','-std=c17','-D_GNU_SOURCE','-O2','-Wall','-Wextra','-Wpedantic','-Wconversion','-Wshadow','-Werror']
    # Source defines _GNU_SOURCE itself, so do not supply it twice under Werror.
    compile.remove('-D_GNU_SOURCE')
    run('owner-build',[*compile,str(audio.ROOT/'src/boot/audio.c'),'-o',str(tree/'usr/libexec/aetheri-audio-owner')])
    build_dir=output/'apps'
    run('configure',['cmake','-S',str(audio.ROOT),'-B',str(build_dir),'-G','Ninja','-DCMAKE_BUILD_TYPE=Release','-DCMAKE_SKIP_RPATH=ON','-DAETHERI_AUDIO=ON','-DAETHERI_AUDIO_PREFIX='+str(sdk)])
    run('player-build',['cmake','--build',str(build_dir),'-j2','--target','aetheri-audio-check'])
    shutil.copyfile(build_dir/'aetheri-audio-check',tree/'usr/libexec/aetheri-audio-check');(tree/'usr/libexec/aetheri-audio-check').chmod(0o755)
    unit=tree/'etc/systemd/system/aetheri-boot.service';text=unit.read_text()
    if text.count('SuccessAction=poweroff-immediate\n')!=1:raise ValueError('different RAM preflight unit')
    unit.write_text(text.replace('SuccessAction=poweroff-immediate\n',''))
    put('etc/systemd/system/aetheri-audio.target','[Unit]\nDescription=Aetheri isolated HDA test\nDefaultDependencies=no\nRequires=aetheri-audio.service\nAfter=aetheri-audio.service\n')
    put('etc/systemd/system/aetheri-audio.service','[Unit]\nDescription=Fixed QEMU HDA owner\nDefaultDependencies=no\nRequires=aetheri-boot.service\nAfter=aetheri-boot.service\nSuccessAction=poweroff-immediate\nFailureAction=poweroff-immediate\n[Service]\nType=oneshot\nExecStart=/usr/libexec/aetheri-audio-owner\nNoNewPrivileges=yes\nCapabilityBoundingSet=CAP_SYS_MODULE CAP_CHOWN CAP_SETUID CAP_SETGID CAP_KILL CAP_DAC_OVERRIDE\nTimeoutStartSec=45\nStandardInput=null\nStandardOutput=tty\nStandardError=tty\nTTYPath=/dev/console\n')
    common='context.spa-libs = { support.* = support/libspa-support audio.convert.* = audioconvert/libspa-audioconvert api.alsa.* = alsa/libspa-alsa }\n'
    put('etc/aetheri-audio/client.conf',common+'context.properties = { mem.allow-mlock = false support.dbus = false }\ncontext.modules = [ { name = libpipewire-module-protocol-native } { name = libpipewire-module-client-node } { name = libpipewire-module-adapter } ]\n')
    put('etc/aetheri-audio/server.conf',common+'''context.properties = { core.daemon = true core.name = pw-hda default.clock.rate = 48000 default.clock.quantum = 1024 default.clock.min-quantum = 1024 default.clock.max-quantum = 1024 mem.allow-mlock = false support.dbus = false }
context.modules = [
 { name = libpipewire-module-protocol-native args = { sockets = [ { name = pw-hda mode = 0600 } ] } }
 { name = libpipewire-module-client-node }
 { name = libpipewire-module-adapter }
 { name = libpipewire-module-link-factory }
 { name = libpipewire-module-spa-node-factory }
 { name = libpipewire-module-access args = { access.legacy = false access.socket = { pw-hda = unrestricted } } }
]
context.objects = [ { factory = adapter args = {
 factory.name = api.alsa.pcm.sink node.name = aetheri.test.hda api.alsa.path = aetheri_hda
 node.always-process = true node.pause-on-idle = false node.suspend-on-idle = false
 api.alsa.use-ucm = false api.alsa.period-size = 1024 api.alsa.disable-batch = true
 audio.format = S16LE audio.rate = 48000 audio.channels = 2 audio.position = [ FL FR ]
 adapter.auto-port-config = { mode = dsp monitor = false position = preserve }
} } ]
''')
    put('etc/aetheri-audio/alsa.conf','pcm.aetheri_hda { type hw card 0 device 0 subdevice 0 }\nctl.hw { @args [ CARD ] @args.CARD { type string default "0" } type hw card $CARD }\n')
    image=output/'image';image.mkdir();shutil.copyfile(output/'base/vmlinuz',image/'vmlinuz')
    cmd=(output/'base/cmdline.txt').read_text().replace('systemd.unit=aetheri-boot.target','systemd.unit=aetheri-audio.target');(image/'cmdline.txt').write_text(cmd)
    paths=sorted(tree.rglob('*'),key=lambda p:p.relative_to(tree).as_posix())
    for p in [tree,*paths]:
        os.utime(p,(0,0),follow_symlinks=False)
    names=b'\0'.join(p.relative_to(tree).as_posix().encode() for p in paths)+b'\0'
    cp=subprocess.run(['cpio','--null','--create','--format=newc','--owner=0:0','--reproducible','--quiet'],input=names,cwd=tree,capture_output=True,check=True)
    (image/'initramfs.cpio.gz').write_bytes(gzip.compress(cp.stdout,mtime=0))
    objects={}
    for p in paths:
        n=p.relative_to(tree).as_posix()
        if p.is_symlink():objects[n]={'symlink':os.readlink(p)}
        elif p.is_file():objects[n]={'sha256':audio.sha(p.read_bytes()),'mode':oct(p.stat().st_mode&0o7777)}
    manifest=dict(format=1,proof='audio-hda',purpose=pins['purpose'],nonce=nonce,kernel_release=pins['kernel_release'],hda_inputs_sha256=pin_hash,audio_inputs_sha256=audio.sha(audio.PROFILE.read_bytes()),builder_sha256=audio.sha(Path(__file__).read_bytes()),owner_sha256=audio.sha((audio.ROOT/'src/boot/audio.c').read_bytes()),root=objects,outputs={n:audio.sha((image/n).read_bytes()) for n in ('vmlinuz','initramfs.cpio.gz','cmdline.txt')})
    (image/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    return manifest


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('output',type=Path);p.add_argument('nonce');p.add_argument('sdk',type=Path);p.add_argument('hardware',type=Path);a=p.parse_args();build(a.output,a.nonce,a.sdk,a.hardware)
