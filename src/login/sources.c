#define _GNU_SOURCE
#include "login/sources.h"
#include "core/digest.h"
#include "core/root.h"
#include "linux/image.h"
#include <errno.h>
#include <fcntl.h>
#include <linux/openat2.h>
#include <selinux/selinux.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/xattr.h>
#include <unistd.h>

enum { DEPTH=16, LEAF=256 };
struct source {
    struct ae_login_source_expect expect;
    char path[AE_LOGIN_SOURCE_PATH_MAX],leaf[LEAF],label[AE_LOGIN_SOURCE_LABEL_MAX];
    struct ae_root_identity ancestors[DEPTH+1];
    size_t depth;
    int parent,file;
    struct statx object;
    struct ae_image image;
};
struct ae_login_sources {
    pid_t owner,thread;
    int error;
    bool busy;
    size_t count;
    struct source sources[AE_LOGIN_SOURCES_MAX];
};
static int owner(const struct ae_login_sources *s)
{
    if(!s)return -EINVAL;
    if(s->owner!=getpid())return -ECHILD;
    if(s->thread!=gettid())return -EXDEV;
    return s->busy?-EBUSY:0;
}
static int literal(const char *s,size_t limit)
{
    if(!s)return -EINVAL;
    size_t n=strnlen(s,limit);
    return !n || n==limit || strchr(s,'/') || !strcmp(s,".") || !strcmp(s,"..")?-EINVAL:0;
}
static int expectation(struct source *s,const struct ae_login_source_expect *e)
{
    if(!e || !e->directory || !e->label || literal(e->leaf,LEAF))return -EINVAL;
    size_t n=strnlen(e->directory,AE_LOGIN_SOURCE_PATH_MAX);
    size_t l=strnlen(e->label,AE_LOGIN_SOURCE_LABEL_MAX);
    if(!n || n==AE_LOGIN_SOURCE_PATH_MAX || e->directory[0]!='/' ||
       (n>1 && e->directory[n-1]=='/') || !l || l==AE_LOGIN_SOURCE_LABEL_MAX || !e->size)return -EINVAL;
    if(e->kind==AE_LOGIN_SOURCE_ELF) {
        if(e->mode!=0555 || e->size>AE_DIGEST_MAX_BYTES)return -EINVAL;
    } else if(e->kind==AE_LOGIN_SOURCE_DATA) {
        if((e->mode!=0444 && e->mode!=0644) || e->size>AE_LOGIN_SOURCE_DATA_MAX)return -EINVAL;
    } else if(e->kind==AE_LOGIN_SOURCE_ASSET) {
        if(e->mode!=0444 || e->size>AE_DIGEST_MAX_BYTES)return -EINVAL;
    } else return -EINVAL;
    size_t depth=0;
    for(size_t start=1;start<n;) {
        size_t end=start;while(end<n && e->directory[end]!='/')++end;
        size_t length=end-start;
        if(!length || length>=LEAF || (length==1 && e->directory[start]=='.') ||
           (length==2 && !memcmp(e->directory+start,"..",2)) || ++depth>DEPTH)return -EINVAL;
        start=end+1;
    }
    s->expect=*e;s->depth=depth;
    memcpy(s->path,e->directory,n+1);strcpy(s->leaf,e->leaf);memcpy(s->label,e->label,l+1);
    s->expect.directory=s->path;s->expect.leaf=s->leaf;s->expect.label=s->label;
    return 0;
}
static int open_component(int parent,const char *name,bool directory,bool no_mount)
{
    /* Directory handles are only used for identity and lookup. Do not require
       permission to enumerate protected ancestors such as /etc/pam.d. */
    struct open_how h={.flags=(uint64_t)(O_CLOEXEC|O_NOFOLLOW|
        (directory?(O_PATH|O_DIRECTORY):(O_RDONLY|O_NONBLOCK))),
        .resolve=RESOLVE_NO_SYMLINKS|RESOLVE_NO_MAGICLINKS};
    if(parent!=AT_FDCWD)h.resolve|=RESOLVE_BENEATH;
    if(no_mount)h.resolve|=RESOLVE_NO_XDEV;
    int fd;do {fd=(int)syscall(SYS_openat2,parent,name,&h,sizeof h);}while(fd<0 && errno==EINTR);
    return fd<0?-errno:fd;
}
static int ancestor(int fd,struct ae_root_identity *identity)
{
    int r=ae_root_observe(fd,identity);if(r)return r;
    return identity->uid || identity->gid || (identity->mode&07022U) || !(identity->mode&0100U)?-EACCES:0;
}
/* Rewalk the independently selected absolute path, checking every ancestor,
   rather than trusting a safe final directory behind an unsafe parent. */
