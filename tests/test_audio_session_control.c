#define _POSIX_C_SOURCE 200809L
#include "audio/session_control.h"
#include "audio/actor.h"
#include "audio/session_guard.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

/* Controlled observer/backend adapters test the bridge's effect fencing and
 * lifetime, not kernel/service admission. GLib context/thread/fork are real. */
struct ae_audio_manager {int alive;};
struct ae_audio_actor {int alive;};
struct ae_audio_session_guard {int alive;};
struct ae_audio_control {GMainContext *context;void *data;GDestroyNotify destroy;};
static struct ae_audio_manager manager;
static struct ae_audio_actor actors[2];
static struct ae_audio_session_guard guard;
static struct ae_audio_control backend;
static struct ae_audio_snapshot observed;
static unsigned opened,fail_at,actor_index,effects,closes,destroys;
static unsigned closed_manager,closed_actors,closed_guard;
static int manager_result,actor_results[2];
static unsigned session_result;
static bool snapshot_ok=true,immediate=false;
static enum ae_audio_submit effect_result=AE_AUDIO_SUBMITTED;
static uint64_t effect_revision;
static struct ae_audio_output_ref effect_ref;
static double effect_volume;
static bool effect_mute;
static enum ae_audio_command_kind effect_kind;
#define CHECK(x) do {if(!(x)) {fprintf(stderr,"session control line %d: %s\n",__LINE__,#x);exit(1);}} while(0)
static bool enter(void) {if(++opened==fail_at) {errno=EIO;return false;}return true;}
struct ae_audio_manager *ae_audio_manager_open(sd_bus *u,sd_bus *s,uid_t uid,int image) {
    CHECK(u==(sd_bus *)1 && s==(sd_bus *)2 && uid==1000 && image==3);
    if(!enter()) return NULL;
    manager.alive=1;return &manager;
}
int ae_audio_manager_check(struct ae_audio_manager *m) {CHECK(m==&manager && m->alive);return manager_result;}
void ae_audio_manager_close(struct ae_audio_manager **m) {
    if(*m) {CHECK(*m==&manager && manager.alive);manager.alive=0;++closed_manager;*m=NULL;}
}
struct ae_audio_actor *ae_audio_actor_open(struct ae_audio_manager *m,const char *unit,int pin,int image,const char *context) {
    CHECK(m==&manager && m->alive && actor_index<2);
    unsigned i=actor_index++;
    CHECK(!strcmp(unit,i ? "policy.service" : "graph.service"));
    CHECK(pin==(i ? 7 : 6) && image==(i ? 5 : 4) && !strcmp(context,i ? "policy" : "graph"));
    if(!enter()) return NULL;
    actors[i].alive=1;return &actors[i];
}
int ae_audio_actor_check(struct ae_audio_actor *a) {CHECK(a && a->alive);return actor_results[a==&actors[1]];}
void ae_audio_actor_close(struct ae_audio_actor **a) {if(*a) {CHECK((*a)->alive);(*a)->alive=0;++closed_actors;*a=NULL;}}
struct ae_audio_session_guard *ae_audio_session_guard_open(uid_t uid,const char *seat,int graph,int policy) {
    CHECK(uid==1000 && !strcmp(seat,"seat0") && graph==6 && policy==7);
    if(!enter()) return NULL;
    guard.alive=1;return &guard;
}
unsigned ae_audio_session_guard_wait(struct ae_audio_session_guard *g,int timeout) {CHECK(g==&guard && g->alive && !timeout);return session_result;}
void ae_audio_session_guard_close(struct ae_audio_session_guard **g) {if(*g) {CHECK((*g)->alive);(*g)->alive=0;++closed_guard;*g=NULL;}}
struct ae_audio_control *ae_audio_control_open_original(GMainContext *context,const char *remote,int pin,
        ae_audio_changed changed,void *data,GDestroyNotify destroy) {
    CHECK(!strcmp(remote,"/private/audio") && pin==6 && context==g_main_context_get_thread_default());
    if(!enter()) return NULL;
    backend=(struct ae_audio_control){context,data,destroy};changed(&observed,data);return &backend;
}
bool ae_audio_control_snapshot(struct ae_audio_control *c,struct ae_audio_snapshot *out) {
    CHECK(c==&backend);if(!snapshot_ok) return false;*out=observed;return true;
}
static gboolean retire(gpointer data) {struct ae_audio_control *c=data;c->destroy(c->data);return G_SOURCE_REMOVE;}
bool ae_audio_control_close(struct ae_audio_control **slot) {
    CHECK(*slot==&backend);*slot=NULL;++closes;
    if(immediate) (void)retire(&backend);
    else {GSource *source=g_idle_source_new();g_source_set_callback(source,retire,&backend,NULL);g_source_attach(source,backend.context);g_source_unref(source);}
    return true;
}
static enum ae_audio_submit effect(uint64_t revision,struct ae_audio_output_ref ref,enum ae_audio_command_kind kind) {
    ++effects;effect_revision=revision;effect_ref=ref;effect_kind=kind;return effect_result;
}
enum ae_audio_submit ae_audio_control_volume(struct ae_audio_control *c,uint64_t r,struct ae_audio_output_ref ref,double value) {
    CHECK(c==&backend);effect_volume=value;return effect(r,ref,AE_AUDIO_VOLUME);
}
enum ae_audio_submit ae_audio_control_mute(struct ae_audio_control *c,uint64_t r,struct ae_audio_output_ref ref,bool value) {
    CHECK(c==&backend);effect_mute=value;return effect(r,ref,AE_AUDIO_MUTE);
}
enum ae_audio_submit ae_audio_control_default(struct ae_audio_control *c,uint64_t r,struct ae_audio_output_ref ref) {
    CHECK(c==&backend);return effect(r,ref,AE_AUDIO_DEFAULT);
}
static void destroyed(void *data) {CHECK(data==(void *)9);++destroys;}
static void reset(void) {
    CHECK(!manager.alive && !actors[0].alive && !actors[1].alive && !guard.alive);
    opened=fail_at=actor_index=effects=closes=destroys=closed_manager=closed_actors=closed_guard=0;
    manager_result=actor_results[0]=actor_results[1]=0;session_result=0;snapshot_ok=true;immediate=false;
    effect_result=AE_AUDIO_SUBMITTED;
    observed=(struct ae_audio_snapshot){.revision=44,.state=AE_AUDIO_READY,.count=1};
    observed.outputs[0]=(struct ae_audio_output){.ref={99,7,8},.volume_available=true,.volume=0.5};
    strcpy(observed.outputs[0].description,"Original output");
}
struct thread_input {struct ae_audio_session_control *s;};
static gpointer wrong_thread(gpointer data) {
    struct ae_audio_session_control *s=((struct thread_input *)data)->s;
    struct ae_audio_session_snapshot snapshot;
    CHECK(!ae_audio_session_control_snapshot(s,&snapshot));
    CHECK(ae_audio_session_control_mute(s,44,observed.outputs[0].ref,true)==AE_AUDIO_INVALID);
    CHECK(!ae_audio_session_control_close(&s) && s);return NULL;
}
int main(void) {
    GMainContext *context=g_main_context_new();g_main_context_push_thread_default(context);
    struct ae_audio_session_profile profile={.user_bus=(sd_bus *)1,.system_bus=(sd_bus *)2,.uid=1000,
        .seat="seat0",.remote="/private/audio",.graph_unit="graph.service",.policy_unit="policy.service",
        .graph_context="graph",.policy_context="policy",.manager_image=3,.graph_image=4,.policy_image=5,.graph_pidfd=6,.policy_pidfd=7};
    CHECK(!ae_audio_session_control_open(NULL,&profile,NULL,NULL));
    struct ae_audio_session_profile bad=profile;bad.policy_unit=bad.graph_unit;
    CHECK(!ae_audio_session_control_open(context,&bad,NULL,NULL));
    bad=profile;bad.remote="relative";CHECK(!ae_audio_session_control_open(context,&bad,NULL,NULL));
    for(unsigned failure=1;failure<=5;++failure) {
        reset();fail_at=failure;
        CHECK(!ae_audio_session_control_open(context,&profile,(void *)9,destroyed) && errno==EIO);
        CHECK(!destroys && !closes);
    }
    /* Admission drift during setup prevents even protocol connection. */
    reset();actor_results[1]=-ESTALE;
    CHECK(!ae_audio_session_control_open(context,&profile,(void *)9,destroyed));
    CHECK(opened==4 && !closes && !destroys);
    reset();struct ae_audio_session_control *s=ae_audio_session_control_open(context,&profile,(void *)9,destroyed);CHECK(s);
    struct ae_audio_session_snapshot snapshot;
    CHECK(ae_audio_session_control_snapshot(s,&snapshot) && !snapshot.loss && snapshot.audio.count==1);
    struct ae_audio_output_ref ref=snapshot.audio.outputs[0].ref;
    CHECK(ae_audio_session_control_volume(s,44,ref,0.75)==AE_AUDIO_SUBMITTED);
    CHECK(effects==1 && effect_revision==44 && ref.connection==effect_ref.connection && ref.serial==effect_ref.serial && ref.id==effect_ref.id && effect_kind==AE_AUDIO_VOLUME && effect_volume==0.75);
    effect_result=AE_AUDIO_STALE;CHECK(ae_audio_session_control_mute(s,43,ref,true)==AE_AUDIO_STALE && effect_mute && effect_kind==AE_AUDIO_MUTE);
    effect_result=AE_AUDIO_BUSY;CHECK(ae_audio_session_control_default(s,44,ref)==AE_AUDIO_BUSY && effect_kind==AE_AUDIO_DEFAULT);
    struct thread_input thread={s};g_thread_join(g_thread_new("wrong-owner",wrong_thread,&thread));CHECK(effects==3 && !closes);
    pid_t child=fork();CHECK(child>=0);
    if(!child) {CHECK(!ae_audio_session_control_snapshot(s,&snapshot));CHECK(!ae_audio_session_control_close(&s));_exit(0);}
    int status;CHECK(waitpid(child,&status,0)==child && WIFEXITED(status) && !WEXITSTATUS(status));
    CHECK(ae_audio_session_control_close(&s) && !s && closes==1 && !destroys);
    while(g_main_context_iteration(context,FALSE)) {}
    CHECK(destroys==1 && closed_manager==1 && closed_actors==2 && closed_guard==1);
    /* Each independent loss, and simultaneous losses, rejects a direct command
     * without a preceding UI refresh. Recovery cannot restore the old attempt. */
    for(unsigned variant=0;variant<7;++variant) {
        reset();immediate=variant%2!=0;s=ae_audio_session_control_open(context,&profile,(void *)9,destroyed);CHECK(s);
        observed.command=17;observed.command_state=AE_AUDIO_COMMAND_PENDING;
        CHECK(ae_audio_session_control_snapshot(s,&snapshot) && !snapshot.loss && snapshot.audio.command==17);
        unsigned expected=0;
        if(variant==0 || variant==6) {manager_result=-EIO;expected|=AE_AUDIO_CONTROL_MANAGER_LOST;}
        if(variant==1 || variant==6) {actor_results[0]=-ESTALE;expected|=AE_AUDIO_CONTROL_GRAPH_LOST;}
        if(variant==2 || variant==6) {actor_results[1]=-ESTALE;expected|=AE_AUDIO_CONTROL_POLICY_LOST;}
        if(variant==3 || variant==6) {session_result=AE_AUDIO_SESSION_INACTIVE;expected|=AE_AUDIO_CONTROL_SESSION_LOST;}
        if(variant==4) {observed.state=AE_AUDIO_FAILED;expected=AE_AUDIO_CONTROL_BACKEND_LOST;}
        if(variant==5) {snapshot_ok=false;expected=AE_AUDIO_CONTROL_BACKEND_LOST;}
        CHECK(ae_audio_session_control_mute(s,44,ref,true)==AE_AUDIO_UNAVAILABLE && !effects && closes==1);
        CHECK(ae_audio_session_control_snapshot(s,&snapshot) && snapshot.loss==expected && !snapshot.audio.count);
        CHECK(snapshot.audio.state==AE_AUDIO_DISCONNECTED);
        CHECK(snapshot.audio.command==17 && snapshot.audio.command_state==AE_AUDIO_COMMAND_UNKNOWN);
        manager_result=actor_results[0]=actor_results[1]=0;session_result=0;observed.state=AE_AUDIO_READY;snapshot_ok=true;
        CHECK(ae_audio_session_control_volume(s,44,ref,0.9)==AE_AUDIO_UNAVAILABLE);
        CHECK(ae_audio_session_control_default(s,44,ref)==AE_AUDIO_UNAVAILABLE);
        CHECK(ae_audio_session_control_snapshot(s,&snapshot) && snapshot.loss==expected && closes==1 && !effects);
        while(g_main_context_iteration(context,FALSE)) {}
        CHECK(!destroys);CHECK(ae_audio_session_control_close(&s) && !s && destroys==1);
    }
    CHECK(!ae_audio_session_control_close(NULL));
    g_main_context_pop_thread_default(context);g_main_context_unref(context);
    puts("session controls: setup unwind, guarded effects, original loss, unknown commands and async ownership passed");return 0;
}
