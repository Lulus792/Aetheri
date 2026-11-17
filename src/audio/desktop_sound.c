/* The authenticated desktop owner starts one mediator for its Settings child. */
#define _GNU_SOURCE
#include "audio/control.h"
#include "audio/control_wire.h"
#include "linux/session.h"
#include <errno.h>
#include <fcntl.h>
#include <glib-unix.h>
#include <signal.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/pidfd.h>
#include <unistd.h>

enum { CHANNEL=3, GRAPH=4, POLICY=5, SETTINGS=6 };
struct sound {
    GMainLoop *loop;
    struct ae_session *session;
    struct ae_audio_control *control;
    struct ae_audio_wire_requests requests;
    pid_t launcher;
    uint64_t cgroup;
    bool destroyed;
};

static bool observe_child(int fd,pid_t launcher,uint64_t *cgroup,uint32_t *pid)
{
    const uint64_t required=PIDFD_INFO_PID|PIDFD_INFO_CREDS|PIDFD_INFO_CGROUPID;
    struct pidfd_info info={.mask=required};
    if(ioctl(fd,PIDFD_GET_INFO,&info) || (info.mask&required)!=required ||
       !info.pid || info.pid!=info.tgid || info.ppid!=(uint32_t)launcher || !info.cgroupid ||
       info.ruid!=getuid() || info.euid!=getuid() || info.suid!=getuid() || info.fsuid!=getuid() ||
       info.rgid!=getgid() || info.egid!=getgid() || info.sgid!=getgid() || info.fsgid!=getgid())
        return false;
    *cgroup=info.cgroupid;
    *pid=info.pid;
    return true;
}

static bool current(struct sound *sound)
{
    unsigned loss;
    if(getppid()!=sound->launcher || ae_session_check(sound->session,&loss)<0 || loss) return false;
    struct pollfd pins[]={{GRAPH,POLLIN,0},{POLICY,POLLIN,0},{SETTINGS,POLLIN,0}};
    int ready;
    do { ready=poll(pins,G_N_ELEMENTS(pins),0); } while(ready<0 && errno==EINTR);
    if(ready) return false;
    for(int fd=GRAPH;fd<=SETTINGS;++fd) {
        uint64_t cgroup;
        uint32_t pid;
        if(!observe_child(fd,sound->launcher,&cgroup,&pid) || cgroup!=sound->cgroup) return false;
    }
    return true;
}

static enum ae_audio_submit submit(void *data,const struct ae_audio_command *command)
{
    struct sound *sound=data;
    if(!current(sound)) {
        g_main_loop_quit(sound->loop);
        return AE_AUDIO_UNAVAILABLE;
    }
    switch(command->kind) {
    case AE_AUDIO_VOLUME:
        return ae_audio_control_volume(sound->control,command->revision,command->target,command->volume);
    case AE_AUDIO_MUTE:
        return ae_audio_control_mute(sound->control,command->revision,command->target,command->mute);
    case AE_AUDIO_DEFAULT:
        return ae_audio_control_default(sound->control,command->revision,command->target);
    }
    return AE_AUDIO_INVALID;
}

static gboolean channel_ready(gint fd,GIOCondition condition,gpointer data)
{
    struct sound *sound=data;
    if((condition&(G_IO_ERR|G_IO_HUP|G_IO_NVAL)) || !current(sound) ||
       ae_audio_wire_dispatch(fd,&sound->requests,submit,sound)<0) {
        g_main_loop_quit(sound->loop);
        return G_SOURCE_REMOVE;
    }
    return G_SOURCE_CONTINUE;
}

static gboolean stop(gpointer data)
{
    struct sound *sound=data;
    g_main_loop_quit(sound->loop);
    return G_SOURCE_REMOVE;
}

static gboolean pin_lost(gint fd,GIOCondition condition,gpointer data)
{
    (void)fd;
    (void)condition;
    return stop(data);
}

static gboolean session_event(gint fd,GIOCondition condition,gpointer data)
{
    (void)fd;
    return !(condition&(G_IO_ERR|G_IO_HUP|G_IO_NVAL)) && current(data)
        ? G_SOURCE_CONTINUE : stop(data);
}

