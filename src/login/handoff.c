#define _GNU_SOURCE
#include "login/handoff.h"
#include "login/loaded_internal.h"
#include "core/root.h"
#include <errno.h>
#include <fcntl.h>
#include <linux/openat2.h>
#include <selinux/selinux.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/xattr.h>
#include <unistd.h>

struct ae_login_handoff {
    struct ae_login_loaded *loaded;
    struct ae_root_identity parent;
    struct statx file_identity;
    int directory,file,error;
    pid_t owner,thread;
    bool busy;
    char path[AE_LOGIN_SOURCE_PATH_MAX],leaf[256],label[AE_LOGIN_SOURCE_LABEL_MAX];
    uint8_t record[AE_LOGIN_OBSERVATION_BYTES];
};

static int owner(const struct ae_login_handoff *handoff)
{
    if(!handoff)return -EINVAL;
    if(getpid()!=handoff->owner)return -ECHILD;
    if(gettid()!=handoff->thread)return -EXDEV;
    return handoff->busy?-EBUSY:0;
}

static int parent_check(const struct ae_login_handoff *handoff)
{
    if(security_getenforce()!=1)return -EACCES;
    return ae_root_check_path(handoff->directory,handoff->path,&handoff->parent);
}

static int setup(struct ae_login_handoff *handoff,struct ae_login_loaded *loaded,int directory,
                  const char *path,const char *leaf,const char *label)
{
    if(!loaded || directory<0 || !path || !leaf || !label)return -EINVAL;
    size_t p=strnlen(path,sizeof handoff->path),n=strnlen(leaf,sizeof handoff->leaf);
    size_t l=strnlen(label,sizeof handoff->label);
    if(!p || p==sizeof handoff->path || path[0]!='/' || (p>1 && path[p-1]=='/') ||
       !n || n==sizeof handoff->leaf || strchr(leaf,'/') || !strcmp(leaf,".") || !strcmp(leaf,"..") ||
       !l || l==sizeof handoff->label)return -EINVAL;
    int flags=fcntl(directory,F_GETFL);
    if(flags<0)return -errno;
    if((flags&O_ACCMODE)!=O_RDONLY || (flags&O_PATH))return -EACCES;
    handoff->loaded=loaded;handoff->owner=getpid();handoff->thread=gettid();
    memcpy(handoff->path,path,p+1);memcpy(handoff->leaf,leaf,n+1);memcpy(handoff->label,label,l+1);
    handoff->directory=fcntl(directory,F_DUPFD_CLOEXEC,3);
    if(handoff->directory<0)return -errno;
    int r=ae_root_observe(handoff->directory,&handoff->parent);
    const struct ae_root_identity *parent=&handoff->parent;
    if(!r && (parent->uid || parent->gid || (parent->mode!=(S_IFDIR|0755) && parent->mode!=(S_IFDIR|0555))))r=-EACCES;
    if(!r)r=parent_check(handoff);
    return r;
}

static int named_file(const struct ae_login_handoff *handoff)
{
    struct open_how how={.flags=O_RDONLY|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK,
        .resolve=RESOLVE_BENEATH|RESOLVE_NO_SYMLINKS|RESOLVE_NO_MAGICLINKS|RESOLVE_NO_XDEV};
    int fd;
    do {fd=(int)syscall(SYS_openat2,handoff->directory,handoff->leaf,&how,sizeof how);}while(fd<0 && errno==EINTR);
    return fd<0?-errno:fd;
}

static int file_stat(const struct ae_login_handoff *handoff,int fd,struct statx *out)
{
    unsigned int fields=STATX_TYPE|STATX_MODE|STATX_UID|STATX_GID|STATX_INO|STATX_NLINK|
        STATX_SIZE|STATX_BTIME|STATX_MTIME|STATX_CTIME|STATX_MNT_ID_UNIQUE;
    if(statx(fd,"",AT_EMPTY_PATH|AT_SYMLINK_NOFOLLOW,fields,out))return -errno;
    if((out->stx_mask&fields)!=fields)return -ENOTSUP;
    if(out->stx_mode!=(S_IFREG|0444) || out->stx_uid || out->stx_gid || out->stx_nlink!=1 ||
       out->stx_size!=AE_LOGIN_OBSERVATION_BYTES || out->stx_mnt_id!=handoff->parent.mount_id)return -EACCES;
    if(fgetxattr(fd,"security.capability",NULL,0)>=0)return -EACCES;
    if(errno!=ENODATA)return -errno;
    char *label=NULL;
    if(fgetfilecon_raw(fd,&label)<0)return -errno;
    int r=label && !strcmp(label,handoff->label)?0:-EACCES;
    freecon(label);
    return r;
}

static bool same_file(const struct statx *a,const struct statx *b)
{
    return a->stx_mnt_id==b->stx_mnt_id && a->stx_ino==b->stx_ino &&
        a->stx_dev_major==b->stx_dev_major && a->stx_dev_minor==b->stx_dev_minor &&
        a->stx_uid==b->stx_uid && a->stx_gid==b->stx_gid && a->stx_mode==b->stx_mode &&
        a->stx_nlink==b->stx_nlink && a->stx_size==b->stx_size &&
        a->stx_btime.tv_sec==b->stx_btime.tv_sec && a->stx_btime.tv_nsec==b->stx_btime.tv_nsec &&
        a->stx_mtime.tv_sec==b->stx_mtime.tv_sec && a->stx_mtime.tv_nsec==b->stx_mtime.tv_nsec &&
        a->stx_ctime.tv_sec==b->stx_ctime.tv_sec && a->stx_ctime.tv_nsec==b->stx_ctime.tv_nsec;
}

