"""Explicit real-user-unit observer check in the disposable audio lab only.

No normal units, policy or account configuration are installed. The temporary
user-bus worker retains original unit references. Unknown effects preserve the
journal/units for inspection and are never replayed or called recovered.
"""
import argparse
import errno
import hashlib
import json
import os
from pathlib import Path
import select
import shutil
import signal
import subprocess
import sys
import time
import uuid

from audio_mac_bus import Bus,BusError
import image_state


def initial_owner(query):
    """Discovery only, before any admitted identity or service effect exists."""
    deadline=time.monotonic()+5
    while True:
        try:
            name=query()
            if not isinstance(name,str) or not name.startswith(':') or len(name)>=256:raise ValueError('invalid original unique owner')
            return name
        except BusError as error:
            if error.remote_name!='org.freedesktop.DBus.Error.NameHasNoOwner' or time.monotonic()>=deadline:raise
            time.sleep(.025)


def user_worker():
    if os.getuid()!=1000 or os.geteuid()!=1000:raise ValueError('ordinary worker required')
    bus=Bus(user=True)
    def owner():return bus.call('GetNameOwner',lambda m:bus.append(m,'s','org.freedesktop.systemd1'),destination='org.freedesktop.DBus',path='/org/freedesktop/DBus',interface='org.freedesktop.DBus',result='s')
    try:
        original=initial_owner(owner)
        print(json.dumps(dict(ready=True,owner=original)),flush=True)
        for raw in sys.stdin:
            request=json.loads(raw);op=request['op'];name=request.get('unit')
            if owner()!=original:raise RuntimeError('original manager departed')
            if op=='start':
                properties=[('Type','s','exec'),('Restart','s','no'),('NoNewPrivileges','b',True),('AmbientCapabilities','t',0),('KillMode','s','control-group'),('TimeoutStopUSec','t',5000000),('ExecStart','a(sasb)',[('/usr/bin/sleep',['/usr/bin/sleep','60'],False)])]
                result=bus.create(name,properties,destination=original)
            elif op=='ref':result=bus.unit('RefUnit',name,destination=original)
            elif op=='reset':result=bus.unit('ResetFailedUnit',name,destination=original)
            elif op=='unref':result=bus.unit('UnrefUnit',name,destination=original)
            elif op=='facts':
                p=subprocess.run(['systemctl','--user','show',name,'--property=LoadState,ActiveState,SubState,MainPID,ControlPID,InvocationID,ControlGroup,Job'],capture_output=True,text=True)
                result=dict(line.split('=',1) for line in p.stdout.splitlines())
                if not result:raise RuntimeError('unit observation failed: '+p.stderr)
            elif op=='quit':
                print(json.dumps(dict(op=op,result=None)),flush=True);return
            else:raise ValueError('unknown worker operation')
            if owner()!=original:raise RuntimeError('original manager changed during observation')
            print(json.dumps(dict(op=op,result=result)),flush=True)
    finally:bus.close()


def wait(predicate,description):
    deadline=time.monotonic()+10
    while not predicate():
        if time.monotonic()>deadline:raise TimeoutError(description)
        time.sleep(.025)


