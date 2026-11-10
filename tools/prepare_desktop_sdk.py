"""Private original Wayland/XKB/FFI development inputs; no package installation.

Only known pkg-config path assignments change. Versions, requirements, headers,
protocols, libraries and scanner remain original signed-package bytes.
"""
import argparse
import contextlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import stat
import subprocess
import sys

from prepare_audio import sealed_package
from prepare_desktop_toolchain import canonical, select, sha

ROOT = Path(__file__).resolve().parents[1]
PROFILE = ROOT / 'spec/desktop-sdk-inputs.json'
PROFILES = {'common': (PROFILE, 'private-common-wayland-sdk'),
            'sdl': (ROOT / 'spec/sdl-sdk-inputs.json', 'private-sdl-build-sdk'),
            'wlroots': (ROOT / 'spec/wlroots-sdk-inputs.json', 'private-wlroots-package-sdk'),
            'apps': (ROOT / 'spec/app-sdk-inputs.json', 'private-core-app-sdk')}
LIMITS = {'private-common-wayland-sdk': (10, 512, 2*1024**2),
          'private-sdl-build-sdk': (5, 1024, 4*1024**2),
          'private-wlroots-package-sdk': (12, 512, 4*1024**2),
          'private-core-app-sdk': (8, 512, 4*1024**2)}
PATH_KEYS = {'prefix', 'exec_prefix', 'includedir', 'libdir', 'bindir', 'datarootdir'}


def rebase(raw, prefix):
    text = raw.decode('utf-8', errors='strict')
    if '\r' in text or '\0' in text:
        raise ValueError('noncanonical pkg-config text')
    result = []
    for line in text.splitlines(keepends=True):
        key, sep, value = line.rstrip('\n').partition('=')
        if sep and key in PATH_KEYS and (value == '/usr' or value.startswith('/usr/')):
            line = key+'='+(prefix/'usr').as_posix()+value[4:]+ ('\n' if line.endswith('\n') else '')
        result.append(line)
    return ''.join(result).encode()


def validate(v):
    if (v.get('format') != 1 or v.get('purpose') not in LIMITS
            or v.get('signer') != '36f612dcf27f7d1a48a835e4dbfcf71c6d9f90a6'):
        raise ValueError('different common SDK profile')
    packages = {p['name']: p for p in v['packages']}
    package_count, file_count, package_bytes = LIMITS[v['purpose']]
    member_bytes = 8*1024**2 if v['purpose'] == 'private-core-app-sdk' else 2*1024**2
    if len(packages) != package_count or len(v['packages']) != package_count:
        raise ValueError('different common SDK package set')
    for p in packages.values():
        if (not re.fullmatch(r'[A-Za-z0-9.+_-]+\.rpm', p['name'])
                or type(p['size']) is not int or not 0 < p['size'] <= package_bytes
                or not re.fullmatch('[0-9a-f]{64}', p['sha256'])):
            raise ValueError('invalid common SDK package')
    files = {}
    if not 1 <= len(v['files']) <= file_count:
        raise ValueError('common SDK inventory bound')
    for e in v['files']:
        n = canonical(e['path'])
        if (not n.startswith('usr/') or n in files or e['package'] not in packages
                or e['mode'] not in (0o644, 0o755) or type(e['size']) is not int
                or not 0 <= e['size'] <= member_bytes or not re.fullmatch('[0-9a-f]{64}', e['sha256'])):
            raise ValueError('invalid common SDK file')
        files[n] = e
    if sum(e['size'] for e in files.values()) > 16*1024**2:
        raise ValueError('common SDK payload bound')
    aliases = v['symlinks']
    if len(aliases) > 64 or set(aliases) != set(v['symlink_packages']):
        raise ValueError('different SDK alias ownership')
    for n, target in aliases.items():
        canonical(n)
        if (n in files or not n.startswith('usr/lib64/') or '/' in target
                or target in ('.', '..') or v['symlink_packages'][n] not in packages):
            raise ValueError('invalid common SDK alias')
        seen, next_name = {n}, str(PurePosixPath(n).parent/target)
        for _ in range(8):
            if next_name in files:
                break
            if next_name in seen or next_name not in aliases:
                raise ValueError('unresolved common SDK alias')
            seen.add(next_name)
            next_name = str(PurePosixPath(next_name).parent/aliases[next_name])
        else:
            raise ValueError('common SDK alias depth')
    pcs = v['pkgconfig']
    if set(pcs) != {n for n in files if n.endswith('.pc')}:
        raise ValueError('different pkg-config derivation inventory')
    for n, text in pcs.items():
        raw = text.encode()
        if len(raw) != files[n]['size'] or sha(raw) != files[n]['sha256']:
            raise ValueError('changed original pkg-config body')
    for e in v.get('source_notices', []):
        canonical(e['path'])
        if (v['purpose'] != 'private-sdl-build-sdk' or not e['path'].startswith('usr/share/aetheri-sources/')
                or e['path'] in files or e['path'] in aliases or e['mode'] != 0o644
                or not re.fullmatch(r'[A-Za-z0-9.+_-]+\.src\.rpm', e['name'])
                or type(e['size']) is not int or not 0 < e['size'] <= 4*1024**2
                or e['source_package'] != 1 or not re.fullmatch('[0-9a-f]{64}', e['sha256'])):
            raise ValueError('invalid source notice data')
        files[e['path']] = e
    return v


