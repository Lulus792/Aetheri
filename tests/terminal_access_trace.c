/* Bounded observational wrapper for the original host; no SDK replacement. */
#undef poll
#undef SDL_PushEvent
#undef SDL_WaitEvent
#undef nx_linux_accessibility_host_prepare
#define _POSIX_C_SOURCE 200809L
#include <SDL3/SDL.h>
#include <nexaui/linux_accessibility_host.h>
#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
enum { TRACE_COUNT=512,TRACE_FDS=19 };
struct trace {
    uint64_t order,ns;char kind;int result,error,timeout;uint32_t event;
    nfds_t count;struct pollfd fds[TRACE_FDS];
};
static pthread_mutex_t lock=PTHREAD_MUTEX_INITIALIZER;
static struct trace records[TRACE_COUNT];static uint64_t serial;
static void record(char kind,int result,int error,int timeout,uint32_t event,const struct pollfd *fds,nfds_t count) {
    struct timespec t;int saved=errno;
    if(clock_gettime(CLOCK_MONOTONIC,&t)) abort();
    if(count>TRACE_FDS) abort();
    if(pthread_mutex_lock(&lock)) abort();
    uint64_t order=serial++;struct trace *r=&records[order%TRACE_COUNT];
    *r=(struct trace){.order=order,.ns=(uint64_t)t.tv_sec*UINT64_C(1000000000)+(uint64_t)t.tv_nsec,
        .kind=kind,.result=result,.error=error,.timeout=timeout,.event=event,.count=count};
    for(nfds_t i=0;i<count;++i) r->fds[i]=fds[i];
    if(pthread_mutex_unlock(&lock)) abort();
    errno=saved;
}
static void dump(void) {
    if(pthread_mutex_lock(&lock)) abort();
    uint64_t first=serial>TRACE_COUNT?serial-TRACE_COUNT:0;
    for(uint64_t i=first;i<serial;++i) {
        const struct trace *r=&records[i%TRACE_COUNT];
        fprintf(stderr,"ACCESS_TRACE order=%llu ns=%llu kind=%c result=%d errno=%d timeout=%d event=%u",
            (unsigned long long)r->order,(unsigned long long)r->ns,r->kind,r->result,r->error,r->timeout,r->event);
        for(nfds_t j=0;j<r->count;++j) fprintf(stderr," fd%lu=%d/%x/%x",(unsigned long)j,r->fds[j].fd,
            (unsigned)(unsigned short)r->fds[j].events,(unsigned)(unsigned short)r->fds[j].revents);
        fputc('\n',stderr);
    }
    if(pthread_mutex_unlock(&lock)) abort();
}
__attribute__((constructor)) static void initialize(void) {if(atexit(dump)) abort();}
int ae_test_access_poll(struct pollfd *fds,nfds_t count,int timeout) {
    record('P',0,0,timeout,0,fds,count);int result=poll(fds,count,timeout);int error=errno;
    record('p',result,error,timeout,0,fds,count);errno=error;return result;
}
bool ae_test_access_push(SDL_Event *event) {
    bool result=SDL_PushEvent(event);record('E',result,0,0,event->type,NULL,0);return result;
}
bool ae_test_access_wait(SDL_Event *event) {
    record('W',0,0,0,0,NULL,0);bool result=SDL_WaitEvent(event);record('w',result,0,0,result?event->type:0,NULL,0);return result;
}
nx_result ae_test_access_prepare(nx_linux_accessibility_host *host,int *fd,int *writable,uint32_t *timeout) {
    nx_result result=nx_linux_accessibility_host_prepare(host,fd,writable,timeout);
    if(result!=NX_OK) {record('A',(int)result,0,0,0,NULL,0);return result;}
    struct pollfd f={*fd,(short)(POLLIN|(*writable?POLLOUT:0)),0};
    record('A',(int)result,0,*timeout==UINT32_MAX?-1:(int)*timeout,0,&f,1);return result;
}
