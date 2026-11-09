"""Explicit disposable DRM/libinput integration; never a login manager.

An independently selected normal compositor and Wayland client run as UID1000
in one finite PAM/logind fixture. No input-injection protocol is exposed. The
host supplies actual QMP hardware events, observes the client, then creates the
protected finish request. Unknown effects retain the original keeper/handles.
"""
from pathlib import Path
import argparse
import errno
import hashlib
import json
import os
import re
import select
import signal
import stat
import subprocess
import sys
import time
import uuid

sys.dont_write_bytecode=True
import image_state
from audio_mac_bus import Bus
from test_libinput_devices import devices,input_nodes,rules,verify_build,facts
import prepare_settings_audio_lab as gui


def run(*args):
    return subprocess.check_output(args,text=True,stderr=subprocess.STDOUT,timeout=15).strip()


def verify_programs(root,files):
    root=Path(root)
    if root.parent!=Path('/run') or not re.fullmatch('a-ci-[0-9a-f]{12}',root.name) or root.resolve()!=root:
        raise ValueError('independently selected fresh compositor inputs required')
    found=set()
    for p in [root,*root.rglob('*')]:
        s=p.lstat()
        if s.st_uid or s.st_gid or s.st_mode&0o222 or not (stat.S_ISDIR(s.st_mode) or stat.S_ISREG(s.st_mode)):
            raise ValueError('unsealed/aliased compositor input')
        if p.is_file():
            n=p.relative_to(root).as_posix();found.add(n);e=files.get(n)
            if e is None or s.st_nlink!=1 or s.st_size!=e['size'] or hashlib.sha256(p.read_bytes()).hexdigest()!=e['sha256']:
                raise ValueError('different compositor input: '+n)
    if found!=set(files):raise ValueError('incomplete compositor inputs')


def verify_input(value):
    selected=value.get('input_profile','build')
    if selected=='build':
        root=Path(value['input']['root']);verify_build(root,value['input']['files'])
        expected=str(root)+'/release:'+str(root)+'/candidate/sdk/usr/lib64:'+value['gui']+'/usr/lib64'
        quirks=str(root)+'/candidate/source/quirks'
        library=str(root)+'/release/libinput.so.10.13.0';lua=str(root)+'/candidate/sdk/usr/lib64/liblua-5.5.so'
    elif selected=='projected':
        import prepare_libinput_runtime as projected
        projected.check_protected(value['input_runtime']);root=Path(value['input_runtime'])
        expected=str(root/'usr/lib64')+':'+value['gui']+'/usr/lib64';quirks=str(root/'usr/share/libinput')
        library=str(root/'usr/lib64/libinput.so.10.13.0');lua=str(root/'usr/lib64/liblua-5.5.so')
    else:raise ValueError('explicit input profile required')
    if value['library_path']!=expected:raise ValueError('unbound library search')
    if value.get('quirks_directory')!=quirks:raise ValueError('unbound libinput quirks')
    if len(list(Path(quirks).glob('*.quirks')))!=54:raise ValueError('incomplete libinput1.32 quirks')
    return library,lua


