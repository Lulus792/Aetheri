"""Project a reviewed Sound GUI test runtime; never install to the host.

The caller independently selects the protected input snapshot and manifest.
Hashes and notice coverage are development input checks, not publisher trust,
app admission, a dependency updater or an exhaustive security/release review.
Only create a fresh tree; preserve partial output after a failed copy.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import stat

import desktop_payload


def relative(value):
    if not isinstance(value,str) or not re.fullmatch(r'[A-Za-z0-9_./+@-]+',value):
        raise ValueError('invalid input name')
    p=PurePosixPath(value)
    if not p.parts or p.is_absolute() or str(p)!=value or any(x in ('.','..') for x in p.parts):
        raise ValueError('noncanonical input name')
    return p


def profile(value):
    if not isinstance(value,dict) or type(value.get('format')) is not int or value['format']!=1 or value.get('purpose')!='settings-audio-lab':
        raise ValueError('different Sound lab profile')
    files=value.get('files');components=value.get('components')
    if not isinstance(files,list) or not 1<=len(files)<=256 or not isinstance(components,dict) or not 1<=len(components)<=64:
        raise ValueError('Sound lab inventory bound')
    selected={};sources=set();payload=[]
    for entry in files:
        if not isinstance(entry,dict):raise ValueError('invalid Sound lab entry')
        source=str(relative(entry.get('source')));destination=str(desktop_payload.destination(entry.get('destination')))
        if source in sources or destination in selected:raise ValueError('duplicate Sound lab input/destination')
        if entry.get('role') not in ('library','program','font','notice') or entry.get('component') not in components:
            raise ValueError('unattributed Sound lab entry')
        component=components[entry['component']]
        if not isinstance(component,dict) or entry.get('package') is not None or entry.get('provenance')!=component.get('origin'):
            raise ValueError('inconsistent Sound lab origin')
        executable=entry['role'] in ('library','program')
        if entry.get('mode')!=(0o755 if executable else 0o644):raise ValueError('different Sound lab file mode')
        converted=dict(entry,source='/reviewed-input/'+source)
        converted.pop('component');converted.pop('role')
        payload.append(converted);selected[destination]=entry;sources.add(source)
    for name,component in components.items():
        if not isinstance(name,str) or not re.fullmatch('[a-z0-9][a-z0-9_.-]{0,79}',name) or not isinstance(component,dict):
            raise ValueError('invalid Sound lab component')
        if not isinstance(component.get('origin'),str) or not component['origin'] or len(component['origin'])>1024:
            raise ValueError('missing Sound lab origin')
        notices=component.get('notices')
        if not isinstance(notices,list) or not 1<=len(notices)<=16 or len(set(notices))!=len(notices):
            raise ValueError('missing/duplicate Sound lab notices')
        if not any(e['component']==name and e['role']!='notice' for e in files):raise ValueError('unused Sound lab component')
        for path in notices:
            entry=selected.get(path)
            if entry is None or entry['component']!=name or entry['role']!='notice':raise ValueError('foreign/missing Sound lab notice')
        if set(notices)!={n for n,e in selected.items() if e['component']==name and e['role']=='notice'}:
            raise ValueError('unexplained Sound lab notice')
    desktop_payload.validate(dict(format=1,files=payload,symlinks={}))
    return value


def regular(path,entry):
    info=path.lstat()
    if not stat.S_ISREG(info.st_mode) or info.st_nlink!=1 or info.st_size!=entry['size']:
        raise ValueError('changed Sound lab file: '+str(path))
    if os.name=='posix' and stat.S_IMODE(info.st_mode)!=entry['mode']:
        raise ValueError('changed Sound lab mode: '+str(path))
    with path.open('rb') as stream:actual=hashlib.file_digest(stream,'sha256').hexdigest()
    if actual!=entry['sha256']:raise ValueError('changed Sound lab bytes: '+str(path))


def check(tree,value):
    profile(value);tree=Path(tree).absolute()
    if tree.is_symlink() or tree.resolve()!=tree or not tree.is_dir():raise ValueError('aliased Sound lab tree')
    expected=set()
    for entry in value['files']:
        name=entry['destination'].lstrip('/');path=tree/name
        current=tree
        for part in PurePosixPath(name).parts[:-1]:
            current=current/part
            if not stat.S_ISDIR(current.lstat().st_mode):raise ValueError('aliased Sound lab parent')
        regular(path,entry);expected.add(name)
        expected.update(str(p) for p in PurePosixPath(name).parents if str(p)!='.')
    actual={p.relative_to(tree).as_posix() for p in tree.rglob('*')}
    if actual!=expected:raise ValueError('unexplained Sound lab payload')
    return dict(files=len(value['files']),components=len(value['components']),bytes=sum(e['size'] for e in value['files']))


def prepare(inputs,tree,value):
    profile(value);inputs=Path(inputs).absolute();tree=Path(tree).absolute()
    if inputs.is_symlink() or inputs.resolve()!=inputs or not inputs.is_dir() or tree.resolve()!=tree:
        raise ValueError('aliased Sound lab input/output')
    if tree==inputs or tree in inputs.parents or inputs in tree.parents:raise ValueError('overlapping Sound lab input/output')
    payload=[]
    for entry in value['files']:
        source=inputs/str(relative(entry['source']));current=inputs
        for part in relative(entry['source']).parts[:-1]:
            current=current/part
            if not stat.S_ISDIR(current.lstat().st_mode):raise ValueError('aliased Sound lab input parent')
        item=dict(entry,source=str(source));item.pop('role');item.pop('component');payload.append(item)
    tree.mkdir(mode=0o700) # Existing/uncertain output is never adopted or overwritten.
    desktop_payload.install(tree,dict(format=1,files=payload,symlinks={}))
    return check(tree,value)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action',choices=('prepare','check'));parser.add_argument('manifest',type=Path)
    parser.add_argument('tree',type=Path);parser.add_argument('--inputs',type=Path)
    args=parser.parse_args();value=json.loads(args.manifest.read_text())
    if args.action=='prepare':
        if args.inputs is None:parser.error('prepare requires --inputs')
        result=prepare(args.inputs,args.tree,value)
    else:
        if args.inputs is not None:parser.error('check does not take --inputs')
        result=check(args.tree,value)
    print(json.dumps(result))


if __name__=='__main__':main()
