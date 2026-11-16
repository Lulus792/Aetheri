#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <linux/capability.h>
#include <selinux/selinux.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

/* Disposable-carrier probe only. Synthetic file opens/socket creation, never
   a password read, policy write, remote connection or installed greeter. */
static FILE *probe_log;
static FILE *output(void) {return probe_log?probe_log:stdout;}
#define CHECK(x) do {if(!(x)){fprintf(probe_log?probe_log:stderr,"login roles line %d: %s errno=%d\n",__LINE__,#x,errno);exit(1);}}while(0)
static void identity(const char *role) {
    uid_t r,e,s;gid_t gr,ge,gs;
    CHECK(!getresuid(&r,&e,&s) && !getresgid(&gr,&ge,&gs));
    bool greeter=!strcmp(role,"greeter");
    CHECK(r==e && e==s && gr==ge && ge==gs && r==(greeter?992u:0u) && gr==r);
    if(greeter) {
        gid_t groups[1];CHECK(getgroups(1,groups)==1 && groups[0]==992);
        struct __user_cap_header_struct h={.version=_LINUX_CAPABILITY_VERSION_3};
        struct __user_cap_data_struct d[2]={{0}};CHECK(!syscall(SYS_capget,&h,d));
        for(size_t i=0;i<2;++i)CHECK(!d[i].effective && !d[i].permitted && !d[i].inheritable);
    }
    char *label=NULL;CHECK(!getcon_raw(&label) && label && security_getenforce()==1);
    const char *wanted=greeter?"system_u:system_r:aetheri_greeter_t:s0":
        !strcmp(role,"worker")?"system_u:system_r:aetheri_login_worker_t:s0":"system_u:system_r:aetheri_login_backend_t:s0";
    CHECK(!strcmp(label,wanted));freecon(label);
}
static void opened(const char *path,int flags,bool permitted) {
    errno=0;int fd=open(path,flags|O_CLOEXEC|O_NOFOLLOW);
    if(permitted)CHECK(fd>=0 && !close(fd));
    else CHECK(fd<0 && (errno==EACCES || errno==EPERM));
}
static void path(char out[1024],const char *root,const char *leaf) {
    int n=snprintf(out,1024,"%s/%s",root,leaf);CHECK(n>0 && n<1024);
}
static void access_probe(const char *role,const char *root) {
    bool control=!strcmp(role,"control"),worker=!strcmp(role,"worker");
    if(!control)identity(role);else CHECK(security_getenforce()==1);
    struct stat st;CHECK(root[0]=='/' && !lstat(root,&st) && S_ISDIR(st.st_mode) && !st.st_uid && !st.st_gid && !(st.st_mode&022));
    char p[1024];path(p,root,"synthetic-shadow");opened(p,O_RDONLY,control || worker);
    path(p,root,"home/document");opened(p,O_RDONLY,control);opened(p,O_WRONLY,control);
    if(!control){opened("/sys/fs/selinux/load",O_WRONLY,false);opened("/sys/fs/selinux/enforce",O_WRONLY,false);}
    for(int i=0;i<2;++i) {
        errno=0;int fd=socket(i?AF_INET6:AF_INET,(i?SOCK_DGRAM:SOCK_STREAM)|SOCK_CLOEXEC,0);
        if(control)CHECK(fd>=0 && !close(fd));else CHECK(fd<0 && (errno==EACCES || errno==EPERM));
    }
    CHECK(fprintf(output(),"AETHERI_ROLE_ACCESS role=%s shadow_read=%d home_denied=%d sockets_denied=%d\n",role,control || worker,!control,!control)>0 && !fflush(output()));
}
static int wait_greeter(const char *root) {
    struct stat st;int flags=fcntl(3,F_GETFL);
    CHECK(!fstat(3,&st) && S_ISREG(st.st_mode) && st.st_uid==0 && st.st_gid==0 &&
        st.st_nlink==1 && (st.st_mode&07777)==0600 && flags>=0 && (flags&O_APPEND) &&
        (flags&O_ACCMODE)==O_RDWR);
    FILE *owned=fdopen(3,"a");CHECK(owned);probe_log=owned;
    identity("greeter");sigset_t signals;CHECK(!sigemptyset(&signals) && !sigaddset(&signals,SIGTERM) && !sigprocmask(SIG_BLOCK,&signals,NULL));
    if(root)access_probe("greeter",root);
    CHECK(fputs("AETHERI_ROLE_GREETER_READY\n",output())>=0 && !fflush(output()));
    struct timespec timeout={.tv_sec=30};int signum=sigtimedwait(&signals,NULL,&timeout);
    int result=signum==SIGTERM?0:2;
    CHECK(fputs(result?"AETHERI_ROLE_GREETER_TIMEOUT\n":"AETHERI_ROLE_GREETER_END\n",output())>=0 && !fflush(output()));
    probe_log=NULL;CHECK(!fclose(owned));return result;
}
int main(int argc,char **argv) {
    if((argc==2 || argc==3) && !strcmp(argv[1],"wait"))return wait_greeter(argc==3?argv[2]:NULL);
    CHECK(argc==3);
    if(!strcmp(argv[1],"spawn-worker")) {
        identity("backend");pid_t child=fork();CHECK(child>=0);
        if(!child){char *args[]={argv[0],"worker",argv[2],NULL};execv(argv[0],args);_exit(127);}
        int status;CHECK(waitpid(child,&status,0)==child && WIFEXITED(status) && !WEXITSTATUS(status));return 0;
    }
    CHECK(!strcmp(argv[1],"control") || !strcmp(argv[1],"backend") || !strcmp(argv[1],"worker") || !strcmp(argv[1],"greeter"));
    access_probe(argv[1],argv[2]);return 0;
}
