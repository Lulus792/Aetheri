#define AETHERI_AUDIO_SESSION_ENTRY ae_audio_session_fixture_main
#include "../src/boot/audio_session.c"
#include <stdarg.h>

/* Redirect only the fixed fixture barrier directory into this test's own
 * mkdtemp. The privileged fixture main and login/device paths are never run. */
static char directory[1024];
static bool inspect_write,partial_hidden;
int __real_open(const char *,int,...);
ssize_t __real_write(int,const void *,size_t);
int __real_renameat2(int,const char *,int,const char *,unsigned);
static const char *redirect(const char *name,char path[1280]) {
    const char prefix[]="/run/aetheri-audio/";
    if(strncmp(name,prefix,sizeof(prefix)-1)) return name;
    int n=snprintf(path,1280,"%s/%s",directory,name+sizeof(prefix)-1);
    return n>0 && n<1280 ? path : NULL;
}
int __wrap_open(const char *name,int flags,...) {
    char path[1280];const char *mapped=redirect(name,path);if(!mapped) {errno=ENAMETOOLONG;return -1;}
    mode_t mode=0;
    if(flags&O_CREAT) {va_list args;va_start(args,flags);mode=(mode_t)va_arg(args,int);va_end(args);}
    return __real_open(mapped,flags,mode);
}
ssize_t __wrap_write(int fd,const void *bytes,size_t size) {
    if(inspect_write && size==2 && !memcmp(bytes,"1\n",2)) {
        char path[1280],value[8];struct stat st;
        int n=snprintf(path,sizeof(path),"%s/ready",directory);
        partial_hidden=n>0 && (size_t)n<sizeof(path) && lstat(path,&st)<0 && errno==ENOENT && !read_text("/run/aetheri-audio/ready",value,sizeof(value));
    }
    return __real_write(fd,bytes,size);
}
int __wrap_renameat2(int oldfd,const char *old,int newfd,const char *name,unsigned flags) {
    char source[1280],target[1280];const char *a=redirect(old,source),*b=redirect(name,target);
    if(!a || !b) {errno=ENAMETOOLONG;return -1;}
    return __real_renameat2(oldfd,a,newfd,b,flags);
}
#define CHECK(x) do {if(!(x)) {fprintf(stderr,"audio barrier failed line %d: %s\n",__LINE__,#x);goto done;}} while(0)
int main(int argc,char **argv) {
    bool okay=false;int fd=-1;char value[8];
    if(argc!=2) return 2;
    int n=snprintf(directory,sizeof(directory),"%s/barrier-XXXXXX",argv[1]);
    if(n<0 || (size_t)n>=sizeof(directory) || !mkdtemp(directory)) return 1;
    /* Reproduce the old reader's actual empty-file observation before a writer
     * has issued its write, rather than relying on a probabilistic race. */
    fd=__wrap_open("/run/aetheri-audio/legacy",O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC,0600);CHECK(fd>=0);
    CHECK(read_text("/run/aetheri-audio/legacy",value,sizeof(value)) && !*value);
    CHECK(!await_barrier("legacy",1));CHECK(close(fd)==0);fd=-1;
    inspect_write=true;CHECK(barrier("ready"));inspect_write=false;
    CHECK(partial_hidden && await_barrier("ready",1));
    CHECK(!barrier("ready")); /* No replacement of an already published marker. */
    CHECK(read_text("/run/aetheri-audio/ready",value,sizeof(value)) && !strcmp(value,"1\n"));okay=true;
done:
    if(fd>=0) close(fd);
    const char *names[]={"legacy","ready","ready.pending"};
    for(unsigned i=0;i<3;++i) {char path[1280];snprintf(path,sizeof(path),"%s/%s",directory,names[i]);(void)unlink(path);}
    if(rmdir(directory)) okay=false;
    if(okay) puts("audio barriers: empty-publication counterexample, atomic visibility and no-replace passed");
    return okay ? 0 : 1;
}
