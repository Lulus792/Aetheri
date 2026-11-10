#define _GNU_SOURCE
#include "login/connection.h"
#include <errno.h>
#include <fcntl.h>
#include <linux/magic.h>
#include <poll.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/pidfd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/vfs.h>
#include <unistd.h>

struct ae_login_connection {
    struct ae_login *model;
    struct ae_login_connection_source source;
    struct stat socket_identity,peer_identity;
    struct ucred credentials;
    pid_t owner,thread;
    int socket,peer,error;
    bool busy;
};
static int connection_owner(const struct ae_login_connection *s) {
    if(!s)return -EINVAL;
    if(getpid()!=s->owner)return -ECHILD;
    if(gettid()!=s->thread)return -EXDEV;
    return s->busy?-EBUSY:0;
}
static int connection_fence(struct ae_login_connection *s,int error) {
    if(!s->error)s->error=error<0?error:-EIO;
    ae_login_interrupted(s->model);return s->error;
}
static int process_live(int fd) {
    struct pollfd p={.fd=fd,.events=POLLIN};int r=poll(&p,1,0);
    if(r<0)return -errno;
    return r?-ESRCH:0;
}
static int process_info(int fd,struct pidfd_info *info) {
    *info=(struct pidfd_info){.mask=PIDFD_INFO_PID|PIDFD_INFO_CREDS};
    struct statfs fs;
    if(fstatfs(fd,&fs))return -errno;
    if((unsigned long)fs.f_type!=(unsigned long)PID_FS_MAGIC)return -EPROTO;
    if(ioctl(fd,PIDFD_GET_INFO,info))return -errno;
    uint64_t required=PIDFD_INFO_PID|PIDFD_INFO_CREDS;
    if((info->mask&required)!=required || !info->pid || info->pid!=info->tgid)return -ENODATA;
    return process_live(fd);
}
static bool same_object(const struct stat *a,const struct stat *b) {
    return a->st_dev==b->st_dev && a->st_ino==b->st_ino && a->st_mode==b->st_mode;
}
static int connection_verify(struct ae_login_connection *s) {
    struct stat socket_id,peer_id;struct pidfd_info info;
    if(fstat(s->socket,&socket_id) || fstat(s->peer,&peer_id))return -errno;
    if(!same_object(&socket_id,&s->socket_identity) || !same_object(&peer_id,&s->peer_identity))return -ESTALE;
    int r=process_info(s->peer,&info);if(r<0)return r;
    if(info.pid!=(uint32_t)s->credentials.pid ||
       info.ruid!=s->source.uid || info.euid!=s->source.uid ||
       info.suid!=s->source.uid || info.fsuid!=s->source.uid ||
       info.rgid!=s->source.gid || info.egid!=s->source.gid ||
       info.sgid!=s->source.gid || info.fsgid!=s->source.gid)return -EACCES;
    r=s->source.check(s->source.userdata);
    if(r)return r<0?r:-EACCES;
    return process_live(s->peer);
}
static int connection_enter(struct ae_login_connection *s) {
    int r=connection_owner(s);if(r<0)return r;
    if(s->error)return s->error;
    s->busy=true;r=connection_verify(s);
    if(r<0){s->busy=false;return connection_fence(s,r);}return 0;
}
static int socket_option(int fd,int option,int wanted) {
    int value=0;socklen_t size=sizeof(value);
    if(getsockopt(fd,SOL_SOCKET,option,&value,&size))return -errno;
    return size==sizeof(value) && value==wanted?0:-EPROTO;
}
int ae_login_connection_create(struct ae_login_connection **out,int *fd,
                               const struct ae_login_connection_source *source) {
    if(!out || *out || !fd || *fd<0 || !source || source->peer_pidfd<0 || !source->check)return -EINVAL;
    int r=socket_option(*fd,SO_DOMAIN,AF_UNIX);
    if(!r)r=socket_option(*fd,SO_TYPE,SOCK_STREAM);
    if(!r)r=socket_option(*fd,SO_ACCEPTCONN,0);
    if(r<0)return r;
    struct sockaddr_un address;socklen_t size=sizeof(address);
    if(getpeername(*fd,(struct sockaddr *)&address,&size))return -errno;
    if(size<sizeof(address.sun_family) || size>sizeof(address) || address.sun_family!=AF_UNIX)return -EPROTO;
    struct ae_login_connection *s=calloc(1,sizeof(*s));if(!s)return -ENOMEM;
    s->socket=s->peer=-1;s->source=*source;s->source.peer_pidfd=-1;s->owner=getpid();s->thread=gettid();
    s->socket=fcntl(*fd,F_DUPFD_CLOEXEC,3);if(s->socket<0){r=-errno;goto failed;}
    size=sizeof(s->credentials);
    if(getsockopt(s->socket,SOL_SOCKET,SO_PEERCRED,&s->credentials,&size)){r=-errno;goto failed;}
    if(size!=sizeof(s->credentials) || s->credentials.pid<=0 ||
       s->credentials.uid!=source->uid || s->credentials.gid!=source->gid){r=-EACCES;goto failed;}
    size=sizeof(s->peer);
    if(getsockopt(s->socket,SOL_SOCKET,SO_PEERPIDFD,&s->peer,&size)){r=-errno;goto failed;}
    if(size!=sizeof(s->peer) || s->peer<0){r=-ENODATA;goto failed;}
    if(fcntl(s->peer,F_GETFD)<0 || !(fcntl(s->peer,F_GETFD)&FD_CLOEXEC)){r=-EPROTO;goto failed;}
    struct stat expected;struct pidfd_info info;
    r=process_info(source->peer_pidfd,&info);if(r<0)goto failed;
    if(fstat(source->peer_pidfd,&expected) || fstat(s->peer,&s->peer_identity) || fstat(s->socket,&s->socket_identity)){r=-errno;goto failed;}
    if(!same_object(&expected,&s->peer_identity)){r=-ESTALE;goto failed;}
    r=ae_login_create(&s->model);if(r<0)goto failed;
    s->busy=true;r=connection_verify(s);s->busy=false;if(r<0)goto failed;
    close(*fd);*fd=-1;*out=s;return 0;
failed:
    if(s->socket>=0)close(s->socket);
    if(s->peer>=0)close(s->peer);
    ae_login_close(&s->model);free(s);return r;
}
int ae_login_connection_close(struct ae_login_connection **out) {
    if(!out)return -EINVAL;
    if(!*out)return 0;
    struct ae_login_connection *s=*out;
    if(getpid()==s->owner){int r=connection_owner(s);if(r<0)return r;}
    *out=NULL;close(s->socket);close(s->peer);ae_login_close(&s->model);free(s);return 0;
}
int ae_login_connection_begin(struct ae_login_connection *s,const char *text,size_t size) {
    int r=connection_enter(s);if(r<0)return r;
    r=ae_login_begin(s->model,text,size);s->busy=false;return r;
}
int ae_login_connection_answer(struct ae_login_connection *s,const char *text,size_t size) {
    int r=connection_enter(s);if(r<0)return r;
    r=ae_login_answer(s->model,text,size);s->busy=false;return r;
}
int ae_login_connection_cancel(struct ae_login_connection *s) {
    int r=connection_enter(s);if(r<0)return r;
    r=ae_login_cancel(s->model);s->busy=false;return r;
}
int ae_login_connection_start(struct ae_login_connection *s) {
    int r=connection_enter(s);if(r<0)return r;
    r=ae_login_start(s->model);s->busy=false;return r;
}
int ae_login_connection_status(struct ae_login_connection *s,struct ae_login_status *out) {
    int r=connection_owner(s);return r<0?r:ae_login_status(s->model,out);
}
int ae_login_connection_watch(struct ae_login_connection *s,int *fd,short *events,int *peer) {
    if(!fd || !events || !peer)return -EINVAL;
    int r=connection_enter(s);if(r<0)return r;
    struct ae_login_status v;ae_login_status(s->model,&v);
    *fd=s->socket;*peer=s->peer;*events=(short)(POLLIN|(v.phase==AE_LOGIN_SENDING?POLLOUT:0));s->busy=false;return 0;
}
/* A kernel recvmsg validates ancillary layout. Close every delivered descriptor
   before rejecting, including truncated messages; the kernel closes undisclosed
   truncated rights. No ancillary kind is part of this protocol. */
