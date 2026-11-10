/* Controlled logind metadata/monitor, real self pidfd/kernel credentials and
   cgroup ID, real eventfd/poll/thread/fork lifetimes. Never login admission. */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <poll.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <systemd/sd-login.h>
#include <unistd.h>
#include <dirent.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"session check %d: %s errno=%d\n",__LINE__,#x,errno);abort();}} while(0)
struct sd_login_monitor {int fd;};
static struct sd_login_monitor *monitor;
static int model_active=1,model_remote=0,query_error,monitor_error,flush_error;
static bool group_changed,uid_changed;
static const char *current_session="original",*current_scope="session-original.scope",*current_group="/user.slice/session-original.scope";
static const char *current_seat="seat0",*current_type="wayland",*current_class="user";
static uint64_t monitor_deadline=UINT64_MAX;
static unsigned snapshots,flushes;
static pid_t creator_pid;
static int monitor_new(const char *category,sd_login_monitor **out) {
    CHECK(!strcmp(category,"session") && !monitor);
    if(monitor_error)return -EIO;
    monitor=calloc(1,sizeof(*monitor));CHECK(monitor);monitor->fd=eventfd(0,EFD_NONBLOCK|EFD_CLOEXEC);CHECK(monitor->fd>=0);*out=monitor;return 0;
}
static sd_login_monitor *monitor_unref(sd_login_monitor *p) {if(p){CHECK(p==monitor && !close(p->fd));free(p);monitor=NULL;}return NULL;}
static int monitor_fd(sd_login_monitor *p) {CHECK(p==monitor);return monitor_error?-EIO:p->fd;}
static int monitor_events(sd_login_monitor *p) {CHECK(p==monitor);return POLLIN;}
static int monitor_timeout(sd_login_monitor *p,uint64_t *out) {CHECK(p==monitor);*out=monitor_deadline;return 0;}
static int monitor_flush(sd_login_monitor *p) {CHECK(p==monitor);++flushes;if(flush_error)return -EIO;uint64_t n;while(read(p->fd,&n,sizeof(n))>0){}CHECK(errno==EAGAIN);return 0;}
static int text(const char *value,char **out) {if(query_error)return -EIO;*out=strdup(value);return *out?0:-ENOMEM;}
static int session_of(int pin,char **out) {CHECK(pin>=0 && fcntl(pin,F_GETFD)&FD_CLOEXEC);++snapshots;return text(current_session,out);}
static int scope_of(int pin,char **out) {(void)pin;return text(current_scope,out);}
static int group_of(int pin,char **out) {(void)pin;return text(current_group,out);}
static int session_uid(const char *s,uid_t *out) {(void)s;*out=getuid();return 0;}
static int session_active(const char *s) {(void)s;return model_active;}
static int session_remote(const char *s) {(void)s;return model_remote;}
static int session_seat(const char *s,char **out) {(void)s;return text(current_seat,out);}
static int session_type(const char *s,char **out) {(void)s;return text(current_type,out);}
static int session_class(const char *s,char **out) {(void)s;return text(current_class,out);}
struct pidfd_info;
static int subject_ioctl(int fd,unsigned long request,struct pidfd_info *out);
static uid_t subject_uid(void) {return uid_changed?0:getuid();}
#define sd_login_monitor_new monitor_new
#define sd_login_monitor_unref monitor_unref
#define sd_login_monitor_get_fd monitor_fd
#define sd_login_monitor_get_events monitor_events
#define sd_login_monitor_get_timeout monitor_timeout
#define sd_login_monitor_flush monitor_flush
#define sd_pidfd_get_session session_of
#define sd_pidfd_get_unit scope_of
#define sd_pidfd_get_cgroup group_of
#define sd_session_get_uid session_uid
#define sd_session_is_active session_active
#define sd_session_is_remote session_remote
#define sd_session_get_seat session_seat
#define sd_session_get_type session_type
#define sd_session_get_class session_class
#define ioctl subject_ioctl
#define getuid subject_uid
#include "../src/linux/session.c"
#undef getuid
#undef ioctl
static int subject_ioctl(int fd,unsigned long request,struct pidfd_info *out) {int r=ioctl(fd,request,out);if(!r && group_changed)out->cgroupid^=1;return r;}

