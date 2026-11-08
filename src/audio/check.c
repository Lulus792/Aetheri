/* Finite private-session playback diagnostic, not an installed audio SDK. */
#define _GNU_SOURCE
#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>
#include <errno.h>
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/prctl.h>
#include <sys/un.h>
#include <unistd.h>

enum { RATE=48000, CHANNELS=2, STRIDE=8, TURN_FRAMES=8192, MAX_FRAMES=96000 };
_Static_assert(sizeof(float)==4 && FLT_RADIX==2 && FLT_MANT_DIG==24,"F32 audio profile");
struct check {
    struct pw_main_loop *loop;struct pw_context *context;struct pw_core *core;struct pw_stream *stream;
    struct spa_hook core_listener,stream_listener;struct spa_source *timer;
    uint32_t total,sent;bool silent,formatted,draining,done,announced,failed,wait_start,started;
};
static void fail(struct check *c,const char *reason) {
    if(!c->failed) fprintf(stderr,"audio-check: %s\n",reason);
    c->failed=true;pw_main_loop_quit(c->loop);
}
static void timeout(void *data,uint64_t expirations) { (void)expirations;fail(data,"deadline; playback not confirmed"); }
static void interrupted(void *data,int signal) { (void)signal;fail(data,"interrupted; playback not confirmed"); }
static void start(void *data,int signal) {
    (void)signal;struct check *c=data;
    if(!c->wait_start || c->started || c->failed || c->done) return;
    if(pw_stream_set_active(c->stream,true)<0) {fail(c,"activation rejected");return;}
    c->started=true;puts("STARTED");
}
static void core_error(void *data,uint32_t id,int seq,int res,const char *message) {
    (void)id;(void)seq;(void)message;if(res<0) fail(data,"server/protocol error");
}
static void state(void *data,enum pw_stream_state old,enum pw_stream_state current,const char *error) {
    (void)old;(void)error;struct check *c=data;
    if(current==PW_STREAM_STATE_ERROR) fail(c,"stream error");
    else if(current==PW_STREAM_STATE_PAUSED && !c->announced) {
        uint32_t node=pw_stream_get_node_id(c->stream);
        if(node==PW_ID_ANY) {fail(c,"missing exported node");return;}
        c->announced=true;printf("CONNECTED node=%u frames=%u rate=%u channels=%u\n",node,c->total,RATE,CHANNELS);
    }
}
static void format(void *data,uint32_t id,const struct spa_pod *param) {
    struct check *c=data;if(id!=SPA_PARAM_Format) return;c->formatted=false;if(!param) return;
    struct spa_audio_info_raw info={0};
    if(spa_format_audio_raw_parse(param,&info)<0 || info.format!=SPA_AUDIO_FORMAT_F32_LE || info.rate!=RATE || info.channels!=CHANNELS || info.position[0]!=SPA_AUDIO_CHANNEL_FL || info.position[1]!=SPA_AUDIO_CHANNEL_FR) {fail(c,"unexpected negotiated format");return;}
    c->formatted=true;
}
static void drained(void *data) {
    struct check *c=data;
    if(!c->draining || c->sent!=c->total) {fail(c,"unexpected drain");return;}
    c->done=true;printf("DRAINED frames=%u\n",c->sent);pw_main_loop_quit(c->loop);
}
static void process(void *data) {
    struct check *c=data;if(c->failed || c->draining) return;
    struct pw_buffer *buffer=pw_stream_dequeue_buffer(c->stream);if(!buffer) return;
    struct spa_buffer *storage=buffer->buffer;
    if(!c->formatted || !storage || storage->n_datas!=1 || !storage->datas || !storage->datas[0].data || (uintptr_t)storage->datas[0].data%_Alignof(float) || !storage->datas[0].chunk || storage->datas[0].maxsize<STRIDE || storage->datas[0].maxsize>4u*1024u*1024u) {fail(c,"unsupported output buffer");return;}
    struct spa_data *d=&storage->datas[0];uint32_t frames=d->maxsize/STRIDE;
    if(frames>TURN_FRAMES) frames=TURN_FRAMES;
    if(buffer->requested && buffer->requested<frames) frames=(uint32_t)buffer->requested;
    if(frames>c->total-c->sent) frames=c->total-c->sent;
    if(!frames) {fail(c,"empty output turn");return;}
    float *samples=d->data;
    for(uint32_t i=0;i<frames;++i) {
        double phase=(double)(c->sent+i)*6.2831853071795864769/(double)RATE;
        samples[2u*i]=c->silent ? 0.0f : (float)(0.125*sin(440.0*phase));
        samples[2u*i+1u]=c->silent ? 0.0f : (float)(0.125*sin(880.0*phase));
    }
    d->chunk->offset=0;d->chunk->stride=STRIDE;d->chunk->size=frames*STRIDE;d->chunk->flags=0;
    buffer->size=frames;
    if(pw_stream_queue_buffer(c->stream,buffer)<0) {fail(c,"queue rejected");return;}
    c->sent+=frames;
    if(c->sent==c->total) {c->draining=true;if(pw_stream_flush(c->stream,true)<0) fail(c,"drain rejected");}
}
static const struct pw_core_events core_events={.version=PW_VERSION_CORE_EVENTS,.error=core_error};
static const struct pw_stream_events stream_events={.version=PW_VERSION_STREAM_EVENTS,.state_changed=state,.param_changed=format,.process=process,.drained=drained};