static bool connection_control(struct msghdr *message) {
    bool invalid=message->msg_controllen!=0 || (message->msg_flags&(MSG_CTRUNC|MSG_TRUNC))!=0;
    for(struct cmsghdr *c=CMSG_FIRSTHDR(message);c;c=CMSG_NXTHDR(message,c)) {
        if(c->cmsg_level!=SOL_SOCKET || (c->cmsg_type!=SCM_RIGHTS && c->cmsg_type!=SCM_PIDFD))continue;
        if(c->cmsg_len<CMSG_LEN(0))continue;
        size_t count=(c->cmsg_len-CMSG_LEN(0))/sizeof(int);
        for(size_t i=0;i<count;++i){int fd;memcpy(&fd,CMSG_DATA(c)+i*sizeof(fd),sizeof(fd));if(fd>=0)close(fd);}
    }
    return invalid;
}
int ae_login_connection_pump(struct ae_login_connection *s) {
    int r=connection_enter(s);if(r<0)return r;
    uint8_t bytes[4096];union {struct cmsghdr align;unsigned char bytes[CMSG_SPACE(256*sizeof(int))+CMSG_SPACE(sizeof(struct ucred))];} control;
    struct iovec iov={.iov_base=bytes,.iov_len=sizeof(bytes)};
    struct msghdr message={.msg_iov=&iov,.msg_iovlen=1,.msg_control=control.bytes,.msg_controllen=sizeof(control.bytes)};
    ssize_t n=recvmsg(s->socket,&message,MSG_DONTWAIT|MSG_CMSG_CLOEXEC);
    if(n>=0){
        bool invalid=connection_control(&message);
        r=invalid?-EPROTO:n==0?-ECONNRESET:connection_verify(s);
        if(!r)r=ae_login_receive(s->model,bytes,(size_t)n);
        volatile uint8_t *p=bytes;for(size_t i=0;i<sizeof(bytes);++i)p[i]=0;
        if(r<0)goto failed;
        s->busy=false;return r;
    }
    if(errno!=EAGAIN && errno!=EWOULDBLOCK && errno!=EINTR){r=-errno;goto failed;}
    if(errno==EINTR){s->busy=false;return 0;}
    const uint8_t *output=NULL;size_t size=0;
    if(!ae_login_output(s->model,&output,&size)) {
        r=connection_verify(s);if(r<0)goto failed;
        n=send(s->socket,output,size,MSG_DONTWAIT|MSG_NOSIGNAL);
        if(n>0){r=ae_login_sent(s->model,(size_t)n);if(r<0)goto failed;}
        else if(n==0){r=-EPIPE;goto failed;}
        else if(errno!=EAGAIN && errno!=EWOULDBLOCK && errno!=EINTR){r=-errno;goto failed;}
    }
    s->busy=false;return 0;
failed:
    s->busy=false;return connection_fence(s,r);
}
int ae_login_connection_interrupt(struct ae_login_connection *s) {
    int r=connection_owner(s);return r<0?r:connection_fence(s,-ECANCELED);
}
