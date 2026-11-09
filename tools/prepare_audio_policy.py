"""Prepare the exact private WirePlumber policy profile as data, never install."""
import argparse
import contextlib
import json
import os
from pathlib import Path
import re
import stat
import subprocess
import sys

import desktop_payload
import prepare_audio as audio

PROFILE=audio.ROOT/'spec/audio-policy-inputs.json'


def profile():
    raw=PROFILE.read_bytes();pins=json.loads(raw)
    if pins['format']!=1 or pins['version']!='0.5.19' or pins['signer']!='36f612dcf27f7d1a48a835e4dbfcf71c6d9f90a6':raise ValueError('unreviewed policy profile')
    expected={'wireplumber-0.5.19-1.fc44.x86_64.rpm','wireplumber-libs-0.5.19-1.fc44.x86_64.rpm','pipewire-libs-1.6.11-1.fc44.x86_64.rpm','alsa-lib-1.2.16.1-1.fc44.x86_64.rpm'}
    if len(pins['packages'])!=4 or {p['name'] for p in pins['packages']}!=expected:raise ValueError('different policy package family')
    for p in pins['packages']:
        if type(p['size']) is not int or not 0<p['size']<=16*1024**2 or not re.fullmatch('[0-9a-f]{64}',p['sha256']):raise ValueError('invalid policy package pin')
    if not 1<=len(pins['files'])<=512:raise ValueError('policy inventory bound')
    seen=set()
    for e in pins['files']:
        audio.canonical(e['path'])
        if e['path'] in seen or e['package'] not in expected or e['mode'] not in (0o644,0o755) or type(e['size']) is not int or not 0<e['size']<=4*1024**2 or not re.fullmatch('[0-9a-f]{64}',e['sha256']) or e['role'] not in ('runtime','notice','test','device-data'):raise ValueError('invalid policy payload')
        seen.add(e['path'])
    if sum(e['size'] for e in pins['files'])>16*1024**2 or pins['symlinks']!={'usr/lib64/libwireplumber-0.5.so.0':'libwireplumber-0.5.so.0.519.0'}:raise ValueError('policy budget/alias changed')
    desktop_payload.validate(pins['existing_runtime'])
    runtime={e['destination'].lstrip('/') for e in pins['existing_runtime']['files']}|{n.lstrip('/') for n in pins['existing_runtime']['symlinks']}
    if seen&runtime or seen&pins['symlinks'].keys():raise ValueError('conflicting policy inventory')
    return pins,audio.sha(raw)


def check(prefix):
    pins,digest=profile();prefix=Path(prefix).absolute()
    if prefix.is_symlink() or prefix.resolve()!=prefix:raise ValueError('aliased policy prefix')
    record=prefix/'inputs.json'
    if record.is_symlink():raise ValueError('different policy prefix profile')
    metadata=json.loads(record.read_text())
    if not isinstance(metadata,dict) or metadata.get('profile_sha256')!=digest:raise ValueError('different policy prefix profile')
    entries=pins['files']+[dict(path=e['destination'].lstrip('/'),size=e['size'],sha256=e['sha256'],mode=e['mode']) for e in pins['existing_runtime']['files']]
    expected={'inputs.json'}
    for e in entries:
        p=prefix/e['path']
        if p.is_symlink() or not p.is_file() or p.stat().st_size!=e['size'] or stat.S_IMODE(p.stat().st_mode)!=e['mode'] or audio.sha(p.read_bytes())!=e['sha256']:raise ValueError('changed policy payload: '+e['path'])
        expected.add(e['path'])
    for n,target in {**pins['symlinks'],**{n.lstrip('/'):t for n,t in pins['existing_runtime']['symlinks'].items()}}.items():
        p=prefix/n
        if not p.is_symlink() or os.readlink(p)!=target:raise ValueError('changed policy alias')
        expected.add(n)
    if expected!={p.relative_to(prefix).as_posix() for p in prefix.rglob('*') if p.is_file() or p.is_symlink()}:raise ValueError('unexpected policy prefix member')
    return pins


def prepare(cache,prefix):
    if sys.platform!='linux' or os.getuid()==0 or os.getuid()!=os.geteuid() or os.getgid()!=os.getegid():raise ValueError('ordinary native Linux build account required')
    pins,digest=profile();prefix=Path(prefix).absolute()
    if prefix.is_symlink() or prefix.resolve()!=prefix:raise ValueError('aliased policy destination')
    proofs=[]
    with contextlib.ExitStack() as stack:
        held=[]
        for e in pins['packages']:
            fd=stack.enter_context(audio.sealed_package(Path(cache)/e['name'],e));path='/proc/self/fd/'+str(fd)
            proof=subprocess.check_output(['rpmkeys','--checksig','--verbose',path],pass_fds=(fd,),text=True)
            if pins['signer']+': ok' not in proof.lower() or 'payload sha256 digest: ok' not in proof.lower():raise ValueError('unverified policy RPM')
            held.append((e,fd,path));proofs.append(dict(name=e['name'],signature=proof))
        prefix.mkdir(mode=0o700)
        for package,fd,path in held:
            wanted={e['path']:e for e in pins['files'] if e['package']==package['name']};seen=set()
            for name,mode,data in audio.cpio_members(subprocess.check_output(['rpm2cpio',path],pass_fds=(fd,))):
                if name not in wanted:continue
                e=wanted[name]
                if not stat.S_ISREG(mode) or len(data)!=e['size'] or audio.sha(data)!=e['sha256']:raise ValueError('changed policy RPM member')
                p=prefix/name;p.parent.mkdir(parents=True,exist_ok=True)
                with p.open('xb') as stream:stream.write(data)
                p.chmod(e['mode']);seen.add(name)
            if seen!=wanted.keys():raise ValueError('missing policy member')
    desktop_payload.install(prefix,pins['existing_runtime'])
    for n,target in pins['symlinks'].items():(prefix/n).symlink_to(target)
    with (prefix/'inputs.json').open('x') as record:json.dump(dict(profile_sha256=digest,signatures=proofs),record,indent=2)
    return check(prefix)


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('action',choices=('prepare','check'));p.add_argument('prefix',type=Path);p.add_argument('--cache',type=Path);a=p.parse_args()
    if a.action=='prepare' and a.cache is None:p.error('prepare needs --cache')
    if a.action=='prepare':prepare(a.cache,a.prefix)
    else:check(a.prefix)
