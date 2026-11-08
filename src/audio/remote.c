#define _GNU_SOURCE
#include "audio/remote.h"
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/pidfd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

int ae_audio_remote_connect(const char *path) {
    struct sockaddr_un address={.sun_family=AF_UNIX};
    if(!path || path[0]!='/' || strlen(path)>=sizeof(address.sun_path)) {errno=EINVAL;return -1;}
    memcpy(address.sun_path,path,strlen(path)+1);
    int fd=socket(AF_UNIX,SOCK_STREAM|SOCK_CLOEXEC|SOCK_NONBLOCK,0);
    if(fd<0) return -1;
    struct ucred peer={0};socklen_t size=sizeof(peer);
    if(connect(fd,(struct sockaddr *)&address,(socklen_t)(offsetof(struct sockaddr_un,sun_path)+strlen(path)+1)) ||
       getsockopt(fd,SOL_SOCKET,SO_PEERCRED,&peer,&size)) {
        int error=errno;(void)close(fd);errno=error;return -1;
    }
    if(size!=sizeof(peer) || peer.uid!=getuid() || peer.pid<=0) {(void)close(fd);errno=EPERM;return -1;}
    return fd;
}

struct observation {struct pidfd_info info;struct stat object;};
static int same_info(const struct pidfd_info *a,const struct pidfd_info *b) {
    return a->pid==b->pid && a->tgid==b->tgid && a->cgroupid==b->cgroupid &&
        a->ruid==b->ruid && a->euid==b->euid && a->suid==b->suid && a->fsuid==b->fsuid &&
        a->rgid==b->rgid && a->egid==b->egid && a->sgid==b->sgid && a->fsgid==b->fsgid;
}
static int info(int fd,struct pidfd_info *out) {
    uint64_t required=PIDFD_INFO_PID|PIDFD_INFO_CREDS|PIDFD_INFO_CGROUPID;
    *out=(struct pidfd_info){.mask=required};
    if(ioctl(fd,PIDFD_GET_INFO,out)) return -1;
    if((out->mask&required)!=required || !out->pid || out->pid!=out->tgid || !out->cgroupid ||
       out->ruid!=getuid() || out->euid!=getuid() || out->suid!=getuid() || out->fsuid!=getuid() ||
       out->rgid!=getgid() || out->egid!=getgid() || out->sgid!=getgid() || out->fsgid!=getgid()) {errno=EACCES;return -1;}
    return 0;
}
static int observe(int fd,struct observation *out) {
    struct pidfd_info after;
    _Static_assert(sizeof(out->object.st_ino)==sizeof(uint64_t),"64-bit pidfs inode required");
    if(info(fd,&out->info) || fstat(fd,&out->object) || info(fd,&after)) return -1;
    if(!out->object.st_ino || !same_info(&out->info,&after)) {errno=ESTALE;return -1;}
    struct pollfd p={fd,POLLIN,0};int result;
    do {result=poll(&p,1,0);} while(result<0 && errno==EINTR);
    if(result) {if(result>0) errno=ESRCH;return -1;}
    return 0;
}
static int same_observation(const struct observation *a,const struct observation *b) {
    return a->object.st_dev==b->object.st_dev && a->object.st_ino==b->object.st_ino && same_info(&a->info,&b->info);
}
int ae_audio_remote_connect_original(const char *path,int original_pidfd) {
    if(original_pidfd<0 || getuid()!=geteuid() || getgid()!=getegid()) {errno=EINVAL;return -1;}
    int original=fcntl(original_pidfd,F_DUPFD_CLOEXEC,3);if(original<0) return -1;
    int socket_fd=-1,peer=-1;struct observation before,connected,after;
    if(observe(original,&before)) goto failed;
    socket_fd=ae_audio_remote_connect(path);if(socket_fd<0) goto failed;
    socklen_t size=sizeof(peer);
    if(getsockopt(socket_fd,SOL_SOCKET,SO_PEERPIDFD,&peer,&size)) goto failed;
    if(size!=sizeof(peer) || peer<0) {errno=EBADMSG;goto failed;}
    int flags=fcntl(peer,F_GETFD);if(flags<0) goto failed;
    if(!(flags&FD_CLOEXEC)) {errno=EPERM;goto failed;}
    struct ucred credentials;size=sizeof(credentials);
    if(getsockopt(socket_fd,SOL_SOCKET,SO_PEERCRED,&credentials,&size) || observe(peer,&connected) || observe(original,&after)) goto failed;
    if(size!=sizeof(credentials) || credentials.pid<=0 || (uint32_t)credentials.pid!=connected.info.pid ||
       credentials.uid!=connected.info.euid || credentials.gid!=connected.info.egid ||
       !same_observation(&before,&connected) || !same_observation(&before,&after)) {errno=ESTALE;goto failed;}
    close(peer);close(original);return socket_fd;
failed:;
    int error=errno;if(peer>=0) close(peer);if(socket_fd>=0) close(socket_fd);close(original);errno=error;return -1;
}
