#include "authority/execution.h"
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <sqlite3.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/pidfd.h>
#include <sys/wait.h>
#include <unistd.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"execution service line %d: %s\n",__LINE__,#x); exit(1); } } while (0)
void ae_test_execution_commit_fault(int);
struct test_state {
    struct ae_catalogue *store;
    const struct ae_execution_profile *profile;
    struct ae_launch_context *context;
    const char *directory,*scenario;
    sd_bus *bus;
    struct ae_execution_service **service;
    int requests,releases,refs,writer,control,pidfd,context_bad,drift,channel_fault;
    pid_t child;
    char unit[80];
};
static struct test_state *active;
int __real_ae_exec_channel_read(struct ae_exec_channel *,struct ae_exec_report *);
int __wrap_ae_exec_channel_read(struct ae_exec_channel *channel,struct ae_exec_report *report)
{
    if (active && active->channel_fault) { active->channel_fault=0; return -EIO; }
    return __real_ae_exec_channel_read(channel,report);
}
static int64_t scalar(const struct test_state *s,const char *sql)
{
    char path[4096]; int n=snprintf(path,sizeof path,"%s/catalogue.sqlite3",s->directory); CHECK(n>0 && n<(int)sizeof path);
    sqlite3 *db=NULL; sqlite3_stmt *statement=NULL;
    CHECK(sqlite3_open_v2(path,&db,SQLITE_OPEN_READONLY,NULL)==SQLITE_OK);
    CHECK(sqlite3_prepare_v2(db,sql,-1,&statement,NULL)==SQLITE_OK && sqlite3_step(statement)==SQLITE_ROW);
    int64_t value=sqlite3_column_int64(statement,0);
    CHECK(sqlite3_finalize(statement)==SQLITE_OK && sqlite3_close(db)==SQLITE_OK); return value;
}
/* Controlled kernel/PID1 adapters only. Actual process pin, pipe/event loop and
 * catalogue transactions remain real. This is not live MAC/unit admission. */
