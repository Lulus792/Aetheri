#include "linux/lifetime.h"
#include "core/root.h"
#include <errno.h>
#include <fcntl.h>
#include <linux/magic.h>
#include <linux/openat2.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/syscall.h>
#include <unistd.h>

#define CGROOT "/sys/fs/cgroup"
#define MANAGER "org.freedesktop.systemd1"
#define UNIT "org.freedesktop.systemd1.Unit"
#define SERVICE "org.freedesktop.systemd1.Service"
struct ae_lifetime {
    pid_t owner;
    int root,group,events,busy,stale,restored;
    struct ae_peer peer;
    sd_bus *bus; /* borrowed; original unit reference belongs to the caller */
    struct ae_process_context context;
    struct ae_root_identity root_identity,group_identity;
    uint64_t pid_namespace,cgroup_namespace;
    char unit_path[1024];
};
static int namespace_check(const struct ae_lifetime *);
static int owner(struct ae_lifetime *v)
{ return v==NULL ? -EINVAL : v->owner!=getpid() ? -ECHILD : v->stale ? -ESTALE : 0; }
static int group_open(int root,const char *path)
{
    if (path[0]!='/' || path[1]==0) return -EINVAL;
    struct open_how how={.flags=O_RDONLY|O_DIRECTORY|O_CLOEXEC,
        .resolve=RESOLVE_BENEATH|RESOLVE_NO_SYMLINKS|RESOLVE_NO_MAGICLINKS|RESOLVE_NO_XDEV};
    int fd; do { fd=(int)syscall(SYS_openat2,root,path+1,&how,sizeof how); } while (fd<0 && errno==EINTR);
    return fd<0 ? -errno : fd;
}
static int unit_path(struct ae_lifetime *v,char output[1024])
{
    sd_bus_message *reply=NULL; const char *path=NULL;
    int r=sd_bus_call_method(v->bus,MANAGER,"/org/freedesktop/systemd1",MANAGER ".Manager","GetUnit",NULL,&reply,"s",v->context.unit);
    if (r>=0 && (!sd_bus_message_has_signature(reply,"o") || sd_bus_message_read(reply,"o",&path)<=0)) r=-EBADMSG;
    if (r>=0 && (path==NULL || strlen(path)>=1024)) r=-E2BIG;
    if (r>=0) { strcpy(output,path); r=0; }
    sd_bus_message_unref(reply); return r;
}
static int snapshot_unit(struct ae_lifetime *v,int terminal,int *failed)
{
    char path[1024]; int r=unit_path(v,path);
    if (r==0 && strcmp(path,v->unit_path)!=0) r=-ESTALE;
    char *name=NULL,*state=NULL,*group=NULL; sd_bus_message *inv=NULL,*job=NULL;
    if (r==0) r=sd_bus_get_property_string(v->bus,MANAGER,path,UNIT,"Id",NULL,&name);
    if (r>=0 && strcmp(name,v->context.unit)!=0) r=-ESTALE;
    if (r>=0) r=sd_bus_get_property(v->bus,MANAGER,path,UNIT,"InvocationID",NULL,&inv,"ay");
    const void *bytes=NULL; size_t size=0;
    if (r>=0 && (sd_bus_message_read_array(inv,'y',&bytes,&size)<=0 || size!=16 || memcmp(bytes,v->context.invocation,16)!=0)) r=-ESTALE;
    if (r>=0) r=sd_bus_get_property_string(v->bus,MANAGER,path,UNIT,"ActiveState",NULL,&state);
    if (r>=0 && (terminal ? strcmp(state,"inactive")!=0 && strcmp(state,"failed")!=0 : strcmp(state,"active")!=0)) r=-EAGAIN;
    uint32_t main_pid=UINT32_MAX,last_pid=UINT32_MAX,job_id=UINT32_MAX; const char *job_path=NULL;
    if (r>=0) r=sd_bus_get_property_trivial(v->bus,MANAGER,path,SERVICE,"MainPID",NULL,'u',&main_pid);
    if (r>=0 && main_pid!=(terminal ? 0U : (uint32_t)v->peer.connection_pid)) r=-EAGAIN;
    if (r>=0) r=sd_bus_get_property_trivial(v->bus,MANAGER,path,SERVICE,"ExecMainPID",NULL,'u',&last_pid);
    if (r>=0 && last_pid!=(uint32_t)v->peer.connection_pid) r=-ESTALE;
    if (r>=0) r=sd_bus_get_property(v->bus,MANAGER,path,UNIT,"Job",NULL,&job,"(uo)");
    if (r>=0 && (sd_bus_message_read(job,"(uo)",&job_id,&job_path)<=0 || job_id!=0 || strcmp(job_path,"/")!=0)) r=-EAGAIN;
    if (r>=0) r=sd_bus_get_property_string(v->bus,MANAGER,path,SERVICE,"ControlGroup",NULL,&group);
    if (r>=0 && strcmp(group,v->context.cgroup)!=0 && (!terminal || group[0]!=0)) r=-ESTALE;
    if (r>=0) { if (failed!=NULL) *failed=strcmp(state,"failed")==0; r=0; }
    free(name); free(state); free(group); sd_bus_message_unref(inv); sd_bus_message_unref(job); return r;
}
static int populated(int fd,int *value)
{
    if (lseek(fd,0,SEEK_SET)<0) return -errno;
    char bytes[4097]; size_t used=0; int r=0;
    while (used<sizeof bytes) {
        ssize_t n; do { n=read(fd,bytes+used,sizeof bytes-used); } while (n<0 && errno==EINTR);
        if (n<0) { r=-errno; break; } if (n==0) break; used+=(size_t)n;
    }
    if (r!=0) return r;
    if (used==0 || used==sizeof bytes || memchr(bytes,0,used)!=NULL) return -EBADMSG;
    size_t offset=0; int found=0,result=0;
    while (offset<used) {
        size_t key=offset;
        while (offset<used && ((bytes[offset]>='a' && bytes[offset]<='z') || bytes[offset]=='_' || bytes[offset]=='.')) ++offset;
        size_t length=offset-key;
        if (length==0 || offset==used || bytes[offset++]!=' ') return -EBADMSG;
        size_t start=offset; uint64_t number=0;
        while (offset<used && bytes[offset]>='0' && bytes[offset]<='9') {
            unsigned int digit=(unsigned int)(bytes[offset++]-'0'); if (number>(UINT64_MAX-digit)/10U) return -EBADMSG; number=number*10U+digit;
        }
        if (start==offset || offset==used || bytes[offset++]!='\n') return -EBADMSG;
        if (length==9 && memcmp(bytes+key,"populated",9)==0) {
            if (found || number>1) return -EBADMSG;
            found=1; result=(int)number;
        }
    }
    if (!found) return -ENODATA;
    *value=result; return 0;
}
static int snapshot_group(struct ae_lifetime *v,int terminal,int *removed)
{
    struct ae_root_identity anchor;
    int r=v->restored ? namespace_check(v) : ae_root_check_path(v->root,CGROOT,&v->root_identity);
    if (r==0 && v->restored) r=ae_root_observe(v->root,&anchor);
    if (r==0 && v->restored && !ae_root_equal(&anchor,&v->root_identity)) r=-ESTALE;
    struct ae_root_identity held,named;
    if (r==0) r=ae_root_observe(v->group,&held);
    if (r==0 && !ae_root_equal(&held,&v->group_identity)) r=-ESTALE;
    if (r!=0) return r;
    int fd=group_open(v->root,v->context.cgroup);
    if (fd==-ENOENT) {
        int count=1; r=populated(v->events,&count);
        /* A missing name is not removal. The original kernfs event object
         * must independently reject access because it has been deactivated.
         * Unlike st_nlink (which may still report 2), this also rejects a
         * renamed live original. Unsupported kernels fail closed. */
        if (terminal && r==-ENODEV) { *removed=1; return 0; }
        return r==0 || r==-ENODEV ? -ESTALE : r;
    }
    if (fd<0) return fd;
    r=ae_root_observe(fd,&named); close(fd);
    if (r==0 && !ae_root_equal(&named,&v->group_identity)) r=-ESTALE;
    int count=1;
    if (r==0) r=populated(v->events,&count);
    if (r==-ENODEV) r=-EAGAIN; /* concurrent removal must be observed again */
    if (r==0 && count!=(terminal ? 0 : 1)) r=-EAGAIN;
    if (r==0) *removed=0;
    return r;
}
int ae_lifetime_capture(sd_bus *bus,const struct ae_peer *peer,const struct ae_process_context *expected,struct ae_lifetime **output)
{
    if (bus==NULL || peer==NULL || expected==NULL || output==NULL || *output!=NULL) return -EINVAL;
    if (getuid()!=0 || geteuid()!=0 || getgid()!=0 || getegid()!=0) return -EPERM;
    int r=ae_process_verify(peer,bus,expected); if (r!=0) return r;
    struct ae_lifetime *v=calloc(1,sizeof *v); if (v==NULL) return -ENOMEM;
    v->owner=getpid(); v->root=v->group=v->events=-1; v->peer=*peer; v->peer.pidfd=-1; v->bus=bus; v->context=*expected;
    v->peer.pidfd=fcntl(peer->pidfd,F_DUPFD_CLOEXEC,3); if (v->peer.pidfd<0) r=-errno;
    if (r==0) { v->root=open(CGROOT,O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW); if (v->root<0) r=-errno; }
    if (r==0) r=ae_root_observe(v->root,&v->root_identity);
    if (r==0 && v->root_identity.filesystem_type!=CGROUP2_SUPER_MAGIC) r=-ENOTSUP;
    if (r==0) { v->group=group_open(v->root,expected->cgroup); if (v->group<0) r=v->group; }
    if (r==0) r=ae_root_observe(v->group,&v->group_identity);
    if (r==0 && (v->group_identity.inode!=expected->cgroup_id || v->group_identity.mount_id!=v->root_identity.mount_id)) r=-ESTALE;
    if (r==0) { v->events=openat(v->group,"cgroup.events",O_RDONLY|O_CLOEXEC|O_NOFOLLOW); if (v->events<0) r=-errno; }
    if (r==0) r=unit_path(v,v->unit_path);
    if (r==0) r=snapshot_unit(v,0,NULL);
    int removed=0;
    if (r==0) r=snapshot_group(v,0,&removed);
    if (r==0) r=ae_process_verify(&v->peer,bus,expected);
    if (r==0) { *output=v; return 0; }
    (void)ae_lifetime_close(&v); return r;
}
static int current_namespace(const char *self,uint64_t *output)
{
    struct stat st;
    if (stat(self,&st)<0) return -errno;
    if (!st.st_ino) return -ENODATA;
    *output=(uint64_t)st.st_ino; return 0;
}
static int namespaces(struct ae_lifetime_checkpoint *v)
{
    /* Host admission is the caller's existing protected supervisor obligation.
     * Self observations only preserve that original namespace binding; reading
     * PID1's namespace links would need an otherwise unnecessary ptrace grant. */
    int r=current_namespace("/proc/self/ns/pid",&v->pid_namespace);
    if (r==0) r=current_namespace("/proc/self/ns/cgroup",&v->cgroup_namespace);
    return r;
}
static int namespace_check(const struct ae_lifetime *v)
{
    struct ae_lifetime_checkpoint now={0}; int r=namespaces(&now);
    if (r==0 && (now.pid_namespace!=v->pid_namespace || now.cgroup_namespace!=v->cgroup_namespace)) r=-ESTALE;
    return r;
}
static int events_identity(int fd,const struct ae_lifetime_checkpoint *v,struct ae_lifetime_checkpoint *out)
{
    const unsigned int required=STATX_TYPE|STATX_MODE|STATX_INO|STATX_UID|STATX_GID|STATX_MNT_ID_UNIQUE;
    struct statx st; struct statfs fs;
    if (statx(fd,"",AT_EMPTY_PATH|AT_SYMLINK_NOFOLLOW,required,&st)<0 || fstatfs(fd,&fs)<0) return -errno;
    if ((st.stx_mask&required)!=required || !st.stx_ino || !st.stx_mnt_id) return -ENOTSUP;
    int flags=fcntl(fd,F_GETFL);
    if (flags<0) return -errno;
    if (!S_ISREG(st.stx_mode) || fs.f_type!=CGROUP2_SUPER_MAGIC ||
        st.stx_mnt_id!=v->root.mount_id || st.stx_dev_major!=v->root.device_major ||
        st.stx_dev_minor!=v->root.device_minor || (flags&O_ACCMODE)!=O_RDONLY || (flags&O_PATH)) return -ESTALE;
    out->events_inode=st.stx_ino; out->events_mount=st.stx_mnt_id;
    out->events_mode=st.stx_mode; out->events_uid=st.stx_uid; out->events_gid=st.stx_gid;
    return 0;
}
static int events_equal(const struct ae_lifetime_checkpoint *a,const struct ae_lifetime_checkpoint *b)
{
    return a->events_inode==b->events_inode && a->events_mount==b->events_mount &&
        a->events_mode==b->events_mode && a->events_uid==b->events_uid && a->events_gid==b->events_gid;
}
int ae_lifetime_export(struct ae_lifetime *v,struct ae_lifetime_checkpoint *output,int handles[AE_LIFETIME_HANDLE_COUNT])
{
    int r=owner(v);
    if (r==0 && (output==NULL || handles==NULL)) r=-EINVAL;
    if (r==0 && v->busy) r=-EBUSY;
    if (r==0 && v->restored) r=-EALREADY;
    if (r==0) for (int n=0;n<AE_LIFETIME_HANDLE_COUNT;++n) if (handles[n]!=-1) r=-EINVAL;
    if (r!=0) return r;
    v->busy=1; struct ae_lifetime_checkpoint value={.context=v->context,.root=v->root_identity,
        .group=v->group_identity,.pid=(uint32_t)v->peer.connection_pid};
    int removed=0,fds[AE_LIFETIME_HANDLE_COUNT]={-1,-1,-1,-1};
    r=ae_process_identify(&v->peer,v->bus,&v->context,&value.process);
    if (r==0) r=namespaces(&value);
    if (r==0) r=snapshot_group(v,0,&removed);
    if (r==0) r=events_identity(v->events,&value,&value);
    const int source[AE_LIFETIME_HANDLE_COUNT]={v->peer.pidfd,v->root,v->group,v->events};
    for (int n=0;r==0 && n<AE_LIFETIME_HANDLE_COUNT;++n) { fds[n]=fcntl(source[n],F_DUPFD_CLOEXEC,3); if (fds[n]<0) r=-errno; }
    if (r==0) r=ae_process_verify(&v->peer,v->bus,&v->context);
    if (r==0) { *output=value; memcpy(handles,fds,sizeof fds); }
    else for (int n=0;n<AE_LIFETIME_HANDLE_COUNT;++n) if (fds[n]>=0) close(fds[n]);
    if (r==-ESTALE) v->stale=1;
    v->busy=0; return r;
}
static int restore_objects(struct ae_lifetime *v,const struct ae_lifetime_checkpoint *expected)
{
    struct ae_root_identity root,group; struct ae_lifetime_checkpoint actual={.root=expected->root};
    int r=ae_root_observe(v->root,&root);
    if (r==0) r=ae_root_observe(v->group,&group);
    if (r==0 && (!ae_root_equal(&root,&expected->root) || !ae_root_equal(&group,&expected->group))) r=-ESTALE;
    if (r==0) r=events_identity(v->events,expected,&actual);
    if (r==0 && !events_equal(&actual,expected)) r=-ESTALE;
    struct stat st; struct statfs fs;
    if (r==0 && (fstat(v->peer.pidfd,&st)<0 || fstatfs(v->peer.pidfd,&fs)<0)) r=-errno;
    if (r==0 && (fs.f_type!=PID_FS_MAGIC || (uint64_t)st.st_ino!=expected->process.pidfd_id)) r=-ESTALE;
    return r;
}
int ae_lifetime_restore(sd_bus *bus,const struct ae_lifetime_checkpoint *expected,const int handles[AE_LIFETIME_HANDLE_COUNT],struct ae_lifetime **output)
{
    if (!bus || !expected || !handles || !output || *output!=NULL) return -EINVAL;
    if (getuid()!=0 || geteuid()!=0 || getgid()!=0 || getegid()!=0) return -EPERM;
    for (int n=0;n<AE_LIFETIME_HANDLE_COUNT;++n) if (handles[n]<0) return -EINVAL;
    uint8_t encoded[AE_PROCESS_IDENTITY_BYTES];
    int r=ae_process_identity_encode(&expected->process,encoded);
    if (r==0 && (memcmp(expected->process.boot,expected->context.boot,16)!=0 || !expected->pid || expected->pid>INT32_MAX ||
        !expected->context.cgroup_id || expected->group.inode!=expected->context.cgroup_id ||
        expected->root.filesystem_type!=CGROUP2_SUPER_MAGIC || expected->group.filesystem_type!=CGROUP2_SUPER_MAGIC ||
        expected->root.mount_id!=expected->group.mount_id || expected->context.cgroup[0]!='/' || !expected->context.cgroup[1] ||
        !expected->context.unit[0] || !expected->context.label[0] ||
        !memchr(expected->context.unit,0,sizeof expected->context.unit) ||
        !memchr(expected->context.label,0,sizeof expected->context.label) ||
        !memchr(expected->context.cgroup,0,sizeof expected->context.cgroup))) r=-EBADMSG;
    sd_id128_t boot; if (r==0) r=sd_id128_get_boot(&boot);
    if (r==0 && memcmp(boot.bytes,expected->context.boot,16)!=0) r=-ESTALE;
    struct ae_lifetime_checkpoint current={0}; if (r==0) r=namespaces(&current);
    if (r==0 && (current.pid_namespace!=expected->pid_namespace || current.cgroup_namespace!=expected->cgroup_namespace)) r=-ESTALE;
    if (r!=0) return r;
    struct ae_lifetime *v=calloc(1,sizeof *v); if (!v) return -ENOMEM;
    v->owner=getpid(); v->root=v->group=v->events=v->peer.pidfd=-1; v->bus=bus; v->restored=1;
    v->context=expected->context; v->root_identity=expected->root; v->group_identity=expected->group;
    v->pid_namespace=expected->pid_namespace; v->cgroup_namespace=expected->cgroup_namespace;
    /* Internal observation pin only: no bus name/connection credentials are
     * reconstructed. Never expose this as a fresh authenticated IPC peer. */
    v->peer.connection_pid=(pid_t)expected->pid; v->peer.connection_uid=expected->context.uid;
    strcpy(v->peer.connection_label,expected->context.label);
    int *dest[AE_LIFETIME_HANDLE_COUNT]={&v->peer.pidfd,&v->root,&v->group,&v->events};
    for (int n=0;r==0 && n<AE_LIFETIME_HANDLE_COUNT;++n) { *dest[n]=fcntl(handles[n],F_DUPFD_CLOEXEC,3); if (*dest[n]<0) r=-errno; }
    if (r==0) r=restore_objects(v,expected);
    if (r==0) r=unit_path(v,v->unit_path);
    /* Restore is read-only and may see either a still-live original or its end.
     * A change during these observations remains pending; it never releases. */
    int alive=r==0 ? ae_peer_alive(&v->peer) : r,removed=0;
    if (r==0 && alive!=0 && alive!=-ESRCH) r=alive;
    if (r==0) r=snapshot_unit(v,alive==-ESRCH,NULL);
    if (r==0) r=snapshot_group(v,alive==-ESRCH,&removed);
    if (r==0 && alive==0) r=ae_process_verify(&v->peer,bus,&v->context);
    if (r==0) r=restore_objects(v,expected);
    if (r==0) { *output=v; return 0; }
    (void)ae_lifetime_close(&v); return r;
}
int ae_lifetime_observe_end(struct ae_lifetime *v,struct ae_lifetime_end *output)
{
    int r=owner(v); if (r==0 && output==NULL) r=-EINVAL; if (r==0 && v->busy) r=-EBUSY; if (r!=0) return r;
    v->busy=1;
    int alive=ae_peer_alive(&v->peer); r=alive==-ESRCH ? 0 : alive==0 ? -EAGAIN : alive;
    int first_removed=0,second_removed=0,first_failed=0,second_failed=0;
    if (r==0) r=snapshot_unit(v,1,&first_failed);
    if (r==0) r=snapshot_group(v,1,&first_removed);
    if (r==0) r=snapshot_unit(v,1,&second_failed);
    if (r==0) r=snapshot_group(v,1,&second_removed);
    if (r==0 && (first_removed!=second_removed || first_failed!=second_failed)) r=-EAGAIN;
    if (r==0 && ae_peer_alive(&v->peer)!=-ESRCH) r=-ESTALE;
    if (r==-ESTALE) v->stale=1;
    if (r==0) {
        struct ae_lifetime_end result={.cgroup_id=v->context.cgroup_id,.pid=(uint32_t)v->peer.connection_pid,.group_removed=second_removed,.unit_failed=second_failed};
        memcpy(result.boot,v->context.boot,16); memcpy(result.invocation,v->context.invocation,16); *output=result;
    }
    v->busy=0; return r;
}
int ae_lifetime_close(struct ae_lifetime **output)
{
    if (output==NULL) return -EINVAL;
    struct ae_lifetime *v=*output; if (v==NULL) return 0;
    if (v->owner!=getpid()) return -ECHILD;
    if (v->busy) return -EBUSY;
    if (v->root>=0) close(v->root);
    if (v->group>=0) close(v->group);
    if (v->events>=0) close(v->events);
    ae_peer_clear(&v->peer); free(v); *output=NULL; return 0;
}
