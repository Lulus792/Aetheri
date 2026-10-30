/* Inert execution-boundary payload, not a registered Aetheri app. */
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/capability.h>
#include <linux/securebits.h>
#include <poll.h>
#include <selinux/selinux.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/signalfd.h>
#include <sys/syscall.h>
#include <systemd/sd-bus.h>
#include <unistd.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"exec payload line %d\n",__LINE__); return 80; } } while (0)
extern char **environ;
int main(int argc,char **argv)
{
    const char *scenario=getenv("AETHERI_SCENARIO"); CHECK(scenario!=NULL);
    CHECK(argc==(strcmp(scenario,"arguments")==0 ? 9 : 1) && (strcmp(argv[0],"aetheri-exec-probe")==0 || strcmp(argv[0],"bin/probe")==0));
    for (int n=1;n<argc;++n) CHECK(strcmp(argv[n],"literal;$HOME")==0);
    uid_t r,e,s; gid_t gr,ge,gs;
    CHECK(getresuid(&r,&e,&s)==0 && r==1000 && e==1000 && s==1000);
    CHECK(getresgid(&gr,&ge,&gs)==0 && gr==1000 && ge==1000 && gs==1000 && getgroups(0,NULL)==0);
    CHECK(setuid(0)==-1 && errno==EPERM && setgid(0)==-1 && errno==EPERM);
    CHECK(prctl(PR_GET_NO_NEW_PRIVS,0L,0L,0L,0L)==1);
    struct __user_cap_header_struct h={.version=_LINUX_CAPABILITY_VERSION_3}; struct __user_cap_data_struct c[2]={{0}};
    CHECK(syscall(SYS_capget,&h,c)==0);
    for (size_t n=0;n<2;++n) CHECK(c[n].effective==0 && c[n].permitted==0 && c[n].inheritable==0);
    for (long n=0;n<=CAP_LAST_CAP;++n) CHECK(prctl(PR_CAPBSET_READ,n,0L,0L,0L)==0 && prctl(PR_CAP_AMBIENT,(long)PR_CAP_AMBIENT_IS_SET,n,0L,0L)==0);
    unsigned long required=SECBIT_NOROOT|SECBIT_NOROOT_LOCKED|SECBIT_KEEP_CAPS_LOCKED|SECBIT_NO_SETUID_FIXUP_LOCKED;
    CHECK((unsigned long)prctl(PR_GET_SECUREBITS,0L,0L,0L,0L)==required);
    struct rlimit core; CHECK(getrlimit(RLIMIT_CORE,&core)==0 && core.rlim_cur==0 && core.rlim_max==0);
    char *label=NULL; const char *expected=getenv("AETHERI_EXPECTED_CONTEXT");
    CHECK(expected!=NULL && getcon_raw(&label)==0 && strcmp(label,expected)==0); freecon(label);
    CHECK(getenv("LD_PRELOAD")==NULL && getenv("LD_LIBRARY_PATH")==NULL && getenv("HOME")==NULL);
    size_t variables=0; while (environ[variables]!=NULL) ++variables; CHECK(variables==3);
    sigset_t mask; CHECK(sigprocmask(SIG_SETMASK,NULL,&mask)==0);
    for (int n=1;n<NSIG;++n) CHECK(sigismember(&mask,n)==0);
    struct sigaction action; CHECK(sigaction(SIGTERM,NULL,&action)==0 && action.sa_handler==SIG_DFL);
    DIR *fds=opendir("/proc/self/fd"); CHECK(fds!=NULL); struct dirent *entry;
    while ((entry=readdir(fds))!=NULL) if (entry->d_name[0]!='.') {
        int fd=atoi(entry->d_name); CHECK(fd<3 || fd==dirfd(fds));
    }
    CHECK(closedir(fds)==0);
    int file=open("proof",O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);
    CHECK(file>=0 && write(file,"target\n",7)==7 && close(file)==0);
    file=open("./proof",O_RDONLY|O_CLOEXEC|O_NOFOLLOW); char bytes[8]={0};
    CHECK(file>=0 && read(file,bytes,sizeof bytes)==7 && memcmp(bytes,"target\n",7)==0 && close(file)==0);
    file=open("proof",O_RDONLY|O_CLOEXEC|O_NOFOLLOW); CHECK(file>=0);
    CHECK(mmap(NULL,4096,PROT_READ|PROT_EXEC,MAP_PRIVATE,file,0)==MAP_FAILED && (errno==EACCES || errno==EPERM));
    CHECK(close(file)==0);
    CHECK(unlink("proof")==0);
    if (strcmp(scenario,"fast-exit")==0) return 125;
    sigemptyset(&mask); sigaddset(&mask,SIGTERM); CHECK(sigprocmask(SIG_BLOCK,&mask,NULL)==0);
    int signal_fd=signalfd(-1,&mask,SFD_CLOEXEC); CHECK(signal_fd>=0);
    if (strcmp(scenario,"p0")==0) {
        /* Isolated live-starter probe only. A fresh post-exec connection supplies
         * actual app credentials; this is not production app registration. */
        sd_bus *bus=NULL; sd_bus_error error=SD_BUS_ERROR_NULL; sd_bus_message *reply=NULL;
        CHECK(sd_bus_open_system(&bus)>=0);
        int denied=sd_bus_call_method(bus,"org.aetheri.System1","/org/aetheri/P0ExecutionBrokerTest1","org.aetheri.P0ExecutionBrokerTest1","Resolve",&error,&reply,"");
        CHECK(denied<0 && sd_bus_error_has_name(&error,"org.aetheri.Error1.NotAuthorized"));
        sd_bus_error_free(&error); reply=sd_bus_message_unref(reply);
        int observed=sd_bus_call_method(bus,"org.aetheri.System1","/org/aetheri/P0ExecutionTest1","org.aetheri.P0ExecutionTest1","Observe",&error,&reply,"");
        if (observed<0) fprintf(stderr,"p0 Observe: %d %s\n",observed,error.message ? error.message : "-");
        CHECK(observed>=0); sd_bus_error_free(&error); sd_bus_message_unref(reply); sd_bus_flush_close_unref(bus);
    }
    CHECK(write(STDOUT_FILENO,"ready\n",6)==6);
    struct pollfd item={.fd=signal_fd,.events=POLLIN}; int result=poll(&item,1,5000);
    CHECK(result==1 && (item.revents&POLLIN)); CHECK(close(signal_fd)==0); return 0;
}
