"""One original image-test QMP connection, local AF_UNIX only, no reconnect."""
import json
import select
import time

import run_vm as vm


class Connection:
    def __init__(self,path,identifier,process,state,save):
        self.process=process;self.state=state;self.save=save;self.buffer=b'';self.counter=0;self.socket=None;self.failed=False
        state.update(state='connecting',requests=[],events=[]);save()
        self.socket=vm.qmp_socket_at(path)
        try:
            if 'QMP' not in self.frame(time.monotonic()+5):raise ValueError('not a QMP greeting')
            self.execute('qmp_capabilities')
            if self.execute('query-uuid')!={'UUID':identifier}:raise ValueError('foreign original VM UUID')
            state['state']='authenticated';save()
        except BaseException:
            self.close();raise

    def frame(self,deadline):
        while b'\n' not in self.buffer:
            left=deadline-time.monotonic()
            if left<=0:raise TimeoutError('original QMP response timed out')
            ready,_,_=select.select([self.socket],[],[],left)
            if not ready:raise TimeoutError('original QMP response timed out')
            data=self.socket.recv(65536)
            if not data:raise EOFError('original QMP connection ended')
            self.buffer+=data
            if len(self.buffer)>1024**2:raise ValueError('QMP frame bound')
        line,self.buffer=self.buffer.split(b'\n',1)
        value=json.loads(line)
        if not isinstance(value,dict):raise ValueError('invalid QMP frame')
        return value

    def event(self,value):
        if not isinstance(value.get('event'),str) or len(self.state['events'])>=128:raise ValueError('QMP event bound')
        self.state['events'].append(value);self.save()

    def execute(self,name,arguments=None):
        if self.failed:raise RuntimeError('original QMP request is uncertain; no further dispatch')
        if self.socket is None:raise RuntimeError('original QMP connection is closed')
        if self.process.poll() is not None:raise RuntimeError('original VM is terminal')
        self.counter+=1;identifier=self.counter
        request=dict(execute=name,id=identifier)
        if arguments is not None:request['arguments']=arguments
        record=dict(request=request,state='pending');self.state['requests'].append(record);self.save()
        try:
            self.socket.sendall(json.dumps(request).encode()+b'\r\n')
            deadline=time.monotonic()+5
            while True:
                value=self.frame(deadline)
                if 'event' in value:self.event(value);continue
                if value.get('id')!=identifier or 'error' in value or 'return' not in value:raise ValueError('uncorrelated/error QMP response')
                record['state']='acknowledged';self.save();return value['return']
        except BaseException:
            self.failed=True;self.state['state']='failed_preserved';self.save();raise

    def poll(self,timeout=.1):
        try:
            if b'\n' not in self.buffer and not select.select([self.socket],[],[],timeout)[0]:return
            value=self.frame(time.monotonic()+5)
            if 'event' not in value:raise ValueError('unsolicited QMP reply')
            self.event(value)
        except BaseException:
            self.failed=True;self.state['state']='disconnected';self.save();raise

    def close(self):
        if self.socket is not None:self.socket.close();self.socket=None;self.state['transport_closed']=True;self.save()


def guest_poweroff(events):
    shutdown=[e for e in events if e.get('event')=='SHUTDOWN']
    return len(shutdown)==1 and shutdown[0].get('data',{}).get('guest') is True and shutdown[0]['data'].get('reason')=='guest-shutdown'
