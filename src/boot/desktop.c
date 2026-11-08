/* Fresh single-threaded, unprivileged development-image desktop owner.
 * This is not a production launch authority or restart/recovery protocol. */
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/pidfd.h>
#include <sys/signalfd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <systemd/sd-daemon.h>
#include <systemd/sd-login.h>
#include <time.h>
#include <unistd.h>
#include <linux/close_range.h>

#define BIN "/usr/libexec/aetheri/"
#define SANS "/usr/share/aetheri/fonts/LiberationSans-Regular.ttf"
#define MONO "/usr/share/fonts/liberation-mono-fonts/LiberationMono-Regular.ttf"
#define BOLD "/usr/share/fonts/liberation-mono-fonts/LiberationMono-Bold.ttf"
enum role { COMPOSITOR, DESKTOP, FILES, TERMINAL, SETTINGS, ROLE_COUNT };
static const char *const names[]={"compositor","desktop","files","terminal","settings"};
struct child {
    pid_t pid;
    int pin,output,status;
    bool reaped,eof,ready,layout,signalled;
    char line[65536];
    size_t used;
};
static void empty(struct child *c) { memset(c,0,sizeof(*c));c->pin=-1;c->output=-1; }
static bool close_fd(int *fd) { if(*fd<0)return true;int n=*fd;*fd=-1;return close(n)==0; }
static bool nonblock(int fd) { int flags=fcntl(fd,F_GETFL);return flags>=0 && fcntl(fd,F_SETFL,flags|O_NONBLOCK)==0; }
static double now(void) { struct timespec t;if(clock_gettime(CLOCK_MONOTONIC,&t))return -1;return (double)t.tv_sec+(double)t.tv_nsec/1e9; }

/* The child waits before target exec. Failed pin capture closes the sole release
 * writer and reaps this known no-exec child; it never signals a numeric PID. */
static bool spawn(struct child *c,char *const argv[],const int *pass,size_t count) {
    int logs[2],gate[2];
    if(c->pid || count>2 || (count && !pass))return false;
    for(int fd=0;fd<3;++fd)if(fcntl(fd,F_GETFD)<0)return false;
    for(size_t i=0;i<count;++i)if(pass[i]<3 || fcntl(pass[i],F_GETFD)<0)return false;
    if(pipe2(logs,O_CLOEXEC))return false;
    if(pipe2(gate,O_CLOEXEC)){close(logs[0]);close(logs[1]);return false;}
    pid_t pid=fork();
    if(pid<0){close(logs[0]);close(logs[1]);close(gate[0]);close(gate[1]);return false;}
    if(!pid){
        close(gate[1]);
        int null=open("/dev/null",O_RDONLY|O_CLOEXEC);
        if(null<0)_exit(125);
        int input=dup2(null,0),output=dup2(logs[1],1),error=dup2(logs[1],2);
        if(input<0 || output<0 || error<0 || close_range(3,~0u,CLOSE_RANGE_CLOEXEC))_exit(125);
        for(size_t i=0;i<count;++i)if(fcntl(pass[i],F_SETFD,0))_exit(125);
        char release=0,extra=0;ssize_t got;
        do{got=read(gate[0],&release,1);}while(got<0 && errno==EINTR);
        if(got!=1 || release!='A')_exit(125);
        do{got=read(gate[0],&extra,1);}while(got<0 && errno==EINTR);
        if(got!=0)_exit(125);
        sigset_t mask;sigemptyset(&mask);
        if(sigprocmask(SIG_SETMASK,&mask,NULL) || signal(SIGPIPE,SIG_DFL)==SIG_ERR)_exit(125);
        execv(argv[0],argv);_exit(126);
    }
    close(logs[1]);close(gate[0]);
    int pin=pidfd_open(pid,0);
    if(pin<0){
        close(gate[1]);close(logs[0]);
        int status;while(waitpid(pid,&status,0)<0 && errno==EINTR){}
        return false;
    }
    c->pid=pid;c->pin=pin;c->output=logs[0];
    bool ok=nonblock(c->output);
    if(ok){ssize_t n;do{n=write(gate[1],"A",1);}while(n<0 && errno==EINTR);ok=n==1;}
    if(close(gate[1]))ok=false;
    printf("SPAWN pid=%ld\n",(long)pid);
    return ok; /* A later failure retains c and its original pin/output. */
}

