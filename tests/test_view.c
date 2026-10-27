#include "linux/view.h"

#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <poll.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/sysmacros.h>
#include <sys/wait.h>
#include <unistd.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"view line %d: %s (errno=%d)\n",__LINE__,#x,errno); exit(1); } } while (0)
static int tree_fault,attributes_fault,move_fault,unshare_fault,private_fault;
int __real_open_tree(int,const char *,unsigned int);
int __wrap_open_tree(int fd,const char *path,unsigned int flags)
{
    if (tree_fault) { tree_fault=0; errno=EIO; return -1; }
    return __real_open_tree(fd,path,flags);
}
int __real_mount_setattr(int,const char *,unsigned int,struct mount_attr *,size_t);
int __wrap_mount_setattr(int fd,const char *path,unsigned int flags,struct mount_attr *attributes,size_t size)
{
    if (attributes_fault) { attributes_fault=0; errno=EIO; return -1; }
    return __real_mount_setattr(fd,path,flags,attributes,size);
}
int __real_move_mount(int,const char *,int,const char *,unsigned int);
int __wrap_move_mount(int source,const char *from,int target,const char *to,unsigned int flags)
{
    if (move_fault) {
        int mode=move_fault; move_fault=0;
        if (mode==2) CHECK(__real_move_mount(source,from,target,to,flags)==0);
        errno=EIO; return -1;
    }
    return __real_move_mount(source,from,target,to,flags);
}
int __real_unshare(int);
int __wrap_unshare(int flags)
{
    if (unshare_fault) { unshare_fault=0; errno=EPERM; return -1; }
    return __real_unshare(flags);
}
int __real_mount(const char *,const char *,const char *,unsigned long,const void *);
int __wrap_mount(const char *source,const char *target,const char *type,unsigned long flags,const void *data)
{
    if (private_fault) { private_fault=0; errno=EPERM; return -1; }
    return __real_mount(source,target,type,flags,data);
}
static int directory(int parent,const char *name)
{
    CHECK(mkdirat(parent,name,0700)==0);
    int fd=openat(parent,name,O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW);
    CHECK(fd>=0); return fd;
}
static void content(int parent,const char *name,const char *value)
{
    int fd=openat(parent,name,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC,0600);
    CHECK(fd>=0 && write(fd,value,strlen(value))==(ssize_t)strlen(value) && close(fd)==0);
}
static void executable(int parent)
{
    int source=open("/proc/self/exe",O_RDONLY|O_CLOEXEC);
    int destination=openat(parent,"executable",O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC,0700);
    CHECK(source>=0 && destination>=0);
    char bytes[4096]; ssize_t size;
    while ((size=read(source,bytes,sizeof bytes))>0) {
        ssize_t offset=0;
        while (offset<size) { ssize_t written=write(destination,bytes+offset,(size_t)(size-offset)); CHECK(written>0); offset+=written; }
    }
    CHECK(size==0 && close(source)==0 && close(destination)==0);
}
static int same_namespace(const struct stat *expected)
{
    struct stat actual; CHECK(stat("/proc/self/ns/mnt",&actual)==0);
    return expected->st_ino==actual.st_ino && expected->st_dev==actual.st_dev;
}
static void remove_directory(int parent,int fd,const char *name)
{
    CHECK(close(fd)==0 && unlinkat(parent,name,AT_REMOVEDIR)==0);
}
static size_t descriptors(void)
{
    DIR *directory=opendir("/proc/self/fd"); CHECK(directory!=NULL);
    size_t count=0; struct dirent *entry;
    while ((entry=readdir(directory))!=NULL) if (strcmp(entry->d_name,".")!=0 && strcmp(entry->d_name,"..")!=0) ++count;
    CHECK(closedir(directory)==0); return count;
}
int main(int argc,char **argv)
{
    CHECK(argc==3 && geteuid()==0);
    FILE *machine=fopen("/sys/class/dmi/id/product_name","r"); char product[80];
    CHECK(machine!=NULL && fgets(product,sizeof product,machine)!=NULL && fclose(machine)==0 && strcmp(product,"Aetheri P0 disposable VM\n")==0);
    struct stat original; CHECK(stat("/proc/self/ns/mnt",&original)==0);
    struct ae_view_namespace *space=NULL;
    if (strcmp(argv[2],"unshare-fault")==0 || strcmp(argv[2],"private-fault")==0) {
        unshare_fault=strcmp(argv[2],"unshare-fault")==0; private_fault=!unshare_fault;
        struct ae_view_namespace *marker=(void *)&original; space=marker;
        CHECK(ae_view_namespace_enter(&space)==-EPERM && space==marker);
        CHECK(same_namespace(&original)==(strcmp(argv[2],"unshare-fault")==0));
        return 0; /* No reuse after partial namespace setup. */
    }
    CHECK(ae_view_namespace_enter(&space)==0 && !same_namespace(&original));
    int root=open(argv[1],O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW); CHECK(root>=0);
    /* Root-owned setup slots are private to this dedicated test. */
    int setup=directory(root,"setup"),code=directory(root,"code"),data=directory(root,"source-data");
    int app=directory(setup,"app"),destination=directory(setup,"data");
    int nested=directory(code,"nested");
    content(code,"file","code"); content(data,"file","data"); content(nested,"underlay","owned");
    executable(data);
    CHECK(mknodat(data,"null",S_IFCHR|0600,makedev(1,3))==0);
    CHECK(symlinkat("file",code,"link")==0);
    CHECK(fchmod(code,0555)==0);
    struct ae_root_identity ci,di,ai,ti;
    CHECK(ae_root_observe(code,&ci)==0 && ae_root_observe(data,&di)==0 && ae_root_observe(app,&ai)==0 && ae_root_observe(destination,&ti)==0);
    struct ae_view *cv=NULL,*dv=NULL;
    struct ae_root_identity changed=ci; changed.mount_id++;
    CHECK(ae_view_clone(space,code,&changed,AE_VIEW_CODE,&cv)==-ESTALE && cv==NULL);
    if (strcmp(argv[2],"faults")==0) {
        size_t before=descriptors();
        tree_fault=1; CHECK(ae_view_clone(space,code,&ci,AE_VIEW_CODE,&cv)==-EIO && cv==NULL);
        attributes_fault=1; CHECK(ae_view_clone(space,code,&ci,AE_VIEW_CODE,&cv)==-EIO && cv==NULL);
        CHECK(descriptors()==before);
        for (int n=0;n<32;++n) CHECK(ae_view_clone(space,code,&ci,AE_VIEW_CODE,&cv)==0 && ae_view_close(&cv)==0);
        CHECK(descriptors()==before);
    }
    /* A nested foreign mount is omitted; only the source mount's own content is
     * cloned. No recursive bind can import a different owner's mounted tree. */
    char nested_path[4096]; CHECK(snprintf(nested_path,sizeof nested_path,"%s/code/nested",argv[1])>0);
    CHECK(mount("tmpfs",nested_path,"tmpfs",MS_NODEV|MS_NOSUID,"size=65536,mode=0700")==0);
    int foreign=openat(code,"nested",O_RDONLY|O_DIRECTORY|O_CLOEXEC); CHECK(foreign>=0);
    content(foreign,"foreign","foreign");
    CHECK(ae_view_clone(space,code,&ci,AE_VIEW_CODE,&cv)==0 && ae_view_clone(space,data,&di,AE_VIEW_DATA,&dv)==0);
    CHECK(ae_view_namespace_close(&space)==-EBUSY);
    enum ae_view_phase phase=0; struct ae_root_identity observed;
    CHECK(ae_view_check(cv,&phase,&observed)==0 && phase==AE_VIEW_DETACHED && observed.mount_id!=ci.mount_id);
    if (strcmp(argv[2],"fork")==0) {
        pid_t child=fork(); CHECK(child>=0);
        if (child==0) {
            CHECK(ae_view_check(cv,&phase,&observed)==-ECHILD && ae_view_close(&cv)==-ECHILD && ae_view_namespace_close(&space)==-ECHILD);
            _exit(0);
        }
        int status; CHECK(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==0);
    }
    if (strcmp(argv[2],"namespace")==0) {
        int saved=open("/proc/self/ns/mnt",O_RDONLY|O_CLOEXEC); CHECK(saved>=0 && unshare(CLONE_NEWNS)==0);
        CHECK(ae_view_check(cv,&phase,&observed)==-ESTALE);
        CHECK(setns(saved,CLONE_NEWNS)==0 && close(saved)==0);
    }
    content(app,"unexpected","preserve");
    CHECK(ae_view_attach(cv,setup,&ai,"app")==-ENOTEMPTY && unlinkat(app,"unexpected",0)==0);
    CHECK(ae_view_attach(cv,setup,&ai,"../app")==-EINVAL);
    char app_path[4096]; CHECK(snprintf(app_path,sizeof app_path,"%s/setup/app",argv[1])>0);
    CHECK(mount(app_path,app_path,NULL,MS_BIND,NULL)==0);
    CHECK(ae_view_attach(cv,setup,&ai,"app")==-EXDEV && umount(app_path)==0);
    CHECK(renameat(setup,"app",setup,"old-app")==0);
    int replacement=directory(setup,"app");
    CHECK(ae_view_attach(cv,setup,&ai,"app")==-ESTALE);
    remove_directory(setup,replacement,"app");
    CHECK(renameat(setup,"old-app",setup,"app")==0);
    if (strcmp(argv[2],"faults")==0) {
        move_fault=1; CHECK(ae_view_attach(cv,setup,&ai,"app")==-EIO);
        CHECK(ae_view_check(cv,&phase,&observed)==0 && phase==AE_VIEW_UNCERTAIN);
        CHECK(ae_view_attach(cv,setup,&ai,"app")==-EALREADY && ae_view_close(&cv)==0);
        CHECK(ae_view_clone(space,code,&ci,AE_VIEW_CODE,&cv)==0);
        move_fault=2; CHECK(ae_view_attach(cv,setup,&ai,"app")==-EIO);
        CHECK(ae_view_check(cv,&phase,&observed)==0 && phase==AE_VIEW_UNCERTAIN && ae_view_attach(cv,setup,&ai,"app")==-EALREADY);
    } else CHECK(ae_view_attach(cv,setup,&ai,"app")==0);
    CHECK(ae_view_attach(dv,setup,&ti,"data")==0);
    int visible=openat(setup,"app",O_RDONLY|O_DIRECTORY|O_CLOEXEC); CHECK(visible>=0);
    int fd=openat(visible,"file",O_WRONLY|O_CLOEXEC); CHECK(fd<0 && errno==EROFS);
    fd=openat(visible,"link",O_RDONLY|O_CLOEXEC); CHECK(fd<0 && errno==ELOOP);
    fd=openat(visible,"nested/foreign",O_RDONLY|O_CLOEXEC); CHECK(fd<0 && errno==ENOENT);
    fd=openat(visible,"nested/underlay",O_RDONLY|O_CLOEXEC); CHECK(fd>=0 && close(fd)==0);
    int vd=openat(setup,"data",O_RDONLY|O_DIRECTORY|O_CLOEXEC); CHECK(vd>=0);
    fd=openat(vd,"null",O_RDONLY|O_CLOEXEC); CHECK(fd<0 && errno==EACCES);
    struct ae_root_identity alias; struct ae_view *rejected=NULL;
    CHECK(ae_root_observe(visible,&alias)==0 && ae_view_clone(space,visible,&alias,AE_VIEW_DATA,&rejected)==-EROFS && rejected==NULL);
    CHECK(ae_root_observe(vd,&alias)==0 && ae_view_clone(space,vd,&alias,AE_VIEW_CODE,&rejected)==-EACCES && rejected==NULL);
    int image=openat(vd,"executable",O_RDONLY|O_CLOEXEC); CHECK(image>=0);
    void *mapping=mmap(NULL,4096,PROT_READ|PROT_EXEC,MAP_PRIVATE,image,0);
    CHECK(mapping==MAP_FAILED && errno==EPERM);
    pid_t child=fork(); CHECK(child>=0);
    if (child==0) {
        char *arguments[]={"must-not-execute",NULL}; char *environment[]={NULL};
        execveat(image,"",arguments,environment,AT_EMPTY_PATH);
        _exit(errno==EACCES ? 0 : 1);
    }
    int status; CHECK(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==0 && close(image)==0);
    content(vd,"created","own-data"); CHECK(unlinkat(vd,"created",0)==0);
    struct statvfs flags; CHECK(fstatvfs(vd,&flags)==0 && (flags.f_flag&(ST_NOEXEC|ST_NODEV|ST_NOSUID))==(ST_NOEXEC|ST_NODEV|ST_NOSUID));
    CHECK(fstatvfs(code,&flags)==0 && !(flags.f_flag&ST_RDONLY));
    enum ae_view_phase sentinel=AE_VIEW_UNCERTAIN; struct ae_root_identity marker; memset(&marker,0xa5,sizeof marker);
    phase=sentinel; observed=marker;
    CHECK(fchmod(code,0755)==0 && ae_view_check(cv,&phase,&observed)==-ESTALE && phase==sentinel && memcmp(&observed,&marker,sizeof marker)==0 && fchmod(code,0555)==0);
    puts("views-attached"); CHECK(fflush(stdout)==0);
    struct pollfd input={.fd=STDIN_FILENO,.events=POLLIN}; char reply[16]={0};
    CHECK(poll(&input,1,5000)==1 && read(STDIN_FILENO,reply,sizeof reply)==9 && memcmp(reply,"continue\n",9)==0);
    CHECK(ae_view_close(&cv)==0 && ae_view_close(&dv)==0 && ae_view_namespace_close(&space)==0);
    /* Closing objects does not unmount attached views: demonstrate the remaining
     * namespace reference explicitly, then unmount only these known test slots. */
    fd=openat(visible,"file",O_WRONLY|O_CLOEXEC); CHECK(fd<0 && errno==EROFS);
    CHECK(close(visible)==0 && close(vd)==0 && close(foreign)==0);
    char path[4096]; CHECK(snprintf(path,sizeof path,"%s/setup/app",argv[1])>0 && umount(path)==0);
    CHECK(snprintf(path,sizeof path,"%s/setup/data",argv[1])>0 && umount(path)==0 && umount(nested_path)==0);
    CHECK(unlinkat(code,"file",0)==0 && unlinkat(code,"link",0)==0 && unlinkat(data,"file",0)==0 && unlinkat(data,"executable",0)==0 && unlinkat(data,"null",0)==0 && unlinkat(nested,"underlay",0)==0);
    remove_directory(code,nested,"nested"); remove_directory(setup,app,"app"); remove_directory(setup,destination,"data");
    remove_directory(root,code,"code"); remove_directory(root,data,"source-data"); remove_directory(root,setup,"setup");
    CHECK(close(root)==0);
    puts("view: actual detached/attached mounts, unchanged source, no child-mount import, lifecycle and fault guards passed");
    return 0;
}
