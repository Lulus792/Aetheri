#include "authority/setup.h"
#include "catalogue/effect.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <sqlite3.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"setup wire line %d: %s\n",__LINE__,#x); exit(1); } } while (0)
#define PATH "/org/aetheri/TestSetup"
#define IFACE "org.aetheri.TestSetup1"
void ae_test_setup_commit_fault(int);
struct state {
    struct ae_setup_service service;
    struct ae_setup_profile profile;
    struct ae_launch_context broker;
    struct ae_process_context helper;
    uint8_t hash[32];
    const char *mode,*directory;
    sd_bus_message *request;
    pid_t host,child;
    int target,stage,refs,sends[5],handled[5],errors[5],builds[5],fallbacks[5],captures;
};
static struct state *active;
static int descriptors(void)
{ DIR *d=opendir("/proc/self/fd"); CHECK(d); int n=0; while (readdir(d)) ++n; CHECK(closedir(d)==0); return n; }
static int parent(void) { return active && getpid()==active->host; }
static void fence(void)
{
    struct ae_execution_fence_status status;
    CHECK(ae_catalogue_fence_execution(active->service.catalogue,&status)==0 && status.fenced);
}
static void guard(unsigned int expected)
{
    struct state *s=active; struct ae_execution_fence_status status;
    CHECK(s->service.busy && ae_catalogue_execution_fence_status(s->service.catalogue,&status)==0 && status.active==expected);
    CHECK(ae_setup_service_clear(&s->service)==-EBUSY);
    CHECK(ae_setup_observe_exit(&s->service,&s->profile,s->hash)==-EBUSY);
    struct ae_catalogue *same=s->service.catalogue;
    CHECK(ae_catalogue_close(&same)==-EBUSY && same==s->service.catalogue);
    uint8_t runtime[16]; memcpy(runtime,s->profile.system_runtime,16); runtime[15]^=1;
    CHECK(ae_catalogue_begin_runtime(same,s->profile.boot,runtime)==-EBUSY);
    sd_bus_error e=SD_BUS_ERROR_NULL;
    CHECK(ae_setup_method_prepare(s->request,&s->service,&e)<0 && sd_bus_error_is_set(&e)); sd_bus_error_free(&e);
}
static int64_t scalar(const char *sql)
{
    char path[4096]; CHECK(snprintf(path,sizeof path,"%s/catalogue.sqlite3",active->directory)>0);
    sqlite3 *db=NULL; sqlite3_stmt *q=NULL; CHECK(sqlite3_open_v2(path,&db,SQLITE_OPEN_READONLY,NULL)==SQLITE_OK);
    CHECK(sqlite3_prepare_v2(db,sql,-1,&q,NULL)==SQLITE_OK && sqlite3_step(q)==SQLITE_ROW);
    int64_t result=sqlite3_column_int64(q,0); CHECK(sqlite3_finalize(q)==SQLITE_OK && sqlite3_close(db)==SQLITE_OK); return result;
}
int __real_ae_setup_load(struct ae_setup_profile *,uint8_t *);
int __wrap_ae_setup_load(struct ae_setup_profile *p,uint8_t *hash)
{ if (!parent()) return __real_ae_setup_load(p,hash); guard(1); *p=active->profile; memcpy(hash,active->hash,32); return 0; }
int __real_ae_peer_capture(sd_bus_message *,struct ae_peer *);
int __wrap_ae_peer_capture(sd_bus_message *m,struct ae_peer *p)
{
    int r=__real_ae_peer_capture(m,p);
    if (r==0 && parent()) {
        guard(1); CHECK(p->connection_uid==0 && p->connection_pid==active->child && ae_peer_alive(p)==0);
        /* Only the actor label/PID1 context are controlled. Bus credentials and
         * the connection's actual ProcessFD remain real, not MAC acceptance. */
        strcpy(p->connection_label,"system_u:system_r:aetheri_setup_t:s0"); ++active->captures;
    }
    return r;
}
int __real_ae_process_observe_registration(const struct ae_peer *,sd_bus *,struct ae_process_context *);
int __wrap_ae_process_observe_registration(const struct ae_peer *p,sd_bus *bus,struct ae_process_context *out)
{
    if (!parent()) return __real_ae_process_observe_registration(p,bus,out);
    guard(1); CHECK(p->connection_pid==active->child && ae_peer_alive(p)==0); *out=active->helper; return 0;
}
#define JOURNAL(name,args,params,stage_number) \
int __real_##name args; \
int __wrap_##name args { \
    if (parent() && stage_number>1 && active->stage==stage_number && strcmp(active->mode,stage_number==2 ? "register-lost" : "prepared-lost")==0) ae_test_setup_commit_fault(2); \
    int result=__real_##name params; \
    if (parent() && result==0 && active->target==stage_number && strstr(active->mode,"-commit")) { guard(1); fence(); } \
    return result; }