static int read_record(int fd,uint8_t out[AE_LOGIN_OBSERVATION_BYTES])
{
    for(size_t used=0;used<AE_LOGIN_OBSERVATION_BYTES;) {
        ssize_t n=pread(fd,out+used,AE_LOGIN_OBSERVATION_BYTES-used,(off_t)used);
        if(n<0){if(errno!=EINTR)return -errno;}
        else if(!n)return -EBADMSG;
        else used+=(size_t)n;
    }
    return 0;
}

int ae_login_handoff_check(void *value)
{
    struct ae_login_handoff *handoff=value;
    int r=owner(handoff);
    if(r)return r;
    if(handoff->error)return handoff->error;
    handoff->busy=true;
    r=parent_check(handoff);
    struct statx before,after,named;
    uint8_t record[AE_LOGIN_OBSERVATION_BYTES];
    int fd=-1;
    if(!r)r=file_stat(handoff,handoff->file,&before);
    if(!r && !same_file(&before,&handoff->file_identity))r=-ESTALE;
    if(!r){fd=named_file(handoff);if(fd<0)r=fd;}
    if(!r)r=file_stat(handoff,fd,&named);
    if(!r && !same_file(&before,&named))r=-ESTALE;
    if(!r)r=read_record(handoff->file,record);
    if(!r && memcmp(record,handoff->record,sizeof record))r=-ESTALE;
    if(!r)r=ae_login_loaded_compare(handoff->loaded,record);
    if(!r)r=file_stat(handoff,handoff->file,&after);
    if(!r && !same_file(&before,&after))r=-ESTALE;
    if(!r)r=parent_check(handoff);
    if(fd>=0)close(fd);
    handoff->busy=false;
    if(r)handoff->error=r;
    return r;
}

int ae_login_handoff_close(struct ae_login_handoff **value)
{
    if(!value)return -EINVAL;
    struct ae_login_handoff *handoff=*value;
    if(!handoff)return 0;
    int r=owner(handoff);
    if(r && r!=-ECHILD)return r;
    if(handoff->file>=0)close(handoff->file);
    if(handoff->directory>=0)close(handoff->directory);
    free(handoff);*value=NULL;
    return 0;
}

int ae_login_handoff_open(struct ae_login_loaded *loaded,int directory,const char *path,
                          const char *leaf,const char *label,struct ae_login_handoff **out)
{
    if(!out || *out)return -EINVAL;
    struct ae_login_handoff *handoff=calloc(1,sizeof *handoff);
    if(!handoff)return -ENOMEM;
    handoff->directory=handoff->file=-1;
    int r=setup(handoff,loaded,directory,path,leaf,label);
    if(!r){handoff->file=named_file(handoff);if(handoff->file<0)r=handoff->file;}
    if(!r)r=file_stat(handoff,handoff->file,&handoff->file_identity);
    if(!r)r=read_record(handoff->file,handoff->record);
    if(!r)r=ae_login_handoff_check(handoff);
    if(r){ae_login_handoff_close(&handoff);return r;}
    *out=handoff;
    return 0;
}

int ae_login_handoff_publish(struct ae_login_loaded *loaded,int directory,const char *path,
                             const char *leaf,const char *label)
{
    if(getuid() || geteuid() || getgid() || getegid())return -EACCES;
    struct ae_login_handoff handoff={.directory=-1,.file=-1};
    int r=setup(&handoff,loaded,directory,path,leaf,label);
    if(!r)r=ae_login_loaded_capture(loaded,handoff.record);
    int fd=-1;
    if(!r){fd=openat(handoff.directory,".",O_TMPFILE|O_WRONLY|O_CLOEXEC,0600);if(fd<0)r=-errno;}
    for(size_t used=0;!r && used<sizeof handoff.record;) {
        ssize_t n=write(fd,handoff.record+used,sizeof handoff.record-used);
        if(n<0){if(errno!=EINTR)r=-errno;}
        else if(!n)r=-EIO;
        else used+=(size_t)n;
    }
    if(!r && fsetfilecon_raw(fd,label)<0)r=-errno;
    if(!r && fchmod(fd,0444))r=-errno;
    if(!r && fsync(fd))r=-errno;
    uint8_t current[AE_LOGIN_OBSERVATION_BYTES];
    if(!r)r=ae_login_loaded_capture(loaded,current);
    if(!r && memcmp(current,handoff.record,sizeof current))r=-ESTALE;
    if(!r)r=parent_check(&handoff);
    if(!r && linkat(fd,"",handoff.directory,leaf,AT_EMPTY_PATH))r=-errno;
    if(!r && fsync(handoff.directory))r=-errno;
    if(fd>=0 && close(fd) && !r)r=-errno;
    if(handoff.directory>=0 && close(handoff.directory) && !r)r=-errno;
    return r;
}
