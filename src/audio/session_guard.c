#define _GNU_SOURCE
#include "session_guard.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <systemd/sd-login.h>
#include <time.h>
#include <unistd.h>

struct ae_audio_session_guard {
    sd_login_monitor *monitor;
    char *session,*seat;
    uid_t uid;
    int pins[2];
    unsigned loss;
};
static bool session_matches(struct ae_audio_session_guard *g) {
    char *current=NULL,*seat=NULL,*type=NULL,*class=NULL;uid_t uid=0;
    bool okay=sd_pid_get_session(0,&current)>=0 && !strcmp(current,g->session) &&
        sd_session_get_uid(g->session,&uid)>=0 && uid==g->uid &&
        sd_session_is_active(g->session)>0 && sd_session_is_remote(g->session)==0 &&
        sd_session_get_seat(g->session,&seat)>=0 && !strcmp(seat,g->seat) &&
        sd_session_get_type(g->session,&type)>=0 && !strcmp(type,"wayland") &&
        sd_session_get_class(g->session,&class)>=0 && !strcmp(class,"user");
    free(current);free(seat);free(type);free(class);return okay;
}
static bool now_us(uint64_t *out) {
    struct timespec t;
    if(clock_gettime(CLOCK_MONOTONIC,&t) || t.tv_sec<0 || (uint64_t)t.tv_sec>UINT64_MAX/1000000u-1u) return false;
    *out=(uint64_t)t.tv_sec*1000000u+(uint64_t)t.tv_nsec/1000u;return *out<=UINT64_MAX-1000000u;
}
void ae_audio_session_guard_close(struct ae_audio_session_guard **pointer) {
    if(!pointer || !*pointer) return;
    struct ae_audio_session_guard *g=*pointer;*pointer=NULL;
    sd_login_monitor_unref(g->monitor);
    for(unsigned i=0;i<2;++i) if(g->pins[i]>=0) close(g->pins[i]);
    free(g->session);free(g->seat);free(g);
}
unsigned ae_audio_session_guard_wait(struct ae_audio_session_guard *g,int timeout_ms) {
    if(!g) return AE_AUDIO_SESSION_OBSERVATION_FAILED;
    if(g->loss) return g->loss;
    if(timeout_ms<0 || timeout_ms>1000) return g->loss=AE_AUDIO_SESSION_OBSERVATION_FAILED;
    int fd=sd_login_monitor_get_fd(g->monitor),events=sd_login_monitor_get_events(g->monitor);
    uint64_t monitor_deadline=0,now=0;
    if(fd<0 || events<0 || events>SHRT_MAX || sd_login_monitor_get_timeout(g->monitor,&monitor_deadline)<0 || !now_us(&now)) return g->loss=AE_AUDIO_SESSION_OBSERVATION_FAILED;
    uint64_t deadline=now+(uint64_t)timeout_ms*1000u;
    if(monitor_deadline<deadline) deadline=monitor_deadline;
    for(;;) {
        int ms=deadline>now ? (int)((deadline-now+999u)/1000u) : 0;
        struct pollfd p[3]={{fd,(short)events,0},{g->pins[0],POLLIN,0},{g->pins[1],POLLIN,0}};
        int r=poll(p,3,ms);
        if(r<0 && errno==EINTR) {if(!now_us(&now)) return g->loss=AE_AUDIO_SESSION_OBSERVATION_FAILED;continue;}
        if(r<0 || (p[0].revents&(POLLERR|POLLHUP|POLLNVAL)) || (p[1].revents&(POLLERR|POLLNVAL)) || (p[2].revents&(POLLERR|POLLNVAL))) return g->loss=AE_AUDIO_SESSION_OBSERVATION_FAILED;
        unsigned loss=0;
        if(p[1].revents&(POLLIN|POLLHUP)) loss|=AE_AUDIO_SESSION_SERVER_EXIT;
        if(p[2].revents&(POLLIN|POLLHUP)) loss|=AE_AUDIO_SESSION_POLICY_EXIT;
        if(!now_us(&now)) return g->loss=AE_AUDIO_SESSION_OBSERVATION_FAILED;
        if(p[0].revents || now>=monitor_deadline) {
            if(sd_login_monitor_flush(g->monitor)<0) loss|=AE_AUDIO_SESSION_OBSERVATION_FAILED;
            else if(!session_matches(g)) loss|=AE_AUDIO_SESSION_INACTIVE;
        }
        return g->loss=loss;
    }
}
struct ae_audio_session_guard *ae_audio_session_guard_open(uid_t uid,const char *seat,int server,int policy) {
    if(uid!=getuid() || uid!=geteuid() || !seat || !*seat || strlen(seat)>128 || server<0 || policy<0 || server==policy) return NULL;
    struct ae_audio_session_guard *g=calloc(1,sizeof(*g));if(!g) return NULL;
    g->pins[0]=g->pins[1]=-1;g->uid=uid;g->seat=strdup(seat);
    /* Subscribe before the initial snapshot, retaining changes during setup. */
    if(!g->seat || sd_login_monitor_new("session",&g->monitor)<0 || sd_pid_get_session(0,&g->session)<0 || !session_matches(g)) goto failed;
    g->pins[0]=fcntl(server,F_DUPFD_CLOEXEC,3);g->pins[1]=fcntl(policy,F_DUPFD_CLOEXEC,3);
    if(g->pins[0]<0 || g->pins[1]<0 || ae_audio_session_guard_wait(g,0) || !session_matches(g)) goto failed;
    return g;
failed:
    ae_audio_session_guard_close(&g);return NULL;
}
