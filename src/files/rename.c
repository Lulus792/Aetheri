#define _GNU_SOURCE
#include "rename.h"
#include "create.h"
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
struct ae_files_rename {
    int directory,witness;struct stat original;bool attempted;
    enum ae_files_rename_result result;char old_name[256],new_name[256];
};
static bool parent(int fd) {
    struct stat st;return getuid()!=0 && getuid()==geteuid() && getgid()==getegid() &&
        !fstat(fd,&st) && S_ISDIR(st.st_mode) && st.st_uid==geteuid() && !(st.st_mode&07022) && (st.st_mode&0300)==0300;
}
static bool raw_leaf(const char *name,size_t size) {
    return name && size && size<=255 && !memchr(name,0,size) && !memchr(name,'/',size) &&
        !(size==1 && name[0]=='.') && !(size==2 && !memcmp(name,"..",2));
}
static bool same(const struct stat *a,const struct stat *b) {
    return a->st_dev==b->st_dev && a->st_ino==b->st_ino && (a->st_mode&S_IFMT)==(b->st_mode&S_IFMT);
}
static bool source(struct ae_files_rename *op) {
    struct stat held,named;return !fstat(op->witness,&held) && same(&held,&op->original) &&
        held.st_uid==geteuid() && !(held.st_mode&07000) &&
        !fstatat(op->directory,op->old_name,&named,AT_SYMLINK_NOFOLLOW) && same(&held,&named);
}
struct ae_files_rename *ae_files_rename_prepare(int directory,int witness,
        const char *old_name,size_t old_size,const char *new_name,size_t new_size,enum ae_files_rename_result *result) {
    if (!result) return NULL;
    *result=AE_FILES_RENAME_INVALID;
    if (directory<0 || witness<0 || !raw_leaf(old_name,old_size) || !ae_files_create_name(new_name,new_size) ||
        (old_size==new_size && !memcmp(old_name,new_name,old_size))) return NULL;
    int flags=fcntl(witness,F_GETFL);struct stat held;
    if (flags<0 || !(flags&O_PATH) || fstat(witness,&held) || (!S_ISREG(held.st_mode) && !S_ISDIR(held.st_mode))) return NULL;
    if (!parent(directory) || held.st_uid!=geteuid() || (held.st_mode&07000)) { *result=AE_FILES_RENAME_DENIED;return NULL; }
    struct ae_files_rename *op=calloc(1,sizeof(*op));if (!op) { *result=AE_FILES_RENAME_NOMEM;return NULL; }
    op->directory=-1;op->witness=-1;op->original=held;op->result=AE_FILES_RENAME_PREPARED;
    memcpy(op->old_name,old_name,old_size);memcpy(op->new_name,new_name,new_size);
    op->directory=fcntl(directory,F_DUPFD_CLOEXEC,4);op->witness=fcntl(witness,F_DUPFD_CLOEXEC,4);
    if (op->directory<0 || op->witness<0) { *result=AE_FILES_RENAME_IO;ae_files_rename_close(op);return NULL; }
    if (!source(op)) { *result=AE_FILES_RENAME_CHANGED;ae_files_rename_close(op);return NULL; }
    *result=op->result;return op;
}
enum ae_files_rename_result ae_files_rename_apply(struct ae_files_rename *op) {
    if (!op) return AE_FILES_RENAME_INVALID;
    if (op->attempted) return op->result;
    op->attempted=true;
    if (!parent(op->directory)) return op->result=AE_FILES_RENAME_DENIED;
    if (!source(op)) return op->result=AE_FILES_RENAME_CHANGED;
    struct stat st;
    if (!fstatat(op->directory,op->new_name,&st,AT_SYMLINK_NOFOLLOW)) return op->result=AE_FILES_RENAME_CONFLICT;
    if (errno!=ENOENT) return op->result=AE_FILES_RENAME_IO;
    /* The destination guard is in the kernel. The earlier source comparison is
       only an ordinary local observation; this syscall has no source-inode CAS. */
    if (renameat2(op->directory,op->old_name,op->directory,op->new_name,RENAME_NOREPLACE)) return op->result=AE_FILES_RENAME_UNKNOWN;
    if (fsync(op->directory)) return op->result=AE_FILES_RENAME_UNKNOWN;
    if (fstatat(op->directory,op->new_name,&st,AT_SYMLINK_NOFOLLOW) || !same(&st,&op->original)) return op->result=AE_FILES_RENAME_UNKNOWN;
    if (!fstatat(op->directory,op->old_name,&st,AT_SYMLINK_NOFOLLOW) || errno!=ENOENT) return op->result=AE_FILES_RENAME_UNKNOWN;
    return op->result=AE_FILES_RENAME_DONE;
}
enum ae_files_rename_result ae_files_rename_status(const struct ae_files_rename *op) { return op ? op->result : AE_FILES_RENAME_INVALID; }
const char *ae_files_rename_old(const struct ae_files_rename *op) { return op ? op->old_name : ""; }
const char *ae_files_rename_new(const struct ae_files_rename *op) { return op ? op->new_name : ""; }
void ae_files_rename_close(struct ae_files_rename *op) { if (op) { if (op->directory>=0) close(op->directory);if (op->witness>=0) close(op->witness);free(op); } }
