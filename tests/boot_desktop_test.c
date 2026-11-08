/* Actual child/pidfd/barrier and ancillary-FD probes, never app admission. */
#include <stdbool.h>
static bool inject_pin_failure;
#define pidfd_open ae_test_pin_open
#define main ae_boot_desktop_main
#include "../src/boot/desktop.c"
#undef main
#undef pidfd_open
#include <assert.h>
#include <dirent.h>
#include <sys/syscall.h>

int ae_test_pin_open(pid_t pid,unsigned int flags) {
    if(inject_pin_failure){errno=EMFILE;return -1;}
    return (int)syscall(SYS_pidfd_open,pid,flags);
}
static size_t fd_count(void) {
    DIR *d=opendir("/proc/self/fd");assert(d);size_t count=0;
    while(readdir(d))++count;
    assert(!closedir(d));return count;
}
static void wait_original(struct child *c) {
    double deadline=now()+5;
    while(!ended(c) && now()<deadline){
        struct pollfd fds[2]={{.fd=c->output,.events=POLLIN},{.fd=c->pin,.events=POLLIN}};
        assert(poll(fds,2,50)>=0);
        if(c->output>=0 && fds[0].revents)assert(drain_line(c,"child"));
        assert(reap(c));
    }
    assert(ended(c) && success(c));
    assert(close_fd(&c->pin));
}
int main(int argc,char **argv) {
    if(argc==4 && !strcmp(argv[1],"--marker")){
        int unexpected=atoi(argv[3]);
        if(fcntl(unexpected,F_GETFD)>=0 || errno!=EBADF)return 8;
        int marker=open(argv[2],O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC,0600);
        if(marker<0 || write(marker,"executed\n",9)!=9 || close(marker))return 9;
        return 0;
    }
    assert(argc==1 && getuid()==1000);
    signal(SIGPIPE,SIG_IGN);
    size_t before=fd_count();
    int pair[2];assert(!socketpair(AF_UNIX,SOCK_SEQPACKET|SOCK_CLOEXEC,0,pair));
    assert(send(pair[0],"L3S",3,0)==3 && packet(pair[1])==SETTINGS+1);
    assert(send(pair[0],"Q30",3,0)==3 && packet(pair[1])==100);
    assert(send(pair[0],"L3X",3,0)==3 && packet(pair[1])==-1);
    assert(send(pair[0],"L3Sxxxxxx",9,0)==9 && packet(pair[1])==-1);
    int source=open("/dev/null",O_RDONLY|O_CLOEXEC);assert(source>=0);
    union {struct cmsghdr align;char bytes[CMSG_SPACE(sizeof(int))];} control={0};
    struct iovec iov={.iov_base="L3S",.iov_len=3};
    struct msghdr message={.msg_iov=&iov,.msg_iovlen=1,.msg_control=control.bytes,.msg_controllen=sizeof(control.bytes)};
    struct cmsghdr *h=CMSG_FIRSTHDR(&message);h->cmsg_level=SOL_SOCKET;h->cmsg_type=SCM_RIGHTS;h->cmsg_len=CMSG_LEN(sizeof(int));memcpy(CMSG_DATA(h),&source,sizeof(source));
    size_t expected=fd_count();assert(sendmsg(pair[0],&message,0)==3);
    assert(packet(pair[1])==-1 && fd_count()==expected);
    assert(!close(source) && !close(pair[0]));assert(packet(pair[1])==-1);assert(!close(pair[1]));
    char directory[]="/tmp/aetheri-desktop-test-XXXXXX";assert(mkdtemp(directory));
    char path[256];int n=snprintf(path,sizeof(path),"%s/marker",directory);assert(n>0 && (size_t)n<sizeof(path));
    int original=open("/dev/null",O_RDONLY);assert(original>=0);
    int unrelated=fcntl(original,F_DUPFD,100);assert(unrelated>=100 && !close(original));
    char number[32];snprintf(number,sizeof(number),"%d",unrelated);
    char *target[]={"/proc/self/exe","--marker",path,number,NULL};
    struct child c;empty(&c);
    inject_pin_failure=true;assert(!spawn(&c,target,NULL,0));inject_pin_failure=false;
    assert(!c.pid && access(path,F_OK)<0 && errno==ENOENT);
    assert(spawn(&c,target,NULL,0) && c.pid>0 && c.pin>=0);
    wait_original(&c);assert(fcntl(unrelated,F_GETFD)>=0);assert(!close(unrelated));
    int marker=open(path,O_RDONLY|O_CLOEXEC);char bytes[16]={0};assert(marker>=0 && read(marker,bytes,sizeof(bytes))==9 && !strcmp(bytes,"executed\n"));assert(!close(marker));
    assert(!unlink(path) && !rmdir(directory));assert(fd_count()==before);
    puts("Desktop owner: packet/FD denial, no-exec pin failure, original child and FD closure passed");
    return 0;
}
