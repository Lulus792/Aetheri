#include "authority/execution.h"
#include "authority/execution_wire.h"
#include "execution-wire.h"
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <sqlite3.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/pidfd.h>
#include <sys/epoll.h>
#include <sys/wait.h>
#include <time.h>
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
    struct ae_execution_endpoint **endpoint;
    pid_t host;
    int wire_mode,send_stage,send_fault,registration_sends,error_sends,notice_fd,notice_count;
    int time_jump,time_skip,notice_dead;
    char unique[256],notices[8];
};
static struct test_state *active;
int __real_ae_peer_capture(sd_bus_message *,struct ae_peer *);
int __wrap_ae_peer_capture(sd_bus_message *m,struct ae_peer *peer)
{
    int r=__real_ae_peer_capture(m,peer);
    if (r==0 && active && active->wire_mode && getpid()==active->host) {
        CHECK(peer->connection_pid==active->child && peer->connection_uid==0);
        /* Controlled actor label, as with the existing synthetic PID1 context.
         * Transport credentials/process FD remain real; not MAC acceptance. */
        strcpy(peer->connection_label,strstr(active->scenario,"wrong-peer") ?
            "system_u:system_r:aetheri_setup_t:s0" : "system_u:system_r:aetheri_starter_t:s0");
        if (strstr(active->scenario,"prepare-commit-lost") && strcmp(sd_bus_message_get_member(m),AE_EXECUTION_PREPARED_METHOD)==0)
            ae_test_execution_commit_fault(2);
    }
    return r;
}
int __real_clock_gettime(clockid_t,struct timespec *);
int __wrap_clock_gettime(clockid_t clock,struct timespec *t)
{
    int r=__real_clock_gettime(clock,t);
    if (r==0 && clock==CLOCK_MONOTONIC && active && active->wire_mode && active->time_jump && getpid()==active->host) {
        if (active->time_skip) --active->time_skip;
        else { t->tv_sec+=5; active->time_jump=0; }
    }
    return r;
}
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
int __real_sd_bus_send(sd_bus *,sd_bus_message *,uint64_t *);
int __wrap_sd_bus_send(sd_bus *bus,sd_bus_message *m,uint64_t *cookie)
{
    struct test_state *s=active; uint8_t type=0;
    if (s && s->wire_mode && getpid()==s->host) {
        CHECK(sd_bus_message_get_type(m,&type)>=0);
        if (type==SD_BUS_MESSAGE_METHOD_ERROR) ++s->error_sends;
        if (type==SD_BUS_MESSAGE_METHOD_RETURN) {
            int stage=sd_bus_message_has_signature(m,AE_EXECUTION_REGISTER_OUTPUT) ? 1 :
                sd_bus_message_has_signature(m,AE_EXECUTION_PREPARED_OUTPUT) ? 2 : 0;
            CHECK(stage!=0);
            struct ae_dispatch_snapshot d; CHECK(ae_catalogue_get_dispatch(s->store,s->profile->lease,s->context,&d)==0);
            CHECK(scalar(s,stage==1 ? "SELECT count(*) FROM operation_steps WHERE step_no=2 AND state='executing'" :
                "SELECT count(*) FROM operation_steps WHERE step_no=3 AND state='executing'")==1);
            CHECK(strcmp(d.state,stage==1 ? "reserved" : "dispatching")==0 && d.invocation_present==(stage==2));
            if (stage==1) ++s->registration_sends; else ++s->releases;
            if (s->send_fault && stage==s->send_stage) {
                int fault=s->send_fault; s->send_fault=0;
                if (fault==2) CHECK(__real_sd_bus_send(bus,m,cookie)>=0);
                return -EIO;
            }
        }
    }
    return __real_sd_bus_send(bus,m,cookie);
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

static void wire_notice(int fd,char value)
{ ssize_t n; do { n=write(fd,&value,1); } while (n<0 && errno==EINTR); CHECK(n==1); }
static int wire_notice_event(sd_event_source *source,int fd,uint32_t events,void *userdata)
{
    (void)events; struct test_state *s=userdata; char values[8]; ssize_t n=read(fd,values,sizeof values);
    if (n<0) { CHECK(errno==EAGAIN || errno==EINTR); return 0; }
    if (n==0) { s->notice_dead=1; CHECK(sd_event_source_set_enabled(source,SD_EVENT_OFF)>=0); return 0; }
    CHECK(s->notice_count+(int)n<=(int)sizeof s->notices);
    memcpy(s->notices+s->notice_count,values,(size_t)n); s->notice_count+=(int)n; return 0;
}
static int wire_child_call(sd_bus *bus,const struct test_state *s,int registration,int *writer)
{
    sd_bus_message *call=NULL,*reply=NULL; uint8_t lease[16],hash[32]; memcpy(lease,s->profile->lease,16);
    CHECK(ae_execution_digest(s->profile,hash)==0);
    if (registration && strstr(s->scenario,"wrong-tuple")) lease[15]^=1;
    CHECK(sd_bus_message_new_method_call(bus,&call,s->unique,AE_EXECUTION_PATH,AE_EXECUTION_INTERFACE,
        registration ? AE_EXECUTION_REGISTER_METHOD : AE_EXECUTION_PREPARED_METHOD)>=0);
    CHECK(sd_bus_message_append_array(call,'y',lease,16)>=0 && sd_bus_message_append_array(call,'y',hash,32)>=0);
    sd_bus_error error=SD_BUS_ERROR_NULL;
    int r=sd_bus_call(bus,call,registration ? 2000000 : 5000000,&error,&reply);
    if (r<0) fprintf(stderr,"wire child %s %s: %d %s %s\n",s->scenario,registration ? "register" : "prepared",r,error.name ? error.name : "-",error.message ? error.message : "-");
    if (r>=0) {
        struct ae_peer peer=AE_PEER_INIT; CHECK(ae_peer_capture_reply(reply,call,&peer)==0 && peer.connection_pid==s->host && peer.connection_uid==0);
        ae_peer_clear(&peer);
        CHECK(sd_bus_message_has_signature(reply,registration ? AE_EXECUTION_REGISTER_OUTPUT : AE_EXECUTION_PREPARED_OUTPUT));
        const uint8_t *fields[]={s->profile->system_epoch,s->profile->system_runtime,s->profile->boot,s->profile->lease,s->profile->root_operation,hash};
        for (size_t n=0;n<6;++n) {
            const void *p=NULL; size_t size=0; CHECK(sd_bus_message_read_array(reply,'y',&p,&size)>0 && size==(n==5 ? 32U : 16U) && memcmp(p,fields[n],size)==0);
        }
        uint64_t revision=0; CHECK(sd_bus_message_read(reply,"t",&revision)>0 && revision==(registration ? 1U : 2U));
        if (registration) {
            int borrowed=-1; CHECK(sd_bus_message_read(reply,"h",&borrowed)>0 && ae_exec_writer_check(borrowed,0,0)==0);
            *writer=fcntl(borrowed,F_DUPFD_CLOEXEC,3); CHECK(*writer>=3);
        }
        CHECK(sd_bus_message_at_end(reply,1)==1); r=0;
    }
    sd_bus_error_free(&error); sd_bus_message_unref(call); sd_bus_message_unref(reply); return r;
}
static int wire_request(void *owner,const struct ae_execution_profile *p,const struct ae_dispatch_snapshot *d)
{
    struct test_state *s=owner; ++s->requests; CHECK(s->requests==1 && p!=s->profile && strcmp(d->state,"reserved")==0);
    CHECK(ae_execution_endpoint_close(s->endpoint)==-EBUSY && *s->endpoint);
    struct ae_dispatch_snapshot observed; CHECK(ae_catalogue_get_dispatch(s->store,p->lease,s->context,&observed)==0);
    CHECK(scalar(s,"SELECT count(*) FROM operation_steps WHERE step_no=1 AND subject_kind='generation' AND state='executing'")==1);
    strcpy(s->unit,d->unit); const char *unique=NULL; CHECK(sd_bus_get_unique_name(s->bus,&unique)>=0 && strlen(unique)<sizeof s->unique); strcpy(s->unique,unique);
    int command[2],notice[2]; CHECK(pipe2(command,O_CLOEXEC)==0 && pipe2(notice,O_CLOEXEC|O_NONBLOCK)==0);
    int inherited=sd_bus_get_fd(s->bus); CHECK(inherited>=0);
    s->child=fork(); CHECK(s->child>=0);
    if (s->child==0) {
        CHECK(close(command[1])==0 && close(notice[0])==0 && close(inherited)==0);
        struct ae_execution_endpoint *held=*s->endpoint; CHECK(ae_execution_endpoint_close(&held)==-ECHILD && held==*s->endpoint);
        sd_bus *bus=NULL; CHECK(sd_bus_open_user(&bus)>=0);
        int writer=-1,r=wire_child_call(bus,s,1,&writer);
        if (r<0) wire_notice(notice[1],'F');
        else {
            wire_notice(notice[1],'B'); r=wire_child_call(bus,s,0,NULL);
            CHECK(close(writer)==0); wire_notice(notice[1],r==0 ? 'P' : 'E');
        }
        sd_bus_flush_close_unref(bus);
        char value; CHECK(read(command[0],&value,1)==0); CHECK(close(command[0])==0 && close(notice[1])==0); _exit(0);
    }
    CHECK(close(command[0])==0 && close(notice[1])==0); s->control=command[1]; s->notice_fd=notice[0];
    s->pidfd=pidfd_open(s->child,0); CHECK(s->pidfd>=3); return 0;
}
static void wire_wait(sd_event *event,struct test_state *s,int count)
{
    struct timespec started,t; CHECK(__real_clock_gettime(CLOCK_MONOTONIC,&started)==0);
    while (s->notice_count<count) {
        CHECK(!s->notice_dead && __real_clock_gettime(CLOCK_MONOTONIC,&t)==0 && t.tv_sec-started.tv_sec<9);
        CHECK(sd_event_run(event,6000000)>0);
    }
}
void ae_test_execution_wire(struct ae_catalogue *store,const struct ae_execution_profile *p,struct ae_launch_context *context,const char *directory,const char *scenario)
{
    CHECK(geteuid()==0); struct ae_execution_endpoint *endpoint=NULL;
    struct test_state s={.store=store,.profile=p,.context=context,.directory=directory,.scenario=scenario,.endpoint=&endpoint,.host=getpid(),.wire_mode=1,
        .control=-1,.pidfd=-1,.notice_fd=-1}; active=&s;
    sd_event *event=NULL; CHECK(sd_event_new(&event)>=0 && sd_bus_open_user(&s.bus)>=0 && sd_bus_attach_event(s.bus,event,0)>=0);
    CHECK(ae_execution_endpoint_new(store,s.bus,event,p,context,wire_request,&s,&endpoint)==0);
    struct ae_execution_result result,marker; memset(&marker,0xa5,sizeof marker); result=marker;
    CHECK(ae_execution_endpoint_arm(endpoint,context,&result)==-EAGAIN && memcmp(&result,&marker,sizeof result)==0);
    CHECK(ae_execution_endpoint_request(endpoint,context,&result)==0 && s.requests==1);
    CHECK(ae_execution_endpoint_request(endpoint,context,&result)==-EALREADY && s.requests==1);
    sd_event_source *notice=NULL; CHECK(sd_event_add_io(event,&notice,s.notice_fd,EPOLLIN|EPOLLHUP,wire_notice_event,&s)>=0);
    if (strstr(scenario,"bind-commit-lost")) ae_test_execution_commit_fault(2);
    if (strstr(scenario,"register-send")) { s.send_stage=1; s.send_fault=strstr(scenario,"lost") ? 2 : 1; }
    wire_wait(event,&s,1);
    if (s.notices[0]!='B') {
        struct ae_execution_endpoint_status diagnostic; CHECK(ae_execution_endpoint_status(endpoint,context,&diagnostic)==0);
        fprintf(stderr,"wire %s first=%c requested=%d bound=%d bind_error=%d reply_attempt=%d reply_error=%d refs=%d\n",scenario,s.notices[0],diagnostic.execution.requested,
            diagnostic.execution.bound,diagnostic.execution.bind_error,diagnostic.registration_reply_attempted,diagnostic.registration_reply_error,s.refs);
    }
    if (strstr(scenario,"wrong-peer") || strstr(scenario,"wrong-tuple") || strstr(scenario,"bind-commit-lost") || strstr(scenario,"register-send-before")) {
        CHECK(s.notices[0]=='F' && !s.releases);
        CHECK(scalar(&s,"SELECT count(*) FROM operation_steps WHERE step_no=3")==0);
        goto done_wire;
    }
    CHECK(s.notices[0]=='B');
    if (strstr(scenario,"prepare-commit-lost") || strstr(scenario,"register-send-lost")) {
        wire_wait(event,&s,2); CHECK(s.notices[1]=='E' && !s.releases);
        CHECK(scalar(&s,"SELECT count(*) FROM operation_steps WHERE step_no=3")==0); goto done_wire;
    }
    struct ae_execution_endpoint_status status;
    for (int n=0;;++n) {
        CHECK(n<32 && ae_execution_endpoint_status(endpoint,context,&status)==0);
        if (status.prepared_pending) break;
        CHECK(s.notice_count==1 && sd_event_run(event,6000000)>0);
    }
    CHECK(status.execution.prepared && !status.release_reply_attempted && !s.releases);
    CHECK(scalar(&s,"SELECT count(*) FROM operation_steps WHERE step_no IN (1,2) AND state='applied'")==2 && scalar(&s,"SELECT count(*) FROM operation_steps WHERE step_no=3")==0);
    int armed=0,success=1;
    if (strstr(scenario,"deadline-event")) {
        wire_wait(event,&s,2); CHECK(s.notices[1]=='E');
        CHECK(ae_execution_endpoint_arm(endpoint,context,&result)==-ETIMEDOUT); success=0;
    } else {
        if (strstr(scenario,"deadline-direct") || strstr(scenario,"release-deadline")) { s.time_jump=1; s.time_skip=strstr(scenario,"release-deadline") ? 1 : 0; }
        if (strstr(scenario,"release-send")) { s.send_stage=2; s.send_fault=strstr(scenario,"lost") ? 2 : 1; }
        if (strstr(scenario,"arm-commit-lost")) ae_test_execution_commit_fault(2);
        if (strstr(scenario,"context-drift")) s.drift=1;
        struct ae_launch_context broker=*context;
        if (strstr(scenario,"broker-replacement")) broker.broker_runtime[15]++;
        if (strstr(scenario,"root-restart")) { uint8_t id[16]; memcpy(id,p->system_runtime,16); id[15]++; CHECK(ae_catalogue_begin_runtime(store,p->boot,id)==0); }
        result=marker; int r=ae_execution_endpoint_arm(endpoint,&broker,&result);
        int rejected=strstr(scenario,"deadline") || strstr(scenario,"release-send") || strstr(scenario,"arm-commit-lost") || strstr(scenario,"context-drift") || strstr(scenario,"broker-replacement") || strstr(scenario,"root-restart");
        CHECK(rejected ? r<0 && memcmp(&result,&marker,sizeof result)==0 : r==0);
        armed=scalar(&s,"SELECT count(*) FROM operation_steps WHERE step_no=3")==1;
        if (strstr(scenario,"arm-commit-lost") || strstr(scenario,"context-drift") || strstr(scenario,"broker-replacement") || strstr(scenario,"root-restart")) {
            s.drift=0; s.time_jump=1; s.time_skip=0;
            CHECK(ae_execution_endpoint_arm(endpoint,context,&result)==-ETIMEDOUT);
        } else CHECK(ae_execution_endpoint_arm(endpoint,context,&result)<0);
        success=!rejected || strstr(scenario,"release-send-lost")!=NULL;
        wire_wait(event,&s,2);
    }
    CHECK(s.notices[1]==(success ? 'P' : 'E'));
    CHECK(scalar(&s,armed ? "SELECT state='running' FROM generation_leases" : "SELECT state='reserved' FROM generation_leases")==1);
    CHECK(s.releases==(success || strstr(scenario,"release-send-before") ? 1 : 0));
    if (!strstr(scenario,"root-restart")) {
        CHECK(ae_execution_endpoint_status(endpoint,context,&status)==0 && !status.prepared_pending);
        int expired=strstr(scenario,"deadline") || strstr(scenario,"arm-commit-lost") || strstr(scenario,"context-drift") || strstr(scenario,"broker-replacement");
        CHECK(status.prepared_expired==expired);
    }
done_wire:
    CHECK(scalar(&s,"SELECT state<>'released' FROM generation_leases")==1 && scalar(&s,"SELECT state<>'completed' FROM operations")==1);
    int64_t revision=scalar(&s,"SELECT catalogue_revision FROM authority_meta");
    CHECK(ae_execution_endpoint_close(&endpoint)==0 && endpoint==NULL && s.refs==0 && ae_execution_endpoint_close(&endpoint)==0);
    CHECK(scalar(&s,"SELECT catalogue_revision FROM authority_meta")==revision);
    struct ae_peer held=AE_PEER_INIT; held.pidfd=s.pidfd; CHECK(ae_peer_alive(&held)==0);
    sd_event_source_unref(notice); CHECK(close(s.notice_fd)==0 && close(s.control)==0);
    int wait_status; CHECK(waitpid(s.child,&wait_status,0)==s.child && WIFEXITED(wait_status) && WEXITSTATUS(wait_status)==0 && close(s.pidfd)==0);
    CHECK(sd_bus_detach_event(s.bus)>=0); sd_bus_unref(s.bus); sd_event_unref(event); active=NULL;
    puts("Execution endpoint: real owner journal and correlated sd-bus FD/deferred replies, controlled actor/context adapters, no early/replayed release or implied drain passed.");
}
