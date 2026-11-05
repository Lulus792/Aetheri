#define _GNU_SOURCE
#include "create.h"
#include "editor/file.h"
#include "text/grapheme.h"
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
struct ae_files_create {
    int directory;enum ae_files_create_kind kind;enum ae_files_create_result result;
    bool attempted;char leaf[256],recovery[64];
};
bool ae_files_create_name(const char *name,size_t size) {
    if (!name || !size || size>255 || (size==1 && name[0]=='.') || (size==2 && !memcmp(name,"..",2))) return false;
    size_t at=0;
    while (at<size) {
        uint32_t c;if (!ae_utf8_next(name,size,&at,&c) || c=='/' || c<32 || (c>=0x7f && c<=0x9f) ||
            (c>=0x200b && c<=0x200f) || (c>=0x2028 && c<=0x202e) || (c>=0x2060 && c<=0x206f)) return false;
    }
    return true;
}
static bool parent(int fd) {
    struct stat st;
    return getuid()!=0 && getuid()==geteuid() && getgid()==getegid() && !fstat(fd,&st) &&
        S_ISDIR(st.st_mode) && st.st_uid==geteuid() && !(st.st_mode&07022) && (st.st_mode&0300)==0300;
}
struct ae_files_create *ae_files_create_prepare(int directory,enum ae_files_create_kind kind,
        const char *name,size_t size,enum ae_files_create_result *result) {
    if (!result) return NULL;
    *result=AE_FILES_CREATE_INVALID;
    if (directory<0 || (kind!=AE_FILES_CREATE_FILE && kind!=AE_FILES_CREATE_DIRECTORY) || !ae_files_create_name(name,size)) return NULL;
    if (!parent(directory)) { *result=AE_FILES_CREATE_DENIED;return NULL; }
    struct ae_files_create *op=calloc(1,sizeof(*op));if (!op) { *result=AE_FILES_CREATE_NOMEM;return NULL; }
    op->directory=fcntl(directory,F_DUPFD_CLOEXEC,4);
    if (op->directory<0) { free(op);*result=AE_FILES_CREATE_IO;return NULL; }
    op->kind=kind;op->result=AE_FILES_CREATE_PREPARED;memcpy(op->leaf,name,size);op->leaf[size]=0;
    *result=op->result;return op;
}
static enum ae_files_create_result file(struct ae_files_create *op) {
    struct ae_editor_file *f=NULL;struct ae_document *d=NULL;
    enum ae_editor_file_result r=ae_editor_file_new(op->directory,op->leaf,&f,&d);
    if (r==AE_EDITOR_FILE_OK) r=ae_editor_file_save(f,d);
    const char *recovery=ae_editor_file_recovery(f);
    if (recovery) { size_t n=strlen(recovery);if (n>=sizeof(op->recovery)) r=AE_EDITOR_FILE_UNKNOWN;else memcpy(op->recovery,recovery,n+1); }
    ae_editor_file_close(f);ae_document_destroy(d);
    return r==AE_EDITOR_FILE_OK ? AE_FILES_CREATE_DONE : r==AE_EDITOR_FILE_CONFLICT ? AE_FILES_CREATE_CONFLICT :
        r==AE_EDITOR_FILE_UNKNOWN ? AE_FILES_CREATE_UNKNOWN : r==AE_EDITOR_FILE_NOMEM ? AE_FILES_CREATE_NOMEM :
        r==AE_EDITOR_FILE_UNSUPPORTED ? AE_FILES_CREATE_DENIED : r==AE_EDITOR_FILE_INVALID ? AE_FILES_CREATE_INVALID : AE_FILES_CREATE_IO;
}
static enum ae_files_create_result directory(struct ae_files_create *op) {
    if (mkdirat(op->directory,op->leaf,0700)) return errno==EEXIST ? AE_FILES_CREATE_CONFLICT : AE_FILES_CREATE_UNKNOWN;
    /* mkdirat returns no object FD. These are ordinary local observations after
       a successful name creation, never allocation/StorageRef enrollment. */
    int fd=openat(op->directory,op->leaf,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
    if (fd<0) return AE_FILES_CREATE_UNKNOWN;
    struct stat held,named;
    bool valid=!fstat(fd,&held) && S_ISDIR(held.st_mode) && held.st_uid==geteuid() && !(held.st_mode&077) &&
        !fstatat(op->directory,op->leaf,&named,AT_SYMLINK_NOFOLLOW) && held.st_dev==named.st_dev && held.st_ino==named.st_ino;
    bool flushed=valid && !fsync(fd) && !fsync(op->directory);
    int closed=close(fd);
    return flushed && !closed ? AE_FILES_CREATE_DONE : AE_FILES_CREATE_UNKNOWN;
}
enum ae_files_create_result ae_files_create_apply(struct ae_files_create *op) {
    if (!op) return AE_FILES_CREATE_INVALID;
    if (op->attempted) return op->result;
    op->attempted=true;
    if (!parent(op->directory)) return op->result=AE_FILES_CREATE_DENIED;
    return op->result=op->kind==AE_FILES_CREATE_FILE ? file(op) : directory(op);
}
enum ae_files_create_result ae_files_create_status(const struct ae_files_create *op) { return op ? op->result : AE_FILES_CREATE_INVALID; }
const char *ae_files_create_leaf(const struct ae_files_create *op) { return op ? op->leaf : ""; }
const char *ae_files_create_recovery(const struct ae_files_create *op) { return op && *op->recovery ? op->recovery : NULL; }
void ae_files_create_close(struct ae_files_create *op) { if (op) { close(op->directory);free(op); } }
