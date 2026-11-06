"""Actual NexaUI worker effects followed by controlled lost acknowledgements.

The separately linked probe and inherited native input exist only in this test.
No normal binary hook, root controller or production recovery claim.
"""
import gzip
import hashlib
import json
import os
from pathlib import Path
import re
import select
import signal
import socket
import subprocess
import sys
import tempfile
import time


def main():
    server_bin,probe,font,terminal,editor,mono,build=map(lambda p:Path(p).resolve(),sys.argv[1:]);assert os.getuid()!=0
    root=Path(tempfile.mkdtemp(prefix='files-view-fault-',dir=build));runtime=root/'runtime';runtime.mkdir(mode=0o700)
    captures=root/'captures';captures.mkdir(mode=0o700)
    env={**os.environ,'XDG_RUNTIME_DIR':str(runtime),'WAYLAND_DISPLAY':'files-fault-private','NEXAUI_PLATFORM':'wayland',
        'DBUS_SESSION_BUS_ADDRESS':'unix:path='+str(root/'absent'),'LIBGL_ALWAYS_SOFTWARE':'1','GALLIUM_DRIVER':'llvmpipe','LP_NUM_THREADS':'2',
        'ASAN_OPTIONS':'detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS':'halt_on_error=1:print_stacktrace=1'}
    for name in ('DISPLAY','WAYLAND_SOCKET','AT_SPI_BUS_ADDRESS'):env.pop(name,None)
    left,right=socket.socketpair(socket.AF_UNIX,socket.SOCK_SEQPACKET);left.settimeout(4)
    children=[];streams=[];pins=[];images=[];cases=[];active='';sequence=0
    def start(binary,name,*args,cwd=root,pass_fds=()):
        stream=(root/(name+'.log')).open('xb');streams.append(stream)
        p=subprocess.Popen([str(binary),*map(str,args)],cwd=cwd,env=env,stdin=subprocess.DEVNULL,stdout=stream,stderr=subprocess.STDOUT,pass_fds=pass_fds)
        children.append(p);return p
    def log(name=None):return (root/((name or active)+'.log')).read_text(errors='replace')
    def wait(predicate,label):
        deadline=time.monotonic()+12
        while not predicate():
            assert all(p.poll() is None for p in children) and time.monotonic()<deadline,(label,root,log()[-3000:]);time.sleep(.01)
    def state(**wanted):
        found=re.findall(r'^VIEW (.+)$',log(),re.M);values=dict(re.findall(r'(\w+)=([^ ]+)',found[-1])) if found else {}
        return all(values.get(k)==str(v) for k,v in wanted.items())
    def event(value):left.sendall(value.encode());assert left.recv(32)==b'OK'
    def key(code,ctrl=False,shift=False,alt=False):
        held=([29] if ctrl else [])+([42] if shift else [])+([56] if alt else [])
        for c in held:event(f'K {c} 1')
        event(f'K {code} 1');event(f'K {code} 0')
        for c in reversed(held):event(f'K {c} 0')
    codes=dict(zip('abcdefghijklmnopqrstuvwxyz',(30,48,46,32,18,33,34,35,23,36,37,38,50,49,24,25,16,19,31,20,22,47,17,45,21,44)));codes['-']=12
    def typing(value):
        for c in value:key(codes[c])
    def capture(label,active_output):
        nonlocal sequence
        sequence+=1;signal.pidfd_send_signal(server_pin,signal.SIGUSR1)
        wait(lambda:all(f'CAPTURE {sequence} {i}\n' in log('server') for i in range(3)),label)
        for i in range(3):
            p=captures/f'capture-{sequence}-output-{i}.ppm';raw=p.read_bytes();magic,size,peak,pixels=raw.split(b'\n',3);w,h=map(int,size.split())
            assert magic==b'P6' and peak==b'255' and len(pixels)==w*h*3
            if i==active_output:assert len(set(pixels))>20
            packed=p.with_suffix('.ppm.gz');packed.write_bytes(gzip.compress(raw,mtime=0));assert gzip.decompress(packed.read_bytes())==raw;p.unlink()
            images.append({'case':label,'output':i,'file':packed.name,'sha256':hashlib.sha256(raw).hexdigest()})
    try:
        active='server';server=start(server_bin,'server','--headless',env['WAYLAND_DISPLAY'],captures,right.fileno(),pass_fds=(right.fileno(),));right.close()
        server_pin=os.pidfd_open(server.pid);pins.append(server_pin);wait(lambda:'READY ' in log('server'),'server ready')
        for kind in ('create','rename','move'):
            case=root/kind;case.mkdir(mode=0o700);documents=case/'documents';documents.mkdir(mode=0o700);cache=case/'cache';cache.mkdir(mode=0o700)
            if kind!='create':(documents/'original').write_bytes(b'uncertain bytes')
            if kind=='move':(documents/'target').mkdir(mode=0o700)
            original=(documents/'original').stat().st_ino if kind!='create' else None
            active=kind;app=start(probe,kind,'--root',documents,'--font',font,'--cache',cache,'--terminal',terminal,'--editor',editor,'--terminal-font',mono,'--diagnostics',cwd=case)
            pin=os.pidfd_open(app.pid);pins.append(pin);wait(lambda:state(count=0 if kind=='create' else 1 if kind=='rename' else 2),'initial fault view')
            if kind=='create':key(49,ctrl=True,shift=True);wait(lambda:state(dialog=1),'new folder');typing('unknown-folder')
            elif kind=='rename':key(60);wait(lambda:state(dialog=1),'rename');key(22,ctrl=True);typing('unknown-name')
            else:
                key(38,ctrl=True);typing('original');wait(lambda:state(count=1),'source filter')
                key(108);key(45,ctrl=True)  # Leave the search field before cutting a filesystem entry.
                key(38,ctrl=True);typing('target');wait(lambda:state(count=1),'target filter');key(28);wait(lambda:state(depth=1),'held target')
                key(47,ctrl=True);wait(lambda:state(dialog=1),'move review');key(22,ctrl=True);typing('unknown-name')
            key(28);marker='FAULT_CREATED directory' if kind=='create' else 'FAULT_RENAMED entry'
            wait(lambda:marker in log() and state(creating=1),'actual effect while original worker waits')
            target=documents/'unknown-folder' if kind=='create' else documents/'unknown-name' if kind=='rename' else documents/'target/unknown-name'
            inode=target.stat().st_ino
            if kind!='create':assert inode==original and not (documents/'original').exists() and target.read_bytes()==b'uncertain bytes'
            key(28);key(49,ctrl=True);assert state(creating=1) and not select.select([pin],[],[],0)[0]
            if kind=='move':
                key(16,ctrl=True);wait(lambda:state(closing=1,creating=1),'close waits for original move worker')
                assert not select.select([pin],[],[],0)[0]
            (case/('release-create' if kind=='create' else 'release-rename')).write_bytes(b'release original')
            if kind!='move':
                wait(lambda:state(unknown=1,creating=0,dialog=1),'uncertain inspection')
                key(28);key(22,ctrl=True);typing('blocked');assert state(unknown=1,creating=0)
                key(1);wait(lambda:state(dialog=0,unknown=1),'inspection cancel retains original unknown')
                key(49,ctrl=True);wait(lambda:state(dialog=1,unknown=1),'new request returns to original inspection')
                latest=log().rsplit('VIEW ',1)[-1]
                assert not re.search(r'^CONTROL (14|15) ',latest,re.M),'inspection must not offer confirmation or an editable name'
                close=re.search(r'^CONTROL 13 .* (\d+)$',latest,re.M);assert close and int(close[1])&512,'Close inspection receives focus'
                # The test compositor places successive clients on outputs0/1/2.
                capture(kind+' unknown inspection',0 if kind=='create' else 1);key(16,ctrl=True)
            assert app.wait(timeout=8)==0,log();children.remove(app);assert select.select([pin],[],[],0)[0]
            assert log().count('FAULT_ATTEMPT '+('mkdir' if kind=='create' else 'rename')+'\n')==1
            if kind=='move':assert 'MOVE attempt=1 result=7' in log()
            assert 'OPEN pid=' not in log() and target.stat().st_ino==inode
            assert sorted(p.name for p in documents.iterdir())==(['unknown-folder'] if kind=='create' else ['unknown-name'] if kind=='rename' else ['target'])
            if kind!='create':assert target.read_bytes()==b'uncertain bytes'
            cases.append({'kind':kind,'one_syscall_attempt':True,'inode':inode,'retained_effect':True,'close_wait':kind=='move'})
        server.terminate();assert server.wait(timeout=8)==0;children.remove(server)
        for p in root.glob('*.log'):assert not re.search(r'AddressSanitizer|LeakSanitizer|runtime error:',p.read_text(errors='replace')),p
        result={'result':'passed','cases':cases,'images':images,'binaries':{str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in (server_bin,probe)},
            'scope':'separately linked native probe, actual effects/lost acknowledgements; no production power-loss recovery or grant/removal API'}
        (root/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({'result':'passed','artifacts':str(root)}))
    finally:
        for p in reversed(children):
            if p.poll() is None:
                p.terminate()
                try:p.wait(timeout=6)
                except subprocess.TimeoutExpired:p.kill();p.wait(timeout=5)
        for pin in pins:os.close(pin)
        for stream in streams:stream.close()
        left.close();right.close();print('Files view fault evidence:',root)


if __name__=='__main__':main()
