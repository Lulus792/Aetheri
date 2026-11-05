#include "editor/file.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/xattr.h>
#include <unistd.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"editor file %d: %s (errno %d)\n",__LINE__,#x,errno);exit(1); } } while (0)
enum fault { NONE, SHORT, INTERRUPT, NO_SPACE, ZERO_WRITE, SYNC_FILE, SYNC_DIRECTORY, SYNC_AFTER,
    RENAME_BEFORE, RENAME_AFTER, CREATE_RACE, CHANGE_RACE, CLOSE_AFTER };
static enum fault fault;
static int allocations=-1,syncs,renames,writes,directory;
static bool oversized_attribute;
static char leaf[64];
void *__real_malloc(size_t size);
void *__wrap_malloc(size_t size) { if (allocations==0) return NULL;if (allocations>0) --allocations;return __real_malloc(size); }
ssize_t __real_fgetxattr(int fd,const char *name,void *value,size_t size);
ssize_t __wrap_fgetxattr(int fd,const char *name,void *value,size_t size) {
    if (oversized_attribute && !strcmp(name,"user.limit") && !size) return 65537;
    return __real_fgetxattr(fd,name,value,size);
}
int __real_close(int fd);
int __wrap_close(int fd) {
    bool lost=fault==CLOSE_AFTER && (fcntl(fd,F_GETFL)&O_ACCMODE)==O_WRONLY;
    int r=__real_close(fd);if (lost) { errno=EIO;return -1; }return r;
}
ssize_t __real_write(int fd,const void *text,size_t size);
ssize_t __wrap_write(int fd,const void *text,size_t size) {
    ++writes;
    if (fault==INTERRUPT && writes==1) { errno=EINTR;return -1; }
    if (fault==NO_SPACE && writes>1) { errno=ENOSPC;return -1; }
    if (fault==ZERO_WRITE) return 0;
    if ((fault==SHORT || fault==NO_SPACE) && size>2) size=2;
    return __real_write(fd,text,size);
}
static void put(const char *name,const char *text,size_t size) {
    int fd=openat(directory,name,O_WRONLY|O_CREAT|O_TRUNC|O_CLOEXEC,0600);CHECK(fd>=0);
    CHECK(__real_write(fd,text,size)==(ssize_t)size);CHECK(close(fd)==0);
}
int __real_fsync(int fd);
int __wrap_fsync(int fd) {
    ++syncs;
    if ((fault==SYNC_FILE && syncs==1) || (fault==SYNC_DIRECTORY && syncs==2) || (fault==SYNC_AFTER && syncs==3)) { errno=EIO;return -1; }
    if (fault==CHANGE_RACE && syncs==2) put(leaf,"external",8);
    return __real_fsync(fd);
}
int __real_renameat2(int olddir,const char *old,int newdir,const char *name,unsigned flags);
int __wrap_renameat2(int olddir,const char *old,int newdir,const char *name,unsigned flags) {
    ++renames;
    if (fault==RENAME_BEFORE) { errno=EIO;return -1; }
    if (fault==CREATE_RACE) put(name,"racer",5);
    int r=__real_renameat2(olddir,old,newdir,name,flags);
    if (fault==RENAME_AFTER && !r) { errno=EIO;return -1; }return r;
}
static void equal_file(const char *name,const char *expected,size_t size) {
    int fd=openat(directory,name,O_RDONLY|O_CLOEXEC|O_NOFOLLOW);CHECK(fd>=0);char bytes[256];
    CHECK(size<sizeof(bytes) && read(fd,bytes,sizeof(bytes))==(ssize_t)size && !memcmp(bytes,expected,size));CHECK(close(fd)==0);
}
static void dirty(struct ae_document *d) { struct ae_document_info info;CHECK(ae_document_info(d,&info) && info.dirty); }
static void clean(struct ae_document *d) { struct ae_document_info info;CHECK(ae_document_info(d,&info) && !info.dirty); }
static void edit(struct ae_document *d) { struct ae_document_info info;CHECK(ae_document_info(d,&info));CHECK(ae_document_replace(d,0,info.size,"changed\r\nlast",13)==AE_DOCUMENT_OK);dirty(d); }
static unsigned sequence;
static void original(struct ae_editor_file **f,struct ae_document **d) {
    CHECK(snprintf(leaf,sizeof(leaf),"file-%u",++sequence)>0);put(leaf,"old\r\nlast",9);
    CHECK(ae_editor_file_open(directory,leaf,f,d)==AE_EDITOR_FILE_OK);clean(*d);
    char bytes[16];CHECK(ae_document_copy(*d,0,9,bytes,sizeof(bytes)) && !memcmp(bytes,"old\r\nlast",9));
}
static void close_pair(struct ae_editor_file *f,struct ae_document *d) { ae_editor_file_close(f);ae_document_destroy(d); }
static size_t descriptors(void) {
    DIR *dir=opendir("/proc/self/fd");CHECK(dir);size_t count=0;struct dirent *entry;
    while ((entry=readdir(dir))) if (strcmp(entry->d_name,".") && strcmp(entry->d_name,"..")) ++count;
    CHECK(closedir(dir)==0);return count;
}
static void successes(void) {
    struct ae_editor_file *f=NULL;struct ae_document *d=NULL;original(&f,&d);edit(d);
    CHECK(ae_editor_file_save(f,d)==AE_EDITOR_FILE_OK);clean(d);equal_file(leaf,"changed\r\nlast",13);
    CHECK(ae_editor_file_save(f,d)==AE_EDITOR_FILE_OK);clean(d);close_pair(f,d);
    for (enum fault mode=SHORT;mode<=INTERRUPT;++mode) {
        f=NULL;d=NULL;original(&f,&d);edit(d);fault=mode;writes=0;
        CHECK(ae_editor_file_save(f,d)==AE_EDITOR_FILE_OK);fault=NONE;clean(d);equal_file(leaf,"changed\r\nlast",13);close_pair(f,d);
    }
    f=NULL;d=NULL;CHECK(ae_editor_file_new(directory,"new",&f,&d)==AE_EDITOR_FILE_OK);clean(d);
    struct stat s;CHECK(fstatat(directory,"new",&s,0)<0 && errno==ENOENT);
    CHECK(ae_editor_file_save(f,d)==AE_EDITOR_FILE_OK);equal_file("new","",0);clean(d);
    CHECK(fstatat(directory,"new",&s,0)==0 && (s.st_mode&0777)==0600);
    edit(d);CHECK(ae_editor_file_save(f,d)==AE_EDITOR_FILE_OK);equal_file("new","changed\r\nlast",13);close_pair(f,d);
    f=NULL;d=NULL;CHECK(ae_editor_file_new(directory,"new",&f,&d)==AE_EDITOR_FILE_CONFLICT && !f && !d);
    put("attrs","alpha\n",6);int fd=openat(directory,"attrs",O_RDONLY|O_CLOEXEC);CHECK(fd>=0);
    CHECK(fchmod(fd,0750)==0 && fsetxattr(fd,"user.aetheri-test","metadata",8,0)==0 && fsetxattr(fd,"user.empty","",0,0)==0);CHECK(close(fd)==0);
    CHECK(ae_editor_file_open(directory,"attrs",&f,&d)==AE_EDITOR_FILE_OK);edit(d);CHECK(ae_editor_file_save(f,d)==AE_EDITOR_FILE_OK);close_pair(f,d);
    fd=openat(directory,"attrs",O_RDONLY|O_CLOEXEC);CHECK(fd>=0);char attr[16];
    CHECK(fgetxattr(fd,"user.aetheri-test",attr,sizeof(attr))==8 && !memcmp(attr,"metadata",8) && fgetxattr(fd,"user.empty",attr,sizeof(attr))==0);
    CHECK(fstat(fd,&s)==0 && (s.st_mode&0777)==0750);CHECK(close(fd)==0);
}
static void large_and_allocation(void) {
    size_t before=descriptors();bool succeeded=false;
    for (int limit=0;limit<24;++limit) {
        struct ae_editor_file *f=NULL;struct ae_document *d=NULL;allocations=limit;
        enum ae_editor_file_result r=ae_editor_file_open(directory,"attrs",&f,&d);allocations=-1;
        if (r==AE_EDITOR_FILE_OK) { close_pair(f,d);succeeded=true;CHECK(descriptors()==before);break; }
        CHECK(r==AE_EDITOR_FILE_NOMEM && !f && !d && descriptors()==before);
    }
    CHECK(succeeded);
    struct ae_editor_file *f=NULL;struct ae_document *d=NULL;CHECK(ae_editor_file_open(directory,"attrs",&f,&d)==AE_EDITOR_FILE_OK);edit(d);
    allocations=1;CHECK(ae_editor_file_save(f,d)==AE_EDITOR_FILE_NOMEM);allocations=-1;
    dirty(d);CHECK(!ae_editor_file_uncertain(f) && !ae_editor_file_recovery(f));equal_file("attrs","changed\r\nlast",13);close_pair(f,d);CHECK(descriptors()==before);
    char *bytes=malloc(AE_DOCUMENT_BYTES);CHECK(bytes);memset(bytes,'t',AE_DOCUMENT_BYTES);
    for (size_t i=65535;i<AE_DOCUMENT_BYTES;i+=65536) bytes[i]='\n';
    put("maximum",bytes,AE_DOCUMENT_BYTES);f=NULL;d=NULL;CHECK(ae_editor_file_open(directory,"maximum",&f,&d)==AE_EDITOR_FILE_OK);
    struct ae_document_info info;CHECK(ae_document_info(d,&info) && info.size==AE_DOCUMENT_BYTES && info.lines==129 && !info.dirty);
    CHECK(ae_document_replace(d,0,1,"x",1)==AE_DOCUMENT_OK);CHECK(ae_editor_file_save(f,d)==AE_EDITOR_FILE_OK);clean(d);close_pair(f,d);
    int fd=openat(directory,"maximum",O_RDONLY|O_CLOEXEC);CHECK(fd>=0 && read(fd,bytes,AE_DOCUMENT_BYTES)==AE_DOCUMENT_BYTES && close(fd)==0);
    CHECK(bytes[0]=='x');bytes[0]='t';for (size_t i=0;i<AE_DOCUMENT_BYTES;++i) CHECK(bytes[i]==(i%65536==65535 ? '\n' : 't'));
    free(bytes);CHECK(descriptors()==before);
}
static void refusals(void) {
    struct ae_editor_file *f=NULL;struct ae_document *d=NULL;
    CHECK(ae_editor_file_open(directory,"../new",&f,&d)==AE_EDITOR_FILE_INVALID && !f && !d);
    CHECK(symlinkat("new",directory,"link")==0 && ae_editor_file_open(directory,"link",&f,&d)==AE_EDITOR_FILE_UNSUPPORTED && !f && !d);
    CHECK(linkat(directory,"new",directory,"hard",0)==0 && ae_editor_file_open(directory,"hard",&f,&d)==AE_EDITOR_FILE_UNSUPPORTED && !f && !d);
    CHECK(unlinkat(directory,"hard",0)==0);
    CHECK(mkfifoat(directory,"fifo",0600)==0 && ae_editor_file_open(directory,"fifo",&f,&d)==AE_EDITOR_FILE_UNSUPPORTED && !f && !d);
    put("invalid","a\033b",3);CHECK(ae_editor_file_open(directory,"invalid",&f,&d)==AE_EDITOR_FILE_UNSUPPORTED && !f && !d);
    int fd=openat(directory,"large",O_RDWR|O_CREAT|O_CLOEXEC,0600);CHECK(fd>=0 && ftruncate(fd,AE_DOCUMENT_BYTES+1)==0 && close(fd)==0);
    CHECK(ae_editor_file_open(directory,"large",&f,&d)==AE_EDITOR_FILE_UNSUPPORTED && !f && !d);
    original(&f,&d);edit(d);put(leaf,"foreign",7);CHECK(ae_editor_file_save(f,d)==AE_EDITOR_FILE_CONFLICT);equal_file(leaf,"foreign",7);dirty(d);close_pair(f,d);
    f=NULL;d=NULL;original(&f,&d);edit(d);CHECK(unlinkat(directory,leaf,0)==0);put(leaf,"old\r\nlast",9);
    CHECK(ae_editor_file_save(f,d)==AE_EDITOR_FILE_CONFLICT);dirty(d);close_pair(f,d);
    f=NULL;d=NULL;original(&f,&d);edit(d);CHECK(fchmodat(directory,leaf,0400,0)==0);
    CHECK(ae_editor_file_save(f,d)==AE_EDITOR_FILE_CONFLICT);close_pair(f,d);
    f=NULL;d=NULL;CHECK(ae_editor_file_open(directory,leaf,&f,&d)==AE_EDITOR_FILE_OK);edit(d);CHECK(ae_editor_file_save(f,d)==AE_EDITOR_FILE_UNSUPPORTED);close_pair(f,d);
    f=NULL;d=NULL;original(&f,&d);edit(d);allocations=0;CHECK(ae_editor_file_save(f,d)==AE_EDITOR_FILE_NOMEM);allocations=-1;
    dirty(d);equal_file(leaf,"old\r\nlast",9);CHECK(!ae_editor_file_recovery(f));close_pair(f,d);
    f=NULL;d=NULL;original(&f,&d);edit(d);CHECK(fchmod(directory,0777)==0);
    CHECK(ae_editor_file_save(f,d)==AE_EDITOR_FILE_UNSUPPORTED);dirty(d);CHECK(fchmod(directory,0700)==0);close_pair(f,d);
    /* The carrier's ext4 cannot store this profile's maximum xattr size.
       Keep real ordinary-xattr round trips above; inject only the size query
       for an actually listed attribute to exercise bounded admission. */
    put("huge-attr","x",1);fd=openat(directory,"huge-attr",O_RDONLY|O_CLOEXEC);CHECK(fd>=0);
    CHECK(fsetxattr(fd,"user.limit","x",1,0)==0 && close(fd)==0);
    f=NULL;d=NULL;oversized_attribute=true;CHECK(ae_editor_file_open(directory,"huge-attr",&f,&d)==AE_EDITOR_FILE_UNSUPPORTED && !f && !d);oversized_attribute=false;
}
static void faults(void) {
    enum fault cases[]={NO_SPACE,ZERO_WRITE,SYNC_FILE,SYNC_DIRECTORY,SYNC_AFTER,RENAME_BEFORE,RENAME_AFTER,CHANGE_RACE,CLOSE_AFTER};
    for (size_t i=0;i<sizeof(cases)/sizeof(*cases);++i) {
        struct ae_editor_file *f=NULL;struct ae_document *d=NULL;original(&f,&d);edit(d);fault=cases[i];writes=syncs=renames=0;
        enum ae_editor_file_result r=ae_editor_file_save(f,d);fault=NONE;
        bool changed=cases[i]==SYNC_AFTER || cases[i]==RENAME_AFTER || cases[i]==CLOSE_AFTER;
        bool unknown=changed || cases[i]==RENAME_BEFORE;
        CHECK(r==(unknown ? AE_EDITOR_FILE_UNKNOWN : cases[i]==CHANGE_RACE ? AE_EDITOR_FILE_CONFLICT : AE_EDITOR_FILE_IO));dirty(d);
        CHECK(ae_editor_file_uncertain(f)==unknown);
        equal_file(leaf,changed ? "changed\r\nlast" : cases[i]==CHANGE_RACE ? "external" : "old\r\nlast",changed ? 13u : cases[i]==CHANGE_RACE ? 8u : 9u);
        const char *recovery=ae_editor_file_recovery(f);
        if (cases[i]==RENAME_BEFORE) { CHECK(recovery);equal_file(recovery,"changed\r\nlast",13); }
        else if (!unknown || cases[i]==SYNC_AFTER) CHECK(!recovery);
        if (unknown) { int count=renames;CHECK(ae_editor_file_save(f,d)==AE_EDITOR_FILE_UNKNOWN && renames==count); }
        close_pair(f,d);
    }
    struct ae_editor_file *f=NULL;struct ae_document *d=NULL;CHECK(ae_editor_file_new(directory,"raced",&f,&d)==AE_EDITOR_FILE_OK);edit(d);
    fault=CREATE_RACE;syncs=renames=0;CHECK(ae_editor_file_save(f,d)==AE_EDITOR_FILE_CONFLICT);fault=NONE;
    equal_file("raced","racer",5);dirty(d);CHECK(!ae_editor_file_uncertain(f) && !ae_editor_file_recovery(f));close_pair(f,d);
    f=NULL;d=NULL;original(&f,&d);fault=SYNC_AFTER;syncs=0;
    CHECK(ae_editor_file_save(f,d)==AE_EDITOR_FILE_UNKNOWN);fault=NONE;
    clean(d);CHECK(ae_editor_file_uncertain(f));close_pair(f,d);
}
int main(void) {
    char path[]="editor-files-XXXXXX";CHECK(mkdtemp(path));directory=open(path,O_RDONLY|O_DIRECTORY|O_CLOEXEC);CHECK(directory>=0 && geteuid()!=0);
    size_t before=descriptors();successes();large_and_allocation();refusals();faults();CHECK(descriptors()==before);
    CHECK(close(directory)==0);printf("Editor file evidence: %s\n",path);return 0;
}