static void changed(const struct ae_audio_snapshot *snapshot,void *data)
{
    struct sound *sound=data;
    // The adapter invokes this on an idle source, after command dispatch sends its result.
    struct ae_audio_message message={.kind=AE_AUDIO_MESSAGE_SNAPSHOT,.snapshot.audio=*snapshot};
    if(!current(sound) || ae_audio_wire_send(CHANNEL,&message) ||
       snapshot->state==AE_AUDIO_DISCONNECTED || snapshot->state==AE_AUDIO_FAILED)
        (void)stop(sound);
}

static void destroyed(gpointer data)
{
    struct sound *sound=data;
    sound->destroyed=true;
    g_main_loop_quit(sound->loop);
}

static GSource *attach_fd(GMainContext *context,int fd,GIOCondition events,
                         GUnixFDSourceFunc callback,struct sound *sound)
{
    GSource *source=g_unix_fd_source_new(fd,events|G_IO_ERR|G_IO_HUP|G_IO_NVAL);
    g_source_set_callback(source,G_SOURCE_FUNC(callback),sound,NULL);
    g_source_attach(source,context);
    return source;
}

int main(int argc,char **argv)
{
    struct sound sound={.launcher=getppid()};
    if(argc!=2 || !getuid() || !ae_audio_wire_admit_parent(CHANNEL)) return 2;
    uint32_t original[3];
    for(int fd=GRAPH;fd<=SETTINGS;++fd) {
        uint64_t cgroup;
        if(fcntl(fd,F_SETFD,FD_CLOEXEC) ||
           !observe_child(fd,sound.launcher,&cgroup,&original[fd-GRAPH]) ||
           (sound.cgroup && cgroup!=sound.cgroup)) return 2;
        sound.cgroup=cgroup;
    }
    if(original[0]==original[1] || original[0]==original[2] || original[1]==original[2]) return 2;
    struct pollfd monitor;
    uint64_t deadline;
    if(ae_session_open(getuid(),"seat0",&sound.session)<0 || !current(&sound) ||
       ae_session_poll(sound.session,&monitor,&deadline)<0 || deadline!=UINT64_MAX) {
        (void)ae_session_close(&sound.session);
        fputs("AETHERI_SOUND_UNAVAILABLE session\n",stderr);
        return 1;
    }

    GMainContext *context=g_main_context_new();
    sound.loop=g_main_loop_new(context,FALSE);
    g_main_context_push_thread_default(context);
    GSource *term=g_unix_signal_source_new(SIGTERM);
    g_source_set_callback(term,stop,&sound,NULL);
    g_source_attach(term,context);
    GSource *watches[]={
        term,
        attach_fd(context,CHANNEL,G_IO_IN,channel_ready,&sound),
        attach_fd(context,GRAPH,G_IO_IN,pin_lost,&sound),
        attach_fd(context,POLICY,G_IO_IN,pin_lost,&sound),
        attach_fd(context,SETTINGS,G_IO_IN,pin_lost,&sound),
        attach_fd(context,monitor.fd,(GIOCondition)monitor.events,session_event,&sound)
    };
    /* Commands must not starve the graph adapter's own readback idle source. */
    g_source_set_priority(watches[1],G_PRIORITY_DEFAULT_IDLE+1);
    sound.control=ae_audio_control_open_original(context,argv[1],GRAPH,changed,&sound,destroyed);
    int result=sound.control?0:1;
    if(sound.control) g_main_loop_run(sound.loop);
    close(CHANNEL);
    for(size_t i=0;i<G_N_ELEMENTS(watches);++i) {
        g_source_destroy(watches[i]);
        g_source_unref(watches[i]);
    }
    (void)ae_session_close(&sound.session);
    if(sound.control) (void)ae_audio_control_close(&sound.control);
    if(!result && !sound.destroyed) {
        GSource *timer=g_timeout_source_new(7000);
        g_source_set_callback(timer,stop,&sound,NULL);
        g_source_attach(timer,context);
        g_main_loop_run(sound.loop);
        g_source_destroy(timer);
        g_source_unref(timer);
        if(!sound.destroyed) return 1; /* Process teardown owns remaining local resources. */
    }
    g_main_context_pop_thread_default(context);
    g_main_loop_unref(sound.loop);
    g_main_context_unref(context);
    return result;
}
