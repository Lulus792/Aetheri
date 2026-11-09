"""Build only upstream WirePlumber's Lua module against the reviewed 5.5 SDK.

No global install, source patch, network or package script. The existing RPM core
and ordinary profiles stay unchanged. Results need separate live acceptance.
"""
import argparse
import io
import json
import os
import re
from pathlib import Path, PurePosixPath
import stat
import subprocess
import tarfile

import prepare_audio as audio
import prepare_audio_control as control
import prepare_audio_policy as policy

ROOT=Path(__file__).resolve().parents[1]
PROFILE=ROOT/'spec/audio-policy-lua-inputs.json'


def profile():
    pins=json.loads(PROFILE.read_bytes())
    if pins['format']!=1 or pins['version']!='0.5.19' or pins['lua_version']!='5.5.1' or pins['source']['commit']!='4ab992fa2931ae2b17ef73508b1370030b7878c3':
        raise ValueError('different build profile')
    if pins['source']['root']!='wireplumber-0.5.19' or not 0<pins['source']['size']<=2*1024**2 or not re.fullmatch('[0-9a-f]{64}',pins['source']['sha256']):raise ValueError('invalid source pin')
    if pins['omitted_source_links']!={'CLAUDE.md':'AGENTS.md','GEMINI.md':'AGENTS.md','tests/wplua/settings':'../wp/settings'}:raise ValueError('different omitted aliases')
    expected={'usr/include/lua5.5/'+n for n in ('lua.h','luaconf.h','lauxlib.h','lualib.h')}|{'usr/lib64/liblua-5.5.so','usr/share/licenses/lua/COPYRIGHT'}
    if set(pins['lua_files'])!=expected:raise ValueError('different Lua SDK inventory')
    for e in [*pins['lua_files'].values(),pins['resource_tool']]:
        if type(e['size']) is not int or not 0<e['size']<=1024**2 or not re.fullmatch('[0-9a-f]{64}',e['sha256']):raise ValueError('invalid build input pin')
    return pins


def source_members(archive,pins):
    p=Path(archive);s=p.lstat();expected=pins['source']
    if not stat.S_ISREG(s.st_mode) or s.st_size!=expected['size']:
        raise ValueError('different WirePlumber archive object/size')
    raw=p.read_bytes()
    if audio.sha(raw)!=expected['sha256']:raise ValueError('changed WirePlumber archive')
    found={};seen=set();total=0
    with tarfile.open(fileobj=io.BytesIO(raw)) as a:
        members=a.getmembers()
        if not 1<=len(members)<=2048:raise ValueError('source member bound')
        for m in members:
            name=PurePosixPath(m.name)
            if name.is_absolute() or '..' in name.parts or not name.parts or name.parts[0]!=expected['root'] or str(name)!=m.name.rstrip('/') or m.name in seen:
                raise ValueError('unsafe WirePlumber member')
            seen.add(m.name)
            if m.issym() and pins['omitted_source_links'].get('/'.join(name.parts[1:]))==m.linkname:
                continue  # Reviewed source aliases are never extracted or followed.
            if not (m.isfile() or m.isdir()):raise ValueError('unsafe WirePlumber type')
            if m.isdir():continue
            total+=m.size
            if m.size>2*1024**2 or total>16*1024**2:raise ValueError('source byte bound')
            found['/'.join(name.parts[1:])]=a.extractfile(m).read()
    return found


def check_file(path,e):
    s=path.lstat()
    if not stat.S_ISREG(s.st_mode) or s.st_size!=e['size'] or audio.sha(path.read_bytes())!=e['sha256']:
        raise ValueError('changed selected input: '+str(path))


