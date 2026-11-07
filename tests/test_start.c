#include "authority/start.h"
#include <dirent.h>
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define MANAGER "org.freedesktop.systemd1"
#define PATH "/org/freedesktop/systemd1"
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"start %s line %d: %s errno=%d\n",active ? active->mode : "none",__LINE__,#x,errno); exit(1); } } while (0)
struct state {
    pid_t parent,child;
    const char *mode,*directory;
    const struct ae_execution_profile *profile;
    struct ae_launch_context *context;
    struct ae_catalogue *store;
    struct ae_supervisor_start *start;
    sd_bus *bus;
    char unique[256],unit[80];
    unsigned int admissions,calls;
    int control;
};
static struct state *active;
int __real_sd_id128_get_boot(sd_id128_t *);
int __wrap_sd_id128_get_boot(sd_id128_t *value)
{
    if (!active || getpid()!=active->parent) return __real_sd_id128_get_boot(value);
    memcpy(value->bytes,active->profile->boot,16); return 0;
}
static int fake_pid1(struct ae_peer *p,int r)
{
    if (r==0 && active && getpid()==active->parent && strcmp(p->unique_name,active->unique)==0) {
        CHECK(p->connection_uid==0 && p->connection_pid==active->child && ae_peer_alive(p)==0);
        /* The private bus and ProcessFD are real. Only this admitted test
         * Manager's PID1 role is controlled; this is not actual PID1 admission. */
        p->connection_pid=1;
    }
    return r;
}
int __real_ae_peer_capture_reply(sd_bus_message *,sd_bus_message *,struct ae_peer *);
int __wrap_ae_peer_capture_reply(sd_bus_message *m,sd_bus_message *q,struct ae_peer *p)
{ return fake_pid1(p,__real_ae_peer_capture_reply(m,q,p)); }
int __real_ae_peer_capture_signal(sd_bus_message *,struct ae_peer *);
int __wrap_ae_peer_capture_signal(sd_bus_message *m,struct ae_peer *p)
{
    struct ae_peer empty=AE_PEER_INIT,marker=empty;
    CHECK(ae_peer_capture(m,&empty)==-EPROTO && memcmp(&empty,&marker,sizeof empty)==0);
    int r=__real_ae_peer_capture_signal(m,p);
    if (!r) {
        struct ae_peer occupied=*p;
        CHECK(__real_ae_peer_capture_signal(m,&occupied)==-EINVAL && memcmp(&occupied,p,sizeof occupied)==0);
    }
    return fake_pid1(p,r);
}
int __real_sd_bus_call_async(sd_bus *,sd_bus_slot **,sd_bus_message *,sd_bus_message_handler_t,void *,uint64_t);
int __wrap_sd_bus_call_async(sd_bus *bus,sd_bus_slot **slot,sd_bus_message *m,sd_bus_message_handler_t fn,void *userdata,uint64_t timeout)
{
    if (!active || getpid()!=active->parent || !sd_bus_message_is_method_call(m,MANAGER ".Manager","StartUnit"))
        return __real_sd_bus_call_async(bus,slot,m,fn,userdata,timeout);
    if (strcmp(active->mode,"submit-before")==0) return -EIO;
    int r=__real_sd_bus_call_async(bus,slot,m,fn,userdata,timeout);
    return r>=0 && strcmp(active->mode,"submit-after")==0 ? -EIO : r;
}
static void emit(sd_bus *bus,const char *unit,uint32_t id,const char *result)
{
    char path[96]; CHECK(snprintf(path,sizeof path,PATH "/job/%u",id)>0);
    CHECK(sd_bus_emit_signal(bus,PATH,MANAGER ".Manager","JobRemoved","uoss",id,path,unit,result)>=0);
}
static int ping(sd_bus_message *m,void *userdata,sd_bus_error *error)
{ (void)userdata; (void)error; return sd_bus_reply_method_return(m,""); }
static int count(sd_bus_message *m,void *userdata,sd_bus_error *error)
{ (void)error; struct state *s=userdata; return sd_bus_reply_method_return(m,"u",s->calls); }
static int drop(sd_bus_message *m,void *userdata,sd_bus_error *error)
{
    (void)error; struct state *s=userdata; CHECK(sd_bus_release_name(s->bus,MANAGER)>=0);
    return sd_bus_reply_method_return(m,"");
}
static int start(sd_bus_message *m,void *userdata,sd_bus_error *error)
{
    (void)error; struct state *s=userdata; const char *unit=NULL,*mode=NULL;
    CHECK(sd_bus_message_read(m,"ss",&unit,&mode)>0 && sd_bus_message_at_end(m,1)==1);
    CHECK(strcmp(unit,s->unit)==0 && strcmp(mode,"fail")==0 && ++s->calls==1);
    if (strcmp(s->mode,"early")==0 || strcmp(s->mode,"lost-reply")==0) emit(s->bus,unit,7,"done");
    if (strcmp(s->mode,"foreign-job")==0) emit(s->bus,unit,8,"done");
    if (strcmp(s->mode,"conflict")==0) { emit(s->bus,unit,7,"done"); emit(s->bus,unit,7,"failed"); }
    if (strcmp(s->mode,"overflow")==0) for (uint32_t n=8;n<13;++n) emit(s->bus,unit,n,"done");
    if (strcmp(s->mode,"wrong-sender")==0) {
        sd_bus *foreign=NULL; CHECK(sd_bus_open_user(&foreign)>=0);
        emit(foreign,unit,7,"failed"); CHECK(sd_bus_flush(foreign)>=0); sd_bus_flush_close_unref(foreign);
    }
    if (strcmp(s->mode,"owner-loss")==0) CHECK(sd_bus_release_name(s->bus,MANAGER)>=0);
    int r=1;
    if (strcmp(s->mode,"lost-reply")!=0) {
        r=sd_bus_reply_method_return(m,"o",strcmp(s->mode,"bad-path")==0 ? PATH "/job/007" : PATH "/job/7");
        if (strcmp(s->mode,"early")!=0 && strcmp(s->mode,"missing-job")!=0)
            emit(s->bus,unit,7,strcmp(s->mode,"failed-job")==0 ? "failed" : "done");
    }
    CHECK(sd_bus_flush(s->bus)>=0); return r;
}
static const sd_bus_vtable methods[]={
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("Ping","","",ping,SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("Count","","u",count,SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("DropName","","",drop,SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("StartUnit","ss","o",start,SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_SIGNAL("JobRemoved","uoss",0),
    SD_BUS_VTABLE_END
};
static void spawn(struct state *s)
{
    int ready[2],control[2]; CHECK(pipe(ready)==0 && pipe(control)==0);
    s->child=fork(); CHECK(s->child>=0);
    if (!s->child) {
        close(ready[0]); close(control[1]);
        CHECK(sd_bus_open_user(&s->bus)>=0);
        sd_bus_slot *slot=NULL; CHECK(sd_bus_add_object_vtable(s->bus,&slot,PATH,MANAGER ".Manager",methods,s)>=0);
        CHECK(sd_bus_request_name(s->bus,MANAGER,0)>=0);
        CHECK(write(ready[1],"R",1)==1 && close(ready[1])==0);
        struct pollfd p={.fd=control[0],.events=POLLIN};
        for (;;) {
            int r=sd_bus_process(s->bus,NULL); CHECK(r>=0);
            if (poll(&p,1,0)>0) { char c; CHECK(read(control[0],&c,1)==1 && c=='X'); break; }
            if (!r) CHECK(sd_bus_wait(s->bus,10000)>=0);
        }
        sd_bus_slot_unref(slot); sd_bus_flush_close_unref(s->bus); close(control[0]); _exit(0);
    }
    close(ready[1]); close(control[0]); s->control=control[1]; char r;
    CHECK(read(ready[0],&r,1)==1 && r=='R' && close(ready[0])==0);
}
static int descriptors(void)
{ DIR *d=opendir("/proc/self/fd"); CHECK(d); int n=0; while (readdir(d)) ++n; CHECK(closedir(d)==0); return n; }
static int64_t scalar(struct state *s,const char *sql)
{
    char path[4096]; CHECK(snprintf(path,sizeof path,"%s/catalogue.sqlite3",s->directory)>0);
    sqlite3 *db=NULL; sqlite3_stmt *q=NULL;
    CHECK(sqlite3_open_v2(path,&db,SQLITE_OPEN_READONLY,NULL)==SQLITE_OK && sqlite3_prepare_v2(db,sql,-1,&q,NULL)==SQLITE_OK);
    CHECK(sqlite3_step(q)==SQLITE_ROW); int64_t value=sqlite3_column_int64(q,0);
    CHECK(sqlite3_finalize(q)==SQLITE_OK && sqlite3_close(db)==SQLITE_OK); return value;
}
static int admit(void *userdata,const struct ae_execution_profile *p,const char *unit)
{
    struct state *s=userdata; ++s->admissions; uint8_t a[32],b[32];
    CHECK(ae_execution_digest(p,a)==0 && ae_execution_digest(s->profile,b)==0 && memcmp(a,b,32)==0 && strcmp(unit,s->unit)==0);
    CHECK(scalar(s,"SELECT count(*) FROM operation_steps WHERE step_no=1 AND state='executing'")==1);
    struct ae_catalogue *copy=s->store; CHECK(ae_catalogue_close(&copy)==-EBUSY && copy==s->store);
    struct ae_supervisor_start *same=s->start;
    CHECK(ae_supervisor_start_close(&same)==-EBUSY && same==s->start);
    if (strcmp(s->mode,"guard-deny")==0) return -EPERM;
    if (strcmp(s->mode,"late-fence")==0 && s->admissions==2) {
        struct ae_execution_fence_status state; CHECK(ae_catalogue_fence_execution(s->store,&state)==0 && state.active==3);
    }
    if (strcmp(s->mode,"guard-owner-loss")==0 && s->admissions==2) {
        CHECK(sd_bus_call_method(s->bus,s->unique,PATH,MANAGER ".Manager","DropName",NULL,NULL,"")>=0);
        struct ae_start_status status;
        for (unsigned int n=0;n<50;++n) {
            CHECK(sd_bus_process(s->bus,NULL)>=0 && ae_supervisor_start_status(s->start,&status)==0);
            if (status.quarantined) break;
            CHECK(sd_bus_wait(s->bus,10000)>=0);
        }
        CHECK(status.quarantined && !status.attempted);
    }
    return 0;
}
static int release(void *userdata,const struct ae_execution_profile *p,const struct ae_dispatch_snapshot *d)
{ (void)userdata; (void)p; (void)d; CHECK(0); return -ENOTSUP; }
void ae_test_start(struct ae_catalogue *store,const struct ae_execution_profile *p,struct ae_launch_context *context,const char *directory,const char *mode)
{
    struct state s={.parent=getpid(),.mode=mode,.directory=directory,.store=store,.profile=p,.context=context}; active=&s;
    struct ae_dispatch_snapshot d; CHECK(ae_catalogue_get_dispatch(store,p->lease,context,&d)==0); strcpy(s.unit,d.unit);
    int baseline=descriptors(); spawn(&s); CHECK(sd_bus_open_user(&s.bus)>=0);
    sd_bus_creds *creds=NULL; const char *name=NULL;
    CHECK(sd_bus_get_name_creds(s.bus,MANAGER,SD_BUS_CREDS_UNIQUE_NAME,&creds)>=0 && sd_bus_creds_get_unique_name(creds,&name)>=0);
    CHECK(strlen(name)<sizeof s.unique); strcpy(s.unique,name); sd_bus_creds_unref(creds);
    sd_bus_message *q=NULL,*reply=NULL;
    CHECK(sd_bus_message_new_method_call(s.bus,&q,s.unique,PATH,MANAGER ".Manager","Ping")>=0 && sd_bus_call(s.bus,q,1000000,NULL,&reply)>=0);
    struct ae_peer manager=AE_PEER_INIT; CHECK(ae_peer_capture_reply(reply,q,&manager)==0 && manager.connection_pid==1);
    sd_bus_message_unref(reply); sd_bus_message_unref(q);
    CHECK(ae_supervisor_start_new(store,s.bus,&manager,p,context,admit,&s,&s.start)==0); ae_peer_clear(&manager);
    sd_event *event=NULL; CHECK(sd_event_new(&event)>=0);
    struct ae_execution_effects effects={.owner=s.start,.request=ae_supervisor_start_request,.release=release};
    struct ae_execution_service *service=NULL; CHECK(ae_execution_service_new(store,s.bus,event,p,context,&effects,&service)==0);
    struct ae_execution_result result,marker; memset(&marker,0xa5,sizeof marker); result=marker;
    int denied=strcmp(mode,"guard-deny")==0 || strcmp(mode,"late-fence")==0 || strcmp(mode,"guard-owner-loss")==0;
    int submit=strncmp(mode,"submit-",7)==0;
    int r=ae_execution_service_request(service,context,&result);
    CHECK(r==(strcmp(mode,"guard-deny")==0 ? -EPERM : strcmp(mode,"late-fence")==0 ? -ESHUTDOWN :
        strcmp(mode,"guard-owner-loss")==0 ? -ESTALE : submit ? -EIO : 0));
    if (r<0) CHECK(memcmp(&result,&marker,sizeof result)==0);
    CHECK(ae_execution_service_request(service,context,&result)==-EALREADY);
    CHECK(ae_supervisor_start_request(s.start,p,&d)==-EALREADY);
    struct ae_start_status status; CHECK(ae_supervisor_start_status(s.start,&status)==0 && status.used && status.attempted==!denied);
    struct ae_execution_fence_status fence;
    CHECK(ae_catalogue_execution_fence_status(store,&fence)==0 && fence.active==(denied ? 0U : 1U));
    if (!denied) {
        struct ae_catalogue *same=store; CHECK(ae_catalogue_close(&same)==-EBUSY && same==store);
        uint8_t replacement[16]; memcpy(replacement,p->system_runtime,16); ++replacement[15];
        CHECK(ae_catalogue_begin_runtime(store,p->boot,replacement)==-EBUSY);
        struct ae_supervisor_start *same_start=s.start; CHECK(ae_supervisor_start_close(&same_start)==-EBUSY && same_start==s.start);
    }
    if (strcmp(mode,"fenced-pending")==0) CHECK(ae_catalogue_fence_execution(store,&fence)==0 && fence.active==1);
    if (strcmp(mode,"fork")==0) {
        pid_t child=fork(); CHECK(child>=0);
        if (!child) { struct ae_start_status sentinel; memset(&sentinel,0xa5,sizeof sentinel); status=sentinel;
            CHECK(ae_supervisor_start_status(s.start,&status)==-ECHILD && memcmp(&status,&sentinel,sizeof status)==0);
            CHECK(ae_supervisor_start_abandon(&s.start)==-ECHILD && s.start); _exit(0); }
        int code; CHECK(waitpid(child,&code,0)==child && WIFEXITED(code) && WEXITSTATUS(code)==0);
    }
    /* Pump the original bus, never retry the call or search jobs by unit name. */
    if (!denied && strcmp(mode,"submit-before")!=0) {
        for (unsigned int n=0;n<600;++n) {
            CHECK(sd_bus_process(s.bus,NULL)>=0 && ae_supervisor_start_status(s.start,&status)==0);
            if (status.job_removed || (status.quarantined && !status.pending) ||
                (strcmp(mode,"missing-job")==0 && status.reply_confirmed && !status.pending)) break;
            CHECK(sd_bus_wait(s.bus,10000)>=0);
        }
    }
    CHECK(ae_supervisor_start_status(s.start,&status)==0);
    int unknown=submit || strcmp(mode,"lost-reply")==0 || strcmp(mode,"bad-path")==0 || strcmp(mode,"conflict")==0 ||
        strcmp(mode,"overflow")==0 || strcmp(mode,"owner-loss")==0;
    int missing=strcmp(mode,"missing-job")==0;
    if (!denied && !unknown && !missing) CHECK(status.reply_confirmed && status.job_removed && !status.quarantined && status.job_id==7 &&
        strcmp(status.result,strcmp(mode,"failed-job")==0 ? "failed" : "done")==0);
    if (missing) CHECK(status.reply_confirmed && !status.pending && !status.quarantined && !status.job_removed && status.job_id==7);
    if (unknown) CHECK(status.quarantined && !status.job_removed);
    if (strcmp(mode,"late-owner-loss")==0) {
        struct ae_start_status original=status;
        CHECK(sd_bus_call_method(s.bus,s.unique,PATH,MANAGER ".Manager","DropName",NULL,NULL,"")>=0);
        for (unsigned int n=0;n<8;++n) { CHECK(sd_bus_process(s.bus,NULL)>=0); CHECK(sd_bus_wait(s.bus,1000)>=0); }
        CHECK(ae_supervisor_start_status(s.start,&status)==0 && memcmp(&status,&original,sizeof status)==0);
    }
    CHECK(ae_catalogue_execution_fence_status(store,&fence)==0 && fence.active==(unknown || missing ? 1U : 0U));
    if (unknown || missing) { CHECK(ae_supervisor_start_close(&s.start)==-EBUSY && s.start); CHECK(ae_supervisor_start_abandon(&s.start)==0); }
    else CHECK(ae_supervisor_start_close(&s.start)==0);
    CHECK(ae_catalogue_execution_fence_status(store,&fence)==0 && fence.active==0);
    CHECK(scalar(&s,"SELECT count(*) FROM operation_steps WHERE step_no=1 AND state='executing'")==1 &&
        scalar(&s,"SELECT count(*) FROM operation_steps WHERE step_no>1")==0 && scalar(&s,"SELECT state='reserved' FROM generation_leases")==1 &&
        scalar(&s,"SELECT state='reserved' FROM launch_dispatches")==1 && scalar(&s,"SELECT reason_code='exec_requested' AND state='running' FROM operations")==1);
    reply=NULL; uint32_t calls=99;
    CHECK(sd_bus_call_method(s.bus,s.unique,PATH,MANAGER ".Manager","Count",NULL,&reply,"")>=0 && sd_bus_message_read(reply,"u",&calls)>0);
    CHECK(calls==(denied || strcmp(mode,"submit-before")==0 ? 0U : 1U)); sd_bus_message_unref(reply);
    CHECK(ae_execution_service_close(&service)==0); sd_event_unref(event); sd_bus_flush_close_unref(s.bus);
    CHECK(write(s.control,"X",1)==1 && close(s.control)==0); int code;
    CHECK(waitpid(s.child,&code,0)==s.child && WIFEXITED(code) && WEXITSTATUS(code)==0 && descriptors()==baseline);
    printf("supervisor start %s: original call/job, bounded signal join, source fence, held owner and unchanged lease passed\n",mode); active=NULL;
}