def session(selection,runtime):
    if os.getuid()!=os.geteuid() or os.getuid()!=1000 or os.getgid()!=os.getegid() or os.getgid()!=1000:
        raise ValueError('ordinary fixture session only')
    value=json.loads(Path(selection).read_bytes());runtime=Path(runtime)
    env={**os.environ,'XDG_RUNTIME_DIR':str(runtime),'LIBSEAT_BACKEND':'logind',
         'WAYLAND_DISPLAY':'aetheri-input','LD_LIBRARY_PATH':value['library_path'],
         'XDG_CONFIG_HOME':value['wacom']['root']+'/usr/share',
         'LIBINPUT_QUIRKS_DIR':value['quirks_directory']}
    env.pop('SUDO_COMMAND',None)
    processes={}
    logs=[]
    def start(name,args,stdin):
        f=(runtime/(name+'.log')).open('xb');logs.append(f)
        p=subprocess.Popen(args,env=env,stdin=stdin,stdout=f,stderr=subprocess.STDOUT)
        processes[name]=p
        return p
    compositor=start('compositor',[value['compositor'],'--drm','aetheri-input','-','/dev/dri/card0'],subprocess.DEVNULL)
    deadline=time.monotonic()+12
    while 'READY aetheri-input outputs=1 renderer=pixman' not in (runtime/'compositor.log').read_text():
        if compositor.poll() is not None or time.monotonic()>deadline:raise RuntimeError('original DRM compositor did not become ready')
        time.sleep(.025)
    client=start('client',[value['client'],'--single-output-input'],subprocess.PIPE)
    print(json.dumps(dict(pid=os.getpid(),compositor=compositor.pid,client=client.pid,
        login=run('loginctl','show-session','self','-p','Id','-p','User','-p','Active','-p','Remote','-p','Seat','-p','Type','-p','Class','-p','Scope','-p','TTY'))),flush=True)
    control=os.open(runtime/'control',os.O_RDONLY|os.O_NONBLOCK|os.O_CLOEXEC)
    deadline=time.monotonic()+300
    while True:
        if any(p.poll() is not None for p in processes.values()):raise RuntimeError('original input actor exited before finish request')
        ready=select.select([control],[],[],.05)[0]
        if ready:
            if os.read(control,32)!=b'finish\n':raise ValueError('invalid fixture finish request')
            break
        if time.monotonic()>deadline:raise TimeoutError('host input acceptance not supplied')
    os.close(control)
    client.stdin.write(b'q');client.stdin.flush();client.stdin.close()
    if client.wait(timeout=10)!=0:raise RuntimeError('original input client failed')
    compositor.terminate()
    if compositor.wait(timeout=10)!=0:raise RuntimeError('original DRM compositor failed')
    for f in logs:f.close()
    print('AETHERI_INPUT_SESSION_END client=0 compositor=0',flush=True)


