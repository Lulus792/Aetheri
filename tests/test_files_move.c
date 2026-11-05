#define _GNU_SOURCE
#include "files/rename.h"
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"Files move %d: %s errno=%d\n",__LINE__,#x,errno);exit(1); } } while (0)
int __real_renameat2(int od,const char *old,int nd,const char *name,unsigned flags);int __real_fsync(int fd);
static unsigned calls,flushes,fail_flush;static bool lost,cross;static ino_t flushed[16];
int __wrap_renameat2(int od,const char *old,int nd,const char *name,unsigned flags) {
    CHECK(flags==RENAME_NOREPLACE);++calls;if (cross) { errno=EXDEV;return -1; }
    int r=__real_renameat2(od,old,nd,name,flags);if (!r && lost) { errno=EIO;return -1; }return r;
}
int __wrap_fsync(int fd) { struct stat st;CHECK(!fstat(fd,&st));if (flushes<16) flushed[flushes]=st.st_ino;++flushes;int r=__real_fsync(fd);if (!r && fail_flush==flushes) { errno=EIO;return -1; }return r; }
static void put(int dir,const char *name) { int fd=openat(dir,name,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC,0600);CHECK(fd>=0 && write(fd,"retained",8)==8 && !close(fd)); }
static struct stat entry(int dir,const char *name) { struct stat st;CHECK(!fstatat(dir,name,&st,AT_SYMLINK_NOFOLLOW));return st; }
static bool absent(int dir,const char *name) { struct stat st;return fstatat(dir,name,&st,AT_SYMLINK_NOFOLLOW)<0 && errno==ENOENT; }
static struct ae_files_rename *prepare(int from,const char *name,int to,const char *leaf,enum ae_files_rename_result expected) {
    int pin=openat(from,name,O_PATH|O_NOFOLLOW|O_CLOEXEC);CHECK(pin>=3);enum ae_files_rename_result r;
    struct ae_files_rename *op=ae_files_move_prepare(from,pin,name,strlen(name),to,leaf,strlen(leaf),&r);close(pin);
    CHECK(r==expected && (expected==AE_FILES_RENAME_PREPARED ? op!=NULL : op==NULL));return op;
}
static int folder(int parent,const char *name) { CHECK(!mkdirat(parent,name,0700));int fd=openat(parent,name,O_RDONLY|O_DIRECTORY|O_CLOEXEC);CHECK(fd>=3);return fd; }
int main(void) {
    CHECK(getuid()!=0);char path[]="files-move-XXXXXX";CHECK(mkdtemp(path));int root=open(path,O_RDONLY|O_DIRECTORY|O_CLOEXEC);CHECK(root>=3);
    int from=folder(root,"source"),to=folder(root,"target");put(from,"note.txt");ino_t inode=entry(from,"note.txt").st_ino;
    struct ae_files_rename *op=prepare(from,"note.txt",to,"note.txt",AE_FILES_RENAME_PREPARED);
    CHECK(!calls && !flushes && absent(to,"note.txt"));CHECK(ae_files_rename_apply(op)==AE_FILES_RENAME_DONE && absent(from,"note.txt") && entry(to,"note.txt").st_ino==inode);
    struct stat fs,ts;CHECK(!fstat(from,&fs) && !fstat(to,&ts) && flushes==2 && flushed[0]==ts.st_ino && flushed[1]==fs.st_ino);
    unsigned before=calls,synced=flushes;put(from,"note.txt");CHECK(ae_files_rename_apply(op)==AE_FILES_RENAME_DONE && calls==before && flushes==synced);ae_files_rename_close(op);
    op=prepare(from,"note.txt",to,"note.txt",AE_FILES_RENAME_PREPARED);CHECK(ae_files_rename_apply(op)==AE_FILES_RENAME_CONFLICT && calls==before);ae_files_rename_close(op);
    CHECK(!symlinkat("missing",to,"link"));op=prepare(from,"note.txt",to,"link",AE_FILES_RENAME_PREPARED);CHECK(ae_files_rename_apply(op)==AE_FILES_RENAME_CONFLICT && S_ISLNK(entry(to,"link").st_mode));ae_files_rename_close(op);
    CHECK(!prepare(from,"note.txt",from,"note.txt",AE_FILES_RENAME_INVALID));
    op=prepare(from,"note.txt",to,"denied",AE_FILES_RENAME_PREPARED);CHECK(!fchmod(to,0777));CHECK(ae_files_rename_apply(op)==AE_FILES_RENAME_DENIED && !fchmod(to,0700));CHECK(ae_files_rename_apply(op)==AE_FILES_RENAME_DENIED && calls==before);ae_files_rename_close(op);
    op=prepare(from,"note.txt",to,"stale",AE_FILES_RENAME_PREPARED);CHECK(!renameat(from,"note.txt",from,"original"));put(from,"note.txt");CHECK(ae_files_rename_apply(op)==AE_FILES_RENAME_CHANGED && absent(to,"stale"));ae_files_rename_close(op);
    op=prepare(from,"note.txt",to,"held.txt",AE_FILES_RENAME_PREPARED);
    CHECK(!renameat(root,"source",root,"moved-source") && !renameat(root,"target",root,"moved-target"));int rf=folder(root,"source"),rt=folder(root,"target");
    CHECK(ae_files_rename_apply(op)==AE_FILES_RENAME_DONE && absent(from,"note.txt") && entry(to,"held.txt").st_size==8 && absent(rf,"note.txt") && absent(rt,"held.txt"));ae_files_rename_close(op);close(rf);close(rt);
    int tree=folder(from,"tree"),child=folder(tree,"child");put(child,"inside");
    CHECK(!prepare(from,"tree",tree,"self",AE_FILES_RENAME_OVERLAP));CHECK(!prepare(from,"tree",child,"nested",AE_FILES_RENAME_OVERLAP));
    int relocated=folder(to,"relocated");op=prepare(from,"tree",relocated,"moved",AE_FILES_RENAME_PREPARED);
    CHECK(!renameat(to,"relocated",tree,"entered"));before=calls;CHECK(ae_files_rename_apply(op)==AE_FILES_RENAME_OVERLAP && calls==before);ae_files_rename_close(op);close(relocated);
    synced=flushes;inode=entry(from,"tree").st_ino;op=prepare(from,"tree",to,"tree",AE_FILES_RENAME_PREPARED);CHECK(ae_files_rename_apply(op)==AE_FILES_RENAME_DONE && entry(to,"tree").st_ino==inode && flushes==synced+3 && entry(child,"inside").st_size==8);ae_files_rename_close(op);close(child);close(tree);
    int deep=folder(to,"deep");for (unsigned i=0;i<130;++i) { int next=folder(deep,"p");close(deep);deep=next; }
    CHECK(!prepare(to,"tree",deep,"too-deep",AE_FILES_RENAME_LIMIT));close(deep);
    put(from,"loss");op=prepare(from,"loss",to,"lost-reply",AE_FILES_RENAME_PREPARED);lost=true;CHECK(ae_files_rename_apply(op)==AE_FILES_RENAME_UNKNOWN && absent(from,"loss") && entry(to,"lost-reply").st_size==8);lost=false;
    CHECK(!renameat(to,"lost-reply",from,"loss"));before=calls;synced=flushes;CHECK(ae_files_rename_apply(op)==AE_FILES_RENAME_UNKNOWN && calls==before && flushes==synced && absent(to,"lost-reply"));ae_files_rename_close(op);
    op=prepare(from,"loss",to,"lost-flush",AE_FILES_RENAME_PREPARED);fail_flush=flushes+2;CHECK(ae_files_rename_apply(op)==AE_FILES_RENAME_UNKNOWN && absent(from,"loss") && entry(to,"lost-flush").st_size==8);fail_flush=0;before=calls;synced=flushes;CHECK(ae_files_rename_apply(op)==AE_FILES_RENAME_UNKNOWN && calls==before && flushes==synced);ae_files_rename_close(op);
    put(from,"cross");op=prepare(from,"cross",to,"cross",AE_FILES_RENAME_PREPARED);cross=true;CHECK(ae_files_rename_apply(op)==AE_FILES_RENAME_UNKNOWN && entry(from,"cross").st_size==8 && absent(to,"cross"));cross=false;ae_files_rename_close(op);
    CHECK(!close(from) && !close(to) && !close(root));printf("Files move evidence: %s\n",path);puts("Files move: held parents/source, both parent and moved-directory flushes, overlap/depth, conflicts, loss without replay and no copy/delete fallback passed");return 0;
}
