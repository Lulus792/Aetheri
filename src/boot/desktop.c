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
#include <systemd/sd-journal.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>
#include <linux/close_range.h>
#include <limits.h>
#include <pwd.h>
#include <sys/prctl.h>
#include "../linux/session.h"
#ifdef AETHERI_DESKTOP_SESSION_LOCK
#include "login/unlock_authority.h"
#include "login/unlock_broker.h"
#endif
#ifdef AETHERI_DESKTOP_SESSION_SUSPEND
#include "boot/suspend.h"
#endif

/* Build-selected paths. The image keeps these defaults; a separate, explicitly
   reviewed lab binary can bind the protected runtime without altering the host.
   These constants are not runtime caller authority or package admission. */
#ifndef AETHERI_DESKTOP_BIN
#define AETHERI_DESKTOP_BIN "/usr/libexec/aetheri/"
#endif
#ifndef AETHERI_DESKTOP_SANS
#define AETHERI_DESKTOP_SANS "/usr/share/aetheri/fonts/LiberationSans-Regular.ttf"
#endif
#ifndef AETHERI_DESKTOP_MONO
#define AETHERI_DESKTOP_MONO "/usr/share/fonts/liberation-mono-fonts/LiberationMono-Regular.ttf"
#endif
#ifndef AETHERI_DESKTOP_BOLD
#define AETHERI_DESKTOP_BOLD "/usr/share/fonts/liberation-mono-fonts/LiberationMono-Bold.ttf"
#endif
#ifndef AETHERI_DESKTOP_LIBRARIES
#define AETHERI_DESKTOP_LIBRARIES "/usr/lib/aetheri/runtime"
#endif
#ifndef AETHERI_DESKTOP_LOCK_FONT
#define AETHERI_DESKTOP_LOCK_FONT AETHERI_DESKTOP_SANS
#endif
#if !defined(AETHERI_DESKTOP_DOCUMENTS) && !defined(AETHERI_DESKTOP_SESSION_ENTRY)
#define AETHERI_DESKTOP_DOCUMENTS "/home/aetheri/Documents"
#endif
#define BIN AETHERI_DESKTOP_BIN
#define SANS AETHERI_DESKTOP_SANS
#define MONO AETHERI_DESKTOP_MONO
#define BOLD AETHERI_DESKTOP_BOLD
enum role { COMPOSITOR, DESKTOP, FILES, TERMINAL, SETTINGS,
#ifdef AETHERI_DESKTOP_SESSION_LOCK
    LOCK_VIEW,
#endif
#ifdef AETHERI_DESKTOP_SESSION_AUDIO
    PIPEWIRE, WIREPLUMBER,
#ifdef AETHERI_DESKTOP_SESSION_SOUND
    SOUND_CONTROL,
#endif
#endif
    ROLE_COUNT };
static const char *const names[]={"compositor","desktop","files","terminal","settings",
#ifdef AETHERI_DESKTOP_SESSION_LOCK
    "lock-view",
#endif
#ifdef AETHERI_DESKTOP_SESSION_AUDIO
    "pipewire","wireplumber",
#ifdef AETHERI_DESKTOP_SESSION_SOUND
    "sound-control",
#endif
#endif
};
struct child {
    pid_t pid;
    int pin,output,status;
    bool reaped,eof,ready,layout,signalled,killed;
    char line[65536];
    size_t used;
};
static void empty(struct child *c) { memset(c,0,sizeof(*c));c->pin=-1;c->output=-1; }
static bool close_fd(int *fd) { if(*fd<0)return true;int n=*fd;*fd=-1;return close(n)==0; }
static bool nonblock(int fd) { int flags=fcntl(fd,F_GETFL);return flags>=0 && fcntl(fd,F_SETFL,flags|O_NONBLOCK)==0; }
static double now(void) { struct timespec t;if(clock_gettime(CLOCK_MONOTONIC,&t))return -1;return (double)t.tv_sec+(double)t.tv_nsec/1e9; }

/* The child waits before target exec. Failed pin capture closes the sole release
 * writer and reaps this known no-exec child; it never signals a numeric PID. */
