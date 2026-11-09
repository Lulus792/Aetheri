#include "settings/audio.h"
#include "audio/session_control.h"
#include <stdio.h>
#include <stdlib.h>
#define CHECK(x) do {if(!(x)) {fprintf(stderr,"audio binding line %d: %s\n",__LINE__,#x);exit(1);}} while(0)
struct ae_audio_session_control {int unused;};
static struct ae_audio_session_snapshot current;
static unsigned effects;
static bool observable=true;
static enum ae_audio_submit result=AE_AUDIO_SUBMITTED;
bool ae_audio_session_control_snapshot(struct ae_audio_session_control *owner,struct ae_audio_session_snapshot *out) {
    CHECK(owner);if(!observable) return false;*out=current;return true;
}
static enum ae_audio_submit effect(uint64_t revision,struct ae_audio_output_ref ref,enum ae_audio_command_kind kind) {
    CHECK(revision==current.audio.revision && ref.connection==91 && ref.serial==2 && ref.id==3);++effects;
    if(result==AE_AUDIO_SUBMITTED) {++current.audio.command;current.audio.command_kind=kind;current.audio.command_target=ref;current.audio.command_state=AE_AUDIO_COMMAND_PENDING;}
    return result;
}
enum ae_audio_submit ae_audio_session_control_volume(struct ae_audio_session_control *o,uint64_t revision,struct ae_audio_output_ref ref,double value) {CHECK(o && value==0.5);return effect(revision,ref,AE_AUDIO_VOLUME);}
enum ae_audio_submit ae_audio_session_control_mute(struct ae_audio_session_control *o,uint64_t revision,struct ae_audio_output_ref ref,bool value) {CHECK(o && value);return effect(revision,ref,AE_AUDIO_MUTE);}
enum ae_audio_submit ae_audio_session_control_default(struct ae_audio_session_control *o,uint64_t revision,struct ae_audio_output_ref ref) {CHECK(o);return effect(revision,ref,AE_AUDIO_DEFAULT);}
int main(void) {
    struct ae_settings m;ae_settings_init(&m);struct ae_audio_session_control owner={0};
    current.audio=(struct ae_audio_snapshot){.revision=1,.state=AE_AUDIO_READY,.count=1};
    current.audio.outputs[0]=(struct ae_audio_output){.ref={91,2,3},.name="output",.volume=1,.volume_available=true,.is_default=true};
    CHECK(ae_settings_audio_pump(&m,&owner));uint64_t id=m.sound.ids[0];
    const enum ae_audio_command_kind kinds[]={AE_AUDIO_VOLUME,AE_AUDIO_MUTE,AE_AUDIO_DEFAULT};
    for(unsigned i=0;i<3;++i) {
        CHECK(ae_settings_sound_request(&m,id,kinds[i],0.5,true));
        CHECK(ae_settings_audio_pump(&m,&owner) && effects==i+1 && m.sound.feedback==AE_SETTINGS_SOUND_SUBMITTED);
        CHECK(ae_settings_audio_pump(&m,&owner) && effects==i+1);
        current.audio.command_state=AE_AUDIO_COMMAND_OBSERVED;++current.audio.revision;
        CHECK(ae_settings_audio_pump(&m,&owner) && m.sound.feedback==AE_SETTINGS_SOUND_OBSERVED);
    }
    CHECK(ae_settings_sound_request(&m,id,AE_AUDIO_DEFAULT,0,false));result=AE_AUDIO_STALE;
    CHECK(ae_settings_audio_pump(&m,&owner) && effects==4 && m.sound.feedback==AE_SETTINGS_SOUND_REJECTED);
    CHECK(ae_settings_audio_pump(&m,&owner) && effects==4);
    CHECK(ae_settings_sound_request(&m,id,AE_AUDIO_DEFAULT,0,false));observable=false;
    CHECK(!ae_settings_audio_pump(&m,&owner) && !m.sound.queued && !ae_settings_sound_enabled(&m) && effects==4);
    CHECK(m.sound.feedback==AE_SETTINGS_SOUND_REJECTED);
    observable=true;CHECK(ae_settings_audio_pump(&m,&owner) && m.sound.feedback==AE_SETTINGS_SOUND_REJECTED);
    CHECK(ae_settings_sound_request(&m,id,AE_AUDIO_DEFAULT,0,false));current.loss=AE_AUDIO_CONTROL_SESSION_LOST;
    CHECK(ae_settings_audio_pump(&m,&owner) && m.sound.fenced && effects==4);
    current.loss=0;CHECK(!ae_settings_audio_pump(&m,&owner) && effects==4);
    puts("Settings audio binding: original snapshots, exact effects, no replay and loss fence passed");return 0;
}
