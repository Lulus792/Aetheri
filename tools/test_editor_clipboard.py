"""Native Editor clipboard with an independent Wayland producer/receiver.

Explicit private disposable-carrier test. No installed authority or user seat.
"""
import hashlib
import gzip
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

ROOT=Path(__file__).resolve().parents[1]


def digest(path): return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    assert len(sys.argv)==7 and os.getuid()!=0
    server_bin,app_bin,peer_bin,font,mono,build=map(lambda p:Path(p).resolve(),sys.argv[1:])
    assert digest(font)==json.loads((ROOT/'spec/ui-inputs.json').read_text())['font']['sha256']
    root=Path(tempfile.mkdtemp(prefix='ec-',dir=build));runtime=root/'run';runtime.mkdir(mode=0o700)
    assert len(os.fsencode(runtime/'ec'))<108
    folder=root/'documents';folder.mkdir(mode=0o700);document=folder/'doc.txt';document.write_bytes(b'base')
    captures=root/'captures';captures.mkdir(mode=0o700)
    env={**os.environ,'XDG_RUNTIME_DIR':str(runtime),'WAYLAND_DISPLAY':'ec','NEXAUI_PLATFORM':'wayland',
         'DBUS_SESSION_BUS_ADDRESS':'unix:path='+str(root/'absent-bus'),
         'ASAN_OPTIONS':'detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS':'halt_on_error=1:print_stacktrace=1'}
    for key in ('DISPLAY','WAYLAND_SOCKET','AT_SPI_BUS_ADDRESS'):env.pop(key,None)
    controller,issuer=socket.socketpair(socket.AF_UNIX,socket.SOCK_SEQPACKET);controller.settimeout(4)
    children=[];streams=[];cases=[];images=[]
    def start(binary,name,*args,pass_fds=()):
        stream=(root/(name+'.log')).open('xb');streams.append(stream)
        p=subprocess.Popen([str(binary),*map(str,args)],env=env,stdin=subprocess.PIPE,stdout=stream,stderr=subprocess.STDOUT,pass_fds=pass_fds)
        children.append(p);return p
    def log(name='app'):
        path=root/(name+'.log');return path.read_text(errors='replace') if path.exists() else ''
    def view():
        rows=re.findall(r'^FRAME (.+)$',log(),re.M)
        return dict(re.findall(r'(\w+)=([^ ]+)',rows[-1])) if rows else {}
    def state(**values):return all(view().get(k)==str(v) for k,v in values.items())
    def wait(test,label,timeout=12):
        until=time.monotonic()+timeout
        while not test():
            assert all(p.poll() is None for p in children),(label,[(p.pid,p.poll()) for p in children],log()[-2200:])
            assert time.monotonic()<until,(label,log()[-2200:])
            time.sleep(.01)
    def event(text):controller.sendall(text.encode());assert controller.recv(32)==b'OK'
    def key(code,ctrl=False):
        if ctrl:event('K 29 1')
        event(f'K {code} 1');event(f'K {code} 0')
        if ctrl:event('K 29 0')
    def focus_editor():
        event('M 500 200');event('B 272 1');event('B 272 0')
        wait(lambda:state(focused=1),'actual Editor keyboard focus')
    def focus_peer(name):
        event('M 4580 100');event('B 272 1');event('B 272 0')
        wait(lambda:log(name).rfind('KEYBOARD_ENTER 0\n')>log(name).rfind('KEYBOARD_LEAVE\n') and state(focused=0),'actual independent peer focus')
    def command(peer,name,c):
        before=log(name).count(f'CLIPBOARD_SYNC {c}\n');peer.stdin.write(c.encode());peer.stdin.flush()
        wait(lambda:log(name).count(f'CLIPBOARD_SYNC {c}\n')>before,'peer '+c)
    def source(name,payload=None):
        if payload is None:peer=start(peer_bin,name,'--clipboard-stall')
        else:
            path=root/(name+'.payload');path.write_bytes(payload);peer=start(peer_bin,name,'--clipboard-file',path)
        wait(lambda:'FRAME 1 ' in log(name),'independent peer mapped')
        peer.stdin.write(b'df');peer.stdin.flush()
        wait(lambda:re.search(r'^FRAME 0 \d+ \d+ 1$',log(name),re.M),'independent fullscreen on output2')
        focus_peer(name);return peer
    def publish(peer,name):
        focus_peer(name);command(peer,name,'C');focus_editor()
    def retire(peer):
        peer.stdin.write(b'q');peer.stdin.flush();assert peer.wait(timeout=6)==0;peer.stdin.close();children.remove(peer)
        focus_editor()
    def saved(expected):
        before=log().count('RESULT ');key(31,ctrl=True)
        wait(lambda:log().count('RESULT ')>before,'original named Save result')
        assert re.findall(r'^RESULT (\d+)$',log(),re.M)[-1]=='0'
        assert document.read_bytes()==expected,'actual saved document bytes'
    def paste_done():
        before=log().count('PASTE complete=');started=log().count('PASTE begin\n');key(47,ctrl=True)
        wait(lambda:log().count('PASTE begin\n')>started,'native read begun')
        wait(lambda:log().count('PASTE complete=')>before and state(paste=0),'native read completed')
    def stalled(peer,select=True):
        publish(peer,'slow')
        if select:key(30,ctrl=True);wait(lambda:state(selected=1),'selection before pending paste')
        else:key(107,ctrl=True);wait(lambda:state(selected=0,cursor=len(payload)),'original EOF before pending paste')
        before=log('slow').count('CLIPBOARD_STALLED');key(47,ctrl=True)
        wait(lambda:log('slow').count('CLIPBOARD_STALLED')>before and state(paste=1),'producer holds actual pipe writer')
        return time.monotonic()
    def broken(peer):
        before=log('slow').count('CLIPBOARD_STALL_BROKEN');command(peer,'slow','T')
        wait(lambda:log('slow').count('CLIPBOARD_STALL_BROKEN')>before,'original native reader actually closed')
    try:
        server=start(server_bin,'server','--headless',env['WAYLAND_DISPLAY'],captures,issuer.fileno(),pass_fds=(issuer.fileno(),));issuer.close()
        wait(lambda:'READY ' in log('server'),'private compositor')
        app=start(app_bin,'app',font,'--mono-font',mono,'--directory',folder,'--diagnostics','doc.txt')
        wait(lambda:state(docs=1,focused=1),'initial native editor');original=view()['active']
        payload='Grüße 👩\u200d💻\nsecond'.encode()
        peer=source('unicode',payload);publish(peer,'unicode');key(30,ctrl=True);paste_done()
        wait(lambda:state(cursor=len(payload)),'Unicode native paste');saved(payload)
        key(44,ctrl=True);wait(lambda:state(cursor=4),'one complete paste undo');saved(b'base')
        key(21,ctrl=True);wait(lambda:state(cursor=len(payload)),'one complete paste redo');saved(payload)
        key(30,ctrl=True);key(46,ctrl=True);focus_peer('unicode');command(peer,'unicode','R')
        record=re.findall(r'^CLIPBOARD_READ (.+)$',log('unicode'),re.M)[-1].split()
        assert int(record[0])==len(payload) and bytes(int(b,16) for b in record[1:])==payload
        focus_editor();key(30,ctrl=True);key(45,ctrl=True);wait(lambda:state(cursor=0),'cut after native copy');saved(b'')
        paste_done();wait(lambda:state(cursor=len(payload)),'native own-source paste');saved(payload)
        cases.append('real Unicode producer -> exact bytes; one undo/redo; independent copy receiver; cut and own-source paste')
        retire(peer)
        slow=source('slow')
        stalled(slow,False)
        server.send_signal(signal.SIGUSR1)
        wait(lambda:all(f'CAPTURE 1 {i}\n' in log('server') for i in range(3)),'native receiving status captured')
        assert state(paste=1),'capture must show the actual pending receive'
        key(106);wait(lambda:state(paste=0,cursor=len(payload)),'no-op EOF gesture cancels original input token');broken(slow)
        saved(payload);cases.append('unchanged EOF/caret still consumes original receive correlation')
        stalled(slow);before=log('slow').count('CLIPBOARD_STALLED');key(47,ctrl=True)
        wait(lambda:log('slow').count('CLIPBOARD_STALLED')>before and state(paste=1),'explicit second paste owns a new reader')
        broken(slow);assert state(paste=1),'old reader retirement must not cancel new request'
        key(1);wait(lambda:state(paste=0),'cancel replacement request');broken(slow);saved(payload)
        cases.append('repeated explicit paste retires only old pipe, new request remains separately cancellable')
        for label,action in (('cursor movement',lambda:key(106)),('toolbar focus',lambda:key(64)),
                             ('Escape',lambda:key(1)),('new document',lambda:key(49,ctrl=True)),
                             ('open dialog',lambda:key(24,ctrl=True))):
            started=stalled(slow);action();wait(lambda:state(paste=0),'pending read cancelled by '+label)
            assert time.monotonic()-started<1.5,label;broken(slow)
            if label=='toolbar focus':key(64)
            if label=='new document':
                wait(lambda:state(docs=2,cursor=0),'new original document while source stalled');key(17,ctrl=True)
                wait(lambda:state(docs=1,active=original),'close clean new tab')
            if label=='open dialog':
                wait(lambda:state(mode=1),'picker stays responsive');key(1);wait(lambda:state(mode=0),'cancel picker')
            saved(payload);assert state(active=original)
            cases.append('pending pipe cancelled without replay: '+label)
        stalled(slow);key(45);wait(lambda:state(cursor=1,paste=0),'typing replaces selection and cancels old paste');broken(slow);saved(b'x')
        key(44,ctrl=True);wait(lambda:state(cursor=len(payload)),'undo independent typing');saved(payload)
        cases.append('typing consumes old receive correlation and preserves original undo')
        stalled(slow);focus_peer('slow');wait(lambda:state(paste=0,focused=0),'actual keyboard leave cancels receive');broken(slow)
        focus_editor();saved(payload);cases.append('independent native focus loss closes reader and never resumes')
        started=stalled(slow)
        while state(paste=1) and time.monotonic()-started<7:
            command(slow,'slow','T');time.sleep(.12)
        wait(lambda:state(paste=0,error=1),'bounded overall timeout')
        elapsed=time.monotonic()-started;assert 4.5<elapsed<7,elapsed
        # The last keepalive may itself observe the reader already closed.
        if 'CLIPBOARD_STALL_BROKEN' not in log('slow').rsplit('CLIPBOARD_STALLED',1)[-1]:broken(slow)
        saved(payload);cases.append('five-second overall deadline rejects partial live transfer without document mutation')
        retire(slow)
        for name,data in (('invalid-utf8',b'\xff'),('embedded-nul',b'bad\0text'),('line-limit',b'x'*65536)):
            peer=source(name,data);publish(peer,name);key(107,ctrl=True);paste_done()
            assert state(error=1,cursor=len(payload));saved(payload);retire(peer)
            cases.append('failed native/model paste preserves original bytes: '+name)
        # Actual focus return precedes pointer/key release and its draw. Wait
        # for those finite setup frames before measuring the unchanged idle
        # budget; continuous repaint still fails this bounded precondition.
        quiet=[log().count('FRAME '),time.monotonic()]
        def settled():
            frames=log().count('FRAME ')
            if frames!=quiet[0]:quiet[:]=[frames,time.monotonic()]
            return time.monotonic()-quiet[1]>.15
        wait(settled,'native focus/input frames settled before idle')
        frames=log().count('FRAME ');stat=Path(f'/proc/{app.pid}/stat')
        def ticks():
            values=stat.read_text().rsplit(')',1)[1].split();return int(values[11])+int(values[12])
        initial=ticks();time.sleep(1.2);delta=ticks()-initial
        assert delta<=2 and log().count('FRAME ')==frames,(delta,frames,log().count('FRAME '))
        key(16,ctrl=True);assert app.wait(timeout=8)==0;app.stdin.close();children.remove(app)
        server.terminate();assert server.wait(timeout=8)==0;server.stdin.close();children.remove(server)
        for i in range(3):
            path=captures/f'capture-1-output-{i}.ppm';raw=path.read_bytes()
            magic,size,peak,pixels=raw.split(b'\n',3);width,height=map(int,size.split())
            assert magic==b'P6' and peak==b'255' and len(pixels)==width*height*3
            if i==0:assert len(set(pixels))>20
            packed=path.with_suffix('.ppm.gz');packed.write_bytes(gzip.compress(raw,mtime=0))
            assert gzip.decompress(packed.read_bytes())==raw;path.unlink()
            images.append(dict(file=packed.name,output=i,sha256=hashlib.sha256(raw).hexdigest()))
        for path in root.glob('*.log'):assert not re.search(r'AddressSanitizer|LeakSanitizer|runtime error:',path.read_text(errors='replace')),path
        result=dict(result='passed',scope='private native Editor/Wayland clipboard; no installed permissions or hardware acceptance',cases=cases,
                    timeout_seconds=elapsed,idle_cpu_ticks=delta,images=images,binaries={str(p):digest(p) for p in (server_bin,app_bin,peer_bin)})
        (root/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(dict(result='passed',artifacts=str(root),cases=len(cases))))
    finally:
        for child in reversed(children):
            if child.poll() is None:
                child.terminate()
                try:child.wait(timeout=5)
                except subprocess.TimeoutExpired:child.kill();child.wait(timeout=5)
            if child.stdin:child.stdin.close()
        for stream in streams:stream.close()
        controller.close();issuer.close();print('Editor clipboard evidence:',root)


if __name__=='__main__':main()