static bool spawn(struct child *c,char *const argv[],const int *pass,size_t count,
                  bool fixed,char *const audio_env[]) {
    enum { FIRST_HANDOFF_FD=3, MAX_HANDOFF_FDS=4 };
    int logs[2],gate[2];
    if(!c || !argv || !argv[0] || argv[0][0]!='/' || c->pid ||
       count>MAX_HANDOFF_FDS || (count && !pass))return false;
    for(int fd=0;fd<3;++fd)if(fcntl(fd,F_GETFD)<0)return false;
    for(size_t i=0;i<count;++i)if(pass[i]<3 || fcntl(pass[i],F_GETFD)<0)return false;
    if(pipe2(logs,O_CLOEXEC))return false;
    if(pipe2(gate,O_CLOEXEC)){close(logs[0]);close(logs[1]);return false;}
    pid_t owner=getpid(),pid=fork();
    if(pid<0){close(logs[0]);close(logs[1]);close(gate[0]);close(gate[1]);return false;}
    if(!pid){
        close(gate[1]);
        /* The audio pair also loses its owner if the session dies abruptly. */
        if(audio_env && (prctl(PR_SET_PDEATHSIG,SIGTERM,0,0,0) || getppid()!=owner))_exit(125);
        int null=open("/dev/null",O_RDONLY|O_CLOEXEC);
        if(null<0)_exit(125);
        int input=dup2(null,0),output=dup2(logs[1],1),error=dup2(logs[1],2);
        if(input<0 || output<0 || error<0 || close_range(3,~0u,CLOSE_RANGE_CLOEXEC))_exit(125);
        if(!fixed)for(size_t i=0;i<count;++i)if(fcntl(pass[i],F_SETFD,0))_exit(125);
        char release=0,extra=0;ssize_t got;
        do{got=read(gate[0],&release,1);}while(got<0 && errno==EINTR);
        if(got!=1 || release!='A')_exit(125);
        do{got=read(gate[0],&extra,1);}while(got<0 && errno==EINTR);
        if(got!=0)_exit(125);
        if(fixed) {
            int saved[MAX_HANDOFF_FDS];
            for(size_t i=0;i<count;++i) {
                saved[i]=fcntl(pass[i],F_DUPFD_CLOEXEC,FIRST_HANDOFF_FD+MAX_HANDOFF_FDS);
                if(saved[i]<0)_exit(125);
            }
            for(size_t i=0;i<count;++i)if(dup2(saved[i],FIRST_HANDOFF_FD+(int)i)<0)_exit(125);
        }
        sigset_t mask;sigemptyset(&mask);
        if(sigprocmask(SIG_SETMASK,&mask,NULL) || signal(SIGPIPE,audio_env?SIG_IGN:SIG_DFL)==SIG_ERR)_exit(125);
        execve(argv[0],argv,audio_env?audio_env:environ);
        int failure=errno;
        (void)dprintf(STDERR_FILENO,"exec failed path=%s errno=%d\n",argv[0],failure);
        _exit(126);
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
static bool spawn_role(struct child *c,enum role role,char *const argv[],const int *pass,size_t count) {
    bool result=spawn(c,argv,pass,count,false,NULL);
    if(c->pid)printf("SPAWN %s %ld\n",names[role],(long)c->pid);
    return result;
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
    static const enum role order[]={FILES,TERMINAL,SETTINGS,DESKTOP,
#ifdef AETHERI_DESKTOP_SESSION_LOCK
        LOCK_VIEW,
#endif
        COMPOSITOR};
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
static bool session_live(struct ae_session *session) {
    unsigned loss=0;int result=ae_session_check(session,&loss);
    if(result<0 || loss) {printf("AETHERI_SESSION_LOST result=%d loss=%u\n",result,loss);return false;}
    return true;
}
/* This entry consumes an existing local login; it never authenticates a password,
 * creates an account, selects another UID or opens a privileged PAM session. */
static bool entry_paths(char **parent,char *documents,size_t size) {
#ifdef AETHERI_DESKTOP_SESSION_ENTRY
    uid_t real,effective,saved;gid_t group,egroup,sgroup;
    if(getresuid(&real,&effective,&saved) || !real || real!=effective || real!=saved ||
       getresgid(&group,&egroup,&sgroup) || !group || group!=egroup || group!=sgroup)return false;
    struct passwd account,*found=NULL;char buffer[16384];
    if(getpwuid_r(real,&account,buffer,sizeof(buffer),&found) || !found || account.pw_uid!=real || account.pw_gid!=group ||
       !account.pw_dir || account.pw_dir[0]!='/' || !account.pw_name || !*account.pw_name)return false;
    char path[64];int length=snprintf(path,sizeof(path),"/run/user/%lu",(unsigned long)real);
    if(length<0 || (size_t)length>=sizeof(path) || !(*parent=strdup(path)))return false;
    /* NSS and the systemd /run/user hierarchy are trusted host views. Caller
     * HOME/USER/runtime hints cannot select another account/runtime directory. */
    if(setenv("HOME",account.pw_dir,1) || setenv("USER",account.pw_name,1) || setenv("LOGNAME",account.pw_name,1))return false;
#ifdef AETHERI_DESKTOP_DOCUMENTS
    int n=snprintf(documents,size,"%s",AETHERI_DESKTOP_DOCUMENTS);
#else
    int n=snprintf(documents,size,"%s/Documents",account.pw_dir);
#endif
    if(n<0 || (size_t)n>=size || prctl(PR_SET_NO_NEW_PRIVS,1,0,0,0))return false;
    return true;
#else
    if(getuid()!=1000 || geteuid()!=1000 || getgid()!=1000 || getegid()!=1000)return false;
    const char *original=getenv("XDG_RUNTIME_DIR");if(!original)return false;
    *parent=strdup(original);int n=snprintf(documents,size,"%s",AETHERI_DESKTOP_DOCUMENTS);
    return *parent && n>=0 && (size_t)n<size;
#endif
}
static bool ready_notification(void) {
#ifdef AETHERI_DESKTOP_SESSION_ENTRY
    /* A display manager may exec this entry directly, without a notify service.
     * A supplied notify channel still requires its original acknowledgement. */
    if(!getenv("NOTIFY_SOCKET"))return true;
#endif
    return sd_notify(0,"READY=1\nSTATUS=Aetheri desktop submitted its first frame")>0;
}
/* Drop inherited descriptors before opening observers or spawning children.
 * The explicit authority profile reserves only the original unlock socket. */
static bool entry_descriptors(void) {
#ifdef AETHERI_DESKTOP_SESSION_ENTRY
    for(int fd=0;fd<3;++fd)if(fcntl(fd,F_GETFD)<0)return false;
#ifdef AETHERI_DESKTOP_UNLOCK_AUTHORITY
    return fcntl(3,F_GETFD)>=0 && close_range(4,~0u,CLOSE_RANGE_UNSHARE)==0;
#else
    return close_range(3,~0u,CLOSE_RANGE_UNSHARE)==0;
#endif
#else
    return true;
#endif
}
/* greetd connects the session's stdio to its VT. Keep diagnostics in the local
 * journal when available; an absent journal leaves the original console usable.
 * This runs after dropping inherited extra FDs and before any child starts. */
#ifdef AETHERI_DESKTOP_SESSION_ENTRY
/* The process standard streams own these duplicates until normal process exit. */
static int entry_log_output=-1,entry_log_error=-1;
#endif
static bool entry_logging(void) {
#ifdef AETHERI_DESKTOP_SESSION_ENTRY
    if(!isatty(1) && !isatty(2))return true;
    int fd=sd_journal_stream_fd("aetheri-session",LOG_INFO,0);
    if(fd<0)return true;
    entry_log_output=dup2(fd,1);
    entry_log_error=dup2(fd,2);
    bool ok=entry_log_output==1 && entry_log_error==2;
    if(close(fd))ok=false;
    return ok;
#else
    return true;
#endif
}

struct session_runtime {
    int parent, directory, cache;
    const char *leaf;
};

static int runtime_match(int parent, const char *name, int held)
{
    struct stat expected, current;
    if (fstat(held, &expected) || fstatat(parent, name, &current, AT_SYMLINK_NOFOLLOW))
        return -errno;
    if (!S_ISDIR(current.st_mode) || expected.st_dev != current.st_dev ||
        expected.st_ino != current.st_ino) return -ESTALE;
    return 0;
}

static int runtime_retire(struct session_runtime *state)
{
    int result = 0;
    if (state->directory >= 0) {
        result = runtime_match(state->parent, state->leaf, state->directory);
        if (!result && state->cache >= 0) {
            result = runtime_match(state->directory, "cache", state->cache);
            if (!result && unlinkat(state->directory, "cache", AT_REMOVEDIR)) result = -errno;
        }
        if (!result) result = runtime_match(state->parent, state->leaf, state->directory);
        if (!result && unlinkat(state->parent, state->leaf, AT_REMOVEDIR)) result = -errno;
    }
    close_fd(&state->cache);
    close_fd(&state->directory);
    close_fd(&state->parent);
    state->leaf = NULL;
    return result;
}

static bool runtime_create(const char *parent, uid_t uid, struct session_runtime *state,
                           char *runtime, size_t size, char *cache, size_t cache_size)
{
    int length = snprintf(runtime, size, "%s/aetheri-XXXXXX", parent);
    if (length < 0 || (size_t)length >= size || (size_t)length + sizeof "/cache" > cache_size)
        return false;
    /* The temporary name is made through the original directory descriptor. */
    const int flags = O_PATH | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC;
    state->parent = open(parent, flags);
    struct stat info;
    if (state->parent < 0 || fstat(state->parent, &info) || info.st_uid != uid ||
        (info.st_mode & 07777) != 0700) return false;
    char candidate[64];
    int n = snprintf(candidate, sizeof candidate, "/proc/self/fd/%d/aetheri-XXXXXX", state->parent);
    if (n < 0 || (size_t)n >= sizeof candidate || !mkdtemp(candidate)) return false;
    char *leaf = strrchr(runtime, '/') + 1;
    memcpy(leaf, strrchr(candidate, '/') + 1, sizeof "aetheri-XXXXXX");
    state->leaf = leaf;
    state->directory = openat(state->parent, leaf, flags);
    if (state->directory < 0 || mkdirat(state->directory, "cache", 0700)) return false;
    state->cache = openat(state->directory, "cache", flags);
    if (state->cache < 0) return false;
    memcpy(cache, runtime, (size_t)length);
    memcpy(cache + length, "/cache", sizeof "/cache");
    return !runtime_match(AT_FDCWD, parent, state->parent) &&
           !runtime_match(state->parent, leaf, state->directory);
}

static void runtime_finish(struct session_runtime *state)
{
    bool held = state->directory >= 0;
    int result = runtime_retire(state);
    if (result) fprintf(stderr, "AETHERI_RUNTIME_CLEANUP error=%d\n", result);
    else if (held) puts("AETHERI_RUNTIME_CLEANUP removed=1");
}

#ifdef AETHERI_DESKTOP_SESSION_AUDIO
struct session_audio {
    bool disabled,stopping;
    double deadline;
    int directory,state;
};

static bool audio_spawn(struct child *c, enum role role, const char *runtime)
{
    char directory[sizeof "XDG_RUNTIME_DIR=/audio"+512];
    char state[sizeof "XDG_STATE_HOME=/audio"+512];
    int n=snprintf(directory,sizeof directory,"XDG_RUNTIME_DIR=%s/audio",runtime);
    int s=snprintf(state,sizeof state,"XDG_STATE_HOME=%s/audio",runtime);
    if(n<0 || (size_t)n>=sizeof directory || s<0 || (size_t)s>=sizeof state)return false;
    char *env[]={directory,state,"LANG=C","HOME=/nonexistent",
        "PIPEWIRE_CONFIG_DIR=/usr/share/aetheri/audio","PIPEWIRE_REMOTE=aetheri-audio",
        "PIPEWIRE_MODULE_DIR=/usr/lib64/pipewire-0.3","SPA_PLUGIN_DIR=/usr/lib64/spa-0.2",
        "WIREPLUMBER_CONFIG_DIR=/usr/share/aetheri/audio:/usr/share/wireplumber",
        "WIREPLUMBER_DATA_DIR=/usr/share/wireplumber","WIREPLUMBER_MODULE_DIR=/usr/lib64/wireplumber-0.5",
        "ALSA_CONFIG_PATH=/usr/share/alsa/alsa.conf","GIO_USE_VFS=local",NULL};
    char *graph[]={BIN "aetheri-pipewire","-c","pipewire.conf",NULL};
    char *policy[]={BIN "aetheri-wireplumber","-p","aetheri-desktop",NULL};
    bool ok=spawn(c,role==PIPEWIRE?graph:policy,NULL,0,false,env);
    if(c->pid)printf("SPAWN %s %ld\n",names[role],(long)c->pid);
    return ok;
}

static void audio_step(struct session_audio *audio, struct child *children,
                       const struct session_runtime *owned, const char *runtime)
{
    if(audio->disabled || audio->stopping)return;
    struct child *graph=&children[PIPEWIRE],*policy=&children[WIREPLUMBER];
    if(graph->reaped || policy->reaped)goto unavailable;
    if(policy->pid)return;
    if(runtime_match(owned->parent,owned->leaf,owned->directory))goto unavailable;
    if(!graph->pid){
        double time=now();
        const int flags=O_PATH|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC;
        if(time<0 || mkdirat(owned->directory,"audio",0700))goto unavailable;
        audio->directory=openat(owned->directory,"audio",flags);
        if(audio->directory<0 || mkdirat(audio->directory,"wireplumber",0700))goto unavailable;
        audio->state=openat(audio->directory,"wireplumber",flags);
        if(audio->state<0 || !audio_spawn(graph,PIPEWIRE,runtime))goto unavailable;
        audio->deadline=time+6;
        return;
    }
    struct stat socket;
    if(runtime_match(owned->directory,"audio",audio->directory) ||
       runtime_match(audio->directory,"wireplumber",audio->state))goto unavailable;
    if(!fstatat(audio->directory,"aetheri-audio",&socket,AT_SYMLINK_NOFOLLOW)){
        if(!S_ISSOCK(socket.st_mode) || socket.st_uid!=getuid() ||
           (socket.st_mode&07777)!=0600 || !audio_spawn(policy,WIREPLUMBER,runtime))goto unavailable;
        return;
    }
    if(errno==ENOENT){
        double time=now();
        if(time>=0 && time<audio->deadline)return;
    }
unavailable:
    puts("AETHERI_AUDIO_UNAVAILABLE");
    audio->stopping=true;
}

static bool audio_retire(struct session_audio *audio, struct child *children, const struct session_runtime *owned)
{
    if(audio->disabled)return true;
    /* Retire the control connection, then routing, then the original graph. */
    for(enum role role=ROLE_COUNT-1;role>=PIPEWIRE;role--){
        struct child *c=&children[role];
        if(!c->pid)continue;
        if(!c->reaped){
            double time=now();
            if(!c->signalled){
                if(pidfd_send_signal(c->pin,SIGTERM,NULL,0) && errno!=ESRCH)return false;
                c->signalled=true;
                audio->deadline=time+4;
#ifdef AETHERI_DESKTOP_SESSION_SOUND
                if(role==SOUND_CONTROL)audio->deadline=time+8;
#endif
            }else if(!c->killed && (time<0 || time>=audio->deadline)){
                if(pidfd_send_signal(c->pin,SIGKILL,NULL,0) && errno!=ESRCH)return false;
                c->killed=true;
            }
            return false;
        }
        if(c->output>=0)return false;
    }
    if(audio->state>=0){
        int result=runtime_match(owned->directory,"audio",audio->directory);
        if(!result)result=runtime_match(audio->directory,"wireplumber",audio->state);
        if(!result && unlinkat(audio->directory,"wireplumber",AT_REMOVEDIR))result=-errno;
        if(result)fprintf(stderr,"AETHERI_AUDIO_STATE_CLEANUP error=%d\n",result);
        close_fd(&audio->state);
    }
    if(audio->directory>=0){
        int result=runtime_match(owned->directory,"audio",audio->directory);
        if(!result && unlinkat(owned->directory,"audio",AT_REMOVEDIR))result=-errno;
        if(result)fprintf(stderr,"AETHERI_AUDIO_RUNTIME_CLEANUP error=%d\n",result);
        close_fd(&audio->directory);
    }
    audio->disabled=true;
    return true;
}
#endif

static bool environment(char *runtime,size_t size,char *cache,size_t cache_size,char *documents,size_t document_size,struct ae_session **session,struct session_runtime *owned) {
    char *original=NULL;
    if(!entry_paths(&original,documents,document_size)){free(original);return false;}
    uid_t uid=getuid();
    FILE *status=fopen("/proc/self/status","re");char line[256];unsigned caps=0;bool nnp=false;
    if(!status){free(original);return false;}
    while(fgets(line,sizeof(line),status)){
        if(!strcmp(line,"CapEff:\t0000000000000000\n"))caps|=1;
        if(!strcmp(line,"CapPrm:\t0000000000000000\n"))caps|=2;
        if(!strcmp(line,"CapInh:\t0000000000000000\n"))caps|=4;
        if(!strcmp(line,"CapAmb:\t0000000000000000\n"))caps|=8;
        if(!strcmp(line,"NoNewPrivs:\t1\n"))nnp=true;
    }
    if(fclose(status) || caps!=15 || !nnp || ae_session_open(uid,"seat0",session)<0){free(original);return false;}
#ifdef AETHERI_DESKTOP_SESSION_ENTRY
    struct stat info;
    const char *parents[]={"/run","/run/user"};
    for(size_t i=0;i<2;++i)if(lstat(parents[i],&info) || !S_ISDIR(info.st_mode) || info.st_uid || (info.st_mode&0022)){
        free(original);return false;
    }
#endif
    bool created = runtime_create(original, uid, owned, runtime, size, cache, cache_size);
    free(original);
    if (!created) return false;
    printf("AETHERI_RUNTIME path=%s cache=%s\n",runtime,cache);
#ifdef AETHERI_DESKTOP_SHARE
    /* A build with an independently bound runtime supplies its own data path
       alongside the existing library path. Keep that choice in the session. */
    if(setenv("LIBINPUT_QUIRKS_DIR",AETHERI_DESKTOP_SHARE "/libinput",1) ||
       setenv("__EGL_VENDOR_LIBRARY_DIRS",AETHERI_DESKTOP_SHARE "/glvnd/egl_vendor.d",1) ||
       unsetenv("__EGL_VENDOR_LIBRARY_FILENAMES") ||
       setenv("LIBGL_DRIVERS_PATH",AETHERI_DESKTOP_LIBRARIES "/dri",1))return false;
#endif
    return setenv("XDG_RUNTIME_DIR",runtime,1)==0 && setenv("WAYLAND_DISPLAY","aetheri-image",1)==0 && setenv("LIBSEAT_BACKEND","logind",1)==0 && setenv("LIBGL_ALWAYS_SOFTWARE","1",1)==0 && setenv("GALLIUM_DRIVER","llvmpipe",1)==0 && setenv("LP_NUM_THREADS","2",1)==0 && setenv("LD_LIBRARY_PATH",AETHERI_DESKTOP_LIBRARIES,1)==0 && setenv("PATH",BIN ":/usr/bin",1)==0 && unsetenv("DISPLAY")==0 && unsetenv("WAYLAND_SOCKET")==0;
}
static bool app_start(struct child *c,enum role role,char *cache,char *documents,struct ae_session *session) {
    if(!session_live(session))return false;
    char *settings[]={BIN "aetheri-settings-nexaui","--font",SANS,"--cache",cache,"--diagnostics",NULL};
    char *terminal[]={BIN "aetheri-terminal-nexaui","--shell",BIN "aetheri-shell","--pty-starter",BIN "aetheri-terminal-pty-starter","--font",MONO,"--bold-font",BOLD,"--cache",cache,"--diagnostics",NULL};
    char *files[]={BIN "aetheri-files-nexaui","--root",documents,"--font",SANS,"--terminal-font",MONO,"--terminal-bold-font",BOLD,"--cache",cache,"--terminal",BIN "aetheri-terminal-nexaui","--editor",BIN "aetheri-editor","--diagnostics",NULL};
    return spawn_role(c,role,role==SETTINGS?settings:role==TERMINAL?terminal:files,NULL,0);
}

#ifdef AETHERI_DESKTOP_SESSION_SOUND
static bool settings_start(struct child *children,const struct session_audio *audio,
                           const char *runtime,char *cache,char *documents,struct ae_session *session)
{
    struct child *graph=&children[PIPEWIRE],*policy=&children[WIREPLUMBER];
    struct child *settings=&children[SETTINGS],*broker=&children[SOUND_CONTROL];
    if(audio->stopping || audio->disabled || !graph->pid || graph->reaped ||
       !policy->pid || policy->reaped || broker->pid)
        return app_start(settings,SETTINGS,cache,documents,session);
    if(!session_live(session))return false;
    char remote[512+sizeof "/audio/aetheri-audio"];
    int length=snprintf(remote,sizeof remote,"%s/audio/aetheri-audio",runtime);
    if(length<0 || (size_t)length>=sizeof remote)return false;

    int channel[2];
    if(socketpair(AF_UNIX,SOCK_SEQPACKET|SOCK_CLOEXEC,0,channel))
        return app_start(settings,SETTINGS,cache,documents,session);
    char *view[]={BIN "aetheri-settings-nexaui","--sound-channel","--font",SANS,
                  "--cache",cache,"--diagnostics",NULL};
    bool started=spawn(settings,view,&channel[1],1,true,NULL);
    close(channel[1]);
    if(settings->pid)printf("SPAWN settings %ld\n",(long)settings->pid);
    if(started && session_live(session)) {
        char *command[]={BIN "aetheri-audio-desktop-control",remote,NULL};
        char *environment[]={"LANG=C","HOME=/nonexistent","PIPEWIRE_CONFIG_DIR=/usr/share/aetheri/audio",
            "PIPEWIRE_MODULE_DIR=/usr/lib64/pipewire-0.3","SPA_PLUGIN_DIR=/usr/lib64/spa-0.2",
            "WIREPLUMBER_MODULE_DIR=/usr/lib64/wireplumber-0.5","GIO_USE_VFS=local",NULL};
        int pass[]={channel[0],graph->pin,policy->pin,settings->pin};
        if(!spawn(broker,command,pass,sizeof pass/sizeof pass[0],true,environment))
            puts("AETHERI_SOUND_UNAVAILABLE launch");
        if(broker->pid)printf("SPAWN sound-control %ld\n",(long)broker->pid);
    }
    close(channel[0]);
    return started;
}
#endif

#ifdef AETHERI_DESKTOP_SESSION_LOCK
struct session_lock {
    int control[2], display[2], credentials[2];
    struct ae_unlock_broker *broker;
    struct ae_unlock_authority_client *authority;
    bool fenced;
};

static bool lock_channels(struct session_lock *lock) {
    return !socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, lock->control) &&
        !socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, lock->display) &&
        !socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, lock->credentials);
}

