#define _GNU_SOURCE
#include "files/launch.h"
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"files launch %d: %s errno=%d\n",__LINE__,#x,errno);exit(1); } } while (0)
static const char leaf[]="literal ' $(never-run)";
static int child(int argc,char **argv) {
    CHECK(argc==9 && !strcmp(argv[1],"--editor") && !strcmp(argv[2],"/independent-editor") &&
        !strcmp(argv[3],"--open-leaf") && !strcmp(argv[4],leaf) && !strcmp(argv[5],"--font") &&
        !strcmp(argv[6],"/independent-font") && !strcmp(argv[7],"--cache") && !strcmp(argv[8],"/independent-cache"));
    CHECK(!getenv("WAYLAND_SOCKET") && getpgrp()==getpid());
    sigset_t mask;CHECK(!sigprocmask(SIG_SETMASK,NULL,&mask) && !sigismember(&mask,SIGINT));
    struct stat cwd,dir,file;CHECK(!stat(".",&cwd) && !fstat(3,&dir) && S_ISDIR(dir.st_mode) && cwd.st_dev==dir.st_dev && cwd.st_ino==dir.st_ino);
    CHECK(!fstat(4,&file) && S_ISREG(file.st_mode) && (fcntl(4,F_GETFL)&O_PATH));
    errno=0;CHECK(fcntl(100,F_GETFD)<0 && errno==EBADF);
    int proof=openat(3,"child-proof",O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC,0600);CHECK(proof>=5);
    CHECK(write(proof,leaf,sizeof(leaf))==(ssize_t)sizeof(leaf) && !close(proof));
    int release=openat(3,"release",O_RDONLY|O_CLOEXEC);char byte;CHECK(release>=5 && read(release,&byte,1)==1 && byte=='q');CHECK(!close(release));return 0;
}
int main(int argc,char **argv) {
    if (argc>1) return child(argc,argv);
    CHECK(getuid()!=0);char path[]="files-launch-XXXXXX";CHECK(mkdtemp(path));CHECK(!chmod(path,0700));
    int dir=open(path,O_RDONLY|O_DIRECTORY|O_CLOEXEC);CHECK(dir>=3);
    int fd=openat(dir,leaf,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC,0600);CHECK(fd>=3 && write(fd,"hello",5)==5 && !close(fd));
    int witness=openat(dir,leaf,O_PATH|O_NOFOLLOW|O_CLOEXEC);CHECK(witness>=3 && !mkfifoat(dir,"release",0600));
    int release=openat(dir,"release",O_RDWR|O_NONBLOCK|O_CLOEXEC);CHECK(release>=3);
    int unwanted=fcntl(witness,F_DUPFD,100);CHECK(unwanted==100);
    const char *old=getenv("WAYLAND_SOCKET");char *saved=old ? strdup(old) : NULL;CHECK(!old || saved);CHECK(!setenv("WAYLAND_SOCKET","100",1));
    sigset_t blocked,prior;sigemptyset(&blocked);sigaddset(&blocked,SIGINT);CHECK(!sigprocmask(SIG_BLOCK,&blocked,&prior));
    struct ae_files_child c={.pid=-1,.pin=-1};
    CHECK(!ae_files_launch(&c,"/proc/self/exe","/independent-editor","/independent-font","/independent-cache",dir,witness,leaf,false));
    CHECK(c.pid>0 && c.pin>=0 && !c.ended && ae_files_child_observe(&c)==0);
    CHECK(ae_files_child_close(&c)<0 && errno==EBUSY);
    CHECK(ae_files_launch(&c,"/proc/self/exe","/independent-editor","/independent-font","/independent-cache",dir,witness,leaf,false)<0);
    CHECK(write(release,"q",1)==1);struct pollfd pending={c.pin,POLLIN,0};CHECK(poll(&pending,1,5000)==1);
    CHECK(ae_files_child_observe(&c)==1 && c.status==0 && ae_files_child_observe(&c)==1);
    int proof=openat(dir,"child-proof",O_RDONLY|O_NOFOLLOW|O_CLOEXEC);char result[sizeof(leaf)];CHECK(proof>=3 && read(proof,result,sizeof(result))==(ssize_t)sizeof(result) && !memcmp(result,leaf,sizeof(result)) && !close(proof));
    CHECK(!ae_files_child_close(&c) && c.pid==-1 && c.pin==-1);
    CHECK(fcntl(dir,F_GETFD)>=0 && fcntl(witness,F_GETFD)>=0 && fcntl(unwanted,F_GETFD)>=0);
    CHECK(!sigprocmask(SIG_SETMASK,&prior,NULL));if (saved) { CHECK(!setenv("WAYLAND_SOCKET",saved,1));free(saved); }else CHECK(!unsetenv("WAYLAND_SOCKET"));
    CHECK(!close(unwanted) && !close(release) && !close(witness) && !close(dir));
    printf("Files launch evidence: %s\n",path);puts("Files launch: literal argv, exact 3/4 FD handoff, closefrom, environment/mask/group and original-child ownership passed");return 0;
}
