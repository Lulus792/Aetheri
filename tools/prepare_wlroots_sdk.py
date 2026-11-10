"""Join original package headers/libraries to the tested libinput 1.32 build.

The package half uses the existing bounded SDK reader. The input half keeps its
genuine installed header and generated metadata, rather than relabelling an RPM.
All outputs are ordinary developer files, never application/volume authority.
"""
import argparse
import json
import os
from pathlib import Path
import stat
import sys

import build_libinput
import prepare_desktop_sdk as sdk
import prepare_libinput_runtime as runtime
from prepare_desktop_toolchain import canonical, sha


def profile():
    v, digest = sdk.profile('wlroots')
    built = v['input_build']
    if (built['version'] != '1.32.0' or built['source'] != json.loads(build_libinput.PROFILE.read_bytes())['source']
            or built['release_export_sha256'] != runtime.profile()['release_export_sha256']
            or built['symlinks'] != {'usr/lib64/libinput.so': 'libinput.so.10',
                                    'usr/lib64/libinput.so.10': 'libinput.so.10.13.0'}):
        raise ValueError('different libinput SDK build')
    names = {e['path'] for e in built['files']}
    if (names != {'usr/include/libinput.h', 'usr/lib64/pkgconfig/libinput.pc'}
            or len(built['files']) != 2 or set(built['runtime_files']) !=
            {'usr/lib64/libinput.so.10.13.0', 'usr/lib64/liblua-5.5.so',
             'usr/share/licenses/libinput/COPYING', 'usr/share/licenses/lua/COPYRIGHT'}):
        raise ValueError('different input SDK inventory')
    for e in [*built['files'], built['record']]:
        canonical(e['path'])
        canonical(e['source_path'])
        if (type(e['size']) is not int or not 0 < e['size'] <= 1024**2
                or len(e['sha256']) != 64 or any(c not in '0123456789abcdef' for c in e['sha256'])):
            raise ValueError('invalid built input pin')
    if (built['record']['path'] != 'build.json' or built['record']['source_path'] != 'inputs.json'
            or {e['path']: e['source_path'] for e in built['files']} !=
            {'usr/include/libinput.h': 'stage/usr/include/libinput.h',
             'usr/lib64/pkgconfig/libinput.pc': 'stage/usr/lib64/pkgconfig/libinput.pc'}):
        raise ValueError('different original build member')
    pc = next(e for e in built['files'] if e['path'].endswith('.pc'))
    if sha(built['pkgconfig'].encode()) != pc['sha256'] or len(built['pkgconfig'].encode()) != pc['size']:
        raise ValueError('changed original input pkg-config')
    notice = v['upstream_notice']
    if (notice['name'] != 'libdrm-2.4.134.tar.xz' or notice['path'] != 'notices/libdrm-2.4.134.tar.xz'
            or not 0 < notice['size'] <= 1024**2 or len(notice['sha256']) != 64):
        raise ValueError('different libdrm notice')
    original = json.loads((sdk.ROOT/'spec/settings-audio-lab-inputs.json').read_bytes())['upstream_notice_sources'][notice['name']]
    if any(notice[k] != original[k] for k in ('url', 'size', 'sha256')):
        raise ValueError('different reviewed libdrm source')
    return v, digest


def admit_record(record, built, library):
    # This exact historical Release record predates build_type/sanitizers keys.
    # Preserve it and inspect its original primary Meson command instead.
    primary = [c for c in record['commands'] if c[:2] == ['meson', 'setup']
               and c[2].endswith('/candidate/build')]
    if (record['profile_sha256'] != built['build_profile_sha256'] or len(primary) != 1
            or '--buildtype=release' not in primary[0] or any('sanitize' in s for s in primary[0])
            or record.get('build_type', 'release') != 'release'
            or record.get('sanitizers', False) is not False
            or any(record['artifact'][k] != library[k] for k in ('size', 'sha256'))):
        raise ValueError('different tested libinput Release record')


def read(path, e):
    return runtime.regular(path, e)


def input_entries(v, prefix):
    built = v['input_build']
    entries = {e['path']: dict(e, mode=0o644) for e in [*built['files'], built['record']]}
    known = {e['path']: e for e in runtime.profile()['files']}
    entries.update({n: known[n] for n in built['runtime_files']})
    pc = 'usr/lib64/pkgconfig/libinput.pc'
    raw = sdk.rebase(built['pkgconfig'].encode(), prefix/'input')
    entries[pc] = dict(entries[pc], size=len(raw), sha256=sha(raw))
    return entries