class Fixture:
    def __init__(self,selection,output):
        if not __debug__ or sys.platform!='linux' or os.geteuid()!=0 or Path('/sys/class/dmi/id/product_name').read_text().strip()!='Aetheri audio lab VM' or Path('/sys/fs/selinux/enforce').read_text().strip()!='1':
            raise ValueError('explicit enforcing disposable audio lab required')
        self.selection=Path(selection);self.value=json.loads(self.selection.read_bytes());v=self.value
        if Path('/proc/sys/kernel/random/boot_id').read_text().strip()!=v['boot']:raise ValueError('different selected boot')
        verify_programs(v['program_root'],v['program_files'])
        self.profile=json.loads((Path(v['program_root'])/'gui-profile.json').read_bytes())
        gui.check(v['gui'],self.profile)
        for p in [Path(v['gui']),*Path(v['gui']).rglob('*')]:
            s=p.lstat()
            if s.st_uid or s.st_gid or s.st_mode&0o022:raise ValueError('unprotected GUI input')
        self.input_library,self.input_lua=verify_input(v)
        verify_build(Path(v['wacom']['root']),v['wacom']['files'],'a-wacom-')
        if v['compositor']!=v['program_root']+'/aetheri-compositor' or v['client']!=v['program_root']+'/test_compositor_client':raise ValueError('foreign selected program')
        if any(Path('/etc/pam.d').glob('aetheri-input-*')):raise RuntimeError('pending/foreign input PAM fixture')
        if any(Path(p).exists() for p in ('/etc/libwacom','/usr/share/libwacom')):raise ValueError('unselected Wacom fallback')
        sessions=run('loginctl','list-sessions','--no-legend','--no-pager')
        if any('seat0' in line for line in sessions.splitlines()):raise RuntimeError('existing local seat session; preserve')
        if not Path('/dev/dri/card0').is_char_device():raise ValueError('original DRM card absent')
        self.output=Path(output);self.output.mkdir(mode=0o700)
        self.token=uuid.uuid4().hex;self.unit='aetheri-input-'+self.token+'.service'
        self.record=dict(state='preparing',boot=v['boot'],unit=self.unit,pending=None,selection_sha256=hashlib.sha256(self.selection.read_bytes()).hexdigest(),input_profile=v.get('input_profile','build'))
        self.bus=Bus();self.private=Bus(private=True);self.pins={};self.groups={};self.refs=[]
        self.save()

    def save(self):image_state.write(self.output/'controller.json',self.record)

    def effect(self,name,action):
        if self.record['pending'] is not None:raise RuntimeError('earlier effect unresolved')
        self.record['pending']=name;self.save();result=action()
        self.record['pending']=None;self.save();return result

    def journal(self):return run('journalctl','-b','-t',self.unit,'-o','cat','--no-pager')

    def wait(self,predicate,description,seconds=15):
        deadline=time.monotonic()+seconds
        while not predicate():
            if time.monotonic()>deadline:raise TimeoutError(description+'; preserve original fixture')
            time.sleep(.025)

    def pin(self,name,pid,expected):
        fd=os.pidfd_open(pid)
        if select.select([fd],[],[],0)[0]:raise RuntimeError('original actor already ended')
        actual=Path('/proc')/str(pid);a=(actual/'exe').stat();b=Path(expected).stat()
        if (a.st_dev,a.st_ino)!=(b.st_dev,b.st_ino):raise RuntimeError('different actor image')
        status=dict((k,v.strip()) for k,v in (line.split(':',1) for line in (actual/'status').read_text().splitlines() if ':' in line))
        if not all(status[k].split()==['1000']*4 for k in ('Uid','Gid')) or not all(int(status[k],16)==0 for k in ('CapEff','CapPrm','CapInh','CapAmb')) or status['NoNewPrivs']!='1':raise RuntimeError('actor ordinary context changed')
        if (actual/'attr/current').read_bytes().rstrip(b'\0\n')!=b'system_u:system_r:unconfined_service_t:s0':raise RuntimeError('different fixture domain')
        self.pins[name]=fd
        return dict(pid=pid,pidfd_inode=os.fstat(fd).st_ino,image=[a.st_dev,a.st_ino],ordinary={k:status[k] for k in ('Uid','Gid','CapEff','CapPrm','CapInh','CapAmb','NoNewPrivs')})

    def check_live(self):
        if any(select.select([p],[],[],0)[0] for p in self.pins.values()):raise RuntimeError('original actor ended')
        current=facts(self.unit)
        if any(current[k]!=self.record['original'][k] for k in ('MainPID','InvocationID','ControlGroup')):raise RuntimeError('original unit changed')
        scope=facts(self.scope)
        if any(scope[k]!=self.record['scope_original'][k] for k in ('InvocationID','ControlGroup')) or scope['ActiveState']!='active':raise RuntimeError('original PAM scope changed')
        for name,actor in self.record['actors'].items():
            group=(Path('/proc')/str(actor['pid'])/'cgroup').read_text().strip()
            if group!='0::'+self.record['scope_original']['ControlGroup']:raise RuntimeError('actor left original PAM scope')
        return current

    def execute(self):
        v=self.value;self.record['baseline']=dict(devices=devices(),nodes=input_nodes(),rules=rules(),udev=run('systemctl','show','systemd-udevd.service','-p','MainPID','-p','InvocationID'))
        self.old_vt=Path('/sys/class/tty/tty0/active').read_text().strip()
        if not re.fullmatch('tty[0-9]+',self.old_vt):raise ValueError('unknown original VT')
        self.runtime=Path('/run/aetheri-input-'+self.token);self.effect('create-runtime',lambda:self.runtime.mkdir(mode=0o700));os.chown(self.runtime,1000,1000)
        os.mkfifo(self.runtime/'control',0o600);os.chown(self.runtime/'control',1000,1000)
        self.control=os.open(self.runtime/'control',os.O_RDWR|os.O_NONBLOCK|os.O_CLOEXEC)
        self.pam=Path('/etc/pam.d')/('aetheri-input-'+self.token)
        content=b'auth required pam_permit.so\naccount required pam_permit.so\nsession required pam_systemd.so type=wayland class=user\n'
        def create_pam():
            with self.pam.open('xb') as f:f.write(content);f.flush();os.fsync(f.fileno())
            self.pam.chmod(0o444);run('restorecon',str(self.pam))
        self.effect('create-pam',create_pam);s=self.pam.stat();self.pam_identity=(s.st_dev,s.st_ino);self.pam_content=content
        self.record.update(runtime=str(self.runtime),old_vt=self.old_vt,pam=str(self.pam),pam_object=list(self.pam_identity));self.save()
        self.effect('activate-test-vt',lambda:run('chvt','8'))
        env=['XDG_SESSION_TYPE=wayland','XDG_SESSION_CLASS=user','XDG_SEAT=seat0','XDG_VTNR=8','PATH=/usr/bin:/bin','PYTHONDONTWRITEBYTECODE=1']
        script=str(Path(__file__).resolve())
        args=['/usr/bin/python3',script,'--session',str(self.selection),str(self.runtime)]
        props=[('Description','s','Aetheri explicit DRM input fixture'),('Type','s','exec'),('User','s','1000'),('Group','s','1000'),('PAMName','s',self.pam.name),('TTYPath','s','/dev/tty8'),('StandardInput','s','tty'),('NoNewPrivileges','b',True),('CapabilityBoundingSet','t',0),('AmbientCapabilities','t',0),('Restart','s','no'),('KillMode','s','control-group'),('StandardOutput','s','journal'),('StandardError','s','journal'),('SyslogIdentifier','s',self.unit),('Environment','as',env),('ExecStart','a(sasb)',[('/usr/bin/python3',args,False)])]
        if facts(self.unit)['LoadState']!='not-found':raise RuntimeError('foreign service name')
        self.effect('start-session',lambda:self.private.create(self.unit,props));self.effect('ref-service',lambda:self.bus.unit('RefUnit',self.unit));self.refs.append(self.unit)
        self.wait(lambda:any(line.startswith('{') for line in self.journal().splitlines()),'session actors')
        rows=[json.loads(line) for line in self.journal().splitlines() if line.startswith('{')]
        if len(rows)!=1:raise RuntimeError('ambiguous original session')
        info=rows[0];original=facts(self.unit)
        if original['MainPID']!=str(info['pid']) or original['ActiveState']!='active' or original['Job']:raise RuntimeError('unsettled original session')
        self.record['original']=original
        login=dict(line.split('=',1) for line in info['login'].splitlines())
        if not all(login[k]==value for k,value in dict(User='1000',Active='yes',Remote='no',Seat='seat0',Type='wayland',Class='user',TTY='tty8').items()):raise RuntimeError('different local session')
        self.scope=login['Scope'];self.effect('ref-scope',lambda:self.bus.unit('RefUnit',self.scope));self.refs.append(self.scope)
        self.record.update(login=login,scope_original=facts(self.scope),actors={})
        for name,executable in [('session','/usr/bin/python3'),('compositor',v['compositor']),('client',v['client'])]:
            pid=info['pid'] if name=='session' else info[name]
            self.record['actors'][name]=self.pin(name,pid,executable)
        for unit in (self.unit,self.scope):
            observed=facts(unit);path=Path('/sys/fs/cgroup'+observed['ControlGroup']);fd=os.open(path,os.O_RDONLY|os.O_DIRECTORY|os.O_CLOEXEC);events=os.open('cgroup.events',os.O_RDONLY|os.O_CLOEXEC,dir_fd=fd)
            self.groups[unit]=(path,fd,events,(os.fstat(fd).st_dev,os.fstat(fd).st_ino))
        self.record['groups']={name:dict(path=str(item[0]),object=list(item[3])) for name,item in self.groups.items()};self.save();self.check_live()
        maps=(Path('/proc')/str(info['compositor'])/'maps').read_text()
        input_paths={line.split()[-1] for line in maps.splitlines() if 'libinput.so.' in line}
        lua_paths={line.split()[-1] for line in maps.splitlines() if 'liblua-' in line}
        if input_paths!={self.input_library} or lua_paths!={self.input_lua}:raise RuntimeError('wrong libinput/Lua runtime')
        (self.output/'compositor-maps.txt').write_text(maps)
        self.wait(lambda:'FRAME 1 ' in (self.runtime/'client.log').read_text(),'actual Wayland frames')
        if 'libinput error:' in (self.runtime/'compositor.log').read_text():raise RuntimeError('libinput initialization/data failure')
        self.record.update(state='awaiting-hardware-input',runtime_bound=True);self.save();print(json.dumps(dict(output=str(self.output),runtime=str(self.runtime),unit=self.unit,state=self.record['state'])),flush=True)
        self.wait(lambda:(self.output/'finish').exists(),'host hardware-input acceptance',seconds=240);self.check_live()
        marker=self.output/'finish';m=marker.lstat()
        if not stat.S_ISREG(m.st_mode) or m.st_uid or m.st_nlink!=1 or marker.read_bytes()!=b'finish\n':raise RuntimeError('invalid protected finish intent')
        text=(self.runtime/'client.log').read_text()
        if not re.search(r'^KEY [01] 30 1 97$',text,re.M) or not re.search(r'^KEY [01] 30 0 97$',text,re.M) or not re.search(r'^MOTION [01] ',text,re.M) or not re.search(r'^BUTTON [01] 272 1$',text,re.M) or not re.search(r'^BUTTON [01] 272 0$',text,re.M):raise RuntimeError('actual input observations incomplete')
        self.effect('finish-session',lambda:os.write(self.control,b'finish\n'))
        self.wait(lambda:all(select.select([p],[],[],0)[0] for p in self.pins.values()) and facts(self.unit)['ActiveState']=='inactive' and not facts(self.unit)['Job'],'original actors end')
        final=facts(self.unit)
        if final['InvocationID']!=original['InvocationID'] or final['MainPID']!='0' or final['ExecMainStatus']!='0' or 'AETHERI_INPUT_SESSION_END client=0 compositor=0' not in self.journal():raise RuntimeError('original session failed')
        for name,(path,fd,events,identity) in self.groups.items():
            if (os.fstat(fd).st_dev,os.fstat(fd).st_ino)!=identity:raise RuntimeError('cgroup handle changed')
            try:
                raw=os.pread(events,4096,0).decode();s=path.stat()
                if (s.st_dev,s.st_ino)!=identity or 'populated 0' not in raw:raise RuntimeError('original cgroup not empty')
                end='same-original-empty'
            except OSError as error:
                if error.errno!=errno.ENODEV or path.exists():raise
                end='original-reader-ENODEV-and-name-absent'
            self.record['groups'][name]['end']=end
        self.wait(lambda:subprocess.run(['loginctl','show-session',login['Id']],capture_output=True).returncode!=0,'original session removal')
        run('udevadm','settle','--timeout=10')
        after=dict(devices=devices(),nodes=input_nodes(),rules=rules(),udev=run('systemctl','show','systemd-udevd.service','-p','MainPID','-p','InvocationID'))
        if after!=self.record['baseline']:raise RuntimeError('input/udev baseline changed')
        for name in ('compositor.log','client.log'):(self.output/name).write_bytes((self.runtime/name).read_bytes())
        if 'libinput error:' in (self.output/'compositor.log').read_text():raise RuntimeError('libinput runtime error')
        (self.output/'session.log').write_text(self.journal())
        self.record.update(state='original-ended',final=final,after=after);self.save()
        for unit in reversed(self.refs):self.effect('unref-'+unit,lambda unit=unit:self.bus.unit('UnrefUnit',unit))
        s=self.pam.stat()
        if (s.st_dev,s.st_ino)!=self.pam_identity or s.st_uid or self.pam.read_bytes()!=self.pam_content:raise RuntimeError('PAM object changed')
        self.effect('remove-owned-pam',self.pam.unlink);self.effect('restore-original-vt',lambda:run('chvt',self.old_vt[3:]))
        verify_programs(v['program_root'],v['program_files']);gui.check(v['gui'],self.profile);verify_input(v);verify_build(Path(v['wacom']['root']),v['wacom']['files'],'a-wacom-')
        for pin in self.pins.values():os.close(pin)
        for path,fd,events,identity in self.groups.values():os.close(events);os.close(fd)
        os.close(self.control);self.bus.close();self.private.close();self.record['state']='retired';self.save();print(json.dumps(dict(output=str(self.output),state='retired')),flush=True)


def main():
    if len(sys.argv)==4 and sys.argv[1]=='--session':session(sys.argv[2],sys.argv[3]);return
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('selection',type=Path);parser.add_argument('output',type=Path);args=parser.parse_args()
    fixture=Fixture(args.selection,args.output)
    try:fixture.execute()
    except BaseException as error:
        fixture.record.update(inspection_required=True,error=repr(error),keeper_pid=os.getpid());fixture.save();print(json.dumps(dict(output=str(fixture.output),error=repr(error))),flush=True)
        while True:signal.pause()


if __name__=='__main__':main()