int __wrap_ae_process_observe_registration(const struct ae_peer *peer,sd_bus *bus,struct ae_process_context *output)
{
    struct test_state *s=active;
    CHECK(s && bus==s->bus && peer->connection_pid==s->child && ae_peer_alive(peer)==0);
    struct ae_process_context c={.uid=0,.cgroup_id=s->drift ? 124 : 123};
    memcpy(c.boot,s->profile->boot,16); c.invocation[6]=0x40; c.invocation[8]=0x80; c.invocation[15]=66;
    strcpy(c.unit,s->unit); strcpy(c.cgroup,"/system.slice/selected"); strcpy(c.label,"system_u:system_r:aetheri_starter_t:s0");
    if (s->context_bad==1) c.uid=1000;
    if (s->context_bad==2) strcpy(c.unit,"foreign.service");
    if (s->context_bad==3) c.boot[15]^=1;
    if (s->context_bad==4) strcpy(c.label,"system_u:system_r:aetheri_setup_t:s0");
    *output=c; return 0;
}
int __wrap_sd_bus_call_method(sd_bus *bus,const char *destination,const char *path,const char *interface,const char *method,
    sd_bus_error *error,sd_bus_message **reply,const char *signature,...)
{
    (void)error; (void)reply;
    struct test_state *s=active;
    CHECK(s && bus==s->bus && strcmp(destination,"org.freedesktop.systemd1")==0 && strcmp(path,"/org/freedesktop/systemd1")==0 &&
        strcmp(interface,"org.freedesktop.systemd1.Manager")==0 && strcmp(signature,"s")==0);
    va_list args; va_start(args,signature); const char *unit=va_arg(args,const char *); va_end(args); CHECK(strcmp(unit,s->unit)==0);
    if (strcmp(method,"RefUnit")==0) { ++s->refs; return 1; }
    CHECK(strcmp(method,"UnrefUnit")==0); --s->refs; CHECK(s->refs==0); return 1;
}
static int request(void *owner,const struct ae_execution_profile *p,const struct ae_dispatch_snapshot *d)
{
    struct test_state *s=owner; ++s->requests; CHECK(s->requests==1 && p!=s->profile); /* profile is an owned copy */
    CHECK(ae_execution_service_close(s->service)==-EBUSY && *s->service);
    CHECK(memcmp(p->launch,s->profile->launch,16)==0 && strcmp(d->state,"reserved")==0);
    struct ae_dispatch_snapshot observed;
    /* GetDispatch starts its own transaction: success proves no outer SQLite
     * transaction spans this external callback. A separate reader sees intent. */
    CHECK(ae_catalogue_get_dispatch(s->store,p->lease,s->context,&observed)==0);
    CHECK(scalar(s,"SELECT count(*) FROM operation_steps WHERE step_no=1 AND action='stage' AND subject_kind='generation' AND state='executing'")==1);
    int pipe[2]; CHECK(pipe2(pipe,O_CLOEXEC)==0); s->child=fork(); CHECK(s->child>=0);
    if (s->child==0) { close(pipe[1]); char value; CHECK(read(pipe[0],&value,1)==0); close(pipe[0]); _exit(0); }
    CHECK(close(pipe[0])==0); s->control=pipe[1]; s->pidfd=pidfd_open(s->child,0); CHECK(s->pidfd>=3);
    return strstr(s->scenario,"request-timeout") ? -ETIMEDOUT : 0;
}
static int release(void *owner,const struct ae_execution_profile *p,const struct ae_dispatch_snapshot *d)
{
    struct test_state *s=owner; ++s->releases; CHECK(s->releases==1 && strcmp(d->state,"dispatching")==0 && d->invocation_present);
    CHECK(ae_execution_service_close(s->service)==-EBUSY && *s->service);
    struct ae_dispatch_snapshot observed;
    CHECK(ae_catalogue_get_dispatch(s->store,p->lease,s->context,&observed)==0 && observed.invocation_present);
    CHECK(scalar(s,"SELECT count(*) FROM operation_steps WHERE step_no=3 AND state='executing'")==1);
    CHECK(scalar(s,"SELECT state='running' FROM generation_leases")==1);
    uint8_t bytes[32]={0}; memcpy(bytes,"AETHXER1",8); bytes[11]=AE_EXEC_UID; bytes[15]=EPERM; memcpy(bytes+16,bytes,16);
    if (!strstr(s->scenario,"unknown")) {
        size_t size=strstr(s->scenario,"malformed") ? 32U : 16U;
        CHECK(write(s->writer,bytes,size)==(ssize_t)size);
    }
    CHECK(close(s->writer)==0); s->writer=-1;
    return strstr(s->scenario,"release-timeout") ? -ETIMEDOUT : 0;
}
static void observed(struct ae_execution_service *service,struct test_state *s,enum ae_exec_channel_state state)
{
    struct ae_execution_service_status status;
    CHECK(ae_execution_service_status(service,s->context,&status)==0 && status.channel.state==state);
    if (state==AE_EXEC_CHANNEL_FAILURE) CHECK(status.channel.phase==AE_EXEC_UID && status.channel.error==EPERM);
    else CHECK(!status.channel.phase && !status.channel.error);
}
void ae_test_execution_service(struct ae_catalogue *store,const struct ae_execution_profile *p,struct ae_launch_context *context,
    const char *directory,const char *scenario)
{
    struct test_state s={.store=store,.profile=p,.context=context,.directory=directory,.scenario=scenario,.writer=-1,.control=-1,.pidfd=-1}; active=&s;
    sd_event *event=NULL; CHECK(sd_event_new(&event)>=0 && sd_bus_new(&s.bus)>=0);
    struct ae_execution_effects effects={.owner=&s,.request=request,.release=release};
    struct ae_execution_service *service=NULL;
    s.service=&service;
    struct ae_execution_result result,marker; memset(&marker,0xa5,sizeof marker); result=marker;
    CHECK(ae_execution_service_new(store,s.bus,event,p,context,&effects,&service)==0);
    CHECK(ae_execution_service_new(store,s.bus,event,p,context,&effects,&service)==-EINVAL);
    if (strcmp(scenario,"exec-service-failure")==0) {
        pid_t probe=fork(); CHECK(probe>=0);
        if (probe==0) {
            struct ae_execution_service_status status; struct ae_execution_service *inherited=service,*fresh=NULL;
            CHECK(ae_execution_service_status(service,context,&status)==-ECHILD && ae_execution_service_close(&inherited)==-ECHILD && inherited==service);
            CHECK(setgroups(0,NULL)==0 && setresgid(1000,1000,1000)==0 && setresuid(1000,1000,1000)==0);
            CHECK(ae_execution_service_new(store,s.bus,event,p,context,&effects,&fresh)==-EPERM && fresh==NULL); _exit(0);
        }
        int probe_status; CHECK(waitpid(probe,&probe_status,0)==probe && WIFEXITED(probe_status) && WEXITSTATUS(probe_status)==0);
    }
    struct ae_launch_context foreign=*context; foreign.broker_runtime[15]++;
    CHECK(ae_execution_service_request(service,&foreign,&result)==-ESTALE && memcmp(&result,&marker,sizeof result)==0);
    struct ae_dispatch_snapshot snapshot; CHECK(ae_catalogue_get_dispatch(store,p->lease,context,&snapshot)==0); strcpy(s.unit,snapshot.unit);
    if (strstr(scenario,"request-commit-lost")) ae_test_execution_commit_fault(2);
    int r=ae_execution_service_request(service,context,&result);
    int request_lost=strstr(scenario,"request-commit-lost")!=NULL,request_timeout=strstr(scenario,"request-timeout")!=NULL;
    CHECK(r==(request_lost ? -EIO : request_timeout ? -ETIMEDOUT : 0));
    if (r<0) CHECK(memcmp(&result,&marker,sizeof result)==0);
    CHECK(ae_execution_service_request(service,context,&result)==-EALREADY && s.requests==!request_lost);
    if (request_lost) {
        struct ae_execution_service_status status; CHECK(ae_execution_service_status(service,context,&status)==0);
        CHECK(!status.requested && status.request_error==-EIO && strcmp(status.dispatch.state,"reserved")==0);
        goto done;
    }
    struct ae_peer peer=AE_PEER_INIT; peer.pidfd=s.pidfd; peer.connection_pid=s.child; peer.connection_uid=0;
    memset(peer.bus_id,0x11,16); strcpy(peer.unique_name,":1.117"); strcpy(peer.connection_label,"system_u:system_r:aetheri_starter_t:s0");
    for (int bad=0;bad<6;++bad) {
        struct ae_peer rejected=peer;
        if (bad==0) rejected.connection_uid=1000;
        if (bad==1) strcpy(rejected.connection_label,"system_u:system_r:aetheri_setup_t:s0");
        s.context_bad=bad>=2 ? bad-1 : 0;
        int writer=-777; result=marker;
        CHECK(ae_execution_service_bind(service,&rejected,&writer,&result)==-EACCES && writer==-777 && memcmp(&result,&marker,sizeof result)==0 && !s.refs);
    }
    s.context_bad=0;
    if (strstr(scenario,"bind-commit-lost")) ae_test_execution_commit_fault(2);
    int writer=-777; result=marker; r=ae_execution_service_bind(service,&peer,&writer,&result);
    int bind_lost=strstr(scenario,"bind-commit-lost")!=NULL;
    CHECK(r==(bind_lost ? -EIO : 0) && s.refs==1);
    if (bind_lost) {
        CHECK(writer==-777 && memcmp(&result,&marker,sizeof result)==0);
        CHECK(ae_execution_service_bind(service,&peer,&writer,&result)==-EALREADY);
        CHECK(sd_event_run(event,1000000)>0); observed(service,&s,AE_EXEC_CHANNEL_UNKNOWN);
        CHECK(sd_event_run(event,0)==0 && scalar(&s,"SELECT state='reserved' FROM generation_leases")==1);
        goto done;
    }
    CHECK(writer>=3); s.writer=writer;
    CHECK(ae_exec_writer_check(writer,geteuid(),getegid())==0);
    struct ae_peer wrong=peer; strcpy(wrong.unique_name,":1.118"); result=marker;
    CHECK(ae_execution_service_prepared(service,&wrong,&result)==-EACCES && memcmp(&result,&marker,sizeof result)==0);
    CHECK(ae_execution_service_arm(service,context,&result)==-EAGAIN && !s.releases);
    if (strstr(scenario,"prepare-commit-lost")) {
        ae_test_execution_commit_fault(2); result=marker;
        CHECK(ae_execution_service_prepared(service,&peer,&result)==-EIO && memcmp(&result,&marker,sizeof result)==0);
    }
    CHECK(ae_execution_service_prepared(service,&peer,&result)==0);
    if (strstr(scenario,"context-drift")) {
        s.drift=1; result=marker;
        CHECK(ae_execution_service_arm(service,context,&result)==-ESTALE && memcmp(&result,&marker,sizeof result)==0);
        s.drift=0; CHECK(ae_execution_service_arm(service,context,&result)==-ESTALE && !s.releases);
        struct ae_execution_service_status status; CHECK(ae_execution_service_status(service,context,&status)==0 && status.context_error==-ESTALE);
        CHECK(scalar(&s,"SELECT count(*) FROM operation_steps WHERE step_no=3")==0); goto done;
    }
    if (strstr(scenario,"root-restart")) {
        uint8_t runtime[16]; memcpy(runtime,p->system_runtime,16); runtime[15]++;
        CHECK(ae_catalogue_begin_runtime(store,p->boot,runtime)==0); result=marker;
        CHECK(ae_execution_service_arm(service,context,&result)==-ESTALE && memcmp(&result,&marker,sizeof result)==0 && !s.releases);
        foreign=*context; memcpy(foreign.system_runtime,runtime,16);
        CHECK(ae_catalogue_get_dispatch(store,p->lease,&foreign,&snapshot)==0 && strcmp(snapshot.state,"uncertain")==0);
        goto done;
    }
    if (strstr(scenario,"broker-replacement")) {
        result=marker; CHECK(ae_execution_service_arm(service,&foreign,&result)==-ESTALE && memcmp(&result,&marker,sizeof result)==0 && !s.releases); goto done;
    }
    if (strstr(scenario,"early-eof")) {
        CHECK(close(s.writer)==0); s.writer=-1; result=marker;
        /* No event dispatch yet: admission itself must sample the channel. */
        CHECK(ae_execution_service_arm(service,context,&result)==-EAGAIN && memcmp(&result,&marker,sizeof result)==0 && !s.releases);
        observed(service,&s,AE_EXEC_CHANNEL_UNKNOWN); CHECK(sd_event_run(event,0)==0); goto done;
    }
    if (strstr(scenario,"early-data")) {
        CHECK(write(s.writer,"AETHXER",7)==7); result=marker;
        CHECK(ae_execution_service_arm(service,context,&result)==-EAGAIN && memcmp(&result,&marker,sizeof result)==0 && !s.releases);
        observed(service,&s,AE_EXEC_CHANNEL_PENDING);
        CHECK(close(s.writer)==0); s.writer=-1; CHECK(sd_event_run(event,1000000)>0);
        observed(service,&s,AE_EXEC_CHANNEL_MALFORMED); CHECK(sd_event_run(event,0)==0);
        CHECK(scalar(&s,"SELECT count(*) FROM operation_steps WHERE step_no=3")==0);
        CHECK(scalar(&s,"SELECT reason_code='exec_channel_malformed' FROM operations")==1); goto done;
    }
    int arm_lost=strstr(scenario,"arm-commit-lost")!=NULL,release_timeout=strstr(scenario,"release-timeout")!=NULL;
    if (arm_lost) ae_test_execution_commit_fault(2);
    result=marker; r=ae_execution_service_arm(service,context,&result);
    CHECK(r==(arm_lost ? -EIO : release_timeout ? -ETIMEDOUT : 0));
    if (r<0) CHECK(memcmp(&result,&marker,sizeof result)==0);
    CHECK(s.releases==!arm_lost && ae_execution_service_arm(service,context,&result)==-EAGAIN && s.releases==!arm_lost);
    CHECK(scalar(&s,"SELECT state='dispatching' FROM launch_dispatches")==1 && scalar(&s,"SELECT state='running' FROM generation_leases")==1);
    if (arm_lost) { CHECK(close(s.writer)==0); s.writer=-1; }
    if (strstr(scenario,"observer-error")) s.channel_fault=1;
    if (strstr(scenario,"report-commit-lost")) ae_test_execution_commit_fault(2);
    CHECK(sd_event_run(event,1000000)>0);
    int observer_error=strstr(scenario,"observer-error")!=NULL;
    observed(service,&s,observer_error ? AE_EXEC_CHANNEL_PENDING : arm_lost || strstr(scenario,"unknown") ? AE_EXEC_CHANNEL_UNKNOWN : strstr(scenario,"malformed") ? AE_EXEC_CHANNEL_MALFORMED : AE_EXEC_CHANNEL_FAILURE);
    CHECK(sd_event_run(event,0)==0); /* Includes unread malformed overflow data. */
    struct ae_execution_service_status status;
    CHECK(ae_execution_service_status(service,context,&status)==0);
    CHECK(status.report_error==(strstr(scenario,"report-commit-lost") ? -EIO : 0));
    CHECK(status.channel_error==(observer_error ? -EIO : 0));
    int64_t version=scalar(&s,"SELECT catalogue_revision FROM authority_meta");
    CHECK(ae_execution_service_record(service,context)==0 && scalar(&s,"SELECT catalogue_revision FROM authority_meta")==version);
    CHECK(scalar(&s,"SELECT count(*) FROM operation_steps WHERE step_no=4 AND action='verify' AND subject_kind='integration' AND state='applied'")==1);
    CHECK(scalar(&s,observer_error ? "SELECT reason_code='exec_observer_e5' FROM operations" :
        arm_lost || strstr(scenario,"unknown") ? "SELECT reason_code='exec_channel_unknown' FROM operations" :
        strstr(scenario,"malformed") ? "SELECT reason_code='exec_channel_malformed' FROM operations" :
        "SELECT reason_code='exec_failure_p8_e1' FROM operations")==1);
done:
    if (s.writer>=0) CHECK(close(s.writer)==0);
    int64_t revision=scalar(&s,"SELECT catalogue_revision FROM authority_meta");
    CHECK(ae_execution_service_close(&service)==0 && service==NULL && !s.refs);
    CHECK(scalar(&s,"SELECT catalogue_revision FROM authority_meta")==revision && scalar(&s,"SELECT state<>'released' FROM generation_leases")==1);
    if (s.child>0) {
        struct ae_peer held=AE_PEER_INIT; held.pidfd=s.pidfd; CHECK(ae_peer_alive(&held)==0);
        CHECK(close(s.control)==0); int wait_status; CHECK(waitpid(s.child,&wait_status,0)==s.child && WIFEXITED(wait_status) && WEXITSTATUS(wait_status)==0);
        CHECK(close(s.pidfd)==0);
    }
    CHECK(ae_execution_service_close(&service)==0); sd_bus_unref(s.bus); sd_event_unref(event); active=NULL;
    puts("Execution coordinator: real owner commits/ProcessFD/event pipe, controlled unit/context adapters, no repeated effects or implied lease release passed.");
}
