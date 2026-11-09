"""Build the private libinput/Lua55 candidate with upstream Meson and tests.

No network, RPM installation or host udev changes. Ordinary native Linux only.
Failure preserves fresh output. Runtime promotion requires input integration.
"""
import argparse
import contextlib
import io
import json
import os
import shlex
from pathlib import Path, PurePosixPath
import shutil
import stat
import subprocess
import tarfile

import prepare_audio as audio
import build_audio_policy_lua as lua_build
import prepare_settings_audio_lab as gui

ROOT=Path(__file__).resolve().parents[1]
PROFILE=ROOT/'spec/libinput-build-inputs.json'
RUNTIME_NAMES={'libudev.so.1','libevdev.so.2','libmtdev.so.1','libwacom.so.9','libgudev-1.0.so.0','libglib-2.0.so.0','libgobject-2.0.so.0','libgmodule-2.0.so.0','libgio-2.0.so.0','libffi.so.8','libpcre2-8.so.0','libsystemd.so.0','libmount.so.1','libblkid.so.1','libselinux.so.1','libz.so.1','libgcc_s.so.1','libc.so.6','libm.so.6','ld-linux-x86-64.so.2'}
LINKS={n+'.so':n+'.so.'+v for n,v in [('libudev','1'),('libevdev','2'),('libmtdev','1'),('libwacom','9'),('libgudev-1.0','0'),('libglib-2.0','0'),('libgobject-2.0','0'),('libgio-2.0','0'),('libgmodule-2.0','0'),('libffi','8'),('libpcre2-8','0'),('libsystemd','0')]}


def source_members(archive,pins):
    archive=Path(archive);lua_build.check_file(archive,pins['source']);raw=archive.read_bytes();found={};total=0
    with tarfile.open(fileobj=io.BytesIO(raw)) as a:
        members=a.getmembers();seen=set()
        if not 1<=len(members)<=4096:raise ValueError('libinput source member bound')
        for m in members:
            p=PurePosixPath(m.name)
            if not p.parts or p.parts[0]!='libinput-1.32.0' or p.is_absolute() or '..' in p.parts or str(p)!=m.name.rstrip('/') or m.name in seen or not (m.isfile() or m.isdir()):raise ValueError('unsafe libinput source member')
            seen.add(m.name)
            if m.isdir():continue
            total+=m.size
            if m.size>2*1024**2 or total>24*1024**2:raise ValueError('libinput source byte bound')
            found['/'.join(p.parts[1:])]=a.extractfile(m).read()
    return found


def selected(name,mode):
    return stat.S_ISREG(mode) and (name.startswith('usr/include/') or name=='usr/lib64/glib-2.0/include/glibconfig.h' or name.endswith('.pc') or name in ('usr/bin/meson','usr/bin/ninja','usr/bin/ninja-build') or name.startswith('usr/lib/python3.14/site-packages/mesonbuild/') and name.endswith('.py'))


