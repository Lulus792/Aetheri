#include "authority/handoff.h"
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <systemd/sd-daemon.h>
#include <unistd.h>

struct ae_handoff_service {
    pid_t owner;
    int busy,stale,received;
    struct ae_catalogue *store;
    sd_bus *bus;
    struct ae_execution_profile profile;
    struct ae_process_context self;
    uint8_t runtime[16];
};
static int current(struct ae_handoff_service *s)
{
    if (s->stale) return -ESTALE;
    struct ae_process_context actual; struct ae_catalogue_info info;
    int r=ae_process_observe_self(s->bus,&actual);
    if (r==0 && !ae_process_context_equal(&actual,&s->self)) r=-ESTALE;
    if (r==0) r=ae_catalogue_get_info(s->store,&info);
    if (r==0 && (memcmp(info.authority_epoch,s->profile.system_epoch,16)!=0 ||
        memcmp(info.boot_id,s->profile.boot,16)!=0 || memcmp(info.runtime_incarnation,s->runtime,16)!=0)) r=-ESTALE;
    if (r==-ESTALE) s->stale=1;
    return r;
}
static int enter(struct ae_handoff_service *s)
{
    if (!s) return -EINVAL;
    if (s->owner!=getpid()) return -ECHILD;
    if (s->busy) return -EBUSY;
    if (getuid()!=0 || geteuid()!=0 || getgid()!=0 || getegid()!=0) return -EPERM;
    if (s->stale) return -ESTALE;
    s->busy=1; return 0;
}
int ae_handoff_service_new(struct ae_catalogue *store,sd_bus *bus,const struct ae_execution_profile *p,
    const struct ae_process_context *self,struct ae_handoff_service **output)
{
    if (!store || !bus || !p || !self || !output || *output) return -EINVAL;
    if (getuid()!=0 || geteuid()!=0 || getgid()!=0 || getegid()!=0) return -EPERM;
    if (self->uid!=0 || memcmp(self->boot,p->boot,16)!=0 ||
        !self->unit[0] || !memchr(self->unit,0,sizeof self->unit) ||
        !self->label[0] || !memchr(self->label,0,sizeof self->label) ||
        !self->cgroup[0] || !memchr(self->cgroup,0,sizeof self->cgroup)) return -ESTALE;
    uint8_t encoded[AE_EXECUTION_BYTES]; int r=ae_execution_encode(p,encoded);
    struct ae_catalogue_info info; if (r==0) r=ae_catalogue_get_info(store,&info);
    if (r!=0) return r;
    struct ae_handoff_service *s=calloc(1,sizeof *s); if (!s) return -ENOMEM;
    s->owner=getpid(); s->store=store; s->bus=bus; s->profile=*p; s->self=*self;
    memcpy(s->runtime,info.runtime_incarnation,16);
    r=current(s); if (r!=0) { free(s); return r; }
    *output=s; return 0;
}
static void handles_close(int handles[AE_LIFETIME_HANDLE_COUNT])
{ for (int n=0;n<AE_LIFETIME_HANDLE_COUNT;++n) if (handles[n]>=0) { close(handles[n]); handles[n]=-1; } }
static struct ae_lifetime_checkpoint checkpoint(const struct ae_execution_handoff *h)
{
    const struct ae_execution_process *p=&h->process;
    struct ae_lifetime_checkpoint v={.root=h->root,.group=h->group,.pid_namespace=h->pid_namespace,
        .cgroup_namespace=h->cgroup_namespace,.events_inode=h->events_inode,.events_mount=h->events_mount,
        .events_mode=h->events_mode,.events_uid=h->events_uid,.events_gid=h->events_gid,.pid=p->pid};
    v.process.pidfd_id=p->pidfd_id; memcpy(v.process.boot,p->boot,16);
    v.context.uid=(uid_t)p->uid; v.context.cgroup_id=p->cgroup_id;
    memcpy(v.context.boot,p->boot,16); memcpy(v.context.invocation,p->invocation,16);
    strcpy(v.context.unit,p->unit); strcpy(v.context.label,p->label); strcpy(v.context.cgroup,p->cgroup);
    return v;
}
int ae_handoff_service_prepare(struct ae_handoff_service *s,struct ae_lifetime *original,int64_t stamp,struct ae_handoff_transfer *output)
{
    int r=enter(s); if (r!=0) return r;
    if (!original || !output || stamp<0) r=-EINVAL;
    if (r==0) for (int n=0;n<AE_LIFETIME_HANDLE_COUNT;++n) if (output->handles[n]!=-1) r=-EINVAL;
    if (r==0 && memcmp(s->runtime,s->profile.system_runtime,16)!=0) r=-ESTALE;
    if (r==0) r=current(s);
    struct ae_execution_process_snapshot stored;
    if (r==0) r=ae_catalogue_get_execution_process(s->store,&s->profile,AE_EXECUTION_APP,&stored);
    struct ae_lifetime_checkpoint actual;
    struct ae_handoff_transfer result=AE_HANDOFF_TRANSFER_INIT;
    if (r==0) r=ae_lifetime_export(original,&actual,result.handles);
    struct ae_execution_handoff h={0};
    if (r==0) {
        h.process=stored.process; h.root=actual.root; h.group=actual.group;
        h.pid_namespace=actual.pid_namespace; h.cgroup_namespace=actual.cgroup_namespace;
        h.events_inode=actual.events_inode; h.events_mount=actual.events_mount;
        h.events_mode=actual.events_mode; h.events_uid=actual.events_uid; h.events_gid=actual.events_gid;
        strcpy(h.custodian,s->self.unit); memcpy(h.custodian_invocation,s->self.invocation,16);
        struct ae_lifetime_checkpoint expected=checkpoint(&h);
        if (actual.pid!=expected.pid || actual.process.pidfd_id!=expected.process.pidfd_id ||
            memcmp(actual.process.boot,expected.process.boot,16)!=0 || !ae_process_context_equal(&actual.context,&expected.context)) r=-ESTALE;
    }
    if (r==0) r=current(s);
    if (r==0) r=ae_catalogue_handoff(s->store,&s->profile,AE_HANDOFF_PREPARE,&h,stamp,&result.journal);
    if (r!=0 || !result.journal.transfer_required) handles_close(result.handles);
    if (r==0) *output=result;
    s->busy=0; return r;
}
int ae_handoff_service_restore(struct ae_handoff_service *s,const int *handles,size_t count,int64_t stamp,
    struct ae_lifetime **output,struct ae_handoff_snapshot *snapshot)
{
    int r=enter(s); if (r!=0) return r;
    if (!handles || count!=AE_LIFETIME_HANDLE_COUNT || stamp<0 || !output || *output || !snapshot) r=-EINVAL;
    for (size_t n=0;r==0 && n<count;++n) {
        if (handles[n]<0) { r=-EINVAL; break; }
        int flags=fcntl(handles[n],F_GETFD); if (flags<0) r=-errno; else if (!(flags&FD_CLOEXEC)) r=-EINVAL;
        for (size_t m=0;m<n;++m) if (handles[m]==handles[n]) r=-EINVAL;
    }
    if (r==0 && memcmp(s->runtime,s->profile.system_runtime,16)==0) r=-ESTALE;
    if (r==0) r=current(s);
    struct ae_handoff_snapshot original;
    if (r==0) r=ae_catalogue_get_handoff(s->store,&s->profile,&original);
    if (r==0 && (original.state>AE_HANDOFF_OBSERVED || strcmp(original.original.custodian,s->self.unit)!=0 ||
        memcmp(original.original.custodian_invocation,s->self.invocation,16)==0)) r=-ESTALE;
    struct ae_lifetime *observer=NULL;
    if (r==0) {
        struct ae_lifetime_checkpoint expected=checkpoint(&original.original);
        r=ae_lifetime_restore(s->bus,&expected,handles,&observer);
    }
    if (r==0) r=current(s);
    struct ae_handoff_result result;
    if (r==0) r=ae_catalogue_handoff(s->store,&s->profile,AE_HANDOFF_OBSERVE,&original.original,stamp,&result);
    if (r==0) { *output=observer; *snapshot=result.snapshot; }
    else (void)ae_lifetime_close(&observer);
    s->busy=0; return r;
}
static int names(struct ae_handoff_service *s,char out[4][AE_HANDOFF_NAME_BYTES])
{
    uint8_t hash[32]; int r=ae_execution_digest(&s->profile,hash); if (r!=0) return r;
    char hex[65]; const char digits[]="0123456789abcdef";
    for (size_t n=0;n<32;++n) { hex[2*n]=digits[hash[n]>>4]; hex[2*n+1]=digits[hash[n]&15]; } hex[64]=0;
    const char *slots[]={"process","anchor","group","events"};
    for (size_t n=0;n<4;++n) {
        int size=snprintf(out[n],AE_HANDOFF_NAME_BYTES,"aetheri-handoff1-%s-%s",hex,slots[n]);
        if (size<0 || size>=AE_HANDOFF_NAME_BYTES) return -EOVERFLOW;
    }
    return 0;
}
static int store_profile(struct ae_handoff_service *s,uint32_t *output)
{
    int r=current(s); char *path=NULL;
    if (r==0) r=sd_bus_path_encode("/org/freedesktop/systemd1/unit",s->self.unit,&path);
    const char *keys[]={"NotifyAccess","FileDescriptorStorePreserve"},*values[]={"main","yes"};
    for (size_t n=0;r>=0 && n<2;++n) {
        char *value=NULL; r=sd_bus_get_property_string(s->bus,"org.freedesktop.systemd1",path,"org.freedesktop.systemd1.Service",keys[n],NULL,&value);
        if (r>=0 && strcmp(value,values[n])!=0) r=-ESTALE;
        free(value);
    }
    uint32_t maximum=0,count=0;
    if (r>=0) r=sd_bus_get_property_trivial(s->bus,"org.freedesktop.systemd1",path,"org.freedesktop.systemd1.Service","FileDescriptorStoreMax",NULL,'u',&maximum);
    if (r>=0) r=sd_bus_get_property_trivial(s->bus,"org.freedesktop.systemd1",path,"org.freedesktop.systemd1.Service","NFileDescriptorStore",NULL,'u',&count);
    free(path);
    if (r>=0 && (maximum!=4 || count>4)) r=-ESTALE;
    if (r>=0) r=current(s);
    if (r==-ESTALE) s->stale=1;
    if (r>=0) { *output=count; return 0; }
    return r;
}
static int processed(void)
{ int r=sd_notify_barrier(0,UINT64_C(2000000)); return r>0 ? 0 : r<0 ? r : -ENOTCONN; }
int ae_handoff_service_send(struct ae_handoff_service *s,struct ae_lifetime *original,int64_t stamp,struct ae_handoff_snapshot *output)
{
    int r=enter(s); if (r!=0) return r;
    if (!original || !output || stamp<0) r=-EINVAL;
    if (r==0 && memcmp(s->runtime,s->profile.system_runtime,16)!=0) r=-ESTALE;
    if (r==0) r=current(s);
    struct ae_handoff_snapshot existing;
    if (r==0) { r=ae_catalogue_get_handoff(s->store,&s->profile,&existing); if (r==0) r=-EALREADY; else if (r==-ENOENT) r=0; }
    uint32_t count=0; if (r==0) r=store_profile(s,&count);
    if (r==0 && count) r=-EEXIST;
    s->busy=0;
    if (r!=0) return r;
    struct ae_handoff_transfer transfer=AE_HANDOFF_TRANSFER_INIT;
    r=ae_handoff_service_prepare(s,original,stamp,&transfer);
    if (r==0 && !transfer.journal.transfer_required) r=-EALREADY;
    if (r==0) r=enter(s);
    if (r==0) {
        char labels[4][AE_HANDOFF_NAME_BYTES]; r=names(s,labels);
        if (r==0) r=store_profile(s,&count);
        if (r==0 && count) r=-EEXIST;
        for (size_t n=0;r==0 && n<4;++n) {
            char message[160]; int size=snprintf(message,sizeof message,"FDSTORE=1\nFDPOLL=0\nFDNAME=%s",labels[n]);
            if (size<0 || size>=(int)sizeof message) r=-EOVERFLOW;
            else { int sent=sd_pid_notify_with_fds(0,0,message,&transfer.handles[n],1); r=sent>0 ? 0 : sent<0 ? sent : -ENOTCONN; }
        }
        if (r==0) r=processed();
        if (r==0) r=store_profile(s,&count);
        if (r==0 && count!=4) r=-EAGAIN;
        if (r==0) *output=transfer.journal.snapshot;
        s->busy=0;
    }
    handles_close(transfer.handles); return r;
}
int ae_handoff_service_receive(struct ae_handoff_service *s,const int *fds,const char *const *labels,size_t count,int64_t stamp,
    struct ae_lifetime **output,struct ae_handoff_snapshot *snapshot)
{
    int r=enter(s); if (r!=0) return r;
    if (!fds || !labels || count!=4 || !output || *output || !snapshot || stamp<0) r=-EINVAL;
    char expected[4][AE_HANDOFF_NAME_BYTES]; if (r==0) r=names(s,expected);
    int ordered[4]={-1,-1,-1,-1};
    for (size_t n=0;r==0 && n<count;++n) {
        if (!labels[n] || strnlen(labels[n],AE_HANDOFF_NAME_BYTES)==AE_HANDOFF_NAME_BYTES || fds[n]<0) { r=-EINVAL; break; }
        size_t slot=0; while (slot<4 && strcmp(labels[n],expected[slot])!=0) ++slot;
        if (slot==4 || ordered[slot]>=0) r=-ESTALE; else ordered[slot]=fds[n];
    }
    uint32_t held=0; if (r==0) r=store_profile(s,&held);
    if (r==0 && held!=4) r=-EAGAIN;
    s->busy=0; if (r!=0) return r;
    struct ae_lifetime *observer=NULL; struct ae_handoff_snapshot original;
    r=ae_handoff_service_restore(s,ordered,4,stamp,&observer,&original);
    if (r==0) { s->received=1; *output=observer; *snapshot=original; }
    return r;
}
int ae_handoff_service_retire_store(struct ae_handoff_service *s,int64_t stamp,struct ae_handoff_snapshot *output)
{
    int r=enter(s); if (r!=0) return r;
    if (!output || stamp<0) r=-EINVAL;
    if (r==0) r=current(s);
    struct ae_handoff_snapshot original;
    if (r==0) r=ae_catalogue_get_handoff(s->store,&s->profile,&original);
    if (r==0 && original.state==AE_HANDOFF_FINISHED) { *output=original; s->busy=0; return 0; }
    if (r==0 && (!s->received || original.state<AE_HANDOFF_OBSERVED)) r=-EACCES;
    uint32_t count=0; if (r==0) r=store_profile(s,&count);
    if (r==0 && original.state==AE_HANDOFF_OBSERVED && count!=4) r=-EAGAIN;
    struct ae_handoff_result result;
    if (r==0) r=ae_catalogue_handoff(s->store,&s->profile,AE_HANDOFF_RETIRE,&original.original,stamp,&result);
    if (r==0 && result.retire_required) {
        char labels[4][AE_HANDOFF_NAME_BYTES]; r=names(s,labels);
        for (size_t n=0;r==0 && n<4;++n) {
            char message[160]; int size=snprintf(message,sizeof message,"FDSTOREREMOVE=1\nFDNAME=%s",labels[n]);
            if (size<0 || size>=(int)sizeof message) r=-EOVERFLOW;
            else { int sent=sd_notify(0,message); r=sent>0 ? 0 : sent<0 ? sent : -ENOTCONN; }
        }
        if (r==0) r=processed();
    }
    if (r==0) r=store_profile(s,&count);
    if (r==0 && count) r=-EAGAIN; /* No repeat removal after a lost/partial effect. */
    if (r==0) r=ae_catalogue_handoff(s->store,&s->profile,AE_HANDOFF_RETIRED,&original.original,stamp,&result);
    if (r==0) *output=result.snapshot;
    s->busy=0; return r;
}
int ae_handoff_service_close(struct ae_handoff_service **output)
{
    if (!output) return -EINVAL;
    struct ae_handoff_service *s=*output; if (!s) return 0;
    if (s->owner!=getpid()) return -ECHILD;
    if (s->busy) return -EBUSY;
    free(s); *output=NULL; return 0;
}
