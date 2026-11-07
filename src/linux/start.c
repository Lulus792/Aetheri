#include "linux/start.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <systemd/sd-id128.h>
#include <unistd.h>

#define MANAGER "org.freedesktop.systemd1"
#define PATH "/org/freedesktop/systemd1"
struct removed { uint32_t id; char path[AE_START_JOB_PATH],result[AE_START_RESULT]; };
struct ae_start {
    pid_t owner;
    int busy;
    sd_bus *bus;
    struct ae_peer manager;
    uint8_t boot[16];
    char unit[256];
    sd_bus_message *request;
    sd_bus_slot *reply,*jobs,*names;
    struct ae_start_status status;
    struct removed removed[4];
    ae_start_changed changed;
    void *observer;
};
static int owner(struct ae_start *s) { return !s ? -EINVAL : s->owner!=getpid() ? -ECHILD : 0; }
static void quarantine(struct ae_start *s,int error)
{ s->status.quarantined=1; if (!s->status.error) s->status.error=error; }
static void changed(struct ae_start *s)
{ if (s->changed) s->changed(s->observer,&s->status); }
static int job_id(const char *path,uint32_t *output)
{
    const char prefix[]="/org/freedesktop/systemd1/job/";
    if (!path || strncmp(path,prefix,sizeof prefix-1)!=0) return -EBADMSG;
    const char *p=path+sizeof prefix-1; uint32_t value=0;
    if (*p<'1' || *p>'9') return -EBADMSG;
    for (;*p;++p) {
        if (*p<'0' || *p>'9' || value>(UINT32_MAX-(uint32_t)(*p-'0'))/10U) return -EBADMSG;
        value=value*10U+(uint32_t)(*p-'0');
    }
    *output=value; return 0;
}
static int same_manager(struct ae_start *s,const struct ae_peer *peer)
{
    return peer->connection_uid==0 && peer->connection_pid==1 &&
        memcmp(peer->bus_id,s->manager.bus_id,16)==0 &&
        strcmp(peer->unique_name,s->manager.unique_name)==0 &&
        strcmp(peer->connection_label,s->manager.connection_label)==0 && ae_peer_alive(&s->manager)==0 ? 0 : -ESTALE;
}
static int current(struct ae_start *s)
{
    if (s->status.quarantined) return -ESTALE;
    sd_id128_t boot,bus; int r=sd_id128_get_boot(&boot);
    if (r>=0 && memcmp(boot.bytes,s->boot,16)!=0) r=-ESTALE;
    if (r>=0) r=sd_bus_get_bus_id(s->bus,&bus);
    if (r>=0 && memcmp(bus.bytes,s->manager.bus_id,16)!=0) r=-ESTALE;
    if (r>=0) r=ae_peer_alive(&s->manager);
    sd_bus_creds *credentials=NULL; const char *unique=NULL;
    if (r>=0) r=sd_bus_get_name_creds(s->bus,MANAGER,SD_BUS_CREDS_UNIQUE_NAME,&credentials);
    if (r>=0) r=sd_bus_creds_get_unique_name(credentials,&unique);
    if (r>=0 && (!unique || strcmp(unique,s->manager.unique_name)!=0)) r=-ESTALE;
    sd_bus_creds_unref(credentials);
    if (r>=0 && s->status.quarantined) r=-ESTALE;
    if (r<0) quarantine(s,r);
    return r<0 ? r : 0;
}
static void reconcile(struct ae_start *s)
{
    if (s->status.quarantined || !s->status.reply_confirmed) return;
    for (unsigned int n=0;n<s->status.buffered;++n) if (strcmp(s->removed[n].path,s->status.job)==0) {
        if (s->removed[n].id!=s->status.job_id) { quarantine(s,-ESTALE); return; }
        s->status.job_removed=1; strcpy(s->status.result,s->removed[n].result);
        s->jobs=sd_bus_slot_unref(s->jobs); s->names=sd_bus_slot_unref(s->names); return;
    }
}
static int removed(sd_bus_message *m,void *userdata,sd_bus_error *error)
{
    (void)error; struct ae_start *s=userdata;
    if (owner(s)!=0) return 0;
    if (s->busy) { quarantine(s,-EBUSY); return 0; } s->busy=1;
    uint32_t id=0; const char *path=NULL,*unit=NULL,*result=NULL;
    int r=sd_bus_message_has_signature(m,"uoss") ? 0 : -EBADMSG;
    if (r==0 && (sd_bus_message_read(m,"uoss",&id,&path,&unit,&result)<=0 || sd_bus_message_at_end(m,1)!=1)) r=-EBADMSG;
    if (r==0 && strcmp(unit,s->unit)!=0) { s->busy=0; return 0; }
    struct ae_peer peer=AE_PEER_INIT;
    if (r==0) r=ae_peer_capture_signal(m,&peer);
    if (r==0) r=same_manager(s,&peer);
    uint32_t decoded=0;
    if (r==0) r=job_id(path,&decoded);
    if (r==0 && (decoded!=id || strnlen(path,AE_START_JOB_PATH)>=AE_START_JOB_PATH ||
        !result[0] || strnlen(result,AE_START_RESULT)>=AE_START_RESULT)) r=-EBADMSG;
    if (r==0 && !s->status.attempted) r=-ESTALE;
    unsigned int n=0;
    for (;r==0 && n<s->status.buffered;++n) if (strcmp(s->removed[n].path,path)==0) {
        if (s->removed[n].id!=id || strcmp(s->removed[n].result,result)!=0) r=-ESTALE;
        break;
    }
    if (r==0 && n==s->status.buffered) {
        if (n==4) r=-ENOSPC;
        else { s->removed[n].id=id; strcpy(s->removed[n].path,path); strcpy(s->removed[n].result,result); ++s->status.buffered; }
    }
    ae_peer_clear(&peer);
    if (r<0) quarantine(s,r); else reconcile(s);
    changed(s); s->busy=0; return 0;
}
static int name_changed(sd_bus_message *m,void *userdata,sd_bus_error *error)
{
    (void)error; struct ae_start *s=userdata; const char *name=NULL,*old=NULL,*now=NULL;
    if (owner(s)!=0) return 0;
    int r=sd_bus_message_read(m,"sss",&name,&old,&now);
    if (r>0 && strcmp(name,MANAGER)!=0) return 0;
    if (r<=0 || sd_bus_message_at_end(m,1)!=1 || strcmp(old,s->manager.unique_name)==0 || strcmp(now,s->manager.unique_name)!=0) quarantine(s,-ESTALE);
    /* Never destroy an observer from a nested bus dispatch. */
    if (!s->busy) { s->busy=1; changed(s); s->busy=0; }
    return 0;
}
static int reply(sd_bus_message *m,void *userdata,sd_bus_error *error)
{
    (void)error; struct ae_start *s=userdata;
    if (owner(s)!=0) return 1;
    if (s->busy) { quarantine(s,-EBUSY); return 1; } s->busy=1;
    s->status.pending=0;
    int r=sd_bus_message_is_method_error(m,NULL) ? -sd_bus_message_get_errno(m) : 0;
    struct ae_peer peer=AE_PEER_INIT; const char *path=NULL; uint32_t id=0;
    if (r==0) r=ae_peer_capture_reply(m,s->request,&peer);
    if (r==0) r=same_manager(s,&peer);
    if (r==0 && (!sd_bus_message_has_signature(m,"o") || sd_bus_message_read(m,"o",&path)<=0 || sd_bus_message_at_end(m,1)!=1)) r=-EBADMSG;
    if (r==0) r=job_id(path,&id);
    if (r==0 && strnlen(path,AE_START_JOB_PATH)>=AE_START_JOB_PATH) r=-E2BIG;
    if (r==0) { s->status.reply_confirmed=1; s->status.job_id=id; strcpy(s->status.job,path); reconcile(s); }
    else quarantine(s,r);
    ae_peer_clear(&peer); s->reply=sd_bus_slot_unref(s->reply);
    changed(s); s->busy=0; return 1;
}
int ae_start_new(sd_bus *bus,const struct ae_peer *manager,const uint8_t boot[16],const char *unit,ae_start_changed callback,void *userdata,struct ae_start **output)
{
    if (!bus || !manager || !boot || !unit || !output || *output || manager->pidfd<0 || manager->connection_pid!=1 || manager->connection_uid!=0) return -EINVAL;
    if (getuid()!=0 || geteuid()!=0 || getgid()!=0 || getegid()!=0) return -EPERM;
    size_t length=strnlen(unit,256); if (!length || length==256 || manager->unique_name[0]!=':' ||
        !memchr(manager->unique_name,0,sizeof manager->unique_name) || !manager->connection_label[0] || !memchr(manager->connection_label,0,sizeof manager->connection_label)) return -EINVAL;
    for (size_t n=0;n<length;++n) if (!((unit[n]>='a' && unit[n]<='z') || (unit[n]>='A' && unit[n]<='Z') ||
        (unit[n]>='0' && unit[n]<='9') || unit[n]=='_' || unit[n]=='-' || unit[n]=='.')) return -EINVAL;
    struct ae_start *s=calloc(1,sizeof *s); if (!s) return -ENOMEM;
    s->owner=getpid(); s->bus=sd_bus_ref(bus); s->manager=*manager; s->manager.pidfd=fcntl(manager->pidfd,F_DUPFD_CLOEXEC,3);
    int r=s->manager.pidfd<0 ? -errno : 0;
    strcpy(s->unit,unit); memcpy(s->boot,boot,16); s->changed=callback; s->observer=userdata;
    if (r==0) r=current(s);
    if (r==0) r=sd_bus_match_signal(bus,&s->jobs,manager->unique_name,PATH,MANAGER ".Manager","JobRemoved",removed,s);
    if (r>=0) r=sd_bus_match_signal(bus,&s->names,"org.freedesktop.DBus","/org/freedesktop/DBus","org.freedesktop.DBus","NameOwnerChanged",name_changed,s);
    if (r<0) { (void)ae_start_abandon(&s); return r; }
    *output=s; return 0;
}
int ae_start_request(struct ae_start *s,ae_start_guard guard,void *userdata)
{
    int r=owner(s); if (r==0 && !guard) r=-EINVAL;
    if (r==0 && s->busy) r=-EBUSY;
    if (r==0 && s->status.used) r=-EALREADY;
    if (r!=0) return r;
    s->busy=1; s->status.used=1;
    r=current(s); if (r==0) r=guard(userdata);
    if (r>0) r=-EIO;
    if (r==0) r=sd_bus_message_new_method_call(s->bus,&s->request,s->manager.unique_name,PATH,MANAGER ".Manager","StartUnit");
    if (r>=0) r=sd_bus_message_append(s->request,"ss",s->unit,"fail");
    if (r>=0) r=current(s);
    if (r==0) r=guard(userdata);
    if (r>0) r=-EIO;
    if (r==0 && s->status.quarantined) r=-ESTALE;
    if (r==0) {
        s->status.attempted=1; s->status.pending=1;
        r=sd_bus_call_async(s->bus,&s->reply,s->request,reply,s,UINT64_C(2000000));
        (void)sd_bus_message_get_cookie(s->request,&s->status.cookie);
        if (r<0) { s->status.pending=0; quarantine(s,r); }
    }
    if (r<0 && !s->status.error) s->status.error=r;
    s->busy=0; return r<0 ? r : 0;
}
int ae_start_status(struct ae_start *s,struct ae_start_status *out)
{ int r=owner(s); if (r==0 && !out) r=-EINVAL; if (r==0) *out=s->status; return r; }
static int close_start(struct ae_start **out,int abandon)
{
    if (!out) return -EINVAL;
    struct ae_start *s=*out; if (!s) return 0;
    int r=owner(s); if (r==0 && s->busy) r=-EBUSY;
    if (r==0 && !abandon && s->status.attempted &&
        (s->status.quarantined || !s->status.reply_confirmed || !s->status.job_removed)) r=-EBUSY;
    if (r!=0) return r;
    sd_bus_slot_unref(s->reply); sd_bus_slot_unref(s->jobs); sd_bus_slot_unref(s->names);
    sd_bus_message_unref(s->request); ae_peer_clear(&s->manager); sd_bus_unref(s->bus); free(s); *out=NULL; return 0;
}
int ae_start_close(struct ae_start **s) { return close_start(s,0); }
int ae_start_abandon(struct ae_start **s) { return close_start(s,1); }