def profile(kind='common'):
    if kind not in PROFILES:
        raise ValueError('unsupported developer SDK profile')
    path, purpose = PROFILES[kind]
    raw = path.read_bytes()
    value = validate(json.loads(raw))
    if value['purpose'] != purpose:
        raise ValueError('different selected SDK purpose')
    return value, sha(raw)


def prefix_path(value):
    p = Path(value).absolute()
    if p.resolve() != p or not re.fullmatch(r'/[A-Za-z0-9_./+-]+', str(p)):
        raise ValueError('aliased or unsupported common SDK path')
    return p


def check(prefix, kind='common'):
    v, digest = profile(kind)
    prefix = prefix_path(prefix)
    expected = {e['path']: e for e in [*v['files'], *v.get('source_notices', [])]}
    seen = set()
    for p in prefix.rglob('*'):
        n = p.relative_to(prefix).as_posix()
        s = p.lstat()
        if stat.S_ISDIR(s.st_mode):
            continue
        seen.add(n)
        if n in v['symlinks']:
            if not stat.S_ISLNK(s.st_mode) or os.readlink(p) != v['symlinks'][n]:
                raise ValueError('changed common SDK alias')
        elif n == 'profile.sha256':
            if not stat.S_ISREG(s.st_mode) or s.st_nlink != 1 or p.read_bytes() != (digest+'\n').encode():
                raise ValueError('different common SDK selection')
        elif n in expected:
            e = expected[n]
            if not stat.S_ISREG(s.st_mode) or s.st_nlink != 1 or stat.S_IMODE(s.st_mode) != e['mode']:
                raise ValueError('changed common SDK file class')
            raw = p.read_bytes()
            if n in v['pkgconfig']:
                good = raw == rebase(v['pkgconfig'][n].encode(), prefix)
            else:
                good = len(raw) == e['size'] and sha(raw) == e['sha256']
            if not good:
                raise ValueError('changed common SDK body: '+n)
        else:
            raise ValueError('unlisted common SDK file: '+n)
    if seen != set(expected)|set(v['symlinks'])|{'profile.sha256'}:
        raise ValueError('incomplete common SDK')
    return dict(profile_sha256=digest, original_files=len(expected), aliases=len(v['symlinks']),
                pkgconfig_rebased=len(v['pkgconfig']), original_bytes=sum(e['size'] for e in expected.values()))


