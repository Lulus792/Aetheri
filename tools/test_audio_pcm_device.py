"""Explicit virtual-HDA FD playback, enforcing disposable audio lab only.

The root-selected playback FD goes only to the confined backend test domain.
No device ownership/mode/ACL or ordinary app permission is broadened. Unknown
effects preserve the original unit, node label, modules and journal for review.
This is not production device admission, revocation or recovery.
"""
import argparse
import base64
import bz2
import hashlib
import json
import os
from pathlib import Path
import shutil
import select
import signal
import stat
import subprocess
import sys

from test_audio_mac import Fixture,label,run,facts,ROLES
from test_audio_user import UserBus


def node(path):
    st=path.lstat()
    if not stat.S_ISCHR(st.st_mode) or os.major(st.st_rdev)!=116 or st.st_uid!=0 or st.st_gid!=63 or stat.S_IMODE(st.st_mode)!=0o660:raise ValueError('different virtual sound node')
    return dict(device=st.st_dev,inode=st.st_ino,rdev=st.st_rdev,uid=st.st_uid,gid=st.st_gid,mode=stat.S_IMODE(st.st_mode))


def policy_command(*args):
    # Cold policy-store reads/compilation can exceed the generic fixture's 20s.
    # A timed-out mutation stays pending; this is not an automatic retry.
    return subprocess.check_output(['semodule',*args],text=True,stderr=subprocess.STDOUT,timeout=55).strip()


