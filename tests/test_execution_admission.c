/* Controlled endpoint/broker adapters; real process-owned pidfd lifetimes.
 * Actual peer/unit/policy evidence belongs to test_execution_runtime.py. */
#include "authority/execution_admission.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/pidfd.h>
#include <sys/wait.h>
#include <unistd.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"execution admission line %d: %s\n",__LINE__,#x); exit(1); } } while (0)
static struct ae_execution_profile profile;
static struct ae_execution_admission *active;
static int verification_error,request_error,requests,arms,reentrant;
static void id(uint8_t p[16],uint8_t n) { memset(p,0,16); p[6]=0x40; p[8]=0x80; p[15]=n; }
static int fds(void)
{
    DIR *dir=opendir("/proc/self/fd"); CHECK(dir); int count=0; struct dirent *e;
    while ((e=readdir(dir))!=NULL) if (e->d_name[0]!='.') ++count;
    CHECK(closedir(dir)==0); return count;
}
int __wrap_ae_broker_verify(struct ae_broker *broker,const struct ae_peer *peer,sd_bus *bus)
{
    CHECK(broker && peer && bus && fcntl(peer->pidfd,F_GETFD)>=0 && peer->connection_uid==1000);
    return verification_error;
}
int __wrap_ae_execution_endpoint_check_profile(struct ae_execution_endpoint *endpoint,const struct ae_execution_profile *p)
{
    CHECK(endpoint); uint8_t actual[32],expected[32]; int r=ae_execution_digest(p,actual);
    CHECK(ae_execution_digest(&profile,expected)==0); return r!=0 ? r : memcmp(actual,expected,32)==0 ? 0 : -ESTALE;
}
static void context(const struct ae_launch_context *c)
{
    CHECK(c->uid==1000 && memcmp(c->account,profile.account,16)==0 && memcmp(c->boot,profile.boot,16)==0 &&
        memcmp(c->system_runtime,profile.system_runtime,16)==0 && memcmp(c->broker_epoch,profile.broker_epoch,16)==0 &&
        memcmp(c->broker_runtime,profile.broker_runtime,16)==0);
    if (reentrant) CHECK(ae_execution_admission_close(&active)==-EBUSY && active!=NULL);
}
int __wrap_ae_execution_endpoint_request(struct ae_execution_endpoint *endpoint,const struct ae_launch_context *c,struct ae_execution_result *output)
{
    CHECK(endpoint && output); context(c); ++requests;
    if (request_error) return request_error;
    *output=(struct ae_execution_result){.effect_required=1}; return 0;
}
int __wrap_ae_execution_endpoint_arm(struct ae_execution_endpoint *endpoint,const struct ae_launch_context *c,struct ae_execution_result *output)
{ CHECK(endpoint && output); context(c); ++arms; *output=(struct ae_execution_result){.effect_required=1}; return 0; }
int main(void)
{
    CHECK(geteuid()==0 && getegid()==0);
    uint8_t *ids[]={profile.system_epoch,profile.system_runtime,profile.broker_epoch,profile.broker_runtime,profile.boot,profile.account,
        profile.install,profile.generation,profile.data,profile.storage,profile.launch,profile.lease,profile.root_operation,profile.user_operation};
    for (uint8_t n=0;n<14;++n) id(ids[n],(uint8_t)(n+1));
    profile.install_revision=2; profile.install_gate_revision=3; profile.data_gate_revision=3;
    struct ae_launch_request q={.install_revision=2}; strcpy(q.action,"run"); strcpy(q.session,"p0"); strcpy(q.seat,"none");
    const uint8_t *from[]={profile.system_epoch,profile.lease,profile.launch,profile.broker_epoch,profile.broker_runtime,profile.account,profile.install,profile.generation,profile.data,profile.boot};
    uint8_t *to[]={q.system_epoch,q.lease,q.launch,q.broker_epoch,q.broker_runtime,q.account,q.install,q.generation,q.data,q.boot};
    for (size_t n=0;n<10;++n) memcpy(to[n],from[n],16);
    CHECK(ae_launch_fingerprint(&q,profile.request_digest)==0);
    struct ae_broker broker=AE_BROKER_INIT;
    memcpy(broker.identity.account,profile.account,16); memcpy(broker.identity.epoch,profile.broker_epoch,16); memcpy(broker.identity.runtime,profile.broker_runtime,16);
    memcpy(broker.context.boot,profile.boot,16);
    struct ae_peer peer=AE_PEER_INIT; peer.connection_uid=1000; peer.connection_pid=getpid(); peer.pidfd=pidfd_open(getpid(),0); CHECK(peer.pidfd>=0);
    broker.peer.pidfd=fcntl(peer.pidfd,F_DUPFD_CLOEXEC,3); CHECK(broker.peer.pidfd>=0);
    struct ae_execution_endpoint *endpoint=(struct ae_execution_endpoint *)&broker;
    sd_bus *bus=(sd_bus *)&peer; int baseline=fds();
    struct ae_execution_profile wrong=profile; wrong.starter_digest[0]=1;
    CHECK(ae_execution_admission_new(endpoint,&broker,bus,&wrong,&active)==-ESTALE && active==NULL);
    CHECK(ae_execution_admission_new(NULL,&broker,bus,&profile,&active)==-EINVAL);
    CHECK(ae_execution_admission_new(endpoint,&broker,bus,&profile,&active)==0);
    CHECK(ae_execution_admission_new(endpoint,&broker,bus,&profile,&active)==-EINVAL);
    struct ae_execution_result output,marker; memset(&marker,0xa5,sizeof marker); output=marker;
    CHECK(ae_execution_admission_arm(active,&output)==-EAGAIN && memcmp(&output,&marker,sizeof marker)==0 && arms==0);
    verification_error=-EACCES;
    CHECK(ae_execution_admission_request(active,&peer,&output)==-EACCES && requests==0 && memcmp(&output,&marker,sizeof marker)==0);
    verification_error=0; broker.identity.runtime[15]^=0x80;
    CHECK(ae_execution_admission_request(active,&peer,&output)==-ESTALE && requests==0); broker.identity.runtime[15]^=0x80;
    reentrant=1; CHECK(ae_execution_admission_request(active,&peer,&output)==0 && requests==1 && fds()==baseline+1);
    CHECK(ae_execution_admission_request(active,&peer,&output)==-EALREADY && requests==1);
    CHECK(close(peer.pidfd)==0); peer.pidfd=-1; /* Local method-capture copy is gone. */
    verification_error=-ENOTCONN;
    CHECK(ae_execution_admission_arm(active,&output)==-ENOTCONN && arms==0);
    verification_error=0; broker.identity.epoch[15]^=0x80;
    CHECK(ae_execution_admission_arm(active,&output)==-ESTALE && arms==0); broker.identity.epoch[15]^=0x80;
    CHECK(ae_execution_admission_arm(active,&output)==0 && arms==1); /* Own original pidfd retained. */
    pid_t child=fork(); CHECK(child>=0);
    if (child==0) { CHECK(ae_execution_admission_close(&active)==-ECHILD); _exit(0); }
    int status; CHECK(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==0);
    CHECK(ae_execution_admission_close(&active)==0 && active==NULL && fds()==baseline-1);
    peer.pidfd=pidfd_open(getpid(),0); CHECK(peer.pidfd>=0); request_error=-EIO;
    CHECK(ae_execution_admission_new(endpoint,&broker,bus,&profile,&active)==0); output=marker;
    CHECK(ae_execution_admission_request(active,&peer,&output)==-EIO && memcmp(&output,&marker,sizeof marker)==0 && requests==2);
    CHECK(ae_execution_admission_request(active,&peer,&output)==-EALREADY && requests==2);
    CHECK(ae_execution_admission_close(&active)==0 && ae_execution_admission_close(&active)==0);
    CHECK(close(peer.pidfd)==0 && close(broker.peer.pidfd)==0);
    puts("Admission context/replacement/lost-request guards, callback/fork ownership and independent original pidfd lifetime passed; actor/endpoint adapters controlled.");
    return 0;
}