int main(int argc,char **argv) {
    const char *remote=NULL;struct check c={.total=RATE};bool frames_set=false;
    for(int i=1;i<argc;++i) {
        if(!strcmp(argv[i],"--remote") && !remote && i+1<argc) remote=argv[++i];
        else if(!strcmp(argv[i],"--frames") && !frames_set && i+1<argc) {
            const char *value=argv[++i];if(!*value || strlen(value)>6 || strspn(value,"0123456789")!=strlen(value)) return 2;
            unsigned long count=strtoul(value,NULL,10);if(!count || count>MAX_FRAMES) return 2;c.total=(uint32_t)count;frames_set=true;
        } else if(!strcmp(argv[i],"--silent") && !c.silent) c.silent=true;
        else if(!strcmp(argv[i],"--wait-for-start") && !c.wait_start) c.wait_start=true;
        else return 2;
    }
    if(!remote || remote[0]!='/' || strlen(remote)>=sizeof(((struct sockaddr_un *)0)->sun_path) || getuid()==0 || getuid()!=geteuid() || getgid()!=getegid()) return 2;
    if(prctl(PR_SET_NO_NEW_PRIVS,1,0,0,0)) return 1;
    setvbuf(stdout,NULL,_IOLBF,0);pw_init(NULL,NULL);
    if(strcmp(pw_get_library_version(),"1.6.11") || strcmp(pw_get_headers_version(),"1.6.11")) {fputs("audio-check: unreviewed library/header version\n",stderr);pw_deinit();return 1;}
    int result=1;c.loop=pw_main_loop_new(NULL);if(!c.loop) goto done;
    struct pw_loop *loop=pw_main_loop_get_loop(c.loop);
    c.context=pw_context_new(loop,NULL,0);if(!c.context) goto done;
    struct pw_properties *properties=pw_properties_new(PW_KEY_REMOTE_NAME,remote,NULL);
    if(!properties) goto done; /* Never turn failed explicit routing into default routing. */
    c.core=pw_context_connect(c.context,properties,0);if(!c.core) goto done;
    pw_core_add_listener(c.core,&c.core_listener,&core_events,&c);
    properties=pw_properties_new(PW_KEY_MEDIA_TYPE,"Audio",PW_KEY_MEDIA_CATEGORY,"Playback",PW_KEY_MEDIA_ROLE,"Test",PW_KEY_NODE_NAME,"aetheri.audio.check","adapter.auto-port-config","{ mode = dsp position = preserve }",NULL);
    if(!properties) goto done;
    c.stream=pw_stream_new(c.core,"Aetheri playback check",properties);
    if(!c.stream) goto done;
    pw_stream_add_listener(c.stream,&c.stream_listener,&stream_events,&c);
    uint8_t bytes[1024];struct spa_pod_builder builder=SPA_POD_BUILDER_INIT(bytes,sizeof(bytes));
    struct spa_audio_info_raw info={.format=SPA_AUDIO_FORMAT_F32_LE,.rate=RATE,.channels=CHANNELS,.position={SPA_AUDIO_CHANNEL_FL,SPA_AUDIO_CHANNEL_FR}};
    const struct spa_pod *params[]={spa_format_audio_raw_build(&builder,SPA_PARAM_EnumFormat,&info)};
    enum pw_stream_flags flags=PW_STREAM_FLAG_MAP_BUFFERS|PW_STREAM_FLAG_DONT_RECONNECT;
    if(c.wait_start) flags|=PW_STREAM_FLAG_INACTIVE;
    if(!params[0] || pw_stream_connect(c.stream,PW_DIRECTION_OUTPUT,PW_ID_ANY,flags,params,1)<0) goto done;
    c.timer=pw_loop_add_timer(loop,timeout,&c);
    if(!c.timer || !pw_loop_add_signal(loop,SIGINT,interrupted,&c) || !pw_loop_add_signal(loop,SIGTERM,interrupted,&c)) goto done;
    if(c.wait_start && !pw_loop_add_signal(loop,SIGUSR1,start,&c)) goto done;
    struct timespec deadline={.tv_sec=15};
    if(pw_loop_update_timer(loop,c.timer,&deadline,NULL,false)<0 || pw_main_loop_run(c.loop)<0) goto done;
    result=c.done && !c.failed && c.sent==c.total ? 0 : 1;
done:
    if(c.stream) {spa_hook_remove(&c.stream_listener);pw_stream_destroy(c.stream);}
    if(c.core) {spa_hook_remove(&c.core_listener);pw_core_disconnect(c.core);}
    if(c.context) pw_context_destroy(c.context);
    if(c.loop) pw_main_loop_destroy(c.loop);
    pw_deinit();return result;
}
