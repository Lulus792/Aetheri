"""Fresh current-desktop development runtime; never an installed OS or app API.

Independent profile hashes bind consumed source bytes. A protected check is a
caller prerequisite, not proof of fresh preparation, publisher or volume trust.
No existing output is adopted, repaired, cleaned or overwritten after failure.
"""
import argparse
from contextlib import ExitStack
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import stat
import sys

import desktop_payload
from prepare_settings_audio_lab import relative

ROOT = Path(__file__).resolve().parents[1]
PROFILE = ROOT/'spec/current-desktop-runtime-inputs.json'
PURPOSE = 'private-current-desktop-runtime'


def validate(value):
    if (not isinstance(value, dict) or type(value.get('format')) is not int or value['format'] != 1
            or value.get('purpose') != PURPOSE or value.get('variant') != 'release'):
        raise ValueError('different current desktop runtime profile')
    files, components = value.get('files'), value.get('components')
    if not isinstance(files, list) or not 1 <= len(files) <= 512 or not isinstance(components, dict) or not 1 <= len(components) <= 128:
        raise ValueError('current runtime inventory bound')
    converted, selected = [], {}
    keys = {'provider', 'input', 'destination', 'size', 'sha256', 'mode', 'component', 'role'}
    for e in files:
        if not isinstance(e, dict) or set(e) != keys:
            raise ValueError('invalid current runtime entry')
        if not isinstance(e['provider'], str) or not re.fullmatch('[a-z][a-z0-9_-]{0,47}', e['provider']):
            raise ValueError('invalid runtime provider')
        source = str(relative(e['input']))
        name = str(desktop_payload.destination(e['destination']))
        if (name in selected or not isinstance(e['component'], str) or e['component'] not in components
                or not isinstance(components[e['component']], dict)
                or e['role'] not in ('program', 'library', 'font', 'data', 'notice')):
            raise ValueError('duplicate or unattributed current runtime entry')
        if e['mode'] != (0o755 if e['role'] in ('program', 'library') else 0o644):
            raise ValueError('different current runtime mode')
        selected[name] = e
        converted.append(dict(source='/reviewed/'+e['provider']+'/'+source, destination=name,
                              size=e['size'], sha256=e['sha256'], mode=e['mode'], package=None,
                              provenance=components[e['component']].get('origin')))
    for name, c in components.items():
        if (not isinstance(name, str) or not re.fullmatch('[a-z0-9][a-z0-9_.-]{0,95}', name)
                or not isinstance(c, dict) or not isinstance(c.get('origin'), str) or not 1 <= len(c['origin']) <= 2048):
            raise ValueError('invalid runtime component origin')
        notices = c.get('notices')
        if not isinstance(notices, list) or not 1 <= len(notices) <= 32 or len(set(notices)) != len(notices):
            raise ValueError('missing or duplicate runtime notices')
        if not any(e['component'] == name and e['role'] != 'notice' for e in files):
            raise ValueError('unused current runtime component')
        if set(notices) != {n for n, e in selected.items() if e['component'] == name and e['role'] == 'notice'}:
            raise ValueError('foreign or incomplete component notices')
    desktop_payload.validate(dict(format=1, files=converted, symlinks=value.get('symlinks')))
    return value


def profile(path=PROFILE):
    with Path(path).open('rb') as f:
        raw = f.read(2*1024**2+1)
    if len(raw) > 2*1024**2:
        raise ValueError('current runtime profile byte bound')
    return validate(json.loads(raw))


def fingerprint(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, separators=(',', ':')).encode()).hexdigest()


def hash_fd(fd, expected):
    os.lseek(fd, 0, os.SEEK_SET)
    used, digest = 0, hashlib.sha256()
    while raw := os.read(fd, 1024**2):
        used += len(raw)
        if used > expected['size']:
            raise ValueError('runtime source grew beyond selected bytes')
        digest.update(raw)
    if used != expected['size'] or digest.hexdigest() != expected['sha256']:
        raise ValueError('different selected runtime bytes')


def check(tree, value):
    validate(value)
    tree = Path(tree).absolute()
    if tree.resolve() != tree or not stat.S_ISDIR(tree.lstat().st_mode):
        raise ValueError('aliased current runtime root')
    names = {'runtime.json', *(e['destination'].lstrip('/') for e in value['files']),
             *(n.lstrip('/') for n in value['symlinks'])}
    expected = names | {str(p) for n in names for p in PurePosixPath(n).parents if str(p) != '.'}
    if {p.relative_to(tree).as_posix() for p in tree.rglob('*')} != expected:
        raise ValueError('unexplained or missing current runtime entry')
    for n in expected - {n.lstrip('/') for n in value['symlinks']}:
        p = tree/n
        s = p.lstat()
        if not (stat.S_ISDIR(s.st_mode) or stat.S_ISREG(s.st_mode)) or p.resolve() != p:
            raise ValueError('aliased current runtime member')
    for e in value['files']:
        p = tree/e['destination'].lstrip('/')
        fd = os.open(p, os.O_RDONLY | getattr(os, 'O_CLOEXEC', 0) | getattr(os, 'O_NOFOLLOW', 0)
                     | getattr(os, 'O_NONBLOCK', 0) | getattr(os, 'O_BINARY', 0))
        try:
            s = os.fstat(fd)
            if (not stat.S_ISREG(s.st_mode) or s.st_nlink != 1 or s.st_size != e['size']
                    or os.name == 'posix' and stat.S_IMODE(s.st_mode) != e['mode']):
                raise ValueError('different current runtime object')
            hash_fd(fd, e)
        finally:
            os.close(fd)
    for n, target in value['symlinks'].items():
        p = tree/n.lstrip('/')
        if not p.is_symlink() or os.readlink(p) != target:
            raise ValueError('different current runtime alias')
    p = tree/'runtime.json'
    s = p.lstat()
    if not stat.S_ISREG(s.st_mode) or s.st_nlink != 1 or not 0 < s.st_size <= 4096:
        raise ValueError('invalid current runtime record')
    if json.loads(p.read_bytes()) != dict(format=1, purpose=PURPOSE, profile_sha256=fingerprint(value)):
        raise ValueError('different current runtime record')
    return dict(files=len(value['files']), aliases=len(value['symlinks']), components=len(value['components']),
                bytes=sum(e['size'] for e in value['files']))