class PCM(Fixture):
    def context(self,role):return 'unconfined_u:unconfined_r:'+ROLES[role]+':s0'
    def check_manager(self):
        assert not select.select([self.manager_pin],[],[],0)[0]
        assert facts('user@1000.service')==self.manager_facts

    def setup(self,binary,source):
        self.modules=policy_command('-lfull')
        if any(line.split()[1] in ('audio-test','audio-user-test','audio-pcm-test') for line in self.modules.splitlines()):raise ValueError('foreign/pending PCM fixture policy')
        self.device=Path('/dev/snd/pcmC0D0p');self.control=Path('/dev/snd/controlC0')
        self.original=node(self.device);self.control_original=node(self.control)
        self.device_context=os.getxattr(self.device,'security.selinux');self.record['device']=dict(path=str(self.device),object=self.original,original_context=base64.b64encode(self.device_context).decode());self.save()
        self.root.mkdir(mode=0o700);self.effect('mount',str(self.root),lambda:run('mount','-t','tmpfs','-o','size=16M,mode=0755','aetheri-audio-'+self.token,str(self.root)))
        self.root_object=(self.root.stat().st_dev,self.root.stat().st_ino)
        code=self.root/'code';code.mkdir();self.program=code/'pcm';self.gateway=code/'gateway'
        for path,name in ((self.program,'probe_audio_pcm'),(self.gateway,'probe_audio_mac')):
            shutil.copyfile(binary/name,path);path.chmod(0o555)
        self.record['images']={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in (self.program,self.gateway)};self.save()
        self.policy={}
        for name in ('audio-test','audio-user-test','audio-pcm-test'):
            path=source/('policy/'+name+'.cil');self.effect('policy-install',name,lambda:policy_command('-X','400','-i',str(path)))
            data=(Path('/var/lib/selinux/targeted/active/modules/400')/name/'cil').read_bytes();assert bz2.decompress(data)==path.read_bytes();self.policy[name]=data
        label(self.root,'aetheri_audio_fixture_test_t');label(code,'aetheri_audio_fixture_test_t')
        label(self.program,'aetheri_audio_graph_exec_test_t');label(self.gateway,'aetheri_audio_gateway_test_t')
        self.root.chmod(0o755)
        for name in ('audio_user_worker.py','test_audio_actor.py','audio_mac_bus.py','image_state.py'):
            dest=code/name;shutil.copyfile(source/'tools'/name,dest);dest.chmod(0o444);label(dest,'aetheri_audio_fixture_test_t')
        self.observer=code/'actor-observer';shutil.copyfile(binary/'probe_audio_actor',self.observer);self.observer.chmod(0o555);label(self.observer,'aetheri_audio_fixture_test_t')
        self.current_target=None;self.manager_facts=facts('user@1000.service');self.supervisor_pid=int(self.manager_facts['MainPID'])
        assert self.supervisor_pid and self.manager_facts['ActiveState']=='active' and not self.manager_facts['Job']
        self.manager_pin=os.pidfd_open(self.supervisor_pid);self.check_manager();self.record['manager']=self.manager_facts;self.save()
        self.bus.close();self.bus=UserBus(self);self.facts=self.bus.facts

    def child(self,mode,fd=None,ordinary=False,phase='initial'):
        launcher="import os,sys;os.dup2(int(sys.argv[1]),3,inheritable=True);os.execv(sys.argv[2],sys.argv[2:])"
        command=['python3','-c',launcher,str(fd),str(self.program),mode] if fd is not None else [str(self.program),mode]
        if ordinary:command=['setpriv','--reuid=1000','--regid=1000','--clear-groups','--no-new-privs',*command]
        p=subprocess.run(command,pass_fds=(fd,) if fd is not None else (),capture_output=True,timeout=25)
        for suffix,data in (('stdout',p.stdout),('stderr',p.stderr)):
            with (self.output/(phase+'-'+mode[2:]+'.'+suffix)).open('xb') as stream:stream.write(data)
        if p.returncode or p.stderr:raise RuntimeError((mode,p.returncode,p.stdout,p.stderr))
        return p.stdout.decode()

    def exercise(self):
        self.record['before_open_denied']=self.child('--denied-open',ordinary=True,phase='before');self.save()
        assert node(self.control)==self.control_original
        control=os.open(self.control,os.O_RDWR|os.O_CLOEXEC|os.O_NOFOLLOW)
        try:
            st=os.fstat(control);assert (st.st_dev,st.st_ino,st.st_rdev)==(self.control_original['device'],self.control_original['inode'],self.control_original['rdev'])
            self.record['baseline']=self.child('--baseline',control);self.save()
        finally:os.close(control)
        assert node(self.device)==self.original and os.getxattr(self.device,'security.selinux')==self.device_context
        self.effect('device-test-label',str(self.device),lambda:label(self.device,'aetheri_audio_pcm_test_t'))
        fd=os.open(self.device,os.O_WRONLY|os.O_CLOEXEC|os.O_NOFOLLOW)
        st=os.fstat(fd);assert (st.st_dev,st.st_ino,st.st_rdev)==(self.original['device'],self.original['inode'],self.original['rdev'])
        self.device_fd=fd;self.record['device']['labelled_context']=base64.b64encode(os.getxattr(self.device,'security.selinux')).decode();self.save()
        unit=self.start('pcm','graph',[str(self.gateway),'exec','pin',str(self.program),'--play','--wait-for-start'],{},pin=fd)
        self.release(unit);self.wait(lambda:'PCM_READY start=explicit' in self.units[unit]['log'].read_text(),[unit],'original PCM image barrier')
        out=self.output/'observer.stdout';err=self.output/'observer.stderr'
        with out.open('xb') as output,err.open('xb') as errors:
            observer=subprocess.Popen([*self.bus.base,str(self.observer),unit,str(self.program),self.context('graph'),str(self.units[unit]['pin']),'wait-exit'],env=self.bus.env,pass_fds=(self.units[unit]['pin'],),stdout=output,stderr=errors)
        observer_pin=os.pidfd_open(observer.pid);self.record['observer']=dict(pid=observer.pid,pidfd_inode=os.fstat(observer_pin).st_ino);self.save()
        self.wait(lambda:'ACTOR_ADMITTED ' in out.read_text(),[unit],'native original PCM user-service observation')
        assert not select.select([observer_pin],[],[],0)[0]
        signal.pidfd_send_signal(self.units[unit]['pin'],signal.SIGUSR1);self.finished(unit)
        text=self.units[unit]['log'].read_text();assert 'PCM_DRAINED signal_frames=96000 silence_frames=8192 submitted_frames=104192 rate=48000 channels=2 format=S16_LE' in text
        self.record['playback']=text;self.save();self.stop(unit)
        assert observer.wait(timeout=10)==0 and select.select([observer_pin],[],[],0)[0];os.close(observer_pin)
        assert 'ACTOR_LOST ' in out.read_text() and 'ACTOR_RETIRED ' in out.read_text() and not err.read_bytes()
        self.record['observer'].update(exit=0,transcript=out.read_text());self.save()
        self.retire_extra_fds();os.close(fd);self.device_fd=None
        assert node(self.device)==self.original and node(self.control)==self.control_original
        self.effect('device-context-restore',str(self.device),lambda:os.setxattr(self.device,'security.selinux',self.device_context))
        assert os.getxattr(self.device,'security.selinux')==self.device_context
        self.record['after_open_denied']=self.child('--denied-open',ordinary=True,phase='after');self.save()

    def retire(self):
        assert self.pending is None and all(u['state']=='retired' for u in self.record['units'].values()) and self.device_fd is None
        assert node(self.device)==self.original and os.getxattr(self.device,'security.selinux')==self.device_context
        assert (self.root.stat().st_dev,self.root.stat().st_ino)==self.root_object
        self.bus.finish();self.check_manager();self.effect('unmount',str(self.root),lambda:run('umount',str(self.root)));self.root.rmdir()
        for unit in self.units.values():label(unit['log'],'var_run_t')
        for name in ('audio-pcm-test','audio-user-test','audio-test'):
            assert (Path('/var/lib/selinux/targeted/active/modules/400')/name/'cil').read_bytes()==self.policy[name]
            self.effect('policy-remove',name,lambda:policy_command('-X','400','-r',name))
        assert policy_command('-lfull')==self.modules and Path('/sys/fs/selinux/enforce').read_text().strip()=='1'
        os.close(self.manager_pin);self.record['state']='passed-retired';self.save()


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('binary','source','output'):parser.add_argument(name,type=Path)
    args=parser.parse_args()
    if os.geteuid()!=0 or Path('/sys/class/dmi/id/product_name').read_text().strip()!='Aetheri audio lab VM' or Path('/sys/fs/selinux/enforce').read_text().strip()!='1':raise ValueError('separate enforcing root-only audio lab required')
    for field,value in (('vendor','0x8086'),('device','0x2668')):
        if (Path('/sys/class/sound/card0/device')/field).read_text().strip()!=value:raise ValueError('different virtual HDA controller')
    fixture=PCM(args.output)
    try:
        fixture.setup(args.binary,args.source);fixture.exercise();fixture.retire();print(json.dumps(fixture.record))
    except BaseException as error:
        fixture.record.update(state='needs-inspection',error=repr(error));fixture.save();raise
    finally:fixture.bus.close()


if __name__=='__main__':main()
