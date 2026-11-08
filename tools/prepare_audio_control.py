"""Prepare the exact audio control SDK, without installing packages."""
import argparse
import contextlib
import json
import os
from pathlib import Path
import re
import stat
import subprocess

import prepare_audio as audio
import desktop_payload

PROFILE=audio.ROOT/'spec/audio-control-inputs.json'


def profile():
    raw=PROFILE.read_bytes();p=json.loads(raw)
    if p['format']!=1 or p['wireplumber_version']!='0.5.19' or p['glib_version']!='2.88.0' or p['signer']!='36f612dcf27f7d1a48a835e4dbfcf71c6d9f90a6':raise ValueError('unreviewed control SDK')
    names={'wireplumber-devel-0.5.19-1.fc44.x86_64.rpm','glib2-devel-2.88.0-1.fc44.x86_64.rpm'}
    if len(p['packages'])!=2 or {e['name'] for e in p['packages']}!=names:raise ValueError('different SDK packages')
    for e in p['packages']:
        if type(e['size']) is not int or not 0<e['size']<=4*1024**2 or not re.fullmatch('[0-9a-f]{64}',e['sha256']):raise ValueError('invalid package pin')
    seen=set()
    if not 1<=len(p['files'])<=512:raise ValueError('SDK member count')
    for e in p['files']:
        n=e['path'];audio.canonical(n)
        if n in seen or e['package'] not in names or e['mode']!=0o644 or type(e['size']) is not int or not 0<e['size']<=1024**2 or not re.fullmatch('[0-9a-f]{64}',e['sha256']):raise ValueError('invalid header pin')
        if not (n.startswith(('usr/include/','usr/share/licenses/')) or n=='usr/lib64/glib-2.0/include/glibconfig.h') or e['role']!=('notice' if '/licenses/' in n else 'header'):raise ValueError('not a header/notice')
        seen.add(n)
    if sum(e['size'] for e in p['files'])>4*1024**2:raise ValueError('SDK byte budget')
    desktop_payload.validate(p['existing_runtime'])
    return p,audio.sha(raw)


def check(prefix):
    p,digest=profile();prefix=Path(prefix).absolute()
    if prefix.is_symlink() or prefix.resolve()!=prefix:raise ValueError('aliased SDK prefix')
    record=prefix/'inputs.json'
    if record.is_symlink() or json.loads(record.read_text())['profile_sha256']!=digest:raise ValueError('different SDK profile')
    expected={'inputs.json'}
    entries=p['files']+[dict(path=e['destination'].lstrip('/'),size=e['size'],sha256=e['sha256'],mode=e['mode']) for e in p['existing_runtime']['files']]
    for e in entries:
        f=prefix/e['path']
        if f.is_symlink() or not f.is_file() or f.stat().st_size!=e['size'] or stat.S_IMODE(f.stat().st_mode)!=e['mode'] or audio.sha(f.read_bytes())!=e['sha256']:raise ValueError('changed SDK object: '+e['path'])
        expected.add(e['path'])
    for n,target in p['existing_runtime']['symlinks'].items():
        n=n.lstrip('/');f=prefix/n
        if not f.is_symlink() or os.readlink(f)!=target:raise ValueError('changed SDK runtime alias')
        expected.add(n)
    if expected!={f.relative_to(prefix).as_posix() for f in prefix.rglob('*') if f.is_file() or f.is_symlink()}:raise ValueError('unexplained SDK object')
    return p


def prepare(cache,prefix):
    if os.name!='posix' or os.uname().sysname!='Linux' or os.getuid()==0 or os.getuid()!=os.geteuid() or os.getgid()!=os.getegid():raise ValueError('ordinary native Linux builder required')
    p,digest=profile();prefix=Path(prefix).absolute()
    if prefix.is_symlink() or prefix.resolve()!=prefix:raise ValueError('aliased SDK destination')
    proofs=[]
    with contextlib.ExitStack() as stack:
        held=[]
        for e in p['packages']:
            fd=stack.enter_context(audio.sealed_package(Path(cache)/e['name'],e));path='/proc/self/fd/'+str(fd)
            proof=subprocess.check_output(['rpmkeys','--checksig','--verbose',path],pass_fds=(fd,),text=True)
            if p['signer']+': ok' not in proof.lower() or 'payload sha256 digest: ok' not in proof.lower():raise ValueError('unverified SDK RPM')
            held.append((e,fd,path));proofs.append(dict(name=e['name'],signature=proof))
        prefix.mkdir(mode=0o700)
        for package,fd,path in held:
            wanted={e['path']:e for e in p['files'] if e['package']==package['name']};seen=set()
            for name,mode,data in audio.cpio_members(subprocess.check_output(['rpm2cpio',path],pass_fds=(fd,))):
                if name not in wanted:continue
                e=wanted[name]
                if not stat.S_ISREG(mode) or len(data)!=e['size'] or audio.sha(data)!=e['sha256']:raise ValueError('different SDK member')
                f=prefix/name;f.parent.mkdir(parents=True,exist_ok=True)
                with f.open('xb') as stream:stream.write(data)
                f.chmod(e['mode']);seen.add(name)
            if seen!=wanted.keys():raise ValueError('missing SDK member')
    desktop_payload.install(prefix,p['existing_runtime'])
    with (prefix/'inputs.json').open('x') as f:json.dump(dict(profile_sha256=digest,signatures=proofs),f,indent=2)
    return check(prefix)


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('action',choices=('prepare','check'));p.add_argument('prefix',type=Path);p.add_argument('--cache',type=Path);a=p.parse_args()
    if a.action=='prepare' and a.cache is None:p.error('prepare needs --cache')
    prepare(a.cache,a.prefix) if a.action=='prepare' else check(a.prefix)
