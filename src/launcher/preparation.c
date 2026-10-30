#include "launcher/preparation.h"
#include "linux/view.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/capability.h>
#include <linux/openat2.h>
#include <selinux/selinux.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <systemd/sd-id128.h>
#include <unistd.h>

#define ROOT "/var/lib/aetheri-p0"
#define LABEL(type) "system_u:object_r:" type ":s0"
enum { SCAFFOLD_BYTES=184 };
struct ae_starter_views {
    pid_t owner;
    struct ae_view_namespace *space;
    struct ae_view *code,*data;
    int root,image_fd,data_fd;
    struct ae_root_identity root_id;
    struct ae_starter_target target;
};
static int label(int fd,const char *expected)
{
    char *actual=NULL; if (fgetfilecon_raw(fd,&actual)<0) return -errno;
    int r=strcmp(actual,expected)==0 ? 0 : -EACCES; freecon(actual); return r;
}
static int opened(int base,const char *path,int directory,mode_t mode,const char *context,int cross)
{
    struct open_how how={.flags=(uint64_t)(O_RDONLY|O_CLOEXEC|O_NOFOLLOW|(directory ? O_DIRECTORY : O_NONBLOCK)),
        .resolve=RESOLVE_NO_SYMLINKS|RESOLVE_NO_MAGICLINKS};
    if (base!=AT_FDCWD) how.resolve|=RESOLVE_BENEATH|(cross ? 0U : RESOLVE_NO_XDEV);
    int fd; do { fd=(int)syscall(SYS_openat2,base,path,&how,sizeof how); } while (fd<0 && errno==EINTR);
    if (fd<0) return -errno;
    struct stat s; int r=fstat(fd,&s)==0 ? 0 : -errno;
    if (r==0 && ((directory ? !S_ISDIR(s.st_mode) : !S_ISREG(s.st_mode)) || s.st_uid!=0 || s.st_gid!=0 ||
        (s.st_mode&07777)!=mode || (!directory && s.st_nlink!=1))) r=-EACCES;
    if (r==0) r=label(fd,context);
    if (r!=0) { close(fd); return r; } return fd;
}
int ae_starter_context(void)
{
    if (getuid()!=0 || geteuid()!=0 || getgid()!=0 || getegid()!=0 || security_getenforce()!=1 ||
        prctl(PR_GET_NO_NEW_PRIVS,0L,0L,0L,0L)!=1) return -EACCES;
    DIR *tasks=opendir("/proc/self/task"); if (tasks==NULL) return -errno;
    size_t count=0; int task_error=0; struct dirent *entry;
    for (;;) { errno=0; entry=readdir(tasks); if (entry==NULL) { task_error=errno ? -errno : 0; break; }
        if (strcmp(entry->d_name,".") && strcmp(entry->d_name,"..")) ++count; }
    if (closedir(tasks)!=0 && task_error==0) task_error=-errno;
    if (task_error!=0 || count!=1) return task_error!=0 ? task_error : -EBUSY;
    char *current=NULL; if (getcon_raw(&current)<0) return -errno;
    int r=strcmp(current,"system_u:system_r:aetheri_starter_t:s0")==0 ? 0 : -EACCES; freecon(current);
    struct __user_cap_header_struct h={.version=_LINUX_CAPABILITY_VERSION_3}; struct __user_cap_data_struct caps[2]={{0}};
    uint32_t required=(UINT32_C(1)<<CAP_SYS_ADMIN)|(UINT32_C(1)<<CAP_DAC_READ_SEARCH)|
        (UINT32_C(1)<<CAP_SETUID)|(UINT32_C(1)<<CAP_SETGID)|(UINT32_C(1)<<CAP_SETPCAP);
    if (r==0 && syscall(SYS_capget,&h,caps)!=0) r=-errno;
    if (r==0 && (caps[0].effective!=required || caps[0].permitted!=required || (caps[0].inheritable&~required) ||
        caps[1].effective || caps[1].permitted || caps[1].inheritable)) r=-EACCES;
    if (r==0) { caps[0].inheritable=0; if (syscall(SYS_capset,&h,caps)!=0) r=-errno; }
    if (r==0 && prctl(PR_CAP_AMBIENT,(long)PR_CAP_AMBIENT_CLEAR_ALL,0L,0L,0L)!=0) r=-errno;
    if (r==0 && syscall(SYS_capget,&h,caps)!=0) r=-errno;
    if (r==0 && (caps[0].effective!=required || caps[0].permitted!=required || caps[0].inheritable ||
        caps[1].effective || caps[1].permitted || caps[1].inheritable)) r=-EACCES;
    return r;
}
int ae_starter_self(const struct ae_execution_profile *p)
{
    int protected=-1,self=-1; struct ae_image expected,actual;
    int r=ae_execution_starter_open(p,&protected,&expected);
    if (r==0) { self=open("/proc/self/exe",O_RDONLY|O_CLOEXEC); if (self<0) r=-errno; }
    if (r==0) r=ae_image_verify(self,p->starter_digest,&actual);
    if (r==0 && !ae_image_equal(&expected,&actual)) r=-ESTALE;
    if (self>=0) close(self);
    if (protected>=0) close(protected);
    return r;
}
static uint64_t number(const uint8_t *p,size_t size)
{ uint64_t n=0; for (size_t m=0;m<size;++m) n=(n<<8)|p[m]; return n; }
static int witnessed(int fd,const uint8_t *bytes)
{
    uint64_t type=number(bytes,8),inode=number(bytes+16,8),seconds=number(bytes+24,8),ns=number(bytes+32,4);
    if (type>INT64_MAX || inode==0 || inode>INT64_MAX || seconds>INT64_MAX || ns>=1000000000) return -EBADMSG;
    struct ae_root_identity actual; int r=ae_root_observe(fd,&actual);
    if (r==0 && (!actual.birth_present || (uint64_t)actual.filesystem_type!=type || memcmp(actual.filesystem_id,bytes+8,8)!=0 ||
        actual.inode!=inode || (uint64_t)actual.birth_seconds!=seconds || actual.birth_nanoseconds!=ns)) r=-ESTALE;
    return r;
}
static int scaffold(const struct ae_execution_profile *p,int *output,int *app_output,int *data_output)
{
    int anchor=opened(AT_FDCWD,ROOT,1,0755,LABEL("aetheri_root_t"),0); if (anchor<0) return anchor;
    int registry=opened(anchor,"registry",1,0755,LABEL("aetheri_registry_t"),0),record=-1,parent=-1,root=-1,app=-1,data=-1;
    int r=registry<0 ? registry : 0; uint8_t bytes[SCAFFOLD_BYTES],digest[32]; struct stat s;
    if (r==0) { record=opened(registry,"execution-views.conf",0,0444,LABEL("aetheri_registry_t"),0); if (record<0) r=record; }
    if (r==0 && fstat(record,&s)!=0) r=-errno;
    if (r==0 && s.st_size!=SCAFFOLD_BYTES) r=-EBADMSG;
    size_t used=0;
    while (r==0 && used<sizeof bytes) {
        ssize_t n=pread(record,bytes+used,sizeof bytes-used,(off_t)used);
        if (n<0) { if (errno!=EINTR) r=-errno; }
        else if (n==0) r=-EBADMSG; else used+=(size_t)n;
    }
    if (r==0 && memcmp(bytes,"AETHVWS1",8)!=0) r=-EBADMSG;
    if (r==0) r=ae_execution_digest(p,digest);
    if (r==0 && memcmp(bytes+8,digest,32)!=0) r=-ESTALE;
    if (r==0) { parent=opened(anchor,"launches",1,0700,LABEL("aetheri_setup_root_t"),0); r=parent<0 ? parent : witnessed(parent,bytes+40); }
    sd_id128_t id; char launch[33]; memcpy(id.bytes,p->launch,16); sd_id128_to_string(id,launch);
    if (r==0) { root=opened(parent,launch,1,0700,LABEL("aetheri_setup_root_t"),0); r=root<0 ? root : witnessed(root,bytes+76); }
    if (r==0) { app=opened(root,"app",1,0700,LABEL("aetheri_setup_root_t"),0); r=app<0 ? app : witnessed(app,bytes+112); }
    if (r==0) { data=opened(root,"data",1,0700,LABEL("aetheri_setup_root_t"),0); r=data<0 ? data : witnessed(data,bytes+148); }
    int fds[]={anchor,registry,record,parent};
    for (size_t n=0;n<sizeof fds/sizeof fds[0];++n) if (fds[n]>=0) close(fds[n]);
    if (r==0) { *output=root; *app_output=app; *data_output=data; }
    else { int failed[]={root,app,data}; for (size_t n=0;n<3;++n) if (failed[n]>=0) close(failed[n]); }
    return r;
}
int ae_starter_views_target(struct ae_starter_views *v,struct ae_starter_target *output)
{
    if (v==NULL || output==NULL) return -EINVAL;
    if (v->owner!=getpid()) return -ECHILD;
    struct ae_root_identity root,code,data,named; enum ae_view_phase phase;
    int r=ae_root_observe(v->root,&root);
    if (r==0 && !ae_root_equal(&root,&v->root_id)) r=-ESTALE;
    if (r==0) r=ae_view_check(v->code,&phase,&code);
    if (r==0 && phase!=AE_VIEW_ATTACHED) r=-ESTALE;
    if (r==0) r=ae_view_check(v->data,&phase,&data);
    if (r==0 && phase!=AE_VIEW_ATTACHED) r=-ESTALE;
    int names[2]={-1,-1}; const char *slots[]={"app","data"};
    for (size_t n=0;r==0 && n<2;++n) {
        struct open_how how={.flags=O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC,
            .resolve=RESOLVE_BENEATH|RESOLVE_NO_SYMLINKS|RESOLVE_NO_MAGICLINKS};
        names[n]=(int)syscall(SYS_openat2,v->root,slots[n],&how,sizeof how);
        r=names[n]<0 ? -errno : ae_root_observe(names[n],&named);
        if (r==0 && !ae_root_equal(&named,n==0 ? &code : &data)) r=-ESTALE;
    }
    if (r==0) r=ae_root_observe(v->data_fd,&named);
    if (r==0 && !ae_root_equal(&named,&data)) r=-ESTALE;
    if (r==0) r=label(v->data_fd,"system_u:object_r:aetheri_app_data_t:s0:c35,c36");
    struct ae_image image;
    if (r==0) r=ae_image_verify(v->image_fd,v->target.command.sha256,&image);
    if (r==0 && !ae_image_equal(&image,&v->target.image)) r=-ESTALE;
    for (size_t n=0;n<2;++n) if (names[n]>=0) close(names[n]);
    if (r==0) *output=v->target;
    return r;
}
int ae_starter_views_close(struct ae_starter_views **output)
{
    if (output==NULL) return -EINVAL;
    struct ae_starter_views *v=*output; if (v==NULL) return 0;
    if (v->owner!=getpid()) return -ECHILD;
    int r=ae_view_close(&v->code),next=ae_view_close(&v->data); if (r==0) r=next;
    next=ae_view_namespace_close(&v->space); if (r==0) r=next;
    int fds[]={v->root,v->image_fd,v->data_fd};
    for (size_t n=0;n<3;++n) if (fds[n]>=0 && close(fds[n])!=0 && r==0) r=-errno;
    free(v); *output=NULL; return r;
}
int ae_starter_views_open(const struct ae_execution_profile *p,struct ae_starter_views **output)
{
    if (p==NULL || output==NULL || *output!=NULL) return -EINVAL;
    uint8_t digest[32]; int r=ae_execution_digest(p,digest); if (r!=0) return r;
    struct ae_starter_views *v=calloc(1,sizeof *v); if (v==NULL) return -ENOMEM;
    v->owner=getpid(); v->root=-1; v->image_fd=-1; v->data_fd=-1;
    struct ae_execution_sources sources=AE_EXECUTION_SOURCES_INIT;
    int a=-1,d=-1;
    r=ae_view_namespace_enter(&v->space);
    if (r==0) r=ae_execution_sources_open(p,&sources);
    if (r==0) r=scaffold(p,&v->root,&a,&d);
    struct ae_root_identity code,data,app,slot;
    if (r==0) r=ae_root_observe(sources.code_fd,&code);
    if (r==0) r=ae_root_observe(sources.data_fd,&data);
    if (r==0) r=ae_root_observe(v->root,&v->root_id);
    if (r==0) r=ae_root_observe(a,&app);
    if (r==0) r=ae_root_observe(d,&slot);
    if (r==0) r=ae_view_clone(v->space,sources.code_fd,&code,AE_VIEW_CODE,&v->code);
    if (r==0) r=ae_view_clone(v->space,sources.data_fd,&data,AE_VIEW_DATA,&v->data);
    if (r==0) r=ae_view_attach(v->code,v->root,&app,"app");
    if (r==0) r=ae_view_attach(v->data,v->root,&slot,"data");
    int mounted=-1;
    if (r==0) {
        struct open_how how={.flags=O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW,.resolve=RESOLVE_BENEATH|RESOLVE_NO_SYMLINKS|RESOLVE_NO_MAGICLINKS};
        mounted=(int)syscall(SYS_openat2,v->root,"app",&how,sizeof how); if (mounted<0) r=-errno;
        if (r==0) { v->data_fd=(int)syscall(SYS_openat2,v->root,"data",&how,sizeof how); if (v->data_fd<0) r=-errno; }
        how.flags=O_RDONLY|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK; how.resolve|=RESOLVE_NO_XDEV;
        if (r==0) { v->image_fd=(int)syscall(SYS_openat2,mounted,sources.command.executable,&how,sizeof how); if (v->image_fd<0) r=-errno; }
    }
    if (r==0) { v->target=(struct ae_starter_target){.image_fd=v->image_fd,.data_fd=v->data_fd,.image=sources.image,.command=sources.command};
        struct ae_starter_target checked; r=ae_starter_views_target(v,&checked); }
    int fds[]={a,d,mounted}; for (size_t n=0;n<3;++n) if (fds[n]>=0) close(fds[n]);
    ae_execution_sources_clear(&sources);
    if (r==0) *output=v; else (void)ae_starter_views_close(&v);
    return r;
}
