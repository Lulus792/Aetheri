"""Private Mesa/DRI runtime from signed RPM data; no package installation.

LLVM exceeds the smaller SDK reader's member bound. This separate streaming
reader keeps that bound unchanged and never materializes a decompressed archive.
Only independently selected paths in a fresh prefix receive original bytes.
"""
import argparse
import contextlib
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import posixpath
import re
import stat
import subprocess
import sys

from prepare_audio import sealed_package
from prepare_desktop_toolchain import canonical

ROOT = Path(__file__).resolve().parents[1]
PROFILE = ROOT/'spec/graphics-runtime-inputs.json'
CHUNK = 1024**2
ARCHIVE_LIMIT = 256*1024**2
MEMBER_LIMIT = 160*1024**2
TAIL_LIMIT = 10239  # rpm2cpio's observed 10-KiB output block, zero padding only.


def digest_file(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as f:
        while raw := f.read(CHUNK):
            h.update(raw)
    return h.hexdigest()


def scan(stream, sink=None):
    """Read bounded newc data. The callback receives only caller-selected sinks.

    Unselected bodies are hashed and discarded. Links are data, never followed.
    Partial RPM hardlink sets are resolved later against their original headers.
    """
    total, seen = 0, {}

    def take(n):
        nonlocal total
        if n < 0 or n > CHUNK or total+n > ARCHIVE_LIMIT:
            raise ValueError('graphics archive bound')
        parts = bytearray()
        while len(parts) < n:
            raw = stream.read(n-len(parts))
            if not raw:
                raise ValueError('truncated graphics archive')
            parts.extend(raw)
        total += n
        return bytes(parts)

    for _ in range(8192):
        head = take(110)
        if head[:6] != b'070701' or not re.fullmatch(b'[0-9a-fA-F]{104}', head[6:]):
            raise ValueError('invalid graphics newc header')
        v = [int(head[i:i+8], 16) for i in range(6, 110, 8)]
        size, namesize = v[6], v[11]
        if not 1 <= namesize <= 4096 or size > MEMBER_LIMIT:
            raise ValueError('graphics member bound')
        raw = take(namesize)
        if raw[-1:] != b'\0' or b'\0' in raw[:-1]:
            raise ValueError('invalid graphics member name')
        name = raw[:-1].decode('utf-8', errors='strict')
        take((-total) % 4)
        if name == 'TRAILER!!!':
            if size:
                raise ValueError('graphics trailer body')
            # rpm2cpio may pad its final block. Bound and reject nonzero tails.
            tail_size = 0
            while tail := stream.read(TAIL_LIMIT+1-tail_size):
                tail_size += len(tail)
                if tail_size > TAIL_LIMIT or total+tail_size > ARCHIVE_LIMIT or any(tail):
                    raise ValueError('data after graphics archive')
            return seen
        if name.startswith('./'):
            name = name[2:]
        canonical(name)
        if name in seen:
            raise ValueError('duplicate graphics member')
        if (not (stat.S_ISREG(v[1]) or stat.S_ISDIR(v[1]) or stat.S_ISLNK(v[1]))
                or not 1 <= v[4] <= 4096 or (stat.S_ISDIR(v[1]) and size)
                or (stat.S_ISLNK(v[1]) and size > 4096)):
            raise ValueError('unsupported graphics member class')
        write = sink(name, v) if sink is not None else None
        h, link, remaining = hashlib.sha256(), bytearray(), size
        while remaining:
            chunk = take(min(CHUNK, remaining))
            h.update(chunk)
            if stat.S_ISLNK(v[1]):
                link.extend(chunk)
            if write is not None:
                write(chunk)
            remaining -= len(chunk)
        seen[name] = dict(fields=v, sha256=h.hexdigest(), link=bytes(link).decode('utf-8', errors='strict'))
        take((-total) % 4)
    raise ValueError('graphics archive count bound')


def select(observed, expected, aliases):
    groups = {}
    for name, e in observed.items():
        v = e['fields']
        if stat.S_ISREG(v[1]) and v[4] > 1:
            groups.setdefault((v[7], v[8], v[0]), []).append(name)
    selected = {}
    for name, e in expected.items():
        if name not in observed:
            raise ValueError('missing graphics member: '+name)
        row = observed[name]
        v, source = row['fields'], name
        if not stat.S_ISREG(v[1]) or stat.S_IMODE(v[1]) != e['mode']:
            raise ValueError('different graphics member class/mode: '+name)
        if v[4] > 1:
            group = groups[(v[7], v[8], v[0])]
            if len(group) > v[4] or any(observed[n]['fields'][1:5] != v[1:5] for n in group):
                raise ValueError('different graphics hardlink header class')
            bodies = [n for n in group if observed[n]['fields'][6]]
            if len(bodies) > 1:
                raise ValueError('multiple graphics hardlink bodies')
            if bodies:
                source = bodies[0]
                row = observed[source]
        if row['fields'][6] != e['size'] or row['sha256'] != e['sha256']:
            raise ValueError('different graphics member bytes: '+name)
        selected.setdefault(source, []).append(name)
    for name, target in aliases.items():
        e = observed.get(name)
        if e is None or not stat.S_ISLNK(e['fields'][1]) or e['link'] != target:
            raise ValueError('different graphics alias: '+name)
    return selected


def validate(v):
    if (v.get('format') != 1 or v.get('purpose') != 'private-mesa-dri-runtime'
            or v.get('signer') != '36f612dcf27f7d1a48a835e4dbfcf71c6d9f90a6'):
        raise ValueError('different graphics runtime profile')
    packages = {p['name']: p for p in v['packages']}
    if len(packages) != len(v['packages']) or not 1 <= len(packages) <= 20:
        raise ValueError('different graphics package set')
    for p in [*v['packages'], *v['notices']]:
        if (not re.fullmatch(r'[A-Za-z0-9.+_-]+', p['name']) or type(p['size']) is not int
                or not 0 < p['size'] <= 80*1024**2 or not re.fullmatch('[0-9a-f]{64}', p['sha256'])):
            raise ValueError('invalid graphics input pin')
    files = {}
    if not 1 <= len(v['files']) <= 512:
        raise ValueError('graphics inventory bound')
    for e in [*v['files'], *v['notices']]:
        n = canonical(e['path'])
        if (not n.startswith('usr/') or n in files or e['mode'] not in (0o644, 0o755)
                or type(e['size']) is not int or not 0 <= e['size'] <= MEMBER_LIMIT
                or not re.fullmatch('[0-9a-f]{64}', e['sha256'])):
            raise ValueError('invalid graphics payload pin')
        files[n] = e
    if sum(e['size'] for e in files.values()) > 320*1024**2:
        raise ValueError('graphics payload bound')
    if not 1 <= len(v['notices']) <= 8 or len({e['name'] for e in v['notices']}) != len(v['notices']):
        raise ValueError('graphics notice inventory bound')
    if not 1 <= len(v['native_files']) <= 32:
        raise ValueError('graphics native prerequisite bound')
    for n, e in v['native_files'].items():
        if (not n.startswith('/usr/lib64/') or canonical(n[1:]) != n[1:]
                or type(e['size']) is not int or not 0 < e['size'] <= 4*1024**2
                or not re.fullmatch('[0-9a-f]{64}', e['sha256'])):
            raise ValueError('invalid native graphics prerequisite')
    if any(e['package'] not in packages for e in v['files']):
        raise ValueError('different graphics file origin')
    for e in v['notices']:
        if not e['path'].startswith('usr/share/licenses/') or e['mode'] != 0o644:
            raise ValueError('invalid graphics notice')
        if e.get('source_package') is not None and e['source_package'] != 1:
            raise ValueError('notice must stay source data')
    aliases = v['symlinks']
    if len(aliases) > 128 or set(aliases) != set(v['symlink_packages']):
        raise ValueError('different graphics alias ownership')
    for n, target in aliases.items():
        canonical(n)
        if (n in files or not n.startswith('usr/lib64/') or not isinstance(target, str)
                or not re.fullmatch(r'[A-Za-z0-9_./+-]+', target)
                or target.startswith('/') or v['symlink_packages'][n] not in packages):
            raise ValueError('invalid graphics alias')
        seen, current = {n}, n
        for _ in range(8):
            current = posixpath.normpath(str(PurePosixPath(current).parent/aliases[current]))
            if not current.startswith('usr/lib64/'):
                raise ValueError('escaping graphics alias')
            if current in files:
                break
            if current in seen or current not in aliases:
                raise ValueError('unresolved graphics alias')
            seen.add(current)
        else:
            raise ValueError('graphics alias depth')
    return v


def profile():
    raw = PROFILE.read_bytes()
    return validate(json.loads(raw)), hashlib.sha256(raw).hexdigest()


def prefix_path(value):
    p = Path(value).absolute()
    if p.resolve() != p or not re.fullmatch(r'/[A-Za-z0-9_./+-]+', str(p)):
        raise ValueError('aliased graphics prefix')
    return p


def check(prefix):
    v, digest = profile()
    prefix = prefix_path(prefix)
    expected = {e['path']: e for e in [*v['files'], *v['notices']]}
    seen = set()
    for p in prefix.rglob('*'):
        n = p.relative_to(prefix).as_posix()
        s = p.lstat()
        if stat.S_ISDIR(s.st_mode):
            continue
        seen.add(n)
        if n in v['symlinks']:
            if not stat.S_ISLNK(s.st_mode) or os.readlink(p) != v['symlinks'][n]:
                raise ValueError('changed graphics alias')
        elif n == 'profile.sha256':
            if not stat.S_ISREG(s.st_mode) or s.st_nlink != 1 or p.read_bytes() != (digest+'\n').encode():
                raise ValueError('different graphics selection')
        elif n in expected:
            e = expected[n]
            if (not stat.S_ISREG(s.st_mode) or s.st_nlink != 1 or stat.S_IMODE(s.st_mode) != e['mode']
                    or s.st_size != e['size'] or digest_file(p) != e['sha256']):
                raise ValueError('changed graphics body: '+n)
        else:
            raise ValueError('unexplained graphics object: '+n)
    if seen != set(expected)|set(v['symlinks'])|{'profile.sha256'}:
        raise ValueError('incomplete graphics runtime')
    return dict(profile_sha256=digest, original_files=len(expected), aliases=len(v['symlinks']),
                original_bytes=sum(e['size'] for e in expected.values()))


def archive(fd, sink=None):
    with subprocess.Popen(['rpm2cpio', '/proc/self/fd/'+str(fd)], pass_fds=(fd,),
                          stdout=subprocess.PIPE, stderr=subprocess.PIPE) as proc:
        try:
            result = scan(proc.stdout, sink)
        except BaseException:
            proc.kill()
            proc.communicate()
            raise
        error = proc.stderr.read(65537)
        if len(error) > 65536 or proc.wait() != 0:
            raise ValueError('graphics rpm2cpio failed: '+error[:4096].decode(errors='replace'))
    return result


def prepare(cache, output):
    if sys.platform != 'linux' or os.geteuid() == 0:
        raise ValueError('use an ordinary native build account')
    v, digest = profile()
    cache, output = prefix_path(cache), prefix_path(output)
    proofs, selected = [], []
    with contextlib.ExitStack() as stack:
        # Authenticate and inspect all sources before the first output effect.
        for p in [*v['packages'], *v['notices']]:
            if p.get('source_package') is None and p in v['notices']:
                fd = os.open(cache/p['name'], os.O_RDONLY | os.O_NOFOLLOW)
                stack.callback(os.close, fd)
                if os.fstat(fd).st_size != p['size'] or digest_file('/proc/self/fd/'+str(fd)) != p['sha256']:
                    raise ValueError('changed original notice')
                # Notice data is copied and rehashed below; never interpreted.
                selected.append((p, fd, None, None))
                continue
            fd = stack.enter_context(sealed_package(cache/p['name'], p))
            path = '/proc/self/fd/'+str(fd)
            def query(args):
                return subprocess.check_output(args+[path], pass_fds=(fd,), text=True)
            proof = query(['rpmkeys', '--checksig', '--verbose'])
            metadata = query(['rpm', '-qp', '--qf', '%{NAME} %{EVR}.%{ARCH}\n%{LICENSE}\n%{SOURCERPM}\n'])
            if (v['signer']+': OK' not in proof or 'Payload SHA256 digest: OK' not in proof
                    or metadata.splitlines() != p['metadata'] or query(['rpm', '-qp', '--scripts'])):
                raise ValueError('unverified graphics origin/scripts')
            proofs.append(dict(package=p['name'], metadata=metadata, signature=proof))
            if p.get('source_package') == 1:
                if query(['rpm', '-qp', '--qf', '%{SOURCEPACKAGE}']) != '1':
                    raise ValueError('notice is not source RPM data')
                selected.append((p, fd, None, None))
                continue
            expected = {e['path']: e for e in v['files'] if e['package'] == p['name']}
            aliases = {n: t for n, t in v['symlinks'].items() if v['symlink_packages'][n] == p['name']}
            observed = archive(fd)
            selected.append((p, fd, observed, select(observed, expected, aliases)))
        output.mkdir(mode=0o700)
        modes = {e['path']: e['mode'] for e in [*v['files'], *v['notices']]}
        for p, fd, observed, sources in selected:
            if observed is None:
                path = output/p['path']
                path.parent.mkdir(parents=True, exist_ok=True)
                h, offset = hashlib.sha256(), 0
                with path.open('xb') as f:
                    while offset < p['size']:
                        raw = os.pread(fd, min(CHUNK, p['size']-offset), offset)
                        if not raw:
                            raise ValueError('truncated graphics notice')
                        h.update(raw)
                        f.write(raw)
                        offset += len(raw)
                if h.hexdigest() != p['sha256'] or os.pread(fd, 1, offset):
                    raise ValueError('changed graphics notice copy')
                path.chmod(p['mode'])
                continue
            with contextlib.ExitStack() as outputs:
                def sink(name, fields):
                    if name not in sources:
                        return None
                    handles = []
                    for dest in sources[name]:
                        path = output/dest
                        path.parent.mkdir(parents=True, exist_ok=True)
                        handle = outputs.enter_context(path.open('xb'))
                        path.chmod(modes[dest])
                        handles.append(handle)
                    def write(raw):
                        for handle in handles:
                            handle.write(raw)
                    return write
                if archive(fd, sink) != observed:
                    raise ValueError('sealed graphics archive changed')
        for n, target in v['symlinks'].items():
            path = output/n
            path.parent.mkdir(parents=True, exist_ok=True)
            path.symlink_to(target)
        with (output/'profile.sha256').open('x') as f:
            f.write(digest+'\n')
    return dict(check=check(output), signatures=proofs)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('action', choices=('prepare', 'check'))
    p.add_argument('prefix', type=Path)
    p.add_argument('--cache', type=Path)
    args = p.parse_args()
    if args.action == 'prepare' and args.cache is None:
        p.error('prepare requires --cache')
    print(json.dumps(prepare(args.cache, args.prefix) if args.action == 'prepare' else check(args.prefix), indent=2))


if __name__ == '__main__':
    main()
