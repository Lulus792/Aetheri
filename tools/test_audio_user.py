"""Confined audio roles under the real user manager, disposable audio lab only.

Administrative fixture, not normal service/app admission. Uses the existing
system-scope PCM/lifetime checks with an original user-bus worker and typed FD
transfer. Unknown effects remain pending; connection close never proves drain.
"""
import argparse
import bz2
import hashlib
import json
import os
from pathlib import Path
import select
import shutil
import socket
import subprocess
import sys
import time

from test_audio_mac import Audio,ROLES,facts,run,label

from audio_user_worker import packet,receive


class UserBus:
    def __init__(self,fixture):
        self.fixture=fixture;self.closed=False;parent,child=socket.socketpair(socket.AF_UNIX,socket.SOCK_SEQPACKET|socket.SOCK_CLOEXEC)
        self.channel=parent
        env={**os.environ,'PYTHONDONTWRITEBYTECODE':'1','XDG_RUNTIME_DIR':'/run/user/1000','DBUS_SESSION_BUS_ADDRESS':'unix:path=/run/user/1000/bus'}
        self.env=env;self.base=['setpriv','--reuid=1000','--regid=1000','--clear-groups','--no-new-privs']
        with (fixture.output/'user-worker.stderr').open('xb') as errors:
            self.process=subprocess.Popen([*self.base,'python3',str(fixture.root/'code/audio_user_worker.py'),'--worker',str(child.fileno())],env=env,pass_fds=(child.fileno(),),stdout=subprocess.DEVNULL,stderr=errors)
        child.close();self.pin=os.pidfd_open(self.process.pid)
        fixture.record['user_worker']=dict(pid=self.process.pid,pidfd_inode=os.fstat(self.pin).st_ino);fixture.save()
        value=self.reply();assert value['ready'];fixture.record['user_worker']['manager_owner']=value['owner'];fixture.save()

    def reply(self):
        if not select.select([self.channel],[],[],8)[0]:raise TimeoutError('original fixture worker reply; do not replay')
        value,fds=receive(self.channel)
        for fd in fds:os.close(fd)
        if fds:raise ValueError('unsolicited fixture reply descriptors')
        return value

    def call(self,op,unit=None,properties=None,member=None,fds=()):
        self.fixture.check_manager();packet(self.channel,dict(op=op,unit=unit,properties=properties,member=member),fds)
        reply=self.reply();assert reply['op']==op;self.fixture.check_manager();return reply['result']

    def create(self,name,properties):
        # This user profile uses MAC plus the existing cap-zero/NNP parent.
        # No user namespace/mount sandbox is silently requested or disabled.
        omit={'User','Group','PrivateDevices','ProtectHome','ProtectSystem','ReadWritePaths'}
        selected=json.loads(json.dumps([p for p in properties if p[0] not in omit]));fds=[]
        def index(fd):
            if fd not in fds:fds.append(fd)
            return fds.index(fd)
        for p in selected:
            if p[0]=='SELinuxContext':p[2]='unconfined_u:unconfined_r:'+p[2].split(':')[2]+':s0'
            if p[1]=='h':p[2]=index(p[2])
            elif p[1]=='a(hs)':
                for pair in p[2]:pair[0]=index(pair[0])
        target=self.fixture.current_target
        if target and name.endswith('.service'):
            after=[target];selected.append(['Requisite','as',[target]])
            if self.fixture.record['units'][name]['role']=='policy':
                graph=next(n for n,u in self.fixture.units.items() if u['role']=='graph' and u['pin'] is not None)
                after.append(graph);selected.append(['BindsTo','as',[graph]])
            selected.append(['After','as',after])
            if self.fixture.record['units'][name]['role'] in ('graph','policy'):selected.append(['PartOf','as',[target]])
        return self.call('create',name,properties=selected,fds=fds)

    def unit(self,member,name):return self.call('unit',name,member=member)
    def stop(self,name):return self.call('stop',name)
    def facts(self,name):return self.call('facts',name)
    def finish(self):
        self.call('quit');self.channel.close();self.closed=True
        assert self.process.wait(timeout=5)==0 and select.select([self.pin],[],[],0)[0]
        os.close(self.pin);self.fixture.record['user_worker']['exit']=0;self.fixture.save()
        assert not (self.fixture.output/'user-worker.stderr').read_bytes()
    def close(self):
        if not self.closed:self.channel.close();self.closed=True


