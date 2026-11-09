#define _GNU_SOURCE
#include "audio/session_guard.h"
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/eventfd.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <systemd/sd-login.h>
#include <unistd.h>

/* Controlled logind observations; pidfds, process exit, poll and FD lifetime
 * are real. The separate PAM/VT image supplies actual logind integration. */
struct sd_login_monitor {int fd;};
static struct sd_login_monitor *monitor;
static bool active=true,foreign=false,flush_failure=false;
static uint64_t monitor_timeout=UINT64_MAX;
int __wrap_sd_login_monitor_new(const char *category,sd_login_monitor **out) {
    if(strcmp(category,"session")) return -EINVAL;
    *out=calloc(1,sizeof(**out));if(!*out) return -ENOMEM;
    (*out)->fd=eventfd(0,EFD_CLOEXEC|EFD_NONBLOCK);
    if((*out)->fd<0) {free(*out);*out=NULL;return -errno;}
    monitor=*out;return 0;
}
sd_login_monitor *__wrap_sd_login_monitor_unref(sd_login_monitor *m) {
    if(m) {if(m->fd>=0) close(m->fd);free(m);monitor=NULL;}return NULL;
}
int __wrap_sd_login_monitor_get_fd(sd_login_monitor *m) {return m->fd;}
int __wrap_sd_login_monitor_get_events(sd_login_monitor *m) {(void)m;return POLLIN;}
int __wrap_sd_login_monitor_get_timeout(sd_login_monitor *m,uint64_t *out) {(void)m;*out=monitor_timeout;return 0;}
int __wrap_sd_login_monitor_flush(sd_login_monitor *m) {
    uint64_t value;if(flush_failure) return -EIO;
    ssize_t n=read(m->fd,&value,sizeof(value));return n==(ssize_t)sizeof(value) || (n<0 && errno==EAGAIN) ? 0 : -EIO;
}
int __wrap_sd_pid_get_session(pid_t pid,char **out) {(void)pid;*out=strdup(foreign ? "foreign" : "original");return *out ? 0 : -ENOMEM;}
int __wrap_sd_session_get_uid(const char *id,uid_t *out) {(void)id;*out=getuid();return 0;}
int __wrap_sd_session_is_active(const char *id) {(void)id;return active;}
int __wrap_sd_session_is_remote(const char *id) {(void)id;return 0;}
int __wrap_sd_session_get_seat(const char *id,char **out) {(void)id;*out=strdup("seat0");return *out ? 0 : -ENOMEM;}
int __wrap_sd_session_get_type(const char *id,char **out) {(void)id;*out=strdup("wayland");return *out ? 0 : -ENOMEM;}
int __wrap_sd_session_get_class(const char *id,char **out) {(void)id;*out=strdup("user");return *out ? 0 : -ENOMEM;}

