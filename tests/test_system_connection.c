#include "authority/system_connection.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"system connection %d: %s\n",__LINE__,#x); exit(1); } } while (0)
static const uint8_t epoch[16]={1,0,0,0,0,0,0x40,0,0x80};
static const uint8_t runtime[16]={2,0,0,0,0,0,0x40,0,0x80};
static const uint8_t events[16]={3,0,0,0,0,0,0x40,0,0x80};
static struct ae_process_context context;
static struct ae_system_connection *reentrant;
static int wrong_peer, wrong_context, recurse;
/* Preserve real bus/cookie/sender/ProcessFD capture. Only the unconfined private
 * component fixture's domain and PID1 context are controlled here. Actual
 * cross-UID/policy/MainPID checks belong to the enforcing normal-services run. */
int __real_ae_peer_capture_reply(sd_bus_message *,sd_bus_message *,struct ae_peer *);
int __wrap_ae_peer_capture_reply(sd_bus_message *m,sd_bus_message *q,struct ae_peer *p)
{
    int r=__real_ae_peer_capture_reply(m,q,p);
    if (r==0) {
        strcpy(p->connection_label,wrong_peer==1 ? "system_u:system_r:foreign_t:s0" : "system_u:system_r:aetheri_system_t:s0");
        if (wrong_peer==2) p->connection_uid=1000;
    }
    return r;
}
int __wrap_ae_process_observe_main(const struct ae_peer *p,sd_bus *b,struct ae_process_context *out)
{ (void)p; (void)b; *out=context; return 0; }
int __wrap_ae_process_verify_main(const struct ae_peer *p,sd_bus *b,const struct ae_process_context *expected)
{
    (void)p; (void)b;
    if (recurse) {
        recurse=0;
        struct ae_system_connection *same=reentrant;
        CHECK(ae_system_connection_close(&same)==-EBUSY && same==reentrant);
        CHECK(ae_system_connection_verify(reentrant)==-EBUSY);
    }
    return wrong_context ? -ESTALE : ae_process_context_equal(expected,&context) ? 0 : -ESTALE;
}
struct server { sd_bus *bus; unsigned int mode,checks,registrations; };
static int bytes_reply(sd_bus_message *m,const uint8_t *bytes,int extra)
{
    sd_bus_message *reply=NULL; int r=sd_bus_message_new_method_return(m,&reply);
    if (r>=0) r=sd_bus_message_append_array(reply,'y',bytes,16);
    if (r>=0 && extra) r=sd_bus_message_append(reply,"u",1U);
    if (r>=0) r=sd_bus_send(NULL,reply,NULL);
    sd_bus_message_unref(reply); return r<0 ? r : 1;
}
static int info(sd_bus_message *m,void *data,sd_bus_error *error)
{
    (void)error;
    struct server *s=data; sd_bus_message *reply=NULL;
    int r=sd_bus_message_new_method_return(m,&reply);
    if (r>=0) r=sd_bus_message_append(reply,"uu",s->mode==5 ? 2U : 1U,1U);
    if (r>=0) r=sd_bus_message_open_container(reply,'a',"s");
    if (r>=0) r=sd_bus_message_close_container(reply);
    if (r>=0) r=sd_bus_message_append_array(reply,'y',s->mode==6 ? runtime : epoch,16);
    if (r>=0) r=sd_bus_message_append_array(reply,'y',events,16);
    if (r>=0) r=sd_bus_send(NULL,reply,NULL);
    sd_bus_message_unref(reply); return r<0 ? r : 1;
}
static int registration(sd_bus_message *m,void *data,sd_bus_error *error)
{
    (void)error; struct server *s=data; ++s->registrations;
    for (unsigned int n=0;n<4;++n) {
        const void *p=NULL; size_t size=0;
        CHECK(sd_bus_message_read_array(m,'y',&p,&size)>0 && size==16);
        if (!n) CHECK(memcmp(p,epoch,16)==0);
    }
    CHECK(sd_bus_message_at_end(m,1)==1);
    return bytes_reply(m,runtime,s->mode==7);
}
static int check(sd_bus_message *m,void *data,sd_bus_error *error)
{
    struct server *s=data; ++s->checks;
    if (s->mode==9) return 1; /* Sent request, deliberately no acknowledgement. */
    if (s->mode==3) return sd_bus_error_set_const(error,"org.aetheri.Error1.NotAuthorized","Controlled rejection");
    return bytes_reply(m,s->mode==1 ? events : runtime,s->mode==2);
}
static int control(sd_bus_message *m,void *data,sd_bus_error *error)
{
    (void)error; struct server *s=data; uint32_t mode=0;
    CHECK(sd_bus_message_read(m,"u",&mode)>0); s->mode=mode;
    if (mode==4 || mode==8) CHECK(sd_bus_release_name(s->bus,"org.aetheri.System1")>=0);
    if (mode==8) CHECK(sd_bus_request_name(s->bus,"org.aetheri.System1",0)>=0);
    return sd_bus_reply_method_return(m,"uu",s->checks,s->registrations);
}
static const sd_bus_vtable management[]={SD_BUS_VTABLE_START(0),SD_BUS_METHOD("GetProtocolInfo","","uuasayay",info,SD_BUS_VTABLE_UNPRIVILEGED),SD_BUS_VTABLE_END};
static const sd_bus_vtable broker[]={SD_BUS_VTABLE_START(0),SD_BUS_METHOD("Register","ayayayay","ay",registration,SD_BUS_VTABLE_UNPRIVILEGED),SD_BUS_METHOD("Check","ay","ay",check,SD_BUS_VTABLE_UNPRIVILEGED),SD_BUS_VTABLE_END};
static const sd_bus_vtable controls[]={SD_BUS_VTABLE_START(0),SD_BUS_METHOD("Mode","u","uu",control,SD_BUS_VTABLE_UNPRIVILEGED),SD_BUS_VTABLE_END};
static pid_t spawn(int *stop,char unique[256])
{
    int ready[2],finish[2]; CHECK(pipe2(ready,O_CLOEXEC)==0 && pipe2(finish,O_CLOEXEC)==0);
    pid_t child=fork(); CHECK(child>=0);
    if (child==0) {
        alarm(30); close(ready[0]); close(finish[1]);
        struct server s={0}; CHECK(sd_bus_open_user(&s.bus)>=0);
        CHECK(sd_bus_add_object_vtable(s.bus,NULL,"/org/aetheri/Management1","org.aetheri.Management1",management,&s)>=0);
        CHECK(sd_bus_add_object_vtable(s.bus,NULL,"/org/aetheri/Broker1","org.aetheri.Broker1",broker,&s)>=0);
        CHECK(sd_bus_add_object_vtable(s.bus,NULL,"/test","org.aetheri.Test",controls,&s)>=0);
        CHECK(sd_bus_request_name(s.bus,"org.aetheri.System1",0)>=0);
        const char *name=NULL; CHECK(sd_bus_get_unique_name(s.bus,&name)>=0);
        char name_buffer[256]={0}; strcpy(name_buffer,name);
        CHECK(write(ready[1],name_buffer,sizeof name_buffer)==sizeof name_buffer); close(ready[1]);
        CHECK(fcntl(finish[0],F_SETFL,O_NONBLOCK)==0);
        for (;;) {
            char byte; ssize_t n=read(finish[0],&byte,1);
            if (n==0) break;
            CHECK(n<0 && errno==EAGAIN);
            int r; do { r=sd_bus_process(s.bus,NULL); CHECK(r>=0); } while (r>0);
            CHECK(sd_bus_wait(s.bus,10000)>=0);
        }
        sd_bus_flush_close_unref(s.bus); close(finish[0]); _exit(0);
    }
    close(ready[1]); close(finish[0]);
    CHECK(read(ready[0],unique,256)==256); close(ready[0]); *stop=finish[1]; return child;
}
static void mode(sd_bus *bus,const char *unique,uint32_t value,uint32_t *checks)
{
    sd_bus_message *reply=NULL;
    CHECK(sd_bus_call_method(bus,unique,"/test","org.aetheri.Test","Mode",NULL,&reply,"u",value)>=0);
    uint32_t registrations=0; CHECK(sd_bus_message_read(reply,"uu",checks,&registrations)>0);
    sd_bus_message_unref(reply);
}
static size_t fds(void)
{
    DIR *d=opendir("/proc/self/fd"); CHECK(d); size_t n=0;
    while (readdir(d)) ++n;
    closedir(d); return n;
}
static void joined(pid_t child,int stop)
{ close(stop); int status=0; CHECK(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==0); }
int main(void)
{
    alarm(45); CHECK(geteuid()==0);
    context.uid=0; context.boot[0]=1; context.invocation[0]=2; context.cgroup_id=3;
    strcpy(context.unit,"aetheri-p0-fixture-system-1.service"); strcpy(context.cgroup,"/system.slice/fixture");
    strcpy(context.label,"system_u:system_r:aetheri_system_t:s0");
    struct ae_broker_identity user; memcpy(user.account,epoch,16); memcpy(user.epoch,events,16); memcpy(user.runtime,runtime,16);
    int stop; char unique[256]; pid_t child=spawn(&stop,unique);
    sd_bus *bus=NULL; sd_event *event=NULL;
    CHECK(sd_bus_open_user(&bus)>=0 && sd_event_new(&event)>=0 && sd_bus_attach_event(bus,event,0)>=0);
    struct ae_system_connection *c=NULL; uint32_t checks=0;
    #define NEW() ae_system_connection_new(bus,event,context.unit,epoch,context.boot,&user,&c)
    CHECK(NEW()==0); size_t bound=fds();
    uint8_t actual[16]; CHECK(ae_system_connection_runtime(c,actual)==0 && memcmp(actual,runtime,16)==0 && memcmp(actual,events,16)!=0);
    for (unsigned int i=0;i<12;++i) CHECK(ae_system_connection_check(c)==0);
    CHECK(fds()==bound);
    reentrant=c; recurse=1; CHECK(ae_system_connection_verify(c)==0 && !recurse);
    pid_t forked=fork(); CHECK(forked>=0);
    if (!forked) _exit(ae_system_connection_verify(c)==-ECHILD && ae_system_connection_close(&c)==-ECHILD ? 0 : 1);
    int status=0; CHECK(waitpid(forked,&status,0)==forked && status==0);
    CHECK(ae_system_connection_verify_selection(c,context.unit,epoch,context.boot)==0);
    wrong_context=1; CHECK(ae_system_connection_verify(c)==-ESTALE); wrong_context=0;
    CHECK(ae_system_connection_verify(c)==-ESTALE && !ae_system_connection_departed(c));
    mode(bus,unique,4,&checks);
    CHECK(ae_system_connection_verify(c)==-ENOTCONN && ae_system_connection_departed(c)==1);
    CHECK(fds()<bound); CHECK(ae_system_connection_close(&c)==0); joined(child,stop);
    child=spawn(&stop,unique);
    for (unsigned int i=0;i<4;++i) {
        unsigned int kind=i==3 ? 9U : i+1U;
        CHECK(NEW()==0); mode(bus,unique,kind,&checks); uint32_t before=checks;
        CHECK(ae_system_connection_check(c)<0 && !ae_system_connection_departed(c));
        CHECK(ae_system_connection_check(c)<0); mode(bus,unique,0,&checks);
        CHECK(checks==before+1); CHECK(ae_system_connection_close(&c)==0);
    }
    for (unsigned int kind=5;kind<=7;++kind) {
        mode(bus,unique,kind,&checks); CHECK(NEW()<0 && !c); mode(bus,unique,0,&checks);
    }
    for (wrong_peer=1;wrong_peer<=2;++wrong_peer) CHECK(NEW()==-EACCES && !c);
    wrong_peer=0;
    CHECK(NEW()==0);
    CHECK(ae_system_connection_verify_selection(c,"other.service",epoch,context.boot)==-ESTALE);
    CHECK(ae_system_connection_verify_selection(c,context.unit,epoch,context.boot)==-ESTALE);
    memset(actual,0xa5,sizeof actual); CHECK(ae_system_connection_runtime(c,actual)==-ESTALE && actual[0]==0xa5);
    CHECK(ae_system_connection_close(&c)==0);
    CHECK(NEW()==0); mode(bus,unique,8,&checks); /* Same process and unique name, lost ownership. */
    CHECK(ae_system_connection_verify(c)==-ENOTCONN && ae_system_connection_departed(c)==1);
    CHECK(ae_system_connection_close(&c)==0);
    context.unit[0]='x'; CHECK(ae_system_connection_new(bus,event,"other.service",epoch,context.boot,&user,&c)==-EACCES && !c); context.unit[0]='a';
    CHECK(NEW()==0);
    sd_bus_message *request=NULL,*reply=NULL;
    CHECK(sd_bus_message_new_method_call(bus,&request,"org.aetheri.System1","/test","org.aetheri.Test","Mode")>=0);
    CHECK(sd_bus_message_append(request,"u",0U)>=0);
    CHECK(ae_system_connection_call(c,request,&reply)==-EACCES && !reply);
    sd_bus_message_unref(request); CHECK(ae_system_connection_close(&c)==0);
    CHECK(NEW()==0); bound=fds(); joined(child,stop); /* Actual original process exit. */
    for (unsigned int n=0;!ae_system_connection_departed(c) && n<12;++n) CHECK(sd_event_run(event,1000000)>=0);
    CHECK(ae_system_connection_departed(c)==1 && fds()<bound);
    child=spawn(&stop,unique);
    CHECK(ae_system_connection_check(c)==-ENOTCONN); CHECK(ae_system_connection_close(&c)==0);
    CHECK(NEW()==0 && ae_system_connection_check(c)==0); CHECK(ae_system_connection_close(&c)==0);
    joined(child,stop);
    sd_bus_flush_close_unref(bus); sd_event_unref(event);
    puts("Actual correlated replies, event/runtime distinction, live quarantine, departure/replacement, no replay, fork/reentry and FD cleanup passed.");
    return 0;
}
