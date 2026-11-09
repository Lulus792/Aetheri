"""Compile/run the installed private SDL/text SDK without a display or device.

Exercises real version/ELF selection, event queue, font shaping and CPU surfaces.
This does not test a Wayland window, GPU context, compositor or app admission.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess

import prepare_desktop_sdk as sdk
import prepare_desktop_toolchain as tools

C_SOURCE = r'''#define _GNU_SOURCE
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <hb.h>
#include <dlfcn.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(e) do { if (!(e)) { fprintf(stderr,"check:%d: %s (%s)\n",__LINE__,#e,SDL_GetError()); return 1; } } while(0)
static int selected(const char *symbol, const char *prefix) {
    Dl_info info; char path[PATH_MAX], expected[PATH_MAX];
    void *value=dlsym(RTLD_DEFAULT,symbol);
    if(!value || !dladdr(value,&info) || !realpath(info.dli_fname,path)) return 0;
    if(snprintf(expected,sizeof(expected),"%s/lib/",prefix)<0) return 0;
    if(strncmp(path,expected,strlen(expected))) return 0;
    printf("SELECTED %s %s\n",symbol,path); return 1;
}
int main(int argc,char **argv) {
    CHECK(argc==2);
    CHECK(SDL_GetVersion()==SDL_VERSIONNUM(3,4,20));
    CHECK(TTF_Version()==SDL_VERSIONNUM(3,2,2));
    CHECK(!strcmp(hb_version_string(),"15.0.0"));
    FT_Library library=NULL; FT_Int major=0,minor=0,patch=0;
    CHECK(!FT_Init_FreeType(&library)); FT_Library_Version(library,&major,&minor,&patch);
    CHECK(major==2 && minor==15 && patch==0); CHECK(!FT_Done_FreeType(library));
    CHECK(selected("SDL_GetVersion",argv[1]) && selected("TTF_Version",argv[1]) &&
          selected("FT_Init_FreeType",argv[1]) && selected("hb_version_string",argv[1]));
    CHECK(SDL_Init(SDL_INIT_EVENTS)); CHECK(TTF_Init());
    SDL_Event event; while(SDL_PollEvent(&event)) {}
    Uint32 kind=SDL_RegisterEvents(1); CHECK(kind!=0);
    memset(&event,0,sizeof(event)); event.type=kind; event.user.code=42;
    CHECK(SDL_PushEvent(&event)); memset(&event,0,sizeof(event));
    CHECK(SDL_WaitEventTimeout(&event,1000) && event.type==kind && event.user.code==42);
    char font_path[PATH_MAX]; CHECK(snprintf(font_path,sizeof(font_path),"%s/share/aetheri-fonts/LiberationSans-Regular.ttf",argv[1])>0);
    TTF_Font *font=TTF_OpenFont(font_path,24.0f); CHECK(font);
    const char *texts[]={"Aetheri", "\xc3\x84theri", "e\xcc\x81", "office"};
    for(size_t i=0;i<sizeof(texts)/sizeof(texts[0]);++i) {
        int w=0,h=0; CHECK(TTF_GetStringSize(font,texts[i],strlen(texts[i]),&w,&h) && w>0 && h>0);
        SDL_Surface *surface=TTF_RenderText_Blended(font,texts[i],strlen(texts[i]),(SDL_Color){255,255,255,255}); CHECK(surface);
        CHECK(surface->w==w && surface->h==h); unsigned ink=0;
        for(int y=0;y<h;++y) for(int x=0;x<w;++x) {
            Uint8 r=0,g=0,b=0,a=0; CHECK(SDL_ReadSurfacePixel(surface,x,y,&r,&g,&b,&a));
            if(a) { CHECK(r==255 && g==255 && b==255); ++ink; }
        }
        CHECK(ink>0); printf("TEXT %zu width=%d height=%d ink=%u\n",i,w,h,ink);
        SDL_DestroySurface(surface);
    }
    TTF_CloseFont(font); TTF_Quit(); SDL_Quit();
    FILE *maps=fopen("/proc/self/maps","r"); CHECK(maps); char *line=NULL;size_t capacity=0;
    while(getline(&line,&capacity,maps)>=0) if(strstr(line,"/home/aetheri/deps/") || strstr(line,"libstdc++.so")) printf("MAP %s",line);
    free(line); CHECK(!fclose(maps)); puts("UI_TEXT_NATIVE_OK"); return 0;
}
'''

CMAKE = '''cmake_minimum_required(VERSION 3.24)
project(AetheriUITextProof LANGUAGES C)
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
find_package(SDL3 3.4.20 EXACT CONFIG REQUIRED)
find_package(SDL3_ttf 3.2.2 EXACT CONFIG REQUIRED)
find_package(Freetype 2.15.0 EXACT REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(HB REQUIRED IMPORTED_TARGET harfbuzz=15.0.0)
add_executable(probe probe.c)
target_compile_options(probe PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Werror)
target_link_libraries(probe PRIVATE SDL3::SDL3 SDL3_ttf::SDL3_ttf Freetype::Freetype PkgConfig::HB ${CMAKE_DL_LIBS})
'''


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for n in ('ui', 'common', 'sdl', 'toolchain', 'work'):
        p.add_argument(n, type=Path)
    p.add_argument('--sanitizers', action='store_true', help='require and exercise the separately instrumented SDK')
    args = p.parse_args()
    if os.getuid() == 0:
        raise ValueError('ordinary native build account required')
    ui, common, sdl, toolchain, work = [sdk.prefix_path(getattr(args, n)) for n in ('ui', 'common', 'sdl', 'toolchain', 'work')]
    sdk.check(common)
    sdk.check(sdl, 'sdl')
    tools.check(toolchain)
    report = json.loads((ui/'result.json').read_bytes())
    prefix = ui/'prefix'
    for n, digest in report['artifacts'].items():
        if hashlib.sha256((prefix/n).read_bytes()).hexdigest() != digest:
            raise ValueError('changed UI SDK artifact')
    if args.sanitizers:
        for n in ('libSDL3.so', 'libSDL3_ttf.so', 'libfreetype.so', 'libharfbuzz.so'):
            dynamic = subprocess.check_output(['readelf', '-d', str(prefix/'lib'/n)], text=True)
            symbols = subprocess.check_output(['nm', '-D', str(prefix/'lib'/n)], text=True)
            if not all(s in dynamic for s in ('libasan.so.8', 'libubsan.so.1')) or '__asan_' not in symbols or '__ubsan_' not in symbols:
                raise ValueError('uninstrumented UI SDK: '+n)
    work.mkdir(mode=0o700)
    (work/'probe.c').write_text(C_SOURCE)
    (work/'CMakeLists.txt').write_text(CMAKE)
    libs = [prefix/'lib', common/'usr/lib64', sdl/'usr/lib64', toolchain/'usr/lib64']
    env = dict(HOME='/home/aetheri', PATH=str(toolchain/'bin')+':/usr/bin:/usr/sbin:/bin:/sbin',
               LC_ALL='C', PYTHONDONTWRITEBYTECODE='1', LD_LIBRARY_PATH=':'.join(map(str, libs)),
               PKG_CONFIG_LIBDIR=':'.join(map(str, [prefix/'lib/pkgconfig', common/'usr/lib64/pkgconfig', common/'usr/share/pkgconfig', sdl/'usr/lib64/pkgconfig'])))
    if args.sanitizers:
        env.update(ASAN_OPTIONS='detect_leaks=1:abort_on_error=1',
                   UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
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

    memory_flags = ['-DCMAKE_C_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer',
                    '-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined'] if args.sanitizers else []
    run('configure', [str(toolchain/'bin/cmake'), '-G', 'Ninja', '-S', str(work), '-B', str(work/'build'),
                      '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_PREFIX_PATH='+';'.join(map(str, [prefix, common/'usr', sdl/'usr'])),
                      '-DCMAKE_BUILD_RPATH='+';'.join(map(str, libs)), *memory_flags])
    run('build', [str(toolchain/'bin/cmake'), '--build', str(work/'build'), '-j2'])
    output = run('probe', [str(work/'build/probe'), str(prefix)])
    if 'UI_TEXT_NATIVE_OK' not in output or output.count('SELECTED ') != 4 or output.count('TEXT ') != 4:
        raise ValueError('incomplete native UI/text result')
    for n in ('libSDL3.so', 'libSDL3_ttf.so', 'libfreetype.so', 'libharfbuzz.so'):
        if not any(line.startswith('MAP ') and str(prefix/'lib'/n) in line for line in output.splitlines()):
            raise ValueError('missing actual SDK mapping: '+n)
    listing = run('loader', ['/lib64/ld-linux-x86-64.so.2', '--list', str(work/'build/probe')])
    cpp_maps = [line for line in output.splitlines() if line.startswith('MAP ') and 'libstdc++.so' in line]
    cpp_required = 'libstdc++.so.6' in listing
    if cpp_required:
        if not cpp_maps or any(str(toolchain/'usr/lib64/libstdc++.so.6.0.36') not in line for line in cpp_maps):
            raise ValueError('different actual C++ runtime')
    elif cpp_maps:
        raise ValueError('unexpected extra C++ runtime')
    with (work/'result.json').open('x') as f:
        json.dump(dict(rows=rows, cpp_runtime_required=cpp_required,
                       sanitizers=args.sanitizers, leak_detection=args.sanitizers,
                       scope='native CPU text/event/ABI proof; no window/GPU/session admission'), f, indent=2)
    print(json.dumps(dict(work=str(work), rows=rows)), flush=True)


if __name__ == '__main__':
    main()