def prepare(cache, output, kind='common'):
    if sys.platform != 'linux' or os.geteuid() == 0:
        raise ValueError('use an ordinary native build account')
    v, digest = profile(kind)
    cache, output = prefix_path(cache), prefix_path(output)
    payload, proofs = {}, []
    with contextlib.ExitStack() as stack:
        for p in v['packages']:
            fd = stack.enter_context(sealed_package(cache/p['name'], p))
            path = '/proc/self/fd/'+str(fd)
            def query(args):
                return subprocess.check_output(args+[path], pass_fds=(fd,), text=True)
            proof = query(['rpmkeys', '--checksig', '--verbose'])
            if v['signer']+': OK' not in proof or 'Payload SHA256 digest: OK' not in proof:
                raise ValueError('unverified common SDK package')
            metadata = query(['rpm', '-qp', '--qf', '%{NAME} %{EVR}.%{ARCH}\n%{LICENSE}\n%{SOURCERPM}\n'])
            if metadata.splitlines() != p['metadata'] or query(['rpm', '-qp', '--scripts']):
                raise ValueError('different SDK origin/scripts')
            expected = {e['path']: e for e in v['files'] if e['package'] == p['name']}
            aliases = {n: t for n, t in v['symlinks'].items() if v['symlink_packages'][n] == p['name']}
            raw = subprocess.check_output(['rpm2cpio', path], pass_fds=(fd,))
            selected = select(raw, expected, aliases)
            if len(selected) != p['selected_files'] or payload.keys() & selected.keys():
                raise ValueError('different common SDK inventory')
            payload.update(selected)
            proofs.append(dict(package=p['name'], signature=proof, metadata=metadata))
        # The source RPM supplies the original per-file notices and Fedora
        # patches omitted by the binary packages. It remains opaque data.
        for p in v.get('source_notices', []):
            fd = stack.enter_context(sealed_package(cache/p['name'], p))
            path = '/proc/self/fd/'+str(fd)
            proof = subprocess.check_output(['rpmkeys', '--checksig', '--verbose', path], pass_fds=(fd,), text=True)
            metadata = subprocess.check_output(['rpm', '-qp', '--qf', '%{NAME} %{EVR}.%{ARCH}\n%{LICENSE}\n%{SOURCERPM}\n', path], pass_fds=(fd,), text=True)
            source = subprocess.check_output(['rpm', '-qp', '--qf', '%{SOURCEPACKAGE}', path], pass_fds=(fd,), text=True)
            if (v['signer']+': OK' not in proof or 'Payload SHA256 digest: OK' not in proof
                    or metadata.splitlines() != p['metadata'] or source != '1'):
                raise ValueError('unverified source notice data')
            payload[p['path']] = os.pread(fd, p['size']+1, 0)
            if len(payload[p['path']]) != p['size'] or sha(payload[p['path']]) != p['sha256']:
                raise ValueError('changed sealed source notice')
            proofs.append(dict(package=p['name'], source_notice_only=True, signature=proof, metadata=metadata))
    # Original bytes are checked before derivation or the first output effect.
    for n, text in v['pkgconfig'].items():
        if payload[n] != text.encode():
            raise ValueError('different original SDK paths')
        payload[n] = rebase(payload[n], output)
    output.mkdir(mode=0o700)
    modes = {e['path']: e['mode'] for e in [*v['files'], *v.get('source_notices', [])]}
    for n, raw in payload.items():
        p = output/n
        p.parent.mkdir(parents=True, exist_ok=True)
        with p.open('xb') as f:
            f.write(raw)
        p.chmod(modes[n])
    for n, target in v['symlinks'].items():
        (output/n).symlink_to(target)
    with (output/'profile.sha256').open('x') as f:
        f.write(digest+'\n')
    return dict(check=check(output, kind), signatures=proofs)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('action', choices=('prepare', 'check'))
    p.add_argument('prefix', type=Path)
    p.add_argument('--cache', type=Path)
    p.add_argument('--profile', choices=tuple(PROFILES), default='common')
    args = p.parse_args()
    if args.action == 'prepare':
        if args.cache is None:
            p.error('prepare requires --cache')
        result = prepare(args.cache, args.prefix, args.profile)
    else:
        result = check(args.prefix, args.profile)
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
