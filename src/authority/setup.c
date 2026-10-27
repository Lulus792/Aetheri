#include "authority/setup.h"
#include "core/digest.h"
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <signal.h>

#define SETUP_IMAGE "/var/lib/aetheri-p0/setup-helper"
static void setup_unit(const struct ae_setup_profile *p,char output[80])
{
    static const char hex[]="0123456789abcdef";
    memcpy(output,"aetheri-app-",12);
    for (size_t n=0;n<16;++n) { output[12+2*n]=hex[p->launch[n]>>4]; output[13+2*n]=hex[p->launch[n]&15]; }
    memcpy(output+44,".service",9);
}
static int equal_string(sd_bus *bus,const char *path,const char *property,const char *expected)
{
    char *text=NULL; int r=sd_bus_get_property_string(bus,"org.freedesktop.systemd1",path,"org.freedesktop.systemd1.Service",property,NULL,&text);
    if (r>=0 && strcmp(text,expected)!=0) r=-ESTALE;
    free(text); return r<0 ? r : 0;
}
/* This is a distinct supervisor-only recovery proof. It cannot substitute for
 * an app ProcessFD or target-exec evidence. Root-protected provisioning guarantees
 * the selected no-exec image has not been replaced since the attempt. */
static int terminal_setup(struct ae_setup_service *s,const struct ae_setup_profile *p,const uint8_t hash[32])
{
    int r=ae_setup_verify_image(p); char name[80]; setup_unit(p,name);
    if (r==0) r=sd_bus_call_method(s->bus,"org.freedesktop.systemd1","/org/freedesktop/systemd1","org.freedesktop.systemd1.Manager","RefUnit",NULL,NULL,"s",name);
    if (r>=0) { s->pinned=1; memcpy(s->context.unit,name,strlen(name)+1); }
    sd_bus_message *unit=NULL,*execution=NULL; const char *path=NULL;
    if (r>=0) r=sd_bus_call_method(s->bus,"org.freedesktop.systemd1","/org/freedesktop/systemd1","org.freedesktop.systemd1.Manager","GetUnit",NULL,&unit,"s",name);
    if (r>=0 && sd_bus_message_read(unit,"o",&path)<=0) r=-EBADMSG;
    const char *names[]={"Type","User","Group","Restart"};
    const char *values[]={"notify","root","root","no"};
    for (size_t n=0;r>=0 && n<4;++n) r=equal_string(s->bus,path,names[n],values[n]);
    sd_bus_message *label=NULL; int ignore_label=1; const char *label_text=NULL;
    if (r>=0) r=sd_bus_get_property(s->bus,"org.freedesktop.systemd1",path,"org.freedesktop.systemd1.Service","SELinuxContext",NULL,&label,"(bs)");
    if (r>=0 && (sd_bus_message_read(label,"(bs)",&ignore_label,&label_text)<=0 || ignore_label || strcmp(label_text,"system_u:system_r:aetheri_setup_t:s0")!=0)) r=-ESTALE;
    sd_bus_message_unref(label);
    uint32_t restarts=1; int nnp=0,private_mounts=0; uint64_t caps=0;
    if (r>=0) r=sd_bus_get_property_trivial(s->bus,"org.freedesktop.systemd1",path,"org.freedesktop.systemd1.Service","NRestarts",NULL,'u',&restarts);
    if (r>=0) r=sd_bus_get_property_trivial(s->bus,"org.freedesktop.systemd1",path,"org.freedesktop.systemd1.Service","NoNewPrivileges",NULL,'b',&nnp);
    if (r>=0) r=sd_bus_get_property_trivial(s->bus,"org.freedesktop.systemd1",path,"org.freedesktop.systemd1.Service","PrivateMounts",NULL,'b',&private_mounts);
    if (r>=0) r=sd_bus_get_property_trivial(s->bus,"org.freedesktop.systemd1",path,"org.freedesktop.systemd1.Service","CapabilityBoundingSet",NULL,'t',&caps);
    if (r>=0 && (restarts!=0 || !nnp || !private_mounts || caps!=((UINT64_C(1)<<21)|(UINT64_C(1)<<2)))) r=-ESTALE;
    const char *empty_commands[]={"ExecCondition","ExecStartPre","ExecStartPost","ExecStop","ExecStopPost","ExecReload"};
    for (size_t n=0;r>=0 && n<6;++n) {
        sd_bus_message *commands=NULL;
        r=sd_bus_get_property(s->bus,"org.freedesktop.systemd1",path,"org.freedesktop.systemd1.Service",empty_commands[n],NULL,&commands,"a(sasbttttuii)");
        if (r>=0 && (sd_bus_message_enter_container(commands,'a',"(sasbttttuii)")<=0 || sd_bus_message_at_end(commands,0)!=1)) r=-ESTALE;
        sd_bus_message_unref(commands);
    }
    if (r>=0) r=sd_bus_get_property(s->bus,"org.freedesktop.systemd1",path,"org.freedesktop.systemd1.Service","ExecStart",NULL,&execution,"a(sasbttttuii)");
    const char *executable=NULL,*argument=NULL; int ignored=1,code=0,status=-1; uint32_t pid=0; uint64_t times[4]={0};
    if (r>=0 && (sd_bus_message_enter_container(execution,'a',"(sasbttttuii)")<=0 || sd_bus_message_enter_container(execution,'r',"sasbttttuii")<=0 ||
        sd_bus_message_read(execution,"s",&executable)<=0 || strcmp(executable,SETUP_IMAGE)!=0 || sd_bus_message_enter_container(execution,'a',"s")<=0 ||
        sd_bus_message_read(execution,"s",&argument)<=0 || strcmp(argument,SETUP_IMAGE)!=0 || sd_bus_message_at_end(execution,0)!=1 || sd_bus_message_exit_container(execution)<0 ||
        sd_bus_message_read(execution,"bttttuii",&ignored,&times[0],&times[1],&times[2],&times[3],&pid,&code,&status)<=0 || ignored || code!=CLD_EXITED || status!=5 || pid==0 ||
        times[0]==0 || times[1]==0 || times[2]<times[0] || times[3]<times[1] || sd_bus_message_exit_container(execution)<0 || sd_bus_message_at_end(execution,0)!=1)) r=-ESTALE;
    sd_bus_message *inv=NULL; const void *bytes=NULL; size_t size=0;
    if (r>=0) r=sd_bus_get_property(s->bus,"org.freedesktop.systemd1",path,"org.freedesktop.systemd1.Unit","InvocationID",NULL,&inv,"ay");
    if (r>=0 && (sd_bus_message_read_array(inv,'y',&bytes,&size)<=0 || size!=16)) r=-ESTALE;
    if (r>=0) { memcpy(s->context.invocation,bytes,16); memcpy(s->context.boot,p->boot,16); }
    uint8_t evidence[100]={0}; size_t used=56;
    if (r>=0) {
        memcpy(evidence,"AETHSTE1",8); memcpy(evidence+8,hash,32); memcpy(evidence+40,s->context.invocation,16);
        for (size_t n=0;n<4;++n) for (size_t m=0;m<8;++m) evidence[used++]=(uint8_t)(times[n]>>(56-8*m));
        uint32_t numbers[]={pid,(uint32_t)code,(uint32_t)status};
        for (size_t n=0;n<3;++n) for (size_t m=0;m<4;++m) evidence[used++]=(uint8_t)(numbers[n]>>(24-8*m));
        r=ae_sha256_bytes(evidence,sizeof evidence,s->evidence);
    }
    sd_bus_message_unref(inv); sd_bus_message_unref(execution); sd_bus_message_unref(unit);
    if (r<0 && s->pinned) {
        (void)sd_bus_call_method(s->bus,"org.freedesktop.systemd1","/org/freedesktop/systemd1","org.freedesktop.systemd1.Manager","UnrefUnit",NULL,NULL,"s",name);
        s->pinned=0;
    }
    return r<0 ? r : 0;
}

