#include "authority/execution_admission.h"
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct ae_execution_admission {
    pid_t owner;
    struct ae_execution_endpoint *endpoint;
    struct ae_broker *broker;
    sd_bus *bus;
    struct ae_execution_profile profile;
    struct ae_peer original;
    int busy,attempted;
};
static int enter(struct ae_execution_admission *a)
{
    if (a==NULL) return -EINVAL;
    if (a->owner!=getpid()) return -ECHILD;
    if (a->busy) return -EBUSY;
    a->busy=1; return 0;
}
static int current(struct ae_execution_admission *a,const struct ae_peer *peer,struct ae_launch_context *c)
{
    int r=ae_broker_verify(a->broker,peer,a->bus);
    const struct ae_broker_identity *b=&a->broker->identity;
    const struct ae_execution_profile *p=&a->profile;
    if (r==0 && (memcmp(b->account,p->account,16)!=0 || memcmp(b->epoch,p->broker_epoch,16)!=0 ||
        memcmp(b->runtime,p->broker_runtime,16)!=0 || memcmp(a->broker->context.boot,p->boot,16)!=0)) r=-ESTALE;
    if (r==0) {
        *c=(struct ae_launch_context){.uid=(uint32_t)peer->connection_uid};
        memcpy(c->system_runtime,p->system_runtime,16); memcpy(c->boot,p->boot,16);
        memcpy(c->account,b->account,16); memcpy(c->broker_epoch,b->epoch,16); memcpy(c->broker_runtime,b->runtime,16);
    }
    return r;
}
int ae_execution_admission_new(struct ae_execution_endpoint *endpoint,struct ae_broker *broker,sd_bus *bus,
    const struct ae_execution_profile *profile,struct ae_execution_admission **output)
{
    if (endpoint==NULL || broker==NULL || bus==NULL || profile==NULL || output==NULL || *output!=NULL) return -EINVAL;
    if (getuid()!=0 || geteuid()!=0 || getgid()!=0 || getegid()!=0) return -EPERM;
    uint8_t hash[32]; int r=ae_execution_digest(profile,hash); if (r!=0) return r;
    r=ae_execution_endpoint_check_profile(endpoint,profile); if (r!=0) return r;
    struct ae_execution_admission *a=calloc(1,sizeof *a); if (a==NULL) return -ENOMEM;
    a->owner=getpid(); a->endpoint=endpoint; a->broker=broker; a->bus=bus; a->profile=*profile;
    a->original=(struct ae_peer)AE_PEER_INIT; *output=a; return 0;
}
int ae_execution_admission_request(struct ae_execution_admission *a,const struct ae_peer *peer,struct ae_execution_result *output)
{
    int r=enter(a); if (r!=0) return r;
    struct ae_launch_context c;
    if (output==NULL || peer==NULL) r=-EINVAL;
    if (r==0 && a->attempted) r=-EALREADY;
    if (r==0) r=current(a,peer,&c);
    int pin=-1;
    if (r==0) { pin=fcntl(peer->pidfd,F_DUPFD_CLOEXEC,3); if (pin<0) r=-errno; }
    if (r==0) {
        a->original=*peer; a->original.pidfd=pin; a->attempted=1;
        r=ae_execution_endpoint_request(a->endpoint,&c,output);
    }
    a->busy=0; return r;
}
int ae_execution_admission_arm(struct ae_execution_admission *a,struct ae_execution_result *output)
{
    int r=enter(a); if (r!=0) return r;
    struct ae_launch_context c;
    if (output==NULL) r=-EINVAL;
    if (r==0 && (!a->attempted || a->original.pidfd<0)) r=-EAGAIN;
    if (r==0) r=current(a,&a->original,&c);
    if (r==0) r=ae_execution_endpoint_arm(a->endpoint,&c,output);
    a->busy=0; return r;
}
int ae_execution_admission_close(struct ae_execution_admission **output)
{
    if (output==NULL) return -EINVAL;
    struct ae_execution_admission *a=*output; if (a==NULL) return 0;
    int r=enter(a); if (r!=0) return r;
    ae_peer_clear(&a->original); free(a); *output=NULL; return 0;
}
