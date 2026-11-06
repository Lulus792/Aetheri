#define _GNU_SOURCE
#include "spawn.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <spawn.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/pidfd.h>
#include <sys/stat.h>
#include <unistd.h>
enum { ENV_COUNT=1024,ENV_BYTES=256*1024,ENV_STRING=16*1024 };
static bool skip(const char *s) {return !strncmp(s,"WAYLAND_SOCKET=",15) || !strncmp(s,"COLORTERM=",10) || !strncmp(s,"TERM=",5);}
static bool stable(int fd,const struct stat *expected) {
    struct stat now;if(fstat(fd,&now)) return false;
    if(now.st_dev!=expected->st_dev || now.st_ino!=expected->st_ino || now.st_size!=expected->st_size || now.st_mode!=expected->st_mode ||
       now.st_uid!=expected->st_uid || now.st_gid!=expected->st_gid || now.st_mtim.tv_sec!=expected->st_mtim.tv_sec ||
       now.st_mtim.tv_nsec!=expected->st_mtim.tv_nsec || now.st_ctim.tv_sec!=expected->st_ctim.tv_sec || now.st_ctim.tv_nsec!=expected->st_ctim.tv_nsec) {errno=ESTALE;return false;}
    return true;
}
void ae_terminal_spawner_close(struct ae_terminal_spawner *s) {
    if(!s) return;
    if(s->helper>=0) close(s->helper);
    if(s->source>=0) close(s->source);
    free(s->environment);free(s->bytes);*s=(struct ae_terminal_spawner)AE_TERMINAL_SPAWNER_INIT;
}
int ae_terminal_spawner_prepare(struct ae_terminal_spawner *s,int helper,int source,char *const environment[],bool welcome) {
    if(!s || s->helper!=-1 || s->source!=-1 || s->environment || s->bytes || helper<3 || source<3 || !environment ||
       getuid()==0 || getuid()!=geteuid() || getgid()!=getegid()) {errno=EINVAL;return -1;}
    int inputs[]={helper,source};struct stat states[2];
    for(unsigned i=0;i<2;++i) {
        int flags=fcntl(inputs[i],F_GETFL);
        if(flags<0 || (flags&O_ACCMODE)!=O_RDONLY || (flags&O_PATH) || fstat(inputs[i],&states[i]) || !S_ISREG(states[i].st_mode) ||
           (states[i].st_mode&(S_ISUID|S_ISGID))) {errno=EINVAL;return -1;}
    }
    size_t count=0,total=sizeof("TERM=dumb");
    for(;count<ENV_COUNT && environment[count];++count) {
        size_t n=strnlen(environment[count],ENV_STRING+1);
        if(n>ENV_STRING || n+1>ENV_BYTES-total) {errno=E2BIG;return -1;}
        total+=n+1;
    }
    if(count==ENV_COUNT) {errno=E2BIG;return -1;}
    struct ae_terminal_spawner next=AE_TERMINAL_SPAWNER_INIT;next.welcome=welcome;
    next.environment=calloc(count+2,sizeof(*next.environment));next.bytes=malloc(total);
    if(!next.environment || !next.bytes) goto failed;
    size_t at=0,used=0;
    for(size_t i=0;i<count;++i) if(!skip(environment[i])) {
        size_t n=strlen(environment[i])+1;memcpy(next.bytes+used,environment[i],n);
        next.environment[at++]=next.bytes+used;used+=n;
    }
    memcpy(next.bytes+used,"TERM=dumb",sizeof("TERM=dumb"));next.environment[at]=next.bytes+used;
    next.helper=fcntl(helper,F_DUPFD_CLOEXEC,10);if(next.helper<0) goto failed;
    next.source=fcntl(source,F_DUPFD_CLOEXEC,10);if(next.source<0) goto failed;
    if(!stable(next.helper,&states[0]) || !stable(next.source,&states[1])) goto failed;
    next.helper_state=states[0];next.source_state=states[1];
    *s=next;return 0;
failed: {
        int saved=errno;ae_terminal_spawner_close(&next);errno=saved;return -1;
    }
}
int ae_terminal_spawner_open(const struct ae_terminal_spawner *s,struct ae_terminal_pty *p,unsigned cols,unsigned rows) {
    if(!s || !p || s->helper<10 || s->source<10 || !s->environment || !s->bytes ||
       p->master!=-1 || p->pin!=-1 || p->pid!=-1 || cols<2 || cols>240 || rows<2 || rows>100 ||
       getuid()==0 || getuid()!=geteuid() || getgid()!=getegid()) {errno=EINVAL;return -1;}
    struct sigaction inherited;if(sigaction(SIGCHLD,NULL,&inherited)) return -1;
    if(inherited.sa_handler!=SIG_DFL || (inherited.sa_flags&SA_NOCLDWAIT)) {errno=EINVAL;return -1;}
    for(int i=0;i<3;++i) if(fcntl(i,F_GETFD)<0) return -1;
    if(!stable(s->helper,&s->helper_state) || !stable(s->source,&s->source_state)) return -1;
    int master=posix_openpt(O_RDWR|O_NOCTTY|O_NONBLOCK|O_CLOEXEC),slave=-1;
    if(master<0) return -1;
    if(grantpt(master) || unlockpt(master)) goto failed;
    slave=ioctl(master,TIOCGPTPEER,O_RDWR|O_NOCTTY|O_CLOEXEC);if(slave<0) goto failed;
    if(ae_terminal_pty_configure_slave(slave)) goto failed;
    struct winsize size={.ws_col=(unsigned short)cols,.ws_row=(unsigned short)rows};if(ioctl(slave,TIOCSWINSZ,&size)) goto failed;
    posix_spawn_file_actions_t files;posix_spawnattr_t attr;
    bool files_ready=false,attr_ready=false;int error=posix_spawn_file_actions_init(&files);
    if(!error) {files_ready=true;error=posix_spawnattr_init(&attr);}
    if(!error) attr_ready=true;
    for(int i=0;i<3 && !error;++i) error=posix_spawn_file_actions_adddup2(&files,slave,i);
    if(!error) error=posix_spawn_file_actions_adddup2(&files,s->source,3);
    if(!error) error=posix_spawn_file_actions_adddup2(&files,s->helper,4);
    if(!error) error=posix_spawn_file_actions_addclosefrom_np(&files,5);
    sigset_t mask,defaults;sigemptyset(&mask);sigemptyset(&defaults);
    const int signals[]={SIGCHLD,SIGINT,SIGQUIT,SIGTSTP,SIGTTIN,SIGTTOU,SIGHUP,SIGTERM,SIGPIPE};
    for(size_t i=0;i<sizeof(signals)/sizeof(signals[0]);++i) sigaddset(&defaults,signals[i]);
    if(!error) error=posix_spawnattr_setsigmask(&attr,&mask);
    if(!error) error=posix_spawnattr_setsigdefault(&attr,&defaults);
    if(!error) error=posix_spawnattr_setflags(&attr,POSIX_SPAWN_SETSID|POSIX_SPAWN_SETSIGMASK|POSIX_SPAWN_SETSIGDEF);
    char *args[]={"aetheri-terminal-pty-starter",s->welcome?"--welcome":"--quiet",NULL};
    int pin=-1;if(!error) error=pidfd_spawn(&pin,"/proc/self/fd/4",&files,&attr,args,s->environment);
    if(attr_ready) posix_spawnattr_destroy(&attr);
    if(files_ready) posix_spawn_file_actions_destroy(&files);
    if(error) {errno=error;goto failed;}
    close(slave);slave=-1;
    /* Publish obligations BEFORE any observation which can fail. The caller
       retains this exact attempt even if the helper exited before this ioctl. */
    *p=(struct ae_terminal_pty){.master=master,.pin=pin,.pid=0,.pidfd_wait=true};
    if(!stable(s->helper,&s->helper_state) || !stable(s->source,&s->source_state)) return -1;
    struct pidfd_info info={.mask=PIDFD_INFO_PID};
    if(ioctl(pin,PIDFD_GET_INFO,&info)) return -1;
    if(!(info.mask&PIDFD_INFO_PID) || !info.pid || info.pid>INT_MAX || info.tgid!=info.pid || info.ppid!=(uint32_t)getpid()) {errno=EIO;return -1;}
    p->pid=(pid_t)info.pid;return 0;
failed: {
        int saved=errno;if(slave>=0) close(slave);close(master);errno=saved;return -1;
    }
}
