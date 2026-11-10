"""Compile and exercise the actual installed wlroots/input/session SDK.

Creates a real headless output, Pixman renderer/allocator and unassigned libinput
context. No seat/device permission, physical output or normal app admission.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess

from build_graphics import private_environment
import prepare_desktop_sdk as sdk

C_SOURCE = r'''#define _GNU_SOURCE
#define WLR_USE_UNSTABLE
#include <wlr/config.h>
#include <wlr/version.h>
#include <wlr/backend/headless.h>
#include <wlr/render/allocator.h>
#include <wlr/render/pixman.h>
#include <libinput.h>
#include <libudev.h>
#include <libseat.h>
#include <dlfcn.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if !WLR_HAS_DRM_BACKEND || !WLR_HAS_LIBINPUT_BACKEND || !WLR_HAS_SESSION
#error Missing required native session build
#endif
#if WLR_HAS_XWAYLAND || WLR_HAS_GLES2_RENDERER || WLR_HAS_GBM_ALLOCATOR
#error Different retained Pixman development profile
#endif
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"check:%d: %s\n",__LINE__,#x); return 1; } } while(0)
static unsigned opens=0;
static int open_device(const char *path,int flags,void *data) {(void)path;(void)flags;(void)data;++opens;return -EPERM;}
static void close_device(int fd,void *data) {(void)fd;(void)data;abort();}
static const struct libinput_interface interface={open_device,close_device};
static int selected(const char *name) { Dl_info info;void *s=dlsym(RTLD_DEFAULT,name);if(!s || !dladdr(s,&info))return 0;printf("SELECTED %s %s\n",name,info.dli_fname);return 1; }
int main(void) {
    CHECK(wlr_version_get_major()==0 && wlr_version_get_minor()==20 && wlr_version_get_micro()==2);
    CHECK(selected("wlr_headless_backend_create") && selected("libinput_udev_create_context") && selected("udev_new") && selected("libseat_set_log_level"));
    libseat_set_log_level(LIBSEAT_LOG_LEVEL_SILENT);
    struct udev *udev=udev_new();CHECK(udev);
    struct libinput *input=libinput_udev_create_context(&interface,NULL,udev);CHECK(input);CHECK(opens==0);
    input=libinput_unref(input);udev=udev_unref(udev);CHECK(!input && !udev);
    struct wl_display *display=wl_display_create();CHECK(display);
    struct wlr_backend *backend=wlr_headless_backend_create(wl_display_get_event_loop(display));CHECK(backend && wlr_backend_is_headless(backend));
    struct wlr_renderer *renderer=wlr_pixman_renderer_create();CHECK(renderer && wlr_renderer_is_pixman(renderer));
    struct wlr_allocator *allocator=wlr_allocator_autocreate(backend,renderer);CHECK(allocator);
    struct wlr_output *output=wlr_headless_add_output(backend,64,64);CHECK(output && wlr_output_is_headless(output));CHECK(wlr_output_init_render(output,allocator,renderer));
    struct wlr_output_state state;wlr_output_state_init(&state);wlr_output_state_set_enabled(&state,true);wlr_output_state_set_custom_mode(&state,64,64,60000);
    CHECK(wlr_output_commit_state(output,&state));wlr_output_state_finish(&state);CHECK(wlr_backend_start(backend));
    CHECK(output->enabled && output->width==64 && output->height==64);CHECK(wl_event_loop_dispatch(wl_display_get_event_loop(display),0)>=0);
    puts("HEADLESS width=64 height=64 renderer=pixman input_device_opens=0");
    FILE *maps=fopen("/proc/self/maps","r");CHECK(maps);char *line=NULL;size_t cap=0;
    while(getline(&line,&cap,maps)>=0) {
        if(strchr(line,'/'))printf("MAP %s",line);
    }
    free(line);CHECK(!fclose(maps));
    wlr_backend_destroy(backend);wlr_allocator_destroy(allocator);wlr_renderer_destroy(renderer);wl_display_destroy(display);
    puts("WLROOTS_NATIVE_OK");return 0;
}
'''

CMAKE = '''cmake_minimum_required(VERSION 3.24)
project(AetheriWlrootsSDK LANGUAGES C)
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
find_package(PkgConfig REQUIRED)
pkg_check_modules(DEPS REQUIRED IMPORTED_TARGET wlroots-0.20=0.20.2 wayland-server=1.26.0 libinput=1.32.0 libudev=259 libseat=0.9.3)
add_executable(probe probe.c)
target_compile_options(probe PRIVATE -Wall -Wextra -Werror)
target_link_libraries(probe PRIVATE PkgConfig::DEPS ${CMAKE_DL_LIBS})
'''


def mapped_files(output):
    files, buffers = set(), set()
    for line in output.splitlines():
        if not line.startswith('MAP '):
            continue
        fields = line[4:].split(None, 5)
        if len(fields) != 6 or not fields[5].startswith('/'):
            continue
        name, permissions = fields[5], fields[1]
        if (re.fullmatch(r'/dev/shm/wlroots-[A-Za-z0-9]{6} \(deleted\)', name)
                and permissions == 'rw-s'):
            buffers.add(name)
        elif name.endswith(' (deleted)') or name.startswith('/memfd:'):
            raise ValueError('unexplained deleted/executable mapping: '+name)
        else:
            files.add(name)
    return files, sorted(buffers)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for n in ('wlroots', 'common_sdk', 'session_sdk', 'sdl_sdk', 'toolchain', 'original_runtime', 'work'):
        p.add_argument(n, type=Path)
    p.add_argument('--sanitizers', action='store_true')
    args = p.parse_args()
    if os.getuid() == 0:
        raise ValueError('ordinary native builder required')
    args.session = True
    env, private = private_environment(args)
    work, wlroots = sdk.prefix_path(args.work), sdk.prefix_path(args.wlroots)
    result = json.loads((wlroots/'result.json').read_bytes())
    library = wlroots/'prefix/lib/libwlroots-0.20.so'
    if hashlib.sha256(library.read_bytes()).hexdigest() != result['library_sha256']:
        raise ValueError('different installed wlroots')
    if args.sanitizers:
        symbols = subprocess.check_output(['nm', '-D', str(library)], text=True)
        dynamic = subprocess.check_output(['readelf', '-dW', str(library)], text=True)
        if not all(s in symbols for s in ('__asan_', '__ubsan_')) or not all(s in dynamic for s in ('libasan.so.8', 'libubsan.so.1')):
            raise ValueError('uninstrumented wlroots')
        env.update(ASAN_OPTIONS='detect_leaks=1:abort_on_error=1', UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    env['LD_LIBRARY_PATH'] = str(wlroots/'prefix/lib')+':'+env['LD_LIBRARY_PATH']
    env['PKG_CONFIG_LIBDIR'] = str(wlroots/'prefix/lib/pkgconfig')+':'+env['PKG_CONFIG_LIBDIR']
    work.mkdir(mode=0o700)
    (work/'probe.c').write_text(C_SOURCE)
    (work/'CMakeLists.txt').write_text(CMAKE)
    rows = []
    def run(name, command):
        result = subprocess.run(command, env=env, capture_output=True)
        for suffix, raw in [('stdout', result.stdout), ('stderr', result.stderr)]:
            with (work/(name+'.'+suffix)).open('xb') as f:
                f.write(raw)
        rows.append(dict(name=name, command=command, exit=result.returncode))
        if result.returncode:
            raise RuntimeError(name+' failed; retain '+str(work))
        return result.stdout.decode()
    flags = ['-DCMAKE_C_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer',
             '-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined'] if args.sanitizers else []
    rpath = str(wlroots/'prefix/lib')+':'+private['rpath']
    run('configure', [str(args.toolchain/'bin/cmake'), '-G', 'Ninja', '-S', str(work), '-B', str(work/'build'),
                      '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_BUILD_RPATH='+rpath.replace(':', ';'), *flags])
    run('build', [str(args.toolchain/'bin/cmake'), '--build', str(work/'build'), '-j2'])
    output = run('probe', [str(work/'build/probe')])
    if 'WLROOTS_NATIVE_OK' not in output or output.count('SELECTED ') != 4:
        raise ValueError('incomplete wlroots proof')
    mapped, buffers = mapped_files(output)
    expected = [library, args.session_sdk/'input/usr/lib64/libinput.so.10.13.0',
                args.session_sdk/'rpm/usr/lib64/libudev.so.1.7.12', args.session_sdk/'input/usr/lib64/liblua-5.5.so']
    if any(str(n) not in mapped for n in expected):
        raise ValueError('different actual wlroots/input/udev/Lua mapping')
    observed = {n: dict(size=Path(n).stat().st_size, sha256=hashlib.sha256(Path(n).read_bytes()).hexdigest()) for n in sorted(mapped)}
    with (work/'result.json').open('x') as f:
        json.dump(dict(rows=rows, mapped=observed, shared_pixel_buffers=buffers, sanitizers=args.sanitizers, leak_detection=args.sanitizers,
                       scope='real headless Pixman and unassigned input ABI/lifetime proof; no device or normal session admission'), f, indent=2)
    print(json.dumps(dict(work=str(work), rows=rows, mapped_files=len(observed))), flush=True)


if __name__ == '__main__':
    main()
