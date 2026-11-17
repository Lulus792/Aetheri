#define _GNU_SOURCE
#include "login/startup.h"
#include "core/root.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/magic.h>
#include <linux/nsfs.h>
#include <sched.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/pidfd.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <unistd.h>

struct ae_login_startup {
    struct ae_login_service *service;
    struct ae_login_sources *sources;
    struct ae_login_loaded *loaded;
    struct ae_root_identity parent;
    struct stat namespace;
    int space,directory,error;
    pid_t owner,thread;
    bool busy,attempted;
    int (*check)(void *);
    void *userdata;
    char path[AE_LOGIN_SOURCE_PATH_MAX],leaf[256],label[AE_LOGIN_SOURCE_LABEL_MAX];
};

static int owner(const struct ae_login_startup *s)
{
    return !s?-EINVAL:getpid()!=s->owner?-ECHILD:gettid()!=s->thread?-EXDEV:s->busy?-EBUSY:0;
}
static bool same(const struct stat *a,const struct stat *b)
{
    return a->st_dev==b->st_dev && a->st_ino==b->st_ino;
}
static int syscall_error(void)
{
    int error=errno;
    return error>0?-error:-EIO;
}
static int external(struct ae_login_startup *s)
{
    int r=s->check(s->userdata);
    return r>0?-EINVAL:r;
}
static int single_thread(void)
{
    if(getpid()!=gettid())return -EXDEV;
    uid_t r,e,v;gid_t gr,ge,gv;
    if(getresuid(&r,&e,&v) || getresgid(&gr,&ge,&gv))return syscall_error();
    if(r || e || v || gr || ge || gv)return -EACCES;
    DIR *tasks=opendir("/proc/self/task");if(!tasks)return syscall_error();
    struct statfs fs;int result=fstatfs(dirfd(tasks),&fs)?syscall_error():fs.f_type==PROC_SUPER_MAGIC?0:-EPROTO;
    unsigned count=0;struct dirent *entry;
    errno=0;
    while(!result && (entry=readdir(tasks))) {
        if(!strcmp(entry->d_name,".") || !strcmp(entry->d_name,".."))continue;
        if(++count>1)result=-EBUSY;
        errno=0;
    }
    if(!result && errno)result=-errno;
    if(!result && count!=1)result=-ESRCH;
    if(closedir(tasks) && !result)result=syscall_error();
    return result;
}
static int space_stat(int fd,struct stat *out)
{
    struct statfs fs;
    if(fstatfs(fd,&fs) || fstat(fd,out))return syscall_error();
    if(fs.f_type!=NSFS_MAGIC || !out->st_ino)return -EPROTO;
    int kind=ioctl(fd,NS_GET_NSTYPE);
    return kind<0?syscall_error():kind==CLONE_NEWNS?0:-EPROTO;
}
static int peer_space(struct ae_login_startup *s,int *out)
{
    int peer=-1;int r=ae_login_service_peer(s->service,&peer);
    if(r)return r;
    int fd=ioctl(peer,PIDFD_GET_MNT_NAMESPACE,0);
    if(fd<0)return syscall_error();
    struct stat st;r=space_stat(fd,&st);
    if(!r)r=ae_login_service_check(s->service);
    if(r){close(fd);return r;}
    *out=fd;return 0;
}
static int namespace_check(struct ae_login_startup *s)
{
    int fd=-1;int r=peer_space(s,&fd);
    struct stat current;
    if(!r)r=space_stat(fd,&current);
    if(!r && !same(&current,&s->namespace))r=-ESTALE;
    if(fd>=0)close(fd);
    if(!r) {
        fd=open("/proc/self/ns/mnt",O_RDONLY|O_CLOEXEC);
        if(fd<0)r=syscall_error();
        else {
            r=space_stat(fd,&current);
            if(!r && !same(&current,&s->namespace))r=-ESTALE;
            close(fd);
        }
    }
    return r;
}
static int copy(char *out,size_t capacity,const char *text)
{
    if(!text)return -EINVAL;
    size_t n=strnlen(text,capacity);
    if(!n || n==capacity)return -EINVAL;
    memcpy(out,text,n+1);return 0;
}
static int destination(struct ae_login_startup *s)
{
    s->directory=open(s->path,O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW);
    if(s->directory<0)return syscall_error();
    int r=ae_root_observe(s->directory,&s->parent);
    if(!r && (s->parent.uid || s->parent.gid ||
        (s->parent.mode!=(S_IFDIR|0755) && s->parent.mode!=(S_IFDIR|0555))))r=-EACCES;
    if(!r)r=ae_root_check_path(s->directory,s->path,&s->parent);
    return r;
}
int ae_login_startup_close(struct ae_login_startup **out)
{
    if(!out)return -EINVAL;
    struct ae_login_startup *s=*out;if(!s)return 0;
    int r=owner(s);if(r && r!=-ECHILD)return r;
    ae_login_loaded_close(&s->loaded);ae_login_sources_close(&s->sources);ae_login_service_close(&s->service);
    if(s->space>=0)close(s->space);
    if(s->directory>=0)close(s->directory);
    free(s);*out=NULL;return 0;
}
int ae_login_startup_create(int fd,sd_bus *bus,const struct ae_login_startup_expect *e,
                            struct ae_login_startup **out)
{
    if(!out || *out || !e || !e->check || !e->sources || !bus || fd<0 ||
       e->service.uid || e->service.gid || !e->source_count || e->source_count>AE_LOGIN_SOURCES_MAX ||
       e->fragment>=e->source_count || e->program>=e->source_count || e->config>=e->source_count ||
       e->dropin_count>AE_LOGIN_SOURCES_MAX-3 || (e->dropin_count && !e->dropins))return -EINVAL;
    int r=single_thread();if(r)return r;
    struct ae_login_startup *s=calloc(1,sizeof *s);if(!s)return -ENOMEM;
    s->owner=getpid();s->thread=gettid();s->space=-1;s->directory=-1;s->check=e->check;s->userdata=e->userdata;
    r=copy(s->path,sizeof s->path,e->directory);
    if(!r)r=copy(s->leaf,sizeof s->leaf,e->leaf);
    if(!r)r=copy(s->label,sizeof s->label,e->label);
    if(!r && (s->path[0]!='/' || strchr(s->leaf,'/') || !strcmp(s->leaf,".") || !strcmp(s->leaf,"..")))r=-EINVAL;
    if(!r)r=external(s);
    if(!r)r=ae_login_service_create(fd,bus,&e->service,&s->service);
    if(!r)r=peer_space(s,&s->space);
    if(!r)r=space_stat(s->space,&s->namespace);
    if(!r)r=external(s);
    if(!r && setns(s->space,CLONE_NEWNS))r=syscall_error();
    if(!r)r=namespace_check(s);
    if(!r && e->enter_view) {
        r=e->enter_view(e->userdata);
        if(r>0)r=-EINVAL;
        if(!r)r=namespace_check(s);
    }
    if(!r)r=external(s);
    if(!r)r=ae_login_sources_create(e->sources,e->source_count,&s->sources);
    if(!r)r=ae_login_loaded_create(s->service,s->sources,e->fragment,e->program,e->config,
                                  e->dropins,e->dropin_count,&s->loaded);
    if(!r)r=destination(s);
    if(!r)r=namespace_check(s);
    if(!r)r=external(s);
    if(r){ae_login_startup_close(&s);return r;}
    *out=s;return 0;
}
static int check(struct ae_login_startup *s)
{
    bool pending=false;
    int r=external(s);
    if(!r)r=namespace_check(s);
    if(!r)r=ae_root_check_path(s->directory,s->path,&s->parent);
    if(r)return r==-EAGAIN?-ESTALE:r;
    r=ae_login_loaded_check_config(s->loaded);
    if(r==-EAGAIN){pending=true;r=0;}
    struct ae_image image;
    if(!r && !pending)r=ae_login_loaded_match_image(s->loaded,&image);
    if(!r)r=namespace_check(s);
    if(!r)r=external(s);
    return r==-EAGAIN?-ESTALE:r?r:pending?-EAGAIN:0;
}
int ae_login_startup_check(void *value)
{
    struct ae_login_startup *s=value;int r=owner(s);if(r)return r;
    if(s->error)return s->error;
    s->busy=true;r=check(s);s->busy=false;
    if(r && r!=-EAGAIN)s->error=r;
    return r;
}
int ae_login_startup_publish(struct ae_login_startup *s)
{
    int r=owner(s);if(r)return r;
    if(s->error)return s->error;
    if(s->attempted)return -EALREADY;
    s->attempted=true;s->busy=true;
    r=check(s);
    if(!r) {
        r=ae_login_handoff_publish(s->loaded,s->directory,s->path,s->leaf,s->label);
        if(!r)r=namespace_check(s);
        if(!r)r=external(s);
        if(r==-EAGAIN)r=-ESTALE;
    }
    s->busy=false;
    if(r)s->error=r;
    return r;
}