static bool lock_session_live(void *session) { return session_live(session); }

static bool lock_start(struct session_lock *lock, struct child *children, struct ae_session *session) {
    char display[32], credential[32];
    snprintf(display, sizeof display, "%d", lock->display[1]);
    snprintf(credential, sizeof credential, "%d", lock->credentials[1]);
    const char *locale = getenv("LANG");
    const char *account = getenv("USER"); /* Derived from the authenticated UID in entry_paths. */
    char *command[] = {BIN "aetheri-lock-view", display, credential, AETHERI_DESKTOP_LOCK_FONT, (char *)account,
        locale && !strncmp(locale, "de_", 3) ? "de" : "en", NULL};
    int pass[] = {lock->display[1], lock->credentials[1]};
    bool started = account && spawn_role(&children[LOCK_VIEW], LOCK_VIEW, command, pass, 2);
    close_fd(&lock->display[1]);
    close_fd(&lock->credentials[1]);
    if (!started) return false;
    struct ae_unlock_broker_config config = {
        .compositor = children[COMPOSITOR].pin, .view = children[LOCK_VIEW].pin,
        .control = lock->control[0], .credentials = lock->credentials[0],
        .worker = BIN "aetheri-unlock-worker", .account = account,
        .session_live = lock_session_live, .session = session, .authority = lock->authority,
    };
    int status = ae_unlock_broker_open(&lock->broker, &config);
    if (status) {
        printf("AETHERI_LOCK_UNAVAILABLE startup=%d\n", status);
        return false;
    }
    lock->authority = NULL;
    lock->control[0] = lock->credentials[0] = -1;
    return true;
}

