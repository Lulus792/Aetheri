#define _GNU_SOURCE
#include "settings/audio_host.h"
#include "audio/session_control.h"
#include <glib-unix.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/eventfd.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

/* Real GLib sources, epoll/poll, timers, threads and descriptor lifetime.
 * Only admitted owner observations/effects are controlled test adapters. */
#define CHECK(x) do {if(!(x)) {fprintf(stderr,"audio host line %d: %s\n",__LINE__,#x);exit(1);}} while(0)
struct ae_audio_session_control {int unused;};
static struct ae_audio_session_snapshot snapshot;
static int hint=-1;
static uint64_t hint_deadline=UINT64_MAX;
static unsigned effects,callbacks;
static struct ae_settings_audio_host *host;
static GThread *original_thread;
static bool inject_poll_idle;
static gboolean idle(gpointer data);
bool ae_audio_session_control_snapshot(struct ae_audio_session_control *s,struct ae_audio_session_snapshot *out) {CHECK(s);*out=snapshot;return true;}
bool ae_audio_session_control_poll(struct ae_audio_session_control *s,struct pollfd out[4],size_t *count,uint64_t *deadline) {
    CHECK(s);
    if(inject_poll_idle) {inject_poll_idle=false;GSource *source=g_idle_source_new();g_source_set_callback(source,idle,NULL,NULL);CHECK(g_source_attach(source,g_main_context_get_thread_default()));g_source_unref(source);}
    *count=snapshot.loss?0:1;out[0]=(struct pollfd){hint,POLLIN,0};*deadline=hint_deadline;return true;
}
static enum ae_audio_submit effect(void) {++effects;return AE_AUDIO_UNAVAILABLE;}
enum ae_audio_submit ae_audio_session_control_volume(struct ae_audio_session_control *s,uint64_t rev,struct ae_audio_output_ref ref,double value) {(void)s;(void)rev;(void)ref;(void)value;return effect();}
enum ae_audio_submit ae_audio_session_control_mute(struct ae_audio_session_control *s,uint64_t rev,struct ae_audio_output_ref ref,bool value) {(void)s;(void)rev;(void)ref;(void)value;return effect();}
enum ae_audio_submit ae_audio_session_control_default(struct ae_audio_session_control *s,uint64_t rev,struct ae_audio_output_ref ref) {(void)s;(void)rev;(void)ref;return effect();}
static gboolean idle(gpointer data) {
    (void)data;CHECK(g_thread_self()==original_thread);++callbacks;
    struct ae_settings_audio_host *borrowed=host;
    CHECK(!ae_settings_audio_host_close(&borrowed) && borrowed==host);
    ++snapshot.audio.revision;return G_SOURCE_REMOVE;
}
static gboolean input(gint fd,GIOCondition condition,gpointer data) {
    (void)data;CHECK(condition&G_IO_IN);uint64_t value;CHECK(read(fd,&value,sizeof(value))==(ssize_t)sizeof(value));return idle(NULL);
}
static void source(GMainContext *context,GSource *s) {CHECK(g_source_attach(s,context));g_source_unref(s);}
static gpointer publish(gpointer data) {GMainContext *context=data;g_main_context_invoke_full(context,G_PRIORITY_DEFAULT,idle,NULL,NULL);return NULL;}
static gpointer foreign(gpointer data) {
    (void)data;uint32_t timeout=0;struct ae_settings_audio_host *copy=host;
    CHECK(ae_settings_audio_host_prepare(host,0,&timeout)<0 && !ae_settings_audio_host_close(&copy) && copy==host);return NULL;
}
static bool poll_one(int fd,int timeout) {
    struct pollfd p={fd,POLLIN,0};int result=poll(&p,1,timeout);CHECK(result>=0 && !(p.revents&(POLLERR|POLLNVAL)));return result>0;
}
static void send_hint(int fd) {uint64_t value=1;CHECK(write(fd,&value,sizeof(value))==(ssize_t)sizeof(value));}
static void quiet(int signal_fd) {
    /* Source attach/removal may also leave GLib's own wakeup FD readable. */
    for(unsigned i=0;i<8;++i) {
        uint32_t timeout=1000;int fd=ae_settings_audio_host_prepare(host,signal_fd,&timeout);CHECK(fd>=0);
        bool ready=poll_one(fd,0);bool dirty=false;CHECK(ae_settings_audio_host_finish(host,&dirty));
        if(!ready) return;
    }
    CHECK(false);
}
int main(void) {
    GMainContext *context=g_main_context_new();g_main_context_push_thread_default(context);original_thread=g_thread_self();
    struct ae_audio_session_control owner={0};struct ae_settings model;ae_settings_init(&model);
    snapshot.audio=(struct ae_audio_snapshot){.state=AE_AUDIO_READY,.revision=1,.count=1};
    snapshot.audio.outputs[0]=(struct ae_audio_output){.ref={17,3,4},.name="output",.volume_available=true,.volume=0.5};
    hint=eventfd(0,EFD_CLOEXEC|EFD_NONBLOCK);int signal_fd=eventfd(0,EFD_CLOEXEC|EFD_NONBLOCK);CHECK(hint>=0 && signal_fd>=0);
    host=ae_settings_audio_host_open(context,&owner);CHECK(host && ae_settings_audio_host_bind(host,&model));
    bool dirty=false;CHECK(ae_settings_audio_host_update(host,&dirty) && dirty && model.sound.attached);
    quiet(signal_fd);g_thread_join(g_thread_new("foreign-owner",foreign,NULL));
    pid_t child=fork();CHECK(child>=0);
    if(!child) {struct ae_settings_audio_host *copy=host;CHECK(!ae_settings_audio_host_close(&copy));_exit(0);}
    int status;CHECK(waitpid(child,&status,0)==child && WIFEXITED(status) && !WEXITSTATUS(status));
    /* Idle readiness shortens the native wait and runs only on the owner. */
    GSource *pending=g_idle_source_new();g_source_set_callback(pending,idle,NULL,NULL);source(context,pending);
    uint32_t timeout=1000;int fd=ae_settings_audio_host_prepare(host,signal_fd,&timeout);CHECK(fd>=0 && timeout==0);
    CHECK(ae_settings_audio_host_prepare(host,signal_fd,&timeout)==-1 && errno==EINVAL); /* No nested prepare. */
    dirty=false;CHECK(ae_settings_audio_host_finish(host,&dirty) && dirty && callbacks==1);
    CHECK(fcntl(fd,F_GETFD)<0 && errno==EBADF);
    quiet(signal_fd);
    /* Actual GLib I/O readiness, not a repeated UI polling interval. */
    int io=eventfd(0,EFD_CLOEXEC|EFD_NONBLOCK);CHECK(io>=0);
    GSource *readable=g_unix_fd_source_new(io,G_IO_IN);g_source_set_callback(readable,G_SOURCE_FUNC(input),NULL,NULL);source(context,readable);
    quiet(signal_fd);timeout=1000;fd=ae_settings_audio_host_prepare(host,signal_fd,&timeout);CHECK(fd>=0);
    send_hint(io);CHECK(poll_one(fd,1000));dirty=false;CHECK(ae_settings_audio_host_finish(host,&dirty) && callbacks==2 && dirty);close(io);
    quiet(signal_fd);
    /* A producer can wake the acquired context from another thread; its
     * callback must not run there or mutate the model before finish. */
    timeout=1000;fd=ae_settings_audio_host_prepare(host,signal_fd,&timeout);CHECK(fd>=0);
    GThread *writer=g_thread_new("publisher",publish,context);CHECK(poll_one(fd,1000));g_thread_join(writer);CHECK(callbacks==2);
    dirty=false;CHECK(ae_settings_audio_host_finish(host,&dirty) && callbacks==3 && dirty);
    quiet(signal_fd);
    GSource *timer=g_timeout_source_new(0);g_source_set_callback(timer,idle,NULL,NULL);source(context,timer);
    timeout=1000;fd=ae_settings_audio_host_prepare(host,signal_fd,&timeout);CHECK(fd>=0 && timeout==0);
    dirty=false;CHECK(ae_settings_audio_host_finish(host,&dirty) && callbacks==4);
    quiet(signal_fd);
    timeout=1000;fd=ae_settings_audio_host_prepare(host,signal_fd,&timeout);CHECK(fd>=0);send_hint(signal_fd);CHECK(poll_one(fd,1000));
    dirty=false;CHECK(ae_settings_audio_host_finish(host,&dirty) && callbacks==4);uint64_t value;CHECK(read(signal_fd,&value,sizeof(value))==(ssize_t)sizeof(value));
    quiet(signal_fd);
    inject_poll_idle=true;timeout=1000;fd=ae_settings_audio_host_prepare(host,signal_fd,&timeout);CHECK(fd>=0 && timeout==0);
    dirty=false;CHECK(ae_settings_audio_host_finish(host,&dirty) && callbacks==5);quiet(signal_fd);
    /* Aborting a wait cannot submit a queued UI request. */
    CHECK(ae_settings_sound_request(&model,model.sound.ids[0],AE_AUDIO_DEFAULT,0,false));
    timeout=1000;fd=ae_settings_audio_host_prepare(host,signal_fd,&timeout);CHECK(fd>=0);
    CHECK(ae_settings_audio_host_abort(host) && model.sound.queued && !effects);
    dirty=false;CHECK(ae_settings_audio_host_update(host,&dirty) && effects==1 && !model.sound.queued);
    quiet(signal_fd);
    /* A dead/session hint is reobserved once; then its permanently readable FD
     * must disappear from future wait sets without being consumed or closed. */
    timeout=1000;fd=ae_settings_audio_host_prepare(host,signal_fd,&timeout);CHECK(fd>=0);snapshot.loss=AE_AUDIO_CONTROL_SESSION_LOST;send_hint(hint);CHECK(poll_one(fd,1000));
    dirty=false;CHECK(ae_settings_audio_host_finish(host,&dirty) && dirty && model.sound.fenced && effects==1);
    quiet(signal_fd);CHECK(read(hint,&value,sizeof(value))==(ssize_t)sizeof(value));
    CHECK(ae_settings_audio_host_bind(host,NULL) && ae_settings_audio_host_close(&host) && !host);
    close(hint);close(signal_fd);g_main_context_pop_thread_default(context);g_main_context_unref(context);
    puts("audio host: native wake multiplexing, GLib I/O/timers/threads, abort and original loss passed");return 0;
}
