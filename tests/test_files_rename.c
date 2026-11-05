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
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"Files rename %d: %s errno=%d\n",__LINE__,#x,errno);exit(1); } } while (0)
int __real_renameat2(int od,const char *old,int nd,const char *name,unsigned flags);int __real_fsync(int fd);void *__real_calloc(size_t n,size_t size);
static unsigned renames,flushes;static bool lost,lost_flush,race,deny;
static void put(int dir,const char *name,const char *text) { int fd=openat(dir,name,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC,0600);CHECK(fd>=0);size_t n=strlen(text);CHECK(write(fd,text,n)==(ssize_t)n && !close(fd)); }
int __wrap_renameat2(int od,const char *old,int nd,const char *name,unsigned flags) {
    CHECK(flags==RENAME_NOREPLACE);++renames;if (race) put(nd,name,"foreign");
    int r=__real_renameat2(od,old,nd,name,flags);if (!r && lost) { errno=EIO;return -1; }return r;
}
int __wrap_fsync(int fd) { ++flushes;int r=__real_fsync(fd);if (!r && lost_flush) { errno=EIO;return -1; }return r; }
void *__wrap_calloc(size_t n,size_t size) { return deny ? NULL : __real_calloc(n,size); }
static struct stat entry(int dir,const char *name) { struct stat st;CHECK(!fstatat(dir,name,&st,AT_SYMLINK_NOFOLLOW));return st; }
static bool absent(int dir,const char *name) { struct stat st;return fstatat(dir,name,&st,AT_SYMLINK_NOFOLLOW)<0 && errno==ENOENT; }
static struct ae_files_rename *prepare(int dir,const char *old,const char *name) {
    int fd=openat(dir,old,O_PATH|O_NOFOLLOW|O_CLOEXEC);CHECK(fd>=0);enum ae_files_rename_result r;
    struct ae_files_rename *op=ae_files_rename_prepare(dir,fd,old,strlen(old),name,strlen(name),&r);close(fd);
    CHECK(op && r==AE_FILES_RENAME_PREPARED && ae_files_rename_status(op)==r && !strcmp(ae_files_rename_old(op),old) && !strcmp(ae_files_rename_new(op),name));return op;
}
int main(void) {
    CHECK(getuid()!=0);char path[]="files-rename-XXXXXX";CHECK(mkdtemp(path) && !chmod(path,0700));int root=open(path,O_RDONLY|O_DIRECTORY|O_CLOEXEC);CHECK(root>=3);
    put(root,"old.txt","original");int witness=openat(root,"old.txt",O_PATH|O_NOFOLLOW|O_CLOEXEC);CHECK(witness>=3);enum ae_files_rename_result r;
    CHECK(!ae_files_rename_prepare(root,witness,"old.txt",7,"old.txt",7,&r) && r==AE_FILES_RENAME_INVALID);
    CHECK(!ae_files_rename_prepare(root,witness,"old.txt",7,"bad/name",8,&r) && r==AE_FILES_RENAME_INVALID);
    deny=true;CHECK(!ae_files_rename_prepare(root,witness,"old.txt",7,"oom",3,&r) && r==AE_FILES_RENAME_NOMEM);deny=false;close(witness);
    struct ae_files_rename *one=prepare(root,"old.txt","new.txt"),*two=prepare(root,"old.txt","other.txt");
    ino_t inode=entry(root,"old.txt").st_ino;CHECK(absent(root,"new.txt") && !renames && !flushes);
    CHECK(ae_files_rename_apply(one)==AE_FILES_RENAME_DONE && absent(root,"old.txt") && entry(root,"new.txt").st_ino==inode);
    unsigned calls=renames,synced=flushes;put(root,"old.txt","replacement");
    CHECK(ae_files_rename_apply(one)==AE_FILES_RENAME_DONE && renames==calls && flushes==synced && entry(root,"old.txt").st_ino!=inode);
    CHECK(ae_files_rename_apply(two)==AE_FILES_RENAME_CHANGED && renames==calls && absent(root,"other.txt"));ae_files_rename_close(one);ae_files_rename_close(two);
    one=prepare(root,"old.txt","new.txt");CHECK(ae_files_rename_apply(one)==AE_FILES_RENAME_CONFLICT && renames==calls && entry(root,"new.txt").st_ino==inode);ae_files_rename_close(one);
    CHECK(!symlinkat("missing",root,"link"));one=prepare(root,"old.txt","link");CHECK(ae_files_rename_apply(one)==AE_FILES_RENAME_CONFLICT && S_ISLNK(entry(root,"link").st_mode));ae_files_rename_close(one);
    witness=openat(root,"link",O_PATH|O_NOFOLLOW|O_CLOEXEC);CHECK(witness>=3);CHECK(!ae_files_rename_prepare(root,witness,"link",4,"renamed-link",12,&r) && r==AE_FILES_RENAME_INVALID);close(witness);
    one=prepare(root,"old.txt","denied");CHECK(!fchmod(root,0777));CHECK(ae_files_rename_apply(one)==AE_FILES_RENAME_DENIED && !fchmod(root,0700));CHECK(ae_files_rename_apply(one)==AE_FILES_RENAME_DENIED && absent(root,"denied"));ae_files_rename_close(one);
    CHECK(!mkdirat(root,"holder",0700));int dir=openat(root,"holder",O_RDONLY|O_DIRECTORY|O_CLOEXEC);CHECK(dir>=3);CHECK(!mkdirat(dir,"child",0700));int child=openat(dir,"child",O_RDONLY|O_DIRECTORY|O_CLOEXEC);CHECK(child>=3);put(child,"payload","retained");close(child);
    one=prepare(dir,"child","renamed-child");inode=entry(dir,"child").st_ino;
    CHECK(!renameat(root,"holder",root,"moved-holder") && !mkdirat(root,"holder",0700));
    CHECK(ae_files_rename_apply(one)==AE_FILES_RENAME_DONE && absent(dir,"child") && entry(dir,"renamed-child").st_ino==inode);
    child=openat(dir,"renamed-child",O_RDONLY|O_DIRECTORY|O_CLOEXEC);CHECK(child>=3 && entry(child,"payload").st_size==8);close(child);
    int replacement=openat(root,"holder",O_RDONLY|O_DIRECTORY|O_CLOEXEC);CHECK(replacement>=3 && absent(replacement,"renamed-child"));close(replacement);close(dir);ae_files_rename_close(one);
    put(root,"bad\nraw","raw");one=prepare(root,"bad\nraw","clean-ä.txt");CHECK(ae_files_rename_apply(one)==AE_FILES_RENAME_DONE && absent(root,"bad\nraw"));ae_files_rename_close(one);
    one=prepare(root,"old.txt","lost-reply");lost=true;CHECK(ae_files_rename_apply(one)==AE_FILES_RENAME_UNKNOWN && absent(root,"old.txt") && entry(root,"lost-reply").st_size==11);lost=false;
    calls=renames;synced=flushes;put(root,"old.txt","new occupant");CHECK(ae_files_rename_apply(one)==AE_FILES_RENAME_UNKNOWN && renames==calls && flushes==synced && entry(root,"old.txt").st_size==12);ae_files_rename_close(one);
    one=prepare(root,"old.txt","lost-flush");lost_flush=true;CHECK(ae_files_rename_apply(one)==AE_FILES_RENAME_UNKNOWN && absent(root,"old.txt") && entry(root,"lost-flush").st_size==12);lost_flush=false;
    calls=renames;synced=flushes;CHECK(ae_files_rename_apply(one)==AE_FILES_RENAME_UNKNOWN && renames==calls && flushes==synced);ae_files_rename_close(one);
    put(root,"race-source","source");one=prepare(root,"race-source","raced-destination");race=true;CHECK(ae_files_rename_apply(one)==AE_FILES_RENAME_UNKNOWN && entry(root,"race-source").st_size==6 && entry(root,"raced-destination").st_size==7);race=false;ae_files_rename_close(one);
    CHECK(!close(root));printf("Files rename evidence: %s\n",path);puts("Files rename: no-replace, held source/parent, conflicts, source replacement, lost replies/flushes and no replay passed");return 0;
}
