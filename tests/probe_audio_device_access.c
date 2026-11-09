/* Fixed administrative carrier control, not a normal application device API. */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <linux/capability.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/sysmacros.h>
#include <unistd.h>

static bool ordinary(void) {
    struct __user_cap_header_struct h={.version=_LINUX_CAPABILITY_VERSION_3};struct __user_cap_data_struct d[2]={{0}};
    gid_t group=0;int groups=getgroups(1,&group);
    return getuid()==1000 && geteuid()==1000 && getgid()==1000 && getegid()==1000 && (groups==0 || (groups==1 && group==1000)) &&
        prctl(PR_GET_NO_NEW_PRIVS,0,0,0,0)==1 && !syscall(SYS_capget,&h,d) &&
        !(d[0].effective|d[0].permitted|d[0].inheritable|d[1].effective|d[1].permitted|d[1].inheritable);
}
int main(int argc,char **argv) {
    if(argc!=2 || (strcmp(argv[1],"--allow") && strcmp(argv[1],"--deny")) || !ordinary()) return 2;
    if(syscall(SYS_close_range,3u,~0u,0)) return 2;
    bool deny=!strcmp(argv[1],"--deny");alarm(10);setvbuf(stdout,NULL,_IOLBF,0);
    const char *paths[]={"/dev/snd/controlC0","/dev/snd/pcmC0D0p"};
    for(unsigned i=0;i<2;++i) {
        int fd=open(paths[i],O_RDWR|O_NONBLOCK|O_NOFOLLOW|O_CLOEXEC),error=errno;
        if(deny) {
            if(fd>=0) {close(fd);return 1;}
            if(error!=EACCES) return 1;
        } else {
            struct stat st;bool valid=fd>=0 && !fstat(fd,&st) && S_ISCHR(st.st_mode) && major(st.st_rdev)==116;
            if(fd>=0) close(fd);
            if(!valid) return 1;
        }
        printf("DEVICE_OPEN_%s uid=1000 path=%s\n",deny ? "DENIED" : "ALLOWED",paths[i]);
    }
    return 0;
}
