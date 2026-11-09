#define _POSIX_C_SOURCE 200809L
#include "audio/session_control.h"
#include "audio/actor.h"
#include "audio/session_guard.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct ae_audio_session_control {
    unsigned refs;
    pid_t process;
    GThread *thread;
    struct ae_audio_manager *manager;
    struct ae_audio_actor *graph,*policy;
    struct ae_audio_session_guard *guard;
    struct ae_audio_control *control;
    struct ae_audio_session_snapshot snapshot;
    void *data;
    GDestroyNotify destroy;
};
static bool owner(const struct ae_audio_session_control *s) {
    return s && s->process==getpid() && s->thread==g_thread_self();
}
static void release(struct ae_audio_session_control *s) {
    if(--s->refs) return;
    ae_audio_session_guard_close(&s->guard);
    ae_audio_actor_close(&s->policy);ae_audio_actor_close(&s->graph);
    ae_audio_manager_close(&s->manager);
    if(s->destroy) s->destroy(s->data);
    free(s);
}
static void retired(void *data) {release(data);}
static void changed(const struct ae_audio_snapshot *snapshot,void *data) {
    struct ae_audio_session_control *s=data;
    if(!s->snapshot.loss) s->snapshot.audio=*snapshot;
}
static void fence(struct ae_audio_session_control *s,unsigned loss) {
    if(s->snapshot.loss) return;
    s->snapshot.loss=loss;
    s->snapshot.audio.state=AE_AUDIO_DISCONNECTED;s->snapshot.audio.count=0;
    memset(s->snapshot.audio.outputs,0,sizeof(s->snapshot.audio.outputs));
    if(s->snapshot.audio.command_state==AE_AUDIO_COMMAND_PENDING) s->snapshot.audio.command_state=AE_AUDIO_COMMAND_UNKNOWN;
    (void)ae_audio_control_close(&s->control);
}
static unsigned check(struct ae_audio_session_control *s) {
    if(s->snapshot.loss) return s->snapshot.loss;
    unsigned loss=0;
    if(ae_audio_manager_check(s->manager)<0) loss|=AE_AUDIO_CONTROL_MANAGER_LOST;
    if(ae_audio_actor_check(s->graph)<0) loss|=AE_AUDIO_CONTROL_GRAPH_LOST;
    if(ae_audio_actor_check(s->policy)<0) loss|=AE_AUDIO_CONTROL_POLICY_LOST;
    if(ae_audio_session_guard_wait(s->guard,0)!=AE_AUDIO_SESSION_LIVE) loss|=AE_AUDIO_CONTROL_SESSION_LOST;
    struct ae_audio_snapshot current;
    if(!ae_audio_control_snapshot(s->control,&current)) loss|=AE_AUDIO_CONTROL_BACKEND_LOST;
    else {
        s->snapshot.audio=current;
        if(current.state==AE_AUDIO_FAILED || current.state==AE_AUDIO_DISCONNECTED)
            loss|=AE_AUDIO_CONTROL_BACKEND_LOST;
    }
    if(loss) fence(s,loss);
    return loss;
}
struct ae_audio_session_control *ae_audio_session_control_open(GMainContext *context,
        const struct ae_audio_session_profile *p,void *data,GDestroyNotify destroy) {
    if(!context || g_main_context_get_thread_default()!=context || !p ||
            !p->graph_unit || !p->policy_unit || !strcmp(p->graph_unit,p->policy_unit) ||
            !p->remote || p->remote[0]!='/' || !p->seat || !p->graph_context || !p->policy_context) {
        errno=EINVAL;return NULL;
    }
    struct ae_audio_session_control *s=calloc(1,sizeof(*s));
    if(!s) return NULL;
    s->refs=1;s->process=getpid();s->thread=g_thread_self();
    s->manager=ae_audio_manager_open(p->user_bus,p->system_bus,p->uid,p->manager_image);
    if(!s->manager) goto fail;
    s->graph=ae_audio_actor_open(s->manager,p->graph_unit,p->graph_pidfd,p->graph_image,p->graph_context);
    if(!s->graph) goto fail;
    s->policy=ae_audio_actor_open(s->manager,p->policy_unit,p->policy_pidfd,p->policy_image,p->policy_context);
    if(!s->policy) goto fail;
    s->guard=ae_audio_session_guard_open(p->uid,p->seat,p->graph_pidfd,p->policy_pidfd);
    if(!s->guard) goto fail;
    /* Recheck after observer setup and immediately before protocol connection.
     * Remaining races require the caller's admitted execution/delegation fence. */
    if(ae_audio_manager_check(s->manager)<0 || ae_audio_actor_check(s->graph)<0 ||
            ae_audio_actor_check(s->policy)<0 || ae_audio_session_guard_wait(s->guard,0)) {
        errno=ESTALE;goto fail;
    }
    ++s->refs;
    s->control=ae_audio_control_open_original(context,p->remote,p->graph_pidfd,changed,s,retired);
    if(!s->control) {--s->refs;goto fail;}
    s->data=data;s->destroy=destroy;
    return s;
fail: {
    int saved=errno;release(s);errno=saved;return NULL;
    }
}
bool ae_audio_session_control_snapshot(struct ae_audio_session_control *s,struct ae_audio_session_snapshot *out) {
    if(!owner(s) || !out) return false;
    (void)check(s);*out=s->snapshot;return true;
}
bool ae_audio_session_control_poll(struct ae_audio_session_control *s,struct pollfd out[4],size_t *count,uint64_t *deadline_us) {
    if(!owner(s) || !out || !count || !deadline_us) return false;
    if(s->snapshot.loss) {*count=0;*deadline_us=UINT64_MAX;return true;}
    struct pollfd fds[4];uint64_t deadline;
    if(!ae_audio_session_guard_poll(s->guard,fds,&deadline)) fence(s,AE_AUDIO_CONTROL_SESSION_LOST);
    else {
        int fd=ae_audio_manager_poll_fd(s->manager);
        if(fd<0) fence(s,AE_AUDIO_CONTROL_MANAGER_LOST);
        else {fds[3]=(struct pollfd){fd,POLLIN,0};memcpy(out,fds,sizeof(fds));*count=4;*deadline_us=deadline;return true;}
    }
    *count=0;*deadline_us=UINT64_MAX;return true;
}
enum ae_audio_submit ae_audio_session_control_volume(struct ae_audio_session_control *s,
        uint64_t revision,struct ae_audio_output_ref ref,double value) {
    if(!owner(s)) return AE_AUDIO_INVALID;
    if(check(s)) return AE_AUDIO_UNAVAILABLE;
    return ae_audio_control_volume(s->control,revision,ref,value);
}
enum ae_audio_submit ae_audio_session_control_mute(struct ae_audio_session_control *s,
        uint64_t revision,struct ae_audio_output_ref ref,bool value) {
    if(!owner(s)) return AE_AUDIO_INVALID;
    if(check(s)) return AE_AUDIO_UNAVAILABLE;
    return ae_audio_control_mute(s->control,revision,ref,value);
}
enum ae_audio_submit ae_audio_session_control_default(struct ae_audio_session_control *s,
        uint64_t revision,struct ae_audio_output_ref ref) {
    if(!owner(s)) return AE_AUDIO_INVALID;
    if(check(s)) return AE_AUDIO_UNAVAILABLE;
    return ae_audio_control_default(s->control,revision,ref);
}
bool ae_audio_session_control_close(struct ae_audio_session_control **slot) {
    if(!slot || !owner(*slot)) return false;
    struct ae_audio_session_control *s=*slot;
    if(s->control && !ae_audio_control_close(&s->control)) return false;
    *slot=NULL;release(s);return true;
}
