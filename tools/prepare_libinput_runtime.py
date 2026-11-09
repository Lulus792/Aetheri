"""Project the independently selected input runtime into a fresh private tree.

No package installation, udev reload or normal provider selection. Receipts never
authorize their own bytes. Counter/instrumented libraries are not runtime inputs.
"""
import argparse
import copy
import hashlib
import json
import os
from pathlib import Path,PurePosixPath
import re
import stat
import subprocess

import build_libinput
import desktop_payload
import prepare_settings_audio_lab as paths

ROOT=Path(__file__).resolve().parents[1]
PROFILE=ROOT/'spec/libinput-runtime-inputs.json'
BASE=ROOT/'spec/desktop-image-inputs.json'
LUA=ROOT/'spec/audio-policy-lua-inputs.json'


def lf_hash(path):return hashlib.sha256(Path(path).read_bytes().replace(b'\r\n',b'\n')).hexdigest()


def profile():
    value=json.loads(PROFILE.read_bytes())
    if value.get('format')!=1 or value.get('id')!='libinput-1.32.0-lua55-runtime':raise ValueError('different input runtime profile')
    for key,p in [('base_desktop_sha256',BASE),('libinput_build_profile_sha256',build_libinput.PROFILE),('lua_profile_sha256',LUA)]:
        if value.get(key)!=lf_hash(p):raise ValueError('different input runtime origin: '+key)
    if value.get('remove_packages')!=['libinput-1.31.3-1.fc44.x86_64','lua-libs-5.4.8-5.fc44.x86_64']:raise ValueError('different replaced providers')
    entries=value.get('files');components=value.get('components')
    if not isinstance(entries,list) or len(entries)!=63 or not isinstance(components,dict) or set(components)!={'libinput','lua'}:raise ValueError('different input runtime inventory')
    destinations=set();payload=[]
    for e in entries:
        if not isinstance(e,dict) or e.get('source') not in ('libinput_build','libinput_archive','lua_sdk') or e.get('component') not in components:raise ValueError('unattributed input runtime entry')
        name=str(paths.relative(e.get('path')));paths.relative(e.get('input'))
        if name in destinations:raise ValueError('duplicate input runtime entry')
        destinations.add(name);component=components[e['component']]
        if not isinstance(component.get('origin'),str) or not component['origin']:raise ValueError('missing input origin')
        payload.append(dict(source='/reviewed/'+e['source']+'/'+e['input'],destination='/'+name,size=e.get('size'),sha256=e.get('sha256'),mode=e.get('mode'),package=None,provenance=component['origin']))
    for name,c in components.items():
        notices=c.get('notices')
        if not isinstance(notices,list) or not notices or len(set(notices))!=len(notices):raise ValueError('missing input notices')
        for n in notices:
            if not any(e['path']==n and e['component']==name and e['mode']==0o644 for e in entries):raise ValueError('foreign input notice')
    expected_links={'usr/lib64/libinput.so.10':'libinput.so.10.13.0'}
    if value.get('symlinks')!=expected_links:raise ValueError('different input library aliases')
    desktop_payload.validate(dict(format=1,files=payload,symlinks={'/'+n:t for n,t in expected_links.items()}))
    if sum(e['path'].startswith('usr/share/libinput/') and e['path'].endswith('.quirks') for e in entries)!=54:raise ValueError('incomplete input data')
    if any('5.4' in e['path'] or e['source']=='libinput_build' and not e['input'].startswith('stage/usr/') for e in entries):raise ValueError('old ABI or non-staged input')
    return value


def regular(path,e):
    path=Path(path)
    if path.resolve()!=path:raise ValueError('aliased selected input: '+str(path))
    s=path.lstat()
    if not stat.S_ISREG(s.st_mode) or s.st_nlink!=1 or s.st_size!=e['size']:raise ValueError('different selected input: '+str(path))
    fd=os.open(path,os.O_RDONLY|getattr(os,'O_NOFOLLOW',0)|getattr(os,'O_CLOEXEC',0)|getattr(os,'O_BINARY',0))
    with os.fdopen(fd,'rb') as f:
        held=os.fstat(f.fileno())
        if (s.st_dev,s.st_ino)!=(held.st_dev,held.st_ino):raise ValueError('selected input replaced')
        raw=f.read(e['size']+1)
    if len(raw)!=e['size'] or hashlib.sha256(raw).hexdigest()!=e['sha256']:raise ValueError('different selected input bytes: '+str(path))
    return raw


def expected_names(value):
    names={'inputs.json',*value['symlinks'],*(e['path'] for e in value['files'])}
    return names|{str(parent) for n in names for parent in PurePosixPath(n).parents if str(parent)!='.'}


def check(prefix):
    value=profile();prefix=Path(prefix).absolute()
    if prefix.resolve()!=prefix or not stat.S_ISDIR(prefix.lstat().st_mode):raise ValueError('aliased input runtime root')
    actual={p.relative_to(prefix).as_posix() for p in prefix.rglob('*')}
    if actual!=expected_names(value):raise ValueError('unexplained/missing input runtime entry')
    for n in actual-set(value['symlinks']):
        p=prefix/n;s=p.lstat()
        if not (stat.S_ISDIR(s.st_mode) or stat.S_ISREG(s.st_mode)) or p.resolve()!=p:raise ValueError('aliased input runtime member')
    for e in value['files']:
        p=prefix/e['path'];regular(p,e)
        if os.name=='posix' and stat.S_IMODE(p.stat().st_mode)!=e['mode']:raise ValueError('different input runtime mode')
    for n,target in value['symlinks'].items():
        p=prefix/n
        if not p.is_symlink() or os.readlink(p)!=target:raise ValueError('different input runtime alias')
    record=prefix/'inputs.json'
    if record.is_symlink() or record.stat().st_nlink!=1:raise ValueError('aliased input runtime record')
    if not 0<record.stat().st_size<=16384:raise ValueError('input runtime record bound')
    receipt=json.loads(record.read_bytes())
    if receipt!=dict(id=value['id'],profile_sha256=lf_hash(PROFILE),components=value['components']):raise ValueError('different input runtime record')
    return value


