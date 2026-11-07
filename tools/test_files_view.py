"""Native opt-in NexaUI Files, real controller work and original editor child.

Private unprivileged seat only; not installed admission or physical acceptance.
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
from xml.sax.saxutils import escape

ROOT=Path(__file__).resolve().parents[1]

def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    assert len(sys.argv) in (8,10)
    server_bin,app_bin,font,terminal,editor,mono,build=map(lambda x:Path(x).resolve(),sys.argv[1:8])
    fixture=Path(sys.argv[9]).resolve() if len(sys.argv)==10 and sys.argv[8]=='--atspi' else None
    assert len(sys.argv)==8 or fixture is not None
    assert os.getuid()!=0
    assert digest(font)==json.loads((ROOT/"spec/ui-inputs.json").read_text())["font"]["sha256"]
    root=Path(tempfile.mkdtemp(prefix="files-view-native-",dir=build));runtime=root/"runtime";runtime.mkdir(mode=0o700)
    folder=root/"documents";folder.mkdir(mode=0o700);(folder/"existing").mkdir(mode=0o700);(folder/"seed.txt").write_bytes(b"seed")
    cache=root/"cache";cache.mkdir(mode=0o700);captures=root/"captures";captures.mkdir(mode=0o700)
    env={**os.environ,"XDG_RUNTIME_DIR":str(runtime),"WAYLAND_DISPLAY":"files-view-private","NEXAUI_PLATFORM":"wayland",
         "DBUS_SESSION_BUS_ADDRESS":"unix:path="+str(root/"absent-bus"),"LIBGL_ALWAYS_SOFTWARE":"1","GALLIUM_DRIVER":"llvmpipe","LP_NUM_THREADS":"2",
         "ASAN_OPTIONS":"detect_leaks=1:halt_on_error=1","UBSAN_OPTIONS":"halt_on_error=1:print_stacktrace=1"}
    for key in ("DISPLAY","WAYLAND_SOCKET","AT_SPI_BUS_ADDRESS"):env.pop(key,None)
    client=root/'atspi-client'
    if fixture:
        assets=json.loads((fixture/'inputs.json').read_text())
        for name,value in assets['files'].items():assert digest(fixture/name.lstrip('/'))==value,name
        sdk=Path(json.loads((ROOT/'spec/nexaui-inputs.json').read_text())['development_builds']['release'])/'dbus-sdk'
        command=['cc','-std=c17','-Wall','-Wextra','-Wpedantic','-Werror','-O2',str(ROOT/'tests/files_view_atspi.c'),'-o',str(client)]
        command += ['-I'+str(fixture/p) for p in ('usr/include/at-spi-2.0','usr/include/glib-2.0','usr/lib64/glib-2.0/include')]
        command += ['-I'+str(sdk/p) for p in ('include/dbus-1.0','lib64/dbus-1.0/include')]
        command += [str(fixture/'usr/lib64/libatspi.so.0.0.1'),'-l:libgobject-2.0.so.0','-l:libglib-2.0.so.0','-Wl,-rpath,'+str(fixture/'usr/lib64'),'-Wl,-rpath-link,'+str(fixture/'usr/lib64')]
        compiled=subprocess.run(command,capture_output=True,text=True);(root/'client-build.log').write_text(compiled.stdout+compiled.stderr);assert compiled.returncode==0,compiled.stderr
        address='unix:path='+str(runtime/'bus');configuration=root/'bus.conf'
        configuration.write_text('<busconfig><type>session</type><listen>'+escape(address)+'</listen><auth>EXTERNAL</auth><policy context="default"><allow user="'+str(os.getuid())+'"/><allow own="*"/><allow send_destination="*"/><allow receive_sender="*"/></policy></busconfig>\n')
        env.update(DBUS_SESSION_BUS_ADDRESS=address,AT_SPI_BUS_ADDRESS=address,LD_LIBRARY_PATH=str(fixture/'usr/lib64'),GSETTINGS_BACKEND='memory')
        for name in ('NO_AT_BRIDGE','DBUS_STARTER_ADDRESS','DBUS_STARTER_BUS_TYPE'):env.pop(name,None)
    left,right=socket.socketpair(socket.AF_UNIX,socket.SOCK_SEQPACKET);left.settimeout(4)
    children=[];streams=[];pins=[];images=[]
    def start(binary,name,*args,pass_fds=(),extra_env=None):
        stream=(root/(name+".log")).open("xb");streams.append(stream)
        p=subprocess.Popen([str(binary),*map(str,args)],env=extra_env or {**env,**({"WAYLAND_DEBUG":"server"} if name=="server" else {})},stdin=subprocess.DEVNULL,stdout=stream,stderr=subprocess.STDOUT,pass_fds=pass_fds)
        children.append(p);return p
    def log(name="app"):return (root/(name+".log")).read_text(errors="replace")
    def wait(test,label):
        deadline=time.monotonic()+12
        while not test():
            assert all(p.poll() is None for p in children),(label,log()[-3500:])
            assert time.monotonic()<deadline,(label,log()[-3500:])
            time.sleep(.01)
    def event(text):left.sendall(text.encode());assert left.recv(32)==b"OK"
    def key(code,ctrl=False,shift=False,alt=False):
        modifiers=([29] if ctrl else [])+([42] if shift else [])+([56] if alt else [])
        for c in modifiers:event(f"K {c} 1")
        event(f"K {code} 1");event(f"K {code} 0")
        for c in reversed(modifiers):event(f"K {c} 0")
    codes=dict(zip("abcdefghijklmnopqrstuvwxyz",(30,48,46,32,18,33,34,35,23,36,37,38,50,49,24,25,16,19,31,20,22,47,17,45,21,44)));codes["."]=52
    def typing(text):
        for c in text:key(codes[c])
    def peer(mode):
        p=start(client,'peer-'+mode,app.pid,mode);assert p.wait(timeout=12)==0,log('peer-'+mode);children.remove(p)
    def view():
        found=re.findall(r"^VIEW (.+)$",log(),re.M)
        return dict(re.findall(r"(\w+)=([^ ]+)",found[-1])) if found else {}
    def state(**wanted):return all(view().get(k)==str(v) for k,v in wanted.items())
    def focused(control):
        latest=log().rsplit('VIEW ',1)[-1];match=re.search(r'^CONTROL '+str(control)+r' .* (\d+)$',latest,re.M)
        return match is not None and int(match[1])&512
    def field_fence(control):
        # Socket acknowledgement is from the compositor, not the app. The native
        # focus round trip must follow all earlier input and preserve selection.
        assert control==15
        key(15);wait(lambda:focused(13),'field Tab reached Cancel')
        key(15,shift=True);wait(lambda:focused(control),'field focus returned after prior input')
    def click(control):
        rows=re.findall(r"^CONTROL "+str(control)+r" (\d+) ([\d.]+) ([\d.]+) ([\d.]+) ([\d.]+) (\d+)$",log(),re.M);assert rows
        _,x,y,w,h,_=rows[-1];event(f"M {40+float(x)+float(w)/2} {40+float(y)+float(h)/2}");event("B 272 1");event("B 272 0")
    def capture(label,checks=()):
        time.sleep(.08);sequence=len(images)//3+1;server.send_signal(signal.SIGUSR1)
        wait(lambda:all(f"CAPTURE {sequence} {i}\n" in log("server") for i in range(3)),label)
        for i in range(3):
            p=captures/f"capture-{sequence}-output-{i}.ppm";raw=p.read_bytes();magic,size,peak,pixels=raw.split(b"\n",3);w,h=map(int,size.split())
            assert magic==b"P6" and peak==b"255" and len(pixels)==w*h*3
            if i==0:
                assert len(set(pixels))>20
                for x,y,expected in checks:assert tuple(pixels[(y*w+x)*3:(y*w+x)*3+3])==expected,(label,x,y)
            packed=p.with_suffix(".ppm.gz");packed.write_bytes(gzip.compress(raw,mtime=0));assert gzip.decompress(packed.read_bytes())==raw;p.unlink()
            images.append({"case":label,"output":i,"file":packed.name,"sha256":hashlib.sha256(raw).hexdigest()})
    def chrome_cases():
        # Actual app geometry and compositor RPCs, not request success alone.
        for w,h,point in ((1920,1080,'M 220 56'),(1000,680,'M 180 16')):
            event(point)
            for _ in range(2):event('B 272 1');event('B 272 0')
            wait(lambda:state(width=w,height=h),'title double-click '+str((w,h)))
            capture('title double-click '+str((w,h)))
        def edge_drag(x,y,w,h,left,right,top,bottom,dx,dy):
            px=x+2 if left else x+w-2 if right else x+w/2;py=y+2 if top else y+h-2 if bottom else y+h/2
            before=log('server').count('.resize(');event(f'M {px} {py}');event('B 272 1')
            wait(lambda:log('server').count('.resize(')>before,'native resize edge admitted')
            event(f'M {px+dx} {py+dy}')
            nw=w+(-dx if left else dx if right else 0);nh=h+(-dy if top else dy if bottom else 0)
            wait(lambda:state(width=nw,height=nh),'native edge geometry');event('B 272 0')
            return x+(dx if left else 0),y+(dy if top else 0),nw,nh
        for left,right,top,bottom in ((1,0,1,0),(0,0,1,0),(0,1,1,0),(0,1,0,0),(0,1,0,1),(0,0,0,1),(1,0,0,1),(1,0,0,0)):
            dx=-20 if left else 20 if right else 0;dy=-15 if top else 15 if bottom else 0
            geometry=edge_drag(40,40,1000,680,left,right,top,bottom,dx,dy)
            assert edge_drag(*geometry,left,right,top,bottom,-dx,-dy)==(40,40,1000,680)
        capture('all eight native resize edges restored',((50,80,(32,35,44)),(1100,740,(16,20,28))))
        key(38,ctrl=True);typing('seed');wait(lambda:state(count=1),'filter before pending-title cancellation')
        moves=log('server').count('.move(');titles=log().count('CHROME_TITLE down\n');cancelled=log().count('CHROME_TITLE cancel\n')
        event('M 220 56');event('B 272 1');wait(lambda:log().count('CHROME_TITLE down\n')>titles,'original title press')
        event('K 1 1');wait(lambda:log().count('CHROME_TITLE cancel\n')>cancelled,'pending caption cancelled')
        event('K 1 1');event('K 1 0');event('M 308 116');event('B 272 0')
        capture('pending title Escape consumes repeat and preserves filter')
        assert state(count=1,width=1000,height=680) and log('server').count('.move(')==moves
        key(38,ctrl=True);wait(lambda:state(count=2),'clear original filter')
        titles=log().count('CHROME_TITLE down\n');cancelled=log('server').count('GRAB_CANCEL restored=1\n')
        event('M 220 56');event('B 272 1');wait(lambda:log().count('CHROME_TITLE down\n')>titles,'title before native move')
        event('M 228 56');wait(lambda:log('server').count('.move(')>moves,'native move admitted')
        event('M 308 116');capture('native move before cancellation',((50,80,(16,20,28)),(125,185,(32,35,44))))
        event('K 1 1');wait(lambda:log('server').count('GRAB_CANCEL restored=1\n')>cancelled,'compositor move cancelled')
        event('K 1 1');event('K 1 0');event('M 388 176');event('B 272 0')
        capture('native move Escape restores and fences held gesture',((50,80,(32,35,44)),(1100,740,(16,20,28))))
        before=log('server').count('.resize(');cancelled=log('server').count('GRAB_CANCEL restored=1\n')
        event('M 1038 718');event('B 272 1');wait(lambda:log('server').count('.resize(')>before,'resize before cancellation')
        event('M 1138 768');wait(lambda:state(width=1100,height=730),'larger native window')
        event('K 1 1');wait(lambda:log('server').count('GRAB_CANCEL restored=1\n')>cancelled,'compositor resize cancelled')
        event('K 1 1');event('K 1 0');event('M 1238 818');event('B 272 0');wait(lambda:state(width=1000,height=680),'original geometry restored')
        capture('native resize Escape restores and fences held gesture',((50,80,(32,35,44)),(1100,740,(16,20,28))))
        titles=log().count('CHROME_TITLE down\n');moves=log('server').count('.move(')
        event('M 220 56');event('B 272 1');wait(lambda:log().count('CHROME_TITLE down\n')>titles,'caption before dialog')
        key(49,ctrl=True);wait(lambda:state(dialog=1),'dialog opened during pending caption')
        event('M 308 116');event('B 272 0');capture('new dialog retires pending caption')
        assert log('server').count('.move(')==moves and state(dialog=1,count=2)
        key(1);wait(lambda:state(dialog=0),'cancel original empty dialog')
        titles=log().count('CHROME_TITLE down\n');event('M 220 56');event('B 272 1')
        wait(lambda:log().count('CHROME_TITLE down\n')>titles,'caption owns eventual release')
        event('M 61 56');wait(lambda:log('server').count('.move(')>moves,'caption crossing Close admitted');event('B 272 0')
        assert app.poll() is None and state(width=1000,height=680)
        before=log('server').count('.move(');event('M 334 94');event('B 272 1');event('M 220 56');event('B 272 0')
        capture('body press is not adopted as caption and Close is not triggered')
        assert app.poll() is None and log('server').count('.move(')==before
        titles=log().count('CHROME_TITLE down\n');event('M 220 56');event('B 272 1')
        wait(lambda:log().count('CHROME_TITLE down\n')>titles,'caption before pointer removal')
        frames=log().count('VIEW ');event('D');wait(lambda:log().count('VIEW ')>frames,'native focus/capture loss')
        enters=len(re.findall(r'-> wl_pointer#\d+\.enter\(',log('server')));event('A');event('M 220 56')
        wait(lambda:len(re.findall(r'-> wl_pointer#\d+\.enter\(',log('server')))>enters,'replacement native pointer entered')
        released=log().count('CHROME_TITLE release\n');event('B 272 1');event('B 272 0')
        wait(lambda:log().count('CHROME_TITLE release\n')>released,'fresh caption click after pointer loss')
        assert state(width=1000,height=680,count=2) and sorted(p.name for p in folder.iterdir())==['existing','seed.txt']
    try:
        if fixture:
            bus=start(fixture/'usr/bin/dbus-daemon','bus','--nofork','--config-file='+str(configuration))
            wait(lambda:(runtime/'bus').is_socket(),'private bus')
            registry=start(fixture/'usr/libexec/at-spi2-registryd','registry')
            def registry_ready():
                p=subprocess.run(['busctl','--address='+address,'--timeout=1','call','org.freedesktop.DBus','/org/freedesktop/DBus','org.freedesktop.DBus','NameHasOwner','s','org.a11y.atspi.Registry'],env=env,capture_output=True,text=True,timeout=2)
                return p.returncode==0 and p.stdout.strip()=='b true'
            wait(registry_ready,'private registry')
        server=start(server_bin,"server","--headless",env["WAYLAND_DISPLAY"],captures,right.fileno(),pass_fds=(right.fileno(),));right.close()
        wait(lambda:"READY " in log("server"),"server")
        app=start(app_bin,"app","--root",folder,"--font",font,"--terminal",terminal,"--editor",editor,"--terminal-font",mono,"--cache",cache,"--diagnostics")
        wait(lambda:state(count=2,depth=0),"initial native view")
        maps=Path(f"/proc/{app.pid}/maps").read_text();assert not any(s in maps for s in ("libSDL","libvulkan","libEGL","libX11"))
        loaded={Path(line.split()[-1]).resolve() for line in maps.splitlines() if "/libnexaui.so" in line};assert len(loaded)==1
        library=loaded.pop();candidate=library.parents[2];metadata=json.loads((candidate/"build.json").read_text())
        pin=json.loads((ROOT/"spec/nexaui-inputs.json").read_text())
        assert str(candidate) in pin["development_builds"].values() and library.name=="libnexaui.so."+pin["version"]
        assert metadata["revision"]==pin["revision"] and metadata["version"]==pin["version"] and metadata["atspi_compiled"]
        assert digest(library)==metadata["artifacts"][library.relative_to(candidate).as_posix()]
        (root/"maps.txt").write_text(maps);capture("initial list")
        chrome_cases()
        if fixture:
            peer('initial');wait(lambda:state(grid=1,selected=1,depth=0),'native selection without opening')
            assert 'OPEN pid=' not in log()
            peer('dialog');wait(lambda:state(dialog=1),'accessible inline naming')
            peer('text');wait(lambda:state(name_bad=1),'native oversized replacement retains original draft')
            capture('native text read write selection and retained rejection')
            key(22,ctrl=True);wait(lambda:state(name_bad=0),'explicit native keyboard clear')
            typing('bad');key(53);wait(lambda:state(name_bad=1),'actual invalid name')
            peer('invalid');peer('cancel');wait(lambda:state(dialog=0,selected=1),'accessible cancel/focus return')
            peer('query');wait(lambda:state(count=1,depth=0),'native query changes actual filtered entries')
            capture('native query edits actual filtered entries')
            peer('query-clear');wait(lambda:state(count=2,depth=0),'native query clear restores actual entries')
            assert sorted(p.name for p in folder.iterdir())==['existing','seed.txt']
        click(2);wait(lambda:state(grid=1),"pointer grid")
        key(49,ctrl=True,shift=True);wait(lambda:state(dialog=1),"inline folder")
        typing("wrok");key(102);key(106);key(106,shift=True);key(106,shift=True)
        field_fence(15);capture("native middle-name selection");typing("or");capture("inline grid folder");key(28)
        wait(lambda:(folder/"work").is_dir() and state(dialog=0,count=3),"real folder worker")
        key(28);wait(lambda:state(depth=1,count=0),"enter held folder")
        key(49,ctrl=True);wait(lambda:state(dialog=1),"inline file")
        typing("long"*20);field_fence(15);capture("native long-name end caret");key(102);field_fence(15);capture("native long-name beginning caret")
        key(30,ctrl=True);typing("note.txt");key(28)
        wait(lambda:(folder/"work/note.txt").is_file() and state(dialog=0,count=1),"real file worker");capture("created file")
        key(28);wait(lambda:re.search(r"^OPEN pid=(\d+) pin=1$",log(),re.M) and re.search(r"^READY .*editor=(\d+)",log(),re.M),"original document")
        terminal_pid=int(re.findall(r"^OPEN pid=(\d+) pin=1$",log(),re.M)[-1]);editor_pid=int(re.findall(r"^READY .*editor=(\d+)",log(),re.M)[-1])
        actors={}
        for pid,binary,parent in ((terminal_pid,terminal,app.pid),(editor_pid,editor,terminal_pid)):
            pin=os.pidfd_open(pid);pins.append(pin);proc=Path(f"/proc/{pid}");fields=(proc/"stat").read_text().rsplit(")",1)[1].split()
            assert int(fields[1])==parent and (proc/"exe").resolve()==binary
            actors[str(pid)]={"binary":str(binary),"sha256":hashlib.sha256((proc/"exe").read_bytes()).hexdigest(),"parent":parent}
        (root/"actors.json").write_text(json.dumps(actors,indent=2))
        def document_log():return log().rsplit("READY wayland opengles2 editor=",1)[-1]
        wait(lambda:"FOCUS 1\n" in document_log() and re.search(r"^DRAWN \d+$",document_log(),re.M),"original editor frame and keyboard focus")
        def accepted():
            values=re.findall(r"^INPUT (\d+)$",document_log(),re.M);return int(values[-1]) if values else 0
        target=accepted()+6;typing("ihello");wait(lambda:accepted()==target,"actual editor input acceptance")
        key(31,ctrl=True);wait(lambda:(folder/"work/note.txt").read_bytes()==b"hello","exact editor save")
        key(16,ctrl=True);wait(lambda:"DOCUMENT_END status=0" in log(),"original document end")
        key(102,alt=True);wait(lambda:state(depth=0),"held root")
        key(38,ctrl=True);typing("seed");wait(lambda:state(count=1),"filter")
        rows=re.findall(r"^CONTROL 5 (\d+) ([\d.]+) ([\d.]+) ([\d.]+) ([\d.]+) (\d+)$",log(),re.M);assert rows
        _,x,y,w,h,_=rows[-1];x,y,w,h=map(float,(x,y,w,h))
        event(f"M {40+x+2} {40+y+h/2}");event("B 272 1");event("B 272 0")
        event("K 42 1");event(f"M {40+x+w-3} {40+y+h/2}");event("B 272 1");event("B 272 0");event("K 42 0")
        capture("native mouse query selection");typing("seed");wait(lambda:state(count=1),"mouse selection replacement")
        key(60);wait(lambda:state(dialog=1),"rename");key(22,ctrl=True);typing("renamed.txt");key(28)
        wait(lambda:(folder/"renamed.txt").is_file() and not (folder/"seed.txt").exists() and state(dialog=0),"rename worker")
        key(45,ctrl=True);key(38,ctrl=True);typing("work");wait(lambda:state(count=1),"move target filter");key(28)
        wait(lambda:state(depth=1),"move target");key(47,ctrl=True);wait(lambda:state(dialog=1),"move review");capture("move review");key(28)
        wait(lambda:(folder/"work/renamed.txt").is_file() and state(dialog=0),"move worker")
        assert (folder/"work/renamed.txt").read_bytes()==b"seed"
        key(64)
        for _ in range(3):key(66)
        wait(lambda:state(german=1,scale="2.00"),"German 200");capture("German grid 200")
        resizes=log("server").count(".resize(");event("M 1038 718");event("B 272 1")
        wait(lambda:log("server").count(".resize(")>resizes,"original native resize admitted")
        event("M 678 458");event("B 272 0");wait(lambda:state(width=640,height=420),"native minimum size")
        key(49,ctrl=True);wait(lambda:state(dialog=1),"minimum 200 inline naming");typing("draft");capture("minimum German 200 inline naming");key(1)
        wait(lambda:state(dialog=0),"minimum inline cancellation");assert not (folder/"work/draft").exists()
        key(67);wait(lambda:state(info_overlay=1),"information overlay");capture("information overlay")
        if fixture:peer('info');wait(lambda:state(info_overlay=0),'native localized information close')
        releases=log().count("KEY_END escape\n");key(1)
        wait(lambda:state(info_overlay=0) and log().count("KEY_END escape\n")>releases,"original Escape release processed and drawn")
        before=log().count("VIEW ");ticks=Path(f"/proc/{app.pid}/stat").read_text().rsplit(")",1)[1].split();initial=int(ticks[11])+int(ticks[12]);time.sleep(1.2)
        ticks=Path(f"/proc/{app.pid}/stat").read_text().rsplit(")",1)[1].split();delta=int(ticks[11])+int(ticks[12])-initial
        assert delta<=2 and log().count("VIEW ")==before,(delta,before,log().count("VIEW "))
        if fixture:peer('close')
        else:key(16,ctrl=True)
        assert app.wait(timeout=8)==0;children.remove(app)
        if fixture:
            wait(lambda:'UNMAP 0\n' in log('server'),'original app retired')
            negative=start(app_bin,'negative-app','--root',folder,'--font',font,'--terminal',terminal,'--editor',editor,'--terminal-font',mono,'--cache',cache,'--diagnostics',extra_env={**env,'NO_AT_BRIDGE':'1'})
            wait(lambda:'VIEW ' in log('negative-app'),'disabled-bridge native view')
            rejected=start(client,'negative-peer',negative.pid,'initial')
            assert rejected.wait(timeout=10)==1 and 'target app was not registered' in log('negative-peer'),log('negative-peer');children.remove(rejected)
            negative.terminate();assert negative.wait(timeout=8)==0;children.remove(negative)
        server.terminate();assert server.wait(timeout=8)==0;children.remove(server)
        if fixture:
            for child in (registry,bus):
                child.terminate();assert child.wait(timeout=8) in (0,-signal.SIGTERM);children.remove(child)
        for p in root.glob("*.log"):assert not re.search(r"AddressSanitizer|LeakSanitizer|runtime error:|GLib-GObject-CRITICAL",p.read_text(errors="replace")),p
        result={"result":"passed","scope":"opt-in opaque NexaUI Files on private compositor; no default/physical/production admission", "images":images,"actors":actors,
                "idle_cpu_ticks":delta,"binaries":{str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in (server_bin,app_bin,terminal,editor)}}
        result["sdk"]={"revision":metadata["revision"],"version":metadata["version"],"library":str(library),"sha256":digest(library)}
        result["font"]={"path":str(font),"sha256":digest(font)}
        result['atspi']='independent private registry/client and disabled-bridge negative' if fixture else 'unavailable bus only'
        result['peer_sha256']=digest(client) if fixture else None
        (root/"result.json").write_text(json.dumps(result,indent=2)+"\n");print(json.dumps({"result":"passed","artifacts":str(root),"images":len(images)}))
    finally:
        for pin in reversed(pins):
            try:signal.pidfd_send_signal(pin,signal.SIGHUP)
            except ProcessLookupError:pass
            os.close(pin)
        for p in reversed(children):
            if p.poll() is None:
                p.terminate()
                try:p.wait(timeout=5)
                except subprocess.TimeoutExpired:p.kill();p.wait(timeout=5)
        for stream in streams:stream.close()
        left.close();right.close();print("Files view evidence:",root)


if __name__=="__main__":main()
