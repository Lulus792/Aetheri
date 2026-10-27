#include "linux/exec_channel.h"
#include <errno.h>
#include <fcntl.h>
#include <linux/magic.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <unistd.h>

struct ae_exec_channel {
    pid_t owner;
    int reader;
    dev_t device;
    ino_t inode;
    size_t used;
    uint8_t bytes[17];
    struct ae_exec_report report;
};
static int pipe_check(int fd,int access,struct stat *output)
{
    if (fd<3) return -EINVAL;
    int flags=fcntl(fd,F_GETFL); if (flags<0) return -errno;
    if ((flags&O_ACCMODE)!=access || !(flags&O_NONBLOCK) || (flags&O_DIRECT)) return -EACCES;
    flags=fcntl(fd,F_GETFD); if (flags<0) return -errno;
    if (!(flags&FD_CLOEXEC)) return -EACCES;
    struct stat s; struct statfs fs;
    if (fstat(fd,&s)!=0 || fstatfs(fd,&fs)!=0) return -errno;
    if (!S_ISFIFO(s.st_mode) || (s.st_mode&07777)!=0600 || s.st_nlink!=1 || fs.f_type!=PIPEFS_MAGIC) return -EACCES;
    if (output) *output=s;
    return 0;
}
int ae_exec_writer_check(int fd,uid_t uid,gid_t gid)
{
    struct stat s; int result=pipe_check(fd,O_WRONLY,&s);
    if (result==0 && (s.st_uid!=uid || s.st_gid!=gid)) result=-EACCES;
    return result;
}
int ae_exec_channel_new(struct ae_exec_channel **output,int *writer)
{
    if (output==NULL || *output!=NULL || writer==NULL) return -EINVAL;
    struct ae_exec_channel *c=calloc(1,sizeof *c); if (c==NULL) return -ENOMEM;
    int fds[2]={-1,-1}; int result=pipe2(fds,O_CLOEXEC|O_NONBLOCK)==0 ? 0 : -errno;
    struct stat s;
    if (result==0) result=pipe_check(fds[0],O_RDONLY,&s);
    if (result==0) result=ae_exec_writer_check(fds[1],geteuid(),getegid());
    if (result!=0) {
        if (fds[0]>=0) close(fds[0]);
        if (fds[1]>=0) close(fds[1]);
        free(c); return result;
    }
    c->owner=getpid(); c->reader=fds[0]; c->device=s.st_dev; c->inode=s.st_ino;
    *writer=fds[1]; *output=c; return 0;
}
int ae_exec_channel_fd(const struct ae_exec_channel *c)
{
    if (c==NULL) return -EINVAL;
    return c->owner==getpid() ? c->reader : -ECHILD;
}
static uint32_t number(const uint8_t *bytes)
{ uint32_t value=0; for (size_t n=0;n<4;++n) value=(value<<8)|bytes[n]; return value; }
static int frame(const struct ae_exec_channel *c,struct ae_exec_report *r)
{
    if (c->used!=16 || memcmp(c->bytes,"AETHXER1",8)!=0) return -EBADMSG;
    uint32_t phase=number(c->bytes+8),error=number(c->bytes+12);
    if (phase<AE_EXEC_PROFILE || phase>AE_EXEC_CALL || error<1 || error>4095) return -EBADMSG;
    *r=(struct ae_exec_report){.state=AE_EXEC_CHANNEL_FAILURE,.phase=(enum ae_exec_phase)phase,.error=(int)error}; return 0;
}
int ae_exec_channel_read(struct ae_exec_channel *c,struct ae_exec_report *output)
{
    if (c==NULL || output==NULL) return -EINVAL;
    if (c->owner!=getpid()) return -ECHILD;
    if (c->report.state==AE_EXEC_CHANNEL_PENDING) {
        struct stat s; int result=pipe_check(c->reader,O_RDONLY,&s);
        if (result==0 && (s.st_dev!=c->device || s.st_ino!=c->inode)) result=-ESTALE;
        if (result!=0) return result;
        for (int attempt=0;attempt<2;++attempt) {
            ssize_t size=read(c->reader,c->bytes+c->used,sizeof c->bytes-c->used);
            if (size<0) {
                if (errno==EAGAIN || errno==EWOULDBLOCK || errno==EINTR) break;
                return -errno;
            }
            if (size==0) {
                if (c->used==0) c->report.state=AE_EXEC_CHANNEL_UNKNOWN;
                else if (frame(c,&c->report)!=0) c->report.state=AE_EXEC_CHANNEL_MALFORMED;
                break;
            }
            c->used+=(size_t)size;
            struct ae_exec_report ignored;
            if (c->used>16 || (c->used==16 && frame(c,&ignored)!=0)) {
                c->report.state=AE_EXEC_CHANNEL_MALFORMED; break;
            }
        }
    }
    *output=c->report; return 0;
}
int ae_exec_channel_close(struct ae_exec_channel **output)
{
    if (output==NULL) return -EINVAL;
    struct ae_exec_channel *c=*output; if (c==NULL) return 0;
    if (c->owner!=getpid()) return -ECHILD;
    int result=close(c->reader)==0 ? 0 : -errno;
    free(c); *output=NULL; return result;
}
