#include "authority/system_connection.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>

#define SYSTEM_NAME "org.aetheri.System1"
#define SYSTEM_LABEL "system_u:system_r:aetheri_system_t:s0"

struct ae_system_connection {
    pid_t owner;
    sd_bus *bus;
    struct ae_peer peer;
    struct ae_process_context context;
    struct ae_broker_identity user;
    uint8_t epoch[16], boot[16], runtime[16], events[16];
    char unit[256], unique[256];
    sd_event_source *exit_source;
    sd_bus_slot *name_slot;
    int busy, error, departed;
};

static int valid_id(const uint8_t *id)
{ return id && (id[6] >> 4) == 4 && (id[8] & 0xc0) == 0x80; }
static int valid_unit(const char *unit)
{
    if (!unit) return 0;
    size_t n=strnlen(unit,256);
    if (n<9 || n>=256 || strcmp(unit+n-8,".service")!=0) return 0;
    for (size_t i=0;i<n;++i) {
        char c=unit[i];
        if (!((c>='a' && c<='z') || (c>='A' && c<='Z') || (c>='0' && c<='9') || c=='-' || c=='_' || c=='.' || c=='@')) return 0;
    }
    return 1;
}
static int owner(const struct ae_system_connection *c)
{ return !c ? -EINVAL : c->owner!=getpid() ? -ECHILD : 0; }
static void retire(struct ae_system_connection *c)
{
    c->exit_source=sd_event_source_unref(c->exit_source);
    c->name_slot=sd_bus_slot_unref(c->name_slot);
    ae_peer_clear(&c->peer);
}
static void depart(struct ae_system_connection *c)
{
    c->departed=1;
    if (c->exit_source) (void)sd_event_source_set_enabled(c->exit_source,SD_EVENT_OFF);
    /* A callback inside sd_bus_call/process verification cannot close its FD. */
    if (!c->busy) retire(c);
}
static int name_changed(sd_bus_message *m,void *data,sd_bus_error *error)
{
    (void)error;
    struct ae_system_connection *c=data;
    const char *name=NULL,*old=NULL,*next=NULL;
    int r=sd_bus_message_read(m,"sss",&name,&old,&next);
    if (r>0 && strcmp(name,SYSTEM_NAME)==0 && strcmp(next,c->unique)!=0) depart(c);
    return r<0 ? r : 0;
}
static int exited(sd_event_source *s,int fd,uint32_t events,void *data)
{
    (void)s; (void)fd; (void)events;
    depart(data); return 0;
}
static int enter(struct ae_system_connection *c)
{
    int r=owner(c);
    if (r==0 && c->busy) r=-EBUSY;
    if (r==0) c->busy=1;
    return r;
}
static int leave(struct ae_system_connection *c,int r)
{
    if (r!=0 && !c->error) c->error=r;
    c->busy=0;
    if (c->departed) { retire(c); return -ENOTCONN; }
    return r;
}
static int resolve(sd_bus *bus,char output[256])
{
    sd_bus_creds *creds=NULL;
    int r=sd_bus_get_name_creds(bus,SYSTEM_NAME,SD_BUS_CREDS_UNIQUE_NAME,&creds);
    const char *name=NULL;
    if (r>=0 && ((sd_bus_creds_get_mask(creds)&SD_BUS_CREDS_UNIQUE_NAME)==0 ||
        sd_bus_creds_get_unique_name(creds,&name)<0 || !name || name[0]!=':' ||
        !sd_bus_service_name_is_valid(name) || strnlen(name,256)>=256)) r=-ENODATA;
    if (r>=0) { strcpy(output,name); r=0; }
    sd_bus_creds_unref(creds); return r;
}
static int dispatch_pending(struct ae_system_connection *c)
{
    /* A synchronous call can retrieve its reply ahead of queued signals. A
     * same-unique-name release/reacquire must still invalidate the binding.
     * This is User's outbound bus; nested callbacks cannot reuse/close us.
     * Do not silently ignore an unbounded queue or add an idle polling loop. */
    for (unsigned int n=0;n<64;++n) {
        int r=sd_bus_process(c->bus,NULL);
        if (c->departed) return -ENOTCONN;
        if (r<=0) return r;
    }
    return -E2BIG;
}
static int named(struct ae_system_connection *c)
{
    if (c->departed) return -ENOTCONN;
    int r=dispatch_pending(c);
    char actual[256];
    if (r==0) r=resolve(c->bus,actual);
    if (r==-ENXIO || r==-ESRCH || (r==0 && strcmp(actual,c->unique)!=0)) { depart(c); return -ENOTCONN; }
    if (r==0) r=dispatch_pending(c);
    return c->departed ? -ENOTCONN : r;
}
static int current(struct ae_system_connection *c)
{
    /* Check departure even after an uncertain call or context error: only
     * independently observed departure permits discarding this binding. */
    int r=c->departed ? -ENOTCONN : ae_peer_alive(&c->peer);
    if (r==-ESRCH) { depart(c); r=-ENOTCONN; }
    if (r==0) r=named(c);
    if (r==0 && c->error) r=c->error;
    if (r==0) r=ae_process_verify_main(&c->peer,c->bus,&c->context);
    if (r==0) r=named(c);
    return c->departed ? -ENOTCONN : r;
}
static int same_peer(const struct ae_peer *a,const struct ae_peer *b)
{
    return a->connection_uid==b->connection_uid && a->connection_pid==b->connection_pid &&
        memcmp(a->bus_id,b->bus_id,16)==0 && strcmp(a->unique_name,b->unique_name)==0 &&
        strcmp(a->connection_label,b->connection_label)==0;
}
static int response(struct ae_system_connection *c,sd_bus_message *request,sd_bus_message *reply)
{
    struct ae_peer peer=AE_PEER_INIT;
    int r=ae_peer_capture_reply(reply,request,&peer);
    if (r==0 && !same_peer(&c->peer,&peer)) r=-ESTALE;
    if (r==0) r=current(c);
    if (r==0) r=ae_process_verify_main(&peer,c->bus,&c->context);
    ae_peer_clear(&peer);
    return c->departed ? -ENOTCONN : r;
}
static int id(sd_bus_message *m,uint8_t output[16])
{
    const void *bytes=NULL; size_t size=0;
    int r=sd_bus_message_read_array(m,'y',&bytes,&size);
    if (r<=0 || size!=16 || !valid_id(bytes)) return r<0 ? r : -EBADMSG;
    memcpy(output,bytes,16); return 0;
}
static int discovery(struct ae_system_connection *c,sd_bus_message *m)
{
    if (!sd_bus_message_has_signature(m,"uuasayay")) return -EBADMSG;
    uint32_t major=0,minor=0; int r=sd_bus_message_read(m,"uu",&major,&minor);
    if (r<=0 || major!=1 || minor<1) return -EPROTONOSUPPORT;
    r=sd_bus_message_enter_container(m,'a',"s");
    if (r<=0) return r<0 ? r : -EBADMSG;
    const char *cap=NULL; unsigned int count=0;
    while ((r=sd_bus_message_read(m,"s",&cap))>0)
        if (++count>16 || !cap || strnlen(cap,129)>128) return -E2BIG;
    if (r<0) return r;
    r=sd_bus_message_exit_container(m);
    uint8_t epoch[16];
    if (r>=0) r=id(m,epoch);
    if (r==0 && memcmp(epoch,c->epoch,16)!=0) r=-ESTALE;
    /* Management1 returns an EVENT incarnation here, not a Root runtime. */
    if (r==0) r=id(m,c->events);
    if (r==0 && sd_bus_message_at_end(m,1)!=1) r=-EBADMSG;
    return r;
}
static int call(struct ae_system_connection *c,sd_bus_message *request,sd_bus_message **output)
{
    int r=current(c);
    const char *dest=sd_bus_message_get_destination(request);
    if (r==0 && (sd_bus_message_get_bus(request)!=c->bus || !dest || strcmp(dest,c->unique)!=0)) r=-EACCES;
    sd_bus_message *reply=NULL;
    if (r==0) r=sd_bus_call(c->bus,request,2000000,NULL,&reply);
    if (r>=0) r=response(c,request,reply);
    if (r==0) { *output=reply; reply=NULL; }
    sd_bus_message_unref(reply); return r;
}
int ae_system_connection_new(sd_bus *bus,sd_event *event,const char *unit,
    const uint8_t epoch[16],const uint8_t boot[16],const struct ae_broker_identity *user,
    struct ae_system_connection **output)
{
    if (!bus || !event || !valid_unit(unit) || !valid_id(epoch) || !boot || !user ||
        !valid_id(user->account) || !valid_id(user->epoch) || !valid_id(user->runtime) || !output || *output) return -EINVAL;
    uint8_t present=0; for (size_t n=0;n<16;++n) present|=boot[n];
    if (!present) return -EINVAL;
    struct ae_system_connection *c=calloc(1,sizeof *c); if (!c) return -ENOMEM;
    c->owner=getpid(); c->bus=bus; c->busy=1; c->peer=(struct ae_peer)AE_PEER_INIT;
    c->user=*user; strcpy(c->unit,unit); memcpy(c->epoch,epoch,16); memcpy(c->boot,boot,16);
    int r=resolve(bus,c->unique);
    if (r==0) r=sd_bus_add_match(bus,&c->name_slot,
        "type='signal',sender='org.freedesktop.DBus',path='/org/freedesktop/DBus',interface='org.freedesktop.DBus',member='NameOwnerChanged',arg0='org.aetheri.System1'",name_changed,c);
    if (r>=0) r=named(c);
    sd_bus_message *request=NULL,*reply=NULL;
    if (r==0) r=sd_bus_message_new_method_call(bus,&request,c->unique,"/org/aetheri/Management1","org.aetheri.Management1","GetProtocolInfo");
    if (r>=0) r=sd_bus_call(bus,request,2000000,NULL,&reply);
    if (r>=0) r=ae_peer_capture_reply(reply,request,&c->peer);
    if (r==0 && (c->peer.connection_uid!=0 || strcmp(c->peer.connection_label,SYSTEM_LABEL)!=0)) r=-EACCES;
    if (r==0) r=ae_process_observe_main(&c->peer,bus,&c->context);
    if (r==0 && (c->context.uid!=0 || strcmp(c->context.label,SYSTEM_LABEL)!=0 ||
        strcmp(c->context.unit,unit)!=0 || memcmp(c->context.boot,boot,16)!=0)) r=-EACCES;
    if (r==0) r=discovery(c,reply);
    if (r==0) r=sd_event_add_io(event,&c->exit_source,c->peer.pidfd,EPOLLIN,exited,c);
    sd_bus_message_unref(request); sd_bus_message_unref(reply); request=NULL; reply=NULL;
    if (r>=0) r=current(c);
    if (r==0) r=sd_bus_message_new_method_call(bus,&request,c->unique,"/org/aetheri/Broker1","org.aetheri.Broker1","Register");
    const uint8_t *fields[]={epoch,user->account,user->epoch,user->runtime};
    for (size_t n=0;r>=0 && n<4;++n) r=sd_bus_message_append_array(request,'y',fields[n],16);
    if (r>=0) r=call(c,request,&reply);
    if (r==0 && !sd_bus_message_has_signature(reply,"ay")) r=-EBADMSG;
    if (r==0) r=id(reply,c->runtime);
    if (r==0 && sd_bus_message_at_end(reply,1)!=1) r=-EBADMSG;
    if (r==0) r=current(c);
    sd_bus_message_unref(request); sd_bus_message_unref(reply);
    r=leave(c,r);
    if (r!=0) { (void)ae_system_connection_close(&c); return r; }
    *output=c; return 0;
}
int ae_system_connection_verify(struct ae_system_connection *c)
{ int r=enter(c); return r!=0 ? r : leave(c,current(c)); }
int ae_system_connection_verify_selection(struct ae_system_connection *c,const char *unit,const uint8_t epoch[16],const uint8_t boot[16])
{
    if (!valid_unit(unit) || !epoch || !boot) return -EINVAL;
    int r=enter(c); if (r!=0) return r;
    r=current(c);
    if (r==0 && (strcmp(unit,c->unit)!=0 || memcmp(epoch,c->epoch,16)!=0 || memcmp(boot,c->boot,16)!=0)) r=-ESTALE;
    return leave(c,r);
}
int ae_system_connection_departed(const struct ae_system_connection *c)
{ int r=owner(c); return r!=0 ? r : c->departed; }
int ae_system_connection_runtime(struct ae_system_connection *c,uint8_t output[16])
{
    if (!output) return -EINVAL;
    int r=ae_system_connection_verify(c);
    if (r==0) memcpy(output,c->runtime,16);
    return r;
}
int ae_system_connection_request(struct ae_system_connection *c,const char *path,const char *interface,const char *method,sd_bus_message **output)
{
    if (!output || *output) return -EINVAL;
    int r=enter(c); if (r!=0) return r;
    r=current(c); sd_bus_message *request=NULL;
    if (r==0) r=sd_bus_message_new_method_call(c->bus,&request,c->unique,path,interface,method);
    r=leave(c,r<0 ? r : 0);
    if (r==0) { *output=request; request=NULL; }
    sd_bus_message_unref(request); return r;
}
int ae_system_connection_call(struct ae_system_connection *c,sd_bus_message *request,sd_bus_message **output)
{
    if (!request || !output || *output) return -EINVAL;
    int r=enter(c); return r!=0 ? r : leave(c,call(c,request,output));
}
int ae_system_connection_check(struct ae_system_connection *c)
{
    int r=enter(c); if (r!=0) return r;
    sd_bus_message *request=NULL,*reply=NULL;
    r=current(c);
    if (r==0) r=sd_bus_message_new_method_call(c->bus,&request,c->unique,"/org/aetheri/Broker1","org.aetheri.Broker1","Check");
    if (r>=0) r=sd_bus_message_append_array(request,'y',c->user.runtime,16);
    if (r>=0) r=call(c,request,&reply);
    uint8_t runtime[16];
    if (r==0 && !sd_bus_message_has_signature(reply,"ay")) r=-EBADMSG;
    if (r==0) r=id(reply,runtime);
    if (r==0 && memcmp(runtime,c->runtime,16)!=0) r=-ESTALE;
    if (r==0 && sd_bus_message_at_end(reply,1)!=1) r=-EBADMSG;
    sd_bus_message_unref(request); sd_bus_message_unref(reply);
    return leave(c,r);
}
int ae_system_connection_close(struct ae_system_connection **output)
{
    if (!output) return -EINVAL;
    struct ae_system_connection *c=*output; if (!c) return 0;
    int r=owner(c); if (r!=0) return r;
    if (c->busy) return -EBUSY;
    retire(c); free(c); *output=NULL; return 0;
}