def check(prefix):
    v, digest = profile()
    prefix = sdk.prefix_path(prefix)
    packages = sdk.check(prefix/'rpm', 'wlroots')
    entries = {'input/'+n: e for n, e in input_entries(v, prefix).items()}
    entries[v['upstream_notice']['path']] = dict(v['upstream_notice'], mode=0o644)
    aliases = {'input/'+n: t for n, t in v['input_build']['symlinks'].items()}
    seen = set()
    for p in prefix.rglob('*'):
        n = p.relative_to(prefix).as_posix()
        s = p.lstat()
        if stat.S_ISDIR(s.st_mode) or n.startswith('rpm/'):
            continue
        seen.add(n)
        if n in aliases:
            if not stat.S_ISLNK(s.st_mode) or os.readlink(p) != aliases[n]:
                raise ValueError('changed built input alias')
        elif n == 'profile.sha256':
            if not stat.S_ISREG(s.st_mode) or s.st_nlink != 1 or p.read_bytes() != (digest+'\n').encode():
                raise ValueError('different joined SDK selection')
        elif n in entries:
            read(p, entries[n])
            if stat.S_IMODE(s.st_mode) != entries[n]['mode']:
                raise ValueError('different joined SDK mode')
        else:
            raise ValueError('unexplained joined SDK file')
    if seen != set(entries)|set(aliases)|{'profile.sha256'}:
        raise ValueError('incomplete joined SDK')
    return dict(packages=packages, original_input_files=len(entries)-1,
                input_aliases=len(aliases), libinput_version='1.32.0', profile_sha256=digest)


def prepare(cache, input_build, input_runtime, output):
    if sys.platform != 'linux' or os.getuid() == 0:
        raise ValueError('ordinary native builder required')
    v, digest = profile()
    cache, input_build, input_runtime, output = [sdk.prefix_path(p) for p in (cache, input_build, input_runtime, output)]
    if any(output == p or output in p.parents or p in output.parents for p in (cache, input_build, input_runtime)):
        raise ValueError('overlapping SDK source/output')
    runtime.check(input_runtime)
    built = v['input_build']
    originals = {e['path']: read(input_build/e['source_path'], e) for e in [*built['files'], built['record']]}
    record = json.loads(originals['build.json'])
    library = next(e for e in runtime.profile()['files'] if e['path'] == 'usr/lib64/libinput.so.10.13.0')
    admit_record(record, built, library)
    header = next(e for e in built['files'] if e['path'].endswith('.h'))
    pc = next(e for e in built['files'] if e['path'].endswith('.pc'))
    if (read(input_build/'source/src/libinput.h', header) != originals[header['path']]
            or read(input_build/'build/meson-private/libinput.pc', pc) != originals[pc['path']]):
        raise ValueError('installed input header/metadata differ from original build')
    # The original runtime supplies the SONAME alias. The additional fixed .so
    # development link resolves the original generated PC's -linput requirement.
    if os.readlink(input_runtime/'usr/lib64/libinput.so.10') != 'libinput.so.10.13.0':
        raise ValueError('different original input runtime alias')
    known = {e['path']: e for e in runtime.profile()['files']}
    originals.update({n: read(input_runtime/n, known[n]) for n in built['runtime_files']})
    notice = v['upstream_notice']
    notice_raw = read(cache/notice['name'], notice)
    # The outer fresh directory records a possible partial attempt on failure.
    # No failed prefix is retried, repaired or cleaned up automatically.
    output.mkdir(mode=0o700)
    package_proof = sdk.prepare(cache, output/'rpm', 'wlroots')
    entries = input_entries(v, output)
    originals['usr/lib64/pkgconfig/libinput.pc'] = sdk.rebase(originals['usr/lib64/pkgconfig/libinput.pc'], output/'input')
    for n, raw in originals.items():
        p = output/'input'/n
        p.parent.mkdir(parents=True, exist_ok=True)
        with p.open('xb') as f:
            f.write(raw)
        p.chmod(entries[n]['mode'])
    for n, target in built['symlinks'].items():
        (output/'input'/n).symlink_to(target)
    p = output/notice['path']
    p.parent.mkdir(parents=True)
    with p.open('xb') as f:
        f.write(notice_raw)
    p.chmod(0o644)
    with (output/'profile.sha256').open('x') as f:
        f.write(digest+'\n')
    return dict(check=check(output), signatures=package_proof['signatures'])


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('action', choices=('prepare', 'check'))
    p.add_argument('prefix', type=Path)
    p.add_argument('--cache', type=Path)
    p.add_argument('--input-build', type=Path)
    p.add_argument('--input-runtime', type=Path)
    args = p.parse_args()
    if args.action == 'prepare' and any(n is None for n in (args.cache, args.input_build, args.input_runtime)):
        p.error('prepare requires cache, input-build and input-runtime')
    print(json.dumps(prepare(args.cache, args.input_build, args.input_runtime, args.prefix)
                     if args.action == 'prepare' else check(args.prefix), indent=2))


if __name__ == '__main__':
    main()
