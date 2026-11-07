#define _GNU_SOURCE
#include "linux/image.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/capability.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <unistd.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"self image %d: %s\n",__LINE__,#x); exit(1); } } while (0)
static unsigned handles(void)
{
    DIR *d=opendir("/proc/self/fd"); CHECK(d); struct dirent *e; unsigned n=0;
    while ((e=readdir(d))) if (e->d_name[0]!='.') ++n;
    CHECK(closedir(d)==0); return n;
}
int main(int argc,char **argv)
{
    CHECK(argc==6 && strlen(argv[2])==64);
    CHECK(prctl(PR_SET_NO_NEW_PRIVS,1L,0L,0L,0L)==0);
    struct __user_cap_header_struct header={.version=_LINUX_CAPABILITY_VERSION_3};
    struct __user_cap_data_struct caps[2]={{0}};
    CHECK(syscall(SYS_capset,&header,caps)==0);
    CHECK(syscall(SYS_capget,&header,caps)==0);
    for (size_t i=0;i<2;++i) CHECK(!caps[i].effective && !caps[i].permitted && !caps[i].inheritable);
    uint8_t digest[32];
    for (size_t i=0;i<32;++i) {
        char pair[3]={argv[2][i*2],argv[2][i*2+1],0},*end;
        unsigned long v=strtoul(pair,&end,16); CHECK(!*end && v<=255); digest[i]=(uint8_t)v;
    }
    char *end; unsigned long uid=strtoul(argv[3],&end,10); CHECK(!*end && uid<=1001);
    unsigned long gid=strtoul(argv[4],&end,10); CHECK(!*end && gid<=1001);
    int fd=open(argv[1],O_RDONLY|O_CLOEXEC|O_NOFOLLOW); CHECK(fd>=0);
    struct ae_image expected,actual,marker; memset(&marker,0xa5,sizeof marker);
    CHECK(ae_image_verify(fd,digest,&expected)==0); CHECK(close(fd)==0);
    int wanted=0;
    if (!strcmp(argv[5],"foreign-object")) wanted=-ESTALE;
    else if (!strcmp(argv[5],"wrong-digest")) { expected.digest[0]^=1; wanted=-EBADMSG; }
    else if (!strcmp(argv[5],"wrong-uid") || !strcmp(argv[5],"wrong-gid")) wanted=-EACCES;
    else CHECK(!strcmp(argv[5],"current"));
    unsigned before=handles();
    for (int n=0;n<12;++n) {
        actual=marker;
        CHECK(ae_image_match_self((uid_t)uid,(gid_t)gid,&expected,&actual)==wanted);
        CHECK(wanted ? !memcmp(&actual,&marker,sizeof actual) : ae_image_equal(&actual,&expected));
        CHECK(handles()==before);
    }
    actual=marker;
    CHECK(ae_image_match_self((uid_t)-1,(gid_t)gid,&expected,&actual)==-EINVAL);
    CHECK(ae_image_match_self((uid_t)uid,(gid_t)-1,&expected,&actual)==-EINVAL);
    CHECK(ae_image_match_self((uid_t)uid,(gid_t)gid,NULL,&actual)==-EINVAL);
    CHECK(ae_image_match_self((uid_t)uid,(gid_t)gid,&expected,NULL)==-EINVAL);
    struct ae_image malformed=expected; malformed.mode=0;
    CHECK(ae_image_match_self((uid_t)uid,(gid_t)gid,&malformed,&actual)==-EINVAL);
    CHECK(!memcmp(&actual,&marker,sizeof actual) && handles()==before);
    struct rlimit old,limited; CHECK(getrlimit(RLIMIT_NOFILE,&old)==0); limited=old; limited.rlim_cur=0;
    CHECK(setrlimit(RLIMIT_NOFILE,&limited)==0);
    CHECK(ae_image_match_self((uid_t)uid,(gid_t)gid,&expected,&actual)==-EMFILE && !memcmp(&actual,&marker,sizeof actual));
    CHECK(setrlimit(RLIMIT_NOFILE,&old)==0 && handles()==before);
    struct ae_peer remote=AE_PEER_INIT; remote.connection_uid=0; remote.connection_pid=getpid();
    remote.pidfd=(int)syscall(SYS_pidfd_open,getpid(),0); CHECK(remote.pidfd>=0); strcpy(remote.connection_label,"untrusted");
    CHECK(ae_image_match_process(&remote,0,&expected,&actual)==-EINVAL && !memcmp(&actual,&marker,sizeof actual));
    ae_peer_clear(&remote); CHECK(handles()==before);
    printf("SELF_IMAGE uid=%u gid=%u case=%s repeats=12 capabilities=empty\n",(unsigned)geteuid(),(unsigned)getegid(),argv[5]);
    return 0;
}
