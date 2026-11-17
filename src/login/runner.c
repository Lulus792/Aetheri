#define _GNU_SOURCE
#include "login/runner.h"
#include "login/service_internal.h"
#include "linux/process.h"
#include <errno.h>
#include <limits.h>
#include <poll.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#define MANAGER "org.freedesktop.systemd1"
#define UNIT "org.freedesktop.systemd1.Unit"
#define SERVICE "org.freedesktop.systemd1.Service"
struct selection {char manager[256],path[1024];uint32_t pid;uint8_t invocation[16];};
struct runner {
    sd_bus *bus;
    struct ae_peer self;
    struct ae_process_context context;
    const struct ae_login_run_profile *profile;
    struct selection backend;
    uint64_t deadline;
    bool active;
};
static int failure(void){int e=errno;return e>0?-e:-EIO;}
static int text(char *out,size_t n,const char *value) {
    if(!value)return -EINVAL;
    size_t size=strnlen(value,n);if(!size || size==n)return -E2BIG;
    memcpy(out,value,size+1);return 0;
}
static int clock_usec(uint64_t *out) {
    struct timespec t;if(clock_gettime(CLOCK_MONOTONIC,&t))return failure();
    if(t.tv_sec<0 || t.tv_nsec<0 || t.tv_nsec>=1000000000)return -EOVERFLOW;
    uint64_t fraction=(uint64_t)t.tv_nsec/1000;
    if((uint64_t)t.tv_sec>(UINT64_MAX-fraction)/1000000)return -EOVERFLOW;
    *out=(uint64_t)t.tv_sec*1000000+fraction;return 0;
}
static int remaining(struct runner *r,int maximum) {
    uint64_t current;int result=clock_usec(&current);if(result)return result;
    if(current>=r->deadline)return -ETIMEDOUT;
    uint64_t delta=r->deadline-current,ms=delta/1000+(delta%1000!=0);
    return ms>(uint64_t)maximum?maximum:(int)ms;
}
static int base_gate(struct runner *r) {
    int result=r->profile->startup.check(r->profile->startup.userdata);
    return result>0?-EINVAL:result;
}
static int registration(struct runner *r) {
    struct ae_process_context observed;
    int result=ae_process_observe_registration(&r->self,r->bus,&observed);
    if(!result && !ae_process_context_equal(&observed,&r->context))result=-ESTALE;
    if(!result)result=base_gate(r);
    return result;
}
static int gate(void *value) {
    struct runner *r=value;
    int result=ae_process_verify_main(&r->self,r->bus,&r->context);
    if(!result)result=base_gate(r);
    return result;
}
static int enter_view(void *value) {
    struct runner *r=value;
    return r->profile->startup.enter_view(r->profile->startup.userdata);
}
static int pause_initial(struct runner *r) {
    int ms=remaining(r,25);if(ms<0)return ms;
    struct timespec t={.tv_nsec=(long)ms*1000000};
    if(nanosleep(&t,NULL) && errno!=EINTR)return failure();
    return r->active?gate(r):registration(r);
}
static int invocation(sd_bus *bus,const struct selection *s,uint8_t out[16]) {
    sd_bus_message *reply=NULL;
    int r=sd_bus_get_property(bus,s->manager,s->path,UNIT,"InvocationID",NULL,&reply,"ay");
    const void *bytes=NULL;size_t size=0;
    if(r>=0)r=sd_bus_message_read_array(reply,'y',&bytes,&size);
    if(r>=0 && (!bytes || size!=16))r=-EBADMSG;
    if(r>=0){uint8_t any=0;memcpy(out,bytes,16);for(size_t i=0;i<16;++i)any|=out[i];if(!any)r=-ENODATA;}
    sd_bus_message_unref(reply);return r<0?r:0;
}
static int selected(struct runner *r) {
    struct selection *s=&r->backend;uint32_t pid=0;uint8_t id[16];
    int result=sd_bus_get_property_trivial(r->bus,s->manager,s->path,SERVICE,"MainPID",NULL,'u',&pid);
    if(result>=0)result=invocation(r->bus,s,id);
    if(!result && (pid!=s->pid || memcmp(id,s->invocation,16)))result=-ESTALE;
    return result<0?result:0;
}
static int select_backend(struct runner *r) {
    struct selection *s=&r->backend;sd_bus_creds *credentials=NULL;
    int result=sd_bus_get_name_creds(r->bus,MANAGER,SD_BUS_CREDS_UNIQUE_NAME,&credentials);
    const char *manager=NULL;
    if(result>=0)result=sd_bus_creds_get_unique_name(credentials,&manager);
    if(result>=0)result=text(s->manager,sizeof s->manager,manager);
    if(!result && s->manager[0]!=':')result=-EPROTO;
    sd_bus_creds_unref(credentials);
    sd_bus_message *reply=NULL;const char *path=NULL;
    if(!result)result=sd_bus_call_method(r->bus,s->manager,"/org/freedesktop/systemd1",MANAGER ".Manager",
        "GetUnit",NULL,&reply,"s",r->profile->startup.service.unit);
    if(result>=0 && !sd_bus_message_has_signature(reply,"o"))result=-EBADMSG;
    if(result>=0)result=sd_bus_message_read(reply,"o",&path);
    if(result==0)result=-EBADMSG;
    if(result>0)result=text(s->path,sizeof s->path,path);
    sd_bus_message_unref(reply);
    if(!result)result=sd_bus_get_property_trivial(r->bus,s->manager,s->path,SERVICE,"MainPID",NULL,'u',&s->pid);
    if(result>=0 && (!s->pid || s->pid>INT32_MAX))result=-ESRCH;
    if(result>=0)result=invocation(r->bus,s,s->invocation);
    if(!result)result=selected(r);
    return result<0?result:0;
}
static int match_selected(sd_bus *bus,const char *manager,const char *path,
    const struct ae_image_process *process,const uint8_t id[16],const uint8_t boot[16],uint64_t group,void *value) {
    (void)bus;(void)boot;(void)group;
    const struct selection *s=value;
    return strcmp(manager,s->manager) || strcmp(path,s->path) ||
        (uint32_t)process->pid!=s->pid || memcmp(id,s->invocation,16)?-ESTALE:0;
}
static int connect_once(struct runner *r,int *out) {
    struct sockaddr_un address={.sun_family=AF_UNIX};
    int n=snprintf(address.sun_path,sizeof address.sun_path,"%s%u.sock",r->profile->socket_prefix,r->backend.pid);
    if(n<0 || (size_t)n>=sizeof address.sun_path)return -ENAMETOOLONG;
    for(;;) {
        int result=selected(r);if(result)return result;
        struct stat st;
        if(!lstat(address.sun_path,&st)){if(!S_ISSOCK(st.st_mode))return -EPROTO;break;}
        if(errno!=ENOENT)return failure();
        result=pause_initial(r);if(result)return result;
    }
    int fd=socket(AF_UNIX,SOCK_STREAM|SOCK_CLOEXEC|SOCK_NONBLOCK,0);if(fd<0)return failure();
    int result=0;
    if(connect(fd,(const struct sockaddr *)&address,sizeof address)) {
        if(errno!=EINPROGRESS)result=failure();
        else for(;;) {
            int ms=remaining(r,INT_MAX);if(ms<0){result=ms;break;}
            struct pollfd p={.fd=fd,.events=POLLOUT};int polled=poll(&p,1,ms);
            if(polled<0){if(errno==EINTR)continue;result=failure();break;}
            if(!polled){result=-ETIMEDOUT;break;}
            int error=0;socklen_t size=sizeof error;
            if(getsockopt(fd,SOL_SOCKET,SO_ERROR,&error,&size))result=failure();
            else if(size!=sizeof error)result=-EPROTO;
            else if(error)result=-error;
            else if(!(p.revents&POLLOUT) || (p.revents&(POLLERR|POLLHUP|POLLNVAL)))result=-ECONNABORTED;
            break;
        }
    }
    if(!result)result=selected(r);
    if(!result)result=gate(r);
    if(result){close(fd);return result;}
    *out=fd;return 0;
}
int ae_login_startup_run(const struct ae_login_run_profile *p) {
    if(!p || !p->unit || !p->label || !*p->label || strnlen(p->label,512)==512 ||
       strnlen(p->unit,256)==256 || !p->startup.service.unit || !p->startup.service.label ||
       !p->startup.service.socket_label || p->startup.service.uid || p->startup.service.gid ||
       !p->socket_prefix || p->socket_prefix[0]!='/' || strnlen(p->socket_prefix,100)==100 ||
       !p->startup.check || p->timeout_usec<100000 || p->timeout_usec>15000000)return -EINVAL;
    uid_t u,e,s;gid_t g,ge,gs;
    if(getresuid(&u,&e,&s) || getresgid(&g,&ge,&gs))return failure();
    if(u || e || s || g || ge || gs || getpid()!=gettid())return -EACCES;
    struct runner r={.self=AE_PEER_INIT,.profile=p};
    const char *phase="self";
    int result=clock_usec(&r.deadline);
    if(!result && r.deadline>UINT64_MAX-p->timeout_usec)result=-EOVERFLOW;
    if(!result)r.deadline+=p->timeout_usec;
    if(!result)result=base_gate(&r);
    if(result>=0)result=sd_bus_new(&r.bus);
    if(result>=0)result=sd_bus_set_address(r.bus,"unix:path=/run/dbus/system_bus_socket");
    if(result>=0)result=sd_bus_set_bus_client(r.bus,1);
    if(result>=0)result=sd_bus_set_method_call_timeout(r.bus,1000000);
    if(result>=0)result=sd_bus_start(r.bus);
    if(result>=0)result=0;
    if(!result)result=ae_peer_capture_self(r.bus,&r.self);
    if(!result)result=ae_process_observe_registration(&r.self,r.bus,&r.context);
    if(!result && (r.context.uid || strcmp(r.context.unit,p->unit) || strcmp(r.context.label,p->label)))result=-EACCES;
    while(!result) {
        struct ae_process_context current;
        int observed=ae_process_observe_main(&r.self,r.bus,&current);
        if(!observed){if(!ae_process_context_equal(&current,&r.context))result=-ESTALE;else r.active=true;break;}
        if(observed!=-EAGAIN){result=observed;break;}
        result=pause_initial(&r);
    }
    if(!result){phase="backend";result=select_backend(&r);}
    if(!result)puts("AETHERI_LOGIN_STARTUP_SELECTED");
    int fd=-1;
    if(!result){phase="connection";result=connect_once(&r,&fd);}
    struct ae_login_service *peer=NULL;
    if(!result){phase="peer";result=ae_login_service_create(fd,r.bus,&p->startup.service,&peer);}
    if(!result){phase="selection";result=ae_login_service_inspect(peer,match_selected,&r.backend);}
    struct ae_login_startup *startup=NULL;
    struct ae_login_startup_expect expected=p->startup;expected.check=gate;expected.userdata=&r;
    if(expected.enter_view)expected.enter_view=enter_view;
    if(!result){phase="namespace";result=ae_login_startup_create(fd,r.bus,&expected,&startup);}
    while(!result) {
        phase="readiness";
        int ready=ae_login_startup_check(startup);
        if(!ready)break;
        if(ready!=-EAGAIN){result=ready;break;}
        result=pause_initial(&r);
    }
    if(!result){int ms=remaining(&r,1);if(ms<0)result=ms;}
    if(!result){phase="publication";result=ae_login_startup_publish(startup);}
    if(result)fprintf(stderr,"AETHERI_LOGIN_STARTUP_STAGE phase=%s error=%d\n",phase,result);
    int closed=ae_login_startup_close(&startup);if(!result && closed)result=closed;
    closed=ae_login_service_close(&peer);if(!result && closed)result=closed;
    if(fd>=0 && close(fd) && !result)result=failure();
    ae_peer_clear(&r.self);sd_bus_unref(r.bus);
    return result;
}