static int reply_error(sd_bus_error *error,int result)
{
    return sd_bus_error_set_const(error,result==-EACCES ? "org.aetheri.Error1.NotAuthorized" :
        result==-EEXIST ? "org.aetheri.Error1.RequestConflict" : result==-EBADMSG ? "org.aetheri.Error1.InvalidArgument" :
        "org.aetheri.Error1.NeedsRecovery","Protected setup requires current identity or reconciliation");
}
static int array(sd_bus_message *message,uint8_t *output,size_t required)
{
    const void *bytes=NULL; size_t size=0; int result=sd_bus_message_read_array(message,'y',&bytes,&size);
    if (result<=0 || size!=required) return result<0 ? result : -EBADMSG;
    memcpy(output,bytes,size); return 0;
}
static int profile(struct ae_setup_service *s,struct ae_setup_profile *p,uint8_t hash[32])
{
    int r=ae_setup_load(p,hash);
    if (r==0 && (memcmp(p->system_epoch,s->epoch,16)!=0 || memcmp(p->system_runtime,s->runtime,16)!=0 || memcmp(p->boot,s->boot,16)!=0)) r=-ESTALE;
    return r;
}
static int respond(sd_bus_message *message,const struct ae_dispatch_snapshot *d)
{
    sd_bus_message *reply=NULL; int r=sd_bus_message_new_method_return(message,&reply);
    if (r>=0) r=sd_bus_message_append_array(reply,'y',d->operation,16);
    if (r>=0) r=sd_bus_message_append(reply,"t",(uint64_t)d->revision);
    if (r>=0) r=sd_bus_send(NULL,reply,NULL);
    sd_bus_message_unref(reply); return r<0 ? r : 1;
}
int ae_setup_method_prepare(sd_bus_message *message,struct ae_setup_service *s,sd_bus_error *error)
{
    struct ae_launch_context current; int r=s->broker(s->owner,message,&current);
    if (r!=0) return reply_error(error,-EACCES);
    uint8_t lease[16],requested[32]; uint64_t gate=0;
    r=array(message,lease,16);
    if (r==0 && sd_bus_message_read(message,"t",&gate)<=0) r=-EBADMSG;
    if (r==0 && (gate<1 || gate>INT64_MAX)) r=-EBADMSG;
    if (r==0) r=array(message,requested,32);
    struct ae_setup_profile p; uint8_t hash[32];
    if (r==0) r=profile(s,&p,hash);
    if (r==0 && (memcmp(lease,p.lease,16)!=0 || memcmp(hash,requested,32)!=0 || gate!=(uint64_t)p.data_gate_revision || sd_bus_message_at_end(message,1)!=1)) r=-ESTALE;
    if (r==0 && (s->peer.pidfd>=0 || s->terminal_only) && memcmp(s->lease,p.lease,16)!=0) {
        struct ae_reservation_receipt old;
        r=(s->terminal_only || ae_peer_alive(&s->peer)==-ESRCH) ? ae_catalogue_get_receipt(s->catalogue,s->lease,&current,&old) : -EBUSY;
        if (r==0 && !old.withdrawn) r=-EAGAIN;
        if (r==0) ae_setup_service_clear(s);
    }
    struct ae_dispatch_snapshot d;
    if (r==0) r=ae_catalogue_begin_setup(s->catalogue,&p,hash,&current,&d);
    return r!=0 ? reply_error(error,r) : respond(message,&d);
}
static int setup_peer(sd_bus_message *message,struct ae_setup_service *s,struct ae_peer *peer,struct ae_process_context *context,
    struct ae_setup_profile *p,uint8_t hash[32])
{
    int r=ae_peer_capture(message,peer);
    if (r==0 && (peer->connection_uid!=0 || strcmp(peer->connection_label,"system_u:system_r:aetheri_setup_t:s0")!=0)) r=-EACCES;
    if (r==0) r=profile(s,p,hash);
    uint8_t lease[16],requested[32];
    if (r==0) r=array(message,lease,16);
    if (r==0) r=array(message,requested,32);
    if (r==0 && (memcmp(p->lease,lease,16)!=0 || memcmp(hash,requested,32)!=0 || sd_bus_message_at_end(message,1)!=1)) r=-ESTALE;
    if (r==0) r=ae_process_observe_registration(peer,s->bus,context);
    char expected[80]; static const char hex[]="0123456789abcdef";
    if (r==0) {
        memcpy(expected,"aetheri-app-",12);
        for (size_t n=0;n<16;++n) { expected[12+2*n]=hex[p->launch[n]>>4]; expected[13+2*n]=hex[p->launch[n]&15]; }
        memcpy(expected+44,".service",9);
    }
    if (r==0 && (context->uid!=0 || memcmp(context->boot,p->boot,16)!=0 || strcmp(context->unit,expected)!=0 || strcmp(context->label,"system_u:system_r:aetheri_setup_t:s0")!=0)) r=-EACCES;
    return r;
}
int ae_setup_method_register(sd_bus_message *message,struct ae_setup_service *s,sd_bus_error *error)
{
    struct ae_peer peer=AE_PEER_INIT; struct ae_process_context context; struct ae_setup_profile p; uint8_t hash[32],evidence[32];
    int r=setup_peer(message,s,&peer,&context,&p,hash);
    if (r==0 && s->terminal_only) r=-EEXIST;
    if (r==0 && s->peer.pidfd>=0 && (strcmp(s->peer.unique_name,peer.unique_name)!=0 || !ae_process_context_equal(&s->context,&context) || memcmp(s->lease,p.lease,16)!=0 || memcmp(s->digest,hash,32)!=0)) r=-EEXIST;
    if (r==0) r=ae_setup_context_digest(&context,hash,evidence);
    if (r==0 && !s->pinned) {
        r=sd_bus_call_method(s->bus,"org.freedesktop.systemd1","/org/freedesktop/systemd1","org.freedesktop.systemd1.Manager","RefUnit",NULL,NULL,"s",context.unit);
        if (r>=0) { s->pinned=1; r=0; }
    }
    if (r==0 && s->peer.pidfd<0) {
        s->peer=peer; peer.pidfd=-1; s->context=context; s->terminal_only=0;
        memcpy(s->lease,p.lease,16); memcpy(s->digest,hash,32); memcpy(s->evidence,evidence,32);
    }
    struct ae_dispatch_snapshot d;
    if (r==0) r=ae_catalogue_register_setup(s->catalogue,&p,hash,context.invocation,evidence,&d);
    if (r==0 && strcmp(d.state,"reserved")!=0) r=-EAGAIN;
    ae_peer_clear(&peer); return r!=0 ? reply_error(error,r) : respond(message,&d);
}
int ae_setup_method_prepared(sd_bus_message *message,struct ae_setup_service *s,sd_bus_error *error)
{
    struct ae_peer peer=AE_PEER_INIT; struct ae_process_context context; struct ae_setup_profile p; uint8_t hash[32];
    int r=setup_peer(message,s,&peer,&context,&p,hash);
    if (r==0 && (s->peer.pidfd<0 || strcmp(s->peer.unique_name,peer.unique_name)!=0 || !ae_process_context_equal(&s->context,&context) || memcmp(s->lease,p.lease,16)!=0 || memcmp(s->digest,hash,32)!=0)) r=-EACCES;
    struct ae_dispatch_snapshot d;
    if (r==0) r=ae_catalogue_prepared_setup(s->catalogue,&p,hash,context.invocation,s->evidence,&d);
    ae_peer_clear(&peer); return r!=0 ? reply_error(error,r) : respond(message,&d);
}
static int supervisor_drained(struct ae_setup_service *s)
{
    if (!s->pinned) return -EAGAIN;
    sd_bus_message *unit=NULL,*job=NULL,*inv=NULL; const char *path=NULL,*job_path=NULL; char *state=NULL,*group=NULL; uint32_t pid=1,job_id=1;
    int r=sd_bus_call_method(s->bus,"org.freedesktop.systemd1","/org/freedesktop/systemd1","org.freedesktop.systemd1.Manager","GetUnit",NULL,&unit,"s",s->context.unit);
    if (r>=0 && sd_bus_message_read(unit,"o",&path)<=0) r=-EBADMSG;
    if (r>=0) r=sd_bus_get_property_string(s->bus,"org.freedesktop.systemd1",path,"org.freedesktop.systemd1.Unit","ActiveState",NULL,&state);
    if (r>=0 && strcmp(state,"inactive")!=0 && strcmp(state,"failed")!=0) r=-EAGAIN;
    if (r>=0) r=sd_bus_get_property_string(s->bus,"org.freedesktop.systemd1",path,"org.freedesktop.systemd1.Service","ControlGroup",NULL,&group);
    if (r>=0 && group[0]!=0) r=-EAGAIN;
    if (r>=0) r=sd_bus_get_property_trivial(s->bus,"org.freedesktop.systemd1",path,"org.freedesktop.systemd1.Service","MainPID",NULL,'u',&pid);
    if (r>=0 && pid!=0) r=-EAGAIN;
    if (r>=0) r=sd_bus_get_property(s->bus,"org.freedesktop.systemd1",path,"org.freedesktop.systemd1.Unit","Job",NULL,&job,"(uo)");
    if (r>=0 && (sd_bus_message_read(job,"(uo)",&job_id,&job_path)<=0 || job_id!=0)) r=-EAGAIN;
    if (r>=0) r=sd_bus_get_property(s->bus,"org.freedesktop.systemd1",path,"org.freedesktop.systemd1.Unit","InvocationID",NULL,&inv,"ay");
    const void *value=NULL; size_t size=0;
    if (r>=0 && (sd_bus_message_read_array(inv,'y',&value,&size)<=0 || size!=16 || memcmp(value,s->context.invocation,16)!=0)) r=-ESTALE;
    free(state); free(group); sd_bus_message_unref(unit); sd_bus_message_unref(job); sd_bus_message_unref(inv);
    return r<0 ? r : 0;
}
int ae_setup_observe_exit(struct ae_setup_service *s,const struct ae_setup_profile *p,const uint8_t hash[32])
{
    if (s==NULL || p==NULL || hash==NULL || s->peer.pidfd>=0 || s->pinned) return -EINVAL;
    int r=terminal_setup(s,p,hash);
    if (r==0) r=supervisor_drained(s);
    if (r!=0) ae_setup_service_clear(s);
    return r;
}
int ae_setup_method_withdraw(sd_bus_message *message,struct ae_setup_service *s,sd_bus_error *error)
{
    struct ae_launch_context context; int r=s->broker(s->owner,message,&context);
    if (r!=0) return reply_error(error,-EACCES);
    struct ae_setup_profile p; uint8_t hash[32],lease[16],cause[16];
    r=array(message,lease,16); if (r==0) r=array(message,cause,16);
    if (r==0) r=profile(s,&p,hash);
    if (r==0 && (memcmp(lease,p.lease,16)!=0 || memcmp(cause,p.user_operation,16)!=0 || sd_bus_message_at_end(message,1)!=1)) r=-ESTALE;
    if (r==0 && s->peer.pidfd<0 && !s->terminal_only) {
        r=ae_setup_observe_exit(s,&p,hash);
        if (r==0) { s->terminal_only=1; memcpy(s->digest,hash,32); memcpy(s->lease,p.lease,16); }
    }
    if (r==0 && (memcmp(hash,s->digest,32)!=0 || memcmp(s->lease,p.lease,16)!=0)) r=-ESTALE;
    struct ae_reservation_receipt old;
    if (r==0) r=ae_catalogue_get_receipt(s->catalogue,p.lease,&context,&old);
    if (r==0 && !old.withdrawn && !s->terminal_only && (s->peer.pidfd<0 || ae_peer_alive(&s->peer)!=-ESRCH)) r=-EAGAIN;
    if (r==0 && !old.withdrawn) r=supervisor_drained(s);
    struct ae_dispatch_snapshot d;
    if (r==0 && !old.withdrawn) r=s->terminal_only ?
        ae_catalogue_record_setup_exit(s->catalogue,&p,hash,s->context.invocation,s->evidence,&d) :
        ae_catalogue_register_setup(s->catalogue,&p,hash,s->context.invocation,s->evidence,&d);
    struct timespec now; int64_t stamp=0;
    if (r==0 && clock_gettime(CLOCK_REALTIME,&now)!=0) r=-errno;
    if (r==0 && (now.tv_sec<0 || (uint64_t)now.tv_sec>(uint64_t)(INT64_MAX-999)/1000 || now.tv_nsec<0 || now.tv_nsec>=1000000000)) r=-EOVERFLOW;
    if (r==0) stamp=(int64_t)now.tv_sec*1000+now.tv_nsec/1000000;
    if (r==0) r=ae_catalogue_withdraw_setup(s->catalogue,&p,hash,s->context.invocation,s->evidence,&context,stamp,&d);
    if (r==0 && s->pinned) {
        int unref=sd_bus_call_method(s->bus,"org.freedesktop.systemd1","/org/freedesktop/systemd1","org.freedesktop.systemd1.Manager","UnrefUnit",NULL,NULL,"s",s->context.unit);
        if (unref>=0) s->pinned=0;
    }
    return r!=0 ? reply_error(error,r) : respond(message,&d);
}
void ae_setup_service_clear(struct ae_setup_service *s)
{
    if (s->pinned && s->bus!=NULL) (void)sd_bus_call_method(s->bus,"org.freedesktop.systemd1","/org/freedesktop/systemd1","org.freedesktop.systemd1.Manager","UnrefUnit",NULL,NULL,"s",s->context.unit);
    s->pinned=0; s->terminal_only=0; ae_peer_clear(&s->peer);
    memset(&s->context,0,sizeof s->context); memset(s->lease,0,sizeof s->lease); memset(s->digest,0,sizeof s->digest); memset(s->evidence,0,sizeof s->evidence);
}
int ae_setup_recover_unbound(struct ae_catalogue *catalogue,const uint8_t boot[16])
{
    struct ae_setup_profile p; uint8_t hash[32]; int r=ae_setup_load(&p,hash);
    if (r==0 && memcmp(p.boot,boot,16)!=0) r=-ESTALE;
    struct ae_setup_service s={.catalogue=catalogue,.peer=AE_PEER_INIT};
    if (r==0) r=sd_bus_new(&s.bus);
    if (r>=0) r=sd_bus_set_address(s.bus,"unix:path=/run/dbus/system_bus_socket");
    if (r>=0) r=sd_bus_set_bus_client(s.bus,1);
    if (r>=0) r=sd_bus_set_method_call_timeout(s.bus,2000000);
    if (r>=0) r=sd_bus_start(s.bus);
    if (r>=0) r=ae_setup_observe_exit(&s,&p,hash);
    struct ae_dispatch_snapshot output; struct ae_launch_context context={.uid=1000};
    if (r==0) {
        memcpy(context.system_runtime,p.system_runtime,16); memcpy(context.broker_epoch,p.broker_epoch,16); memcpy(context.broker_runtime,p.broker_runtime,16);
        memcpy(context.account,p.account,16); memcpy(context.boot,p.boot,16);
        r=ae_catalogue_record_setup_exit(catalogue,&p,hash,s.context.invocation,s.evidence,&output);
    }
    struct timespec now;
    if (r==0 && clock_gettime(CLOCK_REALTIME,&now)!=0) r=-errno;
    if (r==0 && (now.tv_sec<0 || (uint64_t)now.tv_sec>(uint64_t)(INT64_MAX-999)/1000)) r=-EOVERFLOW;
    if (r==0) r=ae_catalogue_withdraw_setup(catalogue,&p,hash,s.context.invocation,s.evidence,&context,(int64_t)now.tv_sec*1000+now.tv_nsec/1000000,&output);
    ae_setup_service_clear(&s); sd_bus_flush_close_unref(s.bus); return r;
}