#define CHECK(x) do {if(!(x)) {fprintf(stderr,"session guard failed line %d: %s\n",__LINE__,#x);goto done;}} while(0)
static bool notify(void) {uint64_t value=1;return write(monitor->fd,&value,sizeof(value))==(ssize_t)sizeof(value);}
int main(void) {
    struct ae_audio_session_guard *g=NULL;pid_t children[2]={-1,-1};int pins[2]={-1,-1};bool okay=false;
    for(unsigned i=0;i<2;++i) {
        children[i]=fork();CHECK(children[i]>=0);
        if(!children[i]) {for(;;) pause();}
        pins[i]=(int)syscall(SYS_pidfd_open,children[i],0);CHECK(pins[i]>=0);
    }
    CHECK(!ae_audio_session_guard_open(getuid(),NULL,pins[0],pins[1]));
    CHECK(!ae_audio_session_guard_open(getuid(),"seat0",-1,pins[1]));
    CHECK(!ae_audio_session_guard_open(getuid(),"seat0",pins[0],pins[0]));
    CHECK(!ae_audio_session_guard_open(getuid(),"seat9",pins[0],pins[1]));
    active=false;CHECK(!ae_audio_session_guard_open(getuid(),"seat0",pins[0],pins[1]));active=true;
    g=ae_audio_session_guard_open(getuid(),"seat0",pins[0],pins[1]);CHECK(g);
    int original_monitor=monitor->fd;
    CHECK(ae_audio_session_guard_wait(g,20)==AE_AUDIO_SESSION_LIVE);
    struct pollfd watched[3];uint64_t deadline=0;
    CHECK(ae_audio_session_guard_poll(g,watched,&deadline) && watched[0].fd==original_monitor && deadline==UINT64_MAX);
    CHECK(fcntl(watched[1].fd,F_GETFD)>=0 && fcntl(watched[2].fd,F_GETFD)>=0);
    active=false;CHECK(notify());CHECK(ae_audio_session_guard_wait(g,100)==AE_AUDIO_SESSION_INACTIVE);
    active=true;CHECK(notify());CHECK(ae_audio_session_guard_wait(g,0)==AE_AUDIO_SESSION_INACTIVE);
    CHECK(!ae_audio_session_guard_poll(g,watched,&deadline));
    ae_audio_session_guard_close(&g);CHECK(!g && !monitor && fcntl(original_monitor,F_GETFD)<0 && errno==EBADF);
    ae_audio_session_guard_close(&g);ae_audio_session_guard_close(NULL);
    g=ae_audio_session_guard_open(getuid(),"seat0",pins[0],pins[1]);CHECK(g);
    foreign=true;CHECK(notify());CHECK(ae_audio_session_guard_wait(g,100)==AE_AUDIO_SESSION_INACTIVE);
    ae_audio_session_guard_close(&g);foreign=false;
    g=ae_audio_session_guard_open(getuid(),"seat0",pins[0],pins[1]);CHECK(g);
    flush_failure=true;CHECK(notify());CHECK(ae_audio_session_guard_wait(g,100)==AE_AUDIO_SESSION_OBSERVATION_FAILED);
    ae_audio_session_guard_close(&g);flush_failure=false;
    g=ae_audio_session_guard_open(getuid(),"seat0",pins[0],pins[1]);CHECK(g);
    CHECK(ae_audio_session_guard_wait(g,-1)==AE_AUDIO_SESSION_OBSERVATION_FAILED);
    CHECK(ae_audio_session_guard_wait(g,0)==AE_AUDIO_SESSION_OBSERVATION_FAILED);ae_audio_session_guard_close(&g);
    g=ae_audio_session_guard_open(getuid(),"seat0",pins[0],pins[1]);CHECK(g);
    monitor_timeout=0;active=false;CHECK(ae_audio_session_guard_wait(g,100)==AE_AUDIO_SESSION_INACTIVE);
    ae_audio_session_guard_close(&g);monitor_timeout=UINT64_MAX;active=true;
    g=ae_audio_session_guard_open(getuid(),"seat0",pins[0],pins[1]);CHECK(g);
    CHECK(syscall(SYS_pidfd_send_signal,pins[0],SIGTERM,NULL,0)==0);
    CHECK(ae_audio_session_guard_wait(g,1000)==AE_AUDIO_SESSION_SERVER_EXIT);
    CHECK(ae_audio_session_guard_wait(g,0)==AE_AUDIO_SESSION_SERVER_EXIT);ae_audio_session_guard_close(&g);
    CHECK(!ae_audio_session_guard_open(getuid(),"seat0",pins[0],pins[1]));
    /* A separate guard can retain two admitted copies of one live process for
     * this FD-ownership test; production role separation is the caller's job. */
    int duplicate=fcntl(pins[1],F_DUPFD_CLOEXEC,3);CHECK(duplicate>=0);
    g=ae_audio_session_guard_open(getuid(),"seat0",pins[1],duplicate);close(duplicate);CHECK(g);
    CHECK(syscall(SYS_pidfd_send_signal,pins[1],SIGTERM,NULL,0)==0);
    close(pins[1]);pins[1]=-1;
    CHECK(ae_audio_session_guard_wait(g,1000)==(AE_AUDIO_SESSION_SERVER_EXIT|AE_AUDIO_SESSION_POLICY_EXIT));
    CHECK(ae_audio_session_guard_wait(NULL,0)==AE_AUDIO_SESSION_OBSERVATION_FAILED);
    okay=true;
done:
    ae_audio_session_guard_close(&g);
    for(unsigned i=0;i<2;++i) if(children[i]>0) {
        if(pins[i]>=0) {(void)syscall(SYS_pidfd_send_signal,pins[i],SIGKILL,NULL,0);close(pins[i]);}
        else (void)kill(children[i],SIGKILL); /* Our still-unreaped direct child. */
        int status;while(waitpid(children[i],&status,0)<0 && errno==EINTR) {}
    }
    if(okay) puts("audio session guard: controlled context, real pidfds and latched losses passed");
    return okay ? 0 : 1;
}
