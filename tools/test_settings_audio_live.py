"""Actual Sound GUI/owner/graph test in the separate enforcing audio lab.

The GUI and headless input controller are trusted unconfined test actors;
graph/policy retain the existing confined user-service roles. This is not
normal app/session admission, a production login manager or device policy.
Unknown effects retain the original handles and journal for inspection.
"""
import argparse
import errno
import gzip
import hashlib
import json
import os
from pathlib import Path
import re
import select
import signal
import socket
import stat
import subprocess
import sys
import time
import traceback

from audio_mac_bus import Bus
from test_audio_mac import Audio, facts, label, run
from test_audio_user import UserAudio
import prepare_audio
import prepare_audio_policy
import prepare_audio_control
import prepare_settings_audio_lab


def ordinary(pid):
    values=dict(line.split(':',1) for line in (Path('/proc')/str(pid)/'status').read_text().splitlines() if ':' in line)
    if any(values[k].split()!=['1000']*4 for k in ('Uid','Gid')) or any(int(values[k],16) for k in ('CapEff','CapPrm','CapInh','CapAmb')) or values['NoNewPrivs'].strip()!='1':
        raise ValueError('different ordinary test process')
    return {k:values[k].strip() for k in ('Uid','Gid','CapEff','CapPrm','CapInh','CapAmb','NoNewPrivs')}


def check_binaries(directory):
    directory=Path(directory).absolute()
    if directory.resolve()!=directory or not directory.is_dir():raise ValueError('different native test directory')
    for name in ('probe_audio_mac','probe_audio_actor','aetheri-audio-control-probe','aetheri-audio-check'):
        p=directory/name
        try:st=p.lstat()
        except FileNotFoundError:raise ValueError('missing native test program: '+name) from None
        if not stat.S_ISREG(st.st_mode) or st.st_uid!=0 or st.st_mode&0o022 or not st.st_mode&0o111 or p.read_bytes()[:4]!=b'\x7fELF':
            raise ValueError('unprotected/non-ELF native test program: '+name)


def dump_diagnostics(stderr):
    # This fixed synthetic node has no ProcessLatency parameter. pw-dump still
    # enumerates it and reports ENOENT with exit0. Preserve that exact diagnostic;
    # relevant Props, the selected node and values are checked independently.
    pattern=r'\[E\]\[\d{2}:\d{2}:\d{2}\.\d+\] default\s+\| \[\s*pw-dump\.c:\s*1509 on_core_error\(\)\] error id:\d+ seq:\d+ res:-2 \(No such file or directory\): enum params id:16 \(Spa:Enum:ParamId:ProcessLatency\) failed'
    return all(re.fullmatch(pattern,line) for line in stderr.splitlines())


def current_frame(text):
    frames=list(re.finditer(r'^FRAME ',text,re.M))
    return text[frames[-1].start():] if frames else text