static int parent_walk(struct source *s,bool capture)
{
    int fd=open_component(AT_FDCWD,"/",true,false);if(fd<0)return fd;
    int r=0;size_t index=0,start=1;
    for(;;) {
        struct ae_root_identity value;r=ancestor(fd,&value);
        if(!r && capture)s->ancestors[index]=value;
        else if(!r && !ae_root_equal(&value,&s->ancestors[index]))r=-ESTALE;
        if(r || index==s->depth)break;
        size_t end=start;while(s->path[end] && s->path[end]!='/')++end;
        char component[LEAF];memcpy(component,s->path+start,end-start);component[end-start]=0;
        int next=open_component(fd,component,true,false);
        if(next<0){r=next;break;}
        close(fd);fd=next;start=end+1;++index;
    }
    if(r){close(fd);return r;}return fd;
}
static int file_stat(int fd,const struct source *s,struct statx *out)
{
    unsigned int fields=STATX_TYPE|STATX_MODE|STATX_UID|STATX_GID|STATX_INO|STATX_NLINK|
        STATX_SIZE|STATX_BTIME|STATX_MTIME|STATX_CTIME|STATX_MNT_ID_UNIQUE;
    int r;do {r=statx(fd,"",AT_EMPTY_PATH|AT_SYMLINK_NOFOLLOW,fields,out);}while(r<0 && errno==EINTR);
    if(r<0)return -errno;
    if((out->stx_mask&fields)!=fields || !out->stx_mnt_id || !out->stx_ino)return -ENOTSUP;
    if(out->stx_mode!=(S_IFREG|s->expect.mode) || out->stx_uid || out->stx_gid ||
       out->stx_nlink!=1 || out->stx_size!=s->expect.size)return -EACCES;
    if(out->stx_btime.tv_sec<0 || out->stx_btime.tv_nsec>=1000000000U ||
       out->stx_mtime.tv_nsec>=1000000000U || out->stx_ctime.tv_nsec>=1000000000U)return -EBADMSG;
    ssize_t cap=fgetxattr(fd,"security.capability",NULL,0);
    if(cap>=0)return -EACCES;
    if(errno!=ENODATA)return -errno;
    char *label=NULL;if(fgetfilecon_raw(fd,&label)<0)return -errno;
    r=!strcmp(label,s->label)?0:-EACCES;freecon(label);return r;
}
static bool same_stat(const struct statx *a,const struct statx *b)
{
    return a->stx_mnt_id==b->stx_mnt_id && a->stx_ino==b->stx_ino &&
        a->stx_dev_major==b->stx_dev_major && a->stx_dev_minor==b->stx_dev_minor &&
        a->stx_uid==b->stx_uid && a->stx_gid==b->stx_gid && a->stx_mode==b->stx_mode &&
        a->stx_nlink==b->stx_nlink && a->stx_size==b->stx_size &&
        a->stx_btime.tv_sec==b->stx_btime.tv_sec && a->stx_btime.tv_nsec==b->stx_btime.tv_nsec &&
        a->stx_mtime.tv_sec==b->stx_mtime.tv_sec && a->stx_mtime.tv_nsec==b->stx_mtime.tv_nsec &&
        a->stx_ctime.tv_sec==b->stx_ctime.tv_sec && a->stx_ctime.tv_nsec==b->stx_ctime.tv_nsec;
}
static int source_check(struct source *s)
{
    int parent=parent_walk(s,false);if(parent<0)return parent;
    struct ae_root_identity held;int r=ancestor(s->parent,&held);
    if(!r && !ae_root_equal(&held,&s->ancestors[s->depth]))r=-ESTALE;
    struct statx first,last,named;
    if(!r)r=file_stat(s->file,s,&first);
    if(!r && !same_stat(&s->object,&first))r=-ESTALE;
    int fd=-1;
    if(!r){fd=open_component(parent,s->leaf,false,true);if(fd<0)r=fd;}
    if(!r)r=file_stat(fd,s,&named);
    if(!r && !same_stat(&s->object,&named))r=-ESTALE;
    if(!r)r=file_stat(s->file,s,&last);
    if(!r && !same_stat(&first,&last))r=-ESTALE;
    if(fd>=0)close(fd);
    close(parent);return r;
}
int ae_login_sources_create(const struct ae_login_source_expect *e,size_t count,struct ae_login_sources **out)
{
    if(!e || !count || count>AE_LOGIN_SOURCES_MAX || !out || *out)return -EINVAL;
    if(security_getenforce()!=1)return -EACCES;
    struct ae_login_sources *s=calloc(1,sizeof *s);if(!s)return -ENOMEM;
    s->owner=getpid();s->thread=gettid();s->count=count;
    for(size_t n=0;n<count;++n)s->sources[n].file=s->sources[n].parent=-1;
    int r=0;
    for(size_t n=0;n<count && !r;++n)r=expectation(&s->sources[n],&e[n]);
    for(size_t n=0;n<count && !r;++n) {
        struct source *v=&s->sources[n];
        v->parent=parent_walk(v,true);if(v->parent<0){r=v->parent;break;}
        v->file=open_component(v->parent,v->leaf,false,true);if(v->file<0){r=v->file;break;}
        r=file_stat(v->file,v,&v->object);
        if(!r && v->object.stx_mnt_id!=v->ancestors[v->depth].mount_id)r=-EXDEV;
        if(!r && v->expect.kind==AE_LOGIN_SOURCE_ELF)r=ae_image_verify(v->file,v->expect.digest,&v->image);
        else if(!r) {
            uint8_t digest[32];r=ae_sha256_fd(v->file,v->expect.size,digest);
            if(!r && memcmp(digest,v->expect.digest,32))r=-EBADMSG;
        }
        for(size_t k=0;k<n && !r;++k) {
            const struct statx *p=&s->sources[k].object;
            if(p->stx_ino==v->object.stx_ino && p->stx_dev_major==v->object.stx_dev_major &&
               p->stx_dev_minor==v->object.stx_dev_minor)r=-EINVAL;
        }
        if(!r)r=source_check(v);
    }
    if(!r)r=ae_login_sources_check(s);
    if(r){ae_login_sources_close(&s);return r;}
    *out=s;return 0;
}
int ae_login_sources_check(void *value)
{
    struct ae_login_sources *s=value;int r=owner(s);if(r)return r;
    if(s->error)return s->error;
    s->busy=true;
    if(security_getenforce()!=1)r=-EACCES;
    for(size_t n=0;n<s->count && !r;++n)r=source_check(&s->sources[n]);
    s->busy=false;if(r)s->error=r;return r;
}
int ae_login_sources_image(struct ae_login_sources *s,size_t index,struct ae_image *out)
{
    int r=owner(s);if(r)return r;
    if(!out || index>=s->count || s->sources[index].expect.kind!=AE_LOGIN_SOURCE_ELF)return -EINVAL;
    r=ae_login_sources_check(s);if(!r)*out=s->sources[index].image;return r;
}
int ae_login_sources_path(struct ae_login_sources *s,size_t index,
                          enum ae_login_source_kind kind,char out[AE_LOGIN_SOURCE_FULL_PATH_MAX])
{
    int r=owner(s);
    if(r)return r;
    if(!out || index>=s->count || s->sources[index].expect.kind!=kind)return -EINVAL;
    r=ae_login_sources_check(s);
    if(r)return r;
    const struct source *source=&s->sources[index];
    size_t size=strlen(source->path);
    memcpy(out,source->path,size);
    if(size>1)out[size++]='/';
    strcpy(out+size,source->leaf);
    return 0;
}
int ae_login_sources_close(struct ae_login_sources **value)
{
    if(!value)return -EINVAL;
    struct ae_login_sources *s=*value;if(!s)return 0;
    int r=owner(s);if(r && r!=-ECHILD)return r;
    for(size_t n=0;n<s->count;++n) {
        if(s->sources[n].file>=0)close(s->sources[n].file);
        if(s->sources[n].parent>=0)close(s->sources[n].parent);
    }
    free(s);*value=NULL;return 0;
}