def prepare(libinput_build,lua_sdk,archive,output):
    if os.name!='posix' or os.getuid()==0 or os.getuid()!=os.geteuid():raise ValueError('ordinary native input-runtime preparation required')
    value=profile();libinput_build,lua_sdk,archive,output=[Path(p).absolute() for p in (libinput_build,lua_sdk,archive,output)]
    for p in (libinput_build,lua_sdk,archive,output):
        if p.resolve()!=p:raise ValueError('aliased input runtime source/output')
    if any(output==p or output in p.parents or p in output.parents for p in (libinput_build,lua_sdk,archive)):raise ValueError('overlapping input runtime source/output')
    pins=json.loads(build_libinput.PROFILE.read_bytes());source=build_libinput.source_members(archive,pins);payload={}
    for e in value['files']:
        if e['source']=='libinput_archive':
            raw=source.get(e['input'])
            if raw is None or len(raw)!=e['size'] or hashlib.sha256(raw).hexdigest()!=e['sha256']:raise ValueError('different input source data/notice')
        else:raw=regular((libinput_build if e['source']=='libinput_build' else lua_sdk)/e['input'],e)
        payload[e['path']]=raw
    output.mkdir(mode=0o700) # Existing or uncertain output is never adopted.
    for e in value['files']:
        p=output/e['path'];p.parent.mkdir(parents=True,exist_ok=True)
        with p.open('xb') as f:f.write(payload[e['path']])
        p.chmod(e['mode'])
    for n,target in value['symlinks'].items():(output/n).symlink_to(target)
    with (output/'inputs.json').open('x') as f:json.dump(dict(id=value['id'],profile_sha256=lf_hash(PROFILE),components=value['components']),f,indent=2)
    check(output)
    return output


def desktop_manifest(prefix):
    """Explicit overlay only; never mutate the normal base or unrelated entries."""
    value=check(prefix);prefix=Path(prefix).absolute();base=json.loads(BASE.read_bytes());desktop_payload.validate(base)
    removed=[e for e in base['files'] if e.get('package') in value['remove_packages']]
    if len(removed)!=55:raise ValueError('different old desktop input boundary')
    result=copy.deepcopy(base);result['files']=[e for e in base['files'] if e not in removed]
    retained={e['destination'] for e in result['files']}
    for e in value['files']:
        if '/'+e['path'] in retained:raise ValueError('candidate conflicts with another desktop owner')
        result['files'].append(dict(source=str(prefix/e['path']),destination='/'+e['path'],size=e['size'],sha256=e['sha256'],mode=e['mode'],package=None,provenance=value['components'][e['component']]['origin']))
    for n,target in value['symlinks'].items():
        if '/'+n in result['symlinks'] and result['symlinks']['/'+n]!=target:raise ValueError('conflicting desktop input alias')
        result['symlinks']['/'+n]=target
    result['packages']=[n for n in result['packages'] if n not in value['remove_packages']]
    result['input_runtime']=dict(id=value['id'],profile_sha256=lf_hash(PROFILE),base_desktop_sha256=value['base_desktop_sha256'],components=value['components'])
    desktop_payload.validate(result)
    return result


def audit_elfs(tree,manifest):
    """Static candidate closure gate; never executes an image program."""
    if manifest.get('input_runtime',{}).get('profile_sha256')!=lf_hash(PROFILE):raise ValueError('unselected ELF audit profile')
    rows={}
    for e in manifest['files']:
        p=Path(tree)/e['destination'].lstrip('/')
        with p.open('rb') as f:magic=f.read(4)
        if magic!=b'\x7fELF':continue
        text=subprocess.check_output(['readelf','--dynamic','--wide',str(p)],text=True,stderr=subprocess.STDOUT)
        needed=re.findall(r'\(NEEDED\).*?\[([^\]]+)\]',text)
        if any(n.startswith('liblua-') and n!='liblua-5.5.so' for n in needed):raise ValueError('retained ELF needs old Lua ABI: '+e['destination'])
        rows[e['destination']]=needed
        if e['destination']=='/usr/lib64/libinput.so.10.13.0' and ('liblua-5.5.so' not in needed or '(RPATH)' in text or '(RUNPATH)' in text):raise ValueError('different candidate ELF linkage')
    if '/usr/lib64/libinput.so.10.13.0' not in rows or '/usr/lib64/liblua-5.5.so' not in rows:raise ValueError('candidate ELF missing')
    return dict(profile_sha256=lf_hash(PROFILE),needed=rows)


def main():
    p=argparse.ArgumentParser(description=__doc__);sub=p.add_subparsers(dest='action',required=True)
    a=sub.add_parser('check');a.add_argument('prefix',type=Path)
    a=sub.add_parser('prepare')
    for n in ('libinput_build','lua_sdk','archive','output'):a.add_argument(n,type=Path)
    a=sub.add_parser('manifest');a.add_argument('prefix',type=Path);a.add_argument('output',type=Path)
    args=p.parse_args();action=vars(args).pop('action')
    if action=='prepare':print(prepare(**vars(args)))
    elif action=='check':print(json.dumps(dict(id=check(args.prefix)['id'])))
    else:
        value=desktop_manifest(args.prefix)
        with args.output.open('x') as f:json.dump(value,f,indent=2)


if __name__=='__main__':main()
