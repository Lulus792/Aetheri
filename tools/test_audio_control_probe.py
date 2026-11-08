"""Run the finite native probe against a real private graph and PCM capture."""
import argparse
import json
import os
from pathlib import Path
import select
import signal
import subprocess
import tempfile

import prepare_audio
import prepare_audio_control
import prepare_audio_policy
from test_audio import run_case


class Probe:
    extra_nodes=()
    def __init__(self,binary,headers,test):self.binary=binary;self.headers=headers;self.test=test
    def configure(self,conf,env):
        env['LD_LIBRARY_PATH']=str(self.headers/'usr/lib64')+':'+env['LD_LIBRARY_PATH']
    def exercise(self,root,env,remote,server,graph,volume,muted):
        p=subprocess.Popen(['setpriv','--no-new-privs',str(self.binary),str(remote),'aetheri.test.sink',self.test],env=env,stdin=subprocess.DEVNULL,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
        pin=os.pidfd_open(p.pid)
        try:
            out,err=p.communicate(timeout=20);(root/'native-probe.stdout').write_bytes(out);(root/'native-probe.stderr').write_bytes(err)
            assert p.returncode==0 and not err,(p.returncode,out,err)
            expected=f'AETHERI_AUDIO_NATIVE_PASS case={self.test} node=aetheri.test.sink '
            assert sum(l.startswith(expected) for l in out.decode().splitlines())==1
            assert f'volume={volume:.6f} muted={int(muted)} observed=1\n'.encode() in out
            assert out.count(b'AETHERI_AUDIO_NATIVE_CLOSED retired=1\n')==1 and b'AETHERI_AUDIO_NATIVE_FAIL' not in out
            assert b'AETHERI_AUDIO_NATIVE_ACTOR uid=1000 gid=1000 cap_zero=1 nnp=1 groups=0\n' in out
            assert select.select([pin],[],[],0)[0]
        finally:
            if p.poll() is None:
                signal.pidfd_send_signal(pin,signal.SIGTERM)
                try:p.wait(timeout=5)
                except subprocess.TimeoutExpired:signal.pidfd_send_signal(pin,signal.SIGKILL);p.wait(timeout=5)
            os.close(pin)
    def server_stopped(self):pass
    def close(self):pass


def run(binary,producer,sdk,policy,headers,work):
    if os.getuid()!=1000 or os.getgroups():raise ValueError('fixed UID1000/empty-groups test worker required')
    prepare_audio.check(sdk);prepare_audio_policy.check(policy);prepare_audio_control.check(headers)
    root=Path(tempfile.mkdtemp(prefix='prb-',dir=work));results=[]
    for test,volume,muted in [('unity',1.0,False),('half',.5,False),('muted',1.0,True)]:
        results.append(run_case(producer,sdk,root/test,48000,policy=policy,volume=volume,muted=muted,controller=Probe(binary,headers,test)))
    (root/'result.json').write_text(json.dumps(dict(passed=True,cases=results),indent=2)+'\n');return root


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for n in ('binary','producer','sdk','policy','headers','work'):p.add_argument(n,type=Path)
    a=p.parse_args();print(run(*(getattr(a,n).resolve() for n in ('binary','producer','sdk','policy','headers','work'))))
