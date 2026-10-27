#include "authority/execution.h"
#include "core/digest.h"
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>

struct ae_execution_service {
    pid_t owner;
    struct ae_catalogue *catalogue;
    sd_bus *bus;
    sd_event *event;
    struct ae_execution_profile profile;
    struct ae_launch_context broker;
    struct ae_execution_effects effects;
    uint8_t digest[32];
    char unit[80];
    struct ae_peer peer;
    struct ae_process_context context;
    struct ae_execution_evidence evidence;
    struct ae_exec_channel *channel;
    sd_event_source *watch;
    struct ae_exec_report report;
    int request_attempted,bind_attempted,pinned,requested,bound,prepared,arm_attempted,armed,effect_active;
    int request_error,bind_error,release_error,channel_error,context_error,report_error;
};
static int owner(const struct ae_execution_service *s)
{ return s==NULL ? -EINVAL : s->owner==getpid() ? 0 : -ECHILD; }
static int same_broker(const struct ae_execution_service *s,const struct ae_launch_context *c)
{
    if (c==NULL) return -EINVAL;
    return c->uid==s->broker.uid && memcmp(c->system_runtime,s->broker.system_runtime,16)==0 &&
        memcmp(c->broker_epoch,s->broker.broker_epoch,16)==0 && memcmp(c->broker_runtime,s->broker.broker_runtime,16)==0 &&
        memcmp(c->account,s->broker.account,16)==0 && memcmp(c->boot,s->broker.boot,16)==0 ? 0 : -ESTALE;
}
static int starter_context(const struct ae_execution_service *s,const struct ae_peer *peer,struct ae_process_context *output)
{
    if (peer==NULL || peer->connection_uid!=0 || strcmp(peer->connection_label,"system_u:system_r:aetheri_starter_t:s0")!=0) return -EACCES;
    struct ae_process_context c; int r=ae_process_observe_registration(peer,s->bus,&c);
    if (r==0 && (c.uid!=0 || memcmp(c.boot,s->profile.boot,16)!=0 || strcmp(c.unit,s->unit)!=0 ||
        strcmp(c.label,"system_u:system_r:aetheri_starter_t:s0")!=0)) r=-EACCES;
    if (r==0) *output=c;
    return r;
}
static int same_starter(struct ae_execution_service *s,const struct ae_peer *peer)
{
    if (peer==NULL || s->peer.pidfd<0 || memcmp(peer->bus_id,s->peer.bus_id,16)!=0 ||
        strcmp(peer->unique_name,s->peer.unique_name)!=0 || peer->connection_pid!=s->peer.connection_pid ||
        peer->connection_uid!=s->peer.connection_uid || strcmp(peer->connection_label,s->peer.connection_label)!=0) return -EACCES;
    if (s->context_error) return s->context_error;
    int r=ae_peer_alive(&s->peer); struct ae_process_context c;
    if (r==0) r=starter_context(s,peer,&c);
    if (r==0 && !ae_process_context_equal(&c,&s->context)) r=-ESTALE;
    if (r!=0) s->context_error=r;
    return r;
}
static int context_digest(struct ae_execution_service *s)
{
    const struct ae_process_context *c=&s->context;
    _Static_assert(90+sizeof c->unit+sizeof c->label+sizeof c->cgroup<=2048,"execution context capacity");
    uint8_t bytes[2048]={0}; size_t used=0;
#define COPY(p,n) do { memcpy(bytes+used,p,n); used+=(n); } while (0)
    COPY("AETHEXC1",8); COPY(s->digest,32); COPY(c->boot,16); COPY(c->invocation,16);
    for (size_t n=0;n<8;++n) bytes[used++]=(uint8_t)(c->cgroup_id>>(56-8*n));
    for (size_t n=0;n<4;++n) bytes[used++]=(uint8_t)((uint32_t)c->uid>>(24-8*n));
    const char *texts[]={c->unit,c->label,c->cgroup};
    for (size_t n=0;n<3;++n) {
        size_t size=strnlen(texts[n],n==0 ? sizeof c->unit : n==1 ? sizeof c->label : sizeof c->cgroup);
        if (size==(n==0 ? sizeof c->unit : n==1 ? sizeof c->label : sizeof c->cgroup)) return -E2BIG;
        bytes[used++]=(uint8_t)(size>>8); bytes[used++]=(uint8_t)size; COPY(texts[n],size);
    }
#undef COPY
    memcpy(s->evidence.invocation,c->invocation,16);
    return ae_sha256_bytes(bytes,used,s->evidence.context_digest);
}
static int observe_channel(sd_event_source *source,int fd,uint32_t events,void *userdata)
{
    (void)fd; (void)events;
    struct ae_execution_service *s=userdata;
    int r=ae_exec_channel_read(s->channel,&s->report);
    if (r!=0) s->channel_error=r;
    if (r!=0 || s->report.state!=AE_EXEC_CHANNEL_PENDING) {
        int disabled=sd_event_source_set_enabled(source,SD_EVENT_OFF);
        if (disabled<0 && !s->channel_error) s->channel_error=disabled;
        if (disabled<0) s->watch=sd_event_source_unref(s->watch);
        s->report_error=ae_execution_service_record(s,&s->broker);
    }
    return 0; /* Retain failure without killing the authority/event loop. */
}
static int pending_channel(struct ae_execution_service *s)
{
    if (s->channel==NULL || s->channel_error) return -EAGAIN;
    int r=ae_exec_channel_read(s->channel,&s->report);
    if (r!=0) s->channel_error=r;
    if ((r!=0 || s->report.state!=AE_EXEC_CHANNEL_PENDING) && s->watch) {
        int disabled=sd_event_source_set_enabled(s->watch,SD_EVENT_OFF);
        if (disabled<0 && !s->channel_error) s->channel_error=disabled;
        if (disabled<0) s->watch=sd_event_source_unref(s->watch);
    }
    if (r!=0 || s->report.state!=AE_EXEC_CHANNEL_PENDING) s->report_error=ae_execution_service_record(s,&s->broker);
    return r!=0 ? r : s->report.state==AE_EXEC_CHANNEL_PENDING && ae_exec_channel_pristine(s->channel)==1 ? 0 : -EAGAIN;
}
int ae_execution_service_new(struct ae_catalogue *store,sd_bus *bus,sd_event *event,const struct ae_execution_profile *p,
    const struct ae_launch_context *c,const struct ae_execution_effects *effects,struct ae_execution_service **output)
{
    if (store==NULL || bus==NULL || event==NULL || p==NULL || c==NULL || effects==NULL || effects->request==NULL || effects->release==NULL || output==NULL || *output!=NULL) return -EINVAL;
    if (getuid()!=0 || geteuid()!=0 || getgid()!=0 || getegid()!=0) return -EPERM;
    struct ae_execution_service *s=calloc(1,sizeof *s); if (s==NULL) return -ENOMEM;
    s->peer=(struct ae_peer)AE_PEER_INIT; s->owner=getpid(); s->catalogue=store; s->bus=bus; s->event=event;
    s->profile=*p; s->broker=*c; s->effects=*effects;
    int r=ae_execution_digest(p,s->digest); struct ae_catalogue_info info; struct ae_dispatch_snapshot d;
    if (r==0) r=ae_catalogue_get_info(store,&info);
    if (r==0 && (memcmp(info.authority_epoch,p->system_epoch,16)!=0 || memcmp(info.runtime_incarnation,p->system_runtime,16)!=0 ||
        memcmp(info.boot_id,p->boot,16)!=0 || memcmp(c->system_runtime,p->system_runtime,16)!=0 || memcmp(c->broker_epoch,p->broker_epoch,16)!=0 ||
        memcmp(c->broker_runtime,p->broker_runtime,16)!=0 || memcmp(c->account,p->account,16)!=0 || memcmp(c->boot,p->boot,16)!=0)) r=-ESTALE;
    if (r==0) r=ae_catalogue_get_dispatch(store,p->lease,c,&d);
    if (r==0 && (memcmp(d.operation,p->root_operation,16)!=0 || memcmp(d.launch,p->launch,16)!=0 || d.install_gate_revision!=p->install_gate_revision ||
        strcmp(d.state,"reserved")!=0 || d.invocation_present)) r=-ESTALE;
    if (r==0) memcpy(s->unit,d.unit,sizeof s->unit);
    if (r!=0) { free(s); return r; }
    *output=s; return 0;
}
int ae_execution_service_request(struct ae_execution_service *s,const struct ae_launch_context *c,struct ae_execution_result *output)
{
    int r=owner(s); if (r==0 && output==NULL) r=-EINVAL;
    if (r==0) r=same_broker(s,c);
    if (r==0 && s->request_attempted) r=-EALREADY;
    if (r!=0) return r;
    s->request_attempted=1; struct ae_execution_result result;
    r=ae_catalogue_execution(s->catalogue,&s->profile,c,AE_EXECUTION_REQUEST,NULL,&result);
    if (r==0 && !result.effect_required) r=-EALREADY;
    if (r==0) {
        s->requested=1;
        s->effect_active=1;
        r=s->effects.request(s->effects.owner,&s->profile,&result.dispatch);
        s->effect_active=0;
        if (r>0) r=-EIO;
    }
    s->request_error=r;
    if (r==0) *output=result;
    return r;
}
int ae_execution_service_bind(struct ae_execution_service *s,const struct ae_peer *peer,int *writer,struct ae_execution_result *output)
{
    int r=owner(s); if (r==0 && (writer==NULL || *writer>=0 || output==NULL)) r=-EINVAL;
    if (r==0 && (!s->requested || s->bind_attempted)) r=-EALREADY;
    struct ae_process_context c; if (r==0) r=starter_context(s,peer,&c);
    if (r!=0) return r;
    int pin=fcntl(peer->pidfd,F_DUPFD_CLOEXEC,3); if (pin<0) return -errno;
    s->bind_attempted=1; s->peer=*peer; s->peer.pidfd=pin; s->context=c;
    r=sd_bus_call_method(s->bus,"org.freedesktop.systemd1","/org/freedesktop/systemd1","org.freedesktop.systemd1.Manager","RefUnit",NULL,NULL,"s",s->unit);
    if (r>=0) { s->pinned=1; r=same_starter(s,peer); }
    if (r==0) r=context_digest(s);
    int endpoint=-1;
    if (r==0) r=ae_exec_channel_new(&s->channel,&endpoint);
    if (r==0) r=sd_event_add_io(s->event,&s->watch,ae_exec_channel_fd(s->channel),EPOLLIN|EPOLLHUP|EPOLLERR,observe_channel,s);
    struct ae_execution_result result;
    if (r>=0) r=ae_catalogue_execution(s->catalogue,&s->profile,&s->broker,AE_EXECUTION_BIND,&s->evidence,&result);
    if (r==0) { s->bound=1; *writer=endpoint; *output=result; }
    else if (endpoint>=0) close(endpoint);
    s->bind_error=r;
    return r;
}
int ae_execution_service_prepared(struct ae_execution_service *s,const struct ae_peer *peer,struct ae_execution_result *output)
{
    int r=owner(s); if (r==0 && output==NULL) r=-EINVAL;
    if (r==0 && (!s->bound || s->arm_attempted || s->report.state!=AE_EXEC_CHANNEL_PENDING || s->channel_error)) r=-EAGAIN;
    if (r==0) r=same_starter(s,peer);
    if (r==0) r=pending_channel(s);
    struct ae_execution_result result;
    if (r==0) r=ae_catalogue_execution(s->catalogue,&s->profile,&s->broker,AE_EXECUTION_PREPARE,&s->evidence,&result);
    if (r==0) { s->prepared=1; *output=result; }
    return r;
}
int ae_execution_service_arm(struct ae_execution_service *s,const struct ae_launch_context *c,struct ae_execution_result *output)
{
    int r=owner(s); if (r==0 && output==NULL) r=-EINVAL;
    if (r==0) r=same_broker(s,c);
    if (r==0 && (!s->prepared || s->arm_attempted || s->report.state!=AE_EXEC_CHANNEL_PENDING || s->channel_error)) r=-EAGAIN;
    if (r==0) r=same_starter(s,&s->peer);
    if (r==0) r=pending_channel(s);
    if (r!=0) return r;
    s->arm_attempted=1; struct ae_execution_result result;
    r=ae_catalogue_execution(s->catalogue,&s->profile,c,AE_EXECUTION_ARM,&s->evidence,&result);
    if (r==0 && !result.effect_required) r=-EALREADY;
    if (r==0) {
        s->armed=1;
        s->effect_active=1;
        r=s->effects.release(s->effects.owner,&s->profile,&result.dispatch);
        s->effect_active=0;
        if (r>0) r=-EIO;
    }
    s->release_error=r;
    if (r==0) *output=result;
    return r;
}
int ae_execution_service_status(struct ae_execution_service *s,const struct ae_launch_context *c,struct ae_execution_service_status *output)
{
    int r=owner(s); if (r==0 && output==NULL) r=-EINVAL;
    if (r==0) r=same_broker(s,c);
    struct ae_execution_service_status result={0};
    if (r==0) r=ae_catalogue_get_dispatch(s->catalogue,s->profile.lease,c,&result.dispatch);
    if (r==0) {
        result.channel=s->report; result.request_error=s->request_error; result.bind_error=s->bind_error; result.release_error=s->release_error; result.channel_error=s->channel_error;
        result.context_error=s->context_error;
        result.report_error=s->report_error;
        result.requested=s->requested; result.bound=s->bound; result.prepared=s->prepared; result.arm_attempted=s->arm_attempted; result.armed=s->armed;
        *output=result;
    }
    return r;
}
int ae_execution_service_record(struct ae_execution_service *s,const struct ae_launch_context *c)
{
    _Static_assert((int)AE_EXEC_CALL==(int)AE_EXECUTION_PHASE_MAX,"Review diagnostic contract after phase expansion");
    int r=owner(s); if (r==0) r=same_broker(s,c); if (r!=0) return r;
    struct ae_execution_diagnostic d={0};
    if (s->report.state==AE_EXEC_CHANNEL_FAILURE) {
        d.kind=AE_EXECUTION_DIAGNOSTIC_FAILURE; d.phase=(uint32_t)s->report.phase; d.error=(uint32_t)s->report.error;
    } else if (s->report.state==AE_EXEC_CHANNEL_UNKNOWN) d.kind=AE_EXECUTION_DIAGNOSTIC_UNKNOWN;
    else if (s->report.state==AE_EXEC_CHANNEL_MALFORMED) d.kind=AE_EXECUTION_DIAGNOSTIC_MALFORMED;
    else if (s->channel_error) {
        d.kind=AE_EXECUTION_DIAGNOSTIC_OBSERVER;
        d.error=s->channel_error>=-4095 && s->channel_error<0 ? (uint32_t)-s->channel_error : (uint32_t)EIO;
    } else return -EAGAIN;
    struct ae_execution_result result;
    r=ae_catalogue_execution_report(s->catalogue,&s->profile,c,&s->evidence,&d,&result);
    s->report_error=r; return r;
}
int ae_execution_service_close(struct ae_execution_service **output)
{
    if (output==NULL) return -EINVAL;
    struct ae_execution_service *s=*output; if (s==NULL) return 0;
    int r=owner(s); if (r!=0) return r;
    if (s->effect_active) return -EBUSY;
    s->watch=sd_event_source_unref(s->watch);
    r=ae_exec_channel_close(&s->channel);
    ae_peer_clear(&s->peer);
    if (s->pinned) {
        int unref=sd_bus_call_method(s->bus,"org.freedesktop.systemd1","/org/freedesktop/systemd1","org.freedesktop.systemd1.Manager","UnrefUnit",NULL,NULL,"s",s->unit);
        if (r==0 && unref<0) r=unref;
    }
    free(s); *output=NULL; return r;
}