static size_t fd_count(void) {DIR *d=opendir("/proc/self/fd");CHECK(d);size_t n=0;while(readdir(d))++n;CHECK(!closedir(d));return n;}
static struct ae_session *open_session(void) {struct ae_session *s=NULL;CHECK(!ae_session_open(getuid(),"seat0",&s) && s);return s;}
static void clean(struct ae_session **s) {CHECK(!ae_session_close(s) && !*s && !monitor);}
static void *foreign_thread(void *data) {
    struct ae_session *s=data,*copy=s;unsigned loss=99;struct pollfd p={.fd=123,.events=7,.revents=3};uint64_t until=55;
    CHECK(ae_session_check(s,&loss)==-EPERM && loss==99);
    CHECK(ae_session_poll(s,&p,&until)==-EPERM && p.fd==123 && p.events==7 && p.revents==3 && until==55);
    CHECK(ae_session_close(&copy)==-EPERM && copy==s);return NULL;
}
static void *foreign_open(void *unused) {
    (void)unused;struct ae_session *s=NULL;CHECK(getpid()==creator_pid);
    CHECK(ae_session_open(getuid(),"seat0",&s)==-EPERM && !s);return NULL;
}
int main(void) {
    if(!getuid()){puts("requires an ordinary native account");return 77;}
    creator_pid=getpid();
    size_t before=fd_count();struct ae_session *s=NULL;unsigned loss=99;
    CHECK(ae_session_open(0,"seat0",&s)==-EINVAL && !s);
    CHECK(ae_session_open(getuid(),"../seat0",&s)==-EINVAL && !s);
    monitor_error=1;CHECK(ae_session_open(getuid(),"seat0",&s)<0 && !s && !monitor);monitor_error=0;
    query_error=1;CHECK(ae_session_open(getuid(),"seat0",&s)<0 && !s && !monitor);query_error=0;
    current_scope="foreign.service";CHECK(ae_session_open(getuid(),"seat0",&s)<0 && !s);current_scope="session-original.scope";
    model_active=0;CHECK(ae_session_open(getuid(),"seat0",&s)<0 && !s);model_active=1;
    model_remote=1;CHECK(ae_session_open(getuid(),"seat0",&s)<0 && !s);model_remote=0;
    s=open_session();CHECK(!ae_session_check(s,&loss) && loss==AE_SESSION_LIVE);
    struct ae_session *original=s;CHECK(ae_session_open(getuid(),"seat0",&s)==-EINVAL && s==original);
    unsigned previous=snapshots;CHECK(!ae_session_check(s,&loss) && !loss && snapshots>previous); /* no event required */
    struct pollfd p;uint64_t until;CHECK(!ae_session_poll(s,&p,&until) && p.fd==monitor->fd && p.events==POLLIN && until==UINT64_MAX);
    pthread_t thread;CHECK(!pthread_create(&thread,NULL,foreign_thread,s) && !pthread_join(thread,NULL));
    CHECK(!pthread_create(&thread,NULL,foreign_open,NULL) && !pthread_join(thread,NULL));
    pid_t child=fork();CHECK(child>=0);if(!child){foreign_thread(s);_exit(0);}int status;CHECK(waitpid(child,&status,0)==child && WIFEXITED(status) && !WEXITSTATUS(status));
    current_session="replacement";CHECK(!ae_session_check(s,&loss) && loss==AE_SESSION_CONTEXT_LOST);current_session="original";
    previous=snapshots;CHECK(!ae_session_check(s,&loss) && loss==AE_SESSION_CONTEXT_LOST && snapshots==previous);
    p=(struct pollfd){.fd=123,.events=7,.revents=3};until=55;CHECK(ae_session_poll(s,&p,&until)==-ESTALE && p.fd==123 && p.events==7 && p.revents==3 && until==55);clean(&s);
    s=open_session();model_active=0;CHECK(!ae_session_check(s,&loss) && loss==AE_SESSION_INACTIVE);model_active=1;
    uint64_t event=1;CHECK(write(monitor->fd,&event,sizeof(event))==(ssize_t)sizeof(event));CHECK(!ae_session_check(s,&loss) && loss==AE_SESSION_INACTIVE);clean(&s);
    s=open_session();monitor_deadline=0;previous=flushes;CHECK(!ae_session_check(s,&loss) && !loss && flushes>previous);
    flush_error=1;CHECK(!ae_session_check(s,&loss) && loss==AE_SESSION_OBSERVATION_FAILED);flush_error=0;monitor_deadline=UINT64_MAX;clean(&s);
    s=open_session();monitor_error=1;p=(struct pollfd){.fd=123};until=55;CHECK(ae_session_poll(s,&p,&until)==-ESTALE && p.fd==123 && until==55);monitor_error=0;
    CHECK(!ae_session_check(s,&loss) && loss==AE_SESSION_OBSERVATION_FAILED);clean(&s);
    s=open_session();group_changed=true;CHECK(!ae_session_check(s,&loss) && loss==AE_SESSION_CONTEXT_LOST);group_changed=false;clean(&s);
    s=open_session();uid_changed=true;CHECK(!ae_session_check(s,&loss) && loss==AE_SESSION_CONTEXT_LOST);uid_changed=false;clean(&s);
    s=open_session();current_group="/foreign";CHECK(!ae_session_check(s,&loss) && loss==AE_SESSION_CONTEXT_LOST);current_group="/user.slice/session-original.scope";clean(&s);
    s=open_session();current_seat="seat1";CHECK(!ae_session_check(s,&loss) && loss==AE_SESSION_INACTIVE);current_seat="seat0";clean(&s);
    s=open_session();query_error=1;CHECK(!ae_session_check(s,&loss) && loss==AE_SESSION_OBSERVATION_FAILED);query_error=0;clean(&s);
    CHECK(fd_count()==before);puts("Session: real self pidfd, original metadata/loss latch, native hints and thread/fork boundaries passed");return 0;
}
