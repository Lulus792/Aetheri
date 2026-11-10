#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "session.h"
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/pidfd.h>
#include <systemd/sd-login.h>
#include <time.h>
#include <unistd.h>

struct ae_session {
    sd_login_monitor *monitor;
    char *session,*scope,*cgroup,*seat;
    pid_t pid,tid;
    uid_t uid;
    int pin;
    uint64_t group_id;
    unsigned loss;
};
static bool owner(const struct ae_session *s) {return s && s->pid==getpid() && s->tid==gettid();}
static bool bounded(const char *s,size_t limit) {return s && *s && strnlen(s,limit)<limit;}
static bool now_us(uint64_t *out) {
    struct timespec t;
    if(clock_gettime(CLOCK_MONOTONIC,&t) || t.tv_sec<0 || (uint64_t)t.tv_sec>UINT64_MAX/1000000u-1u) return false;
    *out=(uint64_t)t.tv_sec*1000000u+(uint64_t)t.tv_nsec/1000u;return true;
}
static bool identity(struct ae_session *s,bool first) {
    struct pidfd_info info={.mask=PIDFD_INFO_PID|PIDFD_INFO_CREDS|PIDFD_INFO_CGROUPID};
    uint64_t required=PIDFD_INFO_PID|PIDFD_INFO_CREDS|PIDFD_INFO_CGROUPID;
    if(ioctl(s->pin,PIDFD_GET_INFO,&info) || (info.mask&required)!=required || !info.cgroupid ||
       info.pid!=(uint32_t)s->pid || info.tgid!=info.pid || info.ruid!=s->uid || info.euid!=s->uid ||
       info.suid!=s->uid || info.fsuid!=s->uid || getuid()!=s->uid || geteuid()!=s->uid) return false;
    if(first) s->group_id=info.cgroupid;
    return s->group_id==info.cgroupid;
}
static unsigned snapshot(struct ae_session *s) {
    char *session=NULL,*scope=NULL,*cgroup=NULL,*seat=NULL,*type=NULL,*class=NULL;uid_t uid=0;
    unsigned loss=AE_SESSION_OBSERVATION_FAILED;
    if(!identity(s,false)) return AE_SESSION_CONTEXT_LOST;
    if(sd_pidfd_get_session(s->pin,&session)<0 || sd_pidfd_get_unit(s->pin,&scope)<0 ||
       sd_pidfd_get_cgroup(s->pin,&cgroup)<0 || !bounded(session,129) || !bounded(scope,256) || !bounded(cgroup,1024)) goto done;
    if(strcmp(session,s->session) || strcmp(scope,s->scope) || strcmp(cgroup,s->cgroup)) {loss=AE_SESSION_CONTEXT_LOST;goto done;}
    int active=sd_session_is_active(s->session),remote=sd_session_is_remote(s->session);
    if(active<0 || remote<0 || sd_session_get_uid(s->session,&uid)<0 || sd_session_get_seat(s->session,&seat)<0 ||
       sd_session_get_type(s->session,&type)<0 || sd_session_get_class(s->session,&class)<0 ||
       !bounded(seat,129) || !bounded(type,32) || !bounded(class,32)) goto done;
    loss=active>0 && remote==0 && uid==s->uid && !strcmp(seat,s->seat) && !strcmp(type,"wayland") && !strcmp(class,"user") ?
        AE_SESSION_LIVE:AE_SESSION_INACTIVE;
    if(!identity(s,false)) loss=AE_SESSION_CONTEXT_LOST;
done:
    free(session);free(scope);free(cgroup);free(seat);free(type);free(class);return loss;
}
int ae_session_poll(struct ae_session *s,struct pollfd *out,uint64_t *deadline) {
    if(!s || !out || !deadline) return -EINVAL;
    if(!owner(s)) return -EPERM;
    if(s->loss) return -ESTALE;
    int fd=sd_login_monitor_get_fd(s->monitor),events=sd_login_monitor_get_events(s->monitor);uint64_t until;
    if(fd<0 || events<=0 || events>SHRT_MAX || sd_login_monitor_get_timeout(s->monitor,&until)<0) {
        s->loss=AE_SESSION_OBSERVATION_FAILED;return -ESTALE;
    }
    *out=(struct pollfd){.fd=fd,.events=(short)events};*deadline=until;return 0;
}
int ae_session_check(struct ae_session *s,unsigned *out) {
    if(!s || !out) return -EINVAL;
    if(!owner(s)) return -EPERM;
    if(!s->loss) {
        struct pollfd p;uint64_t deadline,now;
        if(ae_session_poll(s,&p,&deadline)<0 || !now_us(&now)) s->loss=AE_SESSION_OBSERVATION_FAILED;
        else {
            int n;do {n=poll(&p,1,0);} while(n<0 && errno==EINTR);
            if(n<0 || p.revents&(POLLERR|POLLHUP|POLLNVAL) ||
               ((p.revents || now>=deadline) && sd_login_monitor_flush(s->monitor)<0)) s->loss=AE_SESSION_OBSERVATION_FAILED;
            else s->loss=snapshot(s);
        }
    }
    *out=s->loss;return 0;
}
static void destroy(struct ae_session *s) {
    sd_login_monitor_unref(s->monitor);if(s->pin>=0) close(s->pin);
    free(s->session);free(s->scope);free(s->cgroup);free(s->seat);free(s);
}
int ae_session_close(struct ae_session **pointer) {
    if(!pointer) return -EINVAL;
    struct ae_session *s=*pointer;if(!s) return 0;
    if(!owner(s)) return -EPERM;
    *pointer=NULL;destroy(s);return 0;
}
int ae_session_open(uid_t uid,const char *seat,struct ae_session **out) {
    if(!out || *out || !uid || uid!=getuid() || uid!=geteuid() || !bounded(seat,129)) return -EINVAL;
    if(getpid()!=gettid()) return -EPERM;
    for(const unsigned char *p=(const unsigned char *)seat;*p;++p)
        if(!((*p>='a' && *p<='z') || (*p>='A' && *p<='Z') || (*p>='0' && *p<='9') || *p=='_' || *p=='-' || *p=='.')) return -EINVAL;
    struct ae_session *s=calloc(1,sizeof(*s));if(!s) return -ENOMEM;
    s->pin=-1;s->pid=getpid();s->tid=gettid();s->uid=uid;s->seat=strdup(seat);
    /* Self only: never substitute this pin for a bus-authenticated caller FD. */
    s->pin=pidfd_open(s->pid,0);
    if(!s->seat || s->pin<0 || !identity(s,true) || sd_login_monitor_new("session",&s->monitor)<0 ||
       sd_pidfd_get_session(s->pin,&s->session)<0 || sd_pidfd_get_unit(s->pin,&s->scope)<0 ||
       sd_pidfd_get_cgroup(s->pin,&s->cgroup)<0 || !bounded(s->session,129) || !bounded(s->scope,256) ||
       !bounded(s->cgroup,1024) || s->cgroup[0]!='/' || strlen(s->scope)<7 || strcmp(s->scope+strlen(s->scope)-6,".scope")) goto failed;
    unsigned loss;
    if(ae_session_check(s,&loss)<0 || loss || snapshot(s)) goto failed;
    *out=s;return 0;
failed:
    destroy(s);return -EACCES;
}
