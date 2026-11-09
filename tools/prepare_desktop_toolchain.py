"""Private native desktop build tools, read from signed RPMs as data.

This is not an installer or a target runtime. It deliberately retains the
carrier's GCC/binutils/glibc and does not replace its older libstdc++.
"""
import argparse
import contextlib
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import stat
import subprocess
import sys

from prepare_audio import sealed_package

ROOT = Path(__file__).resolve().parents[1]
PROFILE = ROOT / 'spec/desktop-toolchain-inputs.json'


def sha(raw):
    return hashlib.sha256(raw).hexdigest()


def canonical(name):
    if (not isinstance(name, str) or not name or '\\' in name or '\0' in name
            or str(PurePosixPath(name)) != name or PurePosixPath(name).is_absolute()
            or any(p in ('.', '..') for p in name.split('/'))):
        raise ValueError('noncanonical tool member')
    return name


def profile():
    raw = PROFILE.read_bytes()
    v = json.loads(raw)
    if (v.get('format') != 1 or v.get('purpose') != 'private-desktop-build-tools'
            or v.get('signer') != '36f612dcf27f7d1a48a835e4dbfcf71c6d9f90a6'):
        raise ValueError('different build tool profile')
    packages = {p['name'] for p in v['packages']}
    if len(packages) != 9 or len(v['packages']) != 9:
        raise ValueError('different tool package set')
    for p in v['packages']:
        if (not re.fullmatch(r'[A-Za-z0-9.+_-]+\.rpm', p['name'])
                or type(p['size']) is not int or not 0 < p['size'] <= 32*1024**2
                or not re.fullmatch('[0-9a-f]{64}', p['sha256'])):
            raise ValueError('invalid build RPM pin')
    seen = set()
    if not 1 <= len(v['files']) <= 4096:
        raise ValueError('tool inventory bound')
    for e in v['files']:
        canonical(e['path'])
        if (not e['path'].startswith('usr/') or e['path'] in seen
                or e['package'] not in packages or e['mode'] not in (0o644, 0o755)
                or type(e['size']) is not int or not 0 <= e['size'] <= 64*1024**2
                or not re.fullmatch('[0-9a-f]{64}', e['sha256'])):
            raise ValueError('invalid build tool member')
        seen.add(e['path'])
    if sum(e['size'] for e in v['files']) > 128*1024**2:
        raise ValueError('tool payload bound')
    for n, target in v['symlinks'].items():
        canonical(n)
        if n in seen or '/' in target or target in ('.', '..') or str(PurePosixPath(n).parent/target) not in seen:
            raise ValueError('invalid private tool alias')
    for e in v['notices']:
        canonical(e['path'])
        if (not e['path'].startswith('usr/share/licenses/') or e['path'] in seen
                or not re.fullmatch(r'[A-Za-z0-9._-]+', e['source'])
                or type(e['size']) is not int or not 0 < e['size'] <= 128*1024
                or not re.fullmatch('[0-9a-f]{64}', e['sha256'])):
            raise ValueError('invalid tool notice')
        seen.add(e['path'])
    return v, sha(raw)


def members(raw):
    """Bounded newc headers, including original hardlink identity fields.

    Only trusted pinned Fedora packages reach this parser. No archive pathname
    chooses an output, no archive link is followed, and nothing is extracted.
    """
    if len(raw) > 192*1024**2:
        raise ValueError('tool archive bound')
    at, seen = 0, set()
    for _ in range(16384):
        if at+110 > len(raw) or raw[at:at+6] != b'070701':
            raise ValueError('invalid tool newc header')
        fields = raw[at+6:at+110]
        if not re.fullmatch(b'[0-9a-fA-F]{104}', fields):
            raise ValueError('invalid tool newc fields')
        v = [int(fields[i:i+8], 16) for i in range(0, 104, 8)]
        size, namesize = v[6], v[11]
        at += 110
        if not 1 <= namesize <= 4096 or size > 64*1024**2 or at+namesize > len(raw):
            raise ValueError('tool member bound')
        name = raw[at:at+namesize]
        if name[-1:] != b'\0' or b'\0' in name[:-1]:
            raise ValueError('invalid tool member name')
        name = name[:-1].decode('utf-8', errors='strict')
        at = (at+namesize+3)&~3
        end = at+size
        if end > len(raw):
            raise ValueError('truncated tool body')
        body = memoryview(raw)[at:end]
        at = (end+3)&~3
        if name == 'TRAILER!!!':
            if size or any(raw[end:]):
                raise ValueError('data after tool archive')
            return
        if name.startswith('./'):
            name = name[2:]
        canonical(name)
        if name in seen:
            raise ValueError('duplicate tool member')
        seen.add(name)
        yield name, v, body
    raise ValueError('tool archive count bound')


