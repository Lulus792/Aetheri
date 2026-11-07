#include "linux/unit.h"
#include "core/digest.h"
#include <errno.h>
#include <fcntl.h>
#include <linux/openat2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/xattr.h>
#include <unistd.h>

#define MANAGER "org.freedesktop.systemd1"
#define PATH "/org/freedesktop/systemd1"
#define UNIT MANAGER ".Unit"
#define SERVICE MANAGER ".Service"
static const char *const edges[AE_UNIT_EDGE_COUNT]={"Requires","Wants","Requisite","BindsTo","PartOf","Upholds","Conflicts",
    "OnFailure","OnSuccess","Triggers","TriggeredBy","PropagatesStopTo","PropagatesReloadTo","JoinsNamespaceOf","After"};
struct ae_unit {
    pid_t owner;
    int busy,manager_bad;
    sd_bus *bus;
    sd_bus_slot *names;
    struct ae_peer manager;
    uint8_t boot[16];
    struct ae_unit_profile profile;
    char paths[AE_UNIT_MAX][512];
    struct ae_unit_status status;
    struct ae_start *start;
    ae_start_changed changed;
    void *observer;
    char check[64];
};
static int owner(struct ae_unit *u) { return !u ? -EINVAL : u->owner==getpid() ? 0 : -ECHILD; }
static int builtin(const struct ae_unit_node *n) { return n->kind==AE_UNIT_SYSTEM_SLICE || n->kind==AE_UNIT_ROOT_SLICE; }
static void fail(struct ae_unit *u,int r,int quarantine)
{
    if (!u->status.error) { u->status.error=r; strcpy(u->status.attribute,u->check); }
    if (quarantine) u->status.quarantined=1; else u->status.fenced=1; u->status.ready=0;
}
static int file_stat(int fd,struct statx *s)
{
    unsigned int required=STATX_BASIC_STATS|STATX_MNT_ID_UNIQUE; int r;
    do { r=statx(fd,"",AT_EMPTY_PATH|AT_SYMLINK_NOFOLLOW,required|STATX_BTIME,s); } while (r<0 && errno==EINTR);
    if (r<0) return -errno;
    if ((s->stx_mask&required)!=required || !s->stx_mnt_id) return -ENOTSUP;
    if (!S_ISREG(s->stx_mode) || s->stx_uid || s->stx_gid || s->stx_nlink!=1 ||
        (s->stx_mode!=(S_IFREG|0444) && s->stx_mode!=(S_IFREG|0644)) || !s->stx_size || s->stx_size>AE_UNIT_FILE_MAX) return -EACCES;
    char label[128]; ssize_t size=fgetxattr(fd,"security.selinux",label,sizeof label);
    if (size<0) return -errno;
    if (size>0 && !label[size-1]) --size;
    static const char expected[]="system_u:object_r:systemd_unit_file_t:s0";
    return size==(ssize_t)(sizeof expected-1) && memcmp(label,expected,sizeof expected-1)==0 ? 0 : -EACCES;
}
static int same_stat(const struct statx *a,const struct statx *b)
{
    return a->stx_mnt_id==b->stx_mnt_id && a->stx_ino==b->stx_ino && a->stx_dev_major==b->stx_dev_major && a->stx_dev_minor==b->stx_dev_minor &&
        a->stx_mode==b->stx_mode && a->stx_uid==b->stx_uid && a->stx_gid==b->stx_gid && a->stx_nlink==b->stx_nlink && a->stx_size==b->stx_size &&
        a->stx_mtime.tv_sec==b->stx_mtime.tv_sec && a->stx_mtime.tv_nsec==b->stx_mtime.tv_nsec &&
        a->stx_ctime.tv_sec==b->stx_ctime.tv_sec && a->stx_ctime.tv_nsec==b->stx_ctime.tv_nsec &&
        ((a->stx_mask^b->stx_mask)&STATX_BTIME)==0 && (!(a->stx_mask&STATX_BTIME) ||
        (a->stx_btime.tv_sec==b->stx_btime.tv_sec && a->stx_btime.tv_nsec==b->stx_btime.tv_nsec));
}
int ae_unit_file_capture(int fd,const uint8_t expected[32],struct ae_unit_file *out)
{
    if (fd<0 || !expected || !out) return -EINVAL;
    int flags=fcntl(fd,F_GETFL); if (flags<0) return -errno;
    if ((flags&O_ACCMODE)!=O_RDONLY || (flags&O_PATH)) return -EACCES;
    struct ae_unit_file value; struct statx after; int r=file_stat(fd,&value.object);
    if (!r) r=ae_sha256_fd(fd,value.object.stx_size,value.digest);
    if (!r && memcmp(value.digest,expected,32)!=0) r=-EBADMSG;
    if (!r) r=file_stat(fd,&after);
    if (!r && !same_stat(&value.object,&after)) r=-ESTALE;
    if (!r) *out=value;
    return r;
}
static int source(int directory_fd,const char *directory,const char *leaf,int file_fd,
    const struct ae_root_identity *parent,const struct ae_unit_file *file)
{
        int r=ae_root_check_path(directory_fd,directory,parent);
        struct ae_unit_file held,named;
        if (!r) r=ae_unit_file_capture(file_fd,file->digest,&held);
        if (!r && !same_stat(&held.object,&file->object)) r=-ESTALE;
        int fd=-1;
        if (!r) {
            struct open_how how={.flags=O_RDONLY|O_CLOEXEC|O_NOFOLLOW,.resolve=RESOLVE_BENEATH|RESOLVE_NO_SYMLINKS|RESOLVE_NO_MAGICLINKS|RESOLVE_NO_XDEV};
            fd=(int)syscall(SYS_openat2,directory_fd,leaf,&how,sizeof how); if (fd<0) r=-errno;
        }
        if (!r) r=ae_unit_file_capture(fd,file->digest,&named);
        if (!r && !same_stat(&held.object,&named.object)) r=-ESTALE;
        if (fd>=0) close(fd);
        return r;
}
static int sources(struct ae_unit *u)
{
    strcpy(u->check,"source");
    for (unsigned int i=0;i<u->profile.count;++i) {
        struct ae_unit_node *n=&u->profile.nodes[i]; if (builtin(n)) continue;
        int r=source(n->directory_fd,n->directory,n->name,n->file_fd,&n->parent,&n->file);
        for (unsigned int j=0;!r && j<n->dropin_count;++j) {
            const struct ae_unit_dropin *d=&n->dropins[j]; r=source(d->directory_fd,d->directory,d->leaf,d->file_fd,&d->parent,&d->file);
        }
        if (r) return r;
    }
    return 0;
}
static int current(struct ae_unit *u)
{
    strcpy(u->check,"manager");
    if (u->manager_bad) return -ESTALE;
    sd_id128_t boot,bus; int r=sd_id128_get_boot(&boot);
    if (r>=0 && memcmp(boot.bytes,u->boot,16)) r=-ESTALE;
    if (r>=0) r=sd_bus_get_bus_id(u->bus,&bus);
    if (r>=0 && memcmp(bus.bytes,u->manager.bus_id,16)) r=-ESTALE;
    if (r>=0) r=ae_peer_alive(&u->manager);
    sd_bus_creds *creds=NULL; const char *unique=NULL;
    if (r>=0) r=sd_bus_get_name_creds(u->bus,MANAGER,SD_BUS_CREDS_UNIQUE_NAME,&creds);
    if (r>=0) r=sd_bus_creds_get_unique_name(creds,&unique);
    if (r>=0 && strcmp(unique,u->manager.unique_name)) r=-ESTALE;
    sd_bus_creds_unref(creds);
    if (r>=0 && u->manager_bad) r=-ESTALE;
    if (r<0) { u->manager_bad=1; fail(u,r,1); }
    return r<0 ? r : 0;
}
static int names(sd_bus_message *m,void *userdata,sd_bus_error *error)
{
    (void)error; struct ae_unit *u=userdata; if (owner(u)) return 0;
    const char *name=NULL,*old=NULL,*now=NULL; int r=sd_bus_message_read(m,"sss",&name,&old,&now);
    if (r>0 && strcmp(name,MANAGER)) return 0;
    if (r<=0 || sd_bus_message_at_end(m,1)!=1 || !strcmp(old,u->manager.unique_name) || strcmp(now,u->manager.unique_name)) {
        u->manager_bad=1; fail(u,-ESTALE,1);
    }
    return 0;
}
static int guarded(struct ae_unit *u,ae_start_guard guard,void *data)
{
    int r=current(u); if (!r) r=guard(data); if (r>0) r=-EIO;
    if (!r && u->manager_bad) r=-ESTALE;
    if (!r && sd_bus_is_ready(u->bus)!=1) { r=-ENOTCONN; u->manager_bad=1; strcpy(u->check,"connection"); fail(u,r,1); }
    return r;
}
static int call(struct ae_unit *u,const char *method,const char *argument,int *attempted,ae_start_guard guard,void *data)
{
    int r=guarded(u,guard,data); sd_bus_message *q=NULL,*reply=NULL;
    if (!r) r=sd_bus_message_new_method_call(u->bus,&q,u->manager.unique_name,PATH,MANAGER ".Manager",method);
    if (r>=0 && argument) r=sd_bus_message_append(q,"s",argument);
    if (r>=0) r=guarded(u,guard,data);
    if (!r) { strcpy(u->check,method); *attempted=1; r=sd_bus_call(u->bus,q,UINT64_C(2000000),NULL,&reply); }
    struct ae_peer peer=AE_PEER_INIT;
    if (r>=0) r=ae_peer_capture_reply(reply,q,&peer);
    if (!r && (peer.connection_pid!=1 || peer.connection_uid || memcmp(peer.bus_id,u->manager.bus_id,16) ||
        strcmp(peer.unique_name,u->manager.unique_name) || strcmp(peer.connection_label,u->manager.connection_label) ||
        !sd_bus_message_has_signature(reply,"") || sd_bus_message_at_end(reply,1)!=1)) r=-ESTALE;
    if (!r) r=current(u);
    ae_peer_clear(&peer); sd_bus_message_unref(reply); sd_bus_message_unref(q);
    if (r<0 && *attempted) fail(u,r,1);
    return r<0 ? r : 0;
}
/* All property reads target the admitted UNIQUE Manager, never a replaced owner. */
static int get(struct ae_unit *u,unsigned int i,const char *interface,const char *key,const char *type,sd_bus_message **out)
{
    size_t size=strnlen(key,sizeof u->check); if (size==sizeof u->check) return -E2BIG;
    memcpy(u->check,key,size+1);
    int r=sd_bus_get_property(u->bus,u->manager.unique_name,u->paths[i],interface,key,NULL,out,type); return r<0 ? r : 0;
}
static int end_property(sd_bus_message *m)
{
    /* sd_bus_get_property returns inside the reply's outer variant. */
    return sd_bus_message_at_end(m,0)==1 && sd_bus_message_exit_container(m)>0 && sd_bus_message_at_end(m,1)==1;
}
static int string(struct ae_unit *u,unsigned int i,const char *interface,const char *key,const char *expected)
{
    sd_bus_message *m=NULL; const char *value=NULL; int r=get(u,i,interface,key,"s",&m);
    if (!r && (sd_bus_message_read(m,"s",&value)<=0 || strcmp(value,expected) || !end_property(m))) r=-ESTALE;
    sd_bus_message_unref(m); return r;
}
static int number(struct ae_unit *u,unsigned int i,const char *interface,const char *key,char type,uint64_t expected)
{
    char signature[]={type,0}; sd_bus_message *m=NULL; int r=get(u,i,interface,key,signature,&m);
    uint64_t wide=0; uint32_t small=0; int boolean=0;
    void *p=type=='t' ? (void *)&wide : type=='u' ? (void *)&small : (void *)&boolean;
    if (!r && (sd_bus_message_read(m,signature,p)<=0 || !end_property(m))) r=-EBADMSG;
    if (!r && (type=='t' ? wide : type=='u' ? small : (uint64_t)boolean)!=expected) r=-ESTALE;
    sd_bus_message_unref(m); return r;
}
static int list(struct ae_unit *u,unsigned int i,const char *interface,const char *key,const char *const *values,unsigned int count,int ordered)
{
    sd_bus_message *m=NULL; int r=get(u,i,interface,key,"as",&m); uint32_t seen=0; unsigned int used=0;
    if (!r && sd_bus_message_enter_container(m,'a',"s")<=0) r=-EBADMSG;
    while (!r && sd_bus_message_at_end(m,0)==0) {
        const char *value=NULL; if (sd_bus_message_read(m,"s",&value)<=0) { r=-EBADMSG; break; }
        unsigned int j=0; while (j<count && strcmp(value,values[j])) ++j;
        if (j==count || (seen&(1U<<j)) || (ordered && j!=used)) r=-ESTALE;
        else { seen|=1U<<j; ++used; }
    }
    if (!r && (used!=count || sd_bus_message_exit_container(m)<=0 || !end_property(m))) r=-ESTALE;
    sd_bus_message_unref(m); return r;
}
static int empty(struct ae_unit *u,unsigned int i,const char *interface,const char *key,const char *signature,const char *contents)
{
    sd_bus_message *m=NULL; int r=get(u,i,interface,key,signature,&m);
    if (!r && (sd_bus_message_enter_container(m,'a',contents)<=0 || sd_bus_message_at_end(m,0)!=1 ||
        sd_bus_message_exit_container(m)<=0 || !end_property(m))) r=-ESTALE;
    sd_bus_message_unref(m); return r;
}
static int job(struct ae_unit *u,unsigned int i)
{
    sd_bus_message *m=NULL; uint32_t id=1; const char *path=NULL; int r=get(u,i,UNIT,"Job","(uo)",&m);
    if (!r && (sd_bus_message_read(m,"(uo)",&id,&path)<=0 || id || strcmp(path,"/") || !end_property(m))) r=-EAGAIN;
    sd_bus_message_unref(m); return r;
}
static int lookup(struct ae_unit *u,unsigned int i,int bind)
{
    sd_bus_message *q=NULL,*m=NULL; const char *path=NULL;
    int r=sd_bus_message_new_method_call(u->bus,&q,u->manager.unique_name,PATH,MANAGER ".Manager","GetUnit");
    if (r>=0) r=sd_bus_message_append(q,"s",u->profile.nodes[i].name);
    if (r>=0) r=sd_bus_call(u->bus,q,UINT64_C(2000000),NULL,&m);
    struct ae_peer p=AE_PEER_INIT; if (r>=0) r=ae_peer_capture_reply(m,q,&p);
    if (!r && (p.connection_pid!=1 || p.connection_uid || strcmp(p.unique_name,u->manager.unique_name) ||
        strcmp(p.connection_label,u->manager.connection_label) || memcmp(p.bus_id,u->manager.bus_id,16))) r=-ESTALE;
    if (!r && (!sd_bus_message_has_signature(m,"o") || sd_bus_message_read(m,"o",&path)<=0 ||
        sd_bus_message_at_end(m,1)!=1 || strnlen(path,512)==512)) r=-EBADMSG;
    if (!r && !bind && strcmp(path,u->paths[i])) r=-ESTALE;
    if (!r && bind) strcpy(u->paths[i],path);
    ae_peer_clear(&p); sd_bus_message_unref(q); sd_bus_message_unref(m); return r<0 ? r : 0;
}
static int exec_start(struct ae_unit *u,unsigned int i)
{
    const struct ae_unit_node *n=&u->profile.nodes[i]; sd_bus_message *m=NULL;
    int r=get(u,i,SERVICE,"ExecStartEx","a(sasasttttuii)",&m);
    if (!r && (sd_bus_message_enter_container(m,'a',"(sasasttttuii)")<=0 || sd_bus_message_enter_container(m,'r',"sasasttttuii")<=0)) r=-EBADMSG;
    const char *path=NULL;
    if (!r && (sd_bus_message_read(m,"s",&path)<=0 || strcmp(path,n->executable) || sd_bus_message_enter_container(m,'a',"s")<=0)) r=-ESTALE;
    for (unsigned int j=0;!r && j<n->argc;++j) { const char *arg=NULL; if (sd_bus_message_read(m,"s",&arg)<=0 || strcmp(arg,n->argv[j])) r=-ESTALE; }
    if (!r && (sd_bus_message_at_end(m,0)!=1 || sd_bus_message_exit_container(m)<=0 || sd_bus_message_enter_container(m,'a',"s")<=0 ||
        sd_bus_message_at_end(m,0)!=1 || sd_bus_message_exit_container(m)<=0 || sd_bus_message_skip(m,"ttttuii")<=0 ||
        sd_bus_message_exit_container(m)<=0 || sd_bus_message_at_end(m,0)!=1 || sd_bus_message_exit_container(m)<=0 || !end_property(m))) r=-ESTALE;
    sd_bus_message_unref(m); return r;
}
static int context(struct ae_unit *u,unsigned int i,const char *expected)
{
    sd_bus_message *m=NULL; int ignore=1; const char *label=NULL; int r=get(u,i,SERVICE,"SELinuxContext","(bs)",&m);
    if (!r && (sd_bus_message_read(m,"(bs)",&ignore,&label)<=0 || ignore || strcmp(label,expected) || !end_property(m))) r=-ESTALE;
    sd_bus_message_unref(m); return r;
}
static int no_forced_restart(struct ae_unit *u,unsigned int i)
{
    sd_bus_message *m=NULL; int r=get(u,i,SERVICE,"RestartForceExitStatus","(aiai)",&m);
    if (!r && sd_bus_message_enter_container(m,'r',"aiai")<=0) r=-EBADMSG;
    for (unsigned int j=0;!r && j<2;++j) if (sd_bus_message_enter_container(m,'a',"i")<=0 ||
        sd_bus_message_at_end(m,0)!=1 || sd_bus_message_exit_container(m)<=0) r=-ESTALE;
    if (!r && (sd_bus_message_exit_container(m)<=0 || !end_property(m))) r=-ESTALE;
    sd_bus_message_unref(m); return r;
}
static int service(struct ae_unit *u,unsigned int i)
{
    const struct ae_unit_node *n=&u->profile.nodes[i]; int r=0;
    const char *keys[]={"Type","ExitType","Restart","User","Group","KillMode"};
    const char *values[]={"exec","main","no","root","root","control-group"};
    for (unsigned int j=0;!r && j<6;++j) r=string(u,i,SERVICE,keys[j],values[j]);
    if (!r) r=context(u,i,n->label);
    if (!r) r=no_forced_restart(u,i);
    if (!r) r=string(u,i,SERVICE,"BusName","");
    const char *streams[]={"StandardInput","StandardOutput","StandardError"};
    for (unsigned int j=0;!r && j<3;++j) r=string(u,i,SERVICE,streams[j],"null");
    const char *zero[]={"DynamicUser","RemainAfterExit","FileDescriptorStorePreserve"};
    for (unsigned int j=0;!r && j<2;++j) r=number(u,i,SERVICE,zero[j],'b',0);
    if (!r) r=string(u,i,SERVICE,zero[2],"no");
    const char *booleans[]={"NoNewPrivileges","PrivateMounts","PrivateNetwork","PrivateDevices"};
    const int bits[]={n->no_new_privileges,n->private_mounts,n->private_network,n->private_devices};
    for (unsigned int j=0;!r && j<4;++j) r=number(u,i,SERVICE,booleans[j],'b',(uint64_t)bits[j]);
    if (!r) r=number(u,i,SERVICE,"CapabilityBoundingSet",'t',n->capabilities);
    if (!r) r=number(u,i,SERVICE,"AmbientCapabilities",'t',n->ambient);
    if (!r) r=number(u,i,SERVICE,"UMask",'u',n->umask);
    if (!r) r=number(u,i,SERVICE,"FileDescriptorStoreMax",'u',0);
    const char *none[]={"ExecConditionEx","ExecStartPreEx","ExecStartPostEx","ExecReloadEx","ExecReloadPostEx","ExecStopEx","ExecStopPostEx"};
    for (unsigned int j=0;!r && j<7;++j) r=empty(u,i,SERVICE,none[j],"a(sasasttttuii)","(sasasttttuii)");
    if (!r) r=empty(u,i,SERVICE,"EnvironmentFiles","a(sb)","(sb)");
    if (!r) r=list(u,i,SERVICE,"PassEnvironment",NULL,0,1);
    if (!r) r=list(u,i,SERVICE,"UnsetEnvironment",NULL,0,1);
    const char *env[AE_UNIT_ARGS]; for (unsigned int j=0;j<n->envc;++j) env[j]=n->environment[j];
    if (!r) r=list(u,i,SERVICE,"Environment",env,n->envc,1);
    if (!r) r=exec_start(u,i);
    return r;
}
static int basic(struct ae_unit *u,unsigned int i)
{
    const struct ae_unit_node *n=&u->profile.nodes[i]; int r=lookup(u,i,0);
    if (!r) r=string(u,i,UNIT,"Id",n->name);
    const char *one[]={n->name}; if (!r) r=list(u,i,UNIT,"Names",one,1,0);
    if (!r) r=string(u,i,UNIT,"LoadState","loaded");
    return r;
}
static int has_reference(struct ae_unit *u,unsigned int i)
{
    const char *own=NULL; int r=sd_bus_get_unique_name(u->bus,&own); sd_bus_message *m=NULL; unsigned int found=0,count=0;
    if (r>=0) r=get(u,i,UNIT,"Refs","as",&m);
    if (!r && sd_bus_message_enter_container(m,'a',"s")<=0) r=-EBADMSG;
    while (!r && sd_bus_message_at_end(m,0)==0) {
        const char *value=NULL; if (++count>64 || sd_bus_message_read(m,"s",&value)<=0) { r=-EBADMSG; break; }
        if (!strcmp(value,own)) ++found;
    }
    if (!r && (found!=1 || sd_bus_message_exit_container(m)<=0 || !end_property(m))) r=-ESTALE;
    sd_bus_message_unref(m); return r<0 ? r : 0;
}
static int quiet(struct ae_unit *u,unsigned int i,int fresh)
{
    const struct ae_unit_node *n=&u->profile.nodes[i]; int r=job(u,i); char *state=NULL;
    if (!r) r=sd_bus_get_property_string(u->bus,u->manager.unique_name,u->paths[i],UNIT,"ActiveState",NULL,&state);
    if (r>=0 && (builtin(n) ? strcmp(state,"active")!=0 : fresh ? strcmp(state,"inactive")!=0 : strcmp(state,"inactive")!=0 && strcmp(state,"failed")!=0)) r=-EAGAIN;
    free(state); if (r>0) r=0;
    if (!r && n->kind==AE_UNIT_SERVICE) {
        const char *zero[]={"MainPID","ControlPID","NFileDescriptorStore"};
        for (unsigned int j=0;!r && j<3;++j) r=number(u,i,SERVICE,zero[j],'u',0);
        if (!r && fresh) r=number(u,i,SERVICE,"ExecMainPID",'u',0);
        if (!r && fresh) r=number(u,i,SERVICE,"NRestarts",'u',0);
    }
    if (!r && fresh && !builtin(n)) {
        const char *zero[]={"ActiveEnterTimestampMonotonic","ActiveExitTimestampMonotonic","InactiveExitTimestampMonotonic"};
        for (unsigned int j=0;!r && j<3;++j) r=number(u,i,UNIT,zero[j],'t',0);
        sd_bus_message *m=NULL; const void *bytes=NULL; size_t size=0;
        if (!r) r=get(u,i,UNIT,"InvocationID","ay",&m);
        if (!r && (sd_bus_message_read_array(m,'y',&bytes,&size)<=0 || (size!=0 && size!=16) || !end_property(m))) r=-EBADMSG;
        for (size_t j=0;!r && j<size;++j) if (((const uint8_t *)bytes)[j]) r=-ESTALE;
        sd_bus_message_unref(m);
    }
    return r;
}
static int check(struct ae_unit *u)
{
    int r=current(u); if (!r) r=sources(u);
    for (unsigned int i=0;!r && i<u->profile.count;++i) {
        const struct ae_unit_node *n=&u->profile.nodes[i]; r=basic(u,i);
        if (!r) r=has_reference(u,i);
        char fragment[4354]={0}; if (!builtin(n)) { int size=snprintf(fragment,sizeof fragment,"%s/%s",n->directory,n->name); if (size<=0 || (size_t)size>=sizeof fragment) r=-E2BIG; }
        if (!r) r=string(u,i,UNIT,"FragmentPath",fragment);
        if (!r) r=string(u,i,UNIT,"SourcePath","");
        char drop_paths[AE_UNIT_DROPINS][4354]; const char *drop_values[AE_UNIT_DROPINS];
        for (unsigned int j=0;!r && j<n->dropin_count;++j) {
            int size=snprintf(drop_paths[j],sizeof drop_paths[j],"%s/%s",n->dropins[j].directory,n->dropins[j].leaf);
            if (size<=0 || (size_t)size>=sizeof drop_paths[j]) r=-E2BIG;
            drop_values[j]=drop_paths[j];
        }
        if (!r) r=list(u,i,UNIT,"DropInPaths",drop_values,n->dropin_count,1);
        if (!r) r=number(u,i,UNIT,"Transient",'b',0);
        if (!r) r=number(u,i,UNIT,"Perpetual",'b',(uint64_t)builtin(n));
        if (!r) r=number(u,i,UNIT,"DefaultDependencies",'b',0);
        if (!r) r=number(u,i,UNIT,"NeedDaemonReload",'b',0);
        const char *none[]={"FailureAction","SuccessAction","StartLimitAction","JobTimeoutAction"};
        for (unsigned int j=0;!r && j<4;++j) r=string(u,i,UNIT,none[j],"none");
        if (!r) r=empty(u,i,UNIT,"Conditions","a(sbbsi)","(sbbsi)");
        if (!r) r=empty(u,i,UNIT,"Asserts","a(sbbsi)","(sbbsi)");
        for (unsigned int j=0;!r && j<AE_UNIT_EDGE_COUNT;++j) {
            const char *values[AE_UNIT_MAX]; unsigned int count=0;
            for (unsigned int k=0;k<u->profile.count;++k) if (n->edges[j]&(1U<<k)) values[count++]=u->profile.nodes[k].name;
            r=list(u,i,UNIT,edges[j],values,count,0);
        }
        if (!r && !builtin(n)) {
            const char *values[AE_UNIT_MAX]; unsigned int count=0;
            for (unsigned int j=0;j<u->profile.count;++j) if (u->profile.nodes[j].edges[AE_UNIT_AFTER]&(1U<<i)) values[count++]=u->profile.nodes[j].name;
            r=list(u,i,UNIT,"Before",values,count,0);
            const unsigned int reverse[]={AE_UNIT_REQUIRES,AE_UNIT_WANTS,AE_UNIT_REQUISITE,AE_UNIT_BINDS_TO,AE_UNIT_PART_OF,
                AE_UNIT_UPHOLDS,AE_UNIT_CONFLICTS,AE_UNIT_ON_FAILURE,AE_UNIT_ON_SUCCESS,AE_UNIT_STOP_TO,AE_UNIT_RELOAD_TO};
            const char *keys[]={"RequiredBy","WantedBy","RequisiteOf","BoundBy","ConsistsOf","UpheldBy","ConflictedBy",
                "OnFailureOf","OnSuccessOf","StopPropagatedFrom","ReloadPropagatedFrom"};
            for (unsigned int k=0;!r && k<11;++k) {
                count=0;
                for (unsigned int j=0;j<u->profile.count;++j) if (u->profile.nodes[j].edges[reverse[k]]&(1U<<i)) values[count++]=u->profile.nodes[j].name;
                r=list(u,i,UNIT,keys[k],values,count,0);
            }
        }
        if (!r && n->kind==AE_UNIT_SERVICE) r=service(u,i);
        if (!r) r=quiet(u,i,1);
    }
    if (!r) r=sources(u);
    if (!r) r=current(u);
    return r;
}
static int valid_text(const char *p,size_t capacity) { return memchr(p,0,capacity)!=NULL; }
static int shape(const struct ae_unit_profile *p)
{
    if (!p || !p->count || p->count>AE_UNIT_MAX || (p->nodes[0].kind!=AE_UNIT_SERVICE && p->nodes[0].kind!=AE_UNIT_TARGET)) return -EINVAL;
    for (unsigned int i=0;i<p->count;++i) {
        const struct ae_unit_node *n=&p->nodes[i];
        if (n->kind<AE_UNIT_SERVICE || n->kind>AE_UNIT_ROOT_SLICE || !n->name[0] || !valid_text(n->name,sizeof n->name)) return -EINVAL;
        if (i && n->kind==AE_UNIT_SERVICE) return -EINVAL;
        if (n->edges[AE_UNIT_ON_FAILURE] || n->edges[AE_UNIT_ON_SUCCESS] || n->edges[AE_UNIT_UPHOLDS] ||
            n->edges[AE_UNIT_TRIGGERS] || n->edges[AE_UNIT_TRIGGERED_BY]) return -EINVAL;
        for (const char *s=n->name;*s;++s) if (!((*s>='a' && *s<='z') || (*s>='A' && *s<='Z') || (*s>='0' && *s<='9') || *s=='.' || *s=='-' || *s=='_')) return -EINVAL;
        const char *suffix=n->kind==AE_UNIT_SERVICE ? ".service" : n->kind==AE_UNIT_TARGET ? ".target" : ".slice";
        size_t len=strlen(n->name),tail=strlen(suffix); if (len<=tail || strcmp(n->name+len-tail,suffix)) return -EINVAL;
        for (unsigned int j=0;j<i;++j) if (!strcmp(n->name,p->nodes[j].name)) return -EEXIST;
        for (unsigned int j=0;j<AE_UNIT_EDGE_COUNT;++j) if ((unsigned int)n->edges[j]>>p->count) return -EINVAL;
        if (n->dropin_count>AE_UNIT_DROPINS) return -EINVAL;
        if (builtin(n)) { if (strcmp(n->name,n->kind==AE_UNIT_SYSTEM_SLICE ? "system.slice" : "-.slice") || n->dropin_count) return -EINVAL; continue; }
        if (!valid_text(n->directory,sizeof n->directory) || n->directory[0]!='/' || n->directory_fd<0 || n->file_fd<0 ||
            n->parent.uid || n->parent.gid || (n->parent.mode&0022)) return -EINVAL;
        for (unsigned int j=0;j<n->dropin_count;++j) {
            const struct ae_unit_dropin *d=&n->dropins[j];
            if (!valid_text(d->directory,sizeof d->directory) || d->directory[0]!='/' || d->directory_fd<0 || d->file_fd<0 ||
                d->parent.uid || d->parent.gid || (d->parent.mode&0022) || !valid_text(d->leaf,sizeof d->leaf) || !d->leaf[0] ||
                strchr(d->leaf,'/') || !strcmp(d->leaf,".") || !strcmp(d->leaf,"..")) return -EINVAL;
        }
        if (n->kind!=AE_UNIT_SERVICE) continue;
        if (!valid_text(n->executable,sizeof n->executable) || n->executable[0]!='/' || !n->argc || n->argc>AE_UNIT_ARGS || n->envc>AE_UNIT_ARGS ||
            !valid_text(n->label,sizeof n->label) || !n->label[0] || n->umask>0777 ||
            (n->no_new_privileges!=0 && n->no_new_privileges!=1) || (n->private_mounts!=0 && n->private_mounts!=1) ||
            (n->private_network!=0 && n->private_network!=1) || (n->private_devices!=0 && n->private_devices!=1)) return -EINVAL;
        for (unsigned int j=0;j<n->argc;++j) if (!valid_text(n->argv[j],sizeof n->argv[j])) return -EINVAL;
        for (unsigned int j=0;j<n->envc;++j) if (!valid_text(n->environment[j],sizeof n->environment[j])) return -EINVAL;
    }
    return 0;
}
static int dispose(struct ae_unit **out,int abandon)
{
    if (!out) return -EINVAL;
    struct ae_unit *u=*out; if (!u) return 0;
    int r=owner(u); if (!r && u->busy) r=-EBUSY;
    if (!r && !abandon && (u->status.subscribe_attempted || u->status.ref_attempted) && !u->status.retired) r=-EBUSY;
    if (r) return r;
    r=abandon ? ae_start_abandon(&u->start) : ae_start_close(&u->start); if (r) return r;
    sd_bus_slot_unref(u->names); ae_peer_clear(&u->manager);
    for (unsigned int i=0;i<u->profile.count;++i) if (!builtin(&u->profile.nodes[i])) {
        if (u->profile.nodes[i].file_fd>=0) close(u->profile.nodes[i].file_fd);
        if (u->profile.nodes[i].directory_fd>=0) close(u->profile.nodes[i].directory_fd);
        for (unsigned int j=0;j<u->profile.nodes[i].dropin_count;++j) {
            if (u->profile.nodes[i].dropins[j].file_fd>=0) close(u->profile.nodes[i].dropins[j].file_fd);
            if (u->profile.nodes[i].dropins[j].directory_fd>=0) close(u->profile.nodes[i].dropins[j].directory_fd);
        }
    }
    /* Do not push still-queued effects while abandoning local ownership. Data
     * already delivered remains an original unknown obligation, never rollback. */
    if (u->bus) sd_bus_close(u->bus);
    sd_bus_unref(u->bus); free(u); *out=NULL; return 0;
}
int ae_unit_match_profile(struct ae_unit *u,const uint8_t boot[16],const struct ae_unit_profile *p)
{
    int r=owner(u); if (!r && !boot) r=-EINVAL; if (!r) r=shape(p);
    if (r) return r;
    if (memcmp(u->boot,boot,16) || u->profile.count!=p->count) return -ESTALE;
    for (unsigned int i=0;i<p->count;++i) {
        const struct ae_unit_node *a=&u->profile.nodes[i],*b=&p->nodes[i];
        if (a->kind!=b->kind || strcmp(a->name,b->name) || memcmp(a->edges,b->edges,sizeof a->edges)) return -ESTALE;
        if (builtin(a)) continue;
        if (strcmp(a->directory,b->directory) || !ae_root_equal(&a->parent,&b->parent) ||
            !same_stat(&a->file.object,&b->file.object) || memcmp(a->file.digest,b->file.digest,32) || a->dropin_count!=b->dropin_count) return -ESTALE;
        for (unsigned int j=0;j<a->dropin_count;++j) {
            const struct ae_unit_dropin *x=&a->dropins[j],*y=&b->dropins[j];
            if (strcmp(x->directory,y->directory) || strcmp(x->leaf,y->leaf) || !ae_root_equal(&x->parent,&y->parent) ||
                !same_stat(&x->file.object,&y->file.object) || memcmp(x->file.digest,y->file.digest,32)) return -ESTALE;
        }
        if (a->kind!=AE_UNIT_SERVICE) continue;
        if (strcmp(a->executable,b->executable) || strcmp(a->label,b->label) || a->argc!=b->argc || a->envc!=b->envc ||
            a->capabilities!=b->capabilities || a->ambient!=b->ambient || a->umask!=b->umask ||
            a->no_new_privileges!=b->no_new_privileges || a->private_mounts!=b->private_mounts ||
            a->private_network!=b->private_network || a->private_devices!=b->private_devices) return -ESTALE;
        for (unsigned int j=0;j<a->argc;++j) if (strcmp(a->argv[j],b->argv[j])) return -ESTALE;
        for (unsigned int j=0;j<a->envc;++j) if (strcmp(a->environment[j],b->environment[j])) return -ESTALE;
    }
    return 0;
}
int ae_unit_new(sd_bus **bus,const struct ae_peer *manager,const uint8_t boot[16],const struct ae_unit_profile *p,struct ae_unit **out)
{
    if (!bus || !*bus || !manager || !boot || !out || *out || manager->pidfd<0 || manager->connection_pid!=1 || manager->connection_uid ||
        !valid_text(manager->unique_name,sizeof manager->unique_name) || manager->unique_name[0]!=':' ||
        !valid_text(manager->connection_label,sizeof manager->connection_label) || !manager->connection_label[0]) return -EINVAL;
    if (getuid() || geteuid() || getgid() || getegid()) return -EPERM;
    int r=shape(p); if (r) return r;
    struct ae_unit *u=calloc(1,sizeof *u); if (!u) return -ENOMEM;
    u->owner=getpid(); u->manager=*manager; u->manager.pidfd=-1; memcpy(u->boot,boot,16); u->profile=*p;
    for (unsigned int i=0;i<p->count;++i) {
        u->profile.nodes[i].file_fd=-1; u->profile.nodes[i].directory_fd=-1;
        for (unsigned int j=0;j<p->nodes[i].dropin_count;++j) { u->profile.nodes[i].dropins[j].file_fd=-1; u->profile.nodes[i].dropins[j].directory_fd=-1; }
    }
    u->manager.pidfd=fcntl(manager->pidfd,F_DUPFD_CLOEXEC,3); if (u->manager.pidfd<0) r=-errno;
    for (unsigned int i=0;!r && i<p->count;++i) if (!builtin(&p->nodes[i])) {
        u->profile.nodes[i].directory_fd=fcntl(p->nodes[i].directory_fd,F_DUPFD_CLOEXEC,3);
        if (u->profile.nodes[i].directory_fd<0) r=-errno;
        if (!r) { u->profile.nodes[i].file_fd=fcntl(p->nodes[i].file_fd,F_DUPFD_CLOEXEC,3); if (u->profile.nodes[i].file_fd<0) r=-errno; }
        for (unsigned int j=0;!r && j<p->nodes[i].dropin_count;++j) {
            struct ae_unit_dropin *d=&u->profile.nodes[i].dropins[j]; const struct ae_unit_dropin *original=&p->nodes[i].dropins[j];
            d->directory_fd=fcntl(original->directory_fd,F_DUPFD_CLOEXEC,3); if (d->directory_fd<0) r=-errno;
            if (!r) { d->file_fd=fcntl(original->file_fd,F_DUPFD_CLOEXEC,3); if (d->file_fd<0) r=-errno; }
        }
    }
    u->bus=sd_bus_ref(*bus); if (!r) r=sources(u); if (!r) r=current(u);
    if (!r) r=sd_bus_match_signal(u->bus,&u->names,"org.freedesktop.DBus","/org/freedesktop/DBus","org.freedesktop.DBus","NameOwnerChanged",names,u);
    if (r>=0) r=sd_bus_set_method_call_timeout(u->bus,UINT64_C(2000000));
    if (r<0) { sd_bus_unref(u->bus); u->bus=NULL; (void)ae_unit_abandon(&u); return r; }
    sd_bus_unref(*bus); *bus=NULL; *out=u; return 0;
}
int ae_unit_acquire(struct ae_unit *u,ae_start_guard guard,void *data)
{
    int r=owner(u); if (!r && !guard) r=-EINVAL; if (!r && u->busy) r=-EBUSY;
    if (!r && u->status.used) r=-EALREADY;
    if (r) return r;
    u->busy=1; u->status.used=1; r=sources(u);
    if (!r) r=call(u,"Subscribe",NULL,&u->status.subscribe_attempted,guard,data);
    if (!r) u->status.subscribed=1;
    for (unsigned int i=0;!r && i<u->profile.count;++i) {
        int attempted=0; r=call(u,"RefUnit",u->profile.nodes[i].name,&attempted,guard,data);
        if (attempted) u->status.ref_attempted|=(uint8_t)(1U<<i);
        if (!r) { u->status.referenced|=(uint8_t)(1U<<i); r=lookup(u,i,1); }
    }
    if (!r) r=check(u);
    if (!r) u->status.ready=1; else if (!u->status.quarantined) fail(u,r,0);
    u->busy=0; return r;
}
int ae_unit_check_fresh(struct ae_unit *u)
{
    int r=owner(u); if (!r && u->busy) r=-EBUSY;
    if (!r && (!u->status.ready || u->status.retire_used)) r=-EAGAIN;
    if (r) return r;
    u->busy=1; r=check(u); if (r) fail(u,r,0); u->busy=0; return r;
}
struct start_guard { struct ae_unit *unit; ae_start_guard guard; void *data; };
static int before_start(void *data)
{
    struct start_guard *g=data; struct ae_unit *u=g->unit;
    int r=guarded(u,g->guard,g->data); if (!r) r=check(u);
    if (!r) r=guarded(u,g->guard,g->data);
    if (!r) r=sources(u);
    if (r && !u->status.quarantined) fail(u,r,0);
    return r;
}
static void start_changed(void *data,const struct ae_start_status *status)
{
    struct ae_unit *u=data; if (owner(u)) return;
    u->status.start=*status;
    if (u->changed) { int previous=u->busy; u->busy=1; u->changed(u->observer,status); u->busy=previous; }
}
int ae_unit_start(struct ae_unit *u,ae_start_guard guard,void *data,ae_start_changed changed,void *observer)
{
    int r=owner(u); if (!r && !guard) r=-EINVAL; if (!r && u->busy) r=-EBUSY;
    if (!r && u->status.start_used) r=-EALREADY;
    if (!r && (!u->status.ready || u->status.retire_used)) r=-EAGAIN;
    if (r) return r;
    u->busy=1; u->status.start_used=1; u->changed=changed; u->observer=observer;
    r=ae_start_new(u->bus,&u->manager,u->boot,u->profile.nodes[0].name,start_changed,u,&u->start);
    struct start_guard g={.unit=u,.guard=guard,.data=data}; if (!r) r=ae_start_request(u->start,before_start,&g);
    if (u->start) (void)ae_start_status(u->start,&u->status.start);
    u->busy=0; return r;
}
sd_bus *ae_unit_bus(struct ae_unit *u) { return owner(u)==0 ? u->bus : NULL; }
int ae_unit_status(struct ae_unit *u,struct ae_unit_status *out)
{ int r=owner(u); if (!r && !out) r=-EINVAL; if (!r) { if (u->start) r=ae_start_status(u->start,&u->status.start); if (!r) *out=u->status; } return r; }
int ae_unit_retire(struct ae_unit *u,ae_start_guard guard,void *data)
{
    int r=owner(u); if (!r && !guard) r=-EINVAL; if (!r && u->busy) r=-EBUSY;
    if (!r && u->status.retired) return 0;
    if (!r && (u->status.retire_used || u->status.quarantined || !u->status.subscribed ||
        u->status.referenced!=(uint8_t)((1U<<u->profile.count)-1U))) r=-EAGAIN;
    if (r) return r;
    u->busy=1; r=guarded(u,guard,data);
    for (unsigned int i=0;!r && i<u->profile.count;++i) { r=basic(u,i); if (!r) r=has_reference(u,i); if (!r) r=quiet(u,i,0); }
    if (!r) r=guarded(u,guard,data);
    if (!r) r=ae_start_close(&u->start);
    if (!r) {
        u->status.retire_used=1; u->status.ready=0;
        for (unsigned int j=u->profile.count;!r && j>0;--j) {
            unsigned int i=j-1; int attempted=0; r=call(u,"UnrefUnit",u->profile.nodes[i].name,&attempted,guard,data);
            if (attempted) u->status.unref_attempted|=(uint8_t)(1U<<i);
            if (!r) u->status.referenced&=(uint8_t)~(1U<<i);
        }
        if (!r) r=call(u,"Unsubscribe",NULL,&u->status.unsubscribe_attempted,guard,data);
        if (!r) { u->status.retired=1; u->status.subscribed=0; u->status.ready=0; u->names=sd_bus_slot_unref(u->names); }
    }
    u->busy=0; return r;
}
int ae_unit_close(struct ae_unit **u) { return dispose(u,0); }
int ae_unit_abandon(struct ae_unit **u) { return dispose(u,1); }
