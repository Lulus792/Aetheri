"""Build current desktop/apps against independently pinned private SDKs.

No image writer, package installation, privileged test, service provisioning or
normal app admission. Fresh ordinary Linux development directories only.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import posixpath
import re
import stat
import subprocess
import sys

import prepare_audio
import prepare_audio_control
import prepare_audio_policy
import prepare_desktop_sdk as sdk
import prepare_desktop_toolchain as tools
import prepare_graphics_runtime as graphics
import prepare_settings_audio_lab as gui
import prepare_wlroots_sdk as session
import embed_compose
from prepare_desktop_toolchain import canonical

ROOT = Path(__file__).resolve().parents[1]
PROFILE = ROOT/'spec/app-build-inputs.json'
APPS = ('aetheri-compositor', 'aetheri-desktop', 'aetheri-settings-nexaui',
        'aetheri-files-nexaui', 'aetheri-terminal-nexaui', 'aetheri-terminal-pty-starter',
        'aetheri-shell', 'aetheri-editor', 'aetheri-text-editor')
MODEL_TESTS = (r'^(text\.|editor\.(document|direct|save_work|workspace|modal|history|search|selection|file|view)$'
               r'|shell\.(welcome|editor|completion|parse|failure)$|files\.(model|layout|launch|create|rename|move|session|notifications)$'
               r'|terminal\.(workspace|session|model|selection|paste|composition|pty)$|settings\.(model|sound)$|desktop\.model$'
               r'|compositor\.(compose_model|compose_inputs)$)')


def tree_inventory(prefix, max_file_bytes=64*1024**2, max_total_bytes=256*1024**2):
    """Content/link inventory for developer inputs, not filesystem authority."""
    prefix = Path(prefix).absolute()
    if prefix.resolve() != prefix:
        raise ValueError('aliased component prefix')
    entries, total = {}, 0
    for p in prefix.rglob('*'):
        n = canonical(p.relative_to(prefix).as_posix())
        s = p.lstat()
        if stat.S_ISDIR(s.st_mode):
            continue
        if len(entries) >= 1024:
            raise ValueError('component file count bound')
        if stat.S_ISLNK(s.st_mode):
            entries[n] = dict(symlink=os.readlink(p))
        elif stat.S_ISREG(s.st_mode) and s.st_nlink == 1 and s.st_size <= max_file_bytes:
            total += s.st_size
            if total > max_total_bytes:
                raise ValueError('component total byte bound')
            entries[n] = dict(size=s.st_size, sha256=graphics.digest_file(p))
        else:
            raise ValueError('different component file class/size')
    for n, e in entries.items():
        if 'symlink' not in e:
            continue
        current, seen = n, {n}
        for _ in range(8):
            target = entries[current]['symlink']
            if not target or target.startswith('/') or '\\' in target or '\0' in target:
                raise ValueError('escaping component alias')
            current = posixpath.normpath(str(PurePosixPath(current).parent/target))
            if current.startswith('../') or current not in entries or current in seen:
                raise ValueError('unresolved component alias')
            if 'symlink' not in entries[current]:
                break
            seen.add(current)
        else:
            raise ValueError('component alias depth')
    return entries


def inventory_digest(entries):
    return hashlib.sha256(json.dumps(entries, sort_keys=True, separators=(',', ':')).encode()).hexdigest()


def component_profile():
    v = json.loads(PROFILE.read_bytes())
    names = {name+'-'+kind for name in ('wlroots', 'ui', 'nexaui') for kind in ('release', 'memory')}
    if v.get('format') != 1 or v.get('purpose') != 'private-current-desktop-build' or set(v['components']) != names:
        raise ValueError('different current desktop input profile')
    for n, e in v['components'].items():
        if (e['record'] != ('build.json' if n.startswith('nexaui-') else 'result.json')
                or not 1 <= e['regular_files'] <= 1024 or not 0 <= e['aliases'] <= 128
                or not 0 < e['max_file_bytes'] <= e['total_bytes'] <= 256*1024**2
                or any(not re.fullmatch('[0-9a-f]{64}', e[k]) for k in ('tree_sha256', 'record_sha256', 'source_export_sha256'))):
            raise ValueError('invalid current component pin')
    return v


def check_component(build, pin):
    build = Path(build).absolute()
    if build.resolve() != build:
        raise ValueError('aliased component build')
    record = build/pin['record']
    if record.is_symlink() or not record.is_file() or record.stat().st_size > 2*1024**2 or graphics.digest_file(record) != pin['record_sha256']:
        raise ValueError('different independently selected component record')
    entries = tree_inventory(build/'prefix', pin['max_file_bytes'], pin['total_bytes'])
    counts = dict(regular_files=sum('symlink' not in e for e in entries.values()),
                  aliases=sum('symlink' in e for e in entries.values()))
    if inventory_digest(entries) != pin['tree_sha256'] or any(counts[k] != pin[k] for k in counts):
        raise ValueError('different independently selected component tree')
    return dict(counts, tree_sha256=pin['tree_sha256'], record_sha256=pin['record_sha256'])


def environment(args):
    ui, wlr, nexa = [getattr(args, n)/'prefix' for n in ('ui', 'wlroots', 'nexaui')]
    libs = [ui/'lib', nexa/'lib', wlr/'lib', args.app_sdk/'usr/lib64',
            args.session_sdk/'input/usr/lib64', args.session_sdk/'rpm/usr/lib64',
            args.common_sdk/'usr/lib64', args.graphics_runtime/'usr/lib64',
            args.sdl_sdk/'usr/lib64', args.toolchain/'usr/lib64',
            args.audio_control_prefix/'usr/lib64', args.audio_policy_prefix/'usr/lib64',
            args.audio_prefix/'usr/lib64', args.original_runtime/'usr/lib64']
    pcs = [args.app_sdk/'usr/lib64/pkgconfig', wlr/'lib/pkgconfig', ui/'lib/pkgconfig',
           args.session_sdk/'input/usr/lib64/pkgconfig', args.session_sdk/'rpm/usr/lib64/pkgconfig',
           args.session_sdk/'rpm/usr/share/pkgconfig', args.common_sdk/'usr/lib64/pkgconfig',
           args.common_sdk/'usr/share/pkgconfig', args.sdl_sdk/'usr/lib64/pkgconfig']
    env = dict(HOME='/home/aetheri', LC_ALL='C', PYTHONDONTWRITEBYTECODE='1',
               PATH=':'.join(map(str, [args.common_sdk/'usr/bin', args.toolchain/'bin']))+':/usr/bin:/usr/sbin:/bin:/sbin',
               LD_LIBRARY_PATH=':'.join(map(str, libs)), PKG_CONFIG='/usr/bin/pkg-config',
               PKG_CONFIG_LIBDIR=':'.join(map(str, pcs)), CC='/usr/bin/gcc',
               __EGL_VENDOR_LIBRARY_FILENAMES=str(args.graphics_runtime/'usr/share/glvnd/egl_vendor.d/50_mesa.json'),
               LIBGL_DRIVERS_PATH=str(args.graphics_runtime/'usr/lib64/dri'),
               GBM_BACKENDS_PATH=str(args.graphics_runtime/'usr/lib64/gbm'), MESA_SHADER_CACHE_DISABLE='true')
    if args.sanitizers:
        env.update(ASAN_OPTIONS='detect_leaks=1:abort_on_error=1', UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    return env, libs


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('work', type=Path)
    for n in ('app-sdk', 'common-sdk', 'session-sdk', 'sdl-sdk', 'toolchain', 'graphics-runtime',
              'original-runtime', 'wlroots', 'ui', 'nexaui', 'audio-prefix', 'audio-policy-prefix', 'audio-control-prefix', 'compose-data'):
        p.add_argument('--'+n, type=Path, required=True)
    p.add_argument('--sanitizers', action='store_true')
    args = p.parse_args()
    if sys.platform != 'linux' or os.getuid() == 0 or os.uname().machine != 'x86_64':
        raise ValueError('ordinary native x86-64 Linux builder required')
    for n, value in vars(args).items():
        if isinstance(value, Path):
            setattr(args, n, sdk.prefix_path(value))
    if any(args.work == value or args.work in value.parents or value in args.work.parents
           for n, value in vars(args).items() if isinstance(value, Path) and n != 'work'):
        raise ValueError('overlapping app build/source input')
    v = component_profile()
    kind = 'memory' if args.sanitizers else 'release'
    selected = {n: check_component(getattr(args, n), v['components'][n+'-'+kind])
                for n in ('wlroots', 'ui', 'nexaui')}
    inputs = dict(components=selected, common=sdk.check(args.common_sdk),
                  app=sdk.check(args.app_sdk, 'apps'), session=session.check(args.session_sdk),
                  sdl=sdk.check(args.sdl_sdk, 'sdl'), toolchain=tools.check(args.toolchain),
                  graphics=graphics.check(args.graphics_runtime))
    gui.check(args.original_runtime, json.loads((ROOT/'spec/settings-audio-lab-inputs.json').read_bytes()))
    prepare_audio.check(args.audio_prefix)
    prepare_audio_policy.check(args.audio_policy_prefix)
    prepare_audio_control.check(args.audio_control_prefix)
    compose = json.loads((ROOT/'spec/graphics-inputs.json').read_bytes())['compose_data']
    embed_compose.checked(args.compose_data/'Compose', compose['size'], compose['sha256'])
    embed_compose.checked(args.compose_data/'COPYING', compose['license_size'], compose['license_sha256'])
    inputs['compose'] = dict(table_sha256=compose['sha256'], license_sha256=compose['license_sha256'])
    ui = args.ui/'prefix'
    font = ui/'share/aetheri-fonts/LiberationSans-Regular.ttf'
    pins = json.loads((ROOT/'spec/ui-inputs.json').read_bytes())
    for pth, expected in [(font, pins['font']),
                          (Path(pins['terminal_font']['path']), pins['terminal_font']),
                          (Path(pins['terminal_font']['bold']['path']), pins['terminal_font']['bold'])]:
        if graphics.digest_file(pth) != expected['sha256']:
            raise ValueError('different native app font')
    if graphics.digest_file(pins['terminal_font']['license_path']) != pins['terminal_font']['license_sha256']:
        raise ValueError('different carrier font notice')
    env, libs = environment(args)
    args.work.mkdir(mode=0o700)
    rows = []
    def run(name, command):
        result = subprocess.run(command, env=env, capture_output=True)
        for suffix, raw in [('stdout', result.stdout), ('stderr', result.stderr)]:
            with (args.work/(name+'.'+suffix)).open('xb') as f:
                f.write(raw)
        rows.append(dict(name=name, command=command, exit=result.returncode))
        if result.returncode:
            raise RuntimeError(name+' failed; retain '+str(args.work))
        return result.stdout.decode()
    prefixes = [args.app_sdk/'usr', ui, args.nexaui/'prefix', args.wlroots/'prefix',
                args.common_sdk/'usr', args.session_sdk/'rpm/usr', args.session_sdk/'input/usr']
    command = [str(args.toolchain/'bin/cmake'), '-G', 'Ninja', '-S', str(ROOT), '-B', str(args.work/'build'),
               '-DCMAKE_BUILD_TYPE='+('Debug' if args.sanitizers else 'Release'),
               '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON', '-DCMAKE_PREFIX_PATH='+';'.join(map(str, prefixes)),
               '-DCMAKE_BUILD_RPATH='+';'.join(map(str, libs)), '-DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF',
               '-DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY=OFF', '-DPython3_EXECUTABLE=/usr/bin/python3',
               '-DPKG_CONFIG_EXECUTABLE=/usr/bin/pkg-config',
               '-DOPENSSL_INCLUDE_DIR='+str(args.app_sdk/'usr/include'),
               '-DOPENSSL_CRYPTO_LIBRARY='+str(args.app_sdk/'usr/lib64/libcrypto.so'),
               '-DSQLite3_INCLUDE_DIR='+str(args.app_sdk/'usr/include'),
               '-DSQLite3_LIBRARY='+str(args.app_sdk/'usr/lib64/libsqlite3.so'),
               '-DAETHERI_NEXAUI_PREFIX='+str(args.nexaui/'prefix'),
               '-DNexaUI_DIR:PATH='+str(args.nexaui/'prefix/lib/cmake/NexaUI'),
               '-DSDL3_DIR='+str(ui/'lib/cmake/SDL3'), '-DSDL3_ttf_DIR='+str(ui/'lib/cmake/SDL3_ttf'),
               '-DAETHERI_SETTINGS_TEST_FONT='+str(font),
               '-DAETHERI_TERMINAL_TEST_FONT='+pins['terminal_font']['path'],
               '-DAETHERI_AUDIO_PREFIX='+str(args.audio_prefix),
               '-DAETHERI_AUDIO_POLICY_PREFIX='+str(args.audio_policy_prefix),
               '-DAETHERI_AUDIO_CONTROL_PREFIX='+str(args.audio_control_prefix),
               '-DAETHERI_COMPOSE_TABLE='+str(args.compose_data/'Compose'),
               '-DAETHERI_COMPOSE_LICENSE='+str(args.compose_data/'COPYING'),
               '-DAETHERI_PRIVILEGED_TESTS=OFF', '-DAETHERI_POLICY_PROBES=OFF', '-DBUILD_TESTING=ON']
    command += ['-DAETHERI_'+n+'=ON' for n in ('COMPOSITOR', 'DRM_SESSION', 'COMPOSE', 'DESKTOP', 'SETTINGS',
                'FILES', 'TERMINAL', 'SHELL', 'EDITOR', 'GRAPHICAL_EDITOR', 'LINUX_IPC', 'AUDIO', 'AUDIO_CONTROL')]
    if args.sanitizers:
        command += ['-DAETHERI_SANITIZERS=ON', '-DCMAKE_C_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer',
                    '-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined']
    run('configure', command)
    run('compile', [str(args.toolchain/'bin/cmake'), '--build', str(args.work/'build'), '-j2'])
    # Use the checked original ctest binary: its convenience wrapper intentionally
    # resets LD_LIBRARY_PATH for tool-only use, while GUI tests need this graph.
    ctest = str(args.toolchain/'usr/bin/ctest')
    inventory = json.loads(run('test-inventory', [ctest, '--test-dir', str(args.work/'build'), '--show-only=json-v1']))
    run('models', [ctest, '--test-dir', str(args.work/'build'), '-R', MODEL_TESTS, '-j2', '--output-on-failure'])
    artifacts = {name: dict(size=(args.work/'build'/name).stat().st_size,
                           sha256=graphics.digest_file(args.work/'build'/name)) for name in APPS}
    for n in ('wlroots', 'ui', 'nexaui'):
        if check_component(getattr(args, n), v['components'][n+'-'+kind]) != selected[n]:
            raise ValueError('component changed during app build')
    with (args.work/'result.json').open('x') as f:
        json.dump(dict(rows=rows, inputs=inputs, environment=env, artifacts=artifacts,
                       sanitizers=args.sanitizers, test_count=len(inventory['tests']),
                       model_test_regex=MODEL_TESTS,
                       scope='current private apps/core build and ordinary app model tests; administrative catalogue/core and GUI suites are separate, no image or normal admission'), f, indent=2)
    print(json.dumps(dict(work=str(args.work), apps=len(artifacts), rows=rows)), flush=True)


if __name__ == '__main__':
    main()
