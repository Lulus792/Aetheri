"""Explicit root-only audio-lab role test; no normal service/app provisioning.

The private carrier has no user data or quarantined runtime. The original P0
guest is refused. Unknown dispatches remain pending; never replay them. The
driver keeps its original bus references/process/cgroup handles until retirement.
"""
import argparse
import ctypes
import fcntl
import hashlib
import json
import os
from pathlib import Path
import select
import shutil
import signal
import socket
import stat
import subprocess
import time
import uuid
import bz2
import image_state

from audio_mac_bus import Bus
import prepare_audio,prepare_audio_policy,prepare_audio_control
from test_audio import validate_pcm,check_policy

ROLES={r:'aetheri_audio_'+r+'_test_t' for r in ('graph','policy','control','player','foreign')}


def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def run(*args):return subprocess.check_output(args,text=True,stderr=subprocess.STDOUT,timeout=55 if args[0]=='semodule' else 20).strip()
def label(path,kind):run('chcon','-h','system_u:object_r:'+kind+':s0',str(path))
def facts(unit):
    keys='Id,LoadState,ActiveState,SubState,MainPID,ControlPID,InvocationID,ControlGroup,Job,ExecMainStatus'
    return dict(line.split('=',1) for line in run('systemctl','show',unit,'--property='+keys).splitlines())


