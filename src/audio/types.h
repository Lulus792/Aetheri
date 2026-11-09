#ifndef AETHERI_AUDIO_TYPES_H
#define AETHERI_AUDIO_TYPES_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Copied connection-local observations. No library/renderer types, durable
 * device identity, permission or installed wire contract. */
#define AE_AUDIO_OUTPUTS_MAX 64
enum ae_audio_state {AE_AUDIO_CONNECTING,AE_AUDIO_READY,AE_AUDIO_FAILED,AE_AUDIO_DISCONNECTED};
enum ae_audio_error {AE_AUDIO_ERROR_NONE,AE_AUDIO_ERROR_PLUGINS,AE_AUDIO_ERROR_DEADLINE,AE_AUDIO_ERROR_ID,AE_AUDIO_ERROR_SERIAL,AE_AUDIO_ERROR_NAME,AE_AUDIO_ERROR_DESCRIPTION,AE_AUDIO_ERROR_VOLUME,AE_AUDIO_ERROR_LIMIT,AE_AUDIO_ERROR_SYNC,AE_AUDIO_ERROR_VERSION,AE_AUDIO_ERROR_ENDPOINT};
enum ae_audio_command_state {AE_AUDIO_COMMAND_NONE,AE_AUDIO_COMMAND_PENDING,AE_AUDIO_COMMAND_OBSERVED,AE_AUDIO_COMMAND_UNKNOWN};
enum ae_audio_command_kind {AE_AUDIO_VOLUME,AE_AUDIO_MUTE,AE_AUDIO_DEFAULT};
enum ae_audio_submit {AE_AUDIO_SUBMITTED,AE_AUDIO_INVALID,AE_AUDIO_STALE,AE_AUDIO_UNAVAILABLE,AE_AUDIO_BUSY};
struct ae_audio_output_ref {uint64_t connection,serial;uint32_t id;};
struct ae_audio_output {
    struct ae_audio_output_ref ref;
    char name[256],description[512];
    bool is_default,volume_available,muted;
    double volume; /* WirePlumber cubic scale; observed values can exceed 1. */
};
struct ae_audio_snapshot {
    uint64_t revision;
    enum ae_audio_state state;
    enum ae_audio_error error;
    size_t count;
    struct ae_audio_output outputs[AE_AUDIO_OUTPUTS_MAX];
    uint64_t command;
    enum ae_audio_command_state command_state;
    enum ae_audio_command_kind command_kind;
    struct ae_audio_output_ref command_target;
};
#endif
