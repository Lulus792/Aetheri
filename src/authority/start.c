#include "authority/start.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct ae_supervisor_start {
    pid_t owner;
    int busy,used,hold,error;
    struct ae_catalogue *catalogue;
    struct ae_execution_profile profile;
    struct ae_launch_context broker;
    uint8_t digest[32];
    char unit[80];
    struct ae_start *start;
    ae_supervisor_admit admit;
    void *admission;
};
static struct ae_effect_owner owner_context(struct ae_supervisor_start *s)
{
    struct ae_effect_owner owner;
    memcpy(owner.epoch,s->profile.system_epoch,16); memcpy(owner.boot,s->profile.boot,16); memcpy(owner.runtime,s->profile.system_runtime,16);
    return owner;
}
static int owner(struct ae_supervisor_start *s)
{ return !s ? -EINVAL : s->owner!=getpid() ? -ECHILD : 0; }
static void release_hold(struct ae_supervisor_start *s)
{
    if (s->hold) {
        int r=ae_catalogue_end_execution_effect(s->catalogue);
        if (r==0) s->hold=0; else if (!s->error) s->error=r;
    }
}
static void changed(void *userdata,const struct ae_start_status *status)
{
    struct ae_supervisor_start *s=userdata;
    if (!status->quarantined && status->reply_confirmed && status->job_removed && !status->pending) release_hold(s);
}
static int guard(void *userdata)
{
    struct ae_supervisor_start *s=userdata;
    int r=ae_catalogue_check_execution_effect(s->catalogue,&s->broker);
    if (r==0) r=s->admit(s->admission,&s->profile,s->unit);
    if (r>0) r=-EIO;
    if (r==0) r=ae_catalogue_check_execution_effect(s->catalogue,&s->broker);
    return r;
}
int ae_supervisor_start_new(struct ae_catalogue *store,sd_bus *bus,const struct ae_peer *manager,const struct ae_execution_profile *p,
    const struct ae_launch_context *context,ae_supervisor_admit admit,void *userdata,struct ae_supervisor_start **output)
{
    if (!store || !bus || !manager || !p || !context || !admit || !output || *output) return -EINVAL;
    if (getuid()!=0 || geteuid()!=0 || getgid()!=0 || getegid()!=0) return -EPERM;
    struct ae_supervisor_start *s=calloc(1,sizeof *s); if (!s) return -ENOMEM;
    s->owner=getpid(); s->catalogue=store; s->profile=*p; s->broker=*context; s->admit=admit; s->admission=userdata;
    int r=ae_execution_digest(p,s->digest); struct ae_catalogue_info info; struct ae_dispatch_snapshot d;
    if (r==0) r=ae_catalogue_get_info(store,&info);
    if (r==0 && (memcmp(info.authority_epoch,p->system_epoch,16)!=0 || memcmp(info.runtime_incarnation,p->system_runtime,16)!=0 || memcmp(info.boot_id,p->boot,16)!=0 ||
        memcmp(context->system_runtime,p->system_runtime,16)!=0 || memcmp(context->broker_epoch,p->broker_epoch,16)!=0 || memcmp(context->broker_runtime,p->broker_runtime,16)!=0 ||
        memcmp(context->account,p->account,16)!=0 || memcmp(context->boot,p->boot,16)!=0)) r=-ESTALE;
    if (r==0) r=ae_catalogue_get_dispatch(store,p->lease,context,&d);
    if (r==0 && (memcmp(d.operation,p->root_operation,16)!=0 || memcmp(d.launch,p->launch,16)!=0 || d.install_gate_revision!=p->install_gate_revision ||
        strcmp(d.state,"reserved")!=0 || d.invocation_present)) r=-ESTALE;
    if (r==0) {
        const char hex[]="0123456789abcdef"; memcpy(s->unit,"aetheri-app-",12);
        for (size_t n=0;n<16;++n) { s->unit[12+2*n]=hex[p->launch[n]>>4]; s->unit[13+2*n]=hex[p->launch[n]&15]; }
        memcpy(s->unit+44,".service",9); if (strcmp(s->unit,d.unit)!=0) r=-ESTALE;
    }
    if (r==0) r=ae_catalogue_check_execution_effect(store,context);
    if (r==0) r=ae_start_new(bus,manager,p->boot,s->unit,changed,s,&s->start);
    if (r!=0) { free(s); return r; } *output=s; return 0;
}
int ae_supervisor_start_request(void *userdata,const struct ae_execution_profile *p,const struct ae_dispatch_snapshot *d)
{
    struct ae_supervisor_start *s=userdata; int r=owner(s);
    if (r==0 && (!p || !d)) r=-EINVAL;
    if (r==0 && s->busy) r=-EBUSY;
    if (r==0 && s->used) r=-EALREADY;
    if (r!=0) return r;
    s->busy=1; s->used=1; uint8_t digest[32]; r=ae_execution_digest(p,digest);
    if (r==0 && (!memchr(d->unit,0,sizeof d->unit) || !memchr(d->state,0,sizeof d->state))) r=-EINVAL;
    if (r==0 && (memcmp(digest,s->digest,32)!=0 || memcmp(d->operation,p->root_operation,16)!=0 || memcmp(d->launch,p->launch,16)!=0 ||
        d->install_gate_revision!=p->install_gate_revision || strcmp(d->unit,s->unit)!=0 || strcmp(d->state,"reserved")!=0 || d->invocation_present)) r=-ESTALE;
    if (r==0) r=ae_catalogue_begin_execution_effect(s->catalogue,&s->broker);
    int source=r==0;
    struct ae_effect_owner current=owner_context(s);
    if (r==0) r=ae_catalogue_begin_owner_cleanup(s->catalogue,&current);
    if (r==0) s->hold=1;
    if (r==0) r=ae_start_request(s->start,guard,s);
    struct ae_start_status status;
    if (s->hold && ae_start_status(s->start,&status)==0 && !status.attempted) release_hold(s);
    if (source) { int ended=ae_catalogue_end_execution_effect(s->catalogue); if (r==0) r=ended; }
    s->busy=0; return r;
}
int ae_supervisor_start_status(struct ae_supervisor_start *s,struct ae_start_status *output)
{
    int r=owner(s); if (r==0 && !output) r=-EINVAL;
    if (r==0) r=ae_start_status(s->start,output);
    if (r==0 && s->error) { output->quarantined=1; if (!output->error) output->error=s->error; }
    return r;
}
static int close_start(struct ae_supervisor_start **output,int abandon)
{
    if (!output) return -EINVAL;
    struct ae_supervisor_start *s=*output; if (!s) return 0;
    int r=owner(s); if (r==0 && s->busy) r=-EBUSY;
    if (r!=0) return r;
    r=abandon ? ae_start_abandon(&s->start) : ae_start_close(&s->start);
    if (r!=0) return r;
    release_hold(s); if (s->hold) return s->error ? s->error : -EBUSY;
    free(s); *output=NULL; return 0;
}
int ae_supervisor_start_close(struct ae_supervisor_start **s) { return close_start(s,0); }
int ae_supervisor_start_abandon(struct ae_supervisor_start **s) { return close_start(s,1); }
