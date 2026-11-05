#include "file.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/random.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/xattr.h>
#include <unistd.h>

enum { ATTR_COUNT=64, ATTR_BYTES=65536, ATTR_NAMES=16384 };
struct attribute { char *name;void *value;size_t size; };
struct metadata { struct attribute attrs[ATTR_COUNT];size_t count,bytes; };
struct ae_editor_file {
    int directory,fd;char leaf[256],temporary[64];
    char *baseline;size_t size;struct stat observed;struct metadata metadata;
    bool uncertain;
};
static void erase(void *memory,size_t size) { volatile unsigned char *p=memory;while (size--) *p++=0; }
static void metadata_free(struct metadata *m) {
    for (size_t i=0;i<m->count;++i) { free(m->attrs[i].name);if (m->attrs[i].value) { erase(m->attrs[i].value,m->attrs[i].size);free(m->attrs[i].value); } }
    *m=(struct metadata){0};
}
void ae_editor_file_close(struct ae_editor_file *f) {
    if (!f) return;
    if (f->fd>=0) (void)close(f->fd);
    if (f->directory>=0) (void)close(f->directory);
    if (f->baseline) { erase(f->baseline,f->size);free(f->baseline); }
    metadata_free(&f->metadata);erase(f,sizeof(*f));free(f);
}
bool ae_editor_file_uncertain(const struct ae_editor_file *f) { return f && f->uncertain; }
const char *ae_editor_file_recovery(const struct ae_editor_file *f) { return f && f->temporary[0] ? f->temporary : NULL; }
static bool same(const struct stat *a,const struct stat *b) {
    return a->st_dev==b->st_dev && a->st_ino==b->st_ino && a->st_mode==b->st_mode && a->st_uid==b->st_uid && a->st_gid==b->st_gid &&
        a->st_nlink==b->st_nlink && a->st_size==b->st_size && a->st_mtim.tv_sec==b->st_mtim.tv_sec && a->st_mtim.tv_nsec==b->st_mtim.tv_nsec &&
        a->st_ctim.tv_sec==b->st_ctim.tv_sec && a->st_ctim.tv_nsec==b->st_ctim.tv_nsec;
}
static enum ae_editor_file_result metadata_read(int fd,struct metadata *m) {
    char names[ATTR_NAMES];ssize_t n=flistxattr(fd,names,sizeof(names));
    if (n<0) return errno==ERANGE ? AE_EDITOR_FILE_UNSUPPORTED : AE_EDITOR_FILE_IO;
    size_t at=0;
    while (at<(size_t)n) {
        size_t length=strnlen(names+at,(size_t)n-at);
        if (!length || length>255 || length==(size_t)n-at || m->count==ATTR_COUNT || !strcmp(names+at,"security.capability")) return AE_EDITOR_FILE_UNSUPPORTED;
        ssize_t size=fgetxattr(fd,names+at,NULL,0);
        if (size<0) return AE_EDITOR_FILE_IO;
        if ((size_t)size>ATTR_BYTES-m->bytes) return AE_EDITOR_FILE_UNSUPPORTED;
        struct attribute *a=&m->attrs[m->count++];a->name=strdup(names+at);a->size=(size_t)size;
        if (!a->name) return AE_EDITOR_FILE_NOMEM;
        if (size) { a->value=malloc((size_t)size);if (!a->value) return AE_EDITOR_FILE_NOMEM; }
        /* A zero-length value still gets a nonzero capacity so a concurrent
           growth cannot turn this read into a successful size-only query. */
        unsigned char empty;
        ssize_t got=fgetxattr(fd,names+at,size ? a->value : &empty,size ? (size_t)size : 1u);
        if (got!=size) return AE_EDITOR_FILE_IO;
        m->bytes+=(size_t)size;at+=length+1;
    }
    return AE_EDITOR_FILE_OK;
}
static bool metadata_equal(const struct metadata *a,const struct metadata *b) {
    if (a->count!=b->count || a->bytes!=b->bytes) return false;
    for (size_t i=0;i<a->count;++i) {
        size_t j=0;while (j<b->count && strcmp(a->attrs[i].name,b->attrs[j].name)) ++j;
        if (j==b->count || a->attrs[i].size!=b->attrs[j].size || (a->attrs[i].size && memcmp(a->attrs[i].value,b->attrs[j].value,a->attrs[i].size))) return false;
    }
    return true;
}
static enum ae_editor_file_result attributes_apply(int fd,const struct metadata *m) {
    for (size_t i=0;i<m->count;++i) if (fsetxattr(fd,m->attrs[i].name,m->attrs[i].value,m->attrs[i].size,0)) {
        /* An inherited immutable security label may already be exactly right. */
        unsigned char value[ATTR_BYTES];ssize_t n=fgetxattr(fd,m->attrs[i].name,value,sizeof(value));
        if (n<0 || (size_t)n!=m->attrs[i].size || (n && memcmp(value,m->attrs[i].value,(size_t)n))) return AE_EDITOR_FILE_IO;
    }
    struct metadata check={0};enum ae_editor_file_result r=metadata_read(fd,&check);
    if (r==AE_EDITOR_FILE_OK && !metadata_equal(m,&check)) r=AE_EDITOR_FILE_UNSUPPORTED;
    metadata_free(&check);return r;
}
static enum ae_editor_file_result setup(int directory,const char *leaf,struct ae_editor_file **out) {
    if (!out || *out || !leaf || !*leaf || strnlen(leaf,256)>255 || !strcmp(leaf,".") || !strcmp(leaf,"..") || strchr(leaf,'/')) return AE_EDITOR_FILE_INVALID;
    struct ae_editor_file *f=calloc(1,sizeof(*f));if (!f) return AE_EDITOR_FILE_NOMEM;
    f->fd=f->directory=-1;f->directory=openat(directory,".",O_RDONLY|O_DIRECTORY|O_CLOEXEC);
    struct stat s;
    if (f->directory<0 || fstat(f->directory,&s)) { ae_editor_file_close(f);return AE_EDITOR_FILE_IO; }
    if (s.st_uid!=geteuid() || (s.st_mode&0022) || !geteuid()) { ae_editor_file_close(f);return AE_EDITOR_FILE_UNSUPPORTED; }
    memcpy(f->leaf,leaf,strlen(leaf)+1);*out=f;return AE_EDITOR_FILE_OK;
}
static bool absent(struct ae_editor_file *f) {
    struct stat s;return fstatat(f->directory,f->leaf,&s,AT_SYMLINK_NOFOLLOW)!=0 && errno==ENOENT;
}
static bool named(struct ae_editor_file *f,const struct stat *expected) {
    struct stat s;return fstatat(f->directory,f->leaf,&s,AT_SYMLINK_NOFOLLOW)==0 && same(&s,expected);
}
static bool read_exact(int fd,char *out,size_t size) {
    size_t at=0;
    while (at<size) { ssize_t n=pread(fd,out+at,size-at,(off_t)at);if (n<0 && errno==EINTR) continue;if (n<=0) return false;at+=(size_t)n; }
    char extra;ssize_t n;do { n=pread(fd,&extra,1,(off_t)size); } while (n<0 && errno==EINTR);
    return n==0;
}
static enum ae_editor_file_result content(int fd,const char *expected,size_t size) {
    char bytes[65536];size_t at=0;
    while (at<size) {
        size_t amount=size-at;if (amount>sizeof(bytes)) amount=sizeof(bytes);
        ssize_t n=pread(fd,bytes,amount,(off_t)at);if (n<0 && errno==EINTR) continue;
        if (n<=0) return AE_EDITOR_FILE_IO;
        if (memcmp(bytes,expected+at,(size_t)n)) return AE_EDITOR_FILE_CONFLICT;
        at+=(size_t)n;
    }
    char extra;ssize_t n;do { n=pread(fd,&extra,1,(off_t)size); } while (n<0 && errno==EINTR);
    return n<0 ? AE_EDITOR_FILE_IO : n ? AE_EDITOR_FILE_CONFLICT : AE_EDITOR_FILE_OK;
}
static enum ae_editor_file_result open_file(int directory,const char *leaf,int witness,struct ae_editor_file **out,struct ae_document **document) {
    if (!document || *document) return AE_EDITOR_FILE_INVALID;
    struct ae_editor_file *f=NULL;enum ae_editor_file_result r=setup(directory,leaf,&f);if (r!=AE_EDITOR_FILE_OK) return r;
    if (!out || *out) { ae_editor_file_close(f);return AE_EDITOR_FILE_INVALID; }
    int pin=openat(f->directory,f->leaf,O_PATH|O_CLOEXEC|O_NOFOLLOW);struct stat s;
    if (pin<0 || fstat(pin,&s)) { if (pin>=0) (void)close(pin);ae_editor_file_close(f);return AE_EDITOR_FILE_IO; }
    if (witness>=0) { struct stat expected;if (fstat(witness,&expected) || !S_ISREG(expected.st_mode) || expected.st_dev!=s.st_dev || expected.st_ino!=s.st_ino) { close(pin);ae_editor_file_close(f);return AE_EDITOR_FILE_CONFLICT; } }
    if (!S_ISREG(s.st_mode) || s.st_uid!=geteuid() || s.st_nlink!=1 || (s.st_mode&07000) || s.st_size<0 || s.st_size>AE_DOCUMENT_BYTES) {
        (void)close(pin);ae_editor_file_close(f);return AE_EDITOR_FILE_UNSUPPORTED;
    }
    char path[64];int length=snprintf(path,sizeof(path),"/proc/self/fd/%d",pin);
    if (length>0 && (size_t)length<sizeof(path)) f->fd=open(path,O_RDONLY|O_NONBLOCK|O_CLOEXEC);
    (void)close(pin);
    struct stat before;
    if (f->fd<0 || fstat(f->fd,&before) || !same(&s,&before)) { ae_editor_file_close(f);return AE_EDITOR_FILE_CONFLICT; }
    f->size=(size_t)s.st_size;if (f->size) { f->baseline=malloc(f->size);if (!f->baseline) { ae_editor_file_close(f);return AE_EDITOR_FILE_NOMEM; } }
    if (!read_exact(f->fd,f->baseline,f->size)) r=AE_EDITOR_FILE_IO;
    if (r==AE_EDITOR_FILE_OK) r=metadata_read(f->fd,&f->metadata);
    if (r==AE_EDITOR_FILE_OK && (fstat(f->fd,&f->observed) || !same(&s,&f->observed) || !named(f,&s))) r=AE_EDITOR_FILE_CONFLICT;
    enum ae_document_result text_result=AE_DOCUMENT_INVALID;struct ae_document *d=NULL;
    if (r==AE_EDITOR_FILE_OK) { d=ae_document_create(f->baseline,f->size,&text_result);if (!d) r=text_result==AE_DOCUMENT_NOMEM ? AE_EDITOR_FILE_NOMEM : AE_EDITOR_FILE_UNSUPPORTED; }
    if (r!=AE_EDITOR_FILE_OK) { ae_editor_file_close(f);return r; }
    *document=d;*out=f;return AE_EDITOR_FILE_OK;
}
enum ae_editor_file_result ae_editor_file_open(int directory,const char *leaf,struct ae_editor_file **out,struct ae_document **document) { return open_file(directory,leaf,-1,out,document); }
enum ae_editor_file_result ae_editor_file_open_selected(int directory,const char *leaf,int witness,struct ae_editor_file **out,struct ae_document **document) {
    return witness<0 ? AE_EDITOR_FILE_INVALID : open_file(directory,leaf,witness,out,document);
}
enum ae_editor_file_result ae_editor_file_new(int directory,const char *leaf,struct ae_editor_file **out,struct ae_document **document) {
    if (!out || *out || !document || *document) return AE_EDITOR_FILE_INVALID;
    struct ae_editor_file *f=NULL;enum ae_editor_file_result r=setup(directory,leaf,&f);if (r!=AE_EDITOR_FILE_OK) return r;
    if (!absent(f)) { ae_editor_file_close(f);return AE_EDITOR_FILE_CONFLICT; }
    enum ae_document_result text_result;struct ae_document *d=ae_document_create(NULL,0,&text_result);
    if (!d) { ae_editor_file_close(f);return AE_EDITOR_FILE_NOMEM; }
    *document=d;*out=f;return AE_EDITOR_FILE_OK;
}
static enum ae_editor_file_result current(struct ae_editor_file *f) {
    struct stat parent;
    if (fstat(f->directory,&parent)) return AE_EDITOR_FILE_IO;
    if (parent.st_uid!=geteuid() || (parent.st_mode&0022) || !geteuid()) return AE_EDITOR_FILE_UNSUPPORTED;
    if (f->fd<0) return absent(f) ? AE_EDITOR_FILE_OK : AE_EDITOR_FILE_CONFLICT;
    struct stat s;if (fstat(f->fd,&s) || !same(&f->observed,&s) || !named(f,&s)) return AE_EDITOR_FILE_CONFLICT;
    enum ae_editor_file_result r=content(f->fd,f->baseline,f->size);if (r!=AE_EDITOR_FILE_OK) return r;
    if (fstat(f->fd,&s) || !same(&f->observed,&s) || !named(f,&s)) return AE_EDITOR_FILE_CONFLICT;
    return AE_EDITOR_FILE_OK;
}
static bool write_all(int fd,const char *text,size_t size) {
    size_t at=0;while (at<size) { ssize_t n=write(fd,text+at,size-at);if (n<0 && errno==EINTR) continue;if (n<=0) return false;at+=(size_t)n; }return true;
}
static int temporary(struct ae_editor_file *f) {
    unsigned char random[16];ssize_t n;do { n=getrandom(random,sizeof(random),0); } while (n<0 && errno==EINTR);
    if (n!=(ssize_t)sizeof(random)) return -1;
    static const char hex[]="0123456789abcdef";memcpy(f->temporary,".aetheri-save-",14);
    for (size_t i=0;i<sizeof(random);++i) { f->temporary[14+i*2]=hex[random[i]>>4];f->temporary[15+i*2]=hex[random[i]&15]; }
    f->temporary[46]=0;
    int fd=openat(f->directory,f->temporary,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);
    if (fd<0) f->temporary[0]=0;
    return fd;
}
enum ae_editor_file_result ae_editor_file_save(struct ae_editor_file *f,struct ae_document *d) {
    if (!f || !d) return AE_EDITOR_FILE_INVALID;
    if (f->uncertain) return AE_EDITOR_FILE_UNKNOWN;
    struct ae_document_info info;if (!ae_document_info(d,&info)) return AE_EDITOR_FILE_INVALID;
    enum ae_editor_file_result r=current(f);if (r!=AE_EDITOR_FILE_OK) return r;
    if (f->fd>=0 && !(f->observed.st_mode&0200)) return AE_EDITOR_FILE_UNSUPPORTED;
    char *snapshot=info.size ? malloc(info.size) : NULL;if (info.size && !snapshot) return AE_EDITOR_FILE_NOMEM;
    if (!ae_document_copy(d,0,info.size,snapshot,info.size)) { free(snapshot);return AE_EDITOR_FILE_INVALID; }
    int fd=temporary(f);if (fd<0) { free(snapshot);return AE_EDITOR_FILE_IO; }
    struct metadata created={0};
    r=AE_EDITOR_FILE_IO;
    if (!write_all(fd,snapshot,info.size)) goto before;
    if (f->fd>=0) {
        if (fchown(fd,geteuid(),f->observed.st_gid) || fchmod(fd,f->observed.st_mode&0777)) goto before;
        r=attributes_apply(fd,&f->metadata);if (r!=AE_EDITOR_FILE_OK) goto before;
        struct stat check;
        if (fstat(fd,&check) || check.st_uid!=f->observed.st_uid || check.st_gid!=f->observed.st_gid || (check.st_mode&07777)!=(f->observed.st_mode&07777)) { r=AE_EDITOR_FILE_UNSUPPORTED;goto before; }
    } else {
        if (fchmod(fd,0600)) goto before;
        r=metadata_read(fd,&created);if (r!=AE_EDITOR_FILE_OK) goto before;
    }
    if (fsync(fd) || fsync(f->directory)) { r=AE_EDITOR_FILE_IO;goto before; }
    r=current(f);if (r!=AE_EDITOR_FILE_OK) goto before;
    /* Existing-file replacement is deliberately not advertised as CAS. */
    if (renameat2(f->directory,f->temporary,f->directory,f->leaf,f->fd<0 ? (unsigned)RENAME_NOREPLACE : 0u)) {
        if (f->fd<0 && errno==EEXIST) { r=AE_EDITOR_FILE_CONFLICT;goto before; }
        f->uncertain=true;r=AE_EDITOR_FILE_UNKNOWN;goto done;
    }
    f->temporary[0]=0;
    if (fsync(f->directory) || fstat(fd,&f->observed) || !named(f,&f->observed)) { f->uncertain=true;r=AE_EDITOR_FILE_UNKNOWN;goto done; }
    /* Reopen the same committed object, not a candidate selected by its path. */
    char path[64];int length=snprintf(path,sizeof(path),"/proc/self/fd/%d",fd);
    int reader=length>0 && (size_t)length<sizeof(path) ? open(path,O_RDONLY|O_CLOEXEC) : -1;
    struct stat observed;
    if (reader<0 || content(reader,snapshot,info.size)!=AE_EDITOR_FILE_OK || fstat(reader,&observed) || !same(&f->observed,&observed) || !named(f,&observed)) {
        if (reader>=0) (void)close(reader);
        f->uncertain=true;r=AE_EDITOR_FILE_UNKNOWN;goto done;
    }
    /* Linux close may report an earlier write error after releasing the FD.
       Never retry that FD number, and never acknowledge clean before it. */
    int closed=close(fd);fd=-1;
    if (closed || !ae_document_mark_clean(d,info.revision)) { (void)close(reader);f->uncertain=true;r=AE_EDITOR_FILE_UNKNOWN;goto done; }
    if (f->fd<0) { metadata_free(&f->metadata);f->metadata=created;created=(struct metadata){0}; }
    if (f->fd>=0) (void)close(f->fd);
    f->fd=reader;if (f->baseline) { erase(f->baseline,f->size);free(f->baseline); }
    f->baseline=snapshot;f->size=info.size;snapshot=NULL;r=AE_EDITOR_FILE_OK;goto done;
before:
    if (unlinkat(f->directory,f->temporary,0) || fsync(f->directory)) { f->uncertain=true;r=AE_EDITOR_FILE_UNKNOWN; }
    else f->temporary[0]=0;
done:
    if (fd>=0) (void)close(fd);
    metadata_free(&created);if (snapshot) { erase(snapshot,info.size);free(snapshot); }return r;
}
