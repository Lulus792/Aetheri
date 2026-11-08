"""Real private PipeWire graph -> FIFO -> independent PCM comparison.

No ALSA, speaker, microphone, network module, normal service or default session
is opened. The signed pipe-tunnel and pw-link tools are fixture-only. This does
not prove a physical device, policy broker, speaker drain or production SDK.
"""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import select
import signal
import socket
import stat
import struct
import subprocess
import tempfile
import time

import prepare_audio


def validate_pcm(raw, frames, silent=False):
    if len(raw) % 8 or len(raw) > 8 * 1024**2:
        raise AssertionError("invalid/budgeted captured frames")
    samples = list(struct.iter_unpack('<ff', raw))
    if not samples or any(not math.isfinite(v) for frame in samples for v in frame):
        raise AssertionError("empty/nonfinite capture")
    if silent:
        assert all(v == 0.0 for frame in samples for v in frame), 'silent control contains signal'
        return {"captured_frames": len(samples), "silent": True}
    first = next((i for i, frame in enumerate(samples) if max(map(abs, frame)) > 1e-6), None)
    assert first is not None and first > 0, 'no phase-zero signal start'
    start = first - 1
    assert len(samples) >= start + frames, ('truncated signal', len(samples), start, frames)
    assert all(abs(v) < 1e-6 for frame in samples[:start] for v in frame), 'nonzero prefix'
    peak_error = 0.0
    for i in range(frames):
        for channel, frequency in enumerate((440, 880)):
            expected = 0.125 * math.sin(2 * math.pi * frequency * i / 48000)
            peak_error = max(peak_error, abs(samples[start + i][channel] - expected))
    assert peak_error < 4e-6, ('wrong/missing/reordered samples', peak_error)
    assert all(abs(v) < 1e-6 for frame in samples[start + frames:] for v in frame), 'unexpected signal suffix'
    return {"captured_frames": len(samples), "signal_start": start, "signal_frames": frames, "max_error": peak_error}