def run(binary,source,output):
    if os.geteuid()!=0 or Path('/sys/class/dmi/id/product_name').read_text().strip()!='Aetheri audio lab VM' or Path('/sys/fs/selinux/enforce').read_text().strip()!='1':raise ValueError('separate enforcing root-only audio lab required')
    output.mkdir(mode=0o700);token=uuid.uuid4().hex
    root=Path('/usr/local/libexec/aetheri-audio-actor-'+token);root.mkdir(mode=0o755)
    paths={}
    for name,path in (('probe',binary),('test_audio_actor.py',source/'tools/test_audio_actor.py'),('audio_mac_bus.py',source/'tools/audio_mac_bus.py'),('image_state.py',source/'tools/image_state.py')):
        dest=root/name;shutil.copyfile(path,dest);dest.chmod(0o555 if name=='probe' else 0o444)
        subprocess.run(['chcon','system_u:object_r:bin_t:s0',str(dest)],check=True)
        paths[name]=hashlib.sha256(dest.read_bytes()).hexdigest()
    env={**os.environ,'PYTHONDONTWRITEBYTECODE':'1','XDG_RUNTIME_DIR':'/run/user/1000','DBUS_SESSION_BUS_ADDRESS':'unix:path=/run/user/1000/bus'}
    base=['setpriv','--reuid=1000','--regid=1000','--clear-groups','--no-new-privs']
    record=dict(token=token,boot=Path('/proc/sys/kernel/random/boot_id').read_text().strip(),root=str(root),root_object=[root.stat().st_dev,root.stat().st_ino],inputs=paths,units={},pending=None,state='preparing')
    def save():image_state.write(output/'controller.json',record)
    save()
    with (output/'worker.stderr').open('xb') as errors:
        worker=subprocess.Popen([*base,'python3',str(root/'test_audio_actor.py'),'--worker'],env=env,stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=errors,text=True,bufsize=1)
    worker_pin=os.pidfd_open(worker.pid);record['worker_pid']=worker.pid;save()
    def read_reply():
        if not select.select([worker.stdout],[],[],8)[0]:raise TimeoutError('original worker reply; do not replay')
        raw=worker.stdout.readline(65537)
        if not raw or len(raw)>65536:raise RuntimeError('missing/bounded original worker reply')
        return json.loads(raw)
    def rpc(op,unit=None):
        if record['pending'] is not None:raise RuntimeError('earlier uncertain effect; preserve')
        record['pending']=dict(op=op,unit=unit);save()
        worker.stdin.write(json.dumps(record['pending'])+'\n');worker.stdin.flush()
        reply=read_reply();assert reply['op']==op
        record['pending']=None;save();return reply['result']
    actors={}
    try:
        ready=read_reply();assert ready['ready'];record['manager_owner']=ready['owner'];save()
        for role in ('original','foreign'):
            name='aetheri-audio-actor-'+token+'-'+role+'.service';assert rpc('facts',name)['LoadState']=='not-found'
            record['units'][name]=dict(role=role,state='requesting');save()
            rpc('start',name);rpc('ref',name)
            def active():
                fact=rpc('facts',name);return fact['ActiveState']=='active' and fact['SubState']=='running' and fact['MainPID']!='0' and not fact['Job']
            wait(active,'user service startup');fact=rpc('facts',name);pid=int(fact['MainPID']);pin=os.pidfd_open(pid)
            proc=Path('/proc')/str(pid);context=(proc/'attr/current').read_bytes().rstrip(b'\0\n').decode()
            image=(proc/'exe').stat();expected=Path('/usr/bin/sleep').stat();assert (image.st_dev,image.st_ino)==(expected.st_dev,expected.st_ino)
            group=Path('/sys/fs/cgroup'+fact['ControlGroup']);directory=os.open(group,os.O_RDONLY|os.O_DIRECTORY|os.O_CLOEXEC);events=os.open('cgroup.events',os.O_RDONLY|os.O_CLOEXEC,dir_fd=directory)
            assert fact==rpc('facts',name) and not select.select([pin],[],[],0)[0]
            actors[role]=dict(unit=name,pid=pid,pin=pin,group=directory,events=events,facts=fact,context=context)
            record['units'][name].update(state='original-active',facts=fact,pidfd_inode=os.fstat(pin).st_ino,cgroup_inode=os.fstat(directory).st_ino,context=context);save()
        original=actors['original']
        def probe(name,image,context,mode,case):
            log=(output/(case+'.stdout')).open('xb');err=(output/(case+'.stderr')).open('xb')
            child=subprocess.Popen([*base,str(root/'probe'),name,image,context,str(original['pin']),mode],env=env,pass_fds=(original['pin'],),stdout=log,stderr=err)
            log.close();err.close();record.setdefault('probes',{})[case]=dict(pid=child.pid,state='spawned');save();return child
        for case,name,image,context in (
            ('wrong-unit',actors['foreign']['unit'],'/usr/bin/sleep',original['context']),
            ('wrong-image',original['unit'],'/usr/bin/true',original['context']),
            ('wrong-context',original['unit'],'/usr/bin/sleep','system_u:system_r:init_t:s0')):
            child=probe(name,image,context,'reject',case);assert child.wait(timeout=10)==0
            text=(output/(case+'.stdout')).read_text();assert 'ACTOR_REJECTED ' in text and 'ACTOR_RETIRED ' in text
            assert not (output/(case+'.stderr')).read_bytes();record.setdefault('rejections',{})[case]=text;save()
            record['probes'][case].update(state='exited',exit=child.returncode);save()
        child=probe(original['unit'],'/usr/bin/sleep',original['context'],'wait-exit','original')
        child_pin=os.pidfd_open(child.pid)
        wait(lambda:'ACTOR_ADMITTED ' in (output/'original.stdout').read_text(),'actual native admission')
        assert not select.select([child_pin],[],[],0)[0]
        record['native_observer_admitted']=True;save()
        for role,a in actors.items():
            assert rpc('facts',a['unit'])==a['facts'] and not select.select([a['pin']],[],[],0)[0]
            record['pending']=dict(op='signal-original',unit=a['unit'],signal='SIGKILL');save()
            signal.pidfd_send_signal(a['pin'],signal.SIGKILL);record['pending']=None;save()
            wait(lambda:select.select([a['pin']],[],[],0)[0],'original process exit')
            def terminal():
                fact=rpc('facts',a['unit']);assert fact['InvocationID']==a['facts']['InvocationID']
                return fact['ActiveState']=='failed' and fact['MainPID']==fact['ControlPID']=='0' and not fact['Job']
            wait(terminal,'original user unit terminal')
            def removed():
                try:os.pread(a['events'],4096,0);return False
                except OSError as error:
                    if error.errno!=errno.ENODEV:raise
                return not Path('/sys/fs/cgroup'+a['facts']['ControlGroup']).exists()
            wait(removed,'held original cgroup deactivation')
            rpc('reset',a['unit']);rpc('unref',a['unit']);wait(lambda:rpc('facts',a['unit'])['LoadState']=='not-found','original unit collection')
            for key in ('pin','group','events'):os.close(a[key])
            record['units'][a['unit']]['state']='retired';save()
            if role=='original':
                assert child.wait(timeout=10)==0 and select.select([child_pin],[],[],0)[0];os.close(child_pin)
                text=(output/'original.stdout').read_text();assert 'ACTOR_LOST ' in text and 'latched=1' in text and 'ACTOR_RETIRED ' in text
                assert not (output/'original.stderr').read_bytes();record['native_original']=text;save()
                record['probes']['original'].update(state='exited',exit=child.returncode);save()
        rpc('quit');worker.stdin.close();assert worker.wait(timeout=5)==0 and select.select([worker_pin],[],[],0)[0];os.close(worker_pin)
        assert not (output/'worker.stderr').read_bytes()
        assert [root.stat().st_dev,root.stat().st_ino]==record['root_object']
        assert set(p.name for p in root.iterdir())==set(paths)
        for name,digest in paths.items():assert hashlib.sha256((root/name).read_bytes()).hexdigest()==digest
        for name in paths:(root/name).unlink()
        root.rmdir();assert Path('/sys/fs/selinux/enforce').read_text().strip()=='1'
        record['state']='passed-retired';save();return record
    except BaseException as error:
        record.update(state='needs-inspection',error=repr(error));save();raise


if __name__=='__main__':
    if sys.argv[1:]==['--worker']:user_worker()
    else:
        parser=argparse.ArgumentParser(description=__doc__)
        for name in ('binary','source','output'):parser.add_argument(name,type=Path)
        args=parser.parse_args();print(json.dumps(run(args.binary,args.source,args.output)))