int ae_login_sources_data_digest(struct ae_login_sources *s,size_t index,uint8_t out[32])
{
    int r=owner(s);
    if(r)return r;
    if(!out || index>=s->count || s->sources[index].expect.kind!=AE_LOGIN_SOURCE_DATA)return -EINVAL;
    r=ae_login_sources_check(s);
    if(!r)memcpy(out,s->sources[index].expect.digest,32);
    return r;
}

int ae_login_sources_read_asset(struct ae_login_sources *s,size_t index,void **out,size_t *size)
{
    int r=owner(s);if(r)return r;
    if(!out || *out || !size || index>=s->count ||
       s->sources[index].expect.kind!=AE_LOGIN_SOURCE_ASSET)return -EINVAL;
    r=ae_login_sources_check(s);if(r)return r;
    const struct source *source=&s->sources[index];
    size_t length=(size_t)source->expect.size;
    unsigned char *bytes=malloc(length);
    if(!bytes)return -ENOMEM;
    size_t offset=0;
    while(!r && offset<length) {
        ssize_t count=pread(source->file,bytes+offset,length-offset,(off_t)offset);
        if(count<0 && errno==EINTR)continue;
        if(count<=0)r=count<0?-errno:-EBADMSG;
        else offset+=(size_t)count;
    }
    uint8_t digest[AE_SHA256_BYTES];
    if(!r)r=ae_sha256_bytes(bytes,length,digest);
    if(!r && memcmp(digest,source->expect.digest,sizeof digest))r=-EBADMSG;
    if(!r)r=ae_login_sources_check(s);
    if(r){s->error=r;free(bytes);return r;}
    *out=bytes;*size=length;return 0;
}