class Fixture:
    def __init__(self,output):
        self.output=output;output.mkdir(mode=0o700);self.token=uuid.uuid4().hex
        self.root=Path('/run/a-audio-'+self.token[:12]);self.bus=Bus();self.units={};self.pending=None
        self.extra_pins={};self.facts=facts;self.supervisor_pid=1
        self.capture_reader=None;self.capture_object=None
        self.pump=lambda:None
        self.record=dict(token=self.token,boot=Path('/proc/sys/kernel/random/boot_id').read_text().strip(),root=str(self.root),units={},pending=None,state='preparing')
        self.save()

    def context(self,role):return 'system_u:system_r:'+ROLES[role]+':s0'
    def memory_context(self,role):return 'system_u:object_r:aetheri_audio_'+role+'_mem_test_t:s0'

    def save(self):
        self.record['pending']=self.pending
        image_state.write(self.output/'controller.json',self.record)

    def effect(self,kind,unit,call):
        if self.pending is not None:raise RuntimeError('unknown earlier effect; preserve')
        self.pending=dict(kind=kind,unit=unit);self.save()
        result=call() # Any lost acknowledgement preserves pending; never replay.
        self.pending=None;self.save();return result

    def start(self,case,role,args,env,pin=None,neutral=False):
        name='aetheri-audio-'+self.token+'-'+case+'-'+role+'.service'
        if self.facts(name)['LoadState']!='not-found':raise RuntimeError('foreign fixture unit')
        log=self.output/(case+'-'+role+'.log');stream=log.open('xb')
        if not neutral:label(log,'aetheri_audio_log_test_t')
        props=[('User','s','1000'),('Group','s','1000'),('NoNewPrivileges','b',True),('CapabilityBoundingSet','t',0),('AmbientCapabilities','t',0),
            ('Type','s','exec'),('RemainAfterExit','b',True),('Restart','s','no'),('KillMode','s','control-group'),('TimeoutStopUSec','t',5000000),
            ('StandardOutputFileDescriptor','h',stream.fileno()),('StandardErrorFileDescriptor','h',stream.fileno()),
            ('Environment','as',[k+'='+v for k,v in env.items()]),('ExecStart','a(sasb)',[(args[0],args,False)])]
        if not neutral:
            props+=[('SELinuxContext','s',self.context(role)),('PrivateDevices','b',True),('ProtectHome','s','yes'),('ProtectSystem','s','strict'),('ReadWritePaths','as',[str(self.root)])]
        if pin is not None:props.append(('ExtraFileDescriptors','a(hs)',[(pin,'original-graph')]))
        if pin is not None:
            source=os.fstat(pin);identity=(source.st_dev,source.st_ino)
            if identity not in self.extra_pins:self.extra_pins[identity]=os.dup(pin)
        self.units[name]=dict(log=log,role=role,pin=None,group=None,events=None,neutral=neutral)
        self.record['units'][name]=dict(role=role,state='requesting',arguments=args,log=str(log))
        if pin is not None:self.record['units'][name]['original_extra_object']=list(identity)
        self.save()
        try:job=self.effect('start',name,lambda:self.bus.create(name,props))
        finally:stream.close()
        self.record['units'][name]['job']=job
        self.effect('ref',name,lambda:self.bus.unit('RefUnit',name))
        self.record['units'][name]['referenced']=True;self.save()
        deadline=time.monotonic()+8
        while time.monotonic()<deadline:
            self.pump()
            value=self.facts(name)
            if value['ActiveState']=='failed':raise RuntimeError('fixture startup failed: '+name+' '+log.read_text(errors='replace'))
            if value['ActiveState']=='active' and value['SubState']=='running' and value['MainPID']!='0' and not value['Job']:break
            time.sleep(.025)
        else:raise TimeoutError('fixture original startup')
        pid=int(value['MainPID']);handle=os.pidfd_open(pid)
        self.units[name]['pin']=handle
        proc=Path('/proc')/str(pid)
        status=dict(line.split(':',1) for line in (proc/'status').read_text().splitlines() if ':' in line)
        assert set(status['Uid'].split())=={'1000'} and set(status['Gid'].split())=={'1000'}
        assert all(int(status[k],16)==0 for k in ('CapEff','CapPrm','CapInh','CapAmb')) and status['NoNewPrivs'].strip()=='1'
        context=(proc/'attr/current').read_bytes().removesuffix(b'\0').decode().strip()
        if not neutral:assert context==self.context(role),context
        assert self.facts(name)==value and not select.select([handle],[],[],0)[0]
        assert value['InvocationID'] and int(value['InvocationID'],16)
        group=Path('/sys/fs/cgroup'+value['ControlGroup']);directory=os.open(group,os.O_RDONLY|os.O_DIRECTORY|os.O_CLOEXEC)
        events=os.open('cgroup.events',os.O_RDONLY|os.O_CLOEXEC,dir_fd=directory)
        self.units[name].update(group=directory,events=events,facts=value,pid=pid)
        self.record['units'][name].update(state='original-active',pid=pid,invocation=value['InvocationID'],cgroup=value['ControlGroup'],cgroup_inode=os.fstat(directory).st_ino,label=context)
        if pin is not None:
            original=os.fstat(pin);passed=(proc/'fd/3').stat()
            assert (original.st_dev,original.st_ino)==(passed.st_dev,passed.st_ino),'different passed FD3'
            self.units[name]['extra_object']=(original.st_dev,original.st_ino)
            self.record['units'][name]['extra_fd3_object']=list(self.units[name]['extra_object'])
        self.save();return name

    def release(self,name):
        unit=self.units[name]
        self.wait(lambda:'AETHERI_AUDIO_MAC_READY ' in unit['log'].read_text(errors='replace'),[name],'gateway barrier')
        assert self.facts(name)['InvocationID']==unit['facts']['InvocationID'] and not select.select([unit['pin']],[],[],0)[0]
        signal.pidfd_send_signal(unit['pin'],signal.SIGUSR1)

    def wait(self,predicate,names,description,seconds=12):
        deadline=time.monotonic()+seconds
        while not predicate():
            self.pump()
            for name in names:
                pin=self.units[name]['pin']
                if pin is not None and select.select([pin],[],[],0)[0]:raise RuntimeError(description+' early exit: '+self.units[name]['log'].read_text(errors='replace'))
            if time.monotonic()>=deadline:raise TimeoutError(description)
            time.sleep(.01)

    def finished(self,name):
        unit=self.units[name];deadline=time.monotonic()+20
        while True:
            self.pump();value=self.facts(name)
            if select.select([unit['pin']],[],[],0)[0] and value['MainPID']=='0':break
            if time.monotonic()>deadline:raise TimeoutError('original finite actor')
            time.sleep(.01)
        assert value['InvocationID']==unit['facts']['InvocationID'] and value['ExecMainStatus']=='0' and value['MainPID']=='0',value
        self.record['units'][name]['finite_exit']=0;self.save()

    def stop(self,name,request=True):
        unit=self.units[name]
        if 'facts' not in unit:raise RuntimeError('unadmitted unit needs reconciliation')
        assert self.facts(name)['InvocationID']==unit['facts']['InvocationID']
        if request:self.effect('stop',name,lambda:self.bus.stop(name))
        deadline=time.monotonic()+8
        while time.monotonic()<deadline:
            value=self.facts(name)
            if value['ActiveState'] in ('inactive','failed') and value['MainPID']==value['ControlPID']=='0' and not value['Job'] and select.select([unit['pin']],[],[],0)[0]:break
            time.sleep(.025)
        else:raise TimeoutError('original unit not terminal')
        # Actual deactivation of the held original cgroup is separate from a
        # missing name. This carrier's events FD returns ENODEV after removal.
        import errno
        deadline=time.monotonic()+5
        while True:
            retired=False
            try:os.pread(unit['events'],4096,0)
            except OSError as error:
                if error.errno!=errno.ENODEV:raise
                retired=True
            if retired and not Path('/sys/fs/cgroup'+unit['facts']['ControlGroup']).exists():break
            if time.monotonic()>deadline:raise RuntimeError('original held cgroup remains active')
            time.sleep(.01)
        for key in ('pin','events','group'):os.close(unit[key]);unit[key]=None
        if value['ActiveState']=='failed':self.effect('reset-failed',name,lambda:self.bus.unit('ResetFailedUnit',name))
        self.effect('unref',name,lambda:self.bus.unit('UnrefUnit',name))
        deadline=time.monotonic()+5
        while self.facts(name)['LoadState']!='not-found':
            if time.monotonic()>deadline:raise TimeoutError('original unit GC/extra-FD retirement')
            time.sleep(.025)
        # PID1 also retains its independent supervisory pidfd while the source
        # graph is live. Same pidfs inode does not distinguish those descriptions.
        # Keep our own identity pin and verify absence after ALL source/client
        # units are collected, rather than falsely demanding source drain here.
        if 'extra_object' in unit:self.record['units'][name]['extra_fd_check']='deferred-until-source-retirement'
        self.record['units'][name]['state']='retired';self.save()

    def retire_extra_fds(self):
        assert all(v['state']=='retired' for v in self.record['units'].values())
        for identity,pin in self.extra_pins.items():
            assert (os.fstat(pin).st_dev,os.fstat(pin).st_ino)==identity
            for descriptor in Path('/proc/'+str(self.supervisor_pid)+'/fd').iterdir():
                try:held=descriptor.stat()
                except FileNotFoundError:continue
                if (held.st_dev,held.st_ino)==identity:raise RuntimeError('PID1 retains original object after source/client unit GC')
            for name,unit in self.units.items():
                if unit.get('extra_object')==identity:self.record['units'][name]['extra_fd_check']='absent-after-source-retirement'
            os.close(pin)
        self.extra_pins.clear();self.save()


