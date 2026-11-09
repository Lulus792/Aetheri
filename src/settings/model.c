#include "model.h"
#include "text/grapheme.h"
#include <limits.h>
#include <math.h>
#include <string.h>

void ae_settings_init(struct ae_settings *model) {
    *model = (struct ae_settings){.theme = AE_SETTINGS_DARK, .text_percent = 100};
}
bool ae_settings_snapshot(struct ae_settings *model, const struct ae_settings_display *displays, size_t count) {
    if (!model) return false;
    if (count > AE_SETTINGS_DISPLAYS || (count && !displays) || model->refreshes == UINT_MAX) goto fail;
    for (size_t i = 0; i < count; ++i) {
        const struct ae_settings_display *d = &displays[i];
        if (!d->id || !memchr(d->name, 0, sizeof(d->name)) || !d->name[0] ||
                !isfinite(d->scale) || d->scale <= 0 || !isfinite(d->refresh) || d->refresh < 0 ||
                (d->mode_known && (d->width <= 0 || d->height <= 0))) goto fail;
        for (size_t j = 0; j < i; ++j) if (displays[j].id == d->id) goto fail;
    }
    uint32_t selected = model->selected < model->count ? model->displays[model->selected].id : 0;
    /* The input may refer to our own current records. Copy before clearing. */
    struct ae_settings_display copy[AE_SETTINGS_DISPLAYS] = {0};
    if (count) memcpy(copy, displays, count * sizeof(*copy));
    memcpy(model->displays, copy, sizeof(copy));
    model->count = count; model->selected = 0;
    for (size_t i = 0; i < count; ++i) if (copy[i].id == selected) model->selected = i;
    model->stale = false; ++model->refreshes;
    return true;
fail:
    model->stale = true;
    return false;
}
bool ae_settings_select(struct ae_settings *model, uint32_t id) {
    if (!model || model->stale) return false;
    for (size_t i = 0; i < model->count; ++i) if (model->displays[i].id == id) {
        model->selected = i; return true;
    }
    return false;
}
static bool same_output(struct ae_audio_output_ref a,struct ae_audio_output_ref b) {
    return a.connection==b.connection && a.serial==b.serial && a.id==b.id;
}
static bool sound_text(const char *text,size_t capacity,bool required) {
    const char *end=memchr(text,0,capacity);
    if(!end || (required && end==text)) return false;
    size_t length=(size_t)(end-text),offset=0;uint32_t scalar;
    while(offset<length) if(!ae_utf8_next(text,length,&offset,&scalar) || scalar<32 || scalar==127) return false;
    return true;
}
bool ae_settings_sound_snapshot(struct ae_settings *m,const struct ae_audio_snapshot *input,unsigned loss) {
    if(!m) return false;
    struct ae_settings_sound *s=&m->sound;
    if(loss || (input && (input->state==AE_AUDIO_FAILED || input->state==AE_AUDIO_DISCONNECTED))) {
        if(!s->fenced) s->loss=loss;
        s->fenced=true;s->queued=false;s->feedback=AE_SETTINGS_SOUND_UNKNOWN;return true;
    }
    if(s->fenced) return false;
    if(!input || input->count>AE_AUDIO_OUTPUTS_MAX || input->state<AE_AUDIO_CONNECTING || input->state>AE_AUDIO_DISCONNECTED ||
            input->command_state<AE_AUDIO_COMMAND_NONE || input->command_state>AE_AUDIO_COMMAND_UNKNOWN ||
            (s->attached && input->revision<s->observation.revision)) goto fail;
    unsigned defaults=0;uint64_t connection=s->connection;
    for(size_t i=0;i<input->count;++i) {
        const struct ae_audio_output *o=&input->outputs[i];
        if(!o->ref.connection || !o->ref.serial || !o->ref.id || !sound_text(o->name,sizeof(o->name),true) ||
                !sound_text(o->description,sizeof(o->description),false) || !isfinite(o->volume) || o->volume<0) goto fail;
        if(connection && connection!=o->ref.connection) {s->fenced=true;s->queued=false;s->feedback=AE_SETTINGS_SOUND_UNKNOWN;return false;}
        connection=o->ref.connection;
        if(o->is_default && ++defaults>1) goto fail;
        for(size_t j=0;j<i;++j) if(same_output(input->outputs[j].ref,o->ref)) goto fail;
    }
    uint64_t ids[AE_AUDIO_OUTPUTS_MAX]={0},next=s->next_id?s->next_id:UINT64_C(0x200000000);
    for(size_t i=0;i<input->count;++i) {
        for(size_t j=0;j<s->observation.count;++j)
            if(same_output(input->outputs[i].ref,s->observation.outputs[j].ref)) ids[i]=s->ids[j];
        if(!ids[i]) {if(next>UINT64_MAX-4) goto fail;ids[i]=next;next+=4;}
    }
    struct ae_audio_snapshot copy=*input;
    s->observation=copy;memcpy(s->ids,ids,sizeof(ids));s->next_id=next;s->connection=connection;
    s->attached=true;s->stale=false;
    if(s->feedback==AE_SETTINGS_SOUND_SUBMITTED && copy.command>s->baseline_command &&
            copy.command_kind==s->request.kind && same_output(copy.command_target,s->request.target)) {
        if(copy.command_state==AE_AUDIO_COMMAND_OBSERVED) s->feedback=AE_SETTINGS_SOUND_OBSERVED;
        if(copy.command_state==AE_AUDIO_COMMAND_UNKNOWN) s->feedback=AE_SETTINGS_SOUND_UNKNOWN;
    }
    return true;
fail:
    s->stale=true;
    if(s->queued) s->feedback=AE_SETTINGS_SOUND_REJECTED;
    if(s->dispatching) s->feedback=AE_SETTINGS_SOUND_UNKNOWN;
    s->queued=false;return false;
}
bool ae_settings_sound_enabled(const struct ae_settings *m) {
    if(!m) return false;
    const struct ae_settings_sound *s=&m->sound;
    return s->attached && !s->stale && !s->fenced && !s->queued && !s->dispatching &&
        s->observation.state==AE_AUDIO_READY && s->observation.revision &&
        s->observation.command_state!=AE_AUDIO_COMMAND_PENDING && s->observation.command_state!=AE_AUDIO_COMMAND_UNKNOWN &&
        s->feedback!=AE_SETTINGS_SOUND_SUBMITTED && s->feedback!=AE_SETTINGS_SOUND_UNKNOWN;
}
bool ae_settings_sound_request(struct ae_settings *m,uint64_t id,enum ae_audio_command_kind kind,double volume,bool mute) {
    if(!ae_settings_sound_enabled(m) || kind<AE_AUDIO_VOLUME || kind>AE_AUDIO_DEFAULT ||
            (kind==AE_AUDIO_VOLUME && (!isfinite(volume) || volume<0 || volume>1))) return false;
    struct ae_settings_sound *s=&m->sound;
    for(size_t i=0;i<s->observation.count;++i) if(s->ids[i]==id) {
        const struct ae_audio_output *o=&s->observation.outputs[i];
        if(kind!=AE_AUDIO_DEFAULT && !o->volume_available) return false;
        s->request=(struct ae_settings_sound_request){.kind=kind,.revision=s->observation.revision,
            .target=o->ref,.volume=volume,.mute=mute};
        s->baseline_command=s->observation.command;s->queued=true;s->feedback=AE_SETTINGS_SOUND_QUEUED;return true;
    }
    return false;
}
bool ae_settings_sound_take(struct ae_settings *m,struct ae_settings_sound_request *out) {
    if(!m || !out || !m->sound.queued || m->sound.fenced || m->sound.stale) return false;
    *out=m->sound.request;m->sound.queued=false;m->sound.dispatching=true;return true;
}
void ae_settings_sound_result(struct ae_settings *m,enum ae_audio_submit result) {
    if(!m || !m->sound.dispatching) return;
    m->sound.dispatching=false;
    if(m->sound.fenced) return;
    m->sound.feedback=result==AE_AUDIO_SUBMITTED?AE_SETTINGS_SOUND_SUBMITTED:AE_SETTINGS_SOUND_REJECTED;
}