def check_protected(tree, value):
    """Caller owes independently selected fresh Root-created copy; never chown/adopt."""
    if sys.platform != 'linux':
        raise ValueError('native protected runtime check required')
    result = check(tree, value)
    tree = Path(tree).absolute()
    for p in (tree, *tree.parents, *tree.rglob('*')):
        s = p.lstat()
        if s.st_uid or s.st_gid or not stat.S_ISLNK(s.st_mode) and s.st_mode & 0o022:
            raise ValueError('unprotected current runtime or parent')
    return result


def prepare(sources, output, value, protected=False):
    validate(value)
    if os.name == 'posix' and (os.geteuid() == 0) != protected:
        raise ValueError('ordinary preparation or explicit administrative protected copy required')
    if protected and sys.platform != 'linux':
        raise ValueError('native protected runtime preparation required')
    required = {e['provider'] for e in value['files']}
    if not isinstance(sources, dict) or set(sources) != required:
        raise ValueError('different selected runtime provider set')
    roots = {n: Path(p).absolute() for n, p in sources.items()}
    output = Path(output).absolute()
    if output.resolve() != output:
        raise ValueError('aliased runtime output')
    for root in roots.values():
        if (root.resolve() != root or not root.is_dir() or root == output
                or root in output.parents or output in root.parents):
            raise ValueError('aliased or overlapping runtime provider/output')
    if protected:
        for p in (output.parent, *output.parent.parents):
            s = p.lstat()
            if not stat.S_ISDIR(s.st_mode) or s.st_uid or s.st_gid or s.st_mode & 0o022:
                raise ValueError('unprotected current runtime parent')
    with ExitStack() as held:
        inputs = {}
        for e in value['files']:
            fd = os.open(roots[e['provider']]/e['input'], os.O_RDONLY | getattr(os, 'O_CLOEXEC', 0)
                         | getattr(os, 'O_NOFOLLOW', 0) | getattr(os, 'O_NONBLOCK', 0) | getattr(os, 'O_BINARY', 0))
            held.callback(os.close, fd)
            s = os.fstat(fd)
            if not stat.S_ISREG(s.st_mode) or s.st_nlink != 1 or s.st_size != e['size']:
                raise ValueError('different selected runtime source class/size')
            hash_fd(fd, e)
            inputs[e['destination']] = fd
        output.mkdir(mode=0o755 if protected else 0o700)
        for e in value['files']:
            path = desktop_payload.destination(e['destination'])
            desktop_payload.parents(output, path)
            target = output/str(path).lstrip('/')
            fd = inputs[e['destination']]
            os.lseek(fd, 0, os.SEEK_SET)
            used, digest = 0, hashlib.sha256()
            with target.open('xb') as f:
                while raw := os.read(fd, 1024**2):
                    used += len(raw)
                    if used > e['size']:
                        raise ValueError('changed runtime source; preserve partial copy')
                    digest.update(raw)
                    f.write(raw)
            if used != e['size'] or digest.hexdigest() != e['sha256']:
                raise ValueError('changed runtime source; preserve partial copy')
            target.chmod(e['mode'])
        for n, target in value['symlinks'].items():
            path = desktop_payload.destination(n)
            desktop_payload.parents(output, path)
            (output/n.lstrip('/')).symlink_to(target)
        with (output/'runtime.json').open('x') as f:
            json.dump(dict(format=1, purpose=PURPOSE, profile_sha256=fingerprint(value)), f)
        (output/'runtime.json').chmod(0o644)
        if protected:
            for p in (output, *output.rglob('*')):
                if stat.S_ISDIR(p.lstat().st_mode):
                    p.chmod(0o755)
    return check_protected(output, value) if protected else check(output, value)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('action', choices=('prepare', 'check'))
    p.add_argument('output', type=Path)
    p.add_argument('--sources', type=Path)
    p.add_argument('--profile', type=Path, default=PROFILE)
    p.add_argument('--protected', action='store_true')
    args = p.parse_args()
    value = profile(args.profile)
    if args.action == 'prepare':
        if args.sources is None:
            p.error('prepare requires an independently selected provider map')
        result = prepare(json.loads(args.sources.read_bytes()), args.output, value, args.protected)
    else:
        if args.sources is not None:
            p.error('check does not take provider sources')
        result = check_protected(args.output, value) if args.protected else check(args.output, value)
    print(json.dumps(result))


if __name__ == '__main__':
    main()
