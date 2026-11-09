#ifndef AETHERI_AUDIO_SESSION_CONTROL_H
#define AETHERI_AUDIO_SESSION_CONTROL_H

#include "audio/control.h"
#include <systemd/sd-bus.h>
#include <sys/types.h>

enum ae_audio_control_loss {
    AE_AUDIO_CONTROL_LIVE=0,
    AE_AUDIO_CONTROL_MANAGER_LOST=1,
    AE_AUDIO_CONTROL_GRAPH_LOST=2,
    AE_AUDIO_CONTROL_POLICY_LOST=4,
    AE_AUDIO_CONTROL_SESSION_LOST=8,
    AE_AUDIO_CONTROL_BACKEND_LOST=16
};
struct ae_audio_session_profile {
    sd_bus *user_bus,*system_bus;
    uid_t uid;
    const char *seat,*remote,*graph_unit,*policy_unit;
    const char *graph_context,*policy_context;
    int manager_image,graph_image,policy_image,graph_pidfd,policy_pidfd;
};
struct ae_audio_session_snapshot {
    unsigned loss; /* First nonzero check is immutable, even after recovery. */
    struct ae_audio_snapshot audio;
};
struct ae_audio_session_control;

/* Calling-thread/process-only bridge between the session owner and audio
 * controls. The caller independently admits all profile inputs, protected
 * service/config/library provenance and the local bus/proc/cgroup views.
 * This opens read-only manager/actor/session observers before connecting to
 * the exact original graph. Inputs are borrowed only during open; observers
 * retain their own references/handles. No service start/stop or restart.
 *
 * Push context as thread-default before open and keep it pushed/iterating
 * until the caller's destroy notification. This object creates no thread or
 * timer. The owner must call snapshot on session/process notifications and
 * before painting controls. Each mutation independently checks again.
 * Success is a current observation, not an exec/delegation/mutation lock,
 * device grant, normal app admission or durable recovery transaction.
 * destroy takes ownership of data only after successful open.
 */
struct ae_audio_session_control *ae_audio_session_control_open(GMainContext *,
    const struct ae_audio_session_profile *,void *data,GDestroyNotify destroy);
/* On loss, fence all effects, clear selectable outputs and retire the backend.
 * An already submitted pending command becomes UNKNOWN, never cancelled or
 * replayed. Continue iterating context; closing the observer is not holder
 * drain, service stop, rollback or device revocation. No automatic reopening.
 */
bool ae_audio_session_control_snapshot(struct ae_audio_session_control *,
    struct ae_audio_session_snapshot *);
enum ae_audio_submit ae_audio_session_control_volume(struct ae_audio_session_control *,
    uint64_t,struct ae_audio_output_ref,double);
enum ae_audio_submit ae_audio_session_control_mute(struct ae_audio_session_control *,
    uint64_t,struct ae_audio_output_ref,bool);
enum ae_audio_submit ae_audio_session_control_default(struct ae_audio_session_control *,
    uint64_t,struct ae_audio_output_ref);
/* Clears slot on success. Async destruction can outlive this call: keep the
 * context alive until destroy. Invalid-thread calls leave slot/resources intact.
 */
bool ae_audio_session_control_close(struct ae_audio_session_control **);

#endif