static int packet(int fd) {
    char bytes[8];union {struct cmsghdr align;char bytes[CMSG_SPACE(sizeof(int)*4)];} control={0};
    struct iovec iov={.iov_base=bytes,.iov_len=sizeof(bytes)};
    struct msghdr message={.msg_iov=&iov,.msg_iovlen=1,.msg_control=control.bytes,.msg_controllen=sizeof(control.bytes)};
    ssize_t n=recvmsg(fd,&message,MSG_CMSG_CLOEXEC);
    if(n<0 && (errno==EINTR || errno==EAGAIN || errno==EWOULDBLOCK))return 0;
    bool ancillary=false;
    for(struct cmsghdr *h=CMSG_FIRSTHDR(&message);n>=0 && h;h=CMSG_NXTHDR(&message,h)){
        ancillary=true;
        if(h->cmsg_level==SOL_SOCKET && h->cmsg_type==SCM_RIGHTS && h->cmsg_len>=CMSG_LEN(0)){
            size_t size=h->cmsg_len-CMSG_LEN(0);
            for(size_t i=0;i+sizeof(int)<=size;i+=sizeof(int)){int incoming;memcpy(&incoming,(char *)CMSG_DATA(h)+i,sizeof(incoming));close(incoming);}
        }
    }
    if(n!=3 || ancillary || message.msg_flags&(MSG_TRUNC|MSG_CTRUNC))return -1;
    if(!memcmp(bytes,"Q30",3))return 100;
    if(!memcmp(bytes,"L3S",3))return SETTINGS+1;
    if(!memcmp(bytes,"L3T",3))return TERMINAL+1;
    if(!memcmp(bytes,"L3F",3))return FILES+1;
    return -1;
}
static bool reply(int fd,char kind,enum role role) {
    char bytes[3]={kind,'3',role==SETTINGS?'S':role==TERMINAL?'T':'F'};
    return send(fd,bytes,sizeof(bytes),MSG_NOSIGNAL)==(ssize_t)sizeof(bytes);
}
static bool drain_line(struct child *c,const char *name) {
    char bytes[4096];ssize_t n=read(c->output,bytes,sizeof(bytes));
    if(n<0)return errno==EAGAIN || errno==EWOULDBLOCK || errno==EINTR;
    if(!n){
        c->eof=true;
        if(c->used){c->line[c->used]=0;printf("%s: %s\n",name,c->line);c->used=0;}
        return close_fd(&c->output);
    }
    for(ssize_t i=0;i<n;++i){
        if(bytes[i]=='\n'){
            c->line[c->used]=0;printf("%s: %s\n",name,c->line);
            if(!strncmp(c->line,"READY ",6))c->ready=true;
            if(!strncmp(c->line,"LAYOUT ",7))c->layout=true;
            c->used=0;
        }else{
            if(c->used>=sizeof(c->line)-1 || !bytes[i])return false;
            c->line[c->used++]=bytes[i];
        }
    }
    return true;
}
static bool reap(struct child *c) {
    if(!c->pid || c->reaped)return true;
    pid_t n=waitpid(c->pid,&c->status,WNOHANG);
    if(n<0)return errno==EINTR;
    if(n==c->pid){c->reaped=true;printf("REAP pid=%ld status=%d\n",(long)c->pid,c->status);}
    return true;
}
static bool ended(const struct child *c) { return !c->pid || (c->reaped && c->eof); }
static bool success(const struct child *c) { return WIFEXITED(c->status) && WEXITSTATUS(c->status)==0; }
static bool retire(struct child *children) {
    static const enum role order[]={FILES,TERMINAL,SETTINGS,DESKTOP,COMPOSITOR};
    for(size_t i=0;i<sizeof(order)/sizeof(order[0]);++i){
        struct child *c=&children[order[i]];
        if(!c->pid)continue;
        if(!c->reaped){
            if(!c->signalled){
                if(pidfd_send_signal(c->pin,SIGTERM,NULL,0) && errno!=ESRCH)return false;
                c->signalled=true;
            }
            return false;
        }
        if(!c->eof)return false;
    }
    return true;
}
static bool environment(char *runtime,size_t size,char *cache,size_t cache_size) {
    if(getuid()!=1000 || geteuid()!=1000 || getgid()!=1000 || getegid()!=1000)return false;
    FILE *status=fopen("/proc/self/status","re");char line[256];bool caps=false,nnp=false;
    if(!status)return false;
    while(fgets(line,sizeof(line),status)){
        if(!strcmp(line,"CapEff:\t0000000000000000\n"))caps=true;
        if(!strcmp(line,"NoNewPrivs:\t1\n"))nnp=true;
    }
    if(fclose(status) || !caps || !nnp)return false;
    char *session=NULL,*seat=NULL,*type=NULL,*class=NULL;uid_t uid=0;
    bool ok=sd_pid_get_session(0,&session)>=0 && sd_session_get_uid(session,&uid)>=0 && uid==1000 && sd_session_get_seat(session,&seat)>=0 && !strcmp(seat,"seat0") && sd_session_get_type(session,&type)>=0 && !strcmp(type,"wayland") && sd_session_get_class(session,&class)>=0 && !strcmp(class,"user");
    free(session);free(seat);free(type);free(class);
    const char *original=getenv("XDG_RUNTIME_DIR");struct stat info;
    if(!ok || !original || lstat(original,&info) || !S_ISDIR(info.st_mode) || info.st_uid!=1000 || (info.st_mode&07777)!=0700)return false;
    int n=snprintf(runtime,size,"%s/aetheri",original);
    if(n<0 || (size_t)n>=size || mkdir(runtime,0700))return false;
    n=snprintf(cache,cache_size,"%s/cache",runtime);
    if(n<0 || (size_t)n>=cache_size || mkdir(cache,0700))return false;
    return setenv("XDG_RUNTIME_DIR",runtime,1)==0 && setenv("WAYLAND_DISPLAY","aetheri-image",1)==0 && setenv("LIBSEAT_BACKEND","logind",1)==0 && setenv("LIBGL_ALWAYS_SOFTWARE","1",1)==0 && setenv("GALLIUM_DRIVER","llvmpipe",1)==0 && setenv("LP_NUM_THREADS","2",1)==0 && setenv("LD_LIBRARY_PATH","/usr/lib/aetheri/runtime",1)==0 && setenv("PATH",BIN ":/usr/bin",1)==0 && unsetenv("DISPLAY")==0 && unsetenv("WAYLAND_SOCKET")==0;
}
static bool app_start(struct child *c,enum role role,char *cache) {
    char *settings[]={BIN "aetheri-settings-nexaui","--font",SANS,"--cache",cache,"--diagnostics",NULL};
    char *terminal[]={BIN "aetheri-terminal-nexaui","--shell",BIN "aetheri-shell","--pty-starter",BIN "aetheri-terminal-pty-starter","--font",MONO,"--bold-font",BOLD,"--cache",cache,"--diagnostics",NULL};
    char *files[]={BIN "aetheri-files-nexaui","--root","/home/aetheri/Documents","--font",SANS,"--terminal-font",MONO,"--cache",cache,"--terminal",BIN "aetheri-terminal-nexaui","--editor",BIN "aetheri-editor","--diagnostics",NULL};
    return spawn(c,role==SETTINGS?settings:role==TERMINAL?terminal:files,NULL,0);
}

