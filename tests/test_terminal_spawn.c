#define _GNU_SOURCE
#include "terminal/spawn.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
/* Interpose only this subject's ioctl call sites, not sanitizer/libc calls. */
static bool deny_info;
static int subject_ioctl(int fd,unsigned long request,...);
#define ioctl subject_ioctl
#define ae_terminal_spawner_prepare fault_prepare
#define ae_terminal_spawner_open fault_open
#define ae_terminal_spawner_close fault_close
#include "../src/terminal/spawn.c"
#undef ioctl
#undef ae_terminal_spawner_prepare
#undef ae_terminal_spawner_open
#undef ae_terminal_spawner_close
static int subject_ioctl(int fd,unsigned long request,...) {
    va_list args;va_start(args,request);int result;
    if(request==TIOCGPTPEER) result=ioctl(fd,request,va_arg(args,int));
    else {void *p=va_arg(args,void *);if(request==PIDFD_GET_INFO && deny_info) {errno=EACCES;result=-1;}else result=ioctl(fd,request,p);}
    va_end(args);return result;
}
#define CHECK(x) do {if(!(x)) {fprintf(stderr,"terminal spawn %d: %s (%s)\n",__LINE__,#x,strerror(errno));exit(1);}}while(0)
static struct ae_terminal_pty children[3];
static atomic_bool stop_worker;
static void cleanup(void) {
    for(unsigned i=0;i<3;++i) {
        struct ae_terminal_pty *p=&children[i];if(p->pin<0) continue;
        (void)ae_terminal_pty_hangup(p);
        while(!p->ended || !p->eof) {
            struct pollfd f[]={{p->eof?-1:p->master,POLLIN,0},{p->ended?-1:p->pin,POLLIN,0}};
            int n;do {n=poll(f,2,8000);}while(n<0 && errno==EINTR);if(n<=0) break;
            char data[4096];if(f[0].revents) (void)ae_terminal_pty_read(p,data,sizeof(data));
            if(f[1].revents) (void)ae_terminal_pty_observe(p);
        }
        (void)ae_terminal_pty_close(p);
    }
    atomic_store(&stop_worker,true);
}
static void *worker(void *unused) {
    (void)unused;while(!atomic_load(&stop_worker)) {
        char *p=malloc(1024);CHECK(p);memset(p,7,1024);free(p);struct timespec pause={0,1000000};nanosleep(&pause,NULL);
    }return NULL;
}
static double now(void) {struct timespec t;CHECK(!clock_gettime(CLOCK_MONOTONIC,&t));return (double)t.tv_sec+(double)t.tv_nsec/1e9;}
static void receive(struct ae_terminal_pty *p,const char *wanted,const char *absent) {
    char data[16384]={0};size_t used=0;double end=now()+8;
    while(!strstr(data,wanted)) {
        CHECK(now()<end && used<sizeof(data)-1);
        struct pollfd f={p->master,POLLIN,0};CHECK(poll(&f,1,100)>=0);
        ssize_t n=ae_terminal_pty_read(p,data+used,sizeof(data)-1-used);
        if(n<0 && (errno==EAGAIN || errno==EINTR)) continue;
        CHECK(n>0);used+=(size_t)n;data[used]=0;
    }
    CHECK(!absent || !strstr(data,absent));
}
static void command(struct ae_terminal_pty *p,const char *text) {size_t n=strlen(text);CHECK(write(p->master,text,n)==(ssize_t)n);}
static void settle(struct ae_terminal_pty *p) {
    double end=now()+8;
    while(!p->ended || !p->eof) {
        CHECK(now()<end);struct pollfd f[]={{p->eof?-1:p->master,POLLIN,0},{p->ended?-1:p->pin,POLLIN,0}};CHECK(poll(f,2,100)>=0);
        char data[4096];if(f[0].revents) {ssize_t n=ae_terminal_pty_read(p,data,sizeof(data));CHECK(n>=0 || errno==EAGAIN || errno==EINTR);}
        if(f[1].revents) CHECK(ae_terminal_pty_observe(p)>=0);
    }
}
static unsigned descriptors(void) {
    DIR *dir=opendir("/proc/self/fd");CHECK(dir);unsigned n=0;struct dirent *entry;
    while((entry=readdir(dir))) if(entry->d_name[0]!='.') ++n;
    CHECK(!closedir(dir));return n;
}
int main(int argc,char **argv) {
    CHECK(argc==3 && getuid()!=0);
    for(unsigned i=0;i<3;++i) children[i]=(struct ae_terminal_pty){.master=-1,.pin=-1,.pid=-1};
    CHECK(!atexit(cleanup));struct sigaction action={.sa_handler=SIG_DFL};sigemptyset(&action.sa_mask);CHECK(!sigaction(SIGCHLD,&action,NULL));
    unsigned initial=descriptors();int helper=open(argv[1],O_RDONLY|O_CLOEXEC),shell=open(argv[2],O_RDONLY|O_CLOEXEC);CHECK(helper>=3 && shell>=3);
    char variable[]="AETHERI_SPAWN_TEST=before";
    char *env[]={variable,"PATH=/usr/bin:/bin","WAYLAND_SOCKET=77","COLORTERM=truecolor","TERM=wrong",NULL};
    struct ae_terminal_spawner spawner=AE_TERMINAL_SPAWNER_INIT;
    CHECK(!ae_terminal_spawner_prepare(&spawner,helper,shell,env,true));
    CHECK(ae_terminal_spawner_prepare(&spawner,helper,shell,env,true)<0);strcpy(variable,"AETHERI_SPAWN_TEST=after");
    for(size_t i=0;spawner.environment[i];++i) CHECK(strncmp(spawner.environment[i],"WAYLAND_SOCKET=",15) && strncmp(spawner.environment[i],"COLORTERM=",10));
    close(helper);close(shell);int marker[2];CHECK(!pipe(marker));
    pthread_t thread;CHECK(!pthread_create(&thread,NULL,worker,NULL));
    CHECK(ae_terminal_spawner_open(&spawner,&children[0],1,24)<0 && children[0].pin==-1);
    CHECK(!ae_terminal_spawner_open(&spawner,&children[0],80,24));
    CHECK(!ae_terminal_spawner_open(&spawner,&children[1],80,24));
    receive(&children[0],"aetheri> ",NULL);receive(&children[1],"aetheri> ",NULL);
    CHECK(children[0].pid!=children[1].pid && children[0].pidfd_wait && children[1].pidfd_wait);
    for(unsigned i=0;i<2;++i) {
        CHECK(getsid(children[i].pid)==children[i].pid && tcgetsid(children[i].master)==children[i].pid);
        char path[96];int n=snprintf(path,sizeof(path),"/proc/%ld/exe",(long)children[i].pid);CHECK(n>0 && (size_t)n<sizeof(path));
        struct stat expected,actual;CHECK(!fstat(spawner.source,&expected) && !stat(path,&actual));CHECK(expected.st_dev==actual.st_dev && expected.st_ino==actual.st_ino);
    }
    command(&children[0],"printf '%s\\n' \"$AETHERI_SPAWN_TEST\"\r");receive(&children[0],"before\r\n","after\r\n");
    command(&children[1],"printf 'SECOND_SESSION\\n'\r");receive(&children[1],"SECOND_SESSION\r\n","before\r\n");
    /* External canonical programs need the same UTF-8 erase policy as the
       original pre-thread PTY. Own a fresh file, then exercise actual N_TTY
       multibyte erase; shell line editing must not stand in for this check. */
    CHECK(!ae_terminal_spawner_open(&spawner,&children[2],80,24));receive(&children[2],"aetheri> ",NULL);
    struct termios initial_modes;CHECK(!tcgetattr(children[2].master,&initial_modes));
    printf("CANONICAL_UTF8 enabled=%d\n",(initial_modes.c_iflag&IUTF8)!=0);fflush(stdout);
    char cooked_name[]="terminal-cooked-XXXXXX",cooked_command[128];
    int cooked=mkstemp(cooked_name);CHECK(cooked>=3);
    int length=snprintf(cooked_command,sizeof(cooked_command),"cat > %s\r",cooked_name);
    CHECK(length>0 && (size_t)length<sizeof(cooked_command));command(&children[2],cooked_command);
    unsigned attempts=0;
    while(tcgetpgrp(children[2].master)==children[2].pid && attempts++<5000) {
        struct timespec delay={.tv_nsec=1000000};CHECK(!nanosleep(&delay,NULL));
    }
    CHECK(attempts<5000 && tcgetpgrp(children[2].master)>0);
    command(&children[2],"\xc3\xa9\x7f\r\x04");
    attempts=0;
    while(tcgetpgrp(children[2].master)!=children[2].pid && attempts++<5000) {
        struct timespec delay={.tv_nsec=1000000};CHECK(!nanosleep(&delay,NULL));
    }
    CHECK(attempts<5000);
    char cooked_bytes[8];ssize_t cooked_size=pread(cooked,cooked_bytes,sizeof(cooked_bytes),0);
    CHECK(cooked_size==1 && cooked_bytes[0]=='\n');CHECK(!close(cooked) && !unlink(cooked_name));
    command(&children[2],"exit 0\r");settle(&children[2]);CHECK(!children[2].status && !ae_terminal_pty_close(&children[2]));
    children[2]=(struct ae_terminal_pty){.master=-1,.pin=-1,.pid=-1};
    int writer=ioctl(children[0].master,TIOCGPTPEER,O_RDWR|O_NOCTTY|O_CLOEXEC);CHECK(writer>=0);
    command(&children[0],"exit 0\r");struct pollfd pin={children[0].pin,POLLIN,0};CHECK(poll(&pin,1,8000)==1);
    CHECK(ae_terminal_pty_observe(&children[0])==1 && children[0].ended && !children[0].eof);
    CHECK(ae_terminal_pty_close(&children[0])<0 && errno==EBUSY);close(writer);settle(&children[0]);CHECK(!children[0].status && !ae_terminal_pty_close(&children[0]));
    command(&children[1],"printf 'SURVIVOR\\n'\r");receive(&children[1],"SURVIVOR\r\n",NULL);
    command(&children[1],"exit 0\r");settle(&children[1]);CHECK(!children[1].status && !ae_terminal_pty_close(&children[1]));
    deny_info=true;CHECK(fault_open(&spawner,&children[2],80,24)<0 && children[2].pin>=0 && !children[2].pid);
    CHECK(ae_terminal_pty_close(&children[2])<0 && errno==EBUSY);
    CHECK(!ae_terminal_pty_hangup(&children[2]));settle(&children[2]);CHECK(!ae_terminal_pty_close(&children[2]));
    deny_info=false;CHECK(ae_terminal_spawner_open(&spawner,&children[2],80,24)<0 && errno==EINVAL);
    ae_terminal_spawner_close(&spawner);
    children[2]=(struct ae_terminal_pty){.master=-1,.pin=-1,.pid=-1};
    helper=open(argv[1],O_RDONLY|O_CLOEXEC);shell=open(argv[2],O_RDONLY|O_CLOEXEC);CHECK(helper>=3 && shell>=3);
    char *large=malloc(16*1024+2);CHECK(large);memset(large,'x',16*1024+1);large[16*1024+1]=0;
    char *oversized[]={large,NULL};CHECK(ae_terminal_spawner_prepare(&spawner,helper,shell,oversized,false)<0 && errno==E2BIG && spawner.helper==-1 && !spawner.environment);free(large);
    char bad_name[]="terminal-spawn-invalid-XXXXXX";int file=mkstemp(bad_name);CHECK(file>=3);
    CHECK(write(file,"not an ELF",10)==10 && !fchmod(file,0700) && !close(file));
    file=open(bad_name,O_RDONLY|O_CLOEXEC);CHECK(file>=3 && !unlink(bad_name));
    /* Invalid bootstrap exec must leave no child or descriptor obligation. */
    CHECK(!ae_terminal_spawner_prepare(&spawner,file,shell,env,false));
    for(unsigned i=0;i<4;++i) CHECK(ae_terminal_spawner_open(&spawner,&children[2],80,24)<0 && errno==ENOEXEC && children[2].pin==-1 && children[2].master==-1);
    CHECK(!fchmod(file,0600));
    CHECK(ae_terminal_spawner_open(&spawner,&children[2],80,24)<0 && errno==ESTALE && children[2].pin==-1 && children[2].master==-1);
    CHECK(!fchmod(file,0700));
    ae_terminal_spawner_close(&spawner);
    /* An invalid target AFTER the bootstrap exec is a retained failed child. */
    CHECK(!ae_terminal_spawner_prepare(&spawner,helper,file,env,false));
    int opened=ae_terminal_spawner_open(&spawner,&children[2],80,24);CHECK(opened==0 || children[2].pin>=0);
    settle(&children[2]);CHECK(children[2].status==126 && !ae_terminal_pty_close(&children[2]));
    ae_terminal_spawner_close(&spawner);close(helper);close(shell);close(file);
    atomic_store(&stop_worker,true);CHECK(!pthread_join(thread,NULL));close(marker[0]);close(marker[1]);
    siginfo_t unused={0};CHECK(waitid(P_ALL,0,&unused,WEXITED|WNOHANG)<0 && errno==ECHILD);
    CHECK(descriptors()==initial);
    puts("terminal spawn: post-thread independent controlling PTYs, canonical UTF-8 erase, fixed original images/environment, retained writer, post-dispatch failure ownership and exact FD cleanup passed");return 0;
}