def select(raw, expected, aliases):
    entries, groups = {}, {}
    for name, v, body in members(raw):
        entries[name] = (v, body)
        if stat.S_ISREG(v[1]) and v[4] > 1:
            groups.setdefault((v[7], v[8], v[0]), []).append((v, body))
    result = {}
    for name, e in expected.items():
        if name not in entries:
            raise ValueError('missing tool body: '+name)
        v, body = entries[name]
        if not stat.S_ISREG(v[1]) or stat.S_IMODE(v[1]) != e['mode']:
            raise ValueError('different tool member class/mode: '+name)
        if v[4] > 1:
            group = groups[(v[7], v[8], v[0])]
            if len(group) > v[4] or any(w[1:5] != v[1:5] for w, _ in group):
                raise ValueError('different hardlink header class')
            bodies = [b for _, b in group if len(b)]
            if len(bodies) > 1:
                raise ValueError('multiple hardlink bodies')
            body = bodies[0] if bodies else b''
        if len(body) != e['size'] or sha(body) != e['sha256']:
            raise ValueError('different tool body digest: '+name)
        result[name] = bytes(body)
    for name, target in aliases.items():
        if name not in entries:
            raise ValueError('missing tool alias')
        v, body = entries[name]
        if not stat.S_ISLNK(v[1]) or bytes(body) != target.encode():
            raise ValueError('different tool alias')
    return result


def native(v):
    rows = subprocess.check_output(['rpm', '-q', '--qf', '%{NAME} %{EVR}.%{ARCH}\n',
                                   *[n.split()[0] for n in v['native_packages']]], text=True).splitlines()
    if rows != v['native_packages']:
        raise ValueError('different native compiler/base packages')
    for n, e in v['preserved_native_files'].items():
        p = Path(n)
        if str(p.resolve()) != e['resolved'] or p.stat().st_size != e['size'] or sha(p.read_bytes()) != e['sha256']:
            raise ValueError('changed retained native tooling: '+n)


def wrappers(prefix):
    # Paths are intentionally explicit. User CXXFLAGS/SDK environment cannot
    # select a different standard library or accidental global C++ headers.
    root = str(prefix)
    common = f'export LD_LIBRARY_PATH="{root}/usr/lib64"\n'
    result = {n: '#!/bin/sh\n'+common+f'exec "{root}/usr/bin/{n}" "$@"\n'
              for n in ('cmake', 'ctest', 'ninja')}
    result['c++'] = '#!/bin/sh\n'+common+f'''exec "{root}/usr/bin/g++" -B"{root}/usr/libexec/gcc/x86_64-redhat-linux/16/" \\
  -B/usr/libexec/gcc/x86_64-redhat-linux/16/ -B/usr/lib/gcc/x86_64-redhat-linux/16/ -nostdinc++ \\
  -isystem "{root}/usr/include/c++/16" \\
  -isystem "{root}/usr/include/c++/16/x86_64-redhat-linux" \\
  -isystem "{root}/usr/include/c++/16/backward" \\
  -L"{root}/usr/lib64" "$@"
'''
    return {n: s.encode() for n, s in result.items()}


def check(prefix):
    v, digest = profile()
    prefix = Path(prefix).absolute()
    if prefix.resolve() != prefix:
        raise ValueError('aliased tool prefix')
    expected = {e['path']: e for e in [*v['files'], *v['notices']]}
    generated = {'bin/'+n: raw for n, raw in wrappers(prefix).items()}
    links = {**v['symlinks'], 'usr/lib64/libstdc++.so': 'libstdc++.so.6.0.36', 'bin/g++': 'c++'}
    seen = set()
    for p in prefix.rglob('*'):
        n = p.relative_to(prefix).as_posix()
        s = p.lstat()
        if stat.S_ISDIR(s.st_mode):
            continue
        seen.add(n)
        if n in links:
            if not stat.S_ISLNK(s.st_mode) or os.readlink(p) != links[n]:
                raise ValueError('changed tool alias')
        elif n == 'profile.sha256':
            if not stat.S_ISREG(s.st_mode) or p.read_text() != digest+'\n':
                raise ValueError('different tool selection')
        elif n in generated:
            if not stat.S_ISREG(s.st_mode) or s.st_nlink != 1 or stat.S_IMODE(s.st_mode) != 0o755 or p.read_bytes() != generated[n]:
                raise ValueError('changed tool wrapper')
        elif n in expected:
            e = expected[n]
            if (not stat.S_ISREG(s.st_mode) or s.st_nlink != 1 or s.st_size != e['size']
                    or stat.S_IMODE(s.st_mode) != e.get('mode', 0o644) or sha(p.read_bytes()) != e['sha256']):
                raise ValueError('changed tool file: '+n)
        else:
            raise ValueError('unlisted tool file: '+n)
    if seen != set(expected)|set(generated)|set(links)|{'profile.sha256'}:
        raise ValueError('incomplete tool prefix')
    native(v)
    return dict(profile_sha256=digest, files=len(seen), bytes=sum(e['size'] for e in expected.values()))


