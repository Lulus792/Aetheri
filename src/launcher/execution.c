#include "launcher/execution.h"
#include "linux/exec_channel.h"
#include "execution-wire.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct ae_execution_client {
    pid_t owner;
    sd_bus *bus;
    struct ae_execution_profile profile;
    uint8_t digest[32];
    struct ae_peer root;
    struct ae_process_context context;
    int register_attempted,registered,prepared_attempted,error;
    uint64_t bound_revision;
};
static int unit_valid(const char *unit)
{
    if (unit==NULL) return 0;
    size_t n=strnlen(unit,256);
    if (n<9 || n>=256 || strcmp(unit+n-8,".service")!=0) return 0;
    for (size_t i=0;i<n;++i) {
        char c=unit[i];
        if (!((c>='a' && c<='z') || (c>='A' && c<='Z') || (c>='0' && c<='9') || c=='.' || c=='_' || c=='-' || c=='@')) return 0;
    }
    return 1;
}
static int owner(const struct ae_execution_client *c)
{ return c==NULL ? -EINVAL : c->owner==getpid() ? 0 : -ECHILD; }
static int current(struct ae_execution_client *c)
{
    if (c->error) return c->error;
    int r=ae_process_verify(&c->root,c->bus,&c->context);
    if (r!=0) c->error=r;
    return r;
}
static int blob(sd_bus_message *m,const uint8_t *wanted,size_t expected)
{
    const void *p=NULL; size_t size=0; int r=sd_bus_message_read_array(m,'y',&p,&size);
    return r<0 ? r : r==0 || size!=expected || memcmp(p,wanted,size)!=0 ? -ESTALE : 0;
}
static int discovery(sd_bus_message *m,const struct ae_execution_profile *p)
{
    if (!sd_bus_message_has_signature(m,"uuasayay")) return -EBADMSG;
    uint32_t major=0,minor=0; int r=sd_bus_message_read(m,"uu",&major,&minor);
    if (r<=0 || major!=1 || minor<1) return -EPROTONOSUPPORT;
    r=sd_bus_message_enter_container(m,'a',"s");
    if (r<=0) return r<0 ? r : -EBADMSG;
    unsigned int count=0; const char *capability=NULL;
    while ((r=sd_bus_message_read(m,"s",&capability))>0) {
        if (++count>16 || capability==NULL || strnlen(capability,129)>128) return -E2BIG;
    }
    if (r<0) return r;
    r=sd_bus_message_exit_container(m);
    if (r<0) return r;
    r=blob(m,p->system_epoch,16);
    if (r==0) r=blob(m,p->system_runtime,16);
    if (r==0 && sd_bus_message_at_end(m,1)!=1) r=-EBADMSG;
    return r;
}
static int response(struct ae_execution_client *c,sd_bus_message *request,sd_bus_message *reply)
{
    struct ae_peer peer=AE_PEER_INIT;
    int r=ae_peer_capture_reply(reply,request,&peer);
    if (r==0 && (memcmp(peer.bus_id,c->root.bus_id,16)!=0 || strcmp(peer.unique_name,c->root.unique_name)!=0 ||
        peer.connection_pid!=c->root.connection_pid || peer.connection_uid!=c->root.connection_uid ||
        strcmp(peer.connection_label,c->root.connection_label)!=0)) r=-ESTALE;
    if (r==0) r=current(c);
    if (r==0) r=ae_process_verify(&peer,c->bus,&c->context);
    ae_peer_clear(&peer);
    if (r!=0) c->error=r;
    return r;
}
int ae_execution_client_new(sd_bus *bus,const struct ae_execution_profile *p,const char *unit,struct ae_execution_client **output)
{
    if (bus==NULL || p==NULL || output==NULL || *output!=NULL || !unit_valid(unit)) return -EINVAL;
    if (getuid()!=0 || geteuid()!=0 || getgid()!=0 || getegid()!=0) return -EPERM;
    struct ae_execution_client *c=calloc(1,sizeof *c); if (c==NULL) return -ENOMEM;
    c->owner=getpid(); c->bus=bus; c->profile=*p; c->root=(struct ae_peer)AE_PEER_INIT;
    int r=ae_execution_digest(p,c->digest); sd_bus_creds *credentials=NULL;
    if (r==0) r=sd_bus_get_name_creds(bus,"org.aetheri.System1",SD_BUS_CREDS_UNIQUE_NAME,&credentials);
    const char *unique=NULL;
    if (r>=0 && ((sd_bus_creds_get_mask(credentials)&SD_BUS_CREDS_UNIQUE_NAME)==0 ||
        sd_bus_creds_get_unique_name(credentials,&unique)<0 || unique==NULL || unique[0]!=':' || !sd_bus_service_name_is_valid(unique))) r=-ENODATA;
    sd_bus_message *request=NULL,*reply=NULL;
    if (r>=0) r=sd_bus_message_new_method_call(bus,&request,unique,"/org/aetheri/Management1","org.aetheri.Management1","GetProtocolInfo");
    if (r>=0) r=sd_bus_call(bus,request,2000000,NULL,&reply);
    if (r>=0) r=ae_peer_capture_reply(reply,request,&c->root);
    if (r==0 && (c->root.connection_uid!=0 || strcmp(c->root.connection_label,"system_u:system_r:aetheri_system_t:s0")!=0)) r=-EACCES;
    if (r==0) r=ae_process_observe(&c->root,bus,&c->context);
    if (r==0 && (c->context.uid!=0 || strcmp(c->context.label,"system_u:system_r:aetheri_system_t:s0")!=0 ||
        strcmp(c->context.unit,unit)!=0 || memcmp(c->context.boot,p->boot,16)!=0)) r=-EACCES;
    if (r==0) r=discovery(reply,p);
    if (r==0) r=ae_process_verify(&c->root,bus,&c->context);
    sd_bus_message_unref(request); sd_bus_message_unref(reply); sd_bus_creds_unref(credentials);
    if (r!=0) { ae_peer_clear(&c->root); free(c); return r; }
    *output=c; return 0;
}
static int call(struct ae_execution_client *c,const char *method,sd_bus_message **request,sd_bus_message **reply)
{
    int r=current(c);
    if (r==0) r=sd_bus_message_new_method_call(c->bus,request,c->root.unique_name,AE_EXECUTION_PATH,AE_EXECUTION_INTERFACE,method);
    if (r>=0) r=sd_bus_message_append_array(*request,'y',c->profile.lease,16);
    if (r>=0) r=sd_bus_message_append_array(*request,'y',c->digest,32);
    if (r>=0) r=sd_bus_call(c->bus,*request,strcmp(method,AE_EXECUTION_PREPARED_METHOD)==0 ? 5000000 : 2000000,NULL,reply);
    if (r>=0) r=response(c,*request,*reply);
    return r;
}
static int receipt(struct ae_execution_client *c,sd_bus_message *reply,int registration,uint64_t *revision,int *writer)
{
    if (!sd_bus_message_has_signature(reply,registration ? AE_EXECUTION_REGISTER_OUTPUT : AE_EXECUTION_PREPARED_OUTPUT)) return -EBADMSG;
    const uint8_t *fields[]={c->profile.system_epoch,c->profile.system_runtime,c->profile.boot,c->profile.lease,c->profile.root_operation,c->digest};
    int r=0; for (size_t n=0;r==0 && n<6;++n) r=blob(reply,fields[n],n==5 ? 32U : 16U);
    uint64_t rev=0;
    if (r==0 && sd_bus_message_read(reply,"t",&rev)<=0) r=-EBADMSG;
    if (r==0 && (rev<1 || rev>INT64_MAX || (!registration && rev<=c->bound_revision))) r=-ESTALE;
    int borrowed=-1,fd=-1;
    if (r==0 && registration && sd_bus_message_read(reply,"h",&borrowed)<=0) r=-EBADMSG;
    if (r==0 && sd_bus_message_at_end(reply,1)!=1) r=-EBADMSG;
    if (r==0 && registration) r=ae_exec_writer_check(borrowed,0,0);
    if (r==0 && registration) { fd=fcntl(borrowed,F_DUPFD_CLOEXEC,3); if (fd<0) r=-errno; }
    if (r==0) r=current(c);
    if (r==0) { *revision=rev; if (registration) *writer=fd; }
    else if (fd>=0) close(fd);
    return r;
}
int ae_execution_client_register(struct ae_execution_client *c,int *writer)
{
    int r=owner(c); if (r==0 && (writer==NULL || *writer>=0)) r=-EINVAL;
    if (r==0 && c->register_attempted) r=-EALREADY;
    if (r!=0) return r;
    c->register_attempted=1;
    sd_bus_message *request=NULL,*reply=NULL;
    r=call(c,AE_EXECUTION_REGISTER_METHOD,&request,&reply);
    uint64_t revision=0; int fd=-1;
    if (r==0) r=receipt(c,reply,1,&revision,&fd);
    sd_bus_message_unref(request); sd_bus_message_unref(reply);
    if (r==0) { c->registered=1; c->bound_revision=revision; *writer=fd; }
    else c->error=r;
    return r;
}
int ae_execution_client_prepared(struct ae_execution_client *c)
{
    int r=owner(c); if (r==0 && (!c->registered || c->prepared_attempted)) r=-EALREADY;
    if (r!=0) return r;
    c->prepared_attempted=1;
    sd_bus_message *request=NULL,*reply=NULL;
    r=call(c,AE_EXECUTION_PREPARED_METHOD,&request,&reply);
    uint64_t revision=0;
    if (r==0) r=receipt(c,reply,0,&revision,NULL);
    sd_bus_message_unref(request); sd_bus_message_unref(reply);
    if (r!=0) c->error=r;
    return r;
}
int ae_execution_client_close(struct ae_execution_client **output)
{
    if (output==NULL) return -EINVAL;
    struct ae_execution_client *c=*output; if (c==NULL) return 0;
    int r=owner(c); if (r!=0) return r;
    ae_peer_clear(&c->root); free(c); *output=NULL; return 0;
}
