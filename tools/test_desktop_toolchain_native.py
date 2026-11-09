"""Explicit ordinary-user C/C++/CMake proof; creates only a fresh work tree.

Tests the private tooling, not an Aetheri SDK, app sandbox or product release.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import subprocess
import prepare_desktop_toolchain as tools


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('prefix', type=Path)
    parser.add_argument('work', type=Path)
    args = parser.parse_args()
    if os.getuid() == 0:
        raise ValueError('use an ordinary native build account')
    prefix, work = args.prefix.absolute(), args.work.absolute()
    if prefix.resolve() != prefix or work.resolve() != work:
        raise ValueError('aliased tool/proof tree')
    tools.check(prefix)
    work.mkdir(mode=0o700)
    (work/'CMakeLists.txt').write_text('''cmake_minimum_required(VERSION 3.24)
project(AetheriPrivateToolProof LANGUAGES C CXX)
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
find_package(Threads REQUIRED)
add_compile_options(-Wall -Wextra -Wpedantic -Werror)
add_executable(c17 c17.c)
add_executable(cxx23 cxx23.cpp)
target_link_libraries(cxx23 PRIVATE Threads::Threads)
add_library(adapter SHARED adapter.cpp)
add_executable(c_adapter c_adapter.c)
target_link_libraries(c_adapter PRIVATE ${CMAKE_DL_LIBS})
enable_testing()
add_test(NAME c17 COMMAND c17)
add_test(NAME cxx23 COMMAND cxx23)
add_test(NAME c_adapter COMMAND c_adapter $<TARGET_FILE:adapter>)
install(TARGETS c17 cxx23 c_adapter adapter)
''')
    (work/'c17.c').write_text('''#include <stdio.h>
struct pair {int first; int second;};
int main(void) { const struct pair p={.first=19,.second=23};
if(p.first+p.second!=42) { return 1; } puts("C17_OK"); return 0; }
''')
    (work/'cxx23.cpp').write_text('''#include <algorithm>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
static_assert(_GLIBCXX_RELEASE==16);
int main() {
std::vector<int> v{7,3,5}; std::ranges::sort(v); if(v.front()!=3||v.back()!=7) return 1;
std::atomic<int> x{0}; {std::jthread t([&]{x.store(42);});} if(x.load()!=42) return 2;
bool caught=false; try {throw std::runtime_error("original");} catch(const std::exception& e){caught=std::string(e.what())=="original";} if(!caught) return 3;
if(std::filesystem::path("a/b").filename()!="b") return 4;
std::ifstream maps("/proc/self/maps"); std::string line; bool selected=false;
while(std::getline(maps,line)) if(line.find("__AETHERI_CXX_RUNTIME_PATH__")!=std::string::npos) selected=true;
if(!selected) { return 5; } std::cout<<"CXX23_EXCEPTION_THREAD_VECTOR_PRIVATE_RUNTIME_OK\\n";
}
'''.replace('__AETHERI_CXX_RUNTIME_PATH__',str(prefix/'usr/lib64/libstdc++.so.6.0.36')))
    (work/'adapter.cpp').write_text('''#include <numeric>
#include <string>
#include <vector>
extern "C" int aetheri_tool_proof(int value) { std::vector<int> v{value,value};
std::string s="adapter"; return s.size()==7?std::accumulate(v.begin(),v.end(),0):-1; }
''')
    (work/'c_adapter.c').write_text('''#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
int main(int argc,char **argv) { if(argc!=2) return 1;
void *lib=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL); if(!lib) return 2;
void *symbol=dlsym(lib,"aetheri_tool_proof"); int (*fn)(int)=NULL;
_Static_assert(sizeof(fn)==sizeof(symbol),"POSIX pointer representation");
memcpy(&fn,&symbol,sizeof(fn)); if(!fn||fn(21)!=42) return 3;
if(dlclose(lib)) { return 4; } puts("C_CXX_SHARED_ADAPTER_OK");return 0;}
''')
    env=dict(PATH=str(prefix/'bin')+':/usr/bin:/usr/sbin:/bin:/sbin',HOME='/home/aetheri',LC_ALL='C',PYTHONDONTWRITEBYTECODE='1',CC='/usr/bin/gcc',CXX=str(prefix/'bin/c++'))
    rows = []

    def run(name, args):
        p = subprocess.run(args, env=env, capture_output=True)
        for suffix, raw in [('stdout', p.stdout), ('stderr', p.stderr)]:
            with (work/(name+'.'+suffix)).open('xb') as f:
                f.write(raw)
        rows.append(dict(name=name, arguments=args, exit=p.returncode))
        print(json.dumps(rows[-1]), flush=True)
        if p.returncode:
            raise SystemExit(p.returncode)
        return p.stdout.decode()

    for tool in ['cmake', 'ctest', 'ninja', 'c++']:
        run('version-'+tool.replace('+', 'p'), [str(prefix/'bin'/tool), '--version'])
    run('configure',[str(prefix/'bin/cmake'),'-S',str(work),'-B',str(work/'build'),'-G','Ninja','-DCMAKE_BUILD_TYPE=Release','-DCMAKE_EXPORT_COMPILE_COMMANDS=ON','-DCMAKE_INSTALL_PREFIX='+str(work/'install'),'-DCMAKE_INSTALL_RPATH='+str(prefix/'usr/lib64')])
    run('build',[str(prefix/'bin/cmake'),'--build',str(work/'build'),'-j2'])
    listed = json.loads(run('test-inventory', [str(prefix/'bin/ctest'), '--test-dir', str(work/'build'), '--show-only=json-v1']))
    assert {t['name'] for t in listed['tests']} == {'c17', 'cxx23', 'c_adapter'}
    tested = run('tests',[str(prefix/'bin/ctest'),'--test-dir',str(work/'build'),'--output-on-failure','-V'])
    assert '100% tests passed, 0 tests failed out of 3' in tested
    run('install',[str(prefix/'bin/cmake'),'--install',str(work/'build')])
    runtime=env|{'LD_LIBRARY_PATH':str(prefix/'usr/lib64')}
    for n, marker in [('c17', b'C17_OK'), ('cxx23', b'CXX23_EXCEPTION_THREAD_VECTOR_PRIVATE_RUNTIME_OK')]:
        p = subprocess.run([str(work/'install/bin'/n)], env=runtime, capture_output=True)
        assert p.returncode == 0 and marker in p.stdout, (n, p.stdout, p.stderr)
        (work/(n+'-installed.stdout')).write_bytes(p.stdout)
    closure = {}
    loader = '/lib64/ld-linux-x86-64.so.2'
    for n,p in [('cmake',prefix/'usr/bin/cmake'),('ctest',prefix/'usr/bin/ctest'),('ninja',prefix/'usr/bin/ninja'),('gpp',prefix/'usr/bin/g++'),('cc1plus',prefix/'usr/libexec/gcc/x86_64-redhat-linux/16/cc1plus'),('cxx23',work/'install/bin/cxx23'),('adapter',work/'install/lib/libadapter.so')]:
        text = run('loader-'+n, [loader, '--library-path', str(prefix/'usr/lib64'), '--list', str(p)])
        for line in text.splitlines():
            pieces = line.strip().split()
            selected = pieces[2] if len(pieces) > 2 and pieces[1] == '=>' else pieces[0]
            if selected.startswith('/'):
                selected = Path(selected).resolve()
                raw = selected.read_bytes()
                closure[str(selected)] = dict(size=len(raw), sha256=hashlib.sha256(raw).hexdigest())
    assert str(prefix/'usr/lib64/libstdc++.so.6.0.36') in closure
    assert not any(n.endswith('libstdc++.so.6.0.35') for n in closure)
    helpers = {}
    for n in ['/usr/bin/gcc', '/usr/libexec/gcc/x86_64-redhat-linux/16/cc1',
              '/usr/libexec/gcc/x86_64-redhat-linux/16/collect2',
              '/usr/libexec/gcc/x86_64-redhat-linux/16/liblto_plugin.so',
              '/usr/lib/gcc/x86_64-redhat-linux/16/crtbeginS.o', '/usr/bin/as', '/usr/bin/ld']:
        p = Path(n).resolve()
        raw = p.read_bytes()
        helpers[n] = dict(resolved=str(p), size=len(raw), sha256=hashlib.sha256(raw).hexdigest())
    result=dict(rows=rows,closure=closure,native_helpers=helpers,check=tools.check(prefix),build_type='Release',scope='native compiler/tool proof only; not full SDK or desktop acceptance')
    with (work/'result.json').open('x') as f:json.dump(result,f,indent=2)
    print(json.dumps(dict(result=str(work/'result.json'),rows=len(rows),closure_files=len(closure))),flush=True)


if __name__ == "__main__":
    main()
