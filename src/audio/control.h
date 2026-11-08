#ifndef AETHERI_AUDIO_CONTROL_H
#define AETHERI_AUDIO_CONTROL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <glib.h>

/* Development backend, not a permission/grant or installed management API.
 * All calls and context iteration belong to the creating thread. Push context
 * as that thread's default before open and keep it pushed until destroy; nested
 * upstream GTask operations capture this context as well. Remote is one
 * explicit absolute socket path; no fallback/reconnect. The caller admits the
 * server/configuration. Continue iterating context after close until destroy is
 * called, so cancelled asynchronous work can retire. No child process/thread.
 */
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
struct ae_audio_control;
typedef void (*ae_audio_changed)(const struct ae_audio_snapshot *,void *);

/* Snapshots are borrowed for the callback only; copy to retain. destroy owns
 * data only after successful open. Output refs are connection-local observations,
 * never durable identity or bearer authority. Mutations require the exact latest
 * revision/ref. SUBMITTED is not acknowledgement; OBSERVED means effective state
 * read back after a server barrier, not persistence/audibility or causal proof.
 * Volume changes set all channels equally in [0,1], preserving mute. Mute keeps
 * volume. One command at a time; timeout/disconnect yields UNKNOWN, no replay.
 * Closing retires this observer; it cannot undo/cancel a submitted change.
 * Owned cache subscriptions are disabled before a final same-connection barrier.
 * That local wait is bounded to five seconds, then the original socket closes.
 */
struct ae_audio_control *ae_audio_control_open(GMainContext *,const char *,ae_audio_changed,void *,GDestroyNotify);
/* Strict original-listener variant; borrowed independently admitted live pidfd
 * is used synchronously during open, never selected from socket credentials.
 * See audio/remote.h for the remaining admission/delegation obligations. A
 * failed join yields DISCONNECTED/ENDPOINT, with no protocol connection or
 * fallback. Negative original_pidfd is invalid, never the legacy profile. */
struct ae_audio_control *ae_audio_control_open_original(GMainContext *,const char *,int original_pidfd,ae_audio_changed,void *,GDestroyNotify);
bool ae_audio_control_snapshot(struct ae_audio_control *,struct ae_audio_snapshot *);
enum ae_audio_submit ae_audio_control_volume(struct ae_audio_control *,uint64_t,struct ae_audio_output_ref,double);
enum ae_audio_submit ae_audio_control_mute(struct ae_audio_control *,uint64_t,struct ae_audio_output_ref,bool);
enum ae_audio_submit ae_audio_control_default(struct ae_audio_control *,uint64_t,struct ae_audio_output_ref);
bool ae_audio_control_close(struct ae_audio_control **);

#endif
