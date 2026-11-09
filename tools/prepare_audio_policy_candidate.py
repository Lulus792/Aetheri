"""Compose/check the explicit Lua55 policy candidate; normal RPM checks stay strict.

Trusted development inputs only. No installation, source adoption or app admission.
"""
import argparse
import json
import os
from pathlib import Path
import shutil
import stat

import prepare_audio as audio
import prepare_audio_policy as original
import build_audio_policy_lua as module

PROFILE=audio.ROOT/'spec/audio-policy-candidate-inputs.json'
MODULE='usr/lib64/wireplumber-0.5/libwireplumber-module-lua-scripting.so'
OLD_LUA={'usr/lib64/liblua-5.4.so','usr/share/licenses/lua-libs/mit.txt'}
SOURCE='usr/share/licenses/wireplumber-lua-candidate/source.tar.gz'


def lf_hash(path):return audio.sha(path.read_bytes().replace(b'\r\n',b'\n'))


def profile():
    pins=json.loads(PROFILE.read_bytes());base,_=original.profile();build=module.profile()
    if pins['format']!=1 or pins['id']!='wireplumber-0.5.19-lua55-candidate' or pins['base_profile_sha256']!=lf_hash(original.PROFILE) or pins['module_profile_sha256']!=lf_hash(module.PROFILE):raise ValueError('different candidate origin')
    if pins['module']['size']!=253072 or pins['module']['sha256']!='fd62b2a0697479170fc05ff7b2381c0227640c5608c342b0be7e2eb90d906f67':raise ValueError('different selected module')
    entries={e['path']:dict(e) for e in base['files']}
    entries.update({e['destination'].lstrip('/'):dict(path=e['destination'].lstrip('/'),size=e['size'],sha256=e['sha256'],mode=e['mode'],origin='existing-runtime') for e in base['existing_runtime']['files']})
    if not (OLD_LUA|{MODULE})<=entries.keys():raise ValueError('different replaced base boundary')
    for n in OLD_LUA:del entries[n]
    entries[MODULE]=dict(path=MODULE,**pins['module'],mode=0o755,origin='rebuilt-WirePlumber-source')
    for n in ('usr/lib64/liblua-5.5.so','usr/share/licenses/lua/COPYRIGHT'):
        if n in entries:raise ValueError('conflicting new Lua file')
        entries[n]=dict(path=n,**build['lua_files'][n],mode=0o755 if n.endswith('.so') else 0o644,origin='reviewed-Lua-source-build')
    if SOURCE in entries:raise ValueError('conflicting source notice')
    entries[SOURCE]=dict(path=SOURCE,size=build['source']['size'],sha256=build['source']['sha256'],mode=0o644,origin='original-WirePlumber-source-and-notices')
    links={**base['symlinks'],**{n.lstrip('/'):t for n,t in base['existing_runtime']['symlinks'].items()}}
    return pins,entries,links


def check(prefix):
    pins,entries,links=profile();prefix=Path(prefix).absolute()
    if prefix.resolve()!=prefix or prefix.is_symlink():raise ValueError('aliased candidate prefix')
    record=prefix/'inputs.json'
    if record.is_symlink() or not record.is_file():raise ValueError('missing candidate record')
    metadata=json.loads(record.read_bytes())
    if not isinstance(metadata,dict) or metadata.get('candidate_profile_sha256')!=lf_hash(PROFILE) or metadata.get('candidate_id')!=pins['id']:raise ValueError('different selected candidate profile')
    expected={'inputs.json'}
    for n,e in entries.items():
        p=prefix/n
        if p.is_symlink() or not p.is_file():raise ValueError('missing/aliased candidate member: '+n)
        module.check_file(p,e)
        if stat.S_IMODE(p.stat().st_mode)!=e['mode']:raise ValueError('changed candidate mode: '+n)
        expected.add(n)
    for n,target in links.items():
        p=prefix/n
        if not p.is_symlink() or os.readlink(p)!=target:raise ValueError('changed candidate alias: '+n)
        expected.add(n)
    actual={p.relative_to(prefix).as_posix() for p in prefix.rglob('*') if not p.is_dir() or p.is_symlink()}
    if actual!=expected:raise ValueError('unexplained/missing candidate member')
    return pins


def prepare(base,lua,built_module,archive,output):
    if os.name!='posix' or os.getuid()==0 or os.getuid()!=os.geteuid():raise ValueError('ordinary Linux preparation required')
    base=Path(base).absolute();lua=Path(lua).absolute();built_module=Path(built_module).absolute();archive=Path(archive).absolute();output=Path(output).absolute()
    for p in (base,lua,built_module,archive,output):
        if p.resolve()!=p:raise ValueError('aliased candidate input/output')
    original.check(base);pins,entries,links=profile()
    sources={MODULE:built_module,SOURCE:archive,**{n:lua/n for n in ('usr/lib64/liblua-5.5.so','usr/share/licenses/lua/COPYRIGHT')}}
    for n,p in sources.items():module.check_file(p,entries[n])
    # A fresh prefix keeps mixed rebuilt/RPM provenance distinct from the original.
    output.mkdir(mode=0o700)
    for n,e in entries.items():
        source=sources.get(n,base/n);target=output/n;target.parent.mkdir(parents=True,exist_ok=True)
        with source.open('rb') as src,target.open('xb') as dst:shutil.copyfileobj(src,dst)
        target.chmod(e['mode'])
    for n,target in links.items():(output/n).symlink_to(target)
    receipt=dict(candidate_id=pins['id'],candidate_profile_sha256=lf_hash(PROFILE),base_profile_sha256=pins['base_profile_sha256'],module_profile_sha256=pins['module_profile_sha256'],base_package_proofs=json.loads((base/'inputs.json').read_bytes()),scope=pins['purpose'])
    with (output/'inputs.json').open('x') as f:json.dump(receipt,f,indent=2)
    original.check(base)
    for n,p in sources.items():module.check_file(p,entries[n])
    check(output)
    return output


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);sub=p.add_subparsers(dest='action',required=True)
    a=sub.add_parser('check');a.add_argument('prefix',type=Path)
    a=sub.add_parser('prepare')
    for n in ('base','lua','built_module','archive','output'):a.add_argument(n,type=Path)
    a=p.parse_args();action=vars(a).pop('action');print(check(a.prefix) if action=='check' else prepare(**vars(a)))