class Sound(UserAudio):
    def session_cases(self):return ('sound',)

    def image(self,name,path):
        # The GUI's real session owner performs the native original-actor
        # admission and ongoing checks. The separate ten-second PCM observer
        # processes belong to test_audio_user, not this interactive test.
        Audio.image(self,name,path)

    def setup(self,sdk,policy,control,binary,source,gui,policy_profile='rpm'):
        self.gui=gui;self.root_bus=Bus();self.root_create_bus=Bus(private=True);self.gui_pin=None;self.input_fds=[]
        self.server=None;self.server_pin=None;self.server_log=None;self.input=None
        os.environ['LD_LIBRARY_PATH']=':'.join(str(self.root/n/'usr/lib64') for n in ('control','policy','sdk'))
        super().setup(sdk,policy,control,binary,source,policy_profile=policy_profile)

    def original_sources(self):
        self.check_manager()
        for name in (self.graph,self.policy_unit):
            u=self.units[name];current=self.facts(name)
            assert not select.select([u['pin']],[],[],0)[0]
            assert current['MainPID']==u['facts']['MainPID'] and current['InvocationID']==u['facts']['InvocationID'] and current['ActiveState']=='active' and not current['Job']

    def begin_audio(self):
        root=self.root/'sound';root.mkdir(mode=0o755);label(root,'aetheri_audio_fixture_test_t')
        runtime=root/'run';state=root/'state'
        for p,kind in ((runtime,'run'),(state,'state')):
            p.mkdir(mode=0o700);os.chown(p,1000,1000);label(p,'aetheri_audio_'+kind+'_test_t')
        fifo=root/'capture.fifo';os.mkfifo(fifo,0o600);os.chown(fifo,1000,1000);label(fifo,'aetheri_audio_capture_test_t')
        self.capture_reader=os.open(fifo,os.O_RDONLY|os.O_NONBLOCK|os.O_CLOEXEC)
        st=os.fstat(self.capture_reader);self.capture_object=(st.st_dev,st.st_ino);self.capture_bytes=0
        def pump():
            for _ in range(8):
                try:raw=os.read(self.capture_reader,65536)
                except BlockingIOError:break
                if not raw:break
                self.capture_bytes+=len(raw)
                assert self.capture_bytes<=8*1024**2,'unexpected diagnostic stream budget'
        self.pump=pump
        conf=self.configure_case(root,fifo)
        for p in (conf,*conf.rglob('*')):label(p,'aetheri_audio_config_test_t')
        self.remote=runtime/'aetheri-audio-test'
        self.audio_env=dict(PATH='/usr/bin:/bin',HOME=str(root),XDG_RUNTIME_DIR=str(runtime),XDG_CONFIG_HOME=str(conf),XDG_STATE_HOME=str(state),
            LD_LIBRARY_PATH=':'.join(str(p/'usr/lib64') for p in (self.control,self.policy,self.sdk)),PIPEWIRE_REMOTE=str(self.remote),
            PIPEWIRE_CONFIG_DIR=str(conf),PIPEWIRE_MODULE_DIR=str(conf/'modules'),SPA_PLUGIN_DIR=str(self.sdk/'usr/lib64/spa-0.2'),
            WIREPLUMBER_CONFIG_DIR=str(conf/'wp'),WIREPLUMBER_DATA_DIR=str(self.policy/'usr/share/wireplumber'),
            WIREPLUMBER_MODULE_DIR=str(self.policy/'usr/lib64/wireplumber-0.5'),GIO_USE_VFS='local',GIO_MODULE_DIR='/nonexistent')
        def start(role,args):
            name=self.start('sound',role,[str(self.images['gateway']),'exec','none',*map(str,args)],self.audio_env)
            self.release(name);self.image(name,Path(args[0]));return name
        self.graph=start('graph',[self.images['graph'],'-c','server.conf'])
        self.wait(self.remote.exists,[self.graph],'original graph listener')
        self.policy_unit=start('policy',[self.images['policy'],'-p','aetheri-policy-test'])
        self.original_sources()

    def gui_log(self):
        text=run('journalctl','-b','-t',self.gui_unit,'-o','cat','--no-pager')
        if len(text)>8*1024**2:raise ValueError('GUI diagnostic budget')
        return text

    def gui_wait(self,predicate,description,seconds=15):
        deadline=time.monotonic()+seconds
        while True:
            self.pump();text=self.gui_log()
            if predicate(text):return text
            if self.gui_pin is not None and select.select([self.gui_pin],[],[],0)[0]:raise RuntimeError(description+' early GUI exit: '+text[-3000:])
            if time.monotonic()>deadline:raise TimeoutError(description+': '+text[-3000:])
            time.sleep(.025)

    def begin_gui(self):
        self.gui_dir=self.root/'gui';self.gui_dir.mkdir(mode=0o755)
        runtime=self.gui_dir/'runtime';runtime.mkdir(mode=0o700);os.chown(runtime,1000,1000)
        self.captures=self.gui_dir/'captures';self.captures.mkdir(mode=0o700);os.chown(self.captures,1000,1000)
        env={**self.audio_env,'LD_LIBRARY_PATH':str(self.gui/'usr/lib64'),'XDG_RUNTIME_DIR':str(runtime),'WAYLAND_DISPLAY':'aetheri-sound-live','NEXAUI_PLATFORM':'wayland'}
        self.input,issuer=socket.socketpair(socket.AF_UNIX,socket.SOCK_SEQPACKET|socket.SOCK_CLOEXEC);self.input.settimeout(4)
        self.server_log=(self.output/'compositor.log').open('xb')
        args=[*self.bus.base,str(self.gui/'usr/libexec/aetheri-audio-lab/test_compositor_server'),'--headless',env['WAYLAND_DISPLAY'],str(self.captures),str(issuer.fileno())]
        self.record['gui_compositor']=dict(state='dispatch-pending',arguments=args);self.save()
        self.server=subprocess.Popen(args,env=env,pass_fds=(issuer.fileno(),),stdout=self.server_log,stderr=subprocess.STDOUT);issuer.close()
        self.server_pin=os.pidfd_open(self.server.pid)
        self.record['gui_compositor'].update(state='original-pinned-awaiting-exec',pid=self.server.pid,pidfd_inode=os.fstat(self.server_pin).st_ino);self.save()
        self.wait(lambda:'READY ' in (self.output/'compositor.log').read_text(),[self.graph,self.policy_unit],'headless compositor')
        expected=(self.gui/'usr/libexec/aetheri-audio-lab/test_compositor_server').stat()
        observed=(Path('/proc')/str(self.server.pid)/'exe').stat()
        assert not select.select([self.server_pin],[],[],0)[0] and (expected.st_dev,expected.st_ino)==(observed.st_dev,observed.st_ino)
        self.record['gui_compositor'].update(state='original-running',ordinary=ordinary(self.server.pid));self.save()
        if self.policy_profile=='lua55-candidate':
            maps=(Path('/proc')/str(self.server.pid)/'maps').read_text()
            lua={line.split()[-1] for line in maps.splitlines() if 'liblua-' in line}
            inputs={line.split()[-1] for line in maps.splitlines() if 'libinput.so.' in line}
            assert lua=={str(self.gui/'usr/lib64/liblua-5.4.so')} and inputs=={str(self.gui/'usr/lib64/libinput.so.10')},(lua,inputs)
            assert not select.select([self.server_pin],[],[],0)[0]
            (self.output/'compositor-ready-maps.txt').write_text(maps)
            self.record['gui_compositor']['libinput_lua54_observed']=True;self.save()
        self.gui_unit='aetheri-audio-'+self.token+'-gui.service';self.pam=Path('/etc/pam.d')/('aetheri-audio-gui-'+self.token)
        self.old_vt=Path('/sys/class/tty/tty0/active').read_text().strip();assert re.fullmatch('tty[0-9]+',self.old_vt)
        content=b'auth required pam_permit.so\naccount required pam_permit.so\nsession required pam_systemd.so type=wayland class=user\n'
        with self.pam.open('xb') as f:f.write(content);f.flush();os.fsync(f.fileno())
        self.pam.chmod(0o444);run('restorecon',str(self.pam));st=self.pam.stat();self.pam_object=(st.st_dev,st.st_ino);self.pam_bytes=content
        self.profile=self.root/'gui-profile';data=('AETHAUI1\nseat0\n'+str(self.remote)+'\n'+self.graph+'\n'+self.policy_unit+'\n'+self.context('graph')+'\n'+self.context('policy')+'\n').encode()
        with self.profile.open('xb') as f:f.write(data);f.flush();os.fsync(f.fileno())
        self.profile.chmod(0o444);label(self.profile,'etc_t')
        # SELinux file_receive maps descriptors lacking READ/WRITE (O_PATH)
        # to FILE__IOCTL on this carrier. Carry readonly views of the exact
        # selected image objects; native fstat/current-image binding is unchanged.
        self.input_fds=[os.dup(self.units[self.graph]['pin']),os.dup(self.units[self.policy_unit]['pin']),os.open(self.profile,os.O_RDONLY|os.O_CLOEXEC),
            os.open('/proc/'+self.manager_facts['MainPID']+'/exe',os.O_RDONLY|os.O_CLOEXEC),os.open(self.images['graph'],os.O_RDONLY|os.O_CLOEXEC|os.O_NOFOLLOW),os.open(self.images['policy'],os.O_RDONLY|os.O_CLOEXEC|os.O_NOFOLLOW)]
        self.input_objects=[(os.fstat(fd).st_dev,os.fstat(fd).st_ino) for fd in self.input_fds]
        self.record['gui']=dict(unit=self.gui_unit,state='prepared',pam=str(self.pam),pam_object=list(self.pam_object),profile_sha256=hashlib.sha256(data).hexdigest(),original_inputs=[list(x) for x in self.input_objects],old_vt=self.old_vt);self.save()
        run('chvt','8');self.original_sources()
        gui_env={**self.audio_env,'LD_LIBRARY_PATH':str(self.gui/'usr/lib64'),'XDG_RUNTIME_DIR':'/run/user/1000','DBUS_SESSION_BUS_ADDRESS':'unix:path=/run/user/1000/bus','WAYLAND_DISPLAY':str(runtime/'aetheri-sound-live'),'NEXAUI_PLATFORM':'wayland','XDG_SESSION_TYPE':'wayland','XDG_SESSION_CLASS':'user','XDG_SEAT':'seat0','XDG_VTNR':'8'}
        binary=str(self.gui/'usr/libexec/aetheri-audio-lab/probe_settings_audio_live')
        # Use the carrier's existing ordinary unconfined service transition.
        # Only this independently selected disposable ELF is relabelled.
        label(Path(binary),'bin_t')
        props=[('Description','s','Aetheri explicit live Sound GUI'),('Type','s','exec'),('User','s','1000'),('Group','s','1000'),('PAMName','s',self.pam.name),('TTYPath','s','/dev/tty8'),('StandardInput','s','tty'),
            ('NoNewPrivileges','b',True),('CapabilityBoundingSet','t',0),('AmbientCapabilities','t',0),('Restart','s','no'),('KillMode','s','control-group'),('TimeoutStopUSec','t',5000000),
            ('StandardOutput','s','journal'),('StandardError','s','journal'),('SyslogIdentifier','s',self.gui_unit),('Environment','as',[k+'='+v for k,v in gui_env.items()]),
            ('ExtraFileDescriptors','a(hs)',[(fd,'aetheri-'+str(i+3)) for i,fd in enumerate(self.input_fds)]),('ExecStart','a(sasb)',[(binary,[binary,'--font',str(self.gui/'usr/share/aetheri-audio-lab/font.ttf'),'--diagnostics'],False)])]
        # Root-only PID1 administrative FD delivery avoids routing protected
        # image handles through the unrelated system-bus broker. Keep references
        # on the ordinary system bus; this carrier rejects private RefUnit.
        self.effect('gui-start',self.gui_unit,lambda:self.root_create_bus.create(self.gui_unit,props));self.effect('gui-ref',self.gui_unit,lambda:self.root_bus.unit('RefUnit',self.gui_unit))
        deadline=time.monotonic()+8
        while True:
            current=facts(self.gui_unit)
            if current['ActiveState']=='active' and current['MainPID']!='0' and not current['Job']:break
            if current['ActiveState']=='failed' or time.monotonic()>deadline:raise RuntimeError('GUI startup: '+self.gui_log())
            time.sleep(.025)
        self.gui_facts=current;self.gui_pid=int(current['MainPID']);self.gui_pin=os.pidfd_open(self.gui_pid)
        actual=Path('/proc')/str(self.gui_pid)/'exe';expected=Path(binary).stat();observed=actual.stat()
        assert (observed.st_dev,observed.st_ino)==(expected.st_dev,expected.st_ino)
        context=(Path('/proc')/str(self.gui_pid)/'attr/current').read_bytes().rstrip(b'\0\n').decode()
        assert context=='system_u:system_r:unconfined_service_t:s0',context
        assert not select.select([self.gui_pin],[],[],0)[0] and facts(self.gui_unit)['InvocationID']==current['InvocationID']
        self.record['gui'].update(state='original-running',pid=self.gui_pid,pidfd_inode=os.fstat(self.gui_pin).st_ino,facts=current,ordinary=ordinary(self.gui_pid));self.save()
        text=self.gui_wait(lambda t:'AETHERI_SETTINGS_AUDIO_OPEN original=1' in t and re.search(r'^AUDIO state=1 .*count=1 ',t,re.M),'original owner and live output')
        session=re.findall(r'^AETHERI_SETTINGS_AUDIO_SESSION id=(\S+)$',text,re.M);assert len(session)==1
        login=dict(line.split('=',1) for line in run('loginctl','show-session',session[0],'-p','User','-p','Active','-p','Remote','-p','Seat','-p','Type','-p','Class','-p','Scope','-p','TTY').splitlines())
        assert all(login[k]==v for k,v in {'User':'1000','Active':'yes','Remote':'no','Seat':'seat0','Type':'wayland','Class':'user','TTY':'tty8'}.items()),login
        self.session=session[0];self.scope=login['Scope'];self.effect('gui-scope-ref',self.scope,lambda:self.root_bus.unit('RefUnit',self.scope))
        self.gui_groups={};self.gui_group_facts={}
        for unit in (self.gui_unit,self.scope):
            observed=facts(unit);path=Path('/sys/fs/cgroup'+observed['ControlGroup']);fd=os.open(path,os.O_RDONLY|os.O_DIRECTORY|os.O_CLOEXEC);events=os.open('cgroup.events',os.O_RDONLY|os.O_CLOEXEC,dir_fd=fd)
            self.gui_groups[unit]=(path,fd,events,(os.fstat(fd).st_dev,os.fstat(fd).st_ino));self.gui_group_facts[unit]=observed
        self.record['gui'].update(session=self.session,scope=self.scope,groups={n:dict(path=str(v[0]),object=list(v[3]),facts=self.gui_group_facts[n]) for n,v in self.gui_groups.items()});self.save()

    def event(self,value):
        self.input.sendall(value.encode());assert self.input.recv(32)==b'OK',value

    def key(self,code):self.event('K '+str(code)+' 1');self.event('K '+str(code)+' 0')

    def rect(self,identifier):
        rows=re.findall(r'^CONTROL '+str(identifier)+r' ([\d.-]+) ([\d.-]+) ([\d.-]+) ([\d.-]+) (\d+)$',current_frame(self.gui_log()),re.M)
        assert rows,identifier;return tuple(map(float,rows[-1][:4])),int(rows[-1][4])&1

    def enabled(self,identifier):
        rows=re.findall(r'^CONTROL '+str(identifier)+r' [\d.-]+ [\d.-]+ [\d.-]+ [\d.-]+ (\d+)$',current_frame(self.gui_log()),re.M)
        return bool(rows) and not (int(rows[-1])&1)

    def fenced(self,text):
        frame=current_frame(text)
        rows=re.findall(r'^AUDIO state=\d+ revision=\d+ count=\d+ feedback=(\d+) stale=\d+ fenced=(\d+)$',frame,re.M)
        return bool(rows) and rows[-1]==('5','1') and not any(re.search(r'^CONTROL '+str(self.row+i)+r' ',frame,re.M) for i in range(3))

    def focus(self,identifier):
        def current():
            values=re.findall(r'^FOCUS (\d+)$',self.gui_log(),re.M)
            return int(values[-1]) if values else None
        for _ in range(24):
            previous=current()
            if previous==identifier:return
            self.key(15);self.gui_wait(lambda t:current()!=previous,'keyboard focus transition')
        raise AssertionError('volume control absent from keyboard focus order')

    def click(self,identifier,ratio=.5):
        (x,y,w,h),disabled=self.rect(identifier);assert not disabled,identifier
        self.event(f'M {40+x+w*ratio} {40+y+h/2}');self.event('B 272 1');self.event('B 272 0')

    def observed_output(self):
        rows=re.findall(r'^AUDIO_OUTPUT id=(\d+) default=(\d+) available=(\d+) volume=([\d.]+) muted=(\d+)$',self.gui_log(),re.M)
        return (int(rows[-1][0]),int(rows[-1][1]),int(rows[-1][2]),float(rows[-1][3]),int(rows[-1][4])&1) if rows else None

    def dump(self,label):
        self.original_sources()
        p=subprocess.run([*self.bus.base,str(self.sdk/'usr/bin/pw-dump'),'--remote',str(self.remote)],env=self.audio_env,text=True,capture_output=True,timeout=4)
        (self.output/(label+'.stderr')).write_text(p.stderr)
        assert p.returncode==0 and dump_diagnostics(p.stderr),(p.returncode,p.stderr)
        (self.output/(label+'.json')).write_text(p.stdout)
        values=[v for v in json.loads(p.stdout) if v['type']=='PipeWire:Interface:Node' and v.get('info',{}).get('props',{}).get('node.name')=='aetheri.test.sink'];assert len(values)==1
        return values[0]['info']['params']['Props']

    def capture(self,name):
        def settled(text):
            models=re.findall(r'^MODEL theme=(\d+) german=(\d+) text=(\d+) page=(\d+)$',text,re.M)
            done=re.findall(r'^SETTLED theme=(\d+) german=(\d+) text=(\d+) page=(\d+) (\d+) (\d+)$',text,re.M)
            frames=re.findall(r'^FRAME (\d+) (\d+) ',text,re.M)
            return models and done and frames and done[-1][:4]==models[-1] and done[-1][4:]==frames[-1]
        self.gui_wait(settled,name+' stable presentation')
        self.sequence=getattr(self,'sequence',0)+1;signal.pidfd_send_signal(self.server_pin,signal.SIGUSR1)
        self.wait(lambda:all(f'CAPTURE {self.sequence} {i}\n' in (self.output/'compositor.log').read_text() for i in range(3)),[],name+' capture')
        for i in range(3):
            p=self.captures/f'capture-{self.sequence}-output-{i}.ppm';raw=p.read_bytes();magic,dimensions,peak,pixels=raw.split(b'\n',3);w,h=map(int,dimensions.split());assert magic==b'P6' and peak==b'255' and len(pixels)==w*h*3
            if i==0:assert len({pixels[(y*w+x)*3:(y*w+x)*3+3] for y in range(0,h,4) for x in range(0,w,4)})>10
            packed=gzip.compress(raw,mtime=0);target=self.output/(name+'-'+str(i)+'.ppm.gz');target.write_bytes(packed)
            assert gzip.decompress(packed)==raw;p.unlink()
            self.record.setdefault('captures',[]).append(dict(name=name,output=i,file=target.name,raw_sha256=hashlib.sha256(raw).hexdigest(),sha256=hashlib.sha256(packed).hexdigest()))
        self.save()

    def exercise(self):
        self.begin_audio();self.begin_gui();self.click(53)
        self.gui_wait(lambda t:re.search(r'^MODEL .* page=3$',t,re.M) is not None,'Sound page')
        row=self.observed_output();assert row and row[2] and not row[4];self.row=row[0]
        if not row[1]:
            self.click(self.row);self.gui_wait(lambda t:self.observed_output()[1]==1,'observed default output')
        self.gui_wait(lambda t:self.enabled(self.row+1),'enabled volume control')
        self.policy_lua_ready(self.policy_unit)
        if self.policy_profile=='lua55-candidate':
            # The GUI needs the original C core/APIs, not an embedded Lua engine.
            assert not select.select([self.gui_pin],[],[],0)[0]
            maps=(Path('/proc')/str(self.gui_pid)/'maps').read_text()
            assert not any('liblua-' in line for line in maps.splitlines())
            assert facts(self.gui_unit)['InvocationID']==self.gui_facts['InvocationID']
            (self.output/'gui-ready-maps.txt').write_text(maps)
            self.record['gui']['no_lua_runtime_mapped']=True;self.save()
        self.capture('live-output')
        # Change through a real pointer gesture, then one keyboard decrement.
        initial=self.observed_output()[3];self.click(self.row+1,.5)
        self.gui_wait(lambda t:abs(self.observed_output()[3]-initial)>.1 and self.enabled(self.row+1),'first pointer readback')
        before=self.observed_output()[3];self.focus(self.row+1);self.key(105)
        self.gui_wait(lambda t:abs(self.observed_output()[3]-(before-.01))<.00001 and self.enabled(self.row+1),'keyboard volume readback')
        observed=self.observed_output()[3];props=self.dump('volume-readback');levels=[p['channelVolumes'] for p in props if 'channelVolumes' in p]
        assert len(levels)==1 and all(abs(v-observed**3)<.0001 for v in levels[0]),(observed,props)
        self.record['volume']=dict(ui=observed,raw=levels[0]);self.save();self.capture('volume-changed')
        self.click(self.row+2);self.gui_wait(lambda t:self.observed_output()[4]==1 and self.enabled(self.row+2),'mute readback')
        props=self.dump('mute-readback');assert any(p.get('mute') is True for p in props),props
        self.record['muted']=True;self.save();self.capture('muted')
        self.click(self.row+2);self.gui_wait(lambda t:self.observed_output()[4]==0 and self.enabled(self.row+2),'unmute readback')
        props=self.dump('unmute-readback');assert any(p.get('mute') is False for p in props),props
        self.original_sources();self.stop(self.policy_unit);self.stop(self.graph)
        self.gui_wait(self.fenced,'original-source loss fences UI')
        self.capture('sources-lost');self.record['sources_lost_fenced']=True;self.save()
        self.event('K 29 1');self.key(16);self.event('K 29 0')
        self.finish_gui();self.retire_extra_fds();self.retire_capture()

    def finish_gui(self):
        deadline=time.monotonic()+15
        while not select.select([self.gui_pin],[],[],0)[0] or facts(self.gui_unit)['ActiveState']!='inactive' or facts(self.gui_unit)['Job']:
            assert time.monotonic()<deadline,facts(self.gui_unit);time.sleep(.025)
        final=facts(self.gui_unit);assert final['InvocationID']==self.gui_facts['InvocationID'] and final['MainPID']==final['ControlPID']=='0' and final['ExecMainStatus']=='0',final
        text=self.gui_log();assert 'AETHERI_SETTINGS_AUDIO_RETIRED local_observer_only=1' in text
        (self.output/'gui.log').write_text(text)
        for unit,(path,fd,events,identity) in self.gui_groups.items():
            assert (os.fstat(fd).st_dev,os.fstat(fd).st_ino)==identity
            try:
                raw=os.pread(events,4096,0).decode();assert path.is_dir() and (path.stat().st_dev,path.stat().st_ino)==identity and 'populated 0' in raw
                end='same-original-empty'
            except OSError as error:
                assert error.errno==errno.ENODEV and not path.exists();end='missing-name-and-original-reader-ENODEV'
            now=facts(unit);assert now['InvocationID']==self.gui_group_facts[unit]['InvocationID'] and now['ActiveState']=='inactive' and not now['Job'],now
            self.record['gui']['groups'][unit]['end']=end;os.close(events);os.close(fd)
        deadline=time.monotonic()+15
        while subprocess.run(['loginctl','show-session',self.session],capture_output=True).returncode==0:
            assert time.monotonic()<deadline,'original session remains';time.sleep(.025)
        self.effect('gui-scope-unref',self.scope,lambda:self.root_bus.unit('UnrefUnit',self.scope));self.effect('gui-unref',self.gui_unit,lambda:self.root_bus.unit('UnrefUnit',self.gui_unit))
        deadline=time.monotonic()+5
        while facts(self.gui_unit)['LoadState']!='not-found':assert time.monotonic()<deadline;time.sleep(.025)
        for fd,identity in zip(self.input_fds,self.input_objects):
            assert (os.fstat(fd).st_dev,os.fstat(fd).st_ino)==identity
            for p in Path('/proc/1/fd').iterdir():
                try:current=p.stat()
                except FileNotFoundError:continue
                assert (current.st_dev,current.st_ino)!=identity,'PID1 retains original extra input'
            os.close(fd)
        self.input_fds.clear();os.close(self.gui_pin);self.gui_pin=None;self.root_bus.close();self.root_create_bus.close()
        assert (self.pam.stat().st_dev,self.pam.stat().st_ino)==self.pam_object and self.pam.stat().st_uid==0 and self.pam.read_bytes()==self.pam_bytes;self.pam.unlink();run('chvt',self.old_vt[3:])
        signal.pidfd_send_signal(self.server_pin,signal.SIGTERM);assert self.server.wait(timeout=8)==0 and select.select([self.server_pin],[],[],0)[0]
        os.close(self.server_pin);self.server_pin=None;self.input.close();self.server_log.close();self.record['gui_compositor']['state']='original-ended'
        self.record['gui'].update(state='original-ended',final=final,extra_inputs_retired=True);self.save()


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('sdk','policy','control','binary','source','gui','output'):parser.add_argument(name,type=Path)
    parser.add_argument('--policy-profile',choices=('rpm','lua55-candidate'),default='rpm')
    args=parser.parse_args()
    if not __debug__ or os.geteuid()!=0 or Path('/sys/class/dmi/id/product_name').read_text().strip()!='Aetheri audio lab VM' or Path('/sys/fs/selinux/enforce').read_text().strip()!='1':raise ValueError('explicit unoptimized root-only enforcing audio lab required')
    if list(Path('/etc/pam.d').glob('aetheri-audio-*')) or run('systemctl','list-units','--all','--plain','--no-legend','aetheri-audio-*'):raise ValueError('foreign/pending test actors; reconcile first')
    from test_audio import check_policy
    prepare_audio.check(args.sdk);check_policy(args.policy,args.policy_profile);prepare_audio_control.check(args.control)
    check_binaries(args.binary)
    prepare_settings_audio_lab.check(args.gui,json.loads((args.source/'spec/settings-audio-lab-inputs.json').read_text()))
    fixture=Sound(args.output)
    try:
        fixture.setup(args.sdk,args.policy,args.control,args.binary,args.source,args.gui,policy_profile=args.policy_profile);fixture.exercise();fixture.retire();print(json.dumps(fixture.record),flush=True)
    except BaseException as error:
        fixture.record.update(state='needs-inspection',error=repr(error),traceback=traceback.format_exc(),keeper_pid=os.getpid());fixture.save();print(json.dumps(fixture.record),flush=True)
        # Retain original bus/process/cgroup/input handles. No retry, stop,
        # policy removal or inferred drain follows a failed/unknown test step.
        while True:signal.pause()


if __name__=='__main__':main()
