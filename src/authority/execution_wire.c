#include "authority/execution_wire.h"
#include "execution-wire.h"
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

enum { PREPARED_WAIT_USEC=4000000 };
struct ae_execution_endpoint {
    pid_t owner;
    struct ae_execution_service *service;
    sd_bus *bus;
    sd_event *event;
    sd_bus_slot *slot;
    sd_bus_message *pending;
    sd_event_source *timer;
    uint64_t deadline,bound_revision;
    struct ae_execution_profile profile;
    uint8_t digest[32];
    ae_execution_effect request;
    void *request_owner;
    int busy,prepared_expired,registration_reply_attempted,release_reply_attempted;
    int registration_reply_error,release_reply_error;
};
static int owner(const struct ae_execution_endpoint *e)
{ return e==NULL ? -EINVAL : e->owner==getpid() ? 0 : -ECHILD; }
static int enter(struct ae_execution_endpoint *e)
{ int r=owner(e); if (r==0 && e->busy) r=-EBUSY; if (r==0) e->busy=1; return r; }
static int now(uint64_t *value)
{
    struct timespec t; if (clock_gettime(CLOCK_MONOTONIC,&t)!=0) return -errno;
    if (t.tv_sec<0 || (uint64_t)t.tv_sec>(UINT64_MAX-999999)/1000000) return -EOVERFLOW;
    *value=(uint64_t)t.tv_sec*1000000+(uint64_t)t.tv_nsec/1000; return 0;
}
static int reply_error(sd_bus_error *error,int r)
{
    return sd_bus_error_set_const(error,r==-EACCES ? "org.aetheri.Error1.NotAuthorized" :
        r==-EBADMSG || r==-EPROTO ? "org.aetheri.Error1.InvalidArgument" : "org.aetheri.Error1.NeedsRecovery",
        "Execution requires the selected current starter and original journal context");
}
static int receipt(struct ae_execution_endpoint *e,sd_bus_message *request,const struct ae_dispatch_snapshot *d,int writer)
{
    int registration=writer>=0;
    if (d->revision<1 || memcmp(d->operation,e->profile.root_operation,16)!=0 || memcmp(d->launch,e->profile.launch,16)!=0 ||
        d->install_gate_revision!=e->profile.install_gate_revision ||
        (registration ? strcmp(d->state,"reserved")!=0 || d->invocation_present :
            strcmp(d->state,"dispatching")!=0 || !d->invocation_present || (uint64_t)d->revision<=e->bound_revision)) return -ESTALE;
    sd_bus_message *reply=NULL; int r=sd_bus_message_new_method_return(request,&reply);
    const uint8_t *fields[]={e->profile.system_epoch,e->profile.system_runtime,e->profile.boot,e->profile.lease,d->operation,e->digest};
    for (size_t n=0;r>=0 && n<6;++n) r=sd_bus_message_append_array(reply,'y',fields[n],n==5 ? 32U : 16U);
    if (r>=0) r=sd_bus_message_append(reply,"t",(uint64_t)d->revision);
    if (r>=0 && registration) r=sd_bus_message_append(reply,"h",writer);
    if (r>=0) r=sd_bus_send(e->bus,reply,NULL);
    sd_bus_message_unref(reply); return r<0 ? r : 0;
}
static int request_effect(void *owner,const struct ae_execution_profile *p,const struct ae_dispatch_snapshot *d)
{ struct ae_execution_endpoint *e=owner; return e->request(e->request_owner,p,d); }
static void expire(struct ae_execution_endpoint *);
static int release_effect(void *owner,const struct ae_execution_profile *p,const struct ae_dispatch_snapshot *d)
{
    (void)p; struct ae_execution_endpoint *e=owner;
    if (e->pending==NULL || e->release_reply_attempted || e->prepared_expired) return -EALREADY;
    uint64_t stamp=0; int r=now(&stamp);
    if (r!=0) { e->release_reply_error=r; return r; }
    if (stamp>=e->deadline) { expire(e); return -ETIMEDOUT; }
    /* Consume before any reply allocation/send. An unknown result can never
     * cause another release, error response or transport retry for this call. */
    sd_bus_message *call=e->pending; e->pending=NULL;
    e->timer=sd_event_source_unref(e->timer); e->release_reply_attempted=1;
    r=receipt(e,call,d,-1); e->release_reply_error=r;
    sd_bus_message_unref(call); return r;
}
static void expire(struct ae_execution_endpoint *e)
{
    if (e->pending==NULL) return;
    sd_bus_message *call=e->pending; e->pending=NULL; e->prepared_expired=1;
    e->timer=sd_event_source_unref(e->timer);
    int r=sd_bus_reply_method_errorf(call,"org.aetheri.Error1.NeedsRecovery","Prepared release expired; execution remains unconfirmed");
    e->release_reply_error=r<0 ? r : -ETIMEDOUT;
    sd_bus_message_unref(call);
}
static int timed_out(sd_event_source *source,uint64_t usec,void *userdata)
{
    (void)source; (void)usec; struct ae_execution_endpoint *e=userdata;
    int r=enter(e); if (r==0) { expire(e); e->busy=0; }
    return 0;
}
static int input(struct ae_execution_endpoint *e,sd_bus_message *m,struct ae_peer *peer)
{
    if (sd_bus_message_get_bus(m)!=e->bus || sd_bus_message_get_expect_reply(m)!=1 ||
        !sd_bus_message_has_signature(m,AE_EXECUTION_REGISTER_INPUT)) return -EPROTO;
    int r=ae_peer_capture(m,peer);
    const uint8_t *expected[]={e->profile.lease,e->digest};
    for (size_t n=0;r==0 && n<2;++n) {
        const void *p=NULL; size_t size=0; int read=sd_bus_message_read_array(m,'y',&p,&size);
        if (read<=0 || size!=(n==0 ? 16U : 32U)) r=read<0 ? read : -EBADMSG;
        else if (memcmp(p,expected[n],size)!=0) r=-ESTALE;
    }
    if (r==0 && sd_bus_message_at_end(m,1)!=1) r=-EBADMSG;
    return r;
}
static int register_starter(sd_bus_message *m,void *userdata,sd_bus_error *error)
{
    struct ae_execution_endpoint *e=userdata; int r=enter(e); if (r!=0) return reply_error(error,r);
    struct ae_peer peer=AE_PEER_INIT; int writer=-1; struct ae_execution_result result;
    r=input(e,m,&peer);
    if (r==0) r=ae_execution_service_bind(e->service,&peer,&writer,&result);
    ae_peer_clear(&peer);
    if (r==0) {
        e->registration_reply_attempted=1; e->bound_revision=(uint64_t)result.dispatch.revision;
        e->registration_reply_error=receipt(e,m,&result.dispatch,writer);
        close(writer); /* sd-bus owns any queued copy; issuer retains reader only. */
        e->busy=0; return 1; /* Never an automatic second response after send uncertainty. */
    }
    e->busy=0; return reply_error(error,r);
}
static int prepared(sd_bus_message *m,void *userdata,sd_bus_error *error)
{
    struct ae_execution_endpoint *e=userdata; int r=enter(e); if (r!=0) return reply_error(error,r);
    struct ae_peer peer=AE_PEER_INIT; sd_event_source *timer=NULL; uint64_t stamp=0;
    r=input(e,m,&peer);
    if (r==0 && (!e->registration_reply_attempted || e->registration_reply_error || e->pending || e->release_reply_attempted || e->prepared_expired)) r=-EALREADY;
    if (r==0) r=now(&stamp);
    if (r==0 && stamp>UINT64_MAX-PREPARED_WAIT_USEC) r=-EOVERFLOW;
    if (r==0) r=sd_event_add_time(e->event,&timer,CLOCK_MONOTONIC,stamp+PREPARED_WAIT_USEC,0,timed_out,e);
    struct ae_execution_result result;
    if (r>=0) r=ae_execution_service_prepared(e->service,&peer,&result);
    ae_peer_clear(&peer);
    if (r==0) {
        e->pending=sd_bus_message_ref(m); e->deadline=stamp+PREPARED_WAIT_USEC; e->timer=timer;
        e->busy=0; return 1; /* Deliberately deferred until fresh native ARM. */
    }
    sd_event_source_unref(timer); e->busy=0; return reply_error(error,r);
}
static const sd_bus_vtable methods[]={SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD(AE_EXECUTION_REGISTER_METHOD,AE_EXECUTION_REGISTER_INPUT,AE_EXECUTION_REGISTER_OUTPUT,register_starter,SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD(AE_EXECUTION_PREPARED_METHOD,AE_EXECUTION_PREPARED_INPUT,AE_EXECUTION_PREPARED_OUTPUT,prepared,SD_BUS_VTABLE_UNPRIVILEGED),SD_BUS_VTABLE_END};
int ae_execution_endpoint_new(struct ae_catalogue *store,sd_bus *bus,sd_event *event,const struct ae_execution_profile *p,
    const struct ae_launch_context *c,ae_execution_effect request,void *request_owner,struct ae_execution_endpoint **output)
{
    if (output==NULL || *output!=NULL || request==NULL) return -EINVAL;
    struct ae_execution_endpoint *e=calloc(1,sizeof *e); if (e==NULL) return -ENOMEM;
    e->owner=getpid(); e->bus=bus; e->event=event; e->request=request; e->request_owner=request_owner;
    int r=ae_execution_digest(p,e->digest); if (r==0) e->profile=*p;
    struct ae_execution_effects effects={.owner=e,.request=request_effect,.release=release_effect};
    if (r==0) r=ae_execution_service_new(store,bus,event,p,c,&effects,&e->service);
    if (r==0) r=sd_bus_add_object_vtable(bus,&e->slot,AE_EXECUTION_PATH,AE_EXECUTION_INTERFACE,methods,e);
    if (r<0) { (void)ae_execution_service_close(&e->service); free(e); return r; }
    *output=e; return 0;
}
int ae_execution_endpoint_request(struct ae_execution_endpoint *e,const struct ae_launch_context *c,struct ae_execution_result *output)
{
    int r=enter(e); if (r!=0) return r;
    r=ae_execution_service_request(e->service,c,output); e->busy=0; return r;
}
int ae_execution_endpoint_arm(struct ae_execution_endpoint *e,const struct ae_launch_context *c,struct ae_execution_result *output)
{
    int r=enter(e); if (r!=0) return r;
    if (output==NULL) r=-EINVAL;
    if (r==0 && e->prepared_expired) r=-ETIMEDOUT;
    if (r==0 && e->pending==NULL) r=e->release_reply_attempted ? -EALREADY : -EAGAIN;
    uint64_t stamp=0; if (r==0) r=now(&stamp);
    if (r==0 && stamp>=e->deadline) { expire(e); r=-ETIMEDOUT; }
    if (r==0) r=ae_execution_service_arm(e->service,c,output);
    e->busy=0; return r;
}
int ae_execution_endpoint_status(struct ae_execution_endpoint *e,const struct ae_launch_context *c,struct ae_execution_endpoint_status *output)
{
    int r=enter(e); if (r!=0) return r;
    if (output==NULL) r=-EINVAL;
    struct ae_execution_endpoint_status s={0};
    if (r==0) r=ae_execution_service_status(e->service,c,&s.execution);
    if (r==0) {
        s.prepared_pending=e->pending!=NULL; s.prepared_expired=e->prepared_expired;
        s.registration_reply_attempted=e->registration_reply_attempted; s.release_reply_attempted=e->release_reply_attempted;
        s.registration_reply_error=e->registration_reply_error; s.release_reply_error=e->release_reply_error; *output=s;
    }
    e->busy=0; return r;
}
int ae_execution_endpoint_close(struct ae_execution_endpoint **output)
{
    if (output==NULL) return -EINVAL;
    struct ae_execution_endpoint *e=*output; if (e==NULL) return 0;
    int r=enter(e); if (r!=0) return r;
    r=ae_execution_service_close(&e->service);
    if (e->service!=NULL) { e->busy=0; return r; }
    sd_bus_slot_unref(e->slot); sd_event_source_unref(e->timer); sd_bus_message_unref(e->pending);
    free(e); *output=NULL; return r;
}
