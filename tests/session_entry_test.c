/* Controlled account/logind/notify inputs; no authentication or account writes. */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#define AETHERI_DESKTOP_SESSION_ENTRY 1
#define getresuid ae_test_getresuid
#define getresgid ae_test_getresgid
#define getpwuid_r ae_test_getpwuid_r
#define prctl ae_test_prctl
#define sd_notify ae_test_notify
#define close_range ae_test_close_range
#define main ae_session_entry_main
#include "../src/boot/desktop.c"
#undef main
#undef getresuid
#undef getresgid
#undef getpwuid_r
#undef prctl
#undef sd_notify
#undef close_range
#include <stdarg.h>
#include <sys/syscall.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"entry check %d: %s\n",__LINE__,#x);abort();}} while(0)
static uid_t model_uid=1201,model_euid=1201,model_suid=1201;
static gid_t model_gid=1301,model_egid=1301,model_sgid=1301;
static int account_error,nnp_error,notify_result,notifications,nnp_calls;
static bool missing_account,wrong_account;
static const char *home="/home/original";
static bool descriptor_failure;
int ae_test_close_range(unsigned int first,unsigned int last,int flags) {
    CHECK(first==3 && last==~0u && flags==CLOSE_RANGE_UNSHARE);
    if(descriptor_failure){errno=EPERM;return -1;}
    return (int)syscall(SYS_close_range,first,last,flags);
}
static void descriptor_test(bool failure) {
    int pipe_fds[2];CHECK(!pipe2(pipe_fds,O_CLOEXEC));
    int canary=open("/dev/null",O_RDONLY|O_CLOEXEC);CHECK(canary>=3);
    pid_t pid=fork();CHECK(pid>=0);
    if(!pid) {
        if(dup2(pipe_fds[1],1)!=1)_exit(2);
        descriptor_failure=failure;
        bool result=entry_descriptors();
        int observed=fcntl(canary,F_GETFD);
        bool correct=failure?(!result && observed>=0):(result && observed<0 && errno==EBADF);
        for(int fd=0;fd<3;++fd)if(fcntl(fd,F_GETFD)<0)correct=false;
        char answer=correct?'y':'n';
        _exit(write(1,&answer,1)==1?0:3);
    }
    close(pipe_fds[1]);char answer=0;
    CHECK(read(pipe_fds[0],&answer,1)==1 && answer=='y');close(pipe_fds[0]);
    int status;CHECK(waitpid(pid,&status,0)==pid && WIFEXITED(status) && !WEXITSTATUS(status));
    CHECK(fcntl(canary,F_GETFD)>=0);close(canary);
}
int ae_test_getresuid(uid_t *r,uid_t *e,uid_t *s) {*r=model_uid;*e=model_euid;*s=model_suid;return 0;}
int ae_test_getresgid(gid_t *r,gid_t *e,gid_t *s) {*r=model_gid;*e=model_egid;*s=model_sgid;return 0;}
int ae_test_getpwuid_r(uid_t uid,struct passwd *p,char *buffer,size_t size,struct passwd **out) {
    CHECK(uid==model_uid && size==16384);buffer[0]=0;
    if(account_error)return account_error;
    *out=missing_account?NULL:p;
    *p=(struct passwd){.pw_uid=wrong_account?uid+1:uid,.pw_gid=model_gid,.pw_dir=(char *)home,.pw_name="original"};return 0;
}
int ae_test_prctl(int option,...) {
    va_list args;va_start(args,option);CHECK(option==PR_SET_NO_NEW_PRIVS && va_arg(args,int)==1);
    for(unsigned i=0;i<3;++i)CHECK(va_arg(args,int)==0);
    va_end(args);++nnp_calls;return nnp_error;
}
int ae_test_notify(int unset,const char *text) {CHECK(!unset && text && strstr(text,"READY=1\n"));++notifications;return notify_result;}
static void rejected(void) {char *parent=NULL,documents[4096];int before=nnp_calls;CHECK(!entry_paths(&parent,documents,sizeof(documents)));free(parent);CHECK(nnp_calls==before);}
int main(void) {
    descriptor_test(false);descriptor_test(true);
    char *parent=NULL,documents[4096];
    CHECK(!setenv("HOME","/forged/home",1) && !setenv("USER","forged",1) && !setenv("LOGNAME","forged",1) && !setenv("XDG_RUNTIME_DIR","/forged/runtime",1));
    CHECK(entry_paths(&parent,documents,sizeof(documents)) && !strcmp(parent,"/run/user/1201") && !strcmp(documents,"/home/original/Documents"));free(parent);parent=NULL;
    CHECK(!strcmp(getenv("HOME"),home) && !strcmp(getenv("USER"),"original") && !strcmp(getenv("LOGNAME"),"original") && nnp_calls==1);
    model_uid=0;rejected();model_uid=1201;
    model_euid=1202;rejected();model_euid=1201;
    model_suid=0;rejected();model_suid=1201;
    model_gid=0;rejected();model_gid=1301;
    model_egid=1302;rejected();model_egid=1301;
    model_sgid=0;rejected();model_sgid=1301;
    account_error=ERANGE;rejected();account_error=0;
    missing_account=true;rejected();missing_account=false;
    wrong_account=true;rejected();wrong_account=false;
    home="relative";rejected();home="/home/original";
    CHECK(!entry_paths(&parent,documents,4));free(parent);parent=NULL;
    nnp_error=-1;CHECK(!entry_paths(&parent,documents,sizeof(documents)));free(parent);nnp_error=0;
    CHECK(!unsetenv("NOTIFY_SOCKET") && ready_notification() && notifications==0);
    CHECK(!setenv("NOTIFY_SOCKET","/original/channel",1));notify_result=0;CHECK(!ready_notification());
    notify_result=-EIO;CHECK(!ready_notification());notify_result=1;CHECK(ready_notification() && notifications==3);
    CHECK(!setenv("NOTIFY_SOCKET","",1));notify_result=0;CHECK(!ready_notification() && notifications==4);
    puts("Session entry: original account/runtime, unequal/saved/root rejection, bounded paths, NNP and optional notification passed");return 0;
}
