"""Native fixed owner with separately bound Files; only private test adapters.

No root controller, PAM/DRM service or production app admission in this fixture.
Drift changes only copied source inputs, never the shared SDK or source checkout.
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

import development_session as session
from test_development_desktop import artifact_root,control_position,role_output


def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    build=Path(sys.argv[1]).resolve();settings_ui=sys.argv[2];assert settings_ui in ('sdl','nexaui-owned')
    terminal_ui=sys.argv[3] if len(sys.argv)>3 else 'sdl';assert terminal_ui in ('sdl','nexaui-owned')
    files_ui=sys.argv[4] if len(sys.argv)>4 else 'nexaui-owned';assert files_ui in ('sdl','nexaui-owned')
    compose=len(sys.argv)>5 and sys.argv[5]=='compose'
    assert os.getuid()==1000
    source=Path(__file__).resolve().parents[1]
    profile='debug' if 'AETHERI_SANITIZERS:BOOL=ON' in (build/'CMakeCache.txt').read_text() else 'release'
    session.build_for=lambda selected_profile: build
    selected=session.files_selection(profile,files_ui)
    terminal_profile=session.terminal_selection(profile,terminal_ui)
    terminal=Path(terminal_profile['presentation']['binary']) if terminal_profile else build/'aetheri-terminal'
    root=Path(tempfile.mkdtemp(prefix='fx-own-',dir=artifact_root(build)))
    for name in ('captures','sessions','sessions/fixture','sessions/fixture/runtime','sessions/fixture/cache','sessions/fixture/documents'):
        (root/name).mkdir(mode=0o700,parents=True,exist_ok=True)
    copies={'spec/nexaui-inputs.json',*session.FILES_MODEL_SOURCES,*session.OWN_FILES_SOURCES}
    if terminal_profile:
        view=terminal_profile['presentation']
        copies.update(('spec/ui-inputs.json',*terminal_profile['model'],*view['sources']))
        if 'starter' in view:copies.update(view['starter']['sources'])
    if settings_ui=='nexaui-owned':copies.update((*session.OWN_SETTINGS_SOURCES,'src/settings/model.c','src/settings/model.h'))
    for name in copies:
        p=root/'source'/name;p.parent.mkdir(mode=0o700,parents=True,exist_ok=True);p.write_bytes((source/name).read_bytes())
    documents=root/'sessions/fixture/documents';(documents/'seed.txt').write_bytes(b'seed')
    env={**os.environ,'AETHERI_TEST_FILES_UI':files_ui,'AETHERI_TEST_FILES_SOURCE':str(root/'source'),
        'AETHERI_TEST_TERMINAL_UI':terminal_ui,'AETHERI_TEST_TERMINAL_SOURCE':str(root/'source'),
        'AETHERI_TEST_SETTINGS_UI':settings_ui,'AETHERI_TEST_SETTINGS_SOURCE':str(root/'source'),
        'DBUS_SESSION_BUS_ADDRESS':'unix:path='+str(root/'absent-bus'),
        'ASAN_OPTIONS':'detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS':'halt_on_error=1:print_stacktrace=1'}
    env.pop('AT_SPI_BUS_ADDRESS',None)
    left,right=socket.socketpair(socket.AF_UNIX,socket.SOCK_SEQPACKET);left.settimeout(4)
    stream=(root/'owner.log').open('xb')
    owner=subprocess.Popen([sys.executable,str(source/'tests/development_session_fixture.py'),str(build),str(root),str(right.fileno()),*([str(source/'tools/development_session.py'),'compose'] if compose else [])],
        cwd=root,pass_fds=(right.fileno(),),env=env,stdin=subprocess.DEVNULL,stdout=stream,stderr=subprocess.STDOUT)
    right.close();pins=[os.pidfd_open(owner.pid)];roles={};seen=0;images=[];cases=[];actors=[]
    def log():return (root/'owner.log').read_text(errors='replace')
    def rolelog(role):return role_output(log(),role)
    def alive(pin):return not select.select([pin],[],[],0)[0]
    def pin_process(pid,parent,binary=None):
        fd=os.pidfd_open(pid);pins.append(fd);proc=Path('/proc')/str(pid)
        assert int((proc/'stat').read_text().rsplit(')',1)[1].split()[1])==parent
        executable=(proc/'exe').resolve()
        if binary is not None:assert executable==binary
        actors.append({'pid':pid,'parent':parent,'exe':str(executable),'sha256':digest(proc/'exe')})
        return fd
    def enroll():
        nonlocal seen
        spawns=re.findall(r'^SPAWN (\w+) (\d+)$',log(),re.M)
        for role,pid in spawns[seen:]:
            pid=int(pid);roles[role]=(pid,pin_process(pid,owner.pid))
        seen=len(spawns)
    def wait(predicate,label):
        deadline=time.monotonic()+12
        while not predicate():
            enroll();assert owner.poll() is None and time.monotonic()<deadline,(label,root,log()[-6000:]);time.sleep(.01)
        enroll()
    def event(value):left.sendall(value.encode());assert left.recv(32)==b'OK'
    def key(code,ctrl=False,shift=False):
        modifiers=([29] if ctrl else [])+([42] if shift else [])
        for c in modifiers:event(f'K {c} 1')
        event(f'K {code} 1');event(f'K {code} 0')
        for c in reversed(modifiers):event(f'K {c} 0')
    codes=dict(zip('abcdefghijklmnopqrstuvwxyz',(30,48,46,32,18,33,34,35,23,36,37,38,50,49,24,25,16,19,31,20,22,47,17,45,21,44)))
    codes.update({'.':52,' ':57,'-':12,'\n':28})
    def typing(value):
        for c in value:key(52,shift=True) if c=='>' else key(codes[c])
    def search(query):
        app={'files':'org.aetheri.Files','settings':'org.aetheri.Settings','terminal':'org.aetheri.Terminal','editor':'org.aetheri.Editor'}[query]
        active=rolelog('desktop').count('app='+app+' active=1')
        collapsed=rolelog('desktop').count('STABLE 0 1920x84\n')
        before=rolelog('desktop').count('STABLE 0 1920x1080\n');event('M 100 100');event('K 125 1');key(57);event('K 125 0')
        wait(lambda:rolelog('desktop').count('STABLE 0 1920x1080\n')>before,'search view')
        typing(query);wait(lambda:'query='+query+' ' in rolelog('desktop'),'native query');key(28)
        wait(lambda:rolelog('desktop').count('app='+app+' active=1')>active and rolelog('desktop').count('STABLE 0 1920x84\n')>collapsed,'actual search activation and collapsed desktop')
    def dock(kind):
        point=[]
        def observed():
            p=control_position(rolelog('desktop'),kind)
            if p is None:return False
            point[:]=p;return True
        wait(observed,'complete presented dock control')
        x,y=point;event(f'M {x} {y}');event('B 272 1');event('B 272 0')
    def file_state(**wanted):
        rows=[r for r in re.findall(r'^VIEW (.+)$',rolelog('files'),re.M) if re.search(r'\bcount=',r)]
        values=dict(re.findall(r'(\w+)=([^ ]+)',rows[-1])) if rows else {}
        return all(values.get(k)==str(v) for k,v in wanted.items())
    def count(role):return len(re.findall(r'^SPAWN '+role+' ',log(),re.M))
    def workspace():
        rows=re.findall(r'^WORKSPACE (.+)\n((?:(?:PANE|DIVIDER) .*\n)*)WORKSPACE_END frame=(\d+)\n',rolelog('terminal'),re.M)
        if not rows:return {}
        header,body,end=rows[-1]
        fields=lambda row:{k:int(v) for k,v in re.findall(r'(\w+)=(\d+)(?= |$)',row)}
        state=fields(header);assert state['frame']==int(end)
        state['panes']=[fields(line) for line in body.splitlines() if line.startswith('PANE ')]
        return state
    def capture(label):
        n=len(cases)+1;signal.pidfd_send_signal(roles['compositor'][1],signal.SIGUSR1)
        wait(lambda:all(f'CAPTURE {n} {i}\n' in rolelog('compositor') for i in range(3)),label)
        for i in range(3):
            p=root/'captures'/f'capture-{n}-output-{i}.ppm';raw=p.read_bytes();magic,size,peak,pixels=raw.split(b'\n',3);w,h=map(int,size.split())
            assert magic==b'P6' and peak==b'255' and len(pixels)==w*h*3 and len(set(pixels))>16
            packed=p.with_suffix('.ppm.gz');packed.write_bytes(gzip.compress(raw,mtime=0));assert gzip.decompress(packed.read_bytes())==raw;p.unlink()
            images.append({'case':label,'output':i,'file':packed.name,'sha256':hashlib.sha256(raw).hexdigest()})
        cases.append(label)
    try:
        wait(lambda:'FIXTURE_NOTIFY READY=1' in log() and 'STABLE 2 3840x84\n' in rolelog('desktop'),'owner ready')
        search('files');wait(lambda:file_state(count=1) and 'LAUNCH_REPLY A F\n' in rolelog('desktop'),'bound Files search launch')
        first=roles['files'];maps=Path(f'/proc/{first[0]}/maps').read_text()
        if selected:
            assert digest(Path(f'/proc/{first[0]}/exe'))==selected['presentation']['sha256']
            assert str(Path(selected['build'])/'prefix/lib/libnexaui.so.') in maps and 'libSDL' not in maps
        else:assert (Path(f'/proc/{first[0]}/exe')).resolve()==build/'aetheri-files' and 'libnexaui' not in maps
        (root/'maps.txt').write_text(maps);capture('bound Files from global search')
        replies=rolelog('desktop').count('LAUNCH_REPLY A F\n');dock(5)
        wait(lambda:rolelog('desktop').count('LAUNCH_REPLY A F\n')>replies,'dock reuse');assert count('files')==1 and alive(first[1])
        key(49,ctrl=True);wait(lambda:file_state(dialog=1),'inline file');typing('note.txt');key(28)
        wait(lambda:(documents/'note.txt').is_file() and file_state(dialog=0,count=2),'real creation');key(28)
        wait(lambda:re.search(r'^OPEN pid=(\d+) pin=1$',rolelog('files'),re.M) and re.search(r'^READY .*editor=(\d+)',rolelog('files'),re.M),'original document')
        doc_terminal=int(re.findall(r'^OPEN pid=(\d+) pin=1$',rolelog('files'),re.M)[-1]);doc_editor=int(re.findall(r'^READY .*editor=(\d+)',rolelog('files'),re.M)[-1])
        terminal_pin=pin_process(doc_terminal,first[0],terminal);editor_pin=pin_process(doc_editor,doc_terminal,build/'aetheri-editor')
        if terminal_profile:
            assert alive(terminal_pin)
            assert b'--pty-starter' not in Path(f'/proc/{doc_terminal}/cmdline').read_bytes().split(b'\0')
            editor_maps=Path(f'/proc/{doc_terminal}/maps').read_text()
            assert digest(Path(f'/proc/{doc_terminal}/exe'))==terminal_profile['presentation']['sha256']
            assert str(Path(terminal_profile['build'])/'prefix/lib/libnexaui.so.') in editor_maps and 'libSDL' in editor_maps
            assert alive(terminal_pin);(root/'editor-maps.txt').write_text(editor_maps)
        def document_log():return rolelog('files').rsplit('READY wayland opengles2 editor=',1)[-1]
        wait(lambda:'FOCUS 1\n' in document_log() and re.search(r'^DRAWN \d+$',document_log(),re.M),'editor actually focused/drawn')
        def accepted():
            values=re.findall(r'^INPUT (\d+)$',document_log(),re.M);return int(values[-1]) if values else 0
        target=accepted()+6;typing('ihello');wait(lambda:accepted()==target,'dirty editor input')
        assert (documents/'note.txt').read_bytes()==b''
        focus=rolelog('desktop').count('app=org.aetheri.Files active=1')
        search('files');wait(lambda:rolelog('desktop').count('app=org.aetheri.Files active=1')>focus,'return to Files')
        before_views=len(re.findall(r'^VIEW .*\bcount=',rolelog('files'),re.M));key(16,ctrl=True)
        if selected:wait(lambda:file_state(closing=1),'Files waits for original document')
        else:
            # The original SDL log has no closing field. Observe its new frame,
            # retained original child, then the actual exit after that child saves.
            wait(lambda:len(re.findall(r'^VIEW .*\bcount=',rolelog('files'),re.M))>before_views and file_state(child=doc_terminal),'SDL Files retains original document after close')
        assert alive(first[1]) and alive(terminal_pin) and alive(editor_pin)
        capture('Files close retains the unsaved original editor')
        focus=rolelog('desktop').count('app=org.aetheri.Editor active=1')
        search('editor');wait(lambda:rolelog('desktop').count('app=org.aetheri.Editor active=1')>focus,'return to editor')
        key(31,ctrl=True);wait(lambda:(documents/'note.txt').read_bytes()==b'hello','original document save');key(16,ctrl=True)
        wait(lambda:'LAUNCH_REPLY X F\n' in rolelog('desktop'),'original Files and pipe retired')
        assert not alive(first[1]) and not alive(terminal_pin) and not alive(editor_pin)
        for name in (('spec/nexaui-inputs.json','src/files/nexaui_view.c','src/files/session.c') if selected else ()):
            p=root/'source'/name;original=p.read_bytes()
            if p.suffix=='.json':
                data=json.loads(original);data['adoption']+=' private drift';p.write_text(json.dumps(data)+'\n')
            else:p.write_bytes(original+b'\n/* private fixture drift */\n')
            errors=rolelog('desktop').count('LAUNCH_REPLY E F\n');dock(5)
            wait(lambda:rolelog('desktop').count('LAUNCH_REPLY E F\n')>errors,'changed Files binding refuses spawn')
            assert count('files')==1 and 'FILES_PROFILE_REJECTED ' in log()
            p.write_bytes(original)
        if terminal_profile:
            for name in ('spec/nexaui-inputs.json','spec/ui-inputs.json','src/terminal/model.c','src/terminal/nexaui_view.c','src/terminal/main.c',
                         'src/terminal/spawn.c','src/terminal/workspace.c','src/terminal/pty_starter.c'):
                p=root/'source'/name;original=p.read_bytes()
                if p.suffix=='.json':
                    data=json.loads(original);data['terminal_fixture_drift']=True;p.write_text(json.dumps(data)+'\n')
                else:p.write_bytes(original+b'\n/* private terminal drift */\n')
                for kind,role in ((4,'terminal'),(5,'files')):
                    errors=rolelog('desktop').count('LAUNCH_REPLY E '+('T' if role=='terminal' else 'F')+'\n');dock(kind)
                    wait(lambda:rolelog('desktop').count('LAUNCH_REPLY E '+('T' if role=='terminal' else 'F')+'\n')>errors,'Terminal drift refuses dependent fresh role')
                    assert count('terminal')==0 and count('files')==1
                    rejected='FILES_PROFILE_REJECTED ' if role=='files' and selected and name=='spec/nexaui-inputs.json' else 'TERMINAL_PROFILE_REJECTED '+role
                    assert rejected in log()
                p.write_bytes(original)
        dock(2);wait(lambda:('FRAME ' if settings_ui=='nexaui-owned' else 'READY ') in rolelog('settings'),'independent Settings')
        dock(4);wait(lambda:'READY ' in rolelog('terminal'),'independent Terminal')
        terminal_first=None
        if terminal_profile:
            terminal_first=roles['terminal'];shell=int(re.findall(r'^READY .*shell=(\d+)',rolelog('terminal'),re.M)[-1])
            shell_pin=pin_process(shell,terminal_first[0],build/'aetheri-shell')
            terminal_maps=Path(f'/proc/{terminal_first[0]}/maps').read_text()
            assert digest(Path(f'/proc/{terminal_first[0]}/exe'))==terminal_profile['presentation']['sha256']
            assert str(Path(terminal_profile['build'])/'prefix/lib/libnexaui.so.') in terminal_maps and 'libSDL' in terminal_maps
            assert alive(terminal_first[1]);(root/'terminal-maps.txt').write_text(terminal_maps)
            wait(lambda:'NEXAUI_FRAME ' in rolelog('terminal') and 'FOCUS 1\n' in rolelog('terminal'),'actual selected Terminal frame/focus')
            if compose:
                key(127);key(40);key(18)
                wait(lambda:re.search(r'TEXT commit accepted=1 bytes=2',rolelog('terminal')),'supervised Compose reaches selected presenter')
                key(22,ctrl=True)
            if terminal_profile['presentation']['kind']=='aetheri-terminal-v2':
                command=Path(f'/proc/{terminal_first[0]}/cmdline').read_bytes().split(b'\0')
                assert command[command.index(b'--pty-starter')+1].decode()==terminal_profile['presentation']['starter']['binary']
                wait(lambda:workspace().get('sessions')==1,'supervised original workspace')
                key(32,ctrl=True,shift=True)
                wait(lambda:workspace().get('sessions')==2 and workspace()['visible']==3 and
                     any(p['selected'] and p['x']==9 for p in workspace()['panes']),'supervised independent split shell')
                split=next(p for p in workspace()['panes'] if p['selected'])
                split_pin=pin_process(split['pid'],terminal_first[0],build/'aetheri-shell')
                typing('printf owned > owned-proof\n')
                proof=root/'owned-proof'
                wait(lambda:proof.exists() and proof.read_bytes()==b'owned','split executes in original owner working directory')
                key(20,ctrl=True,shift=True)
                wait(lambda:workspace().get('sessions')==3 and workspace()['tabs']==2 and
                     any(p['selected'] and p['x']==9 for p in workspace()['panes']),'supervised independent tab shell')
                tab=next(p for p in workspace()['panes'] if p['selected'])
                tab_pin=pin_process(tab['pid'],terminal_first[0],build/'aetheri-shell')
                key(17,ctrl=True,shift=True)
                wait(lambda:workspace().get('sessions')==2 and workspace()['tabs']==1,'supervised tab closes independently')
                assert not alive(tab_pin) and alive(split_pin) and alive(shell_pin)
                capture('owner-selected original terminal creates independent split and tab')
            p=root/'source/src/terminal/nexaui_view.c';original=p.read_bytes();p.write_bytes(original+b'\n/* live original remains owned */\n')
            replies=rolelog('desktop').count('LAUNCH_REPLY A T\n');dock(4)
            wait(lambda:rolelog('desktop').count('LAUNCH_REPLY A T\n')>replies,'reuse original Terminal under source drift')
            assert count('terminal')==1 and alive(terminal_first[1]);p.write_bytes(original)
            search('terminal');key(16,ctrl=True,shift=True)
            wait(lambda:'LAUNCH_REPLY X T\n' in rolelog('desktop'),'original Terminal and pipe retired')
            assert not alive(terminal_first[1]) and not alive(shell_pin)
            if terminal_profile['presentation']['kind']=='aetheri-terminal-v2':assert not alive(split_pin)
            ready_before=rolelog('terminal').count('READY ');frames_before=rolelog('terminal').count('NEXAUI_FRAME ')
            dock(4);wait(lambda:count('terminal')==2 and alive(roles['terminal'][1]) and rolelog('terminal').count('READY ')>ready_before and rolelog('terminal').count('NEXAUI_FRAME ')>frames_before,'fresh selected Terminal')
            assert roles['terminal'][0]!=terminal_first[0]
            capture('same selected Terminal reused then freshly reopened with original child end')

        dock(5);wait(lambda:count('files')==2 and file_state(**({'count':2,'closing':0} if selected else {'count':2,'child':-1})),'fresh bound Files')
        second=roles['files'];assert second[0]!=first[0] and all(alive(roles[r][1]) for r in ('files','settings','terminal','desktop','compositor'))
        assert (documents/'note.txt').read_bytes()==b'hello';capture('fresh Files coexists with Settings and Terminal')
        before=rolelog('desktop').count('STABLE 0 1920x1080\n');event('K 125 1');key(57);event('K 125 0')
        wait(lambda:rolelog('desktop').count('STABLE 0 1920x1080\n')>before,'quit search');key(16,ctrl=True)
        assert owner.wait(timeout=12)==0,log();enroll();assert all(not alive(fd) for fd in pins)
        end=json.loads(re.search(r'^SESSION_END (.+)$',log(),re.M)[1]);assert all(v==0 for v in end['children'].values())
        expected_history=[{'role':'files','pid':first[0],'status':0}]
        if terminal_first:expected_history.append({'role':'terminal','pid':terminal_first[0],'status':0})
        assert end['app_history']==expected_history
        assert log().rfind('files: CLOSED files ')<log().rfind('terminal: CLOSED ')<log().rfind('settings: ' + ('SHUTDOWN signal' if settings_ui=='nexaui-owned' else 'CLOSED '))<log().rfind('desktop: CLOSED ')
        assert not re.search(r'AddressSanitizer|LeakSanitizer|runtime error:',log())
        for name in copies:assert (root/'source'/name).read_bytes()==(source/name).read_bytes()
        result={'result':'passed','profile':profile,'settings_ui':settings_ui,'files_ui':files_ui,'terminal_ui':terminal_ui,'terminal_selection':terminal_profile,'compose':compose,'selection':selected,'binding':session.profile_fingerprint(selected) if selected else None,
            'cases':cases,'images':images,'actors':actors,'end':end,'scope':'native private owner/seat; no DRM, production admission or complete removal'}
        (root/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({'result':'passed','artifacts':str(root)}))
    finally:
        if owner.poll() is None:owner.terminate()
        for fd in reversed(pins):
            try:signal.pidfd_send_signal(fd,signal.SIGKILL)
            except ProcessLookupError:pass
            os.close(fd)
        if owner.poll() is None:owner.wait(timeout=5)
        left.close();right.close();stream.close();print('Files owner evidence:',root)


if __name__=='__main__':main()
