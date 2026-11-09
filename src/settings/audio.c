#include "settings/audio.h"
#include "audio/session_control.h"

static bool observe(struct ae_settings *m,struct ae_audio_session_control *owner) {
    struct ae_audio_session_snapshot snapshot;
    if(!owner || !ae_audio_session_control_snapshot(owner,&snapshot)) {
        (void)ae_settings_sound_snapshot(m,NULL,0);return false;
    }
    return ae_settings_sound_snapshot(m,&snapshot.audio,snapshot.loss);
}
bool ae_settings_audio_pump(struct ae_settings *m,struct ae_audio_session_control *owner) {
    if(!m || !observe(m,owner)) return false;
    struct ae_settings_sound_request request;
    if(!ae_settings_sound_take(m,&request)) return true;
    enum ae_audio_submit result=AE_AUDIO_INVALID;
    switch(request.kind) {
        case AE_AUDIO_VOLUME:result=ae_audio_session_control_volume(owner,request.revision,request.target,request.volume);break;
        case AE_AUDIO_MUTE:result=ae_audio_session_control_mute(owner,request.revision,request.target,request.mute);break;
        case AE_AUDIO_DEFAULT:result=ae_audio_session_control_default(owner,request.revision,request.target);break;
    }
    ae_settings_sound_result(m,result);
    return observe(m,owner);
}