static void lock_step(struct session_lock *lock) {
    if (!lock->broker) return;
    int status = ae_unlock_broker_step(lock->broker);
    if (status && !lock->fenced) {
        lock->fenced = true;
        printf("AETHERI_LOCK_UNAVAILABLE live=%d\n", status);
    }
}

static void lock_close(struct session_lock *lock) {
    ae_unlock_broker_close(&lock->broker);
    ae_unlock_authority_client_close(&lock->authority);
    for (unsigned i = 0; i < 2; ++i) {
        close_fd(&lock->control[i]);
        close_fd(&lock->display[i]);
        close_fd(&lock->credentials[i]);
    }
}
#endif

int main(void) {
    if(!entry_descriptors()){fputs("AETHERI_DESKTOP_FAIL descriptors\n",stderr);return 1;}
    if(!entry_logging()){fputs("AETHERI_DESKTOP_FAIL logging\n",stderr);return 1;}
    struct child children[ROLE_COUNT];for(size_t i=0;i<ROLE_COUNT;++i)empty(&children[i]);
#ifdef AETHERI_DESKTOP_SESSION_AUDIO
    struct session_audio audio={.directory=-1,.state=-1};
#endif
#ifdef AETHERI_DESKTOP_SESSION_LOCK
    struct session_lock lock = {.control = {-1,-1}, .display = {-1,-1}, .credentials = {-1,-1}};
#endif
#ifdef AETHERI_DESKTOP_UNLOCK_AUTHORITY
    if (ae_unlock_authority_client_open(&lock.authority, 3)) {
        fputs("AETHERI_DESKTOP_FAIL unlock authority\n", stderr);
        return 1;
    }
#endif
    struct session_runtime owned = { .parent = -1, .directory = -1, .cache = -1 };
    char runtime[512],cache[512],documents[4096];struct ae_session *session=NULL;setvbuf(stdout,NULL,_IOLBF,0);
    if(!environment(runtime,sizeof(runtime),cache,sizeof(cache),documents,sizeof(documents),&session,&owned)){runtime_finish(&owned);ae_session_close(&session);fputs("AETHERI_DESKTOP_FAIL context\n",stderr);return 1;}
    sigset_t mask;sigemptyset(&mask);sigaddset(&mask,SIGCHLD);sigaddset(&mask,SIGTERM);sigaddset(&mask,SIGINT);
    if(sigprocmask(SIG_BLOCK,&mask,NULL) || signal(SIGCHLD,SIG_DFL)==SIG_ERR || signal(SIGPIPE,SIG_IGN)==SIG_ERR){runtime_finish(&owned);ae_session_close(&session);return 1;}
    int signals=signalfd(-1,&mask,SFD_CLOEXEC|SFD_NONBLOCK),shell[2]={-1,-1},launch[2]={-1,-1};
    if(signals<0 || socketpair(AF_UNIX,SOCK_STREAM|SOCK_CLOEXEC,0,shell) || socketpair(AF_UNIX,SOCK_SEQPACKET|SOCK_CLOEXEC,0,launch) || !nonblock(launch[0])){
        close_fd(&signals);for(unsigned i=0;i<2;++i){close_fd(&shell[i]);close_fd(&launch[i]);}runtime_finish(&owned);ae_session_close(&session);return 1;
    }
    char shell_number[32],launch_number[32];snprintf(shell_number,sizeof(shell_number),"%d",shell[0]);
#ifdef AETHERI_DESKTOP_SESSION_LOCK
    bool supported = lock_channels(&lock);
    char control_number[32], display_number[32];
    snprintf(control_number, sizeof control_number, "%d", lock.control[1]);
    snprintf(display_number, sizeof display_number, "%d", lock.display[0]);
    char *compositor[]={BIN "aetheri-compositor","--drm","aetheri-image","-","auto","--shell-fd",shell_number,
        "--compose","--session-lock-fd",control_number,"--lock-view-fd",display_number,NULL};
    int compositor_fds[] = {shell[0], lock.control[1], lock.display[0]};
#else
    bool supported = true;
    char *compositor[]={BIN "aetheri-compositor","--drm","aetheri-image","-","auto","--shell-fd",shell_number,"--compose",NULL};
    int compositor_fds[] = {shell[0]};
#endif
    bool failed=!supported || !session_live(session) ||
        !spawn_role(&children[COMPOSITOR],COMPOSITOR,compositor,compositor_fds,sizeof compositor_fds / sizeof compositor_fds[0]);
    bool stopping=failed,notified=false,fence_logged=false;
#ifdef AETHERI_DESKTOP_SESSION_SUSPEND
    struct ae_suspend_server *suspend = NULL;
#endif
#ifdef AETHERI_DESKTOP_SESSION_LOCK
    close_fd(&lock.control[1]);
    close_fd(&lock.display[0]);
#endif
    close_fd(&shell[0]);unsigned launches[ROLE_COUNT]={0};double deadline=now()+60;
    while(true){
        if(!stopping && !session_live(session)){failed=true;stopping=true;}
        for(size_t i=0;i<ROLE_COUNT;++i)if(!reap(&children[i])){failed=true;stopping=true;}
#ifdef AETHERI_DESKTOP_SESSION_LOCK
        if (!stopping && children[COMPOSITOR].ready && !lock.broker && !lock_start(&lock,children,session))
            failed=stopping=true;
        if (!stopping) lock_step(&lock);
#endif
        if(!stopping && children[COMPOSITOR].ready && !children[DESKTOP].pid){
            snprintf(shell_number,sizeof(shell_number),"%d",shell[1]);snprintf(launch_number,sizeof(launch_number),"%d",launch[1]);
            int pass[]={shell[1],launch[1]};
            char *desktop[]={BIN "aetheri-desktop","--font",SANS,"--cache",cache,"--launcher-fd",launch_number,"--diagnostics",NULL};
            if(!session_live(session) || setenv("WAYLAND_SOCKET",shell_number,1) || !spawn_role(&children[DESKTOP],DESKTOP,desktop,pass,2)){failed=true;stopping=true;}
            unsetenv("WAYLAND_SOCKET");close_fd(&shell[1]);close_fd(&launch[1]);
        }
        if(!stopping && !notified && children[COMPOSITOR].ready && children[DESKTOP].ready && children[DESKTOP].layout){
            if(!session_live(session) || !ready_notification()){failed=true;stopping=true;}
            else{
#ifdef AETHERI_DESKTOP_SESSION_SUSPEND
                int status = lock.fenced ? -ESTALE : ae_suspend_server_open(&suspend);
                if (status) printf("AETHERI_SUSPEND_UNAVAILABLE error=%d\n", status);
#endif
                notified=true;printf("AETHERI_DESKTOP_READY uid=%lu cap_zero=1 seat=seat0\n",(unsigned long)getuid());
            }
        }
#ifdef AETHERI_DESKTOP_SESSION_SUSPEND
        if (stopping || lock.fenced) ae_suspend_server_close(&suspend);
        else ae_suspend_server_step(suspend, lock.broker);
#endif
#ifdef AETHERI_DESKTOP_SESSION_AUDIO
        if(!stopping && notified)audio_step(&audio,children,&owned,runtime);
#endif
#ifdef AETHERI_DESKTOP_SESSION_SOUND
        struct child *broker=&children[SOUND_CONTROL];
        if(broker->pid && broker->reaped && broker->output<0) {
            close_fd(&broker->pin);
            empty(broker);
        }
#endif
        for(size_t i=0;i<ROLE_COUNT;++i){
            struct child *c=&children[i];
            if(i>SETTINGS || !c->pid || !c->reaped)continue;
            if(!stopping && i>=FILES && success(c) && c->eof){
#ifdef AETHERI_DESKTOP_SESSION_SOUND
                if(i==SETTINGS && broker->pid)continue;
#endif
                if(!reply(launch[0],'X',(enum role)i)){failed=true;stopping=true;}
                close_fd(&c->pin);empty(c);
            }else if(!stopping && (i<FILES || !success(c))){
                if(!notified || !success(c))failed=true;
                stopping=true;
            }
        }
        if(!stopping && !notified && now()>=deadline){failed=true;stopping=true;}
#ifdef AETHERI_DESKTOP_SESSION_AUDIO
        if(stopping)audio.stopping=true;
        bool audio_ended=!audio.stopping || audio_retire(&audio,children,&owned);
#endif
        if(stopping){
            if(!fence_logged){puts("STOPPING");fence_logged=true;}
#ifdef AETHERI_DESKTOP_SESSION_SUSPEND
            ae_suspend_server_close(&suspend);
#endif
#ifdef AETHERI_DESKTOP_SESSION_LOCK
            lock_close(&lock);
#endif
            close_fd(&launch[1]);close_fd(&shell[1]);
#ifdef AETHERI_DESKTOP_SESSION_AUDIO
            if(audio_ended && retire(children))break;
#else
            if(retire(children))break;
#endif
        }
        enum { SESSION_EVENTS = ROLE_COUNT + 3
#ifdef AETHERI_DESKTOP_SESSION_LOCK
            + AE_UNLOCK_BROKER_FDS
#endif
#ifdef AETHERI_DESKTOP_SESSION_SUSPEND
            + AE_SUSPEND_SERVER_FDS
#endif
        };
        struct pollfd fds[SESSION_EVENTS];int roles[SESSION_EVENTS];nfds_t count=0;uint64_t until=UINT64_MAX;
        fds[count]=(struct pollfd){.fd=signals,.events=POLLIN};roles[count++]=-1;
        if(!stopping){
            if(ae_session_poll(session,&fds[count],&until)<0){failed=true;stopping=true;continue;}
            roles[count++]=-3;
        }
        if(!stopping){fds[count]=(struct pollfd){.fd=launch[0],.events=POLLIN};roles[count++]=-2;}
        for(size_t i=0;i<ROLE_COUNT;++i)if(children[i].output>=0){fds[count]=(struct pollfd){.fd=children[i].output,.events=POLLIN};roles[count++]=(int)i;}
        int timeout=!notified && !stopping?1000:-1;
#ifdef AETHERI_DESKTOP_SESSION_AUDIO
        if(!audio_ended || (!audio.disabled && children[PIPEWIRE].pid && !children[WIREPLUMBER].pid))timeout=100;
#endif
#ifdef AETHERI_DESKTOP_SESSION_LOCK
        if (!stopping && lock.broker) {
            int lock_timeout;
            ae_unlock_broker_poll(lock.broker,&fds[count],&lock_timeout);
            for (unsigned i=0;i<AE_UNLOCK_BROKER_FDS;++i) roles[count++]=-4;
            if (lock_timeout>=0 && (timeout<0 || lock_timeout<timeout)) timeout=lock_timeout;
        }
#endif
#ifdef AETHERI_DESKTOP_SESSION_SUSPEND
        if (!stopping && suspend) {
            ae_suspend_server_poll(suspend, &fds[count]);
            for (unsigned i=0;i<AE_SUSPEND_SERVER_FDS;++i) roles[count++]=-4;
        }
#endif
        if(until!=UINT64_MAX){
            double seconds=now();if(seconds<0){failed=true;stopping=true;continue;}
            double ms=(double)until/1000.0-seconds*1000.0;
            int due=ms<=0?0:ms>=(double)INT_MAX?INT_MAX:(int)ms+1;
            if(timeout<0 || due<timeout)timeout=due;
        }
        int n=poll(fds,count,timeout);
        if(n<0){if(errno==EINTR)continue;failed=true;stopping=true;continue;}
        for(nfds_t i=0;i<count;++i){
            if(!fds[i].revents)continue;
            if(roles[i]==-1){struct signalfd_siginfo info;while(read(signals,&info,sizeof(info))==(ssize_t)sizeof(info))if(info.ssi_signo!=SIGCHLD)stopping=true;}
            else if(roles[i]==-3){if(!session_live(session)){failed=true;stopping=true;}}
            else if(roles[i]==-2){
                int request=packet(launch[0]);
                if(request==100)stopping=true;
                else if(request<0){failed=true;stopping=true;}
                else if(request>0 && !stopping){
                    enum role role=(enum role)(request-1);
#ifdef AETHERI_DESKTOP_SESSION_LOCK
                    lock_step(&lock);
                    if(!ae_unlock_broker_open_desktop(lock.broker)) {
                        if(!reply(launch[0],'E',role)) failed=stopping=true;
                        continue;
                    }
#endif
                    if(!reap(&children[role])){failed=true;stopping=true;}
                    else if(children[role].pid){if(!reply(launch[0],children[role].reaped?'E':'A',role)){failed=true;stopping=true;}}
                    else if(launches[role]>=128){if(!reply(launch[0],'E',role)){failed=true;stopping=true;}}
                    else{
                        ++launches[role];
                        bool started;
#ifdef AETHERI_DESKTOP_SESSION_SOUND
                        if(role==SETTINGS)started=settings_start(children,&audio,runtime,cache,documents,session);
                        else
#endif
                        started=app_start(&children[role],role,cache,documents,session);
                        if(!started || !reply(launch[0],'A',role)){failed=true;stopping=true;}
                    }
                }
            }else if(roles[i]>=0 && !drain_line(&children[roles[i]],names[roles[i]])){
#ifdef AETHERI_DESKTOP_SESSION_AUDIO
                if(roles[i]>=PIPEWIRE){
                    struct child *child=&children[roles[i]];
                    close_fd(&child->output);
#ifdef AETHERI_DESKTOP_SESSION_SOUND
                    if(roles[i]==SOUND_CONTROL) {
                        puts("AETHERI_SOUND_UNAVAILABLE log");
                        if(!child->reaped)(void)pidfd_send_signal(child->pin,SIGTERM,NULL,0);
                        continue;
                    }
#endif
                    puts("AETHERI_AUDIO_UNAVAILABLE");
                    audio.stopping=true;
                    continue;
                }
#endif
                failed=true;
                stopping=true;
            }
        }
    }
    for(size_t i=0;i<ROLE_COUNT;++i){
        struct child *c=&children[i];
        if(i<=SETTINGS){
            if(!ended(c) || (c->pid && !success(c) && !(WIFSIGNALED(c->status) && WTERMSIG(c->status)==SIGTERM)))failed=true;
        }else if(c->pid && (!c->reaped || c->output>=0))failed=true;
        close_fd(&c->pin);close_fd(&c->output);
    }
    close_fd(&launch[0]);close_fd(&signals);
    runtime_finish(&owned);
    if(ae_session_close(&session)<0)failed=true;
    printf("NATIVE_SESSION_END {\"children\":{");bool comma=false;
    for(size_t i=0;i<ROLE_COUNT;++i)if(children[i].pid){
        int code=WIFEXITED(children[i].status)?WEXITSTATUS(children[i].status):WIFSIGNALED(children[i].status)?128+WTERMSIG(children[i].status):125;
        printf("%s\"%s\":%d",comma?",":"",names[i],code);comma=true;
    }
    puts("}}");printf("AETHERI_DESKTOP_END status=%s\n",failed?"failed":"ok");
    return failed?1:0;
}
