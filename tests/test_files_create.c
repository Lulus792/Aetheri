#define _GNU_SOURCE
#include "files/create.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"Files create %d: %s errno=%d\n",__LINE__,#x,errno);exit(1); } } while (0)
int __real_fsync(int fd);int __real_mkdirat(int dir,const char *name,mode_t mode);void *__real_calloc(size_t n,size_t size);
static unsigned flushes,mkdirs,fail_flush;static bool lost_mkdir,deny;
int __wrap_fsync(int fd) { int r=__real_fsync(fd);++flushes;if (!r && fail_flush==flushes) { errno=EIO;return -1; }return r; }
int __wrap_mkdirat(int dir,const char *name,mode_t mode) { ++mkdirs;int r=__real_mkdirat(dir,name,mode);if (!r && lost_mkdir) { errno=EIO;return -1; }return r; }
void *__wrap_calloc(size_t n,size_t size) { return deny ? NULL : __real_calloc(n,size); }
static struct ae_files_create *prepare(int dir,enum ae_files_create_kind kind,const char *name) {
    enum ae_files_create_result r;struct ae_files_create *op=ae_files_create_prepare(dir,kind,name,strlen(name),&r);
    CHECK(op && r==AE_FILES_CREATE_PREPARED && ae_files_create_status(op)==r && !strcmp(ae_files_create_leaf(op),name));return op;
}
static struct stat entry(int dir,const char *name) { struct stat st;CHECK(!fstatat(dir,name,&st,AT_SYMLINK_NOFOLLOW));return st; }
int main(void) {
    CHECK(getuid()!=0);char path[]="files-create-XXXXXX";CHECK(mkdtemp(path) && !chmod(path,0700));int root=open(path,O_RDONLY|O_DIRECTORY|O_CLOEXEC);CHECK(root>=3);
    const char *bad[]={"",".","..","x/y","bad\nname","\xff","a\xe2\x80\xa8"};
    for (size_t i=0;i<sizeof(bad)/sizeof(*bad);++i) CHECK(!ae_files_create_name(bad[i],strlen(bad[i])));
    CHECK(ae_files_create_name("notes ä ' $(literal)",strlen("notes ä ' $(literal)")));
    char huge[256];memset(huge,'a',sizeof(huge));CHECK(!ae_files_create_name(huge,sizeof(huge)));
    enum ae_files_create_result r;deny=true;CHECK(!ae_files_create_prepare(root,AE_FILES_CREATE_FILE,"oom",3,&r) && r==AE_FILES_CREATE_NOMEM);deny=false;
    CHECK(fstatat(root,"oom",&(struct stat){0},0)<0 && errno==ENOENT && !flushes && !mkdirs);
    struct ae_files_create *one=prepare(root,AE_FILES_CREATE_FILE,"new.txt"),*two=prepare(root,AE_FILES_CREATE_FILE,"new.txt");
    CHECK(ae_files_create_apply(one)==AE_FILES_CREATE_DONE);struct stat st=entry(root,"new.txt");CHECK(S_ISREG(st.st_mode) && st.st_size==0 && (st.st_mode&0777)==0600);
    unsigned before=flushes;CHECK(ae_files_create_apply(one)==AE_FILES_CREATE_DONE && flushes==before);
    CHECK(ae_files_create_apply(two)==AE_FILES_CREATE_CONFLICT && flushes==before && entry(root,"new.txt").st_ino==st.st_ino);
    ae_files_create_close(one);ae_files_create_close(two);
    CHECK(!symlinkat("missing",root,"link"));one=prepare(root,AE_FILES_CREATE_FILE,"link");CHECK(ae_files_create_apply(one)==AE_FILES_CREATE_CONFLICT && S_ISLNK(entry(root,"link").st_mode));ae_files_create_close(one);
    one=prepare(root,AE_FILES_CREATE_DIRECTORY,"folder");CHECK(ae_files_create_apply(one)==AE_FILES_CREATE_DONE);st=entry(root,"folder");CHECK(S_ISDIR(st.st_mode) && (st.st_mode&0777)==0700);
    unsigned made=mkdirs;before=flushes;CHECK(ae_files_create_apply(one)==AE_FILES_CREATE_DONE && mkdirs==made && flushes==before);ae_files_create_close(one);
    one=prepare(root,AE_FILES_CREATE_DIRECTORY,"link");CHECK(ae_files_create_apply(one)==AE_FILES_CREATE_CONFLICT && S_ISLNK(entry(root,"link").st_mode));ae_files_create_close(one);
    int directory=openat(root,"folder",O_RDONLY|O_DIRECTORY|O_CLOEXEC);CHECK(directory>=3);
    one=prepare(directory,AE_FILES_CREATE_FILE,"held.txt");CHECK(!renameat(root,"folder",root,"moved") && !mkdirat(root,"folder",0700));
    CHECK(ae_files_create_apply(one)==AE_FILES_CREATE_DONE && entry(directory,"held.txt").st_size==0);int replacement=openat(root,"folder",O_RDONLY|O_DIRECTORY|O_CLOEXEC);CHECK(replacement>=3 && fstatat(replacement,"held.txt",&st,0)<0 && errno==ENOENT);close(replacement);close(directory);ae_files_create_close(one);
    one=prepare(root,AE_FILES_CREATE_FILE,"denied");CHECK(!fchmod(root,0777));CHECK(ae_files_create_apply(one)==AE_FILES_CREATE_DENIED && !fchmod(root,0700));CHECK(ae_files_create_apply(one)==AE_FILES_CREATE_DENIED);ae_files_create_close(one);
    one=prepare(root,AE_FILES_CREATE_DIRECTORY,"folder-unknown");fail_flush=flushes+1;CHECK(ae_files_create_apply(one)==AE_FILES_CREATE_UNKNOWN && S_ISDIR(entry(root,"folder-unknown").st_mode));before=flushes;made=mkdirs;fail_flush=0;CHECK(ae_files_create_apply(one)==AE_FILES_CREATE_UNKNOWN && flushes==before && mkdirs==made);ae_files_create_close(one);
    one=prepare(root,AE_FILES_CREATE_FILE,"file-unknown");fail_flush=flushes+3;CHECK(ae_files_create_apply(one)==AE_FILES_CREATE_UNKNOWN && S_ISREG(entry(root,"file-unknown").st_mode));before=flushes;fail_flush=0;CHECK(ae_files_create_apply(one)==AE_FILES_CREATE_UNKNOWN && flushes==before);ae_files_create_close(one);
    one=prepare(root,AE_FILES_CREATE_DIRECTORY,"mkdir-unknown");lost_mkdir=true;CHECK(ae_files_create_apply(one)==AE_FILES_CREATE_UNKNOWN && S_ISDIR(entry(root,"mkdir-unknown").st_mode));made=mkdirs;lost_mkdir=false;CHECK(ae_files_create_apply(one)==AE_FILES_CREATE_UNKNOWN && mkdirs==made);ae_files_create_close(one);
    CHECK(!close(root));printf("Files create evidence: %s\n",path);puts("Files create: create-only conflicts, held target, private modes, before-effect failure and unreplayed lost mkdir/flush acknowledgements passed");return 0;
}