def build(archive,sdk,headers,old_policy,lua,resource_tool,output):
    if os.name!='posix' or os.getuid()==0 or os.getuid()!=os.geteuid() or os.uname().machine!='x86_64':
        raise ValueError('ordinary native x86-64 Linux builder required')
    pins=profile()
    sdk=Path(sdk).absolute();headers=Path(headers).absolute();old_policy=Path(old_policy).absolute();lua=Path(lua).absolute();resource_tool=Path(resource_tool).absolute();output=Path(output).absolute()
    for p in (sdk,headers,old_policy,lua,output,resource_tool):
        if p.resolve()!=p:raise ValueError('aliased build path')
    audio.check(sdk);control.check(headers);policy.check(old_policy)
    for name,e in pins['lua_files'].items():check_file(lua/name,e)
    check_file(resource_tool,pins['resource_tool'])
    members=source_members(archive,pins)
    output.mkdir(mode=0o700);source=output/'source';source.mkdir();logs=output/'logs';logs.mkdir()
    for name,raw in members.items():
        p=source/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(raw);p.chmod(0o644)
    module=source/'modules/module-lua-scripting'
    includes=[headers/'usr/include/wireplumber-0.5',headers/'usr/include/glib-2.0',headers/'usr/lib64/glib-2.0/include',sdk/'usr/include/pipewire-0.3',sdk/'usr/include/spa-0.2',lua/'usr/include/lua5.5',module]
    flags=['-std=gnu17','-O2','-fPIC','-fstack-protector-strong','-D_FORTIFY_SOURCE=3','-D_GNU_SOURCE','-DG_LOG_USE_STRUCTURED','-DWP_USE_LOCAL_LOG_TOPIC_IN_G_LOG','-DGETTEXT_PACKAGE="wireplumber"','-DGLIB_VERSION_MIN_REQUIRED=GLIB_VERSION_2_68','-DGLIB_VERSION_MAX_ALLOWED=GLIB_VERSION_2_68','-DSPA_AUDIO_MAX_CHANNELS=128u','-Wall','-Wextra','-Wno-unused-parameter',*['-I'+str(p) for p in includes]]
    libs=[str(old_policy/'usr/lib64/libwireplumber-0.5.so.0.519.0'),str(old_policy/'usr/lib64/libgio-2.0.so.0.8800.0'),str(old_policy/'usr/lib64/libgobject-2.0.so.0.8800.0'),str(headers/'usr/lib64/libglib-2.0.so.0.8800.0'),str(sdk/'usr/lib64/libpipewire-0.3.so.0.1611.0'),str(lua/'usr/lib64/liblua-5.5.so'),'-lm','-ldl','-pthread']
    env={**os.environ,'LC_ALL':'C','SOURCE_DATE_EPOCH':'0','LD_LIBRARY_PATH':':'.join(str(p/'usr/lib64') for p in (lua,headers,old_policy,sdk))}
    commands=[]
    def run(label,args):
        commands.append(args);p=subprocess.run(args,env=env,cwd=source,capture_output=True,timeout=120)
        (logs/(label+'.stdout')).write_bytes(p.stdout);(logs/(label+'.stderr')).write_bytes(p.stderr)
        if p.returncode:raise RuntimeError(label+' failed; inspect '+str(logs))
        return p.stdout
    compiler=run('compiler',['gcc','--version']).decode();triple=run('target',['gcc','-dumpmachine']).decode().strip()
    generated=[]
    for name,cname,manual in [('wplua','_wplua',True),('api','_m_lua_scripting_resources',False)]:
        target=output/(name+'-resources.c')
        run(name+'-resource',[str(resource_tool),str(module/name/'gresource.xml'),'--sourcedir='+str(module/name),'--generate-source','--c-name='+cname,'--target='+str(target),*(['--manual-register'] if manual else [])]);generated.append(target)
    # Exact upstream modules/meson.build + wplua/meson.build compilation boundary.
    wplua=[module/'wplua'/n for n in ('boxed.c','closure.c','object.c','userdata.c','value.c','wplua.c')]+generated[:1]
    api=[module/n for n in ('module.c','script.c','api/pod.c','api/json.c','api/api.c')]+generated[1:]
    objects=[]
    for i,p in enumerate(wplua+api):
        obj=output/(str(i)+'.o');run('compile-'+str(i),['gcc',*flags,'-fvisibility=hidden','-c',str(p),'-o',str(obj)]);objects.append(str(obj))
    target=output/'libwireplumber-module-lua-scripting.so'
    run('link',['gcc','-shared','-Wl,-z,relro,-z,now,--no-undefined,-soname,'+target.name,*objects,*libs,'-o',str(target)])
    # The upstream model suite exercises the exact same wplua objects and old core.
    test=output/'test-wplua'
    run('test-build',['gcc',*flags,str(source/'tests/wplua/wplua.c'),*objects[:len(wplua)],*libs,'-o',str(test)])
    env.update(G_TEST_SRCDIR=str(source/'tests/wplua'),G_TEST_BUILDDIR=str(output),HOME=str(output),XDG_RUNTIME_DIR=str(output))
    test_output=run('test-wplua',[str(test)]).decode()
    if '1..11\n' not in test_output or len(re.findall(r'^ok [0-9]+ /wplua/',test_output,re.M))!=11 or re.search(r'^not ok ',test_output,re.M):raise ValueError('incomplete upstream model suite')
    dynamic=run('dynamic',['readelf','--dynamic','--wide',str(target)]).decode()
    if 'liblua-5.5.so' not in dynamic or 'liblua-5.4' in dynamic or '(RPATH)' in dynamic or '(RUNPATH)' in dynamic:raise ValueError('different Lua linkage/path')
    loader=run('loader',['ldd','-r',str(target)]).decode()
    if 'not found' in loader or 'undefined symbol' in loader:raise ValueError('unresolved module dependency')
    selected={
        'libwireplumber-0.5.so.0':old_policy/'usr/lib64/libwireplumber-0.5.so.0',
        'libgio-2.0.so.0':old_policy/'usr/lib64/libgio-2.0.so.0',
        'libgobject-2.0.so.0':old_policy/'usr/lib64/libgobject-2.0.so.0',
        'libglib-2.0.so.0':headers/'usr/lib64/libglib-2.0.so.0',
        'libpipewire-0.3.so.0':sdk/'usr/lib64/libpipewire-0.3.so.0',
        'liblua-5.5.so':lua/'usr/lib64/liblua-5.5.so',
    }
    for soname,path in selected.items():
        if not re.search(r'^\s*'+re.escape(soname)+r' => '+re.escape(str(path))+r' \(',loader,re.M):raise ValueError('different loaded object: '+soname)
    # Recheck immutable source snapshots after all compiler/test subprocesses.
    audio.check(sdk);control.check(headers);policy.check(old_policy)
    for name,e in pins['lua_files'].items():check_file(lua/name,e)
    check_file(resource_tool,pins['resource_tool'])
    record=dict(format=1,profile_sha256=audio.sha(PROFILE.read_bytes()),compiler=compiler,target_triple=triple,commands=commands,upstream_tests=11,artifact=dict(size=target.stat().st_size,sha256=audio.sha(target.read_bytes())),scope=pins['purpose'])
    (output/'inputs.json').write_text(json.dumps(record,indent=2)+'\n')
    return record


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('archive','sdk','headers','old_policy','lua','resource_tool','output'):p.add_argument(name,type=Path)
    a=p.parse_args();print(json.dumps(build(**vars(a))))
