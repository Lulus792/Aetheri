"""Real direct-input Editor on a private native Wayland seat.

Explicit disposable-carrier test, not normal app/permission/hardware acceptance.
Uses original compositor process handles; no system service or installed inputs.
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

ROOT = Path(__file__).resolve().parents[1]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    assert len(sys.argv) == 6 and os.getuid() != 0
    server_bin, app_bin, font, mono, build = map(lambda p: Path(p).resolve(), sys.argv[1:])
    assert digest(font) == json.loads((ROOT / 'spec/ui-inputs.json').read_text())['font']['sha256']
    root = Path(tempfile.mkdtemp(prefix='ed-', dir=build))
    runtime = root / 'runtime'; runtime.mkdir(mode=0o700)
    captures = root / 'captures'; captures.mkdir(mode=0o700)
    folder = root / 'documents'; folder.mkdir(mode=0o700)
    (folder / 'seed.txt').write_bytes(b'original\n')
    assert len(os.fsencode(runtime/'ed')) < 108
    env = {**os.environ, 'XDG_RUNTIME_DIR': str(runtime), 'WAYLAND_DISPLAY': 'ed',
           'NEXAUI_PLATFORM': 'wayland', 'DBUS_SESSION_BUS_ADDRESS': 'unix:path=' + str(root / 'absent-bus'),
           'LIBGL_ALWAYS_SOFTWARE': '1', 'GALLIUM_DRIVER': 'llvmpipe', 'LP_NUM_THREADS': '2',
           'ASAN_OPTIONS': 'detect_leaks=1:halt_on_error=1', 'UBSAN_OPTIONS': 'halt_on_error=1:print_stacktrace=1'}
    for name in ('DISPLAY', 'WAYLAND_SOCKET', 'AT_SPI_BUS_ADDRESS'):
        env.pop(name, None)
    left, right = socket.socketpair(socket.AF_UNIX, socket.SOCK_SEQPACKET); left.settimeout(4)
    children = []; streams = []; images = []; current_name = 'app'
    def start(binary, name, *args, pass_fds=()):
        stream = (root / (name + '.log')).open('xb'); streams.append(stream)
        p = subprocess.Popen([str(binary), *map(str, args)], env=env, stdin=subprocess.DEVNULL,
                             stdout=stream, stderr=subprocess.STDOUT, pass_fds=pass_fds)
        children.append(p); return p
    def log(name=None):
        if name is None: name = current_name
        path = root / (name + '.log')
        return path.read_text(errors='replace') if path.exists() else ''
    def view():
        frames = re.findall(r'^FRAME ([^\n]+)$', log(), re.M)
        if not frames: return {}
        width, height, percent = frames[-1].split()[:3]
        return dict(width=width,height=height,percent=percent,**dict(re.findall(r'(\w+)=([^ ]+)', frames[-1])))
    def state(**values):
        return all(view().get(k) == str(v) for k, v in values.items())
    def wait(test, label):
        deadline = time.monotonic() + 14
        while not test():
            assert all(p.poll() is None for p in children), (label, [(p.pid,p.poll()) for p in children], log()[-2500:],log('server')[-1000:])
            assert time.monotonic() < deadline, (label, log()[-2500:])
            time.sleep(.01)
    def event(text):
        left.sendall(text.encode()); assert left.recv(32) == b'OK'
    def key(code, ctrl=False, shift=False):
        modifiers = ([29] if ctrl else []) + ([42] if shift else [])
        for codepoint in modifiers: event(f'K {codepoint} 1')
        event(f'K {code} 1'); event(f'K {code} 0')
        for codepoint in reversed(modifiers): event(f'K {codepoint} 0')
    codes = dict(zip('abcdefghijklmnopqrstuvwxyz', (30,48,46,32,18,33,34,35,23,36,37,38,50,49,24,25,16,19,31,20,22,47,17,45,21,44)))
    codes['.'] = 52
    def typing(text):
        for char in text: key(codes[char])
    def click(name):
        # Only the latest rendered control list; old dialog IDs are never reused.
        controls = log().rsplit('FRAME ', 2)[-2]
        matches = re.findall(r'^CONTROL (\d+) ([\d.]+) ([\d.]+) ([\d.]+) ([\d.]+) ' + re.escape(name) + r'$', controls, re.M)
        assert matches, (name, controls[-2500:])
        _, x, y, w, h = matches[-1]
        event(f'M {40+float(x)+float(w)/2} {40+float(y)+float(h)/2}')
        event('B 272 1'); event('B 272 0')
    def capture(label):
        time.sleep(.08); sequence = len(images)//3+1; server.send_signal(signal.SIGUSR1)
        wait(lambda: all(f'CAPTURE {sequence} {i}\n' in log('server') for i in range(3)), label)
        visible = False
        for i in range(3):
            path = captures / f'capture-{sequence}-output-{i}.ppm'; raw = path.read_bytes()
            magic, size, peak, pixels = raw.split(b'\n', 3); w, h = map(int, size.split())
            assert magic == b'P6' and peak == b'255' and len(pixels) == w*h*3
            visible |= len(set(pixels)) > 20
            packed = path.with_suffix('.ppm.gz'); packed.write_bytes(gzip.compress(raw, mtime=0))
            assert gzip.decompress(packed.read_bytes()) == raw; path.unlink()
            images.append(dict(case=label, output=i, file=packed.name, sha256=hashlib.sha256(raw).hexdigest()))
        # The compositor assigns successive windows to successive outputs.
        assert visible, label
    try:
        server = start(server_bin, 'server', '--headless', env['WAYLAND_DISPLAY'], captures, right.fileno(), pass_fds=(right.fileno(),)); right.close()
        wait(lambda: 'READY ' in log('server'), 'private compositor')
        app = start(app_bin, 'app', font, '--mono-font', mono, '--directory', folder, '--diagnostics')
        wait(lambda: state(docs=1, mode=0, cursor=0, density='1.00'), 'initial native editor')
        first = view()['active']; maps = Path(f'/proc/{app.pid}/maps').read_text()
        assert not any(name in maps for name in ('libSDL', 'libvulkan', 'libEGL', 'libX11'))
        loaded = {Path(line.split()[-1]).resolve() for line in maps.splitlines() if '/libnexaui.so' in line}; assert len(loaded) == 1
        library = loaded.pop(); metadata = json.loads((library.parents[2]/'build.json').read_text())
        pin = json.loads((ROOT/'spec/nexaui-inputs.json').read_text())
        assert metadata['revision'] == pin['revision'] and metadata['version'] == pin['version']
        assert digest(library) == metadata['artifacts'][library.relative_to(library.parents[2]).as_posix()]
        (root/'maps.txt').write_text(maps)
        typing('hello'); wait(lambda: state(cursor=5), 'all native characters delivered')
        key(28); typing('world'); wait(lambda: state(cursor=11), 'native newline and second line')
        capture('direct text and line numbers')
        key(31, ctrl=True); wait(lambda: state(mode=2, draft=0), 'first-save picker')
        typing('draft.txt'); wait(lambda: state(draft=9), 'native filename field')
        capture('first save with labelled filename')
        key(28); wait(lambda: state(mode=0) and (folder/'draft.txt').exists(), 'actual first save')
        assert (folder/'draft.txt').read_bytes() == b'hello\nworld'
        key(30, ctrl=True); wait(lambda: state(selected=1, begin=0, end=11), 'select original whole document')
        typing('changed'); wait(lambda: state(cursor=7, selected=0), 'direct replacement')
        # Selection replacement and subsequent adjacent typing are distinct
        # undo groups; verify both original transitions rather than guessing.
        key(44, ctrl=True); wait(lambda: state(cursor=1), 'undo adjacent typing')
        key(44, ctrl=True); wait(lambda: state(cursor=11), 'undo original replacement')
        key(21, ctrl=True); wait(lambda: state(cursor=1), 'redo original replacement')
        key(21, ctrl=True); wait(lambda: state(cursor=7), 'redo adjacent typing')
        key(31, ctrl=True); wait(lambda: (folder/'draft.txt').read_bytes() == b'changed', 'save actual edited bytes')
        key(49, ctrl=True); wait(lambda: state(docs=2, cursor=0), 'new original tab')
        second = view()['active']; assert second != first
        typing('unsaved'); wait(lambda: state(cursor=7), 'second tab input')
        key(15, ctrl=True); wait(lambda: state(active=first, cursor=7), 'switch preserves first document')
        key(15, ctrl=True); wait(lambda: state(active=second, cursor=7), 'switch restores second document')
        key(17, ctrl=True); wait(lambda: state(review=second), 'original dirty close review')
        capture('document-first unsaved close review')
        key(1); wait(lambda: state(review=0, active=second, docs=2), 'Escape cancels original close')
        assert (folder/'draft.txt').read_bytes() == b'changed'
        key(17, ctrl=True); wait(lambda: state(review=second), 'review reopened')
        click('Discard'); wait(lambda: state(docs=1, active=first, review=0), 'discard closes only second tab')
        assert (folder/'draft.txt').read_bytes() == b'changed'
        # Clean last tab closes to a real empty view, not process termination.
        key(17, ctrl=True); wait(lambda: state(docs=0, active=0), 'last document empty view')
        capture('empty editor window')
        key(49, ctrl=True); wait(lambda: state(docs=1), 'new document from empty view')
        typing('review'); wait(lambda: state(cursor=6), 'new document before Save close')
        key(17, ctrl=True); wait(lambda: int(view().get('review','0')) != 0, 'new document close review')
        click('Save'); wait(lambda: state(mode=2), 'Save review retains original destination')
        typing('closed.txt'); wait(lambda: state(draft=10), 'close-save destination')
        key(28); wait(lambda: state(mode=0,docs=0,review=0), 'successful first save completes original close')
        assert (folder/'closed.txt').read_bytes() == b'review'
        key(24, ctrl=True); wait(lambda: state(mode=1), 'open picker from empty view')
        click('seed.txt'); key(28); wait(lambda: state(mode=0, docs=1), 'open independently selected original file')
        key(107, ctrl=True); wait(lambda: state(cursor=9), 'end original opened file')
        typing('added'); wait(lambda: state(cursor=14), 'append original source')
        # External modification must not be silently replaced by save.
        (folder/'seed.txt').write_bytes(b'external')
        key(31, ctrl=True); wait(lambda: state(error=1), 'external-change conflict')
        assert (folder/'seed.txt').read_bytes() == b'external'
        capture('external change retains document and file')
        key(17, ctrl=True); wait(lambda: int(view().get('review', '0')) != 0, 'conflicting document review')
        click('Discard'); wait(lambda: state(docs=0, review=0), 'explicit discard after conflict')
        assert (folder/'seed.txt').read_bytes() == b'external'
        before = log().count('FRAME '); stat = Path(f'/proc/{app.pid}/stat')
        def ticks():
            fields = stat.read_text().rsplit(')',1)[1].split(); return int(fields[11])+int(fields[12])
        initial = ticks(); time.sleep(1.2); delta = ticks()-initial
        assert delta <= 2 and log().count('FRAME ') == before, (delta, before, log().count('FRAME '))
        key(16, ctrl=True); assert app.wait(timeout=8) == 0; children.remove(app)
        for name, flags in (('contrast-200', ('--german','--contrast','--text-percent','200')),
                            ('light-150', ('--light','--text-percent','150'))):
            current_name = name
            if name=='light-150':
                event('Z 2 200')
                wait(lambda:'OUTPUT_SCALE 2 2.00\n' in log('server'),'actual 4K output density committed')
            app = start(app_bin, name, font, '--mono-font', mono, '--directory', folder, '--diagnostics', *flags, 'draft.txt')
            wait(lambda: state(docs=1,mode=0,percent=200 if name=='contrast-200' else 150,
                               density='1.00' if name=='contrast-200' else '2.00'), name+' native window')
            canvas_focus = view()['focus']
            key(64); wait(lambda: view().get('focus') != canvas_focus, name+' F6 reaches toolbar')
            key(64); wait(lambda: view().get('focus') == canvas_focus, name+' F6 returns to document')
            key(30,ctrl=True); wait(lambda: state(selected=1,begin=0,end=7), name+' native selection')
            capture(name+' selected document')
            key(16,ctrl=True); assert app.wait(timeout=8)==0; children.remove(app)
        server.terminate(); assert server.wait(timeout=8) == 0; children.remove(server)
        for path in root.glob('*.log'):
            assert not re.search(r'AddressSanitizer|LeakSanitizer|runtime error:', path.read_text(errors='replace')), path
        result = dict(result='passed', scope='private native opaque Editor; no installed admission or hardware acceptance',
                      images=images, idle_cpu_ticks=delta, sdk=dict(version=metadata['version'], revision=metadata['revision'], sha256=digest(library)),
                      binaries={str(p):digest(p) for p in (server_bin,app_bin)}, font=dict(path=str(font),sha256=digest(font)),
                      remaining='native editable accessibility, external IME, asynchronous I/O, normal app/session admission')
        (root/'result.json').write_text(json.dumps(result,indent=2)+'\n')
        print(json.dumps(dict(result='passed',artifacts=str(root),images=len(images))))
    finally:
        # Only original Popen children of this disposable native test; this is
        # administrative test retirement, never production lifecycle settlement.
        for child in reversed(children):
            if child.poll() is None:
                child.terminate()
                try: child.wait(timeout=5)
                except subprocess.TimeoutExpired: child.kill(); child.wait(timeout=5)
        for stream in streams: stream.close()
        left.close(); right.close(); print('Editor native evidence:',root)


if __name__ == '__main__':
    main()
