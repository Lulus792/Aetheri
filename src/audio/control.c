#define _GNU_SOURCE
#include "audio/control.h"
#include "audio/remote.h"
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <sys/random.h>
#include <wp/wp.h>

struct ae_audio_control {
    unsigned refs,loading;
    bool closing,closed,barrier;
    GThread *thread;
    GMainContext *context;
    GCancellable *cancel;
    GSource *idle,*deadline;
    WpCore *core;
    WpObjectManager *objects;
    WpPlugin *mixer,*defaults;
    uint64_t connection;
    double requested_volume;
    bool requested_mute;
    struct ae_audio_snapshot snapshot;
    ae_audio_changed changed;
    void *data;
    GDestroyNotify destroy;
};
static void dirty(struct ae_audio_control *);
static gboolean timeout_command(gpointer);
static struct ae_audio_control *retain(struct ae_audio_control *c) {++c->refs;return c;}
static gboolean finalize_owner(gpointer data) {
    struct ae_audio_control *c=data;
    g_clear_object(&c->mixer);g_clear_object(&c->defaults);g_clear_object(&c->objects);
    g_clear_object(&c->core);g_clear_object(&c->cancel);g_main_context_unref(c->context);
    if(c->destroy) c->destroy(c->data);
    free(c);return G_SOURCE_REMOVE;
}
static void release(gpointer data) {
    struct ae_audio_control *c=data;
    if(--c->refs) return;
    if(!c->core) {(void)finalize_owner(c);return;}
    /* The last sync callback can still be on a dynamically loaded module's
     * dispatch stack. Keep the core/modules until a later idle turn, after
     * already-ready GTask callbacks have returned. The caller keeps this
     * context running until its data-destroy notification, as before. */
    GSource *source=g_idle_source_new();g_source_set_callback(source,finalize_owner,c,NULL);g_source_attach(source,c->context);g_source_unref(source);
}
static bool owner(struct ae_audio_control *c) {return c && !c->closing && c->thread==g_thread_self();}
static void source_clear(GSource **p) {if(*p) {g_source_destroy(*p);g_source_unref(*p);*p=NULL;}}
static bool equal_ref(struct ae_audio_output_ref a,struct ae_audio_output_ref b) {return a.connection==b.connection && a.serial==b.serial && a.id==b.id;}
static bool text_copy(char *out,size_t capacity,const char *text) {
    if(!text || !g_utf8_validate(text,-1,NULL) || strlen(text)>=capacity) return false;
    memcpy(out,text,strlen(text)+1);return true;
}
static bool serial_value(const char *s,uint64_t *out) {
    if(!s || !*s || strspn(s,"0123456789")!=strlen(s)) return false;
    errno=0;char *end=NULL;unsigned long long n=strtoull(s,&end,10);
    if(errno || !end || *end || !n) return false;
    *out=(uint64_t)n;return true;
}
static void fail(struct ae_audio_control *c,enum ae_audio_state state) {
    c->snapshot.state=state;c->snapshot.count=0;
    if(c->snapshot.command_state==AE_AUDIO_COMMAND_PENDING) c->snapshot.command_state=AE_AUDIO_COMMAND_UNKNOWN;
    source_clear(&c->deadline);dirty(c);
}
static int compare_output(const void *a,const void *b) {
    uint64_t x=((const struct ae_audio_output *)a)->ref.serial,y=((const struct ae_audio_output *)b)->ref.serial;
    return (x>y)-(x<y);
}
static bool collect(struct ae_audio_control *c) {
    c->snapshot.count=0;guint default_id=UINT32_MAX;
    g_signal_emit_by_name(c->defaults,"get-default-node","Audio/Sink",&default_id);
    WpIterator *it=wp_object_manager_new_iterator(c->objects);GValue item=G_VALUE_INIT;bool okay=true;
    while(wp_iterator_next(it,&item)) {
        WpNode *node=g_value_get_object(&item);
        if(c->snapshot.count==AE_AUDIO_OUTPUTS_MAX) {c->snapshot.error=AE_AUDIO_ERROR_LIMIT;okay=false;g_value_unset(&item);break;}
        struct ae_audio_output *o=&c->snapshot.outputs[c->snapshot.count];memset(o,0,sizeof(*o));
        o->ref.connection=c->connection;o->ref.id=wp_proxy_get_bound_id(WP_PROXY(node));
        const char *name=wp_pipewire_object_get_property(WP_PIPEWIRE_OBJECT(node),"node.name");
        const char *description=wp_pipewire_object_get_property(WP_PIPEWIRE_OBJECT(node),"node.description");
        const char *serial=wp_pipewire_object_get_property(WP_PIPEWIRE_OBJECT(node),"object.serial");
        if(o->ref.id==UINT32_MAX) c->snapshot.error=AE_AUDIO_ERROR_ID;
        else if(!serial_value(serial,&o->ref.serial)) c->snapshot.error=AE_AUDIO_ERROR_SERIAL;
        else if(!text_copy(o->name,sizeof(o->name),name) || !*o->name) c->snapshot.error=AE_AUDIO_ERROR_NAME;
        else if(!text_copy(o->description,sizeof(o->description),description ? description : name)) c->snapshot.error=AE_AUDIO_ERROR_DESCRIPTION;
        okay=c->snapshot.error==AE_AUDIO_ERROR_NONE;
        o->is_default=o->ref.id==default_id;
        GVariant *v=NULL;g_signal_emit_by_name(c->mixer,"get-volume",o->ref.id,&v);
        if(v) {
            gboolean mute=FALSE;
            o->volume_available=g_variant_lookup(v,"volume","d",&o->volume) && g_variant_lookup(v,"mute","b",&mute);
            o->muted=mute!=FALSE;
            if(o->volume_available && (!isfinite(o->volume) || o->volume<0 || o->volume>16)) {okay=false;c->snapshot.error=AE_AUDIO_ERROR_VOLUME;}
            g_variant_unref(v);
        }
        g_value_unset(&item);if(!okay) break;
        ++c->snapshot.count;
    }
    wp_iterator_unref(it);
    if(okay) qsort(c->snapshot.outputs,c->snapshot.count,sizeof(c->snapshot.outputs[0]),compare_output);
    return okay;
}
static void observe_command(struct ae_audio_control *c) {
    if(c->snapshot.command_state!=AE_AUDIO_COMMAND_PENDING || !c->barrier) return;
    for(size_t i=0;i<c->snapshot.count;++i) {
        const struct ae_audio_output *o=&c->snapshot.outputs[i];
        if(!equal_ref(o->ref,c->snapshot.command_target)) continue;
        bool matches=c->snapshot.command_kind==AE_AUDIO_DEFAULT ? o->is_default :
            o->volume_available && (c->snapshot.command_kind==AE_AUDIO_MUTE ? o->muted==c->requested_mute : fabs(o->volume-c->requested_volume)<=0.00001);
        if(matches) {c->snapshot.command_state=AE_AUDIO_COMMAND_OBSERVED;source_clear(&c->deadline);}
        return;
    }
    c->snapshot.command_state=AE_AUDIO_COMMAND_UNKNOWN;source_clear(&c->deadline);
}
static gboolean publish(gpointer data) {
    struct ae_audio_control *c=data;GSource *source=c->idle;c->idle=NULL;g_source_unref(source);
    if(c->closing) return G_SOURCE_REMOVE;
    if(c->snapshot.state==AE_AUDIO_READY && !collect(c)) {c->snapshot.state=AE_AUDIO_FAILED;c->snapshot.count=0;}
    if(c->snapshot.state==AE_AUDIO_READY) observe_command(c);
    else if(c->snapshot.command_state==AE_AUDIO_COMMAND_PENDING) {c->snapshot.command_state=AE_AUDIO_COMMAND_UNKNOWN;source_clear(&c->deadline);}
    if(c->snapshot.revision==UINT64_MAX) c->snapshot.state=AE_AUDIO_FAILED;
    else ++c->snapshot.revision;
    if(c->changed) c->changed(&c->snapshot,c->data);
    return G_SOURCE_REMOVE;
}
static void dirty(struct ae_audio_control *c) {
    if(c->closing || c->idle) return;
    c->idle=g_idle_source_new();g_source_set_callback(c->idle,publish,retain(c),release);g_source_attach(c->idle,c->context);
}
static void changed(WpObjectManager *objects,gpointer data) {(void)objects;dirty(data);}
static void defaults_changed(WpPlugin *plugin,gpointer data) {(void)plugin;dirty(data);}
static void volume_changed(WpPlugin *plugin,guint id,gpointer data) {(void)plugin;(void)id;dirty(data);}
static void installed(WpObjectManager *objects,gpointer data) {
    (void)objects;struct ae_audio_control *c=data;
    if(c->snapshot.state==AE_AUDIO_CONNECTING) c->snapshot.state=AE_AUDIO_READY;
    source_clear(&c->deadline);
    dirty(c);
}
static void disconnected(WpCore *core,gpointer data) {(void)core;struct ae_audio_control *c=data;if(!c->closing) fail(c,AE_AUDIO_DISCONNECTED);}
static void loaded(GObject *source,GAsyncResult *result,gpointer data) {
    struct ae_audio_control *c=data;GError *error=NULL;
    bool okay=wp_core_load_component_finish(WP_CORE(source),result,&error)!=FALSE;g_clear_error(&error);
    if(!c->closing) {
        if(!okay) {c->snapshot.error=AE_AUDIO_ERROR_PLUGINS;fail(c,AE_AUDIO_FAILED);}
        if(--c->loading==0 && c->snapshot.state==AE_AUDIO_CONNECTING) {
            c->mixer=wp_plugin_find(c->core,"mixer-api");c->defaults=wp_plugin_find(c->core,"default-nodes-api");
            if(!c->mixer || !c->defaults) {c->snapshot.error=AE_AUDIO_ERROR_PLUGINS;fail(c,AE_AUDIO_FAILED);}
            else {
                g_object_set(c->mixer,"scale",1,NULL);
                g_signal_connect(c->mixer,"changed",G_CALLBACK(volume_changed),c);
                g_signal_connect(c->defaults,"changed",G_CALLBACK(defaults_changed),c);
                wp_core_install_object_manager(c->core,c->objects);
            }
        }
    }
    release(c);
}
static void connected(WpCore *core,gpointer data) {
    struct ae_audio_control *c=data;
    if(c->closing || c->loading || c->snapshot.state!=AE_AUDIO_CONNECTING) return;
    const char *version=wp_core_get_remote_version(core);
    if(!version || strcmp(version,"1.6.11")) {c->snapshot.error=AE_AUDIO_ERROR_VERSION;fail(c,AE_AUDIO_FAILED);return;}
    c->loading=2;
    wp_core_load_component(core,"libwireplumber-module-default-nodes-api","module",NULL,NULL,c->cancel,loaded,retain(c));
    wp_core_load_component(core,"libwireplumber-module-mixer-api","module",NULL,NULL,c->cancel,loaded,retain(c));
}
static struct ae_audio_control *open_control(GMainContext *context,const char *remote,int original,ae_audio_changed callback,void *data,GDestroyNotify destroy) {
    if(!context || g_main_context_get_thread_default()!=context || !remote || remote[0]!='/' || strlen(remote)>100 || strchr(remote,'\n') || strchr(remote,'\r')) return NULL;
    struct ae_audio_control *c=calloc(1,sizeof(*c));if(!c) return NULL;
    if(getrandom(&c->connection,sizeof(c->connection),0)!=(ssize_t)sizeof(c->connection) || !c->connection) {free(c);return NULL;}
    c->refs=1;c->thread=g_thread_self();c->context=g_main_context_ref(context);c->cancel=g_cancellable_new();
    c->changed=callback;c->data=data;c->destroy=destroy;c->snapshot.state=AE_AUDIO_CONNECTING;
    wp_init(WP_INIT_ALL);
    if(strcmp(wp_get_library_version(),"0.5.19") || glib_major_version!=2 || glib_minor_version!=88 || glib_micro_version!=0) {c->destroy=NULL;release(c);return NULL;}
    c->core=wp_core_new(context,NULL,wp_properties_new("remote.name",remote,"application.name","Aetheri Audio Control",NULL));
    c->objects=wp_object_manager_new();
    wp_object_manager_add_interest(c->objects,WP_TYPE_NODE,WP_CONSTRAINT_TYPE_PW_GLOBAL_PROPERTY,"media.class","=s","Audio/Sink",NULL);
    wp_object_manager_request_object_features(c->objects,WP_TYPE_NODE,WP_PIPEWIRE_OBJECT_FEATURES_MINIMAL);
    g_signal_connect(c->objects,"installed",G_CALLBACK(installed),c);g_signal_connect(c->objects,"objects-changed",G_CALLBACK(changed),c);
    g_signal_connect(c->core,"disconnected",G_CALLBACK(disconnected),c);
    g_signal_connect(c->core,"connected",G_CALLBACK(connected),c);
    c->deadline=g_timeout_source_new(5000);g_source_set_callback(c->deadline,timeout_command,retain(c),release);g_source_attach(c->deadline,c->context);
    g_main_context_push_thread_default(context);
    int fd=original>=0 ? ae_audio_remote_connect_original(remote,original) : ae_audio_remote_connect(remote);
    if(fd<0 && original>=0) c->snapshot.error=AE_AUDIO_ERROR_ENDPOINT;
    /* connect_fd consumes the original socket on success and on error. The
     * ambient PIPEWIRE_REMOTE may not override the caller's explicit endpoint. */
    if(fd<0 || !wp_core_connect_fd(c->core,fd)) fail(c,AE_AUDIO_DISCONNECTED);
    g_main_context_pop_thread_default(context);dirty(c);return c;
}
struct ae_audio_control *ae_audio_control_open(GMainContext *context,const char *remote,ae_audio_changed callback,void *data,GDestroyNotify destroy) {
    return open_control(context,remote,-1,callback,data,destroy);
}
struct ae_audio_control *ae_audio_control_open_original(GMainContext *context,const char *remote,int original,ae_audio_changed callback,void *data,GDestroyNotify destroy) {
    return original<0 ? NULL : open_control(context,remote,original,callback,data,destroy);
}
bool ae_audio_control_snapshot(struct ae_audio_control *c,struct ae_audio_snapshot *out) {if(!owner(c) || !out) return false;*out=c->snapshot;return true;}
static gboolean timeout_command(gpointer data) {
    struct ae_audio_control *c=data;GSource *s=c->deadline;c->deadline=NULL;g_source_unref(s);
    c->snapshot.error=AE_AUDIO_ERROR_DEADLINE;fail(c,AE_AUDIO_FAILED);return G_SOURCE_REMOVE;
}
static void synchronized(GObject *source,GAsyncResult *result,gpointer data) {
    struct ae_audio_control *c=data;GError *error=NULL;bool okay=wp_core_sync_finish(WP_CORE(source),result,&error)!=FALSE;g_clear_error(&error);
    if(!c->closing) {if(okay) {c->barrier=true;dirty(c);} else {c->snapshot.error=AE_AUDIO_ERROR_SYNC;fail(c,AE_AUDIO_FAILED);}}
    release(c);
}
static enum ae_audio_submit submit(struct ae_audio_control *c,uint64_t revision,struct ae_audio_output_ref ref,enum ae_audio_command_kind kind,double volume,bool mute) {
    if(!owner(c) || (kind==AE_AUDIO_VOLUME && (!isfinite(volume) || volume<0 || volume>1))) return AE_AUDIO_INVALID;
    if(c->snapshot.state!=AE_AUDIO_READY) return AE_AUDIO_UNAVAILABLE;
    if(c->snapshot.command_state==AE_AUDIO_COMMAND_PENDING) return AE_AUDIO_BUSY;
    if(c->snapshot.revision!=revision || c->idle || ref.connection!=c->connection) return AE_AUDIO_STALE;
    const struct ae_audio_output *selected=NULL;
    for(size_t i=0;i<c->snapshot.count;++i) if(equal_ref(c->snapshot.outputs[i].ref,ref)) selected=&c->snapshot.outputs[i];
    if(!selected) return AE_AUDIO_STALE;
    if(kind!=AE_AUDIO_DEFAULT && !selected->volume_available) return AE_AUDIO_UNAVAILABLE;
    /* The pinned default module interpolates node.name into JSON without
     * escaping. Never pass quotes/backslashes/control or ambiguous names. */
    if(kind==AE_AUDIO_DEFAULT) {
        if(strspn(selected->name,"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._:-")!=strlen(selected->name)) return AE_AUDIO_UNAVAILABLE;
        for(size_t i=0;i<c->snapshot.count;++i) if(!equal_ref(c->snapshot.outputs[i].ref,ref) && !strcmp(c->snapshot.outputs[i].name,selected->name)) return AE_AUDIO_UNAVAILABLE;
    }
    if(c->snapshot.command==UINT64_MAX) return AE_AUDIO_UNAVAILABLE;
    gboolean accepted=FALSE;
    if(kind==AE_AUDIO_DEFAULT) g_signal_emit_by_name(c->defaults,"set-default-configured-node-name","Audio/Sink",selected->name,&accepted);
    else {
        GVariantBuilder builder;g_variant_builder_init(&builder,G_VARIANT_TYPE_VARDICT);
        if(kind==AE_AUDIO_VOLUME) g_variant_builder_add(&builder,"{sv}","volume",g_variant_new_double(volume));
        else g_variant_builder_add(&builder,"{sv}","mute",g_variant_new_boolean(mute));
        GVariant *value=g_variant_ref_sink(g_variant_builder_end(&builder));
        g_signal_emit_by_name(c->mixer,"set-volume",ref.id,value,&accepted);g_variant_unref(value);
    }
    if(!accepted) return AE_AUDIO_UNAVAILABLE;
    ++c->snapshot.command;c->snapshot.command_state=AE_AUDIO_COMMAND_PENDING;c->snapshot.command_kind=kind;c->snapshot.command_target=ref;
    c->barrier=false;c->requested_volume=volume;c->requested_mute=mute;
    c->deadline=g_timeout_source_new(5000);g_source_set_callback(c->deadline,timeout_command,retain(c),release);g_source_attach(c->deadline,c->context);
    g_main_context_push_thread_default(c->context);struct ae_audio_control *held=retain(c);
    /* The pinned implementation completes its GTask even when sync returns
     * false. The callback, not this branch, still owns held. */
    if(!wp_core_sync(c->core,c->cancel,synchronized,held)) {c->snapshot.error=AE_AUDIO_ERROR_SYNC;fail(c,AE_AUDIO_FAILED);}
    g_main_context_pop_thread_default(c->context);dirty(c);return AE_AUDIO_SUBMITTED;
}
enum ae_audio_submit ae_audio_control_volume(struct ae_audio_control *c,uint64_t rev,struct ae_audio_output_ref ref,double v) {return submit(c,rev,ref,AE_AUDIO_VOLUME,v,false);}
enum ae_audio_submit ae_audio_control_mute(struct ae_audio_control *c,uint64_t rev,struct ae_audio_output_ref ref,bool mute) {return submit(c,rev,ref,AE_AUDIO_MUTE,0,mute);}
enum ae_audio_submit ae_audio_control_default(struct ae_audio_control *c,uint64_t rev,struct ae_audio_output_ref ref) {return submit(c,rev,ref,AE_AUDIO_DEFAULT,0,false);}
static void finish_close(struct ae_audio_control *c) {
    if(c->closed) return;
    c->closed=true;source_clear(&c->deadline);wp_core_disconnect(c->core);
}
static gboolean close_timeout(gpointer data) {
    struct ae_audio_control *c=data;GSource *s=c->deadline;c->deadline=NULL;g_source_unref(s);
    finish_close(c);return G_SOURCE_REMOVE;
}
static gboolean close_idle(gpointer data) {
    struct ae_audio_control *c=data;GSource *s=c->idle;c->idle=NULL;g_source_unref(s);
    finish_close(c);return G_SOURCE_REMOVE;
}
static void queue_close(struct ae_audio_control *c) {
    if(c->closed || c->idle) return;
    c->idle=g_idle_source_new();g_source_set_callback(c->idle,close_idle,retain(c),release);g_source_attach(c->idle,c->context);
}
static void close_synchronized(GObject *source,GAsyncResult *result,gpointer data) {
    struct ae_audio_control *c=data;GError *error=NULL;
    (void)wp_core_sync_finish(WP_CORE(source),result,&error);g_clear_error(&error);
    /* A sync completion may still be inside protocol-native's on_remote_data.
     * Disconnecting that remote there invalidates its active read dispatch. */
    queue_close(c);release(c);
}
bool ae_audio_control_close(struct ae_audio_control **slot) {
    if(!slot || !owner(*slot)) return false;
    struct ae_audio_control *c=*slot;*slot=NULL;c->closing=true;
    source_clear(&c->idle);source_clear(&c->deadline);
    g_signal_handlers_disconnect_by_data(c->core,c);g_signal_handlers_disconnect_by_data(c->objects,c);
    if(c->mixer) g_signal_handlers_disconnect_by_data(c->mixer,c);
    if(c->defaults) g_signal_handlers_disconnect_by_data(c->defaults,c);
    g_cancellable_cancel(c->cancel);
    /* Stop producers of cache-refresh work before the final barrier. This is
     * local observer retirement, not app/process/device drain or rollback. */
    if(c->mixer) wp_object_deactivate(WP_OBJECT(c->mixer),WP_PLUGIN_FEATURE_ENABLED);
    if(c->defaults) wp_object_deactivate(WP_OBJECT(c->defaults),WP_PLUGIN_FEATURE_ENABLED);
    if(wp_core_get_pw_core(c->core)) {
        c->deadline=g_timeout_source_new(5000);g_source_set_callback(c->deadline,close_timeout,retain(c),release);g_source_attach(c->deadline,c->context);
        g_main_context_push_thread_default(c->context);
        /* Even a false return retains its asynchronous callback obligation. */
        if(!wp_core_sync(c->core,NULL,close_synchronized,retain(c))) queue_close(c);
        g_main_context_pop_thread_default(c->context);
    } else queue_close(c);
    release(c);return true;
}
