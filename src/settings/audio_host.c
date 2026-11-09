#define _POSIX_C_SOURCE 200809L
#include "settings/audio_host.h"
#include "settings/audio.h"
#include "audio/session_control.h"
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <time.h>
#include <unistd.h>

#define POLL_MAX 256
struct ae_settings_audio_host {
    GMainContext *context;
    GThread *thread;
    pid_t process;
    struct ae_audio_session_control *owner;
    struct ae_settings *model;
    GPollFD fds[POLL_MAX];
    int count,priority,multiplexer;
    bool waiting,dispatching;
};
static bool owner(const struct ae_settings_audio_host *h) {
    return h && h->process==getpid() && h->thread==g_thread_self() &&
        g_main_context_get_thread_default()==h->context;
}
struct ae_settings_audio_host *ae_settings_audio_host_open(GMainContext *context,struct ae_audio_session_control *audio) {
    if(!context || !audio || g_main_context_get_thread_default()!=context) {errno=EINVAL;return NULL;}
    struct ae_settings_audio_host *h=calloc(1,sizeof(*h));if(!h) return NULL;
    h->context=g_main_context_ref(context);h->thread=g_thread_self();h->process=getpid();h->owner=audio;h->multiplexer=-1;return h;
}
bool ae_settings_audio_host_bind(struct ae_settings_audio_host *h,struct ae_settings *m) {
    if(!owner(h) || h->waiting || h->dispatching || (m && h->model)) return false;
    h->model=m;return true;
}
bool ae_settings_audio_host_update(struct ae_settings_audio_host *h,bool *dirty) {
    if(!owner(h) || !h->model || !dirty || h->waiting || h->dispatching) return false;
    struct ae_settings_sound *s=&h->model->sound;
    uint64_t revision=s->observation.revision;enum ae_audio_state state=s->observation.state;
    enum ae_settings_sound_feedback feedback=s->feedback;bool stale=s->stale,fenced=s->fenced,attached=s->attached;size_t count=s->observation.count;
    (void)ae_settings_audio_pump(h->model,h->owner);
    if(revision!=s->observation.revision || state!=s->observation.state || feedback!=s->feedback || stale!=s->stale || fenced!=s->fenced || attached!=s->attached || count!=s->observation.count) *dirty=true;
    return true;
}
static uint32_t events(short value) {
    uint32_t result=0;
    if(value&POLLIN) result|=EPOLLIN;
    if(value&POLLOUT) result|=EPOLLOUT;
    if(value&POLLPRI) result|=EPOLLPRI;
    return result;
}
static bool add(int epoll_fd,int fd,uint32_t mask,bool required,uint32_t *timeout) {
    if(fd<0) return !required;
    struct epoll_event e={.events=mask,.data={.fd=fd}};
    if(epoll_ctl(epoll_fd,EPOLL_CTL_ADD,fd,&e)==0) return true;
    if(errno==EEXIST) return epoll_ctl(epoll_fd,EPOLL_CTL_MOD,fd,&e)==0;
    /* A regular file or concurrently retired source is handled by the actual
     * nonblocking GLib poll below; epoll is a wake hint, never FD identity. */
    if(!required && (errno==EPERM || errno==EBADF || errno==ENOENT)) {*timeout=0;return true;}
    return false;
}
static bool deadline_timeout(uint64_t deadline,uint32_t *timeout) {
    if(deadline==UINT64_MAX) return true;
    struct timespec now;
    if(clock_gettime(CLOCK_MONOTONIC,&now) || now.tv_sec<0 || (uint64_t)now.tv_sec>UINT64_MAX/1000000u-1) return false;
    uint64_t current=(uint64_t)now.tv_sec*1000000u+(uint64_t)now.tv_nsec/1000u;
    uint64_t diff=deadline>current?deadline-current:0;
    uint64_t ms=diff/1000+(diff%1000!=0);
    if(ms>=UINT32_MAX) ms=UINT32_MAX-1;
    if(ms<*timeout) *timeout=(uint32_t)ms;
    return true;
}
int ae_settings_audio_host_prepare(struct ae_settings_audio_host *h,int signal_fd,uint32_t *timeout) {
    if(!owner(h) || !h->model || !timeout || signal_fd<0 || h->waiting || h->dispatching) {errno=EINVAL;return -1;}
    if(!g_main_context_acquire(h->context)) {errno=EBUSY;return -1;}
    h->waiting=true;
    /* Collect first: an observation failure can fence/retire the backend and
     * attach close sources. Include those changes in this prepare/query. */
    struct pollfd original[4];size_t count=0;uint64_t deadline;
    if(!ae_audio_session_control_poll(h->owner,original,&count,&deadline) || count>4) goto failed;
    bool ready=g_main_context_prepare(h->context,&h->priority)!=FALSE;
    int delay=-1;h->count=g_main_context_query(h->context,h->priority,&delay,h->fds,POLL_MAX);
    if(h->count<0 || h->count>POLL_MAX) {errno=EOVERFLOW;goto failed;}
    if(ready) *timeout=0;
    else if(delay>=0 && (uint32_t)delay<*timeout) *timeout=(uint32_t)delay;
    if(!deadline_timeout(deadline,timeout)) goto failed;
    h->multiplexer=epoll_create1(EPOLL_CLOEXEC);if(h->multiplexer<0) goto failed;
    if(!add(h->multiplexer,signal_fd,EPOLLIN,true,timeout)) goto failed;
    for(int i=0;i<h->count;++i) {
        uint32_t mask=h->fds[i].fd==signal_fd?EPOLLIN:0;
        for(int j=0;j<h->count;++j) if(h->fds[j].fd==h->fds[i].fd) mask|=events((short)h->fds[j].events);
        for(size_t j=0;j<count;++j) if(original[j].fd==h->fds[i].fd) mask|=events(original[j].events);
        if(!add(h->multiplexer,h->fds[i].fd,mask,false,timeout)) goto failed;
    }
    for(size_t i=0;i<count;++i) {
        uint32_t mask=original[i].fd==signal_fd?EPOLLIN:0;
        for(size_t j=0;j<count;++j) if(original[j].fd==original[i].fd) mask|=events(original[j].events);
        for(int j=0;j<h->count;++j) if(h->fds[j].fd==original[i].fd) mask|=events((short)h->fds[j].events);
        if(!add(h->multiplexer,original[i].fd,mask,true,timeout)) goto failed;
    }
    return h->multiplexer;
failed: {
    int saved=errno;if(h->multiplexer>=0) close(h->multiplexer);h->multiplexer=-1;
    h->waiting=false;g_main_context_release(h->context);errno=saved;return -1;
    }
}
bool ae_settings_audio_host_finish(struct ae_settings_audio_host *h,bool *dirty) {
    if(!owner(h) || !h->waiting || !dirty) return false;
    int result=g_poll(h->fds,(guint)h->count,0);
    bool okay=result>=0 || errno==EINTR;
    if(result<0) for(int i=0;i<h->count;++i) h->fds[i].revents=0;
    h->dispatching=true;
    if(okay && g_main_context_check(h->context,h->priority,h->fds,h->count)) {
        g_main_context_dispatch(h->context);*dirty=true;
    }
    h->dispatching=false;
    close(h->multiplexer);h->multiplexer=-1;h->waiting=false;
    g_main_context_release(h->context);
    return okay && ae_settings_audio_host_update(h,dirty);
}
bool ae_settings_audio_host_abort(struct ae_settings_audio_host *h) {
    if(!owner(h) || !h->waiting || h->dispatching) return false;
    close(h->multiplexer);h->multiplexer=-1;h->waiting=false;g_main_context_release(h->context);return true;
}
bool ae_settings_audio_host_close(struct ae_settings_audio_host **slot) {
    if(!slot || !owner(*slot) || (*slot)->waiting || (*slot)->dispatching) return false;
    struct ae_settings_audio_host *h=*slot;*slot=NULL;g_main_context_unref(h->context);free(h);return true;
}
