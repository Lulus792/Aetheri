#ifndef AETHERI_AUDIO_CONTROL_H
#define AETHERI_AUDIO_CONTROL_H

#include "audio/types.h"
#include <glib.h>

/* Development backend, not a permission/grant or installed management API.
 * All calls and context iteration belong to the creating thread. Push context
 * as that thread's default before open and keep it pushed until destroy; nested
 * upstream GTask operations capture this context as well. Remote is one
 * explicit absolute socket path; no fallback/reconnect. The caller admits the
 * server/configuration. Continue iterating context after close until destroy is
 * called, so cancelled asynchronous work can retire. No child process/thread.
 */
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