def configuration(root,sdk,policy):
    conf=root/'conf';conf.mkdir();wp=conf/'wp';wp.mkdir();fragments=wp/'wireplumber.conf.d';fragments.mkdir()
    (wp/'wireplumber.conf').write_bytes(b'context.properties = { mem.allow-mlock = false support.dbus = false }\n'+(policy/'usr/share/wireplumber/wireplumber.conf').read_bytes())
    (fragments/'90-aetheri-test.conf').write_text('wireplumber.profiles = { aetheri-policy-test = { inherits = [ main-embedded ] hardware.audio = disabled hardware.bluetooth = disabled hardware.video-capture = disabled support.dbus = disabled support.system-dbus = disabled api.notifications = disabled } }\n')
    modules=conf/'modules';modules.mkdir()
    for directory in (sdk/'usr/lib64/pipewire-0.3',policy/'usr/lib64/pipewire-0.3'):
        for path in directory.glob('*.so'):(modules/path.name).symlink_to(path)
    common='context.spa-libs = { support.* = support/libspa-support audio.convert.* = audioconvert/libspa-audioconvert }\n'
    (conf/'client.conf').write_text(common+'context.properties = { mem.allow-mlock = false support.dbus = false }\ncontext.modules = [ { name = libpipewire-module-protocol-native } { name = libpipewire-module-metadata } { name = libpipewire-module-client-node } { name = libpipewire-module-adapter } ]\n')
    return conf,common


