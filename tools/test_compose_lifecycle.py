"""Independent native v3 client: original field/focus and secret boundaries."""
import hashlib
import json
import os
from pathlib import Path
import re
import signal
import socket
import subprocess
import sys
import tempfile
import time


def main():
    server_bin,client_bin,build=map(lambda s:Path(s).resolve(),sys.argv[1:])
    assert os.getuid()!=0
    root=Path(tempfile.mkdtemp(prefix='compose-lifecycle-',dir=build))
    runtime=root/'runtime';runtime.mkdir(mode=0o700)
    env={**os.environ,'XDG_RUNTIME_DIR':str(runtime),'WAYLAND_DISPLAY':'compose'}
    for key in ('DISPLAY','WAYLAND_SOCKET'):env.pop(key,None)
    controller,issuer=socket.socketpair(socket.AF_UNIX,socket.SOCK_SEQPACKET);controller.settimeout(4)
    children,streams,pins,cases=[],[],[],[]
    binaries={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in (server_bin,client_bin)}
    def start(binary,name,*args,pass_fds=()):
        stream=(root/(name+'.log')).open('wb');streams.append(stream)
        p=subprocess.Popen([str(binary),*map(str,args)],env=env,stdin=subprocess.PIPE,stdout=stream,stderr=subprocess.STDOUT,pass_fds=pass_fds)
        children.append(p);pins.append(os.pidfd_open(p.pid));return p
    def log(name='client'):return (root/(name+'.log')).read_text(errors='replace')
    def wait(predicate,label):
        end=time.monotonic()+7
        while not predicate():
            assert all(p.poll() is None for p in children),(label,root,log(),log('server'))
            assert time.monotonic()<end,(label,root,log(),log('server'));time.sleep(.01)
    def event(text):controller.sendall(text.encode());assert controller.recv(32)==b'OK',text
    def tap(code):event(f'K {code} 1');event(f'K {code} 0')
    def click(x,y):event(f'M {x} {y}');event('B 272 1');event('B 272 0')
    def command(c):
        before=log().count('TEXT_SYNC '+c+'\n');client.stdin.write(c.encode());client.stdin.flush()
        wait(lambda:log().count('TEXT_SYNC '+c+'\n')>before,'native client commit '+c)
    def preedit(index=0,size=2):return log().count(f'TEXT_PREEDIT {index} {size}')
    def compose(index=0):
        before=preedit(index);tap(127);wait(lambda:preedit(index)>before,'original enabled native preedit')
    def raw_menu():
        before=log().count(' 127 1 ');tap(127);wait(lambda:log().count(' 127 1 ')>before,'nonconsumed key reaches original focused client')
    def close(p):p.stdin.write(b'q');p.stdin.flush();assert p.wait(timeout=6)==0;p.stdin.close();children.remove(p)
    try:
        server=start(server_bin,'server','--headless','compose','-',issuer.fileno(),'--compose',pass_fds=(issuer.fileno(),));issuer.close()
        wait(lambda:'READY ' in log('server'),'real built-in provider')
        client=start(client_bin,'client','--text-input')
        wait(lambda:'FRAME 1 ' in log() and 'TEXT_ENTER 0 ' in log(),'two independent text-input objects')
        click(100,100);wait(lambda:log().rfind('KEYBOARD_ENTER 0')>log().rfind('KEYBOARD_LEAVE'),'original focused surface')
        command('E');compose();before=preedit(size=0);command('R')
        wait(lambda:preedit(size=0)>before,'repeated enable resets same-surface field')
        before=log().count(' 30 1 ');tap(30);wait(lambda:log().count(' 30 1 ')>before,'next field receives ordinary a')
        assert 'TEXT_COMMIT ' not in log()
        cases.append('committed repeated enable resets composition even with unchanged wlroots enabled state')
        compose();before=preedit(size=0);command('C');assert preedit(size=0)==before
        event('K 42 1');tap(40);event('K 42 0');tap(30)
        wait(lambda:'TEXT_COMMIT 0 2 c3 a4\n' in log(),'ordinary cursor update preserves exact Compose result')
        assert log().count('TEXT_COMMIT 0 2 c3 a4\n')==1
        cases.append('cursor-only commit preserves draft and native UTF-8 result once')
        for flag in ('P','I','H','S'):
            command('N');compose();before=preedit(size=0);command(flag)
            wait(lambda:preedit(size=0)>before,'secret content transition clears draft')
            before=log().count('TEXT_PREEDIT ');raw_menu();command('C');assert log().count('TEXT_PREEDIT ')==before
        command('N')
        cases.append('password, PIN, hidden and sensitive fields exclude the provider without discarding ordinary key input')
        compose();before=preedit(size=0);command('A');assert preedit(size=0)==before
        event('K 42 1');tap(40);event('K 42 0');tap(30)
        wait(lambda:log().count('TEXT_COMMIT 0 2 c3 a4\n')==2,'competing enable cannot steal original composition')
        command('X');before=log().count('TEXT_PREEDIT ');raw_menu();command('C')
        assert log().count('TEXT_PREEDIT ')==before and 'TEXT_COMMIT 1 ' not in log()
        command('A');compose(1);tap(1);command('B');command('E')
        cases.append('competing enable is ignored and requires a fresh enable after original disable')
        compose();command('T');before=log().count('TEXT_PREEDIT ');raw_menu();command('C')
        assert log().count('TEXT_PREEDIT ')==before
        command('E');compose();tap(1)
        cases.append('destroyed resource and a replacement cannot inherit original field admission or draft')
        compose();before=log().count('TEXT_PREEDIT ')
        peer=start(client_bin,'peer','--input');wait(lambda:'FRAME 1 ' in log('peer'),'foreign native client')
        click(4580,100);wait(lambda:log().rfind('KEYBOARD_LEAVE')>log().rfind('KEYBOARD_ENTER'),'actual other-client focus')
        command('E');before=log().count('TEXT_PREEDIT ')
        n=log('peer').count(' 127 1 ');tap(127);wait(lambda:log('peer').count(' 127 1 ')>n,'foreign focused client receives own key')
        command('C');assert log().count('TEXT_PREEDIT ')==before
        close(peer);click(100,100)
        wait(lambda:log().rfind('KEYBOARD_ENTER')>log().rfind('KEYBOARD_LEAVE'),'original returns with fresh enter')
        before=log().count('TEXT_PREEDIT ');raw_menu();command('C');assert log().count('TEXT_PREEDIT ')==before
        command('E');compose();tap(1)
        cases.append('unfocused enable cannot steal keys or reactivate automatically after a later enter')
        command('X');command('U');left=log().count('KEYBOARD_LEAVE')
        peer=start(client_bin,'peer-pending','--input')
        wait(lambda:'FRAME 1 ' in log('peer-pending') and log().count('KEYBOARD_LEAVE')>left,'another focus epoch before pending enable commit')
        close(peer);click(100,100)
        wait(lambda:log().rfind('KEYBOARD_ENTER')>log().rfind('KEYBOARD_LEAVE'),'fresh enter after uncommitted old enable')
        command('V');before=log().count('TEXT_PREEDIT ');tap(127);command('C')
        assert log().count('TEXT_PREEDIT ')==before,'enable from before focus change must not become admitted on a later commit'
        command('E');compose();tap(1)
        cases.append('an uncommitted enable cannot cross an actual focus epoch and become fresh by committing later')
        close(client);server.terminate();assert server.wait(timeout=6)==0;children.remove(server)
        assert not re.search(r'AddressSanitizer|LeakSanitizer|runtime error:',log()+log('peer')+log('peer-pending')+log('server'))
        assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest()==h for p,h in binaries.items())
        result={'result':'passed','cases':cases,'binaries':binaries,'scope':'native built-in Compose routing; no external IME/session/app authority acceptance'}
        (root/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({**result,'artifacts':str(root)}))
    finally:
        for fd in reversed(pins):
            try:signal.pidfd_send_signal(fd,signal.SIGKILL)
            except ProcessLookupError:pass
            finally:os.close(fd)
        for p in children:
            p.wait(timeout=5)
            if p.stdin:p.stdin.close()
        for f in streams:f.close()
        controller.close();issuer.close();print('Compose lifecycle evidence:',root)


if __name__=='__main__':main()