def build(archive,cache,gui_runtime,lua_sdk,output,build_type='release'):
    if os.name!='posix' or os.getuid()==0 or os.getuid()!=os.geteuid() or os.uname().machine!='x86_64':raise ValueError('ordinary native x86-64 Linux builder required')
    if build_type not in ('release','debugoptimized'):raise ValueError('unsupported libinput build type')
    pins=json.loads(PROFILE.read_bytes())
    if pins['format']!=1 or pins['version']!='1.32.0' or pins['source']['commit']!='5cf833b572f5a33663888da321dce0ba1abd82a2':raise ValueError('different libinput candidate')
    archive,cache,gui_runtime,lua_sdk,output=[Path(p).absolute() for p in (archive,cache,gui_runtime,lua_sdk,output)]
    for p in (archive,cache,gui_runtime,lua_sdk,output):
        if p.resolve()!=p:raise ValueError('aliased build input/output')
    members=source_members(archive,pins);runtime=json.loads((ROOT/'spec/settings-audio-lab-inputs.json').read_bytes());gui.check(gui_runtime,runtime)
    lua=lua_build.profile()
    for n,e in lua['lua_files'].items():lua_build.check_file(lua_sdk/n,e)
    payload={};proofs=[]
    # Verify all packages before effects; signature, parser and metadata share
    # the same sealed snapshot. No scriptlet or archive link is executed.
    with contextlib.ExitStack() as stack:
        for package in pins['packages']:
            fd=stack.enter_context(audio.sealed_package(cache/package['name'],package));path='/proc/self/fd/'+str(fd)
            proof=subprocess.check_output(['rpmkeys','--checksig','--verbose',path],pass_fds=(fd,),text=True)
            if pins['signer']+': OK' not in proof or 'Payload SHA256 digest: OK' not in proof:raise ValueError('unverified build RPM')
            metadata=subprocess.check_output(['rpm','-qp','--qf','%{NAME} %{EVR}.%{ARCH}\n%{LICENSE}\n%{SOURCERPM}\n',path],pass_fds=(fd,),text=True)
            scripts=subprocess.check_output(['rpm','-qp','--scripts',path],pass_fds=(fd,),text=True)
            if metadata.splitlines()!=package['metadata'] or scripts:raise ValueError('different SDK origin/scripts')
            count=0
            for name,mode,raw in audio.cpio_members(subprocess.check_output(['rpm2cpio',path],pass_fds=(fd,))):
                if not selected(name,mode):continue
                if name in payload:raise ValueError('conflicting SDK member')
                payload[name]=(bytes(raw),stat.S_IMODE(mode));count+=1
            if count!=package['selected_files']:raise ValueError('changed SDK selection: '+package['name'])
            proofs.append(dict(package=package['name'],signature=proof,metadata=metadata,selected=count))
    output.mkdir(mode=0o700);sdk=output/'sdk';sdk.mkdir();source=output/'source';source.mkdir();logs=output/'logs';logs.mkdir();temporary=output/'tmp';temporary.mkdir(mode=0o700)
    def put(root,n,b,mode):
        p=root/n;p.parent.mkdir(parents=True,exist_ok=True)
        with p.open('xb') as f:f.write(b)
        p.chmod(mode)
    for n,(raw,mode) in payload.items():put(sdk,n,raw,mode)
    copied=set()
    for e in runtime['files']:
        name=Path(e['destination']).name
        if e['role']=='library' and name in RUNTIME_NAMES:put(sdk,e['destination'].lstrip('/'),(gui_runtime/e['destination'].lstrip('/')).read_bytes(),e['mode']);copied.add(name)
    if copied!=RUNTIME_NAMES:raise ValueError('different SDK runtime closure')
    for n in lua['lua_files']:put(sdk,n,(lua_sdk/n).read_bytes(),0o755 if n.endswith('.so') else 0o644)
    lib=sdk/'usr/lib64';pc=lib/'pkgconfig'
    for n,target in LINKS.items():(lib/n).symlink_to(target)
    # The RPM .pc files describe /usr. Ask pkgconf to relocate their prefix;
    # otherwise an apparently successful version lookup can use host headers.
    put(sdk,'usr/bin/pkg-config',b'#!/usr/bin/python3\nimport os,sys\nos.execv("/usr/bin/pkg-config",["pkg-config","--define-prefix",*sys.argv[1:]])\n',0o755)
    # A generic pkg-config name advertises the actual 5.5 ABI/version. It never
    # impersonates lua5.4 and no old-ABI shared-library alias is provided.
    (pc/'lua.pc').write_text('prefix='+str(sdk/'usr')+'\nlibdir=${prefix}/lib64\nincludedir=${prefix}/include/lua5.5\n\nName: Lua\nDescription: Reviewed private Lua55 SDK\nVersion: 5.5.1\nLibs: -L${libdir} -llua-5.5\nCflags: -I${includedir}\n')
    for n,raw in members.items():put(source,n,raw,0o644)
    # Upstream test helpers need their reviewed executable bits, not package
    # scriptlets. Restore only regular source entries whose tag records execute.
    with tarfile.open(archive) as a:
        for m in a:
            if m.isfile() and m.mode&0o111:(source/'/'.join(PurePosixPath(m.name).parts[1:])).chmod(0o755)
    env={**os.environ,'LC_ALL':'C','SOURCE_DATE_EPOCH':'0','PATH':str(sdk/'usr/bin')+':/usr/bin:/bin','PYTHONPATH':str(sdk/'usr/lib/python3.14/site-packages'),'PYTHONDONTWRITEBYTECODE':'1','PKG_CONFIG_LIBDIR':str(pc),'PKG_CONFIG_PATH':'','PKG_CONFIG_SYSROOT_DIR':'','LD_LIBRARY_PATH':str(lib),'CC':'/usr/bin/gcc','CFLAGS':'-fstack-protector-strong -D_FORTIFY_SOURCE=3','LDFLAGS':'-Wl,-z,relro,-z,now','HOME':str(output),'TMPDIR':str(temporary)}
    commands=[]
    def run(label,args,negative=None):
        commands.append(args);p=subprocess.run(args,cwd=source,env=env,capture_output=True)
        (logs/(label+'.stdout')).write_bytes(p.stdout);(logs/(label+'.stderr')).write_bytes(p.stderr)
        if negative is None and p.returncode:raise RuntimeError(label+' failed; inspect '+str(logs))
        if negative is not None and (p.returncode==0 or negative not in p.stderr):raise RuntimeError(label+' counterexample did not detect its guard')
        return p.stdout
    compiler=run('compiler',['gcc','--version']).decode();triple=run('triple',['gcc','-dumpmachine']).decode().strip()
    versions=run('dependencies',['pkg-config','--modversion','lua','libevdev','libudev','mtdev','libwacom']).decode().splitlines()
    if versions!=['5.5.1','1.13.6','259','1.1.6','2.19.0']:raise ValueError('different libinput build providers')
    cflags=shlex.split(run('dependency-paths',['pkg-config','--cflags','libudev','libevdev','libwacom']).decode())
    if '-I'+str(sdk/'usr/include') not in cflags or any(x.startswith('-I/') and not x.startswith('-I'+str(sdk)+'/') for x in cflags):raise ValueError('unrelocated SDK header dependency')
    builddir=output/'build';stage=output/'stage'
    run('configure',['meson','setup',str(builddir),str(source),'--prefix=/usr','--libdir=lib64','--wrap-mode=nodownload','--buildtype='+build_type,'-Dc_std=gnu17','-Db_ndebug=false','-Dauto_features=disabled','-Dlua-plugins=enabled','-Dautoload-plugins=false','-Dmtdev=true','-Dlibwacom=true','-Dtests=true','-Dinstall-tests=false','-Ddebug-gui=false','-Ddocumentation=false','-Dzshcompletiondir=no'])
    run('compile',['meson','compile','-C',str(builddir),'-j2'])
    run('deviceless',['meson','test','-C',str(builddir),'--no-rebuild','--verbose','--print-errorlogs','--num-processes=1','libinput-test-deviceless'])
    run('utils',['meson','test','-C',str(builddir),'--no-rebuild','--verbose','--print-errorlogs','--num-processes=1','test-utils','test-library-version'])
    env['DESTDIR']=str(stage);run('stage',['meson','install','-C',str(builddir),'--no-rebuild'])
    target=stage/'usr/lib64/libinput.so.10.13.0'
    dynamic=run('dynamic',['readelf','--dynamic','--wide',str(target)]).decode()
    if 'liblua-5.5.so' not in dynamic or 'liblua-5.4' in dynamic or '(RPATH)' in dynamic or '(RUNPATH)' in dynamic:raise ValueError('different installed Lua linkage/path')
    probe=output/'test-libinput-lua'
    run('probe-build',['gcc','-std=c17','-O2','-Wall','-Wextra','-Werror','-I'+str(source/'src'),'-I'+str(sdk/'usr/include'),'-I'+str(sdk/'usr/include/lua5.5'),str(ROOT/'tests/test_libinput_lua.c'),str(target),str(lib/'liblua-5.5.so'),'-Wl,-rpath-link,'+str(lib),'-Wl,-z,relro,-z,now','-o',str(probe)])
    env['LD_LIBRARY_PATH']=str(stage/'usr/lib64')+':'+str(lib)
    run('matching-bytecode',[str(probe),str(output/'matching-bytecode')])
    # Upstream's embedded negative vector is Lua5.4; a Lua55 decoder could reject
    # it for version mismatch even without text-only admission. Regenerate55 above
    # and prove the actual guard with a separate deliberately weakened library.
    counter=output/'counter-source';shutil.copytree(source,counter);path=counter/'src/libinput-plugin-lua.c';raw=path.read_bytes();before=b'luaL_loadfilex(L, path, "t")'
    if raw.count(before)!=1:raise ValueError('different bytecode guard preimage')
    path.write_bytes(raw.replace(before,b'luaL_loadfilex(L, path, NULL)'))
    counterdir=output/'counter-build'
    configuration=commands[next(i for i,c in enumerate(commands) if c[:2]==['meson','setup'])]
    run('counter-configure',[v.replace(str(builddir),str(counterdir)).replace(str(source),str(counter)).replace('-Dtests=true','-Dtests=false') for v in configuration])
    run('counter-compile',['ninja','-C',str(counterdir),'-j2','libinput.so.10.13.0'])
    env['LD_LIBRARY_PATH']=str(counterdir)+':'+str(lib)
    run('matching-bytecode-counter',[str(probe),str(output/'matching-bytecode-counter')],negative=b'MATCHING_BYTECODE_ACCEPTED')
    gui.check(gui_runtime,runtime)
    for n,e in lua['lua_files'].items():lua_build.check_file(lua_sdk/n,e)
    record=dict(format=1,build_type=build_type,profile_sha256=audio.sha(PROFILE.read_bytes()),compiler=compiler,target_triple=triple,providers=versions,packages=proofs,commands=commands,artifact=dict(path=str(target),size=target.stat().st_size,sha256=audio.sha(target.read_bytes())),scope=pins['purpose'])
    (output/'inputs.json').write_text(json.dumps(record,indent=2)+'\n');return record


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for n in ('archive','cache','gui_runtime','lua_sdk','output'):p.add_argument(n,type=Path)
    p.add_argument('--build-type',choices=('release','debugoptimized'),default='release')
    a=p.parse_args();print(json.dumps(build(**vars(a))))
