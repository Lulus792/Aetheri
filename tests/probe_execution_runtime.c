/* Explicit isolated integration fixture, not installed authority/admission. */
#include "authority/execution_wire.h"
#include "launcher/preparation.h"
#include "launcher/execution.h"
#include "linux/image.h"
#include "core/digest.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <selinux/selinux.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/pidfd.h>
#include <poll.h>
#include <systemd/sd-daemon.h>
#include <systemd/sd-id128.h>
#include <unistd.h>
#define ROOT "/var/lib/aetheri-p0"
#define IFACE "org.aetheri.P0ExecutionTest1"
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"execution runtime line %d: %s errno=%d\n",__LINE__,#x,errno); exit(1); } } while (0)
static struct ae_execution_profile p;
static struct ae_launch_context broker;
static struct ae_catalogue *store;
static sd_bus *bus;
static sd_event *event;
static struct ae_execution_endpoint *endpoint;
static struct ae_peer starter=AE_PEER_INIT,app=AE_PEER_INIT;
static struct ae_process_context starter_context,app_context;
static sd_bus_message *waiting;
static sd_event_source *arm_source;
static int begun,armed;
static void id(uint8_t out[16],uint8_t last) { memset(out,0,16); out[6]=0x40; out[8]=0x80; out[15]=last; }
static void load(const char *root)
{
    char path[4096]; CHECK(snprintf(path,sizeof path,"%s/registry/execution.conf",root)>0);
    FILE *file=fopen(path,"rb"); uint8_t bytes[AE_EXECUTION_BYTES+1];
    CHECK(file && fread(bytes,1,sizeof bytes,file)==AE_EXECUTION_BYTES && fclose(file)==0 && ae_execution_decode(bytes,AE_EXECUTION_BYTES,&p)==0);
    sd_id128_t boot; CHECK(sd_id128_get_boot(&boot)>=0 && memcmp(boot.bytes,p.boot,16)==0);
    broker=(struct ae_launch_context){.uid=1000}; memcpy(broker.system_runtime,p.system_runtime,16);
    memcpy(broker.broker_epoch,p.broker_epoch,16); memcpy(broker.broker_runtime,p.broker_runtime,16);
    memcpy(broker.account,p.account,16); memcpy(broker.boot,p.boot,16);
}
static void catalogue(const char *root,int create)
{
    char path[4096]; CHECK(snprintf(path,sizeof path,"%s/system",root)>0);
    struct ae_catalogue_identity identity={.kind=AE_CATALOGUE_SYSTEM}; memcpy(identity.authority_epoch,p.system_epoch,16);
    CHECK((create ? ae_catalogue_create(path,&identity,p.boot,&store) : ae_catalogue_open(path,&identity,&store))==0);
    if (create) CHECK(ae_catalogue_enroll_account(store,p.account,1000)==0);
}
static void reserve(void)
{
    CHECK(ae_catalogue_begin_runtime(store,p.boot,p.system_runtime)==0);
    int64_t gate=0; CHECK(ae_catalogue_admit_install(store,p.install,p.generation,2,2,p.boot,p.system_runtime,&gate)==0 && gate==p.install_gate_revision);
    struct ae_launch_request q={.install_revision=2};
    const uint8_t *source[]={p.system_epoch,p.lease,p.launch,p.broker_epoch,p.broker_runtime,p.account,p.install,p.generation,p.data,p.boot};
    uint8_t *dest[]={q.system_epoch,q.lease,q.launch,q.broker_epoch,q.broker_runtime,q.account,q.install,q.generation,q.data,q.boot};
    for (size_t n=0;n<10;++n) memcpy(dest[n],source[n],16);
    strcpy(q.action,"run"); strcpy(q.session,"p0"); strcpy(q.seat,"none"); uint8_t hash[32];
    CHECK(ae_launch_fingerprint(&q,hash)==0 && memcmp(hash,p.request_digest,32)==0);
    struct ae_reservation_ids ids={.accepted_unix_ms=1,.expires_boottime_ns=INT64_MAX}; id(ids.plan,40); memcpy(ids.operation,p.root_operation,16);
    struct ae_dispatch_snapshot d; CHECK(ae_catalogue_reserve_launch(store,&q,&broker,&ids,&d)==0);
}
static int admin(sd_bus_message *m)
{ struct ae_peer peer=AE_PEER_INIT; int r=ae_peer_capture(m,&peer); if (r==0 && peer.connection_uid!=0) r=-EACCES; ae_peer_clear(&peer); return r; }
static int request(void *owner,const struct ae_execution_profile *profile,const struct ae_dispatch_snapshot *dispatch)
{
    (void)owner; CHECK(profile && dispatch && strcmp(dispatch->state,"reserved")==0);
    sd_bus_message *reply=NULL; sd_bus_error error=SD_BUS_ERROR_NULL;
    int r=sd_bus_call_method(bus,"org.freedesktop.systemd1","/org/freedesktop/systemd1","org.freedesktop.systemd1.Manager","StartUnit",&error,&reply,"ss",dispatch->unit,"fail");
    const char *job=NULL,*prefix="/org/freedesktop/systemd1/job/";
    if (r>=0 && (sd_bus_message_read(reply,"o",&job)<=0 || strncmp(job,prefix,strlen(prefix))!=0)) r=-EPROTO;
    if (r<0) fprintf(stderr,"compiled StartUnit: %d %s\n",r,error.message ? error.message : "-");
    sd_bus_error_free(&error); sd_bus_message_unref(reply); return r<0 ? r : 0;
}
static int info(sd_bus_message *m,void *userdata,sd_bus_error *error)
{
    (void)userdata; (void)error; sd_bus_message *reply=NULL; int r=sd_bus_message_new_method_return(m,&reply);
    if (r>=0) r=sd_bus_message_append(reply,"uu",1U,1U);
    if (r>=0) r=sd_bus_message_open_container(reply,'a',"s");
    if (r>=0) r=sd_bus_message_close_container(reply);
    if (r>=0) r=sd_bus_message_append_array(reply,'y',p.system_epoch,16);
    if (r>=0) r=sd_bus_message_append_array(reply,'y',p.system_runtime,16);
    if (r>=0) r=sd_bus_send(bus,reply,NULL);
    sd_bus_message_unref(reply); return r;
}
static int return_app(void)
{
    if (waiting==NULL || app.pidfd<0) return 0;
    struct ae_execution_endpoint_status status; CHECK(ae_execution_endpoint_status(endpoint,&broker,&status)==0 && status.execution.armed && armed);
    CHECK(ae_peer_alive(&starter)==0 && ae_peer_alive(&app)==0 && starter.connection_pid==app.connection_pid);
    sd_bus_message *reply=NULL; int r=sd_bus_message_new_method_return(waiting,&reply);
    if (r>=0) r=sd_bus_message_append(reply,"uush",(uint32_t)app.connection_pid,(uint32_t)app.connection_uid,app.connection_label,app.pidfd);
    const uint8_t *fields[]={p.lease,p.root_operation,app_context.invocation};
    for (size_t n=0;r>=0 && n<3;++n) r=sd_bus_message_append_array(reply,'y',fields[n],16);
    if (r>=0) r=sd_bus_send(bus,reply,NULL);
    sd_bus_message_unref(reply); waiting=sd_bus_message_unref(waiting); return r;
}
static int control(sd_bus_message *m,void *userdata,sd_bus_error *error)
{
    (void)userdata; (void)error; CHECK(admin(m)==0);
    if (strcmp(sd_bus_message_get_member(m),"Begin")==0) {
        struct ae_process_context self; int observed=ae_process_observe_self(bus,&self);
        if (observed!=0) fprintf(stderr,"Root self observation failed: %d\n",observed);
        CHECK(observed==0 && strcmp(self.unit,AE_STARTER_ROOT_UNIT)==0 && memcmp(self.boot,p.boot,16)==0);
        CHECK(!begun); begun=1; struct ae_execution_result result; int r=ae_execution_endpoint_request(endpoint,&broker,&result);
        if (r!=0) return sd_bus_reply_method_errorf(m,"org.aetheri.Error1.NeedsRecovery","Native request failed: %d",r);
        return sd_bus_reply_method_return(m,"");
    }
    CHECK(waiting==NULL); waiting=sd_bus_message_ref(m); int r=return_app(); CHECK(r>=0); return 1;
}
static int observe(sd_bus_message *m,void *userdata,sd_bus_error *error)
{
    (void)userdata; (void)error; CHECK(app.pidfd<0 && starter.pidfd>=0 && armed);
    struct ae_peer peer=AE_PEER_INIT; CHECK(ae_peer_capture(m,&peer)==0);
    CHECK(peer.connection_uid==1000 && strcmp(peer.connection_label,AE_STARTER_TARGET_CONTEXT)==0 &&
        peer.connection_pid==starter.connection_pid && ae_peer_alive(&starter)==0);
    CHECK(ae_process_observe(&peer,bus,&app_context)==0 && app_context.uid==1000 &&
        memcmp(app_context.boot,p.boot,16)==0 && memcmp(app_context.invocation,starter_context.invocation,16)==0 &&
        strcmp(app_context.unit,starter_context.unit)==0 && app_context.cgroup_id==starter_context.cgroup_id);
    app=peer; CHECK(sd_bus_reply_method_return(m,"")>=0 && return_app()>=0); return 1;
}
static int arm(sd_event_source *source,void *userdata)
{
    (void)source; (void)userdata; struct ae_execution_endpoint_status status;
    CHECK(ae_execution_endpoint_status(endpoint,&broker,&status)==0 && status.prepared_pending && status.execution.prepared);
    struct ae_execution_result result; int r=ae_execution_endpoint_arm(endpoint,&broker,&result);
    if (r!=0) { fprintf(stderr,"native ARM failed: %d\n",r); return r; }
    armed=1; return 0;
}
static int filter(sd_bus_message *m,void *userdata,sd_bus_error *error)
{
    (void)userdata; (void)error;
    if (sd_bus_message_is_method_call(m,"org.aetheri.Execution1","RegisterStarter")) {
        CHECK(starter.pidfd<0 && ae_peer_capture(m,&starter)==0 && starter.connection_uid==0 &&
            strcmp(starter.connection_label,"system_u:system_r:aetheri_starter_t:s0")==0);
        CHECK(ae_process_observe_registration(&starter,bus,&starter_context)==0);
    }
    if (sd_bus_message_is_method_call(m,"org.aetheri.Execution1","ReportPrepared")) {
        CHECK(arm_source==NULL && sd_event_add_defer(event,&arm_source,arm,NULL)>=0);
    }
    return 0;
}
static const sd_bus_vtable discovery[]={SD_BUS_VTABLE_START(0),SD_BUS_METHOD("GetProtocolInfo","","uuasayay",info,SD_BUS_VTABLE_UNPRIVILEGED),SD_BUS_VTABLE_END};
static const sd_bus_vtable methods[]={SD_BUS_VTABLE_START(0),SD_BUS_METHOD("Begin","","",control,SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("Wait","","uushayayay",control,SD_BUS_VTABLE_UNPRIVILEGED),SD_BUS_METHOD("Observe","","",observe,SD_BUS_VTABLE_UNPRIVILEGED),SD_BUS_VTABLE_END};
static void serve(void)
{
    char *label=NULL; CHECK(getcon_raw(&label)==0 && strcmp(label,"system_u:system_r:aetheri_system_t:s0")==0 && security_getenforce()==1 && prctl(PR_GET_NO_NEW_PRIVS,0L,0L,0L,0L)==1); freecon(label);
    CHECK(sd_event_new(&event)>=0 && sd_bus_open_system(&bus)>=0 && sd_bus_attach_event(bus,event,0)>=0);
    reserve(); CHECK(ae_execution_endpoint_new(store,bus,event,&p,&broker,request,NULL,&endpoint)==0);
    sd_bus_slot *a=NULL,*b=NULL,*f=NULL;
    CHECK(sd_bus_add_object_vtable(bus,&a,"/org/aetheri/Management1","org.aetheri.Management1",discovery,NULL)>=0);
    CHECK(sd_bus_add_object_vtable(bus,&b,"/org/aetheri/P0ExecutionTest1",IFACE,methods,NULL)>=0 && sd_bus_add_filter(bus,&f,filter,NULL)>=0);
    CHECK(sd_bus_request_name(bus,"org.aetheri.System1",0)>=0 && sd_notify(0,"READY=1\nSTATUS=Isolated native execution fixture")>0);
    CHECK(sd_event_loop(event)>=0);
}
static void drive(const char *root)
{
    CHECK(sd_bus_open_system(&bus)>=0); struct ae_execution_client *check=NULL; CHECK(ae_execution_client_new(bus,&p,AE_STARTER_ROOT_UNIT,&check)==0);
    sd_bus_creds *creds=NULL; const char *unique=NULL; CHECK(sd_bus_get_name_creds(bus,"org.aetheri.System1",SD_BUS_CREDS_UNIQUE_NAME,&creds)>=0 && sd_bus_creds_get_unique_name(creds,&unique)>=0);
    struct ae_peer held=AE_PEER_INIT; struct ae_process_context original={0};
    for (int n=0;n<2;++n) {
        sd_bus_message *call=NULL,*reply=NULL; CHECK(sd_bus_message_new_method_call(bus,&call,unique,"/org/aetheri/P0ExecutionTest1",IFACE,n==0 ? "Begin" : "Wait")>=0);
        sd_bus_error error=SD_BUS_ERROR_NULL; int r=sd_bus_call(bus,call,10000000,&error,&reply);
        if (r<0) fprintf(stderr,"drive %d: %d %s\n",n,r,error.message ? error.message : "-");
        CHECK(r>=0); struct ae_peer root_peer=AE_PEER_INIT; struct ae_process_context current;
        CHECK(ae_peer_capture_reply(reply,call,&root_peer)==0 && root_peer.connection_uid==0 &&
            strcmp(root_peer.connection_label,"system_u:system_r:aetheri_system_t:s0")==0 && ae_process_observe(&root_peer,bus,&current)==0 && strcmp(current.unit,AE_STARTER_ROOT_UNIT)==0);
        if (n==0) { held=root_peer; root_peer.pidfd=-1; original=current; }
        else {
            CHECK(ae_peer_alive(&held)==0 && root_peer.connection_pid==held.connection_pid && ae_process_context_equal(&original,&current));
            uint32_t pid=0,uid=0; const char *context=NULL; int borrowed=-1;
            CHECK(sd_bus_message_read(reply,"uush",&pid,&uid,&context,&borrowed)>0 && uid==1000 && strcmp(context,AE_STARTER_TARGET_CONTEXT)==0);
            struct ae_peer observed=AE_PEER_INIT; observed.connection_uid=uid; observed.connection_pid=(pid_t)pid; strcpy(observed.connection_label,context);
            observed.pidfd=fcntl(borrowed,F_DUPFD_CLOEXEC,3); CHECK(observed.pidfd>=3);
            uint8_t invocation[16];
            for (size_t m=0;m<3;++m) { const void *bytes=NULL; size_t size=0; CHECK(sd_bus_message_read_array(reply,'y',&bytes,&size)>0 && size==16);
                if (m<2) CHECK(memcmp(bytes,m==0 ? p.lease : p.root_operation,16)==0); else memcpy(invocation,bytes,16); }
            CHECK(sd_bus_message_at_end(reply,1)==1);
            struct ae_process_context actual; CHECK(ae_process_observe(&observed,bus,&actual)==0 && actual.uid==1000 && memcmp(actual.boot,p.boot,16)==0 && memcmp(actual.invocation,invocation,16)==0);
            char unit[80]; sd_id128_t launch; memcpy(launch.bytes,p.launch,16); char hex[33]; sd_id128_to_string(launch,hex); CHECK(snprintf(unit,sizeof unit,"aetheri-app-%s.service",hex)>0 && strcmp(actual.unit,unit)==0);
            char image_path[4096]; CHECK(snprintf(image_path,sizeof image_path,"%s/store/sha256/",root)>0); size_t used=strlen(image_path);
            for (size_t m=0;m<32;++m) snprintf(image_path+used+2*m,3,"%02x",p.artifact[m]);
            size_t prefix=strlen(image_path); strcat(image_path,"/manifest"); FILE *manifest=fopen(image_path,"rb"); CHECK(manifest);
            uint8_t bytes[AE_FIXTURE_MAX_BYTES+1]; size_t count=fread(bytes,1,sizeof bytes,manifest); CHECK(count<sizeof bytes && fclose(manifest)==0);
            struct ae_fixture_command command; CHECK(ae_fixture_resolve_action(bytes,count,p.artifact,"run",&command)==0 && strcmp(command.executable,"bin/probe")==0);
            image_path[prefix]=0; strcat(image_path,"/payload/bin/probe");
            int fd=open(image_path,O_RDONLY|O_CLOEXEC|O_NOFOLLOW); CHECK(fd>=3); struct ae_image image,matched;
            CHECK(ae_image_verify(fd,command.sha256,&image)==0 && ae_image_match_process(&observed,1000,&image,&matched)==0);
            CHECK(pidfd_send_signal(observed.pidfd,SIGTERM,NULL,0)==0); struct pollfd terminal={.fd=observed.pidfd,.events=POLLIN};
            CHECK(poll(&terminal,1,10000)==1 && (terminal.revents&POLLIN) && ae_peer_alive(&observed)==-ESRCH);
            CHECK(close(fd)==0); ae_peer_clear(&observed);
        }
        ae_peer_clear(&root_peer); sd_bus_message_unref(call); sd_bus_message_unref(reply); sd_bus_error_free(&error);
    }
    ae_peer_clear(&held); sd_bus_creds_unref(creds); CHECK(ae_execution_client_close(&check)==0); sd_bus_flush_close_unref(bus);
    puts("Actual native Root journal/starter/private views/UID1000 app-domain ELF observed; no production readiness or lease-release claim.");
}
int main(int argc,char **argv)
{
    CHECK(geteuid()==0 && (argc==2 || argc==3)); const char *root=argc==3 ? argv[2] : ROOT;
    if (argc==2 && strcmp(argv[1],"serve")==0) { uint8_t digest[32]; CHECK(ae_execution_profile_load(&p,digest)==0); }
    load(root);
    if (strcmp(argv[1],"drive")==0) { drive(root); return 0; }
    catalogue(root,strcmp(argv[1],"init")==0);
    if (strcmp(argv[1],"init")==0) { CHECK(ae_catalogue_close(&store)==0); return 0; }
    CHECK(strcmp(argv[1],"serve")==0 && argc==2); serve(); return 0;
}
