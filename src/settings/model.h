#ifndef AETHERI_SETTINGS_MODEL_H
#define AETHERI_SETTINGS_MODEL_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "audio/types.h"

/* Transient display observations, never saved device identities or authority. */
#define AE_SETTINGS_DISPLAYS 16
struct ae_settings_display {
    uint32_t id;
    char name[128];
    int width, height;
    float scale, refresh;
    bool mode_known;
};
enum ae_settings_page { AE_SETTINGS_DISPLAY, AE_SETTINGS_APPEARANCE, AE_SETTINGS_LANGUAGE, AE_SETTINGS_SOUND };
enum ae_settings_sound_feedback {AE_SETTINGS_SOUND_IDLE,AE_SETTINGS_SOUND_QUEUED,
    AE_SETTINGS_SOUND_SUBMITTED,AE_SETTINGS_SOUND_OBSERVED,AE_SETTINGS_SOUND_REJECTED,AE_SETTINGS_SOUND_UNKNOWN};
struct ae_settings_sound_request {
    enum ae_audio_command_kind kind;
    uint64_t revision;
    struct ae_audio_output_ref target;
    double volume;
    bool mute;
};
struct ae_settings_sound {
    struct ae_audio_snapshot observation;
    uint64_t ids[AE_AUDIO_OUTPUTS_MAX],next_id,connection,baseline_command;
    bool attached,stale,fenced,queued,dispatching;
    unsigned loss;
    enum ae_settings_sound_feedback feedback;
    struct ae_settings_sound_request request;
};
enum ae_settings_theme { AE_SETTINGS_DARK, AE_SETTINGS_LIGHT, AE_SETTINGS_CONTRAST };
struct ae_settings {
    struct ae_settings_display displays[AE_SETTINGS_DISPLAYS];
    size_t count, selected;
    unsigned refreshes;
    bool stale, german;
    enum ae_settings_page page;
    enum ae_settings_theme theme;
    unsigned text_percent;
    struct ae_settings_sound sound;
};
void ae_settings_init(struct ae_settings *model);
/* Failed snapshots preserve the previous observation and mark it stale. */
bool ae_settings_snapshot(struct ae_settings *model, const struct ae_settings_display *displays, size_t count);
bool ae_settings_select(struct ae_settings *model, uint32_t id);
/* Snapshot input must come from an admitted original session owner. No socket,
 * caller, service, device or process is authenticated by this display model.
 * Invalid snapshots retain old observations but disable effects. IDs belong
 * to this window, follow complete live refs and never rebind to replacements.
 * First owner/backend loss fences the model permanently; no reconnect here. */
bool ae_settings_sound_snapshot(struct ae_settings *,const struct ae_audio_snapshot *,unsigned loss);
bool ae_settings_sound_enabled(const struct ae_settings *);
/* Enqueue one explicit UI intent, without optimistic device state. take is
 * destructive, at most once; a lost reply must never replay it. The host feeds
 * the actual submit result and subsequent original-owner observations. */
bool ae_settings_sound_request(struct ae_settings *,uint64_t output_id,
    enum ae_audio_command_kind,double volume,bool mute);
bool ae_settings_sound_take(struct ae_settings *,struct ae_settings_sound_request *);
void ae_settings_sound_result(struct ae_settings *,enum ae_audio_submit);
#endif
