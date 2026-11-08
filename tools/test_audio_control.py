"""Exercise the C adapter against actual private policy and independent PCM."""
import argparse
import json
import os
from pathlib import Path
import select
import signal
import subprocess
import tempfile
import time

import prepare_audio
import prepare_audio_control
import prepare_audio_policy
from test_audio import run_case


class Client:
    def __init__(self,binary,remote,root,env,immediate=False):
        self.root=root;self.pending=None;self.snapshot=None;self.results=[];self.closed=False;self.thread=False;self.buffer=b'';self.raw=bytearray()
        self.log=(root.with_suffix('.stderr')).open('xb')
        self.process=subprocess.Popen(['setpriv','--no-new-privs',str(binary),str(remote),*(['--close-immediate'] if immediate else [])],env=env,stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=self.log)
        self.pin=os.pidfd_open(self.process.pid);os.set_blocking(self.process.stdout.fileno(),False)

    def pump(self,timeout=.01):
        if not select.select([self.process.stdout],[],[],timeout)[0]:return
        raw=os.read(self.process.stdout.fileno(),65536);self.raw.extend(raw);self.buffer+=raw
        assert len(self.raw)<=1024**2,'native transcript bound'
        while b'\n' in self.buffer:
            raw,self.buffer=self.buffer.split(b'\n',1);parts=raw.decode().split()
            if parts[0]=='SNAP':
                assert self.pending is None and len(parts)==7
                self.pending=dict(revision=int(parts[1]),state=int(parts[2]),count=int(parts[3]),command=int(parts[4]),command_state=int(parts[5]),error=int(parts[6]),outputs=[])
            elif parts[0]=='OUT':
                assert self.pending is not None and len(parts)==9
                self.pending['outputs'].append(dict(connection=int(parts[1]),serial=int(parts[2]),id=int(parts[3]),default=int(parts[4]),available=int(parts[5]),muted=int(parts[6]),volume=float(parts[7]),name=bytes.fromhex(parts[8]).decode()))
            elif parts[0]=='END':
                assert self.pending and self.pending['count']==len(self.pending['outputs'])
                self.snapshot=self.pending;self.pending=None
            elif parts[0]=='RESULT':self.results.append(int(parts[1]))
            elif parts==['CLOSED']:self.closed=True
            elif parts==['THREAD','1']:self.thread=True
            else:raise AssertionError(('unexpected native output',parts))

    def wait(self,predicate,seconds=8):
        deadline=time.monotonic()+seconds
        while not predicate():
            self.pump()
            assert time.monotonic()<deadline,('native wait',self.raw[-2000:].decode())
            assert self.process.poll() is None or predicate(),('native exited',self.process.returncode,self.raw[-2000:].decode())

    def send(self,line):
        assert self.process.poll() is None
        self.process.stdin.write((line+'\n').encode());self.process.stdin.flush()

    def output(self,name):return next(o for o in self.snapshot['outputs'] if o['name']==name)

    def request(self,kind,name,value,expected=0,revision=None,ref=None):
        for attempt in range(4):
            self.pump(0);before=self.snapshot;rev=before['revision'] if revision is None else revision
            target=self.output(name) if ref is None else ref;index=len(self.results)
            self.send(f"{kind} {rev} {target['connection']} {target['serial']} {target['id']} {value}")
            self.wait(lambda:len(self.results)>index);actual=self.results[index]
            if actual==2 and expected==0 and revision is None and ref is None:
                # STALE explicitly means no submission. Only a new observation
                # permits another user request; SUBMITTED is never replayed.
                self.wait(lambda:self.snapshot['revision']>rev);continue
            assert actual==expected,(kind,expected,actual,before)
            if expected==0:
                self.wait(lambda:self.snapshot['command']==before['command']+1 and self.snapshot['command_state']==2)
            return
        raise AssertionError('repeated observation drift')

    def inspect(self,binary,sdk,policy,control):
        assert not select.select([self.pin],[],[],0)[0]
        p=Path('/proc')/str(self.process.pid)
        assert (p/'exe').resolve()==binary.resolve()
        status=dict(l.split(':',1) for l in (p/'status').read_text().splitlines() if ':' in l)
        assert set(status['Uid'].split())=={str(os.getuid())} and set(status['Gid'].split())=={str(os.getgid())}
        assert int(status['CapEff'].strip(),16)==0 and status['NoNewPrivs'].strip()=='1'
        maps=(p/'maps').read_text();(self.root.with_suffix('.maps')).write_text(maps)
        for path in (sdk/'usr/lib64/libpipewire-0.3.so.0.1611.0',policy/'usr/lib64/libwireplumber-0.5.so.0.519.0',control/'usr/lib64/libglib-2.0.so.0.8800.0'):
            assert str(path) in maps,path
        fds=[os.readlink(f) for f in (p/'fd').iterdir() if f.exists()]
        assert not any(f.startswith(('/dev/snd/','/dev/video')) for f in fds)
        sockets={f[8:-1] for f in fds if f.startswith('socket:[')}
        unix={l.split()[6] for l in (p/'net/unix').read_text().splitlines()[1:]};assert sockets<=unix
        assert not select.select([self.pin],[],[],0)[0]
        (self.root.with_suffix('.actor.json')).write_text(json.dumps(dict(pid=self.process.pid,uid=os.getuid(),gid=os.getgid(),capabilities=0,no_new_privileges=True,unix_sockets_only=True,no_audio_video_fds=True),indent=2))

    def close(self,successful=True):
        if self.pin is None:return
        try:
            if successful:
                if not self.closed:self.send('close');self.wait(lambda:self.closed)
                assert self.process.wait(timeout=5)==0
                self.pump(0);assert self.closed and not self.buffer and self.pending is None
            elif self.process.poll() is None:
                signal.pidfd_send_signal(self.pin,signal.SIGTERM);self.process.wait(timeout=5)
            assert select.select([self.pin],[],[],0)[0]
        finally:
            # A failed observation may not abandon the original child.
            if self.process.poll() is None:
                signal.pidfd_send_signal(self.pin,signal.SIGKILL);self.process.wait(timeout=5)
            os.close(self.pin);self.pin=None
            self.process.stdin.close();self.process.stdout.close();self.log.close()
            self.root.with_suffix('.stdout').write_bytes(self.raw)


