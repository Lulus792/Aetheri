#include "linux/exec_channel.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <sys/wait.h>
#include <unistd.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"exec channel line %d: %s\n",__LINE__,#x); exit(1); } } while (0)
static int allocation_fault,pipe_fault,stat_fault,fs_fault,fcntl_fault,read_fault,read_error,close_fault,reads;
void *__real_calloc(size_t,size_t);
void *__wrap_calloc(size_t n,size_t size)
{ if (allocation_fault) { allocation_fault=0; errno=ENOMEM; return NULL; } return __real_calloc(n,size); }
int __real_pipe2(int *,int);
int __wrap_pipe2(int *fds,int flags)
{ if (pipe_fault) { pipe_fault=0; errno=EMFILE; return -1; } return __real_pipe2(fds,flags); }
int __real_fstat(int,struct stat *);
int __wrap_fstat(int fd,struct stat *s)
{ if (stat_fault && --stat_fault==0) { errno=EIO; return -1; } return __real_fstat(fd,s); }
int __real_fstatfs(int,struct statfs *);
int __wrap_fstatfs(int fd,struct statfs *s)
{ if (fs_fault && --fs_fault==0) { errno=EIO; return -1; } return __real_fstatfs(fd,s); }
int __real_fcntl(int,int,...);
int __wrap_fcntl(int fd,int command,...)
{
    if (fcntl_fault && --fcntl_fault==0) { errno=EIO; return -1; }
    if (command==F_GETFL || command==F_GETFD) return __real_fcntl(fd,command);
    CHECK(command==F_SETFL || command==F_SETFD || command==F_DUPFD_CLOEXEC);
    va_list args; va_start(args,command); int value=va_arg(args,int); va_end(args);
    return __real_fcntl(fd,command,value);
}
ssize_t __real_read(int,void *,size_t);
ssize_t __wrap_read(int fd,void *bytes,size_t size)
{
    ++reads;
    if (read_fault && --read_fault==0) { errno=read_error; return -1; }
    return __real_read(fd,bytes,size);
}
int __real_close(int);
int __wrap_close(int fd)
{
    int result=__real_close(fd);
    if (close_fault) { close_fault=0; CHECK(result==0); errno=EIO; return -1; }
    return result;
}
static int descriptors(void)
{
    DIR *d=opendir("/proc/self/fd"); CHECK(d); int count=0;
    while (readdir(d)) ++count;
    CHECK(closedir(d)==0); return count;
}
static void record(uint8_t bytes[16],uint32_t phase,uint32_t error)
{
    memcpy(bytes,"AETHXER1",8);
    for (size_t n=0;n<4;++n) { bytes[8+n]=(uint8_t)(phase>>(24-8*n)); bytes[12+n]=(uint8_t)(error>>(24-8*n)); }
}
static struct ae_exec_report observe(struct ae_exec_channel *c,enum ae_exec_channel_state state)
{
    struct ae_exec_report r; reads=0;
    CHECK(ae_exec_channel_read(c,&r)==0 && r.state==state && reads<=2);
    if (state!=AE_EXEC_CHANNEL_FAILURE) CHECK(!r.phase && !r.error);
    return r;
}
static void stream(const uint8_t *bytes,size_t size,enum ae_exec_channel_state expected)
{
    struct ae_exec_channel *c=NULL; int writer=-1;
    CHECK(ae_exec_channel_new(&c,&writer)==0);
    CHECK(ae_exec_writer_check(writer,geteuid(),getegid())==0);
    observe(c,AE_EXEC_CHANNEL_PENDING);
    if (size) CHECK(write(writer,bytes,size)==(ssize_t)size);
    CHECK(close(writer)==0);
    struct ae_exec_report r=observe(c,expected);
    if (expected==AE_EXEC_CHANNEL_FAILURE) CHECK((uint32_t)r.phase==bytes[11] && r.error==(int)(((uint32_t)bytes[14]<<8)|bytes[15]));
    int fd=ae_exec_channel_fd(c); CHECK(fd>=3);
    r=observe(c,expected); CHECK(reads==0);
    CHECK(ae_exec_channel_close(&c)==0 && c==NULL && fcntl(fd,F_GETFD)==-1 && errno==EBADF);
}
int main(int argc,char **argv)
{
    CHECK(argc==2); alarm(15); int baseline=descriptors();
    uint8_t bytes[32]={0}; record(bytes,AE_EXEC_CALL,ENOEXEC);
    stream(NULL,0,AE_EXEC_CHANNEL_UNKNOWN);
    for (size_t n=1;n<16;++n) stream(bytes,n,AE_EXEC_CHANNEL_MALFORMED);
    for (uint32_t n=AE_EXEC_PROFILE;n<=AE_EXEC_CALL;++n) { record(bytes,n,1); stream(bytes,16,AE_EXEC_CHANNEL_FAILURE); }
    record(bytes,AE_EXEC_CALL,4095); stream(bytes,16,AE_EXEC_CHANNEL_FAILURE);
    const uint32_t invalid[][2]={{0,EIO},{11,EIO},{UINT32_MAX,EIO},{AE_EXEC_CALL,0},{AE_EXEC_CALL,4096},{AE_EXEC_CALL,UINT32_MAX}};
    for (size_t n=0;n<sizeof invalid/sizeof invalid[0];++n) { record(bytes,invalid[n][0],invalid[n][1]); stream(bytes,16,AE_EXEC_CHANNEL_MALFORMED); }
    record(bytes,AE_EXEC_CALL,ENOEXEC); bytes[0]='X'; stream(bytes,16,AE_EXEC_CHANNEL_MALFORMED);
    record(bytes,AE_EXEC_CALL,ENOEXEC); memcpy(bytes+16,bytes,16);
    stream(bytes,17,AE_EXEC_CHANNEL_MALFORMED); stream(bytes,32,AE_EXEC_CHANNEL_MALFORMED);
    struct ae_exec_channel *c=NULL; int writer=-1;
    CHECK(ae_exec_channel_new(&c,&writer)==0);
    for (size_t n=0;n<16;++n) { CHECK(write(writer,bytes+n,1)==1); observe(c,AE_EXEC_CHANNEL_PENDING); }
    CHECK(close(writer)==0); observe(c,AE_EXEC_CHANNEL_FAILURE); CHECK(ae_exec_channel_close(&c)==0);
    CHECK(ae_exec_channel_new(&c,&writer)==0 && write(writer,bytes,16)==16); observe(c,AE_EXEC_CHANNEL_PENDING);
    CHECK(write(writer,bytes,1)==1); observe(c,AE_EXEC_CHANNEL_MALFORMED);
    CHECK(write(writer,bytes,16)==16); observe(c,AE_EXEC_CHANNEL_MALFORMED); CHECK(reads==0);
    CHECK(close(writer)==0 && ae_exec_channel_close(&c)==0);
    /* A duplicated writer delays EOF; a dead/closed local source is not proof
     * that transport copies are gone. Never wait synchronously for that EOF. */
    CHECK(ae_exec_channel_new(&c,&writer)==0); int copy=fcntl(writer,F_DUPFD_CLOEXEC,3); CHECK(copy>=3);
    CHECK(close(writer)==0); observe(c,AE_EXEC_CHANNEL_PENDING);
    CHECK(close(copy)==0); observe(c,AE_EXEC_CHANNEL_UNKNOWN); CHECK(ae_exec_channel_close(&c)==0);
    CHECK(ae_exec_channel_new(&c,&writer)==0); int reader=ae_exec_channel_fd(c);
    CHECK(ae_exec_writer_check(reader,geteuid(),getegid())==-EACCES);
    CHECK(ae_exec_writer_check(writer,geteuid()+1,getegid())==-EACCES);
    CHECK(ae_exec_writer_check(writer,geteuid(),getegid()+1)==-EACCES);
    CHECK(ae_exec_writer_check(-1,geteuid(),getegid())==-EINVAL);
    CHECK(fcntl(writer,F_SETFD,0)==0 && ae_exec_writer_check(writer,geteuid(),getegid())==-EACCES && fcntl(writer,F_SETFD,FD_CLOEXEC)==0);
    CHECK(fcntl(writer,F_SETFL,0)==0 && ae_exec_writer_check(writer,geteuid(),getegid())==-EACCES);
    observe(c,AE_EXEC_CHANNEL_PENDING); /* Issuer reader has its own OFD/flags. */
    CHECK(fcntl(writer,F_SETFL,O_NONBLOCK)==0);
    struct ae_exec_report r,marker; memset(&marker,0xa5,sizeof marker); r=marker;
    CHECK(fcntl(reader,F_SETFL,0)==0 && ae_exec_channel_read(c,&r)==-EACCES && memcmp(&r,&marker,sizeof r)==0);
    CHECK(fcntl(reader,F_SETFL,O_NONBLOCK)==0);
    copy=fcntl(reader,F_DUPFD_CLOEXEC,3); CHECK(copy>=3); int other[2]; CHECK(pipe2(other,O_NONBLOCK|O_CLOEXEC)==0);
    CHECK(dup3(other[0],reader,O_CLOEXEC)==reader && ae_exec_channel_read(c,&r)==-ESTALE && memcmp(&r,&marker,sizeof r)==0);
    CHECK(dup3(copy,reader,O_CLOEXEC)==reader && close(copy)==0 && close(other[0])==0 && close(other[1])==0);
    observe(c,AE_EXEC_CHANNEL_PENDING);
    CHECK(write(writer,bytes,16)==16 && close(writer)==0);
    read_fault=2; read_error=EIO; r=marker;
    CHECK(ae_exec_channel_read(c,&r)==-EIO && read_fault==0 && memcmp(&r,&marker,sizeof r)==0);
    observe(c,AE_EXEC_CHANNEL_FAILURE); CHECK(ae_exec_channel_close(&c)==0);
    CHECK(ae_exec_channel_new(&c,&writer)==0);
    read_fault=1; read_error=EINTR; observe(c,AE_EXEC_CHANNEL_PENDING); CHECK(reads==1 && read_fault==0);
    CHECK(close(writer)==0); observe(c,AE_EXEC_CHANNEL_UNKNOWN); CHECK(ae_exec_channel_close(&c)==0);
    /* Reject a named FIFO even with correct creator/mode/direction/flags. */
    char directory[4096],path[4096]; int n=snprintf(directory,sizeof directory,"%s/channel-XXXXXX",argv[1]); CHECK(n>0 && n<(int)sizeof directory && mkdtemp(directory));
    n=snprintf(path,sizeof path,"%s/fifo",directory); CHECK(n>0 && n<(int)sizeof path && mkfifo(path,0600)==0 && chmod(path,0600)==0);
    int fifo_reader=open(path,O_RDONLY|O_NONBLOCK|O_CLOEXEC),fifo_writer=open(path,O_WRONLY|O_NONBLOCK|O_CLOEXEC);
    CHECK(fifo_reader>=3 && fifo_writer>=3 && ae_exec_writer_check(fifo_writer,geteuid(),getegid())==-EACCES);
    CHECK(close(fifo_reader)==0 && close(fifo_writer)==0 && unlink(path)==0 && rmdir(directory)==0);
    /* Constructor faults never publish a partial object or leak an endpoint. */
    for (int fault=0;fault<10;++fault) {
        writer=-777;
        if (fault==0) allocation_fault=1;
        else if (fault==1) pipe_fault=1;
        else if (fault<4) stat_fault=fault-1;
        else if (fault<6) fs_fault=fault-3;
        else fcntl_fault=fault-5;
        CHECK(ae_exec_channel_new(&c,&writer)==(fault==0 ? -ENOMEM : fault==1 ? -EMFILE : -EIO) && c==NULL && writer==-777);
        CHECK(!allocation_fault && !pipe_fault && !stat_fault && !fs_fault && !fcntl_fault && descriptors()==baseline);
    }
    CHECK(ae_exec_channel_new(&c,&writer)==0); reader=ae_exec_channel_fd(c);
    pid_t child=fork(); CHECK(child>=0);
    if (child==0) {
        CHECK(ae_exec_channel_fd(c)==-ECHILD && ae_exec_channel_read(c,&r)==-ECHILD && ae_exec_channel_close(&c)==-ECHILD);
        CHECK(close(reader)==0 && close(writer)==0); _exit(0);
    }
    int status; CHECK(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==0);
    int another=-777; CHECK(ae_exec_channel_new(&c,&another)==-EINVAL && another==-777);
    CHECK(ae_exec_channel_read(c,NULL)==-EINVAL && ae_exec_channel_read(NULL,&r)==-EINVAL && ae_exec_channel_fd(NULL)==-EINVAL);
    CHECK(ae_exec_channel_new(NULL,&another)==-EINVAL && ae_exec_channel_close(NULL)==-EINVAL);
    CHECK(close(writer)==0); close_fault=1;
    CHECK(ae_exec_channel_close(&c)==-EIO && c==NULL && close_fault==0 && descriptors()==baseline);
    CHECK(ae_exec_channel_close(&c)==0);
    child=fork(); CHECK(child>=0);
    if (child==0) {
        CHECK(close(STDIN_FILENO)==0); writer=-777;
        CHECK(ae_exec_channel_new(&c,&writer)==-EINVAL && c==NULL && writer==-777);
        CHECK(fcntl(STDIN_FILENO,F_GETFD)==-1 && errno==EBADF); _exit(0);
    }
    CHECK(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==0 && descriptors()==baseline);
    puts("Execution error channel: bounded incremental frames, unknown EOF, no replay/reset, real FD identity, creator/flags, rollback and process-owned cleanup passed."); return 0;
}
