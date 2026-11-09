"""Finite ordinary-user bus worker for the explicit disposable audio fixture."""
import array
import json
import os
import socket
import subprocess
import sys
from audio_mac_bus import Bus
from test_audio_actor import initial_owner

LIMIT=131072


def packet(channel,value,fds=()):
    raw=json.dumps(value).encode();assert len(raw)<=LIMIT and len(fds)<=8
    ancillary=[(socket.SOL_SOCKET,socket.SCM_RIGHTS,array.array('i',fds))] if fds else []
    if channel.sendmsg([raw],ancillary)!=len(raw):raise RuntimeError('uncertain fixture packet; do not replay')


def receive(channel):
    raw,ancillary,flags,_=channel.recvmsg(LIMIT,socket.CMSG_SPACE(8*array.array('i').itemsize),socket.MSG_CMSG_CLOEXEC)
    fds=[]
    try:
        invalid=False
        for level,kind,data in ancillary:
            if level!=socket.SOL_SOCKET or kind!=socket.SCM_RIGHTS:invalid=True;continue
            values=array.array('i');whole=len(data)-len(data)%values.itemsize
            values.frombytes(data[:whole]);fds.extend(values);invalid|=whole!=len(data)
        if invalid or not raw or flags&(socket.MSG_TRUNC|socket.MSG_CTRUNC) or len(fds)>8:raise ValueError('foreign/missing/truncated fixture packet')
        return json.loads(raw),fds
    except BaseException:
        for fd in fds:os.close(fd)
        raise


def worker(number):
    if os.getuid()!=1000 or os.geteuid()!=1000:raise ValueError('ordinary fixture worker required')
    channel=socket.socket(fileno=number);bus=Bus(user=True)
    def owner():return bus.call('GetNameOwner',lambda m:bus.append(m,'s','org.freedesktop.systemd1'),destination='org.freedesktop.DBus',path='/org/freedesktop/DBus',interface='org.freedesktop.DBus',result='s')
    try:
        original=initial_owner(owner);packet(channel,dict(ready=True,owner=original))
        while True:
            request,fds=receive(channel)
            try:
                op=request['op'];name=request.get('unit')
                if owner()!=original:raise RuntimeError('original user manager departed')
                if op=='create':
                    properties=request['properties']
                    for prop in properties:
                        if prop[1]=='h':prop[2]=fds[prop[2]]
                        elif prop[1]=='a(hs)':
                            for pair in prop[2]:pair[0]=fds[pair[0]]
                    result=bus.create(name,properties,destination=original)
                elif op=='unit':result=bus.unit(request['member'],name,destination=original)
                elif op=='stop':result=bus.stop(name,destination=original)
                elif op=='start':result=bus.call('StartUnit',lambda m:(bus.append(m,'s',name),bus.append(m,'s','fail')),destination=original,result='o')
                elif op=='reload':result=bus.call('Reload',destination=original)
                elif op=='facts':
                    keys='Id,LoadState,ActiveState,SubState,MainPID,ControlPID,InvocationID,ControlGroup,Job,ExecMainStatus,FragmentPath'
                    p=subprocess.run(['systemctl','--user','show',name,'--property='+keys],capture_output=True,text=True)
                    result=dict(line.split('=',1) for line in p.stdout.splitlines())
                    if not result:raise RuntimeError('user unit observation failed: '+p.stderr)
                elif op=='quit':
                    packet(channel,dict(op=op,result=None));return
                else:raise ValueError('unknown fixture method')
                if owner()!=original:raise RuntimeError('manager changed during fixture method')
            finally:
                for fd in fds:os.close(fd)
            packet(channel,dict(op=op,result=result))
    finally:bus.close();channel.close()


if __name__=='__main__':
    if len(sys.argv)!=3 or sys.argv[1]!='--worker':raise ValueError('explicit inherited fixture channel required')
    worker(int(sys.argv[2]))