struct fingerprint_bytes {
    uint8_t bytes[32768];
    size_t used;
};

_Static_assert(16 + AE_LOGIN_SOURCES_MAX *
    (AE_LOGIN_SOURCE_PATH_MAX + LEAF + AE_LOGIN_SOURCE_LABEL_MAX + AE_SHA256_BYTES +
     19 * 8 + (DEPTH + 1) * 96) <= sizeof ((struct fingerprint_bytes *)0)->bytes,
    "source fingerprint buffer must cover every admitted path and ancestor");

static void fingerprint_number(struct fingerprint_bytes *buffer,uint64_t number)
{
    for(size_t n=0;n<8;++n)buffer->bytes[buffer->used++]=(uint8_t)(number>>(56U-(unsigned)n*8U));
}

static void fingerprint_text(struct fingerprint_bytes *buffer,const char *text)
{
    size_t size=strlen(text)+1;
    memcpy(buffer->bytes+buffer->used,text,size);
    buffer->used+=size;
}

int ae_login_sources_fingerprint(struct ae_login_sources *s,uint8_t out[32])
{
    if(!out)return -EINVAL;
    int r=ae_login_sources_check(s);
    if(r)return r;
    struct fingerprint_bytes buffer={.bytes="AETHLGS1",.used=8};
    fingerprint_number(&buffer,s->count);
    for(size_t i=0;i<s->count;++i) {
        const struct source *source=&s->sources[i];
        const struct statx *object=&source->object;
        fingerprint_text(&buffer,source->path);
        fingerprint_text(&buffer,source->leaf);
        fingerprint_text(&buffer,source->label);
        memcpy(buffer.bytes+buffer.used,source->expect.digest,32);buffer.used+=32;
        const uint64_t numbers[]={source->expect.kind,source->expect.mode,source->expect.size,source->depth,
            object->stx_mnt_id,object->stx_ino,object->stx_dev_major,object->stx_dev_minor,
            object->stx_uid,object->stx_gid,object->stx_mode,object->stx_nlink,object->stx_size,
            (uint64_t)object->stx_btime.tv_sec,object->stx_btime.tv_nsec,
            (uint64_t)object->stx_mtime.tv_sec,object->stx_mtime.tv_nsec,
            (uint64_t)object->stx_ctime.tv_sec,object->stx_ctime.tv_nsec};
        for(size_t k=0;k<sizeof numbers/sizeof numbers[0];++k)fingerprint_number(&buffer,numbers[k]);
        for(size_t j=0;j<=source->depth;++j) {
            const struct ae_root_identity *root=&source->ancestors[j];
            const uint64_t fields[]={root->mount_id,root->inode,root->device_major,root->device_minor,
                root->uid,root->gid,root->mode,(uint64_t)root->filesystem_type,(uint64_t)root->birth_present,
                (uint64_t)root->birth_seconds,root->birth_nanoseconds};
            for(size_t k=0;k<sizeof fields/sizeof fields[0];++k)fingerprint_number(&buffer,fields[k]);
            memcpy(buffer.bytes+buffer.used,root->filesystem_id,8);buffer.used+=8;
        }
    }
    r=ae_login_sources_check(s);
    return r?r:ae_sha256_bytes(buffer.bytes,buffer.used,out);
}