class UserAudio(Audio):
    def context(self,role):return 'unconfined_u:unconfined_r:'+ROLES[role]+':s0'
    def memory_context(self,role):return 'unconfined_u:object_r:aetheri_audio_'+role+'_mem_test_t:s0'
    def session_cases(self):return ('unity','half','muted')

    def check_manager(self):
        assert not select.select([self.manager_pin],[],[],0)[0]
        assert facts('user@1000.service')==self.manager_facts,'original user manager changed'

    def setup(self,sdk,policy,control,binary,source):
        super().setup(sdk,policy,control,binary,source)
        self.observers={};self.targets={};self.current_target=None;self.source=source
        self.manager_facts=facts('user@1000.service');pid=int(self.manager_facts['MainPID'])
        assert self.manager_facts['ActiveState']=='active' and pid and not self.manager_facts['Job']
        self.manager_pin=os.pidfd_open(pid);self.supervisor_pid=pid;self.check_manager()
        self.record['user_manager']=dict(facts=self.manager_facts,pidfd_inode=os.fstat(self.manager_pin).st_ino);self.save()
        module=source/'policy/audio-user-test.cil'
        if any(line.split()[1]=='audio-user-test' for line in self.record['modules_before'].splitlines()):raise RuntimeError('foreign user audio module')
        self.record['user_policy_sha256']=hashlib.sha256(module.read_bytes()).hexdigest();self.save()
        self.effect('user-policy-install','audio-user-test',lambda:run('semodule','-X','400','-i',str(module)))
        stored=Path('/var/lib/selinux/targeted/active/modules/400/audio-user-test/cil');self.user_module=stored.read_bytes()
        assert bz2.decompress(self.user_module)==module.read_bytes()
        code=self.root/'code'
        for name in ('audio_user_worker.py','test_audio_actor.py','audio_mac_bus.py','image_state.py'):
            dest=code/name;shutil.copyfile(source/'tools'/name,dest);dest.chmod(0o444);label(dest,'aetheri_audio_fixture_test_t')
        shutil.copyfile(binary/'probe_audio_actor',code/'actor-observer');(code/'actor-observer').chmod(0o555);label(code/'actor-observer','aetheri_audio_fixture_test_t')
        # Targets cannot be transient units. Keep their root-selected ordinary
        # unit files distinct from the disposable transient source services.
        for case in self.session_cases():
            target='aetheri-audio-'+self.token+'-'+case+'-session.target';self.targets[case]=target
            path=Path('/etc/systemd/user')/target
            content=b'[Unit]\nDescription=Aetheri fixed audio test session\n'
            self.record.setdefault('targets',{})[target]=dict(case=case,state='file-preparing',path=str(path),sha256=hashlib.sha256(content).hexdigest());self.save()
            with path.open('xb') as stream:stream.write(content);stream.flush();os.fsync(stream.fileno())
            path.chmod(0o644);run('restorecon',str(path));st=path.stat()
            self.record['targets'][target].update(state='file-ready',object=[st.st_dev,st.st_ino]);self.save()
        self.bus.close();self.bus=UserBus(self);self.facts=self.bus.facts
        self.effect('session-reload',None,lambda:self.bus.call('reload'))

    def start(self,case,role,*args,**kwargs):
        self.current_target=None
        if case!='memory':
            target=self.targets[case]
            if self.record['targets'][target]['state']=='file-ready':
                observed=self.facts(target);assert observed['LoadState']=='loaded' and observed['ActiveState']=='inactive' and observed['FragmentPath']==self.record['targets'][target]['path']
                self.record['targets'][target]['state']='requesting';self.save()
                self.effect('session-start',target,lambda:self.bus.call('start',target))
                self.effect('session-ref',target,lambda:self.bus.unit('RefUnit',target))
                self.wait(lambda:self.facts(target)['ActiveState']=='active' and not self.facts(target)['Job'],[],'original session target')
                self.record['targets'][target].update(state='active',invocation=self.facts(target)['InvocationID']);self.save()
            self.current_target=self.targets[case]
        name=super().start(case,role,*args,**kwargs)
        if self.current_target:self.record['units'][name]['session_target']=self.current_target;self.save()
        return name

    def image(self,name,path):
        super().image(name,path)
        if self.units[name]['role']!='policy':return
        # Both source roles are now executing their selected images. The native
        # client independently joins them to this actual user manager/invocation.
        for role in ('graph','policy'):
            selected=next(n for n,u in self.units.items() if u['role']==role and u['pin'] is not None)
            unit=self.units[selected];out=self.output/('observe-'+selected+'.stdout');err=out.with_suffix('.stderr')
            with out.open('xb') as output,err.open('xb') as errors:
                p=subprocess.Popen([*self.bus.base,str(self.root/'code/actor-observer'),selected,str(self.images[role]),self.context(role),str(unit['pin']),'wait-exit'],env=self.bus.env,pass_fds=(unit['pin'],),stdout=output,stderr=errors)
            pin=os.pidfd_open(p.pid);self.observers[selected]=(p,pin,out,err)
            self.record.setdefault('native_observers',{})[selected]=dict(pid=p.pid,pidfd_inode=os.fstat(pin).st_ino,state='spawned');self.save()
            self.wait(lambda:'ACTOR_ADMITTED ' in out.read_text(),[selected],'native original user-service admission')
            assert not select.select([pin],[],[],0)[0]

    def stop(self,name):
        target=self.record['units'][name].get('session_target');source=target and self.units[name]['role'] in ('graph','policy')
        if source and self.record['targets'][target]['state']=='active':
            original_sources=[n for n,u in self.record['units'].items() if u.get('session_target')==target and u['role'] in ('graph','policy')]
            assert len(original_sources)==2
            for selected in original_sources:
                observed=self.facts(selected);baseline=self.units[selected]['facts']
                assert not select.select([self.units[selected]['pin']],[],[],0)[0]
                assert observed['ActiveState']=='active' and observed['SubState']=='running' and not observed['Job']
                assert observed['MainPID']==baseline['MainPID'] and observed['InvocationID']==baseline['InvocationID']
            self.record['targets'][target]['original_sources_live_before_stop']=original_sources;self.save()
            self.effect('session-stop',target,lambda:self.bus.stop(target))
            def stopped():
                if self.facts(target)['ActiveState']!='inactive' or self.facts(target)['Job']:return False
                return all(select.select([self.units[n]['pin']],[],[],0)[0] and self.facts(n)['MainPID']=='0' and not self.facts(n)['Job'] for n in original_sources)
            self.wait(stopped,[],'session target stops both original sources')
            assert self.facts(target)['InvocationID']==self.record['targets'][target]['invocation']
            self.record['targets'][target].update(state='stopped',original_sources_exited=original_sources);self.save()
        # Observe the independently ended original unit/cgroup without sending
        # a second stop that could mask a missing PartOf dependency.
        super().stop(name,request=not source)
        if name in self.observers:
            process,pin,out,err=self.observers.pop(name)
            assert process.wait(timeout=10)==0 and select.select([pin],[],[],0)[0]
            os.close(pin);text=out.read_text()
            assert 'ACTOR_ADMITTED ' in text and 'ACTOR_LOST ' in text and 'latched=1' in text and 'ACTOR_RETIRED ' in text and not err.read_bytes()
            self.record['native_observers'][name].update(state='retired',exit=0,transcript=text);self.save()
        if source and all(u['state']=='retired' for u in self.record['units'].values() if u.get('session_target')==target):
            self.effect('session-unref',target,lambda:self.bus.unit('UnrefUnit',target))
            value=self.record['targets'][target];path=Path(value['path']);st=path.stat()
            assert [st.st_dev,st.st_ino]==value['object'] and st.st_uid==0 and not st.st_mode&0o022 and hashlib.sha256(path.read_bytes()).hexdigest()==value['sha256']
            self.effect('session-file-remove',target,lambda:path.unlink())
            self.effect('session-reload',target,lambda:self.bus.call('reload'))
            self.wait(lambda:self.facts(target)['LoadState']=='not-found',[],'original session target collection')
            self.record['targets'][target]['state']='retired';self.save()

    def retire(self):
        assert not self.observers and all(t['state']=='retired' for t in self.record['targets'].values())
        self.bus.finish();self.check_manager()
        stored=Path('/var/lib/selinux/targeted/active/modules/400/audio-user-test/cil');assert stored.read_bytes()==self.user_module
        self.effect('user-policy-remove','audio-user-test',lambda:run('semodule','-X','400','-r','audio-user-test'))
        super().retire();os.close(self.manager_pin)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('sdk','policy','control','binary','source','output'):parser.add_argument(name,type=Path)
    args=parser.parse_args()
    if os.geteuid()!=0 or Path('/sys/class/dmi/id/product_name').read_text().strip()!='Aetheri audio lab VM' or Path('/sys/fs/selinux/enforce').read_text().strip()!='1':raise ValueError('separate root-only enforcing audio lab required')
    fixture=UserAudio(args.output)
    try:
        fixture.setup(args.sdk,args.policy,args.control,args.binary,args.source);fixture.memory_factories()
        for case in fixture.session_cases():fixture.case(case)
        fixture.retire();print(json.dumps(fixture.record))
    except BaseException as error:
        fixture.record.update(state='needs-inspection',error=repr(error));fixture.save();raise
    finally:fixture.bus.close()