JOURNAL(ae_catalogue_begin_setup,(struct ae_catalogue *c,const struct ae_setup_profile *p,const uint8_t *h,const struct ae_launch_context *b,struct ae_dispatch_snapshot *o),(c,p,h,b,o),1)
JOURNAL(ae_catalogue_register_setup,(struct ae_catalogue *c,const struct ae_setup_profile *p,const uint8_t *h,const uint8_t *i,const uint8_t *v,struct ae_dispatch_snapshot *o),(c,p,h,i,v,o),2)
JOURNAL(ae_catalogue_prepared_setup,(struct ae_catalogue *c,const struct ae_setup_profile *p,const uint8_t *h,const uint8_t *i,const uint8_t *v,struct ae_dispatch_snapshot *o),(c,p,h,i,v,o),3)
int __real_sd_bus_message_new_method_return(sd_bus_message *,sd_bus_message **);
int __wrap_sd_bus_message_new_method_return(sd_bus_message *m,sd_bus_message **out)
{
    int r=__real_sd_bus_message_new_method_return(m,out);
    if (parent()) {
        guard(active->stage==4 ? 1U : 2U); ++active->builds[active->stage];
        if (active->stage==active->target && strstr(active->mode,"-build")) fence();
    }
    return r;
}
int __real_sd_bus_send(sd_bus *,sd_bus_message *,uint64_t *);
int __wrap_sd_bus_send(sd_bus *bus,sd_bus_message *m,uint64_t *cookie)
{
    if (!parent()) return __real_sd_bus_send(bus,m,cookie);
    guard(active->stage==4 ? 1U : 2U); ++active->sends[active->stage];
    CHECK(active->sends[active->stage]==1);
    if (active->stage==active->target && strstr(active->mode,"-send-before")) { fence(); return -EIO; }
    int r=__real_sd_bus_send(bus,m,cookie);
    if (active->stage==active->target && strstr(active->mode,"-send")) {
        fence(); if (strstr(active->mode,"-send-lost")) return -EIO;
    }
    return r;
}
static sd_bus_message *snapshot(void)
{
    sd_bus_message *m=NULL; CHECK(sd_bus_message_new_signal(active->service.bus,&m,PATH,IFACE,"Snapshot")>=0); return m;
}
static void seal(sd_bus_message *m) { CHECK(sd_bus_message_seal(m,1,0)>=0 && sd_bus_message_rewind(m,1)>=0); }
int __real_sd_bus_call_method(sd_bus *,const char *,const char *,const char *,const char *,sd_bus_error *,sd_bus_message **,const char *,...);
int __wrap_sd_bus_call_method(sd_bus *bus,const char *destination,const char *path,const char *interface,const char *method,sd_bus_error *error,sd_bus_message **out,const char *signature,...)
{
    (void)bus; (void)path; (void)interface; (void)error;
    CHECK(parent() && strcmp(destination,"org.freedesktop.systemd1")==0 && strcmp(signature,"s")==0);
    va_list args; va_start(args,signature); const char *unit=va_arg(args,const char *); CHECK(strcmp(unit,active->helper.unit)==0); va_end(args);
    if (strcmp(method,"RefUnit")==0) { guard(1); CHECK(!active->refs); ++active->refs; return 1; }
    if (strcmp(method,"UnrefUnit")==0) { CHECK(active->refs==1); --active->refs; return 1; }
    CHECK(strcmp(method,"GetUnit")==0); guard(1); *out=snapshot(); CHECK(sd_bus_message_append(*out,"o","/selected")>=0); seal(*out); return 1;
}
int __wrap_sd_bus_get_property_string(sd_bus *b,const char *d,const char *p,const char *i,const char *key,sd_bus_error *e,char **out)
{
    (void)b;(void)d;(void)p;(void)i;(void)e; CHECK(parent()); guard(1);
    CHECK(strcmp(key,"ActiveState")==0 || strcmp(key,"ControlGroup")==0);
    *out=strdup(strcmp(key,"ActiveState")==0 ? "failed" : ""); CHECK(*out); return 1;
}
int __wrap_sd_bus_get_property_trivial(sd_bus *b,const char *d,const char *p,const char *i,const char *key,sd_bus_error *e,char type,void *out)
{ (void)b;(void)d;(void)p;(void)i;(void)e; guard(1); CHECK(type=='u' && strcmp(key,"MainPID")==0); *(uint32_t *)out=0; return 1; }
int __wrap_sd_bus_get_property(sd_bus *b,const char *d,const char *p,const char *i,const char *key,sd_bus_error *e,sd_bus_message **out,const char *signature)
{
    (void)b;(void)d;(void)p;(void)i;(void)e;(void)signature; guard(1); *out=snapshot();
    if (strcmp(key,"Job")==0) CHECK(sd_bus_message_append(*out,"(uo)",0U,"/")>=0);
    else { CHECK(strcmp(key,"InvocationID")==0); CHECK(sd_bus_message_append_array(*out,'y',active->helper.invocation,16)>=0); }
    seal(*out); return 1;
}
static int broker(void *owner,sd_bus_message *m,struct ae_launch_context *out)
{ (void)m; CHECK(owner==active); guard(1); *out=active->broker; return 0; }
static int handler(sd_bus_message *m,void *owner,sd_bus_error *e)
{
    CHECK(owner==active); struct state *s=active; const char *method=sd_bus_message_get_member(m);
    s->stage=strcmp(method,"PrepareSetup")==0 ? 1 : strcmp(method,"RegisterSetup")==0 ? 2 : strcmp(method,"ReportPrepared")==0 ? 3 : 4;
    s->request=m;
    if (s->stage==s->target && strstr(s->mode,"-before") && !strstr(s->mode,"-send")) fence();
    uint8_t original=s->service.runtime[15]; if (strcmp(s->mode,"owner-drift")==0) s->service.runtime[15]^=1;
    int r=s->stage==1 ? ae_setup_method_prepare(m,&s->service,e) : s->stage==2 ? ae_setup_method_register(m,&s->service,e) :
        s->stage==3 ? ae_setup_method_prepared(m,&s->service,e) : ae_setup_method_withdraw(m,&s->service,e);
    s->service.runtime[15]=original;
    if (s->sends[s->stage]) {
        if (r==1 && !sd_bus_error_is_set(e)) ++s->handled[s->stage]; else ++s->fallbacks[s->stage];
    }
    else { CHECK(r<0 && sd_bus_error_is_set(e)); ++s->errors[s->stage]; }
    struct ae_execution_fence_status status; CHECK(!s->service.busy && ae_catalogue_execution_fence_status(s->service.catalogue,&status)==0 && !status.active);
    s->request=NULL; return r;
}
static const sd_bus_vtable table[]={SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("PrepareSetup","aytay","ayt",handler,SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("RegisterSetup","ayay","ayt",handler,SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("ReportPrepared","ayay","ayt",handler,SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("WithdrawSetup","ayay","ayt",handler,SD_BUS_VTABLE_UNPRIVILEGED),SD_BUS_VTABLE_END};
static int call(sd_bus *bus,const char *destination,int phase)
{
    const char *methods[]={"","PrepareSetup","RegisterSetup","ReportPrepared","WithdrawSetup"};
    sd_bus_message *m=NULL,*reply=NULL; CHECK(sd_bus_message_new_method_call(bus,&m,destination,PATH,IFACE,methods[phase])>=0);
    CHECK(sd_bus_message_append_array(m,'y',active->profile.lease,16)>=0);
    if (phase==1) CHECK(sd_bus_message_append(m,"t",(uint64_t)active->profile.data_gate_revision)>=0);
    CHECK(sd_bus_message_append_array(m,'y',phase==4 ? active->profile.user_operation : active->hash,phase==4 ? 16U : 32U)>=0);
    sd_bus_error e=SD_BUS_ERROR_NULL; int r=sd_bus_call(bus,m,2000000,&e,&reply);
    if (r>=0) {
        struct ae_peer peer=AE_PEER_INIT; CHECK(ae_peer_capture_reply(reply,m,&peer)==0 && peer.connection_pid==active->host); ae_peer_clear(&peer);
        const void *bytes=NULL; size_t size=0; uint64_t revision=0;
        CHECK(sd_bus_message_read_array(reply,'y',&bytes,&size)>0 && size==16 && memcmp(bytes,active->profile.root_operation,16)==0);
        CHECK(sd_bus_message_read(reply,"t",&revision)>0 && revision==(phase==4 ? 2U : 1U) && sd_bus_message_at_end(reply,1)==1); r=0;
    } else {
        const char *expected=strstr(active->mode,"-send-before") && phase==active->target ? SD_BUS_ERROR_TIMEOUT : "org.aetheri.Error1.NeedsRecovery";
        if (strstr(active->mode,"-send-before") && phase==active->target) CHECK(r==-ETIMEDOUT);
        if (!sd_bus_error_has_name(&e,expected)) fprintf(stderr,"setup client %s phase %d: %d %s expected %s\n",active->mode,phase,r,e.name ? e.name : "-",expected);
        CHECK(sd_bus_error_has_name(&e,expected));
    }
    sd_bus_error_free(&e); sd_bus_message_unref(m); sd_bus_message_unref(reply); return r;
}
static void client(int cleanup,int failure)
{
    const char *unique=NULL; CHECK(sd_bus_get_unique_name(active->service.bus,&unique)>=0);
    int inherited=sd_bus_get_fd(active->service.bus); CHECK(inherited>=0);
    active->child=fork(); CHECK(active->child>=0);
    if (!active->child) {
        CHECK(close(inherited)==0); sd_bus *bus=NULL; CHECK(sd_bus_open_user(&bus)>=0);
        int failed=0;
        for (int phase=cleanup ? 4 : 1;phase<= (cleanup ? 4 : 3);++phase) if (call(bus,unique,phase)<0) { failed=phase; break; }
        CHECK(failed==failure); sd_bus_flush_close_unref(bus); _exit(0);
    }
    struct timespec start,now; CHECK(clock_gettime(CLOCK_MONOTONIC,&start)==0); int status=0;
    for (;;) {
        pid_t r=waitpid(active->child,&status,WNOHANG); CHECK(r==0 || r==active->child); if (r) break;
        CHECK(clock_gettime(CLOCK_MONOTONIC,&now)==0 && now.tv_sec-start.tv_sec<12);
        int processed=sd_bus_process(active->service.bus,NULL); CHECK(processed>=0); if (!processed) CHECK(sd_bus_wait(active->service.bus,50000)>=0);
    }
    CHECK(WIFEXITED(status) && !WEXITSTATUS(status));
}
void ae_test_setup_wire(struct ae_catalogue *store,const struct ae_setup_profile *p,const uint8_t *hash,struct ae_launch_context *context,const char *directory,const char *mode)
{
    CHECK(geteuid()==0); int baseline=descriptors();
    struct state s={.profile=*p,.broker=*context,.mode=mode,.directory=directory,.host=getpid()}; active=&s;
    s.service=(struct ae_setup_service){.catalogue=store,.owner=&s,.broker=broker,.peer=AE_PEER_INIT};
    memcpy(s.service.epoch,p->system_epoch,16); memcpy(s.service.runtime,p->system_runtime,16); memcpy(s.service.boot,p->boot,16); memcpy(s.hash,hash,32);
    struct ae_dispatch_snapshot dispatch; CHECK(ae_catalogue_get_dispatch(store,p->lease,context,&dispatch)==0);
    strcpy(s.helper.unit,dispatch.unit); strcpy(s.helper.label,"system_u:system_r:aetheri_setup_t:s0"); strcpy(s.helper.cgroup,"/system.slice/test-setup.service");
    memcpy(s.helper.boot,p->boot,16); memcpy(s.helper.invocation,p->launch,16); s.helper.invocation[15]=66; s.helper.cgroup_id=99;
    s.target=strncmp(mode,"prepare-",8)==0 ? 1 : strncmp(mode,"register-",9)==0 ? 2 : strncmp(mode,"prepared-",9)==0 ? 3 : 0;
    int failure=0;
    if (s.target) failure=strstr(mode,"-send") ? (s.target==3 ? 0 : s.target+1) : s.target;
    if (strstr(mode,"-send-before")) failure=s.target;
    if (strcmp(mode,"owner-drift")==0) failure=1;
    CHECK(sd_bus_open_user(&s.service.bus)>=0); sd_bus_slot *slot=NULL;
    CHECK(sd_bus_add_object_vtable(s.service.bus,&slot,PATH,IFACE,table,&s)>=0);
    client(0,failure);
    CHECK(scalar("SELECT state='reserved' FROM generation_leases")==1);
    int has_setup=scalar("SELECT count(*) FROM operation_steps WHERE step_no=1")==1;
    CHECK(has_setup==!(strcmp(mode,"prepare-before")==0 || strcmp(mode,"owner-drift")==0));
    for (int phase=1;phase<=3;++phase) {
        CHECK(!s.fallbacks[phase]);
        if (!failure || phase<failure) CHECK(s.handled[phase]==1 && s.sends[phase]==1 && !s.errors[phase]);
        else if (phase==failure) {
            if (strstr(mode,"-send-before")) CHECK(!s.errors[phase] && s.sends[phase]==1 && s.handled[phase]==1);
            else {
                CHECK(s.errors[phase]==1 && !s.sends[phase] && !s.handled[phase]);
                CHECK(s.builds[phase]==(phase==s.target && strstr(mode,"-build") ? 1 : 0));
            }
        }
        else CHECK(!s.errors[phase] && !s.sends[phase] && !s.handled[phase]);
    }
    int prepared_observed=s.errors[3] || s.handled[3];
    CHECK(scalar("SELECT count(*) FROM operation_steps WHERE step_no IN (1,2) AND state='applied'")==(prepared_observed ? 2 : 0));
    fence();
    if (s.service.peer.pidfd>=0) {
        CHECK(s.refs==1 && ae_peer_alive(&s.service.peer)==-ESRCH && s.service.peer.connection_pid==s.child);
        CHECK(scalar("SELECT count(*) FROM operation_steps WHERE step_no=2")==1 && ae_process_context_equal(&s.service.context,&s.helper));
        int pin=s.service.peer.pidfd; pid_t original_child=s.child;
        client(1,0); CHECK(s.refs==0 && s.service.peer.pidfd==pin && fcntl(pin,F_GETFD)>=0);
        CHECK(s.service.peer.connection_pid==original_child && s.handled[4]==1 && s.sends[4]==1 && !s.errors[4]);
        CHECK(scalar("SELECT state='released' FROM generation_leases")==1);
    } else {
        CHECK(!s.refs);
        if (!has_setup) CHECK(ae_catalogue_cancel_reservation(store,p->lease,context,p->broker_epoch,p->user_operation,1,123500,&dispatch)==0);
        else CHECK(scalar("SELECT state='running' FROM operations")==1); /* No fabricated peer/drain proof. */
    }
    CHECK(ae_setup_service_clear(&s.service)==0 && !s.refs && s.service.peer.pidfd<0);
    struct ae_execution_fence_status status; CHECK(ae_catalogue_execution_fence_status(store,&status)==0 && status.fenced && !status.active);
    sd_bus_slot_unref(slot); sd_bus_flush_close_unref(s.service.bus); active=NULL;
    CHECK(descriptors()==baseline);
    printf("Setup fence: %s; real private-bus replies/process pins/native journals, controlled actor/PID1; no target execution or general drain.\n",mode);
}
