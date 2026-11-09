"""Build the reviewed private Lua candidate SDK; never install on the host.

No network, upstream makefile, package script or system-prefix mutation.
The caller selects the checked source archive and a fresh ordinary-user tree.
This is build evidence, not WirePlumber ABI/runtime or release acceptance.
"""
import argparse
import hashlib
import io
import json
import os
from pathlib import Path, PurePosixPath
import stat
import subprocess
import tarfile

from build_graphics import apply_reviewed_patch

ROOT=Path(__file__).resolve().parents[1]
PROFILE=ROOT/'spec/audio-lua-inputs.json'


def sha(raw):return hashlib.sha256(raw).hexdigest()


def profile():
    raw=PROFILE.read_bytes();pins=json.loads(raw)
    if pins['format']!=1 or pins['version']!='5.5.1' or pins['source']['commit']!='7579fc9d7ed90240487251dfb69168f8e64e9294' or pins['soname']!='liblua-5.5.so':
        raise ValueError('unreviewed Lua candidate')
    if set(pins['patch']['files'])!={'lcode.c','lcorolib.c','lgc.c','lobject.c','lstate.c','lvm.c'}:
        raise ValueError('different Lua patch boundary')
    return pins,sha(raw)


def source_members(archive,pins):
    path=Path(archive);info=path.lstat();source=pins['source']
    if not stat.S_ISREG(info.st_mode) or info.st_size!=source['size'] or info.st_size>2*1024**2:
        raise ValueError('different Lua source object/size')
    with path.open('rb') as f:raw=f.read(2*1024**2+1)
    if len(raw)!=source['size'] or sha(raw)!=source['sha256']:raise ValueError('different Lua source bytes')
    result={};total=0
    with tarfile.open(fileobj=io.BytesIO(raw),mode='r:gz') as stream:
        members=stream.getmembers()
        if not 1<=len(members)<=256:raise ValueError('Lua source member bound')
        seen=set()
        for member in members:
            p=PurePosixPath(member.name)
            if p.is_absolute() or '..' in p.parts or not p.parts or str(p)!=member.name.rstrip('/') or p.parts[0]!=source['root'] or member.name in seen or not (member.isdir() or member.isfile()):
                raise ValueError('unsafe Lua source member')
            seen.add(member.name)
            if member.isdir():continue
            name='/'.join(p.parts[1:]);total+=member.size
            if not name or member.size>1024**2 or total>8*1024**2:raise ValueError('Lua source byte bound')
            result[name]=stream.extractfile(member).read()
    return result


def build(archive,output,sanitizers=False):
    if os.name!='posix' or os.getuid()==0 or os.getuid()!=os.geteuid() or os.uname().machine!='x86_64':
        raise ValueError('ordinary native x86-64 Linux builder required')
    pins,digest=profile();members=source_members(archive,pins);output=Path(output).absolute()
    if output.resolve()!=output:raise ValueError('aliased Lua output')
    # Archive checks and the patch digest precede output effects. Existing/partial
    # output is never adopted or removed, including after a failed compiler.
    patch=ROOT/pins['patch']['path']
    if sha(patch.read_bytes())!=pins['patch']['sha256']:raise ValueError('changed Lua patch')
    output.mkdir(mode=0o700);source=output/'source';source.mkdir();logs=output/'logs';logs.mkdir()
    for name,raw in members.items():
        path=source/name;path.parent.mkdir(parents=True,exist_ok=True)
        with path.open('xb') as f:f.write(raw)
        path.chmod(0o644)
    apply_reviewed_patch(source,pins['patch'])
    expected=sorted(name for name in members if '/' not in name and name.endswith('.c') and name not in ('lua.c','ltests.c','onelua.c'))
    if len(expected)!=32:raise ValueError('different Lua compilation inventory')
    prefix=output/'prefix';lib=prefix/'usr/lib64';include=prefix/'usr/include/lua5.5';lib.mkdir(parents=True);include.mkdir(parents=True)
    env={**os.environ,'LC_ALL':'C','SOURCE_DATE_EPOCH':'0'}
    flags=['-std=c17','-O2','-fPIC','-fstack-protector-strong','-D_FORTIFY_SOURCE=3','-DLUA_USE_LINUX','-Wall','-Wextra']
    if sanitizers:flags+=['-O1','-fsanitize=address,undefined','-fno-omit-frame-pointer','-DLUAI_ASSERT']
    commands=[]
    def run(name,args):
        commands.append(args);p=subprocess.run(args,cwd=source,env=env,capture_output=True)
        (logs/(name+'.log')).write_bytes(p.stdout+p.stderr)
        if p.returncode:raise RuntimeError('Lua build failed: '+name+'; inspect '+str(logs))
        return p.stdout
    compiler=run('compiler',['gcc','--version']).decode()
    target_triple=run('compiler-target',['gcc','-dumpmachine']).decode().strip()
    objects=[]
    for name in expected:
        obj=output/(name[:-2]+'.o');run(name,['gcc',*flags,'-c',name,'-o',str(obj)]);objects.append(str(obj))
    target=lib/pins['soname']
    run('link',['gcc',*flags,'-shared','-Wl,-z,relro,-z,now,--no-undefined,-soname,'+pins['soname'],*objects,'-lm','-ldl','-o',str(target)])
    headers=('lua.h','luaconf.h','lauxlib.h','lualib.h')
    for name in headers:(include/name).write_bytes((source/name).read_bytes())
    # Keep the exact original copyright/permission block, not an Aetheri notice.
    header=(source/'lua.h').read_bytes();notice=header[header.index(b'/******************************************************************************'):header.rindex(b'******************************************************************************/')+len(b'******************************************************************************/')]
    license_path=prefix/'usr/share/licenses/lua/COPYRIGHT';license_path.parent.mkdir(parents=True);license_path.write_bytes(notice+b'\n')
    pc=lib/'pkgconfig';pc.mkdir()
    (pc/'lua5.5.pc').write_text('prefix='+str(prefix/'usr')+'\nlibdir=${prefix}/lib64\nincludedir=${prefix}/include/lua5.5\n\nName: Lua\nDescription: Private reviewed Lua candidate\nVersion: 5.5.1\nLibs: -L${libdir} -llua-5.5\nLibs.private: -lm -ldl\nCflags: -I${includedir}\n')
    # The CLI is a build/test tool only, outside the SDK runtime prefix.
    run('test-cli',['gcc',*flags,'lua.c','-L'+str(lib),'-Wl,-rpath,$ORIGIN/prefix/usr/lib64','-llua-5.5','-lm','-ldl','-o',str(output/'lua-test')])
    run('elf',['readelf','--dynamic','--wide',str(target)])
    record=dict(format=1,profile_sha256=digest,source_commit=pins['source']['commit'],compiler=compiler,target_triple=target_triple,commands=commands,sanitizers=sanitizers,files={p.relative_to(prefix).as_posix():dict(size=p.stat().st_size,sha256=sha(p.read_bytes())) for p in prefix.rglob('*') if p.is_file()},scope=pins['purpose'])
    (output/'inputs.json').write_text(json.dumps(record,indent=2)+'\n')
    return record


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('archive',type=Path);parser.add_argument('output',type=Path);parser.add_argument('--sanitizers',action='store_true');args=parser.parse_args()
    print(json.dumps(build(args.archive,args.output,args.sanitizers)))
