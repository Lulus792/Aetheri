"""Read the separate, exact HDA library inputs as data; no installation."""
import argparse
import contextlib
import hashlib
import json
import os
from pathlib import Path
import stat
import subprocess
import sys

import prepare_audio as audio

PROFILE = audio.ROOT / 'spec/audio-hda-inputs.json'


def profile():
    raw = PROFILE.read_bytes();pins = json.loads(raw)
    if pins['format'] != 1 or pins['kernel_release'] != '6.19.10-300.fc44.x86_64' or pins['signer'] != '36f612dcf27f7d1a48a835e4dbfcf71c6d9f90a6':
        raise ValueError('unreviewed HDA profile')
    names = {p['name'] for p in pins['packages']}
    if names != {'alsa-lib-1.2.16.1-1.fc44.x86_64.rpm','pipewire-libs-1.6.11-1.fc44.x86_64.rpm'} or len(pins['packages']) != 2:
        raise ValueError('different HDA package selection')
    selected={'usr/lib64/libasound.so.2.0.0','usr/share/licenses/alsa-lib/COPYING','usr/lib64/spa-0.2/alsa/libspa-alsa.so'}
    if len(pins['files']) != 3 or {e['path'] for e in pins['files']}!=selected or pins['symlinks'] != {'usr/lib64/libasound.so.2':'libasound.so.2.0.0'}:
        raise ValueError('different HDA payload selection')
    for entry in pins['files']:
        audio.canonical(entry['path'])
        if entry['package'] not in names or entry['mode'] not in (0o644,0o755) or not 0 < entry['size'] < 4*1024**2 or len(entry['sha256']) != 64:
            raise ValueError('invalid HDA payload')
    return pins, hashlib.sha256(raw).hexdigest()


def check(prefix):
    pins,digest = profile();prefix = Path(prefix).absolute()
    if prefix.is_symlink() or prefix.resolve() != prefix:
        raise ValueError('aliased HDA prefix')
    record = prefix/'inputs.json'
    if record.is_symlink() or json.loads(record.read_text())['profile_sha256'] != digest:
        raise ValueError('different HDA prefix profile')
    expected = {'inputs.json'}
    for e in pins['files']:
        p = prefix/e['path']
        if p.is_symlink() or not p.is_file() or p.stat().st_size != e['size'] or stat.S_IMODE(p.stat().st_mode) != e['mode'] or audio.sha(p.read_bytes()) != e['sha256']:
            raise ValueError('changed HDA payload')
        expected.add(e['path'])
    for n,target in pins['symlinks'].items():
        p=prefix/n
        if not p.is_symlink() or os.readlink(p) != target:raise ValueError('changed HDA alias')
        expected.add(n)
    if expected != {p.relative_to(prefix).as_posix() for p in prefix.rglob('*') if p.is_file() or p.is_symlink()}:
        raise ValueError('unexpected HDA prefix members')


def prepare(cache,prefix):
    if sys.platform != 'linux' or os.getuid() == 0 or os.getuid() != os.geteuid() or os.getgid() != os.getegid():
        raise ValueError('use ordinary Linux build account')
    pins,digest=profile();prefix=Path(prefix).absolute()
    if prefix.is_symlink() or prefix.resolve()!=prefix:raise ValueError('aliased HDA destination')
    proofs=[]
    with contextlib.ExitStack() as stack:
        packages=[]
        for entry in pins['packages']:
            fd=stack.enter_context(audio.sealed_package(Path(cache)/entry['name'],entry));path='/proc/self/fd/'+str(fd)
            proof=subprocess.check_output(['rpmkeys','--checksig','--verbose',path],pass_fds=(fd,),text=True)
            if pins['signer']+': ok' not in proof.lower() or 'payload sha256 digest: ok' not in proof.lower():raise ValueError('unverified HDA RPM')
            proofs.append(dict(name=entry['name'],signature=proof));packages.append((entry,fd,path))
        prefix.mkdir(mode=0o700)
        for entry,fd,path in packages:
            wanted={e['path']:e for e in pins['files'] if e['package']==entry['name']};seen=set()
            archive=subprocess.check_output(['rpm2cpio',path],pass_fds=(fd,))
            for name,mode,data in audio.cpio_members(archive):
                if name not in wanted:continue
                e=wanted[name]
                if not stat.S_ISREG(mode) or len(data)!=e['size'] or audio.sha(data)!=e['sha256']:raise ValueError('different HDA payload bytes')
                p=prefix/name;p.parent.mkdir(parents=True,exist_ok=True)
                with p.open('xb') as f:f.write(data)
                p.chmod(e['mode']);seen.add(name)
            if seen!=wanted.keys():raise ValueError('missing HDA payload')
    for name,target in pins['symlinks'].items():(prefix/name).symlink_to(target)
    (prefix/'inputs.json').write_text(json.dumps(dict(profile_sha256=digest,signatures=proofs),indent=2)+'\n')
    check(prefix)


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('action',choices=('prepare','check'));p.add_argument('prefix',type=Path);p.add_argument('--cache',type=Path);a=p.parse_args()
    if a.action=='prepare' and a.cache is None:p.error('prepare needs --cache')
    if a.action=='prepare':prepare(a.cache,a.prefix)
    else:check(a.prefix)
