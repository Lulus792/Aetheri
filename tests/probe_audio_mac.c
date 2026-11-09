#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <linux/capability.h>
#include <linux/memfd.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/mman.h>
#include <sys/xattr.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/un.h>
#include <unistd.h>

/* Fixture gateway, not an installed launcher/admission API. PID1 supplies
 * protected literal argv/domain and at most the original graph pidfd in FD3.
 * The root controller pins this exact service before releasing the barrier. */
static bool ordinary(void) {
    struct __user_cap_header_struct h={.version=_LINUX_CAPABILITY_VERSION_3};struct __user_cap_data_struct d[2]={{0}};
    gid_t group=0;int groups=getgroups(1,&group);
    return getuid()==1000 && geteuid()==1000 && getgid()==1000 && getegid()==1000 && (groups==0 || (groups==1 && group==1000)) &&
        prctl(PR_GET_NO_NEW_PRIVS,0,0,0,0)==1 && !syscall(SYS_capget,&h,d) &&
        !(d[0].effective|d[0].permitted|d[0].inheritable|d[1].effective|d[1].permitted|d[1].inheritable);
}
int main(int argc,char **argv) {
    bool execute=argc>=4 && !strcmp(argv[1],"exec") && (!strcmp(argv[2],"pin") || !strcmp(argv[2],"none"));
    bool foreign=argc==4 && !strcmp(argv[1],"foreign");
    bool hold=argc==2 && !strcmp(argv[1],"memory-hold");
    bool memory=argc==2 && (!strcmp(argv[1],"memory") || hold);
    if((!execute && !foreign && !memory) || !ordinary()) return 2;
    bool pin=execute && !strcmp(argv[2],"pin");
    if(syscall(SYS_close_range,pin ? 4u : 3u,UINT32_MAX,0)) return 2;
    sigset_t set;sigemptyset(&set);sigaddset(&set,SIGUSR1);if(sigprocmask(SIG_BLOCK,&set,NULL)) return 2;
    alarm(25);setvbuf(stdout,NULL,_IOLBF,0);
    char context[512];int label=open("/proc/self/attr/current",O_RDONLY|O_CLOEXEC);
    ssize_t n=label<0 ? -1 : read(label,context,sizeof(context)-1);
    if(label>=0) close(label);
    if(n<=0 || n>=(ssize_t)sizeof(context)-1) return 2;
    context[n]=0;context[strcspn(context,"\n")]=0;
    printf("AETHERI_AUDIO_MAC_READY uid=1000 nnp=1 cap_zero=1 label=%s\n",context);
    int number=0;if(sigwait(&set,&number) || number!=SIGUSR1) return 2;
    if(memory) {
        sigset_t finish;sigemptyset(&finish);sigaddset(&finish,SIGUSR2);
        if(hold && sigprocmask(SIG_BLOCK,&finish,NULL)) return 2;
        int fd=(int)syscall(SYS_memfd_create,"aetheri-audio-diagnostic",MFD_CLOEXEC|MFD_ALLOW_SEALING|MFD_NOEXEC_SEAL);
        if(fd<0) {printf("MEMORY create=-1 errno=%d\n",errno);return 1;}
        char value[512];ssize_t length=fgetxattr(fd,"security.selinux",value,sizeof(value)-1);
        int context_error=errno;if(length>=0) value[length]=0;else value[0]=0;
        errno=0;int resized=ftruncate(fd,4096),resize_error=errno;
        printf("MEMORY label=%s label_errno=%d resized=%d resize_errno=%d\n",value,length<0 ? context_error : 0,resized,resize_error);
        if(resized==0) {
            void *map=mmap(NULL,4096,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);int map_error=errno;
            printf("MEMORY mapped=%d map_errno=%d\n",map!=MAP_FAILED,map==MAP_FAILED ? map_error : 0);
            if(map!=MAP_FAILED) munmap(map,4096);
        }
        if(hold) {
            printf("MEMORY_HOLD fd=%d\n",fd);
            if(sigwait(&finish,&number) || number!=SIGUSR2) {close(fd);return 2;}
        }
        close(fd);return 0;
    }
    if(execute) {
        if(argv[3][0]!='/') return 2;
        if(unsetenv("LISTEN_PID") || unsetenv("LISTEN_FDS") || unsetenv("LISTEN_FDNAMES")) return 2;
        alarm(0);if(sigprocmask(SIG_UNBLOCK,&set,NULL)) return 2;
        execv(argv[3],argv+3);return 125;
    }
    int net=socket(AF_INET,SOCK_STREAM|SOCK_CLOEXEC,0),net_error=errno;
    if(net>=0) close(net);
    int secret=open(argv[3],O_RDONLY|O_CLOEXEC),secret_error=errno;
    if(secret>=0) close(secret);
    struct sockaddr_un address={.sun_family=AF_UNIX};
    if(argv[2][0]!='/' || strlen(argv[2])>=sizeof(address.sun_path)) return 2;
    strcpy(address.sun_path,argv[2]);int fd=socket(AF_UNIX,SOCK_STREAM|SOCK_CLOEXEC,0);if(fd<0) return 2;
    int connected=connect(fd,(struct sockaddr *)&address,sizeof(address)),connect_error=errno;close(fd);
    if(net>=0 || secret>=0 || connected==0 || net_error!=EACCES || secret_error!=EACCES || connect_error!=EACCES) {
        fprintf(stderr,"denials net=%d/%d secret=%d/%d connect=%d/%d\n",net,net_error,secret,secret_error,connected,connect_error);return 1;
    }
    puts("AETHERI_AUDIO_MAC_DENIED socket=peer-domain secret=mac tcp=mac uid=1000");return 0;
}
