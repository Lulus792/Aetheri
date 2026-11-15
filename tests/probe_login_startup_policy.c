#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <selinux/selinux.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

/* Explicit Root-only disposable-carrier probe; never an installed executable.
   Every target is synthetic and independently prepared. No policy write,
   password read, network connection or process signal is attempted. */
#define CHECK(x) do {if(!(x)){fprintf(stderr,"startup policy line %d: %s errno=%d\n",__LINE__,#x,errno);exit(1);}}while(0)
static void denied_file(const char *parent,const char *leaf,int flags) {
    char path[1024];int n=snprintf(path,sizeof path,"%s/%s",parent,leaf);
    CHECK(n>0 && (size_t)n<sizeof path);
    errno=0;int fd=open(path,flags|O_CLOEXEC|O_NOFOLLOW);
    CHECK(fd<0 && (errno==EACCES || errno==EPERM));
}
int main(int argc,char **argv) {
    CHECK(argc==3 && !getuid() && !geteuid() && !getgid() && !getegid());
    bool control=!strcmp(argv[1],"control");CHECK(control || !strcmp(argv[1],"confined"));
    char *context=NULL;CHECK(!getcon_raw(&context) && context);
    if(!control)CHECK(!strcmp(context,"system_u:system_r:aetheri_login_startup_t:s0"));
    freecon(context);CHECK(security_getenforce()==1);
    const char *root=argv[2];char path[1024];struct stat st;
    CHECK(root[0]=='/' && !lstat(root,&st) && S_ISDIR(st.st_mode) && !st.st_uid && !st.st_gid && !(st.st_mode&022));
    if(control) {
        for(size_t i=0;i<3;++i) {
            const char *leaf=i==0?"synthetic-shadow":i==1?"home/document":"outside/state";
            CHECK(snprintf(path,sizeof path,"%s/%s",root,leaf)>0);
            int fd=open(path,(i==0?O_RDONLY:O_RDWR)|O_CLOEXEC|O_NOFOLLOW);CHECK(fd>=0);
            char byte;CHECK(read(fd,&byte,1)==1 && byte=='s');CHECK(!close(fd));
        }
        int fd=socket(AF_INET,SOCK_STREAM|SOCK_CLOEXEC,0);CHECK(fd>=0 && !close(fd));
        fd=socket(AF_INET6,SOCK_DGRAM|SOCK_CLOEXEC,0);CHECK(fd>=0 && !close(fd));
        puts("AETHERI_LOGIN_POLICY_CONTROL synthetic_files=3 sockets=2");return 0;
    }
    denied_file(root,"synthetic-shadow",O_RDONLY);
    denied_file(root,"home/document",O_RDONLY);
    denied_file(root,"home/document",O_WRONLY);
    denied_file(root,"outside/state",O_WRONLY);
    denied_file("/sys/fs/selinux","load",O_WRONLY);
    denied_file("/sys/fs/selinux","enforce",O_WRONLY);
    for(int i=0;i<2;++i) {
        errno=0;int fd=socket(i?AF_INET6:AF_INET,(i?SOCK_DGRAM:SOCK_STREAM)|SOCK_CLOEXEC,0);
        CHECK(fd<0 && (errno==EACCES || errno==EPERM));
    }
    CHECK(snprintf(path,sizeof path,"%s/runtime",root)>0);
    int parent=open(path,O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW);CHECK(parent>=0);
    int fd=openat(parent,".",O_TMPFILE|O_WRONLY|O_CLOEXEC,0600);CHECK(fd>=0);
    CHECK(write(fd,"synthetic\n",10)==10);
    CHECK(!fsetfilecon_raw(fd,"system_u:object_r:aetheri_login_handoff_t:s0"));
    CHECK(!fchmod(fd,0444) && !fsync(fd));
    CHECK(!linkat(fd,"",parent,"probe-owned",AT_EMPTY_PATH) && !fsync(parent));
    CHECK(!close(fd) && !close(parent));
    puts("AETHERI_LOGIN_POLICY_PASS denied_files=6 denied_sockets=2 owned_publication=1");return 0;
}
