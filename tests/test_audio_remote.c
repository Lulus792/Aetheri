#define _GNU_SOURCE
#include "audio/remote.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

static bool deny_peer_pidfd;
int __real_getsockopt(int,int,int,void *,socklen_t *);
int __wrap_getsockopt(int fd,int level,int option,void *value,socklen_t *size) {
    if(deny_peer_pidfd && level==SOL_SOCKET && option==SO_PEERPIDFD) {errno=ENOPROTOOPT;return -1;}
    return __real_getsockopt(fd,level,option,value,size);
}
struct actor {pid_t pid;int pin,events;};
static bool event(int fd,char expected) {
    struct pollfd p={fd,POLLIN,0};char value;
    return poll(&p,1,1000)==1 && read(fd,&value,1)==1 && value==expected;
}
static int listener(const char *path) {
    struct sockaddr_un address={.sun_family=AF_UNIX};
    if(strlen(path)>=sizeof(address.sun_path)) return -1;
    memcpy(address.sun_path,path,strlen(path)+1);
    int fd=socket(AF_UNIX,SOCK_STREAM|SOCK_CLOEXEC|SOCK_NONBLOCK,0);if(fd<0) return -1;
    if(bind(fd,(struct sockaddr *)&address,(socklen_t)(offsetof(struct sockaddr_un,sun_path)+strlen(path)+1)) || listen(fd,8)) {close(fd);return -1;}
    return fd;
}
static void serve(const char *path,char marker,int inherited) {
    int server=inherited>=0 ? 4 : listener(path);if(server<0) _exit(125);
    if(write(3,"R",1)!=1) _exit(125);
    int clients[8];for(unsigned i=0;i<8;++i) clients[i]=-1;
    for(;;) {
        struct pollfd p[9]={{server,POLLIN,0}};
        for(unsigned i=0;i<8;++i) p[i+1]=(struct pollfd){clients[i],POLLIN,0};
        if(poll(p,9,-1)<0) {if(errno==EINTR) continue;_exit(125);}
        if(p[0].revents&POLLIN) {
            int fd=accept4(server,NULL,NULL,SOCK_CLOEXEC|SOCK_NONBLOCK);if(fd<0) _exit(125);
            unsigned i=0;for(;i<8 && clients[i]>=0;++i) {}if(i==8) _exit(125);clients[i]=fd;
        }
        for(unsigned i=0;i<8;++i) if(p[i+1].revents&(POLLIN|POLLHUP)) {
            char value;ssize_t n=read(clients[i],&value,1);
            if(n==1) {if(value!='P' || write(clients[i],&marker,1)!=1) _exit(125);}
            else if(n==0) {close(clients[i]);clients[i]=-1;if(write(3,"E",1)!=1) _exit(125);}
            else if(errno!=EAGAIN && errno!=EINTR) _exit(125);
        }
    }
}
static bool spawn(struct actor *out,const char *path,char marker,int inherited) {
    int pipe[2];if(pipe2(pipe,O_CLOEXEC)) return false;
    pid_t pid=fork();if(pid<0) {close(pipe[0]);close(pipe[1]);return false;}
    if(!pid) {
        int held=inherited<0 ? -1 : fcntl(inherited,F_DUPFD_CLOEXEC,10);
        if(inherited>=0 && held<0) _exit(125);
        if(pipe[1]!=3 && dup3(pipe[1],3,0)<0) _exit(125);
        if(held>=0 && dup3(held,4,0)<0) _exit(125);
        if(syscall(SYS_close_range,held>=0 ? 5u : 4u,~0u,0u)) _exit(125);
        serve(path,marker,held);_exit(125);
    }
    close(pipe[1]);*out=(struct actor){pid,(int)syscall(SYS_pidfd_open,pid,0),pipe[0]};
    return out->pin>=0 && event(out->events,'R');
}
static bool stop(struct actor *a) {
    if(a->pid<=0) return true;
    if(a->pin>=0) (void)syscall(SYS_pidfd_send_signal,a->pin,SIGKILL,NULL,0);
    else (void)kill(a->pid,SIGKILL); /* Our unreaped direct child, not lookup. */
    int status;pid_t r;do {r=waitpid(a->pid,&status,0);} while(r<0 && errno==EINTR);
    if(a->pin>=0) close(a->pin);
    if(a->events>=0) close(a->events);
    a->pid=-1;a->pin=a->events=-1;return r>0 && WIFSIGNALED(status) && WTERMSIG(status)==SIGKILL;
}
static bool exchange(int fd,char expected) {return write(fd,"P",1)==1 && event(fd,expected);}
static int count_fds(void) {
    DIR *d=opendir("/proc/self/fd");if(!d) return -1;
    int n=0;while(readdir(d)) ++n;closedir(d);return n;
}
#define CHECK(x) do {if(!(x)) {fprintf(stderr,"audio peer failed line %d: %s errno=%d\n",__LINE__,#x,errno);goto done;}} while(0)
int main(int argc,char **argv) {
    struct actor original={-1,-1,-1},impostor={-1,-1,-1},inherited={-1,-1,-1};
    int good=-1,other=-1,activated=-1,dead=-1,pipes[2]={-1,-1};bool okay=false;
    char directory[256]={0},path[108]={0},activation[108]={0};
    if(argc!=2) return 2;
    int n=snprintf(directory,sizeof(directory),"%s/peer-XXXXXX",argv[1]);
    if(n<0 || (size_t)n>=sizeof(directory) || !mkdtemp(directory)) return 1;
    n=snprintf(path,sizeof(path),"%s/socket",directory);if(n<=0 || (size_t)n>=sizeof(path)) {path[0]=0;goto done;}
    n=snprintf(activation,sizeof(activation),"%s/inherited",directory);if(n<=0 || (size_t)n>=sizeof(activation)) {activation[0]=0;goto done;}
    CHECK(spawn(&original,path,'A',-1));
    int before=count_fds();CHECK(before>0);
    good=ae_audio_remote_connect_original(path,original.pin);CHECK(good>=0);
    CHECK((fcntl(good,F_GETFD)&FD_CLOEXEC) && (fcntl(good,F_GETFL)&O_NONBLOCK) && exchange(good,'A'));
    deny_peer_pidfd=true;other=ae_audio_remote_connect_original(path,original.pin);CHECK(other<0 && errno==ENOPROTOOPT);deny_peer_pidfd=false;
    CHECK(event(original.events,'E')); /* Refusal sent no application bytes. */
    CHECK(pipe2(pipes,O_CLOEXEC)==0);other=ae_audio_remote_connect_original(path,pipes[0]);CHECK(other<0);close(pipes[0]);close(pipes[1]);pipes[0]=pipes[1]=-1;
    CHECK(unlink(path)==0 && spawn(&impostor,path,'B',-1));
    for(unsigned i=0;i<8;++i) {
        other=ae_audio_remote_connect_original(path,original.pin);CHECK(other<0 && errno==ESTALE);
        CHECK(event(impostor.events,'E'));
    }
    CHECK(exchange(good,'A')); /* Established original FD ignores path takeover. */
    other=ae_audio_remote_connect(path);CHECK(other>=0 && exchange(other,'B'));close(other);other=-1;
    CHECK(event(impostor.events,'E')); /* Same-UID-only counterexample. */
    dead=fcntl(original.pin,F_DUPFD_CLOEXEC,3);CHECK(dead>=0 && stop(&original));
    other=ae_audio_remote_connect_original(path,dead);CHECK(other<0);
    other=ae_audio_remote_connect_original(path,impostor.pin);CHECK(other>=0 && exchange(other,'B'));close(other);other=-1;
    CHECK(event(impostor.events,'E'));
    activated=listener(activation);CHECK(activated>=0 && spawn(&inherited,activation,'C',activated));close(activated);activated=-1;
    other=ae_audio_remote_connect_original(activation,inherited.pin);CHECK(other<0 && errno==ESTALE && event(inherited.events,'E'));
    other=ae_audio_remote_connect(activation);CHECK(other>=0 && exchange(other,'C'));close(other);other=-1;
    CHECK(event(inherited.events,'E'));
    close(good);good=-1;close(dead);dead=-1;
    CHECK(stop(&impostor) && stop(&inherited));
    CHECK(count_fds()==before-2); /* Original actor's pin + event FD retired. */
    okay=true;
done:
    if(good>=0) close(good);
    if(other>=0) close(other);
    if(activated>=0) close(activated);
    if(dead>=0) close(dead);
    for(unsigned i=0;i<2;++i) if(pipes[i]>=0) close(pipes[i]);
    bool a=stop(&original),b=stop(&impostor),c=stop(&inherited);
    if(!a || !b || !c) okay=false;
    if(*path) (void)unlink(path);
    if(*activation) (void)unlink(activation);
    if(rmdir(directory)) okay=false;
    if(okay) puts("audio original peer: kernel join, takeover, stale pin, unsupported option, inherited listener and FD retirement passed");
    return okay ? 0 : 1;
}