int main(void) {
    struct child children[ROLE_COUNT];for(size_t i=0;i<ROLE_COUNT;++i)empty(&children[i]);
    char runtime[512],cache[512];setvbuf(stdout,NULL,_IOLBF,0);
    if(!environment(runtime,sizeof(runtime),cache,sizeof(cache))){fputs("AETHERI_DESKTOP_FAIL context\n",stderr);return 1;}
    sigset_t mask;sigemptyset(&mask);sigaddset(&mask,SIGCHLD);sigaddset(&mask,SIGTERM);sigaddset(&mask,SIGINT);
    if(sigprocmask(SIG_BLOCK,&mask,NULL) || signal(SIGCHLD,SIG_DFL)==SIG_ERR || signal(SIGPIPE,SIG_IGN)==SIG_ERR)return 1;
    int signals=signalfd(-1,&mask,SFD_CLOEXEC|SFD_NONBLOCK),shell[2]={-1,-1},launch[2]={-1,-1};
    if(signals<0 || socketpair(AF_UNIX,SOCK_STREAM|SOCK_CLOEXEC,0,shell) || socketpair(AF_UNIX,SOCK_SEQPACKET|SOCK_CLOEXEC,0,launch) || !nonblock(launch[0]))return 1;
    char shell_number[32],launch_number[32];snprintf(shell_number,sizeof(shell_number),"%d",shell[0]);
    char *compositor[]={BIN "aetheri-compositor","--drm","aetheri-image","-","/dev/dri/card0","--shell-fd",shell_number,"--compose",NULL};
    bool failed=!spawn(&children[COMPOSITOR],compositor,&shell[0],1),stopping=failed,notified=false;
    close_fd(&shell[0]);unsigned launches[ROLE_COUNT]={0};double deadline=now()+60;
    while(true){
        for(size_t i=0;i<ROLE_COUNT;++i)if(!reap(&children[i])){failed=true;stopping=true;}
        if(!stopping && children[COMPOSITOR].ready && !children[DESKTOP].pid){
            snprintf(shell_number,sizeof(shell_number),"%d",shell[1]);snprintf(launch_number,sizeof(launch_number),"%d",launch[1]);
            int pass[]={shell[1],launch[1]};
            char *desktop[]={BIN "aetheri-desktop","--font",SANS,"--cache",cache,"--launcher-fd",launch_number,"--diagnostics",NULL};
            if(setenv("WAYLAND_SOCKET",shell_number,1) || !spawn(&children[DESKTOP],desktop,pass,2)){failed=true;stopping=true;}
            unsetenv("WAYLAND_SOCKET");close_fd(&shell[1]);close_fd(&launch[1]);
        }
        if(!stopping && !notified && children[COMPOSITOR].ready && children[DESKTOP].ready && children[DESKTOP].layout){
            char *session=NULL;bool active=sd_pid_get_session(0,&session)>=0 && sd_session_is_active(session)>0;free(session);
            if(!active || sd_notify(0,"READY=1\nSTATUS=Aetheri image desktop submitted its first frame")<=0){failed=true;stopping=true;}
            else{notified=true;puts("AETHERI_DESKTOP_READY uid=1000 cap_zero=1 seat=seat0");}
        }
        for(size_t i=0;i<ROLE_COUNT;++i){
            struct child *c=&children[i];
            if(!c->pid || !c->reaped)continue;
            if(!stopping && i>=FILES && success(c) && c->eof){
                if(!reply(launch[0],'X',(enum role)i)){failed=true;stopping=true;}
                close_fd(&c->pin);empty(c);
            }else if(!stopping && (i<FILES || !success(c))){
                if(!notified || !success(c))failed=true;
                stopping=true;
            }
        }
        if(!stopping && !notified && now()>=deadline){failed=true;stopping=true;}
        if(stopping){
            close_fd(&launch[1]);close_fd(&shell[1]);
            if(retire(children))break;
        }
        struct pollfd fds[ROLE_COUNT+2];int roles[ROLE_COUNT+2];nfds_t count=0;
        fds[count]=(struct pollfd){.fd=signals,.events=POLLIN};roles[count++]=-1;
        if(!stopping){fds[count]=(struct pollfd){.fd=launch[0],.events=POLLIN};roles[count++]=-2;}
        for(size_t i=0;i<ROLE_COUNT;++i)if(children[i].output>=0){fds[count]=(struct pollfd){.fd=children[i].output,.events=POLLIN};roles[count++]=(int)i;}
        int n=poll(fds,count,!notified && !stopping?1000:-1);
        if(n<0){if(errno==EINTR)continue;failed=true;stopping=true;continue;}
        for(nfds_t i=0;i<count;++i){
            if(!fds[i].revents)continue;
            if(roles[i]==-1){struct signalfd_siginfo info;while(read(signals,&info,sizeof(info))==(ssize_t)sizeof(info))if(info.ssi_signo!=SIGCHLD)stopping=true;}
            else if(roles[i]==-2){
                int request=packet(launch[0]);
                if(request==100)stopping=true;
                else if(request<0){failed=true;stopping=true;}
                else if(request>0 && !stopping){
                    enum role role=(enum role)(request-1);
                    if(!reap(&children[role])){failed=true;stopping=true;}
                    else if(children[role].pid){if(!reply(launch[0],children[role].reaped?'E':'A',role)){failed=true;stopping=true;}}
                    else if(launches[role]>=128){if(!reply(launch[0],'E',role)){failed=true;stopping=true;}}
                    else{
                        ++launches[role];
                        if(!app_start(&children[role],role,cache) || !reply(launch[0],'A',role)){failed=true;stopping=true;}
                    }
                }
            }else if(!drain_line(&children[roles[i]],names[roles[i]])){failed=true;stopping=true;}
        }
    }
    for(size_t i=0;i<ROLE_COUNT;++i){
        struct child *c=&children[i];
        if(!ended(c) || (c->pid && !success(c) && !(WIFSIGNALED(c->status) && WTERMSIG(c->status)==SIGTERM)))failed=true;
        close_fd(&c->pin);close_fd(&c->output);
    }
    close_fd(&launch[0]);close_fd(&signals);
    printf("AETHERI_DESKTOP_END status=%s\n",failed?"failed":"ok");
    return failed?1:0;
}