class Audio(Fixture):
    def retire_capture(self):
        # Failure paths retain this bounded observer until original actors and
        # uncertain effects are independently settled. It is not holder drain.
        if self.capture_reader is None:return
        assert self.pending is None and all(u['state']=='retired' for u in self.record['units'].values())
        st=os.fstat(self.capture_reader);assert (st.st_dev,st.st_ino)==self.capture_object
        os.close(self.capture_reader);self.capture_reader=None;self.capture_object=None;self.pump=lambda:None

    def setup(self,sdk,policy,control,binary,source,policy_profile='rpm'):
        prepare_audio.check(sdk);check_policy(policy,policy_profile);prepare_audio_control.check(control)
        self.policy_profile=policy_profile
        self.record.update(policy_profile=policy_profile,policy_receipt_sha256=sha(policy/'inputs.json'));self.save()
        if Path('/sys/fs/selinux/policy_capabilities/memfd_class').read_text().strip()!='0':raise RuntimeError('different memfd policy class; review before dispatch')
        self.record['modules_before']=run('semodule','-lfull')
        if any(line.split()[1]=='audio-test' for line in self.record['modules_before'].splitlines()):raise RuntimeError('foreign/pending audio module; inspect first')
        self.record['policy_sha256']=sha(source/'policy/audio-test.cil');self.save()
        self.root.mkdir(mode=0o700)
        self.effect('mount',str(self.root),lambda:run('mount','-t','tmpfs','-o','size=256M,mode=0755','aetheri-audio-'+self.token,str(self.root)))
        self.root_object=(self.root.stat().st_dev,self.root.stat().st_ino)
        for name,path in (('sdk',sdk),('policy',policy),('control',control)):
            shutil.copytree(path,self.root/name,symlinks=True)
        self.sdk=self.root/'sdk';self.policy=self.root/'policy';self.control=self.root/'control'
        code=self.root/'code';code.mkdir()
        self.images={name:code/name for name in ('gateway','control','player')}
        for name,filename in (('gateway','probe_audio_mac'),('control','aetheri-audio-control-probe'),('player','aetheri-audio-check')):
            shutil.copyfile(binary/filename,self.images[name]);self.images[name].chmod(0o555)
        self.images.update(graph=self.sdk/'usr/bin/pipewire',policy=self.policy/'usr/bin/wireplumber')
        self.record['images']={name:dict(path=str(path),sha256=sha(path)) for name,path in self.images.items()};self.save()
        self.effect('policy-install','audio-test',lambda:run('semodule','-X','400','-i',str(source/'policy/audio-test.cil')))
        stored=Path('/var/lib/selinux/targeted/active/modules/400/audio-test/cil')
        assert bz2.decompress(stored.read_bytes())==(source/'policy/audio-test.cil').read_bytes()
        self.module_bytes=stored.read_bytes()
        label(self.root,'aetheri_audio_fixture_test_t');label(code,'aetheri_audio_fixture_test_t')
        for prefix in (self.sdk,self.policy,self.control):
            for path in (prefix,*prefix.rglob('*')):
                label(path,'aetheri_audio_library_test_t')
                if path.is_dir() and not path.is_symlink():path.chmod(0o755)
        for name,path in self.images.items():
            label(path,'aetheri_audio_'+('gateway' if name=='gateway' else name+'_exec')+'_test_t')
        self.root.chmod(0o755)

    def memory_factories(self):
        for role in ('graph','policy','control','player'):
            name=self.start('memory',role,[str(self.images['gateway']),'memory'],{})
            self.release(name);self.finished(name)
            text=self.units[name]['log'].read_text()
            expected='MEMORY label='+self.memory_context(role)+' label_errno=0 resized=0 resize_errno=0'
            assert expected in text and 'MEMORY mapped=1 map_errno=0' in text,text
            self.stop(name)
            self.record.setdefault('memory_factories',{})[role]=text;self.save()

    def image(self,name,path):
        unit=self.units[name];proc=Path('/proc')/str(unit['pid'])
        self.wait(lambda:os.readlink(proc/'exe')==str(path),[name],'selected target image')
        assert not select.select([unit['pin']],[],[],0)[0]
        assert self.facts(name)['InvocationID']==unit['facts']['InvocationID']
        assert (proc/'attr/current').read_bytes().rstrip(b'\0\n').decode()==self.context(unit['role'])
        status=dict(line.split(':',1) for line in (proc/'status').read_text().splitlines() if ':' in line)
        assert set(status['Uid'].split())==set(status['Gid'].split())=={'1000'}
        assert all(int(status[k],16)==0 for k in ('CapEff','CapPrm','CapInh','CapAmb')) and status['NoNewPrivs'].strip()=='1'
        maps=(proc/'maps').read_text();(self.output/(name+'.maps')).write_text(maps)
        assert str(self.sdk/'usr/lib64/libpipewire-0.3.so.0.1611.0') in maps
        if unit['role'] in ('policy','control'):
            for library in (self.policy/'usr/lib64/libwireplumber-0.5.so.0.519.0',self.control/'usr/lib64/libglib-2.0.so.0.8800.0'):
                assert str(library) in maps
        self.record['units'][name]['observed_image']=dict(path=str(path),sha256=sha(path));self.save()

    def configure_case(self,root,fifo):
        conf,common=configuration(root,self.sdk,self.policy)
        (conf/'server.conf').write_text(common+'''context.properties = { core.daemon = true core.name = aetheri-audio-test default.clock.rate = 48000 default.clock.quantum = 1024 default.clock.min-quantum = 1024 default.clock.max-quantum = 1024 mem.allow-mlock = false support.dbus = false }
context.modules = [
 { name = libpipewire-module-protocol-native args = { sockets = [ { name = aetheri-audio-test mode = 0600 } ] } }
 { name = libpipewire-module-client-node }
 { name = libpipewire-module-adapter }
 { name = libpipewire-module-link-factory }
 { name = libpipewire-module-spa-node-factory }
 { name = libpipewire-module-metadata }
 { name = libpipewire-module-client-device }
 { name = libpipewire-module-access args = { access.legacy = false access.socket = { aetheri-audio-test = unrestricted } } }
 { name = libpipewire-module-pipe-tunnel args = { tunnel.mode = sink pipe.filename = '''+json.dumps(str(fifo))+''' audio.format = F32LE audio.rate = 48000 audio.channels = 2 audio.position = [ FL FR ] stream.props = { media.class = Audio/Sink node.virtual = true node.name = aetheri.test.sink adapter.auto-port-config = { mode = dsp position = preserve } } } }
]
context.objects = [ { factory = spa-node-factory args = { factory.name = support.node.driver node.name = Test-Driver priority.driver = 200000 } } ]
''')
        return conf

    def policy_lua_ready(self,name):
        # Re-observe the original actor without invoking UserAudio.image again:
        # that override creates separate once-only actor observers.
        Audio.image(self,name,self.images['policy'])
        unit=self.units[name];maps=(Path('/proc')/str(unit['pid'])/'maps').read_text()
        soname='liblua-5.5.so' if self.policy_profile=='lua55-candidate' else 'liblua-5.4.so'
        lua=self.policy/'usr/lib64'/soname
        scripting=self.policy/'usr/lib64/wireplumber-0.5/libwireplumber-module-lua-scripting.so'
        mapped={line.split()[-1] for line in maps.splitlines() if 'liblua-' in line}
        mapped_module={line.split()[-1] for line in maps.splitlines() if 'libwireplumber-module-lua-scripting.so' in line}
        assert mapped=={str(lua)} and mapped_module=={str(scripting)},(mapped,mapped_module)
        if self.policy_profile=='lua55-candidate':
            import prepare_audio_policy_candidate as candidate
            _,entries,_=candidate.profile()
        else:
            pins,_=prepare_audio_policy.profile()
            entries={e['path']:e for e in pins['files']}
            entries.update({e['destination'].lstrip('/'):e for e in pins['existing_runtime']['files']})
        for p in (lua,scripting):assert sha(p)==entries[p.relative_to(self.policy).as_posix()]['sha256']
        assert not select.select([unit['pin']],[],[],0)[0]
        self.record['units'][name]['lua_ready']=dict(profile=self.policy_profile,lua=str(lua),lua_sha256=sha(lua),module=str(scripting),module_sha256=sha(scripting));self.save()

    def extra_environment(self,root,conf):return {}
    def signal_frames(self):return 48000
    def validate_case(self,case,raw):return validate_pcm(raw,self.signal_frames(),silent=case=='muted',gain=.125 if case=='half' else 1)

    def case(self,case):
        root=self.root/case;root.mkdir(mode=0o755);label(root,'aetheri_audio_fixture_test_t')
        runtime=root/'run';state=root/'state'
        for path,kind in ((runtime,'run'),(state,'state')):
            path.mkdir(mode=0o700);os.chown(path,1000,1000);label(path,'aetheri_audio_'+kind+'_test_t')
        fifo=root/'capture.fifo';os.mkfifo(fifo,0o600);os.chown(fifo,1000,1000);label(fifo,'aetheri_audio_capture_test_t')
        reader=os.open(fifo,os.O_RDONLY|os.O_NONBLOCK|os.O_CLOEXEC);captured=bytearray()
        self.capture_reader=reader;st=os.fstat(reader);self.capture_object=(st.st_dev,st.st_ino)
        def pump():
            for _ in range(8):
                try:raw=os.read(reader,65536)
                except BlockingIOError:break
                if not raw:break
                captured.extend(raw)
                assert len(captured)<=8*1024**2,'PCM budget'
        self.pump=pump
        conf=self.configure_case(root,fifo)
        for path in (conf,*conf.rglob('*')):label(path,'aetheri_audio_config_test_t')
        remote=runtime/'aetheri-audio-test'
        env=dict(PATH='/usr/bin:/bin',HOME=str(root),XDG_RUNTIME_DIR=str(runtime),XDG_CONFIG_HOME=str(conf),XDG_STATE_HOME=str(state),
            LD_LIBRARY_PATH=':'.join(str(p/'usr/lib64') for p in (self.control,self.policy,self.sdk)),PIPEWIRE_REMOTE=str(remote),
            PIPEWIRE_CONFIG_DIR=str(conf),PIPEWIRE_MODULE_DIR=str(conf/'modules'),SPA_PLUGIN_DIR=str(self.sdk/'usr/lib64/spa-0.2'),
            WIREPLUMBER_CONFIG_DIR=str(conf/'wp'),WIREPLUMBER_DATA_DIR=str(self.policy/'usr/share/wireplumber'),
            WIREPLUMBER_MODULE_DIR=str(self.policy/'usr/lib64/wireplumber-0.5'),GIO_USE_VFS='local',GIO_MODULE_DIR='/nonexistent')
        env.update(self.extra_environment(root,conf))
        def start(role,arguments,pin=None):
            unit=self.start(case,role,[str(self.images['gateway']),'exec','pin' if pin is not None else 'none',*map(str,arguments)],env,pin=pin)
            self.release(unit);self.image(unit,Path(arguments[0]));return unit
        graph=start('graph',[self.images['graph'],'-c','server.conf'])
        self.wait(remote.exists,[graph],'original graph socket')
        assert stat.S_ISSOCK(remote.stat().st_mode) and remote.stat().st_uid==1000 and stat.S_IMODE(remote.stat().st_mode)==0o600
        policy=start('policy',[self.images['policy'],'-p','aetheri-policy-test'])
        # Independent same-UID controls prove the listener and DAC paths work.
        secret=root/'secret';secret.write_bytes(b'fixture-secret\n');secret.chmod(0o644);label(secret,'aetheri_audio_secret_test_t')
        positive="import os,socket,sys;from pathlib import Path;assert os.getuid()==1000;assert Path(sys.argv[2]).read_bytes()==b'fixture-secret\\n';s=socket.socket(socket.AF_INET,socket.SOCK_STREAM);s.close();s=socket.socket(socket.AF_UNIX,socket.SOCK_STREAM);s.connect(sys.argv[1]);s.close();print('DAC_LISTENER_TCP_CONTROL_PASS')"
        base=['setpriv','--reuid=1000','--regid=1000','--clear-groups','--no-new-privs']
        control=subprocess.run([*base,'python3','-c',positive,str(remote),str(secret)],capture_output=True,text=True,timeout=5)
        (self.output/(case+'-dac-control.log')).write_text(control.stdout+control.stderr)
        assert control.returncode==0,control.stderr
        foreign=self.start(case,'foreign',[str(self.images['gateway']),'foreign',str(remote),str(secret)],env)
        self.release(foreign);self.finished(foreign)
        assert 'AETHERI_AUDIO_MAC_DENIED ' in self.units[foreign]['log'].read_text()
        self.stop(foreign)
        native=start('control',[self.images['control'],str(remote),'aetheri.test.sink',case,'--server-pidfd','3','--wait-for-start'],self.units[graph]['pin'])
        self.wait(lambda:'AETHERI_AUDIO_NATIVE_WAIT start=explicit' in self.units[native]['log'].read_text(),[graph,policy,native],'native image barrier')
        signal.pidfd_send_signal(self.units[native]['pin'],signal.SIGUSR1)
        self.finished(native)
        assert 'AETHERI_AUDIO_NATIVE_PASS case='+case in self.units[native]['log'].read_text()
        assert 'AETHERI_AUDIO_NATIVE_PEER original=1' in self.units[native]['log'].read_text()
        self.stop(native)
        player=start('player',[self.images['player'],'--remote',str(remote),'--frames',str(self.signal_frames()),'--wait-for-start','--autoconnect','--server-pidfd','3'],self.units[graph]['pin'])
        self.wait(lambda:'CONNECTED node=' in self.units[player]['log'].read_text(),[graph,policy,player],'player connected')
        def linked():
            pump()
            result=subprocess.run([*base,str(self.sdk/'usr/bin/pw-dump'),'--remote',str(remote)],env=env,capture_output=True,text=True,timeout=4)
            assert result.returncode==0,result.stderr
            objects=json.loads(result.stdout);nodes={o['id']:o.get('info',{}).get('props',{}).get('node.name') for o in objects if o['type']=='PipeWire:Interface:Node'}
            image_state.write(self.output/(case+'-graph.json'),dict(objects=objects))
            ports={}
            for o in objects:
                if o['type']!='PipeWire:Interface:Port':continue
                p=o['info']['props'];role=nodes.get(int(p.get('node.id',-1)));channel=p.get('audio.channel')
                if role in ('aetheri.audio.check','aetheri.test.sink') and channel in ('FL','FR'):
                    if p.get('port.direction')!=('out' if role=='aetheri.audio.check' else 'in'):continue
                    key=(role,channel);assert key not in ports;ports[key]=o['id']
            if len(ports)!=4:return False
            links=[o for o in objects if o['type']=='PipeWire:Interface:Link']
            expected={(ports[('aetheri.audio.check',c)],ports[('aetheri.test.sink',c)]) for c in ('FL','FR')}
            if {(o['info']['output-port-id'],o['info']['input-port-id']) for o in links}!=expected:return False
            clients={o['id']:o.get('info',{}).get('props',{}) for o in objects if o['type']=='PipeWire:Interface:Client'}
            assert all(str(clients[int(o['info']['props']['client.id'])]['pipewire.sec.pid'])==str(self.units[policy]['pid']) and str(clients[int(o['info']['props']['client.id'])]['pipewire.sec.uid'])=='1000' for o in links)
            (self.output/(case+'-linked.json')).write_text(result.stdout);return True
        self.wait(linked,[graph,policy,player],'original policy stereo links')
        self.policy_lua_ready(policy)
        assert not any(captured),'signal before explicit activation'
        signal.pidfd_send_signal(self.units[player]['pin'],signal.SIGUSR1)
        self.finished(player)
        assert 'DRAINED frames='+str(self.signal_frames()) in self.units[player]['log'].read_text()
        self.stop(player)
        for _ in range(30):pump();time.sleep(.01)
        self.stop(policy);self.stop(graph);self.retire_extra_fds();pump();self.retire_capture()
        raw=bytes(captured);(self.output/(case+'.f32le')).write_bytes(raw)
        result=self.validate_case(case,raw)
        self.record.setdefault('cases',{})[case]=result;self.save()

    def retire(self):
        assert self.pending is None and all(v['state']=='retired' for v in self.record['units'].values())
        assert not any(ROLES[r] in run('ps','-eZ') for r in ROLES)
        assert (self.root.stat().st_dev,self.root.stat().st_ino)==self.root_object
        self.effect('unmount',str(self.root),lambda:run('umount',str(self.root)));self.root.rmdir()
        for unit in self.units.values():label(unit['log'],'var_run_t')
        stored=Path('/var/lib/selinux/targeted/active/modules/400/audio-test/cil')
        assert stored.read_bytes()==self.module_bytes
        self.effect('policy-remove','audio-test',lambda:run('semodule','-X','400','-r','audio-test'))
        assert run('semodule','-lfull')==self.record['modules_before'] and run('getenforce')=='Enforcing'
        self.record['state']='passed-retired';self.save()


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('sdk','policy','control','binary','source','output'):parser.add_argument(name,type=Path)
    parser.add_argument('--policy-profile',choices=('rpm','lua55-candidate'),default='rpm')
    args=parser.parse_args()
    if os.geteuid()!=0 or Path('/sys/class/dmi/id/product_name').read_text().strip()!='Aetheri audio lab VM' or run('getenforce')!='Enforcing':raise RuntimeError('requires the separate registered enforcing audio lab')
    prepare_audio.check(args.sdk);check_policy(args.policy,args.policy_profile);prepare_audio_control.check(args.control)
    if run('systemctl','list-units','--all','--plain','--no-legend','aetheri-audio-*'):raise RuntimeError('prior units remain')
    fixture=Audio(args.output)
    try:
        fixture.setup(args.sdk,args.policy,args.control,args.binary,args.source,policy_profile=args.policy_profile)
        fixture.memory_factories()
        for case in ('unity','half','muted'):fixture.case(case)
        fixture.retire();print(json.dumps(fixture.record))
    except BaseException as error:
        fixture.record.update(state='needs-inspection',error=repr(error),keeper_pid=os.getpid());fixture.save()
        print(json.dumps(dict(state='needs-inspection',keeper_pid=os.getpid(),output=str(args.output),error=repr(error))),flush=True)
        # Preserve the original bus/process/cgroup handles and pending effects.
        # A later exact administrative reconciliation must decide retirement.
        while True:signal.pause()
    finally:fixture.bus.close()


if __name__=='__main__':main()
