"""Separate Wayland client observes startup layouts and fresh-device mappings.

Only the inherited test fixture injects physical key codes. No host input,
installed service, physical keyboard or runtime layout selector is exercised.
"""
import json
import os
from pathlib import Path
import socket
import subprocess
import sys
import tempfile
import time


def main():
    binary,fixture,client_binary,build=map(lambda p:Path(p).resolve(),sys.argv[1:])
    assert os.getuid()!=0
    root=Path(tempfile.mkdtemp(prefix='keyboard-layout-',dir=build))
    runtime=root/'runtime';runtime.mkdir(mode=0o700)
    env={**os.environ,'XDG_RUNTIME_DIR':str(runtime),'WAYLAND_DISPLAY':'layout-private',
         'XKB_DEFAULT_LAYOUT':'fr','ASAN_OPTIONS':'detect_leaks=1:halt_on_error=1',
         'UBSAN_OPTIONS':'halt_on_error=1:print_stacktrace=1'}
    for name in ('DISPLAY','WAYLAND_SOCKET'):env.pop(name,None)
    cases=[]
    # Every malformed profile must fail before creating a compositor socket.
    invalid=[['--keyboard-layout'],['--keyboard-layout',''],['--keyboard-layout','fr'],
             ['--keyboard-layout','de,us'],['--keyboard-layout','../de'],
             ['--keyboard-layout','de','--keyboard-layout','us']]
    for index,options in enumerate(invalid):
        p=subprocess.run([str(binary),'--headless',env['WAYLAND_DISPLAY'],'-',*options],env=env,capture_output=True,timeout=5)
        (root/f'invalid-{index}.log').write_bytes(p.stdout+p.stderr)
        assert p.returncode==2 and b'usage (unprivileged)' in p.stderr,(options,p.returncode)
        assert not (runtime/env['WAYLAND_DISPLAY']).exists()
    p=subprocess.run([str(binary),'--nested',env['WAYLAND_DISPLAY'],'-','parent','--keyboard-layout','de'],env=env,capture_output=True,timeout=5)
    (root/'nested-invalid.log').write_bytes(p.stdout+p.stderr)
    assert p.returncode==2 and b'usage (unprivileged)' in p.stderr
    cases.append('invalid, repeated and nested overrides fail before socket creation')

    for layout in ('us','de'):
        folder=root/layout;folder.mkdir();processes=[];streams=[]
        controller,issuer=socket.socketpair(socket.AF_UNIX,socket.SOCK_SEQPACKET);controller.settimeout(4)
        def start(program,name,*args,pass_fds=()):
            stream=(folder/(name+'.log')).open('xb');streams.append(stream)
            p=subprocess.Popen([str(program),*map(str,args)],env=env,stdin=subprocess.PIPE,stdout=stream,stderr=subprocess.STDOUT,pass_fds=pass_fds)
            processes.append(p);return p
        def log(name='client'):return (folder/(name+'.log')).read_text(errors='replace')
        def wait(check,label):
            end=time.monotonic()+7
            while not check():
                assert all(p.poll() is None for p in processes),(layout,label,log('server'))
                assert time.monotonic()<end,(layout,label,log(),log('server'))
                time.sleep(.01)
        def event(text):controller.sendall(text.encode());assert controller.recv(32)==b'OK',text
        def key(code,down):event(f'K {code} {int(down)}')
        def character(code,expected,modifier=None):
            line=f'KEY 0 {code} 1 {expected}\n';before=log().count(line)
            if modifier is not None:key(modifier,True)
            key(code,True);key(code,False)
            if modifier is not None:key(modifier,False)
            wait(lambda:log().count(line)>before,f'code{code}/U+{expected:04X}')
        try:
            server=start(fixture,'server','--headless',env['WAYLAND_DISPLAY'],'-',issuer.fileno(),'--keyboard-layout',layout,pass_fds=(issuer.fileno(),));issuer.close()
            wait(lambda:'READY ' in log('server'),'server ready')
            client=start(client_binary,'client','--input')
            wait(lambda:'KEYBOARD_ENTER 1\n' in log(),'native client keymap/focus')
            event('M 100 100');event('B 272 1');event('B 272 0')
            wait(lambda:'KEYBOARD_ENTER 0\n' in log(),'first original window focus')
            expected=(121,122,59,39,91,45) if layout=='us' else (122,121,246,228,252,223)
            for code,value in zip((21,44,39,40,26,12),expected):character(code,value)
            character(21,89 if layout=='us' else 90,42)
            character(16,113 if layout=='us' else 64,100)
            # Each newly attached keyboard must use the same explicit profile,
            # even if the backend supplied a different original map.
            for add in ('A','G'):
                maps=log().count('KEYMAP\n');enters=log().count('KEYBOARD_ENTER 0\n')
                event('D');event(add)
                wait(lambda:log().count('KEYMAP\n')>maps and log().count('KEYBOARD_ENTER 0\n')>enters,'fresh keyboard')
                character(21,expected[0]);character(30,97)
            cases.append(layout+' native Y/Z, punctuation, Shift/AltGr and replacement keyboard')
            client.stdin.write(b'q');client.stdin.flush();assert client.wait(timeout=6)==0,log()
            server.terminate();assert server.wait(timeout=6)==0,log('server')
        finally:
            for p in reversed(processes):
                if p.poll() is None:
                    p.terminate()
                    try:p.wait(timeout=3)
                    except subprocess.TimeoutExpired:p.kill();p.wait(timeout=3)
                p.stdin.close()
            for stream in streams:stream.close()
            controller.close();issuer.close()
    result=dict(passed=True,cases=cases,physical_keyboard=False,installed_selector=False)
    (root/'result.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(dict(result=result,artifacts=str(root))))


if __name__=='__main__':main()
