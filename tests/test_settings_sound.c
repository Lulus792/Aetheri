#include "settings/model.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do {if(!(x)) {fprintf(stderr,"sound model line %d: %s\n",__LINE__,#x);exit(1);}} while(0)
static struct ae_audio_snapshot sample(void) {
    struct ae_audio_snapshot s={.state=AE_AUDIO_READY,.revision=7,.count=2};
    s.outputs[0]=(struct ae_audio_output){.ref={33,101,4},.name="speakers",.description="Speakers",.volume=0.6,.volume_available=true,.is_default=true};
    s.outputs[1]=(struct ae_audio_output){.ref={33,102,5},.name="headphones",.description="Headphones",.volume=1,.volume_available=true};return s;
}
int main(void) {
    struct ae_settings m;ae_settings_init(&m);struct ae_audio_snapshot s=sample();
    CHECK(!ae_settings_sound_enabled(&m));CHECK(ae_settings_sound_snapshot(&m,&s,0));
    uint64_t speakers=m.sound.ids[0],headphones=m.sound.ids[1];
    CHECK(ae_settings_sound_enabled(&m) && speakers!=headphones);
    struct ae_audio_output swap=s.outputs[0];s.outputs[0]=s.outputs[1];s.outputs[1]=swap;++s.revision;
    CHECK(ae_settings_sound_snapshot(&m,&s,0) && m.sound.ids[0]==headphones && m.sound.ids[1]==speakers);
    CHECK(!ae_settings_sound_request(&m,0,AE_AUDIO_DEFAULT,0,false));
    CHECK(!ae_settings_sound_request(&m,speakers,AE_AUDIO_VOLUME,NAN,false));
    CHECK(!ae_settings_sound_request(&m,speakers,AE_AUDIO_VOLUME,1.1,false));
    CHECK(ae_settings_sound_request(&m,speakers,AE_AUDIO_VOLUME,0.4,false));
    CHECK(m.sound.observation.outputs[1].volume==0.6 && !ae_settings_sound_enabled(&m));
    struct ae_settings_sound_request request;
    CHECK(ae_settings_sound_take(&m,&request) && request.target.id==4 && request.revision==8 && request.volume==0.4);
    CHECK(!ae_settings_sound_take(&m,&request));ae_settings_sound_result(&m,AE_AUDIO_SUBMITTED);
    CHECK(m.sound.feedback==AE_SETTINGS_SOUND_SUBMITTED && !ae_settings_sound_enabled(&m));
    /* An older observed command must not acknowledge our new intent. */
    s.command_state=AE_AUDIO_COMMAND_OBSERVED;CHECK(ae_settings_sound_snapshot(&m,&s,0));
    CHECK(m.sound.feedback==AE_SETTINGS_SOUND_SUBMITTED);
    s.command=1;s.command_kind=request.kind;s.command_target=request.target;s.command_state=AE_AUDIO_COMMAND_PENDING;++s.revision;
    CHECK(ae_settings_sound_snapshot(&m,&s,0) && !ae_settings_sound_enabled(&m));
    s.command_state=AE_AUDIO_COMMAND_OBSERVED;s.outputs[1].volume=0.4;++s.revision;
    CHECK(ae_settings_sound_snapshot(&m,&s,0) && ae_settings_sound_enabled(&m) && m.sound.feedback==AE_SETTINGS_SOUND_OBSERVED);
    CHECK(ae_settings_sound_request(&m,headphones,AE_AUDIO_DEFAULT,0,false));
    CHECK(ae_settings_sound_take(&m,&request));ae_settings_sound_result(&m,AE_AUDIO_STALE);
    CHECK(m.sound.feedback==AE_SETTINGS_SOUND_REJECTED && !m.sound.observation.outputs[0].is_default);
    CHECK(!ae_settings_sound_take(&m,&request));
    struct ae_audio_snapshot invalid=s;invalid.outputs[0].volume=NAN;
    CHECK(!ae_settings_sound_snapshot(&m,&invalid,0) && m.sound.stale && !ae_settings_sound_enabled(&m));
    CHECK(ae_settings_sound_snapshot(&m,&s,0));invalid=s;invalid.outputs[0].name[0]='\n';
    CHECK(!ae_settings_sound_snapshot(&m,&invalid,0));CHECK(ae_settings_sound_snapshot(&m,&s,0));
    invalid=s;invalid.outputs[0].description[0]=(char)0xff;CHECK(!ae_settings_sound_snapshot(&m,&invalid,0));
    CHECK(ae_settings_sound_snapshot(&m,&s,0));invalid=s;invalid.outputs[0].ref=s.outputs[1].ref;
    CHECK(!ae_settings_sound_snapshot(&m,&invalid,0));CHECK(ae_settings_sound_snapshot(&m,&s,0));
    /* Numeric node reuse and remove/readd cannot inherit captured UI actions. */
    ++s.outputs[0].ref.serial;++s.revision;
    CHECK(ae_settings_sound_snapshot(&m,&s,0) && m.sound.ids[0]!=headphones);
    CHECK(!ae_settings_sound_request(&m,headphones,AE_AUDIO_DEFAULT,0,false));
    uint64_t replacement=m.sound.ids[0];s.count=0;++s.revision;CHECK(ae_settings_sound_snapshot(&m,&s,0));
    s.count=2;++s.revision;CHECK(ae_settings_sound_snapshot(&m,&s,0) && m.sound.ids[0]!=replacement);
    CHECK(ae_settings_sound_snapshot(&m,&m.sound.observation,0));
    CHECK(ae_settings_sound_request(&m,m.sound.ids[0],AE_AUDIO_MUTE,0,true));
    CHECK(ae_settings_sound_snapshot(&m,NULL,4) && m.sound.fenced && !m.sound.queued);
    CHECK(!ae_settings_sound_take(&m,&request) && !ae_settings_sound_snapshot(&m,&s,0));
    CHECK(!ae_settings_sound_enabled(&m) && m.sound.loss==4);
    ae_settings_init(&m);s=sample();CHECK(ae_settings_sound_snapshot(&m,&s,0));
    s.outputs[0].ref.connection=s.outputs[1].ref.connection=34;
    CHECK(!ae_settings_sound_snapshot(&m,&s,0) && m.sound.fenced);
    ae_settings_init(&m);s=sample();m.sound.next_id=UINT64_MAX-2;
    CHECK(!ae_settings_sound_snapshot(&m,&s,0));
    puts("sound model: frozen identities, validation, explicit intent, observed result and permanent loss passed");return 0;
}