def run_case(binary, prefix, root, frames, silent=False, failure=None):
    root.mkdir(mode=0o700)
    runtime, conf, home = [root / n for n in ('run', 'conf', 'home')]
    for p in (runtime, conf, home):
        p.mkdir(mode=0o700)
    remote = runtime / 'aetheri-audio-test'
    if len(os.fsencode(remote)) >= 108:
        raise ValueError('audio socket path bound')
    fifo = root / 'capture.fifo'
    os.mkfifo(fifo, 0o600)
    reader = os.open(fifo, os.O_RDONLY | os.O_NONBLOCK | os.O_CLOEXEC)
    fifo_identity = os.fstat(reader)
    libs = prefix / 'usr/lib64'
    env = {'PATH': '/usr/bin:/bin', 'HOME': str(home), 'XDG_RUNTIME_DIR': str(runtime),
           'XDG_CONFIG_HOME': str(home), 'PIPEWIRE_CONFIG_DIR': str(conf),
           'PIPEWIRE_MODULE_DIR': str(libs / 'pipewire-0.3'), 'SPA_PLUGIN_DIR': str(libs / 'spa-0.2'),
           'LD_LIBRARY_PATH': str(libs), 'PIPEWIRE_REMOTE': str(remote),
           'ASAN_OPTIONS': 'detect_leaks=1:halt_on_error=1',
           'UBSAN_OPTIONS': 'halt_on_error=1:print_stacktrace=1'}
    common = '''context.spa-libs = {
 support.* = support/libspa-support
 audio.convert.* = audioconvert/libspa-audioconvert
}
'''
    (conf / 'client.conf').write_text(common + '''context.properties = { mem.allow-mlock = false support.dbus = false }
context.modules = [
 { name = libpipewire-module-protocol-native }
 { name = libpipewire-module-client-node }
 { name = libpipewire-module-adapter }
]
''')
    (conf / 'server.conf').write_text(common + '''context.properties = {
 core.daemon = true
 core.name = aetheri-audio-test
 default.clock.rate = 48000
 default.clock.quantum = 1024
 default.clock.min-quantum = 1024
 default.clock.max-quantum = 1024
 mem.allow-mlock = false
 support.dbus = false
}
context.modules = [
 { name = libpipewire-module-protocol-native args = { sockets = [ { name = aetheri-audio-test mode = 0600 } ] } }
 { name = libpipewire-module-client-node }
 { name = libpipewire-module-adapter }
 { name = libpipewire-module-link-factory }
 { name = libpipewire-module-spa-node-factory }
 { name = libpipewire-module-access args = { access.legacy = false access.socket = { aetheri-audio-test = unrestricted } } }
 { name = libpipewire-module-pipe-tunnel args = {
   tunnel.mode = sink
   pipe.filename = ''' + json.dumps(str(fifo)) + '''
   audio.format = F32LE
   audio.rate = 48000
   audio.channels = 2
   audio.position = [ FL FR ]
   stream.props = { node.name = aetheri.test.sink adapter.auto-port-config = { mode = dsp position = preserve } }
 } }
]
context.objects = [ { factory = spa-node-factory args = {
 factory.name = support.node.driver node.name = Test-Driver priority.driver = 200000
} } ]
''')
    children, streams, captured = [], [], bytearray()

    def start(name, args):
        stream = (root / (name + '.log')).open('xb');streams.append(stream)
        p = subprocess.Popen(['setpriv', '--no-new-privs', *map(str, args)], cwd=root, env=env,
                             stdin=subprocess.DEVNULL, stdout=stream, stderr=subprocess.STDOUT)
        pin = os.pidfd_open(p.pid);children.append((p, pin))
        return p

    def pump():
        for _ in range(4):
            try:
                raw = os.read(reader, 65536)
            except BlockingIOError:
                break
            if not raw:
                break
            captured.extend(raw)
            assert len(captured) <= 8 * 1024**2, 'capture budget'

    def wait(predicate, label, seconds=12):
        deadline = time.monotonic() + seconds
        while not predicate():
            pump()
            assert all(p.poll() is None for p, _ in children), (label, 'early child exit')
            assert time.monotonic() < deadline, label
            time.sleep(.005)

    def command(name, args):
        p = subprocess.run([str(prefix / 'usr/bin' / args[0]), '--remote', str(remote), *args[1:]],
                           cwd=root, env=env, capture_output=True, timeout=4)
        (root / (name + '.log')).write_bytes(p.stdout + p.stderr)
        assert p.returncode == 0, (name, p.returncode, p.stderr)
        return p.stdout

    def graph():
        raw = command('graph', ['pw-dump'])
        (root / 'graph.json').write_bytes(raw)
        return json.loads(raw)

    try:
        server = start('server', [prefix / 'usr/bin/pipewire', '-c', 'server.conf'])
        wait(lambda: remote.exists(), 'private server socket')
        s = remote.lstat();assert stat.S_ISSOCK(s.st_mode) and s.st_uid == os.getuid() and stat.S_IMODE(s.st_mode) == 0o600
        # The private path is fixture routing, not process/production admission.
        client = start('client', [binary, '--remote', remote, '--frames', frames, '--wait-for-start', *(['--silent'] if silent else [])])
        wait(lambda: b'CONNECTED node=' in (root / 'client.log').read_bytes(), 'original client connected')
        actors = {}; private_libraries = set()
        expected_libraries = {str(prefix/e['path']) for e in json.loads(prepare_audio.PROFILE.read_text())['files'] if e['role']=='runtime' and '/lib64/' in e['path']}
        for name, actor, image in (('server',server,prefix/'usr/bin/pipewire'),('client',client,binary)):
            pin = next(pin for p, pin in children if p is actor)
            assert not select.select([pin], [], [], 0)[0]
            proc = Path('/proc') / str(actor.pid)
            assert (proc/'exe').resolve() == image.resolve()
            status = dict(line.split(':',1) for line in (proc/'status').read_text().splitlines() if ':' in line)
            assert set(status['Uid'].split()) == {str(os.getuid())} and set(status['Gid'].split()) == {str(os.getgid())}
            assert status['NoNewPrivs'].strip() == '1' and int(status['CapEff'].strip(),16) == 0
            maps = (proc/'maps').read_text()
            mapped = {line.split()[-1] for line in maps.splitlines() if 'libpipewire-0.3.so.' in line}
            assert mapped == {str(prefix/'usr/lib64/libpipewire-0.3.so.0.1611.0')}, mapped
            private = {line.split()[-1] for line in maps.splitlines() if str(prefix/'usr/lib64')+'/' in line}
            private_libraries |= private
            fds = []
            for path in (proc/'fd').iterdir():
                try: fds.append(os.readlink(path))
                except FileNotFoundError: pass
            assert not any(p.startswith(('/dev/snd/','/dev/video')) for p in fds), fds
            sockets = {p[8:-1] for p in fds if p.startswith('socket:[')}
            unix = {line.split()[6] for line in (proc/'net/unix').read_text().splitlines()[1:]}
            assert sockets <= unix, 'non-Unix socket in private audio process'
            assert not select.select([pin], [], [], 0)[0]
            actors[name] = {'pid': actor.pid, 'image': str(image), 'uid': os.getuid(), 'gid': os.getgid(),
                            'no_new_privileges': True, 'effective_capabilities': 0, 'pipewire_library': sorted(mapped), 'private_libraries': sorted(private),
                            'no_audio_video_device_fd_observed': True, 'unix_sockets_only': True}
        (root/'actors.json').write_text(json.dumps(actors,indent=2)+'\n')
        assert expected_libraries <= private_libraries, ('unused selected runtime', sorted(expected_libraries-private_libraries))
        if failure:
            if failure == 'server-loss':
                pin = next(pin for p, pin in children if p is server)
                signal.pidfd_send_signal(pin, signal.SIGTERM)
                assert server.wait(timeout=5) == 0
            elif failure == 'interrupt':
                pin = next(pin for p, pin in children if p is client)
                signal.pidfd_send_signal(pin, signal.SIGINT)
            elif failure != 'deadline':
                raise ValueError('unknown audio failure fixture')
            assert client.wait(timeout=18 if failure == 'deadline' else 5) == 1
            log = (root / 'client.log').read_text()
            assert 'DRAINED ' not in log
            reason = {'server-loss': 'server/protocol error', 'interrupt': 'interrupted', 'deadline': 'deadline'}[failure]
            assert reason in log, log
            pump();assert not any(captured), 'signal before activation'
            result = {'passed': True, 'negative': failure, 'client_exit': 1}
            (root / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
            return result
        ports = {}

        def linked_ports():
            nonlocal ports
            objects = graph()
            nodes = {o['id']: o.get('info', {}).get('props', {}).get('node.name') for o in objects if o['type'] == 'PipeWire:Interface:Node'}
            assert set(nodes.values()) <= {'aetheri.audio.check', 'aetheri.test.sink', 'Test-Driver'}, nodes
            ports = {}
            for o in objects:
                if o['type'] != 'PipeWire:Interface:Port':
                    continue
                p = o.get('info', {}).get('props', {})
                role = nodes.get(int(p.get('node.id', -1)))
                channel = p.get('audio.channel')
                if role in ('aetheri.audio.check', 'aetheri.test.sink') and channel in ('FL', 'FR'):
                    direction = 'out' if role == 'aetheri.audio.check' else 'in'
                    if p.get('port.direction') == direction:
                        key = (role, channel);assert key not in ports;ports[key] = o['id']
            return len(ports) == 4

        wait(linked_ports, 'exact original stereo ports')
        for channel in ('FL', 'FR'):
            command('link-' + channel, ['pw-link', str(ports[('aetheri.audio.check', channel)]), str(ports[('aetheri.test.sink', channel)])])
        # Linking one channel must not consume the other channel's early frames.
        # Keep the original producer inactive until both links exist.
        for _ in range(20):
            pump();time.sleep(.005)
        assert not any(captured), 'audio before explicit start'
        assert client.poll() is None and b'DRAINED ' not in (root / 'client.log').read_bytes()
        client_pin = next(pin for p, pin in children if p is client)
        signal.pidfd_send_signal(client_pin, signal.SIGUSR1)
        deadline = time.monotonic() + 18
        while client.poll() is None:
            pump();assert server.poll() is None and time.monotonic() < deadline
            time.sleep(.003)
        assert client.returncode == 0, (root / 'client.log').read_text()
        assert f'DRAINED frames={frames}\n' in (root / 'client.log').read_text()
        for _ in range(30):
            pump();time.sleep(.005)
        # Stop only this retained private server, then consume its remaining FIFO.
        pin = next(pin for p, pin in children if p is server)
        signal.pidfd_send_signal(pin, signal.SIGTERM)
        assert server.wait(timeout=5) == 0
        pump()
        st = fifo.lstat();assert (st.st_dev, st.st_ino) == (fifo_identity.st_dev, fifo_identity.st_ino)
        result = validate_pcm(captured, frames, silent)
        result.update(passed=True, requested_frames=frames, capture_sha256=hashlib.sha256(captured).hexdigest())
        (root / 'capture.f32le').write_bytes(captured)
        (root / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
        return result
    finally:
        for p, pin in reversed(children):
            if p.poll() is None:
                signal.pidfd_send_signal(pin, signal.SIGTERM)
                try:
                    p.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    signal.pidfd_send_signal(pin, signal.SIGKILL);p.wait(timeout=5)
            assert select.select([pin], [], [], 0)[0], 'original child remains live'
            os.close(pin)
        for stream in streams:
            stream.close()
        os.close(reader)
        if not (root / 'capture.f32le').exists():
            (root / 'capture.f32le').write_bytes(captured)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('binary', type=Path)
    parser.add_argument('prefix', type=Path)
    parser.add_argument('work', type=Path)
    args = parser.parse_args()
    assert os.getuid() != 0
    prepare_audio.check(args.prefix)
    root = Path(tempfile.mkdtemp(prefix='aud-', dir=args.work))
    print('Audio evidence:', root, flush=True)
    results = []
    for name, frames, silent, failure in (('tone',48000,False,None), ('partial',257,False,None),
            ('maximum',96000,False,None), ('silent',257,True,None),
            ('server-loss',48000,False,'server-loss'), ('interrupt',48000,False,'interrupt'),
            ('deadline',48000,False,'deadline')):
        results.append(run_case(args.binary.resolve(), args.prefix.resolve(), root / name, frames, silent, failure))
    prepare_audio.check(args.prefix)
    (root / 'result.json').write_text(json.dumps({'passed': True, 'cases': results}, indent=2) + '\n')
    print('Private PCM playback verified', flush=True)


if __name__ == '__main__':
    main()
