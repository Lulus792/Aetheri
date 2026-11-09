"""Real private EGL/GLES shader draw and pixel readback in the disposable lab.

This is a surfaceless llvmpipe proof, not a window, physical GPU or session test.
The observed loader/map inventory includes transitive and dynamically loaded code.
"""
import argparse
import json
import os
from pathlib import Path
import subprocess

import prepare_desktop_sdk as sdk
import prepare_desktop_toolchain as tools
import prepare_graphics_runtime as graphics

C_SOURCE = r'''#define _GNU_SOURCE
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if(!(x)) { fprintf(stderr,"check:%d: %s EGL=%x GL=%x\n",__LINE__,#x,eglGetError(),glGetError()); return 1; } } while(0)
static GLuint shader(GLenum type,const char *source) {
    GLuint s=glCreateShader(type); GLint ok=0; glShaderSource(s,1,&source,NULL);
    glCompileShader(s); glGetShaderiv(s,GL_COMPILE_STATUS,&ok);
    if(!ok) { char log[4096]; glGetShaderInfoLog(s,sizeof(log),NULL,log); fprintf(stderr,"shader: %s\n",log); glDeleteShader(s); return 0; }
    return s;
}
int main(void) {
    PFNEGLGETPLATFORMDISPLAYEXTPROC platform=(PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT"); CHECK(platform);
    EGLDisplay d=platform(EGL_PLATFORM_SURFACELESS_MESA,EGL_DEFAULT_DISPLAY,NULL); CHECK(d!=EGL_NO_DISPLAY);
    EGLint major=0,minor=0; CHECK(eglInitialize(d,&major,&minor)); CHECK(major==1 && minor==5);
    const char *vendor=eglQueryString(d,EGL_VENDOR); CHECK(vendor && strstr(vendor,"Mesa"));
    CHECK(eglBindAPI(EGL_OPENGL_ES_API));
    EGLint attrs[]={EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_RENDERABLE_TYPE,EGL_OPENGL_ES2_BIT,EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_ALPHA_SIZE,8,EGL_NONE};
    EGLConfig config; EGLint count=0; CHECK(eglChooseConfig(d,attrs,&config,1,&count) && count==1);
    EGLint surface_attrs[]={EGL_WIDTH,64,EGL_HEIGHT,64,EGL_NONE};
    EGLSurface surface=eglCreatePbufferSurface(d,config,surface_attrs); CHECK(surface!=EGL_NO_SURFACE);
    EGLint context_attrs[]={EGL_CONTEXT_CLIENT_VERSION,2,EGL_NONE};
    EGLContext context=eglCreateContext(d,config,EGL_NO_CONTEXT,context_attrs); CHECK(context!=EGL_NO_CONTEXT);
    CHECK(eglMakeCurrent(d,surface,surface,context));
    const char *version=(const char *)glGetString(GL_VERSION), *renderer=(const char *)glGetString(GL_RENDERER);
    CHECK(version && renderer && strstr(version,"Mesa 26.2.5") && strstr(renderer,"llvmpipe") && strstr(renderer,"LLVM 22.1.8"));
    printf("EGL %d.%d %s\nGL %s\nRENDERER %s\n",major,minor,vendor,version,renderer);
    GLuint vertex=shader(GL_VERTEX_SHADER,"attribute vec2 position; void main(){gl_Position=vec4(position,0.0,1.0);}"); CHECK(vertex);
    GLuint fragment=shader(GL_FRAGMENT_SHADER,"precision mediump float; void main(){gl_FragColor=vec4(1.0,0.0,0.0,1.0);}"); CHECK(fragment);
    GLuint program=glCreateProgram(); glAttachShader(program,vertex); glAttachShader(program,fragment);
    glBindAttribLocation(program,0,"position"); glLinkProgram(program); GLint linked=0;glGetProgramiv(program,GL_LINK_STATUS,&linked); CHECK(linked);
    glUseProgram(program); const GLfloat vertices[]={-0.8f,-0.8f,0.8f,-0.8f,0.0f,0.8f};
    GLuint buffer=0;glGenBuffers(1,&buffer);glBindBuffer(GL_ARRAY_BUFFER,buffer);glBufferData(GL_ARRAY_BUFFER,sizeof(vertices),vertices,GL_STATIC_DRAW);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,0,NULL);glEnableVertexAttribArray(0);
    glViewport(0,0,64,64);glClearColor(0,0,1,1);glClear(GL_COLOR_BUFFER_BIT);glDrawArrays(GL_TRIANGLES,0,3);glFinish();CHECK(glGetError()==GL_NO_ERROR);
    unsigned char pixels[64*64*4];glReadPixels(0,0,64,64,GL_RGBA,GL_UNSIGNED_BYTE,pixels);CHECK(glGetError()==GL_NO_ERROR);
    unsigned red=0,blue=0;
    for(size_t i=0;i<64*64;++i) { const unsigned char *p=pixels+i*4;CHECK(p[3]==255 && p[1]==0);if(p[0]==255 && p[2]==0)++red;else if(p[0]==0 && p[2]==255)++blue;else CHECK(0); }
    CHECK(red>1000 && blue>1000 && red+blue==4096);
    const unsigned char *center=pixels+(25*64+32)*4;CHECK(center[0]==255 && center[2]==0 && pixels[0]==0 && pixels[2]==255);
    printf("PIXELS red=%u blue=%u total=4096\n",red,blue);
    FILE *maps=fopen("/proc/self/maps","r");CHECK(maps);char *line=NULL;size_t cap=0;
    while(getline(&line,&cap,maps)>=0) if(strchr(line,'/')) printf("MAP %s",line);
    free(line);CHECK(!fclose(maps));
    glDeleteBuffers(1,&buffer);glDeleteProgram(program);glDeleteShader(fragment);glDeleteShader(vertex);
    CHECK(eglMakeCurrent(d,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT));
    CHECK(eglDestroyContext(d,context));CHECK(eglDestroySurface(d,surface));CHECK(eglTerminate(d));CHECK(eglReleaseThread());
    puts("GRAPHICS_NATIVE_OK");return 0;
}
'''


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('graphics', 'common', 'sdl', 'toolchain', 'work'):
        p.add_argument(name, type=Path)
    args = p.parse_args()
    if os.getuid() == 0:
        raise ValueError('ordinary native build account required')
    runtime, common, sdl, toolchain, work = [sdk.prefix_path(getattr(args, n)) for n in ('graphics', 'common', 'sdl', 'toolchain', 'work')]
    graphics.check(runtime)
    sdk.check(common)
    sdk.check(sdl, 'sdl')
    tools.check(toolchain)
    v, _ = graphics.profile()
    for name, e in v['native_files'].items():
        path = Path(name)
        if path.resolve().as_posix() != name or path.stat().st_size != e['size'] or graphics.digest_file(path) != e['sha256']:
            raise ValueError('different native graphics prerequisite: '+name)
    work.mkdir(mode=0o700)
    (work/'probe.c').write_text(C_SOURCE)
    libs = [runtime/'usr/lib64', common/'usr/lib64', sdl/'usr/lib64', toolchain/'usr/lib64']
    env = dict(HOME='/home/aetheri', PATH=str(toolchain/'bin')+':/usr/bin:/usr/sbin:/bin:/sbin',
               LC_ALL='C', LD_LIBRARY_PATH=':'.join(map(str, libs)),
               __EGL_VENDOR_LIBRARY_FILENAMES=str(runtime/'usr/share/glvnd/egl_vendor.d/50_mesa.json'),
               LIBGL_DRIVERS_PATH=str(runtime/'usr/lib64/dri'), GBM_BACKENDS_PATH=str(runtime/'usr/lib64/gbm'),
               EGL_PLATFORM='surfaceless', LIBGL_ALWAYS_SOFTWARE='true', GALLIUM_DRIVER='llvmpipe',
               MESA_SHADER_CACHE_DISABLE='true')
    rows = []
    def run(name, command, expected_exit=0):
        result = subprocess.run(command, env=env, capture_output=True)
        for suffix, raw in [('stdout', result.stdout), ('stderr', result.stderr)]:
            with (work/(name+'.'+suffix)).open('xb') as f:
                f.write(raw)
        rows.append(dict(name=name, command=command, exit=result.returncode, expected_exit=expected_exit))
        if result.returncode != expected_exit:
            raise RuntimeError(name+' failed; retain '+str(work))
        return result.stdout.decode()
    executable = work/'probe'
    run('compile', ['/usr/bin/gcc', '-std=c17', '-O2', '-Wall', '-Wextra', '-Wconversion', '-Werror',
                    '-I'+str(sdl/'usr/include'), str(work/'probe.c'), '-L'+str(sdl/'usr/lib64'),
                    '-Wl,-rpath,'+':'.join(map(str, libs)), '-lEGL', '-lGLESv2', '-o', str(executable)])
    output = run('draw', [str(executable)])
    if 'GRAPHICS_NATIVE_OK' not in output or 'PIXELS red=' not in output:
        raise ValueError('incomplete graphics draw proof')
    mapped = set()
    for line in output.splitlines():
        if not line.startswith('MAP '):
            continue
        fields = line[4:].split(None, 5)
        if len(fields) == 6 and fields[5].startswith('/'):
            # Private driver JIT memory may have a named deleted memfd; it is
            # not an ELF dependency or a filesystem library selection.
            if fields[5].startswith('/memfd:'):
                continue
            mapped.add(fields[5])
    required = [runtime/'usr/lib64'/n for n in ('libEGL_mesa.so.0.0.0', 'libgallium-26.2.5.so', 'libLLVM.so.22.1')]
    required += [sdl/'usr/lib64'/n for n in ('libEGL.so.1.1.0', 'libGLESv2.so.2.1.0', 'libGLdispatch.so.0.0.0')]
    required.append(toolchain/'usr/lib64/libstdc++.so.6.0.36')
    if any(str(n) not in mapped for n in required):
        raise ValueError('missing actual private renderer/dispatch/C++ mapping')
    admitted = set(v['native_files']) | {str(executable)}
    for prefix, entries in ((runtime, [*v['files'], *v['notices']]),
                            (common, sdk.profile()[0]['files']),
                            (sdl, sdk.profile('sdl')[0]['files']),
                            (toolchain, tools.profile()[0]['files'])):
        admitted.update(str(prefix/e['path']) for e in entries)
    if mapped-admitted:
        raise ValueError('unexplained renderer mapping: '+repr(sorted(mapped-admitted)))
    observed = {n: dict(size=Path(n).stat().st_size, sha256=graphics.digest_file(n)) for n in sorted(mapped)}
    # This separate test-only executable has the real context/shaders/clear but
    # deliberately omits drawing. A loaded renderer alone cannot satisfy pixels.
    draw_call = 'glDrawArrays(GL_TRIANGLES,0,3);'
    if C_SOURCE.count(draw_call) != 1:
        raise ValueError('different no-draw counter source')
    (work/'counter.c').write_text(C_SOURCE.replace(draw_call, ''))
    run('counter-compile', ['/usr/bin/gcc', '-std=c17', '-O2', '-Wall', '-Wextra', '-Wconversion', '-Werror',
                            '-I'+str(sdl/'usr/include'), str(work/'counter.c'), '-L'+str(sdl/'usr/lib64'),
                            '-Wl,-rpath,'+':'.join(map(str, libs)), '-lEGL', '-lGLESv2', '-o', str(work/'counter')])
    counter = run('counter-draw', [str(work/'counter')], expected_exit=1)
    if ('GRAPHICS_NATIVE_OK' in counter
            or 'red>1000 && blue>1000 && red+blue==4096' not in (work/'counter-draw.stderr').read_text()):
        raise ValueError('no-draw counter failed for a different reason')
    with (work/'result.json').open('x') as f:
        json.dump(dict(rows=rows, mapped=observed, scope='real private surfaceless llvmpipe shader/pixel proof; no physical GPU/window/session acceptance'), f, indent=2)
    print(json.dumps(dict(work=str(work), rows=rows, mapped_files=len(observed))), flush=True)


if __name__ == '__main__':
    main()
