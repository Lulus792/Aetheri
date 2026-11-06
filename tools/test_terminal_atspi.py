"""Actual SDL-host Terminal text through a private registry and independent C peer."""
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
from xml.sax.saxutils import escape

ROOT=Path(__file__).resolve().parents[1]
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()

def main():
    assert len(sys.argv) in (7,8)
    server_bin,terminal,shell,font,build,fixture=map(lambda x:Path(x).resolve(),sys.argv[1:7])
    query_concurrent=len(sys.argv)==8 and sys.argv[7]=='--query-concurrent'
    query_stress=query_concurrent or (len(sys.argv)==8 and sys.argv[7]=='--query-stress')
    starter=Path(sys.argv[7]).resolve() if len(sys.argv)==8 and not query_stress else None
    assert os.getuid()==1000
    assets=json.loads((fixture/'inputs.json').read_text())
    for n,h in assets['files'].items():assert digest(fixture/n.lstrip('/'))==h,n
    pin=json.loads((ROOT/'spec/ui-inputs.json').read_text())['terminal_font']
    for f in (pin,pin['bold']):
        assert digest(Path(f['path']))==f['sha256'] and digest(Path(f['license_path']))==f['license_sha256']
    root=Path(tempfile.mkdtemp(prefix='terminal-query-stress-' if query_stress else 'terminal-tabs-atspi-' if starter else 'terminal-atspi-',dir=build))
    for n in ('runtime','cache','captures'):(root/n).mkdir(mode=0o700)
    client=root/'client';sdk=Path('/home/aetheri/deps/nexaui-settings-release-02/dbus-sdk')
    command=['cc','-std=c17','-Wall','-Wextra','-Wpedantic','-Werror','-O2',str(ROOT/'tests/terminal_view_atspi.c'),'-o',str(client)]
    command+=['-I'+str(fixture/p) for p in ('usr/include/at-spi-2.0','usr/include/glib-2.0','usr/lib64/glib-2.0/include')]
    command+=['-I'+str(sdk/p) for p in ('include/dbus-1.0','lib64/dbus-1.0/include')]
    command+=[str(fixture/'usr/lib64/libatspi.so.0.0.1'),'-l:libgobject-2.0.so.0','-l:libglib-2.0.so.0','-l:libdbus-1.so.3','-Wl,-rpath,'+str(fixture/'usr/lib64'),'-Wl,-rpath-link,'+str(fixture/'usr/lib64')]
    p=subprocess.run(command,capture_output=True,text=True);(root/'client-build.log').write_text(p.stdout+p.stderr);assert p.returncode==0,p.stderr
    address='unix:path='+str(root/'runtime/bus')
    configuration=root/'bus.conf'
    configuration.write_text('<busconfig><type>session</type><listen>'+escape(address)+'</listen><auth>EXTERNAL</auth><policy context="default"><allow user="1000"/><allow own="*"/><allow send_destination="*"/><allow receive_sender="*"/></policy></busconfig>\n')
    env={**os.environ,'XDG_RUNTIME_DIR':str(root/'runtime'),'WAYLAND_DISPLAY':'terminal-access-private',
        'DBUS_SESSION_BUS_ADDRESS':address,'AT_SPI_BUS_ADDRESS':address,'GSETTINGS_BACKEND':'memory',
        'LD_LIBRARY_PATH':str(fixture/'usr/lib64'),'LIBGL_ALWAYS_SOFTWARE':'1','GALLIUM_DRIVER':'llvmpipe','LP_NUM_THREADS':'2',
        'ASAN_OPTIONS':'detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS':'halt_on_error=1:print_stacktrace=1'}
    candidate=None
    if os.environ.get('AETHERI_TEST_SDL_CANDIDATE'):
        assert query_stress
        candidate=Path(os.environ['AETHERI_TEST_SDL_CANDIDATE']).resolve()
        assert candidate.read_bytes()[:4]==b'\x7fELF' and digest(candidate)==os.environ['AETHERI_TEST_SDL_SHA256']
        env['LD_LIBRARY_PATH']=str(candidate.parent)+':'+env['LD_LIBRARY_PATH']
    for n in ('DISPLAY','WAYLAND_SOCKET','NO_AT_BRIDGE','DBUS_STARTER_ADDRESS','DBUS_STARTER_BUS_TYPE'):env.pop(n,None)
    children=[];streams=[];pins=[];images=[];peers=[];actors=[]
    left,right=socket.socketpair(socket.AF_UNIX,socket.SOCK_SEQPACKET);left.settimeout(4)
    def log(n='app'):return (root/(n+'.log')).read_text(errors='replace')
    def start(binary,name,*args,pass_fds=(),extra_env=None):
        stream=(root/(name+'.log')).open('xb');streams.append(stream)
        p=subprocess.Popen([str(binary),*map(str,args)],cwd=root,env=extra_env or env,stdin=subprocess.DEVNULL,stdout=stream,stderr=subprocess.STDOUT,pass_fds=pass_fds)
        children.append(p);pins.append(os.pidfd_open(p.pid));return p
    def wait(check,label):
        end=time.monotonic()+10
        while not check():
            assert time.monotonic()<end,(label,root,log()[-2000:] if (root/'app.log').exists() else '')
            assert all(p.poll() is None for p in children),(label,[(p.pid,p.returncode) for p in children],root)
            time.sleep(.01)
    def ready():
        p=subprocess.run(['busctl','--address='+address,'--timeout=1','call','org.freedesktop.DBus','/org/freedesktop/DBus','org.freedesktop.DBus','NameHasOwner','s','org.a11y.atspi.Registry'],env=env,capture_output=True,text=True,timeout=2)
        return p.returncode==0 and p.stdout.strip()=='b true'
    def event(s):left.sendall(s.encode());assert left.recv(32)==b'OK',s
    def key(code,ctrl=False,shift=False):
        if ctrl:event('K 29 1')
        if shift:event('K 42 1')
        event(f'K {code} 1');event(f'K {code} 0')
        if shift:event('K 42 0')
        if ctrl:event('K 29 0')
    codes=dict(zip('abcdefghijklmnopqrstuvwxyz',(30,48,46,32,18,33,34,35,23,36,37,38,50,49,24,25,16,19,31,20,22,47,17,45,21,44)))
    codes.update(dict(zip('1234567890',range(2,12))));codes.update({' ':57,"'":40,'\\':43,'-':12,'\n':28})
    def typing(s):
        for c in s:key(52,shift=True) if c=='>' else key(codes[c])
    def reading():
        rows=re.findall(r'^READ_FOCUS id=(\d+)$',log(),re.M);return int(rows[-1]) if rows else -1
    def accepted():
        rows=re.findall(r'^INPUT (\d+)$',log(),re.M);return int(rows[-1]) if rows else 0
    def peer(pid,mode,value,name,expected=0):
        p=start(client,name,pid,mode,value);assert p.wait(timeout=10)==expected,(name,root,log(name));children.remove(p)
        peers.append({'name':name,'exit':p.returncode})
    def retire(p,status):
        p.terminate();assert p.wait(timeout=8)==status,(p.pid,p.returncode,root);children.remove(p)
    def registry(name):
        b=start(fixture/'usr/bin/dbus-daemon',name+'-bus','--nofork','--config-file='+str(configuration),'--print-address=1')
        wait(lambda:(root/'runtime/bus').is_socket(),'bus socket')
        r=start(fixture/'usr/libexec/at-spi2-registryd',name+'-registry');wait(ready,'registry');return b,r
    def capture(label):
        n=len(images)//3+1;server.send_signal(signal.SIGUSR1)
        wait(lambda:all(f'CAPTURE {n} {i}\n' in log('server') for i in range(3)),label)
        for i in range(3):
            p=root/'captures'/f'capture-{n}-output-{i}.ppm';raw=p.read_bytes();magic,size,peak,pixels=raw.split(b'\n',3);w,h=map(int,size.split())
            assert magic==b'P6' and peak==b'255' and len(pixels)==w*h*3
            if i==0:assert len(set(pixels))>20
            packed=p.with_suffix('.ppm.gz');packed.write_bytes(gzip.compress(raw,mtime=0));p.unlink()
            images.append({'file':packed.name,'sha256':hashlib.sha256(raw).hexdigest(),'case':label,'output':i})
    args=('--shell',shell,'--font',font,'--cache',root/'cache','--diagnostics')
    if starter:args+=('--pty-starter',starter)
    try:
        bus,reg=registry('first')
        server=start(server_bin,'server','--headless',env['WAYLAND_DISPLAY'],root/'captures',right.fileno(),pass_fds=(right.fileno(),));right.close()
        wait(lambda:'READY ' in log('server'),'compositor')
        monitor=None
        if query_concurrent:
            monitor=start(Path('/usr/bin/busctl'),'bus-wire','--address='+address,'--json=short','monitor')
            wait(lambda:ready() and '"member":"NameHasOwner"' in log('bus-wire'),'private bus monitor observes actual query')
        app=start(terminal,'app',*args)
        wait(lambda:'ACCESSIBILITY available=1\n' in log() and 'FOCUS 1\n' in log() and 'NEXAUI_FRAME ' in log(),'native frame/focus/transport')
        if starter:
            sessions={}
            def workspace():
                blocks=re.findall(r'^WORKSPACE (.+)\n((?:(?:PANE|DIVIDER) .*\n)*)WORKSPACE_END frame=(\d+)\n',log(),re.M)
                if not blocks:return {}
                header,body,end=blocks[-1]
                parse=lambda row:{k:int(v) for k,v in re.findall(r'(\w+)=(\d+)(?= |$)',row)}
                value=parse(header);assert value['frame']==int(end)
                value['panes']={row['slot']:row for row in (parse(line) for line in body.splitlines() if line.startswith('PANE '))}
                return value
            def shells_ready(count):
                value=workspace()
                return bool(value) and value['sessions']==count and value['panes'][value['active']]['x']==9
            def retain():
                for pane in workspace()['panes'].values():
                    cookie=pane['cookie']
                    if cookie in sessions:continue
                    fd=os.pidfd_open(pane['pid']);pins.append(fd)
                    actual=os.stat(f'/proc/{pane["pid"]}/exe');expected=os.stat(shell)
                    parent=Path(f'/proc/{pane["pid"]}/stat').read_text().rsplit(')',1)[1].split()[1]
                    assert int(parent)==app.pid and (actual.st_dev,actual.st_ino)==(expected.st_dev,expected.st_ino)
                    sessions[cookie]={'pid':pane['pid'],'pin':fd,'inode':os.fstat(fd).st_ino}
            wait(lambda:shells_ready(1),'first original shell');retain()
            for count in (2,3):
                key(20,ctrl=True,shift=True);wait(lambda:shells_ready(count),'new original tab');retain()
            peer(app.pid,'tabs','3:2:0','select-first-tab')
            wait(lambda:workspace()['active']==0,'native selection activates original first session')
            typing('printf first > native-first\n')
            wait(lambda:(root/'native-first').exists() and (root/'native-first').read_bytes()==b'first' and shells_ready(3),'first original shell command')
            peer(app.pid,'tabs','3:0:2','select-last-tab')
            wait(lambda:workspace()['active']==2,'native selection activates original third session')
            typing('printf third > native-third\n')
            wait(lambda:(root/'native-third').exists() and (root/'native-third').read_bytes()==b'third' and shells_ready(3),'third original shell command')
            key(68,shift=True);wait(lambda:workspace()['menu']==1,'modal command menu')
            peer(app.pid,'tabs-disabled','3:2:-1','blocked-group-selection')
            assert workspace()['active']==2
            key(1);wait(lambda:workspace()['menu']==0,'menu cancellation')
            held,channel=socket.socketpair(socket.AF_UNIX,socket.SOCK_SEQPACKET);held.settimeout(8)
            stale=start(client,'retired-tab',app.pid,'retire-tab','3:2:-1',channel.fileno(),pass_fds=(channel.fileno(),));channel.close()
            assert held.recv(16)==b'HELD'
            original=workspace()['panes'][2]['cookie']
            key(17,ctrl=True,shift=True);wait(lambda:shells_ready(2) and workspace()['tabs']==2,'original selected tab exits')
            assert held.recv(16)==b'RETIRED'
            assert select.select([sessions[original]['pin']],[],[],0)[0]
            key(20,ctrl=True,shift=True);wait(lambda:shells_ready(3),'fresh tab after original retirement');retain()
            replacement=workspace()['panes'][2]['cookie']
            assert replacement!=original and sessions[replacement]['inode']!=sessions[original]['inode']
            held.sendall(b'REUSED')
            assert stale.wait(timeout=8)==0,log('retired-tab');children.remove(stale);held.close()
            peers.append({'name':'retired-tab','exit':0})
            assert workspace()['active']==2 and workspace()['panes'][2]['cookie']==replacement
            peer(app.pid,'tabs','3:2:1','select-retained-neighbor')
            wait(lambda:workspace()['active']==1,'retained neighbor session')
            key(15,ctrl=True);wait(lambda:workspace()['active']==2,'keyboard next tab')
            peer(app.pid,'tabs','3:2:-1','keyboard-selection-observation')
            wait(lambda:f'DRAWN {workspace()["frame"]}\n' in log(),'matching native tab frame')
            capture('native selection, modal refusal and fresh session after retired tab')
            key(16,ctrl=True,shift=True);assert app.wait(timeout=8)==0;children.remove(app)
            assert all(select.select([p['pin']],[],[],0)[0] for p in sessions.values())
            retire(server,0);retire(reg,-signal.SIGTERM);retire(bus,0)
            assert all(select.select([p],[],[],0)[0] for p in pins)
            for path in root.glob('*.log'):assert not re.search(r'AddressSanitizer|LeakSanitizer|runtime error:',path.read_text(errors='replace')),path
            result={'result':'passed','images':images,'peers':peers,'client_sha256':digest(client),
                    'sessions':{str(k):{n:v for n,v in value.items() if n!='pin'} for k,value in sessions.items()},
                    'scope':'independent native required tab selection on original shells, modal refusal and retired objects; not human screenreader acceptance'}
            (root/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({'result':'passed','artifacts':str(root)}));return
        def position():
            rows=re.findall(r'^VIEW .* x=(\d+) y=(\d+) history=(\d+)',log(),re.M)
            return tuple(map(int,rows[-1])) if rows else (-1,-1,-1)
        wait(lambda:position()[0]==9,'original prompt after complete welcome')
        peer(app.pid,'startup','','startup-information')
        baseline=position()
        typing("printf 'row-\\303\\251\\344\\270\\255\\n'\n")
        def output_ready():
            x,y,history=position();return x==9 and (history>baseline[2] or y>baseline[1])
        wait(output_ready,'complete native output and returned original prompt')
        peer(app.pid,'focused','row-é中','read')
        before=accepted();peer(app.pid,'focus','row-é中','native-read-focus')
        wait(lambda:reading()>0,'actual native reading focus');assert accepted()==before
        maps=Path(f'/proc/{app.pid}/maps').read_text();assert 'libnexaui.so.0.1.18' in maps and 'libSDL3.so' in maps
        if candidate:assert str(candidate) in maps and digest(candidate)==os.environ['AETHERI_TEST_SDL_SHA256']
        (root/'app-maps.txt').write_text(maps)
        shell_pid=int(re.search(r'^READY .*shell=(\d+)',log(),re.M)[1]);shell_pin=os.pidfd_open(shell_pid);pins.append(shell_pin)
        assert Path(f'/proc/{shell_pid}/exe').resolve()==shell
        actors.append({'pid':app.pid,'exe':str(terminal),'sha256':digest(Path(f'/proc/{app.pid}/exe')),'shell':shell_pid})
        if candidate:actors[-1]['sdl_candidate']={'path':str(candidate),'sha256':digest(candidate)}
        capture('independent Unicode text queries on actual native terminal')
        if query_stress:
            # A bounded transition workload, not retries of a failed query. Any
            # failed native operation stops the original task and is retained.
            for cycle in range(16):
                loss=log().count('FOCUS 0\n');gain=log().count('FOCUS 1\n')
                event('D');wait(lambda:log().count('FOCUS 0\n')==loss+1,'query-workload original focus loss')
                peer(app.pid,'unfocused','row-é中',f'cycle-{cycle}-unfocused')
                event('A');wait(lambda:log().count('FOCUS 1\n')==gain+1,'query-workload original focus return')
                if query_concurrent:
                    batch=[(f'cycle-{cycle}-stream-{i}',start(client,f'cycle-{cycle}-stream-{i}',app.pid,'active-wire-stream','row-é中')) for i in range(4)]
                    for name,p in batch:
                        status=p.wait(timeout=10);children.remove(p);peers.append({'name':name,'exit':status})
                        assert status==0,(name,root,log(name))
                else:peer(app.pid,'active-wire-stream','row-é中',f'cycle-{cycle}-stream')
            capture('same original process after sixteen native focus/query transitions')
            key(16,ctrl=True,shift=True);assert app.wait(timeout=8)==0;children.remove(app)
            assert select.select([shell_pin],[],[],0)[0]
            if monitor:
                monitor.terminate();status=monitor.wait(timeout=8);children.remove(monitor);assert status in (0,-signal.SIGTERM)
            retire(server,0);retire(reg,-signal.SIGTERM);retire(bus,0)
            assert all(select.select([p],[],[],0)[0] for p in pins)
            for path in root.glob('*.log'):assert not re.search(r'AddressSanitizer|LeakSanitizer|runtime error:',path.read_text(errors='replace')),path
            result={'result':'passed','images':images,'peers':peers,'actors':actors,'cycles':16,'concurrent_clients':4 if query_concurrent else 1,'client_sha256':digest(client),
                    'scope':'bounded native focus/128-state-query/read-only-text transitions, not historical-failure root cause or human acceptance'}
            (root/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({'result':'passed','artifacts':str(root)}));return
        event('D');wait(lambda:'FOCUS 0\n' in log(),'native focus loss')
        peer(app.pid,'unfocused','row-é中','unfocused')
        peer(app.pid,'focused-now','row-é中','wrong-focus',1)
        assert 'native frame wanted=1 defunct=0' in log('wrong-focus'),log('wrong-focus')
        peer(app.pid,'reject-focus','row-é中','unfocused-read-request')
        event('A');wait(lambda:log().count('FOCUS 1\n')==2,'native focus return');peer(app.pid,'focused','row-é中','refocused')
        peer(app.pid,'active-wire-stream','row-é中','bounded-state-stream')
        # Plain native input keeps its original shell owner. Reading focus can
        # be cleared/entered without placing the shortcut into the PTY queue.
        before=accepted();key(19,ctrl=True,shift=True);wait(lambda:reading()==0,'keyboard clears read focus')
        peer(app.pid,'no-row-focus','row-é中','cleared-read-focus')
        key(19,ctrl=True,shift=True);wait(lambda:reading()>0,'keyboard focuses actual prompt row')
        assert accepted()==before
        peer(app.pid,'focused-row','aetheri>  ','keyboard-read-focus')
        held,channel=socket.socketpair(socket.AF_UNIX,socket.SOCK_SEQPACKET);held.settimeout(8)
        stale=start(client,'retired-read-row',app.pid,'retire-row','aetheri>  ',channel.fileno(),pass_fds=(channel.fileno(),));channel.close()
        assert held.recv(16)==b'HELD'
        typing('printf ready > focus-proof\n')
        assert stale.wait(timeout=8)==0,log('retired-read-row');children.remove(stale);held.close()
        peers.append({'name':'retired-read-row','exit':0})
        wait(lambda:(root/'focus-proof').exists() and (root/'focus-proof').read_bytes()==b'ready' and reading()==0,'original shell writes proof after stale focus retires')
        def zoomed():
            rows=re.findall(r'^CHROME maximized=(\d+) header=',log(),re.M);return int(rows[-1]) if rows else -1
        peer(app.pid,'chrome','Zoom window','native-zoom')
        wait(lambda:zoomed()==1,'native action produces observed maximize')
        peer(app.pid,'chrome','Restore window','native-restore')
        wait(lambda:zoomed()==0,'native action produces observed restore')
        peer(app.pid,'disabled','Minimize unavailable','disabled-minimize')
        # The same original app must survive loss/recreation of its private bus.
        retire(reg,-signal.SIGTERM);retire(bus,0)
        wait(lambda:not (root/'runtime/bus').exists(),'old bus retired')
        bus,reg=registry('second')
        peer(app.pid,'focused','row-é中','reconnected')
        key(64);wait(lambda:'german=1 ' in log(),'localized controls')
        capture('same original process after private bus replacement')
        before=len(re.findall(r'^NEXAUI_FRAME ',log(),re.M));s=Path(f'/proc/{app.pid}/stat').read_text().rsplit(')',1)[1].split();ticks=int(s[11])+int(s[12]);time.sleep(2)
        s=Path(f'/proc/{app.pid}/stat').read_text().rsplit(')',1)[1].split();idle={'seconds':2,'frames':len(re.findall(r'^NEXAUI_FRAME ',log(),re.M))-before,'cpu_ticks':int(s[11])+int(s[12])-ticks}
        assert idle['frames']==0 and idle['cpu_ticks']<=1,idle
        peer(app.pid,'close','Fenster schließen','close')
        assert app.wait(timeout=8)==0;children.remove(app)
        assert select.select([shell_pin],[],[],0)[0],('original shell not ended',root)
        negative=start(terminal,'negative',*args,extra_env={**env,'NO_AT_BRIDGE':'1'})
        wait(lambda:'NEXAUI_FRAME ' in log('negative'),'disabled native frame')
        peer(negative.pid,'focused','row-é中','negative-peer',1)
        assert 'target app was not registered' in log('negative-peer')
        retire(negative,0);retire(server,0);retire(reg,-signal.SIGTERM);retire(bus,0)
        assert all(select.select([p],[],[],0)[0] for p in pins)
        for p in root.glob('*.log'):assert not re.search(r'AddressSanitizer|LeakSanitizer|runtime error:',p.read_text(errors='replace')),p
        result={'result':'passed','images':images,'peers':peers,'idle':idle,'actors':actors,'client_sha256':digest(client),'scope':'private native read-only text/close/focus/reconnect, not human screenreader or complete accessible Terminal'}
        (root/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({'result':'passed','artifacts':str(root)}))
    finally:
        for p in reversed(children):
            if p.poll() is None:p.terminate()
            if p.poll() is None:p.wait(timeout=10)
        for p in pins:os.close(p)
        for s in streams:s.close()
        left.close();right.close()
        print('Terminal AT-SPI evidence:',root)

if __name__=='__main__':main()