class Controller:
    extra_nodes=('aetheri.control.second','aetheri.control."unsafe')
    def __init__(self,binary,sdk,policy,control):self.binary=binary;self.sdk=sdk;self.policy=policy;self.control=control;self.clients=[];self.result={}

    def configure(self,conf,env):
        env['LD_LIBRARY_PATH']=str(self.control/'usr/lib64')+':'+env['LD_LIBRARY_PATH']
        p=conf/'server.conf';s=p.read_text().replace('media.class = Audio/Sink node.virtual = true node.name = aetheri.test.sink','media.class = Audio/Sink priority.session = 2000 node.virtual = true node.name = aetheri.test.sink')
        extra=''.join(' { factory = adapter args = { factory.name = support.null-audio-sink node.name = '+json.dumps(n)+' media.class = Audio/Sink node.virtual = true priority.session = 1 audio.position = [ FL FR ] } }' for n in self.extra_nodes)
        s=s.replace('context.objects = [','context.objects = ['+extra,1)
        p.write_text(s)

    def exercise(self,root,env,remote,server,graph,volume,muted):
        self.root=root
        c=Client(self.binary,remote,root/'native',env);self.clients.append(c)
        c.wait(lambda:c.snapshot and c.snapshot['state']==1 and c.snapshot['count']==3 and all(o['available'] for o in c.snapshot['outputs']) and any(o['name']=='aetheri.test.sink' and o['default'] for o in c.snapshot['outputs']))
        c.inspect(self.binary,self.sdk,self.policy,self.control)
        first='aetheri.test.sink';second='aetheri.control.second'
        assert c.output(first)['default'] and not c.output(second)['default']
        initial=c.snapshot.copy();wrong=c.output(first).copy();wrong['connection']^=1
        c.request('volume',first,.5,expected=2,ref=wrong)
        wrong=c.output(first).copy();wrong['serial']+=100000
        c.request('volume',first,.5,expected=2,ref=wrong)
        c.request('volume',first,.5,expected=2,revision=0)
        for invalid in ('nan','inf','-0.1','1.1'):c.request('volume',first,invalid,expected=1)
        c.request('default','aetheri.control."unsafe',0,expected=3)
        c.send('thread');c.wait(lambda:c.thread)
        c.request('default',second,0);assert c.output(second)['default']
        # Independent server metadata, not the adapter's optimistic state.
        objects=graph();metadata=[o for o in objects if o['type']=='PipeWire:Interface:Metadata' and o.get('props',{}).get('metadata.name')=='default']
        assert len(metadata)==1
        items=metadata[0]['metadata'];default=next(x for x in items if x.get('key')=='default.audio.sink')
        assert default['value']['name']==second,default
        c.request('default',first,0);assert c.output(first)['default']
        c.request('mute',first,1);assert c.output(first)['muted']
        c.request('volume',first,volume);assert c.output(first)['muted'],'volume implicitly unmutes'
        c.request('mute',first,1 if muted else 0)
        assert abs(c.output(first)['volume']-volume)<.00001 and bool(c.output(first)['muted'])==muted
        missing=Client(self.binary,remote.with_name('missing'),root/'missing',env);self.clients.append(missing)
        missing.wait(lambda:missing.snapshot and missing.snapshot['state'] in (2,3))
        assert missing.snapshot['count']==0;missing.close()
        producer=self.binary.parent/'aetheri-audio-check'
        negative=subprocess.run(['setpriv','--no-new-privs',str(producer),'--remote',str(remote.with_name('missing')),'--frames','1','--wait-for-start'],env=env,capture_output=True,timeout=3)
        (root/'producer-missing.log').write_bytes(negative.stdout+negative.stderr)
        assert negative.returncode==1 and b'CONNECTED ' not in negative.stdout
        immediate=Client(self.binary,remote,root/'immediate',env,True);self.clients.append(immediate);immediate.wait(lambda:immediate.closed);immediate.close()
        # Suspend only this retained private server. A locally submitted command
        # now cannot get its barrier/readback; timeout must remain UNKNOWN.
        pin=os.pidfd_open(server.pid)
        try:
            signal.pidfd_send_signal(pin,signal.SIGSTOP)
            for attempt in range(4):
                c.pump(0);before=c.snapshot;target=c.output(first);index=len(c.results)
                c.send(f"volume {before['revision']} {target['connection']} {target['serial']} {target['id']} 0.37")
                c.wait(lambda:len(c.results)>index)
                if c.results[index]==2:
                    c.wait(lambda:c.snapshot['revision']>before['revision']);continue
                assert c.results[index]==0;break
            else:raise AssertionError('timeout fixture could not submit')
            c.wait(lambda:c.snapshot['state']==2 and c.snapshot['error']==2)
            assert c.snapshot['command_state']==3 and c.snapshot['count']==0
            uncertain=c.snapshot.copy()
        finally:
            signal.pidfd_send_signal(pin,signal.SIGCONT);os.close(pin)
        c.close()
        # This explicit replacement is a new observer, not recovery/replay by
        # the library. The old unknown command is never reported cancelled.
        c=Client(self.binary,remote,root/'replacement',env);self.clients.append(c)
        c.wait(lambda:c.snapshot and c.snapshot['state']==1 and c.snapshot['count']==3 and all(o['available'] for o in c.snapshot['outputs']) and any(o['name']==first and o['default'] for o in c.snapshot['outputs']))
        c.inspect(self.binary,self.sdk,self.policy,self.control)
        c.request('volume',first,volume);c.request('mute',first,1 if muted else 0)
        self.active=c
        self.result=dict(passed=True,initial=initial,final=c.snapshot.copy(),rejections=8,default_switch=True,unsafe_name_rejected=True,mute_preserved=True,missing_remote=True,producer_missing_remote=True,immediate_close=True,wrong_thread=True,timeout_unknown=uncertain)
        (root/'native-control.json').write_text(json.dumps(self.result,indent=2)+'\n')

    def server_stopped(self):
        c=self.active;c.wait(lambda:c.snapshot and c.snapshot['state']==3)
        assert c.snapshot['count']==0
        ref=self.result['final']['outputs'][0]
        c.request('volume','',.5,expected=3,ref=ref)
        self.result['server_loss']=c.snapshot.copy();c.close()
        (self.root/'native-control.json').write_text(json.dumps(self.result,indent=2)+'\n')

    def close(self):
        errors=[]
        for c in reversed(self.clients):
            try:c.close(successful=c.closed)
            except BaseException as error:errors.append(error)
        if errors:raise RuntimeError('native controller retirement failed') from errors[0]