def prepare(cache, notices, output):
    if sys.platform != 'linux' or os.geteuid() == 0:
        raise ValueError('use an ordinary native build account')
    v, digest = profile()
    native(v)
    cache, notices, output = [Path(p).absolute() for p in (cache, notices, output)]
    if any(p.resolve() != p for p in (cache, notices, output)) or not re.fullmatch(r'/[A-Za-z0-9_./+-]+', str(output)):
        raise ValueError('aliased or unsupported tool path')
    payload, proofs = {}, []
    with contextlib.ExitStack() as stack:
        for package in v['packages']:
            fd = stack.enter_context(sealed_package(cache/package['name'], package))
            path = '/proc/self/fd/'+str(fd)
            def query(args):
                return subprocess.check_output(args+[path], pass_fds=(fd,), text=True)
            proof = query(['rpmkeys', '--checksig', '--verbose'])
            if v['signer']+': OK' not in proof or 'Payload SHA256 digest: OK' not in proof:
                raise ValueError('unverified tool RPM')
            metadata = query(['rpm', '-qp', '--qf', '%{NAME} %{EVR}.%{ARCH}\n%{LICENSE}\n%{SOURCERPM}\n'])
            if metadata.splitlines() != package['metadata'] or query(['rpm', '-qp', '--scripts']):
                raise ValueError('different tool origin/scripts')
            expected = {e['path']: e for e in v['files'] if e['package'] == package['name']}
            # Each explicit alias belongs to a regular payload in this package.
            aliases = {n: target for n, target in v['symlinks'].items()
                       if str(PurePosixPath(n).parent/target) in expected}
            raw = subprocess.check_output(['rpm2cpio', path], pass_fds=(fd,))
            selected = select(raw, expected, aliases)
            if len(selected) != package['selected_files'] or payload.keys() & selected.keys():
                raise ValueError('different tool inventory')
            payload.update(selected)
            proofs.append(dict(package=package['name'], metadata=metadata, signature=proof))
    for e in v['notices']:
        raw = (notices/e['source']).read_bytes()
        if len(raw) != e['size'] or sha(raw) != e['sha256']:
            raise ValueError('changed tool notice')
        payload[e['path']] = raw
    # All package/notice checks precede the first output effect. Unknown partial
    # creation is retained; a failed attempt is never adopted, repaired or removed.
    output.mkdir(mode=0o700)
    modes = {e['path']: e['mode'] for e in v['files']}
    for n, raw in payload.items():
        p = output/n
        p.parent.mkdir(parents=True, exist_ok=True)
        with p.open('xb') as f:
            f.write(raw)
        p.chmod(modes.get(n, 0o644))
    for n, raw in wrappers(output).items():
        p = output/'bin'/n
        p.parent.mkdir(exist_ok=True)
        with p.open('xb') as f:
            f.write(raw)
        p.chmod(0o755)
    for n, target in {**v['symlinks'], 'usr/lib64/libstdc++.so': 'libstdc++.so.6.0.36', 'bin/g++': 'c++'}.items():
        (output/n).symlink_to(target)
    with (output/'profile.sha256').open('x') as f:
        f.write(digest+'\n')
    return dict(check=check(output), signatures=proofs)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('action', choices=('prepare', 'check'))
    p.add_argument('prefix', type=Path)
    p.add_argument('--cache', type=Path)
    p.add_argument('--notices', type=Path)
    args = p.parse_args()
    if args.action == 'prepare':
        if args.cache is None or args.notices is None:
            p.error('prepare requires --cache and --notices')
        result = prepare(args.cache, args.notices, args.prefix)
    else:
        result = check(args.prefix)
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
