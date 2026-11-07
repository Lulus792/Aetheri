"""Native Editor with an explicitly controlled fsync adapter on its worker.

Real file bytes, original document/UI and thread ownership; the pause/fault is
test-only, not an installed file API or physical slow-disk/power-loss proof.
"""
import gzip
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
    assert len(sys.argv)==6 and os.getuid()!=0
    server_bin,app_bin,font,mono,build=map(lambda p:Path(p).resolve(),sys.argv[1:])
    root=Path(tempfile.mkdtemp(prefix='es-',dir=build));runtime=root/'run';runtime.mkdir(mode=0o700)
    assert len(os.fsencode(runtime/'es'))<108
    folder=root/'documents';folder.mkdir(mode=0o700);(folder/'doc.txt').write_bytes(b'base')
    captures=root/'captures';captures.mkdir(mode=0o700)
    env={**os.environ,'XDG_RUNTIME_DIR':str(runtime),'WAYLAND_DISPLAY':'es','NEXAUI_PLATFORM':'wayland',
         'DBUS_SESSION_BUS_ADDRESS':'unix:path='+str(root/'absent-bus'),
         'ASAN_OPTIONS':'detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS':'halt_on_error=1:print_stacktrace=1'}
    for name in ('DISPLAY','WAYLAND_SOCKET','AT_SPI_BUS_ADDRESS'):env.pop(name,None)
    left,right=socket.socketpair(socket.AF_UNIX,socket.SOCK_SEQPACKET);left.settimeout(4)
    command_read,command_write=os.pipe2(os.O_CLOEXEC);children=[];streams=[];images=[];app_pin=-1
    def start(binary,name,*args,pass_fds=(),extra=None):
        stream=(root/(name+'.log')).open('xb');streams.append(stream)
        child=subprocess.Popen([str(binary),*map(str,args)],env={**env,**(extra or {})},stdin=subprocess.DEVNULL,stdout=stream,stderr=subprocess.STDOUT,pass_fds=pass_fds)
        children.append(child);return child
    def log(name='app'):
        path=root/(name+'.log');return path.read_text(errors='replace') if path.exists() else ''
    def view():
        frames=re.findall(r'^FRAME (.+)$',log(),re.M)
        return dict(re.findall(r'(\w+)=([^ ]+)',frames[-1])) if frames else {}
    def state(**values):return all(view().get(k)==str(v) for k,v in values.items())
    def wait(test,label):
        deadline=time.monotonic()+14
        while not test():
            assert all(p.poll() is None for p in children),(label,[(p.pid,p.poll()) for p in children],log()[-2600:])
            assert time.monotonic()<deadline,(label,log()[-2600:])
            time.sleep(.01)
    def event(text):left.sendall(text.encode());assert left.recv(32)==b'OK'
    def key(code,ctrl=False):
        if ctrl:event('K 29 1')
        event(f'K {code} 1');event(f'K {code} 0')
        if ctrl:event('K 29 0')
    codes=dict(zip('abcdefghijklmnopqrstuvwxyz',(30,48,46,32,18,33,34,35,23,36,37,38,50,49,24,25,16,19,31,20,22,47,17,45,21,44)));codes['.']=52
    def typing(text):
        for c in text:key(codes[c])
    def click(name):
        previous=log().rsplit('FRAME ',2)[-2]
        rows=re.findall(r'^CONTROL \d+ ([\d.]+) ([\d.]+) ([\d.]+) ([\d.]+) '+re.escape(name)+r'$',previous,re.M);assert rows,(name,previous[-1600:])
        x,y,w,h=map(float,rows[-1]);event(f'M {40+x+w/2} {40+y+h/2}');event('B 272 1');event('B 272 0')
    def pause(number):wait(lambda:f'SAVE_PAUSE {number}\n' in log() and state(io=1),'original fsync paused '+str(number))
    def release(number,fault=False):
        assert os.write(command_write,b'f' if fault else b'c')==1
        wait(lambda:log().count('SAVE complete=')>=number and state(io=0),'original worker joined '+str(number))
    def capture():
        server.send_signal(signal.SIGUSR1)
        wait(lambda:all(f'CAPTURE 1 {i}\n' in log('server') for i in range(3)),'native editable saving state captured')
    try:
        server=start(server_bin,'server','--headless',env['WAYLAND_DISPLAY'],captures,right.fileno(),pass_fds=(right.fileno(),));right.close()
        wait(lambda:'READY ' in log('server'),'private compositor')
        app=start(app_bin,'app',font,'--mono-font',mono,'--directory',folder,'--diagnostics','doc.txt',pass_fds=(command_read,),extra={'AETHERI_TEST_SAVE_FD':str(command_read)})
        app_pin=os.pidfd_open(app.pid);os.close(command_read);command_read=-1
        wait(lambda:state(docs=1,focused=1),'original native editor');original=view()['active']
        typing('first');wait(lambda:state(cursor=5,dirty=1),'original unsaved text');key(31,ctrl=True);pause(1)
        assert (folder/'doc.txt').read_bytes()==b'base'
        typing('later');wait(lambda:state(cursor=10,io=1,dirty=1),'UI edits while original fsync is blocked')
        focus=view()['focus'];key(64);wait(lambda:view().get('focus')!=focus and state(io=1),'toolbar while saving');key(64);wait(lambda:view().get('focus')==focus,'document focus restored')
        key(49,ctrl=True);wait(lambda:state(docs=2,cursor=0),'new tab during save');typing('other');wait(lambda:state(cursor=5),'independent tab editing')
        key(17,ctrl=True);wait(lambda:int(view().get('review','0'))!=0,'other-tab close review');click('Discard');wait(lambda:state(docs=1,active=original,io=1,cursor=10),'other-tab close keeps original save')
        capture();release(1);assert (folder/'doc.txt').read_bytes()==b'firstbase';assert state(cursor=10,dirty=1)
        key(44,ctrl=True);wait(lambda:state(cursor=5,dirty=0),'Undo returns to exact saved snapshot');key(21,ctrl=True);wait(lambda:state(cursor=10,dirty=1),'Redo retains later unsaved text')
        key(31,ctrl=True);pause(2);key(17,ctrl=True);wait(lambda:state(pending=original,io=1),'close binds original pending save')
        key(1);wait(lambda:state(pending=0,io=1),'Escape cancels pending close');typing('tail');wait(lambda:state(cursor=14),'edits after cancelled close')
        key(16,ctrl=True);wait(lambda:state(closing=1,io=1),'window close waits for original worker');key(1);wait(lambda:state(closing=0,io=1),'Escape retains window while save continues')
        release(2);assert (folder/'doc.txt').read_bytes()==b'firstlaterbase' and state(cursor=14,dirty=1)
        key(17,ctrl=True);wait(lambda:state(review=original),'original close review');click('Save');pause(3)
        click('Cancel');wait(lambda:state(review=0,io=1),'Cancel review while saving');typing('new');wait(lambda:state(cursor=17),'continued original document after cancelled review')
        release(3);assert state(docs=1,review=0,cursor=17,dirty=1) and (folder/'doc.txt').read_bytes()==b'firstlatertailbase'
        key(17,ctrl=True);wait(lambda:state(review=original),'fresh close review');click('Save');pause(4);release(4)
        wait(lambda:state(docs=0,review=0),'exact original review closes after successful save');assert (folder/'doc.txt').read_bytes()==b'firstlatertailnewbase'
        key(49,ctrl=True);wait(lambda:state(docs=1),'new first-save document');typing('first');wait(lambda:state(cursor=5),'first-save text');key(31,ctrl=True);wait(lambda:state(mode=2),'first destination picker')
        typing('fresh.txt');wait(lambda:state(draft=9),'first literal destination');key(28);pause(5)
        typing('extra');wait(lambda:state(cursor=10,io=1),'new document editable during first save');assert not (folder/'fresh.txt').exists()
        (folder/'fresh.txt').write_bytes(b'outside');release(5);assert state(dirty=1,error=1) and (folder/'fresh.txt').read_bytes()==b'outside'
        key(31,ctrl=True);wait(lambda:state(mode=2),'known failure remains untitled');typing('new.txt');wait(lambda:state(draft=7),'explicit replacement destination');key(28);pause(6);release(6)
        assert state(dirty=0) and (folder/'new.txt').read_bytes()==b'firstextra'
        typing('last');wait(lambda:state(cursor=14),'newer text before external conflict');key(31,ctrl=True);pause(7)
        (folder/'new.txt').write_bytes(b'changed elsewhere');release(7);assert state(dirty=1,error=1) and (folder/'new.txt').read_bytes()==b'changed elsewhere'
        key(17,ctrl=True);wait(lambda:int(view().get('review','0'))!=0,'external conflict retains close review');click('Discard');wait(lambda:state(docs=0),'explicit discard after known conflict')
        key(49,ctrl=True);wait(lambda:state(docs=1),'separate uncertainty document');typing('stable');wait(lambda:state(cursor=6),'known bytes before uncertainty');key(31,ctrl=True);wait(lambda:state(mode=2),'uncertainty fixture destination')
        typing('unknown.txt');wait(lambda:state(draft=11),'uncertainty literal name');key(28);pause(8);release(8);assert state(dirty=0,uncertain=0)
        key(31,ctrl=True);pause(9);release(9,True);assert state(dirty=0,uncertain=1,error=1) and (folder/'unknown.txt').read_bytes()==b'stable'
        unknown=view()['active'];key(17,ctrl=True);wait(lambda:state(review=unknown),'clean text still requires UNKNOWN review')
        click('Discard');key(1);wait(lambda:state(review=0,docs=1,uncertain=1),'disabled discard never drops uncertainty');key(17,ctrl=True);wait(lambda:state(review=unknown),'same original UNKNOWN remains retained');click('Cancel');wait(lambda:state(review=0,io=0),'UNKNOWN retained without active worker')
        assert log().count('SAVE begin=')==9
        # Explicit administrative retirement of this separate uncertainty
        # fixture. This is NOT successful user close/recovery or lease drain.
        signal.pidfd_send_signal(app_pin,signal.SIGKILL);assert app.wait(timeout=8)==-signal.SIGKILL;children.remove(app)
        os.close(app_pin);app_pin=-1;server.terminate();assert server.wait(timeout=8)==0;children.remove(server)
        for i in range(3):
            path=captures/f'capture-1-output-{i}.ppm';raw=path.read_bytes();packed=path.with_suffix('.ppm.gz');packed.write_bytes(gzip.compress(raw,mtime=0));assert gzip.decompress(packed.read_bytes())==raw;path.unlink()
            images.append(dict(file=packed.name,output=i,sha256=hashlib.sha256(raw).hexdigest()))
        for path in root.glob('*.log'):assert not re.search(r'AddressSanitizer|LeakSanitizer|runtime error:',path.read_text(errors='replace')),path
        result=dict(result='passed',scope='native UI and controlled real fsync adapter; not physical disk, power-loss, normal app admission or UNKNOWN recovery',
                    images=images,saves=9,unknown_fixture='original pidfd administratively retired after worker join, retained file bytes',
                    binaries={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in (server_bin,app_bin)})
        (root/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(dict(result='passed',artifacts=str(root),saves=9)))
    finally:
        if app_pin>=0:os.close(app_pin)
        for child in reversed(children):
            if child.poll() is None:
                child.terminate()
                try:child.wait(timeout=5)
                except subprocess.TimeoutExpired:child.kill();child.wait(timeout=5)
        for stream in streams:stream.close()
        if command_read>=0:os.close(command_read)
        os.close(command_write);left.close();right.close();print('Editor save evidence:',root)


if __name__=='__main__':main()
