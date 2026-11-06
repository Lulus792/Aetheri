"""Real private Wayland window with original post-thread shell sessions."""
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
    server_bin,terminal,shell,starter,font,build,peer_bin=map(lambda p:Path(p).resolve(),sys.argv[1:])
    assert os.getuid()==1000
    root=Path(tempfile.mkdtemp(prefix='terminal-multiplex-',dir=build))
    for name in ('runtime','cache','captures'):(root/name).mkdir(mode=0o700)
    env={**os.environ,'XDG_RUNTIME_DIR':str(root/'runtime'),'WAYLAND_DISPLAY':'terminal-multiplex-private',
         'NO_AT_BRIDGE':'1','WAYLAND_DEBUG':'client','GSETTINGS_BACKEND':'memory','LIBGL_ALWAYS_SOFTWARE':'1','GALLIUM_DRIVER':'llvmpipe','LP_NUM_THREADS':'2',
         'ASAN_OPTIONS':'detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS':'halt_on_error=1:print_stacktrace=1'}
    for name in ('DISPLAY','WAYLAND_SOCKET'):env.pop(name,None)
    left,right=socket.socketpair(socket.AF_UNIX,socket.SOCK_SEQPACKET);left.settimeout(4)
    children=[];streams=[];pins=[];shells={};images=[];cases=[]
    def log(name='app'):return (root/(name+'.log')).read_text(errors='replace')
    def start(binary,name,*args,pass_fds=(),pipe=False):
        f=(root/(name+'.log')).open('xb');streams.append(f)
        err=subprocess.STDOUT
        if name=='app':
            err=(root/'app-wire.log').open('xb');streams.append(err)
        p=subprocess.Popen([str(binary),*map(str,args)],cwd=root,env=env,stdin=subprocess.PIPE if pipe else subprocess.DEVNULL,stdout=f,stderr=err,pass_fds=pass_fds)
        children.append(p);pins.append(os.pidfd_open(p.pid));return p
    def fields(text):return {k:float(v) if '.' in v else int(v) for k,v in re.findall(r'(\w+)=(-?[\d.]+)',text)}
    def state():
        blocks=re.findall(r'^WORKSPACE (.+)\n((?:(?:PANE|DIVIDER) .*\n)*)WORKSPACE_END frame=(\d+)\n',log(),re.M)
        if not blocks:return {}
        header,body,end=blocks[-1];result=fields(header);assert result['frame']==int(end)
        result['panes']={};result['dividers']=[]
        for line in body.splitlines():
            row=fields(line)
            if line.startswith('PANE '):result['panes'][row['slot']]=row
            else:result['dividers'].append(row)
        return result
    def wait(check,label):
        end=time.monotonic()+10
        while not check():
            assert time.monotonic()<end,(label,root,log()[-3000:] if (root/'app.log').exists() else '')
            assert all(p.poll() is None for p in children),(label,[(p.pid,p.returncode) for p in children],root)
            time.sleep(.01)
    def event(text):left.sendall(text.encode());assert left.recv(32)==b'OK',text
    def key(code,ctrl=False,shift=False,alt=False):
        for flag,code_mod in ((ctrl,29),(shift,42),(alt,56)):
            if flag:event(f'K {code_mod} 1')
        event(f'K {code} 1');event(f'K {code} 0')
        for flag,code_mod in ((alt,56),(shift,42),(ctrl,29)):
            if flag:event(f'K {code_mod} 0')
    codes=dict(zip('abcdefghijklmnopqrstuvwxyz',(30,48,46,32,18,33,34,35,23,36,37,38,50,49,24,25,16,19,31,20,22,47,17,45,21,44)))
    codes.update({' ':57,'-':12,'\n':28})
    def typing(text):
        for c in text:key(52,shift=True) if c=='>' else key(codes[c])
    def clipboard():
        rows=re.findall(r'^CLIPBOARD state (.+)$',log(),re.M)
        return fields(rows[-1]) if rows else {}
    def peer_command(p,name,command):
        before=log(name).count(f'CLIPBOARD_SYNC {command}\n')
        p.stdin.write(command.encode());p.stdin.flush()
        wait(lambda:log(name).count(f'CLIPBOARD_SYNC {command}\n')>before,'independent peer '+command)
    def publish(name,payload=None):
        if payload is None:p=start(peer_bin,name,'--clipboard-stall',pipe=True)
        else:
            path=root/(name+'.payload');path.write_bytes(payload)
            p=start(peer_bin,name,'--clipboard-file',path,pipe=True)
        wait(lambda:'FRAME 1 ' in log(name),'independent clipboard windows')
        p.stdin.write(b'df');p.stdin.flush()
        wait(lambda:re.search(r'^FRAME 0 \d+ \d+ 1$',log(name),re.M),'clipboard peer on third output')
        event('M 4580 100');event('B 272 1');event('B 272 0')
        wait(lambda:log(name).rfind('KEYBOARD_ENTER 0\n')>log(name).rfind('KEYBOARD_LEAVE\n') and clipboard().get('focused')==0,'peer actual focus')
        peer_command(p,name,'C')
        pane=state()['panes'][state()['active']]
        event(f'M {40+pane["left"]+5} {40+pane["top"]+10}');event('B 272 1');event('B 272 0')
        wait(lambda:clipboard().get('focused')==1 and clipboard().get('available')==1,'original pane focus and clipboard offer')
        return p
    def close_peer(p):
        p.stdin.write(b'q');p.stdin.flush();assert p.wait(timeout=8)==0
        p.stdin.close();children.remove(p)
    def cancel_pending(name,switch,active):
        p=publish(name);before={i:(p['x'],p['y']) for i,p in state()['panes'].items()}
        focus=len(re.findall(r'^FOCUS [01]\n',log(),re.M));key(47,ctrl=True,shift=True)
        wait(lambda:'CLIPBOARD_STALLED ' in log(name),'independent producer holds original transfer')
        switch()
        wait(lambda:state()['active']==active and clipboard().get('receiving')==0,'internal focus cancels original pending paste')
        peer_command(p,name,'T');wait(lambda:'CLIPBOARD_STALL_BROKEN\n' in log(name),'original receive pipe is closed')
        assert clipboard().get('focused')==1 and len(re.findall(r'^FOCUS [01]\n',log(),re.M))==focus
        assert {i:(p['x'],p['y']) for i,p in state()['panes'].items()}==before
        close_peer(p)
        cases.append(name+': same-window switch closes old receive FD without retargeting either shell')
    def ready(count=None):
        s=state()
        return bool(s) and (count is None or s['sessions']==count) and s['panes'][s['active']]['x']==9 and not s['panes'][s['active']]['closing']
    def capture_shells():
        for p in state()['panes'].values():
            cookie=p['cookie']
            if cookie in shells:continue
            fd=os.pidfd_open(p['pid']);pins.append(fd)
            status=Path(f'/proc/{p["pid"]}/stat').read_text().rsplit(')',1)[1].split()
            actual=os.stat(f'/proc/{p["pid"]}/exe');expected=os.stat(shell)
            assert int(status[1])==app.pid and (actual.st_dev,actual.st_ino)==(expected.st_dev,expected.st_ino)
            assert all(os.fstat(fd).st_ino!=a['inode'] for a in shells.values())
            shells[cookie]={'pid':p['pid'],'pin':fd,'inode':os.fstat(fd).st_ino}
    def proof(text,name,value):
        typing(text);wait(lambda:(root/name).exists() and (root/name).read_bytes()==value,name)
        wait(ready,'returned actual prompt')
    def snapshot(label):
        wait(lambda:state() and f'DRAWN {state()["frame"]}\n' in log(),'compositor-observed current frame')
        number=len(images)//3+1;server.send_signal(signal.SIGUSR1)
        wait(lambda:all(f'CAPTURE {number} {i}\n' in log('server') for i in range(3)),label)
        for i in range(3):
            p=root/'captures'/f'capture-{number}-output-{i}.ppm';raw=p.read_bytes();magic,size,peak,pixels=raw.split(b'\n',3);w,h=map(int,size.split())
            assert magic==b'P6' and peak==b'255' and len(pixels)==w*h*3
            if i==0:assert len(set(pixels))>20
            packed=p.with_suffix('.ppm.gz');packed.write_bytes(gzip.compress(raw,mtime=0));p.unlink()
            images.append({'case':label,'output':i,'file':packed.name,'sha256':hashlib.sha256(raw).hexdigest()})
        cases.append(label)
    try:
        server=start(server_bin,'server','--headless',env['WAYLAND_DISPLAY'],root/'captures',right.fileno(),pass_fds=(right.fileno(),));right.close()
        wait(lambda:'READY ' in log('server'),'private compositor')
        event('I on')
        app=start(terminal,'app','--shell',shell,'--pty-starter',starter,'--font',font,'--cache',root/'cache','--diagnostics')
        wait(lambda:ready(1),'original shell and workspace');capture_shells();snapshot('one session has no tab strip')
        proof('printf one > one-proof\n','one-proof',b'one')
        key(32,ctrl=True,shift=True);wait(lambda:ready(2) and state()['visible']==3,'real right split');capture_shells()
        proof('printf two > two-proof\n','two-proof',b'two');snapshot('two independent live shell panes')
        cancel_pending('pending-pane',lambda:key(105,alt=True),0)
        p=publish('finite-pane',b'printf pasted > paste-proof')
        wait(lambda:clipboard().get('barrier')==0,'old native receive barrier settled')
        key(47,ctrl=True,shift=True)
        wait(lambda:state()['panes'][0]['x']==9+len('printf pasted > paste-proof'),'new paste enters only selected original pane')
        assert state()['panes'][1]['x']==9 and not (root/'paste-proof').exists()
        key(28);wait(lambda:(root/'paste-proof').exists() and (root/'paste-proof').read_bytes()==b'pasted' and ready(),'only explicit Enter executes pasted command')
        close_peer(p);cases.append('fresh clipboard receive targets selected pane and never executes automatically')
        key(106,alt=True);wait(lambda:state()['active']==1,'return to original right pane')
        event('I 0');wait(lambda:re.search(r'^VIEW .*composition=7 ',log(),re.M),'real right-pane preedit')
        p=state()['panes'][0];event(f'M {40+p["left"]+5} {40+p["top"]+10}');event('B 272 1');event('B 272 0')
        wait(lambda:state()['active']==0 and re.findall(r'^VIEW (.+)$',log(),re.M)[-1].find('composition=0 ')>=0,'pointer focus retires old-pane draft')
        assert all(p['x']==9 for p in state()['panes'].values())
        cases.append('native pane focus cancels preedit without inserting it into either shell')
        key(105,alt=True);wait(lambda:state()['active']==0,'keyboard focuses left pane')
        typing('printf draft')
        key(20,ctrl=True,shift=True);wait(lambda:ready(3) and state()['tabs']==2,'new tab from original window');capture_shells()
        third=state()['panes'][state()['active']]['cookie'];proof('printf tab > tab-proof\n','tab-proof',b'tab')
        cancel_pending('pending-tab',lambda:key(15,ctrl=True),0)
        key(15,ctrl=True);wait(lambda:state()['panes'][state()['active']]['cookie']==third,'return to third original session')
        event('I 0');wait(lambda:re.findall(r'^VIEW (.+)$',log(),re.M)[-1].find('composition=7 ')>=0,'new-tab preedit')
        stale=log().count('TEXT commit stale');before={i:(p['x'],p['y']) for i,p in state()['panes'].items()}
        event('I 8');wait(lambda:state()['active']==0 and state()['tabs']==2 and log().count('TEXT commit stale')>stale,'prior tab focus rejects old queued commit')
        assert {i:(p['x'],p['y']) for i,p in state()['panes'].items()}==before
        cases.append('native Ctrl+Tab and queued old commit cannot retarget input to another shell')
        key(46,ctrl=True);wait(ready,'original draft cancelled without dispatch')
        proof('printf retained > retained-proof\n','retained-proof',b'retained')
        key(15,ctrl=True);wait(lambda:state()['panes'][state()['active']]['cookie']==third,'next tab')
        key(17,ctrl=True,shift=True);wait(lambda:ready(2) and state()['tabs']==1,'individual tab kernel exit and stream end')
        assert select.select([shells[third]['pin']],[],[],0)[0]
        assert all(not select.select([p['pin']],[],[],0)[0] for c,p in shells.items() if c!=third)
        key(106,alt=True);wait(lambda:state()['active']==1,'right pane survives tab close')
        key(32,ctrl=True,shift=True);wait(lambda:ready(3) and state()['visible']==7,'third pane in reused slot');capture_shells()
        proof('printf three > three-proof\n','three-proof',b'three');snapshot('three panes preserve original sessions')
        divider=max(state()['dividers'],key=lambda d:d['height']);old=state()['panes'][0]['cols']
        x=40+divider['x']+divider['width']/2;y=40+divider['y']+40
        event(f'M {x} {y}');event('B 272 1');event(f'M {x+60} {y}');event('B 272 0')
        wait(lambda:state()['panes'][0]['cols']!=old,'actual split divider resizes original grids')
        key(105,alt=True);key(105,alt=True);wait(lambda:state()['active']==0,'focus original pane after drag')
        proof('stty size > tty-size\n','tty-size',f'{state()["panes"][0]["rows"]} {state()["panes"][0]["cols"]}\n'.encode())
        # The input socket acknowledges injection, not the client's asynchronous
        # resize request. Hold the original button until the compositor's new
        # configure is acknowledged on that same client connection, then move.
        # Releasing before this boundary can invalidate the original grab serial.
        wire_start=len(log('app-wire'));event('M 1038 738');event('B 272 1')
        def resize_started():
            wire=log('app-wire')[wire_start:]
            request=re.search(r'-> xdg_toplevel#\d+\.resize\(wl_seat#\d+, \d+, 10\)',wire)
            configured=re.search(r'(?<!-> )xdg_surface#(\d+)\.configure\((\d+)\)',wire[request.end():]) if request else None
            return bool(configured and re.search(r'-> xdg_surface#'+configured[1]+r'\.ack_configure\('+configured[2]+r'\)',wire))
        wait(resize_started,'original serial-bound native resize has begun')
        event('M 518 358');wait(lambda:state()['fallback']==1,'small-window readable focus fallback')
        event('B 272 0');snapshot('small viewport retains all sessions')
        key(106,alt=True);wait(lambda:state()['active']!=0,'hidden pane remains reachable by keyboard')
        key(50,ctrl=True,shift=True);wait(lambda:'CHROME maximized=1 ' in log() and state()['fallback']==0,'maximize restores full split layout')
        key(66);key(66);key(66);key(64);key(65)
        wait(lambda:'points=32 ' in log() and 'german=1 contrast=1 ' in log(),'German high-contrast large-text workspace')
        key(68,shift=True);wait(lambda:state()['menu']==1,'keyboard opens real terminal menu');snapshot('scaled localized keyboard menu')
        key(1);wait(lambda:state()['menu']==0,'Escape cancels menu without PTY input')
        for count in range(4,9):
            key(20,ctrl=True,shift=True);wait(lambda:ready(count),f'bounded session {count}');capture_shells()
        launches=log().count('SESSION slot=');frame_before=state()['frame'];key(20,ctrl=True,shift=True)
        wait(lambda:state()['sessions']==8 and state()['frame']>frame_before,'eight-session capacity retained after request frame')
        assert log().count('SESSION slot=')==launches
        proof('printf bounded > bounded-proof\n','bounded-proof',b'bounded');snapshot('eight sessions in one original window')
        last=state()['panes'][state()['active']]['cookie'];key(17,ctrl=True,shift=True);wait(lambda:ready(7),'one pane drains before slot reuse')
        assert select.select([shells[last]['pin']],[],[],0)[0]
        key(20,ctrl=True,shift=True);wait(lambda:ready(8),'fresh original process after retired slot');capture_shells()
        active=state()['panes'][state()['active']]['cookie'];assert active!=last
        assert shells[active]['inode']!=shells[last]['inode']
        key(50,ctrl=True,shift=True)
        # Existing text-size policy scales the 480x320 minimum to 960x640.
        wait(lambda:re.search(r'^VIEW .*points=32 width=960 height=640 ',log(),re.M) and
             state()['sessions']==8,'eight sessions retained at minimum size and doubled text')
        snapshot('minimum-size large-text tabs remain reachable')
        key(68,shift=True);wait(lambda:state()['menu']==1,'minimum-size localized command menu')
        snapshot('minimum-size menu preserves complete action labels')
        key(1);wait(lambda:state()['menu']==0,'minimum-size menu cancels')
        wait(lambda:f'DRAWN {state()["frame"]}\n' in log(),'final original window is compositor-observed')
        before=len(re.findall(r'^NEXAUI_FRAME ',log(),re.M));s=Path(f'/proc/{app.pid}/stat').read_text().rsplit(')',1)[1].split();ticks=int(s[11])+int(s[12]);time.sleep(2)
        s=Path(f'/proc/{app.pid}/stat').read_text().rsplit(')',1)[1].split();idle={'frames':len(re.findall(r'^NEXAUI_FRAME ',log(),re.M))-before,'cpu_ticks':int(s[11])+int(s[12])-ticks,'seconds':2}
        assert idle['frames']==0 and idle['cpu_ticks']<=1,idle
        key(16,ctrl=True,shift=True);assert app.wait(timeout=10)==0;children.remove(app)
        assert all(select.select([p['pin']],[],[],0)[0] for p in shells.values())
        server.terminate();assert server.wait(timeout=10)==0;children.remove(server)
        assert all(select.select([p],[],[],0)[0] for p in pins)
        for p in root.glob('*.log'):assert not re.search(r'AddressSanitizer|LeakSanitizer|runtime error:',p.read_text(errors='replace')),p
        result={'result':'passed','cases':cases,'images':images,'idle':idle,'shells':{str(k):{n:v for n,v in p.items() if n!='pin'} for k,p in shells.items()},
                'scope':'private native window/keyboard/pointer and original PTYs, not installed admission, assistive/physical or persistent-session acceptance'}
        (root/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({'result':'passed','artifacts':str(root)}))
    finally:
        for p in reversed(children):
            if p.poll() is None:p.terminate()
            if p.poll() is None:p.wait(timeout=10)
        for fd in pins:os.close(fd)
        for f in streams:f.close()
        left.close();right.close();print('Terminal multiplex evidence:',root)

if __name__=='__main__':main()