def retirement_fault(producer,sdk,policy,control,root):
    class Fault(Controller):
        def exercise(self,*args):
            self.held=[(p,os.dup(pin)) for p,pin in self.test_owners]
            raise RuntimeError('injected exercise failure')
        def close(self):raise RuntimeError('injected controller cleanup failure')
    fault=Fault(None,sdk,policy,control);fault.held=[]
    try:
        try:run_case(producer,sdk,root/'cleanup-fault',48000,policy=policy,controller=fault)
        except RuntimeError as error:assert str(error)=='injected controller cleanup failure'
        else:raise AssertionError('cleanup fault was hidden')
        assert len(fault.held)==2
        assert all(p.poll() is not None and select.select([pin],[],[],0)[0] for p,pin in fault.held),'cleanup exception stranded an original actor'
        result=dict(passed=True,original_actors_retired=len(fault.held),cleanup_error_preserved=True)
        (root/'cleanup-fault.json').write_text(json.dumps(result,indent=2)+'\n')
    finally:
        # The regression itself also retires original handles if it exposes
        # the old skip-on-exception bug; a failed test must not strand actors.
        for p,pin in fault.held:
            try:
                if p.poll() is None:
                    signal.pidfd_send_signal(pin,signal.SIGTERM)
                    try:p.wait(timeout=5)
                    except subprocess.TimeoutExpired:signal.pidfd_send_signal(pin,signal.SIGKILL);p.wait(timeout=5)
            finally:os.close(pin)


def run(binary,producer,sdk,policy,control,work):
    if os.getuid()==0:raise ValueError('ordinary test user required')
    prepare_audio.check(sdk);prepare_audio_policy.check(policy);prepare_audio_control.check(control)
    root=Path(tempfile.mkdtemp(prefix='ctl-',dir=work));results=[]
    for name,volume,muted in [('unity',1.0,False),('half',.5,False),('muted',1.0,True)]:
        results.append(run_case(producer,sdk,root/name,48000,policy=policy,volume=volume,muted=muted,controller=Controller(binary,sdk,policy,control)))
    retirement_fault(producer,sdk,policy,control,root)
    (root/'result.json').write_text(json.dumps(dict(passed=True,cases=results),indent=2)+'\n');return root


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('binary','producer','sdk','policy','control','work'):p.add_argument(name,type=Path)
    a=p.parse_args();print(run(*(getattr(a,n).resolve() for n in ('binary','producer','sdk','policy','control','work'))))
