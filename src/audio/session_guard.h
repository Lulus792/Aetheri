#ifndef AETHERI_AUDIO_SESSION_GUARD_H
#define AETHERI_AUDIO_SESSION_GUARD_H

#include <sys/types.h>
#include <poll.h>
#include <stdbool.h>
#include <stdint.h>

enum ae_audio_session_loss {
    AE_AUDIO_SESSION_LIVE=0,
    AE_AUDIO_SESSION_INACTIVE=1,
    AE_AUDIO_SESSION_SERVER_EXIT=2,
    AE_AUDIO_SESSION_POLICY_EXIT=4,
    AE_AUDIO_SESSION_OBSERVATION_FAILED=8
};
struct ae_audio_session_guard;

/* Read-only, calling-thread observer for one original local Wayland session.
 * expected_uid/seat and both distinct live pidfds must already be independently
 * admitted by the caller. The guard duplicates the FDs; it cannot authenticate
 * images, units, callers, a manager or endpoint. Trusted logind/proc views are
 * prerequisites. No thread, callback, signal, RPC, reconnect or restart is made.
 * A loss is latched: returning to the same VT never restores LIVE. The caller
 * must fence new work and coordinate stop with its original manager. This is
 * not device/FD revocation, general holder drain or production app admission.
 */
struct ae_audio_session_guard *ae_audio_session_guard_open(uid_t expected_uid,
    const char *expected_seat,int server_pidfd,int policy_pidfd);
/* timeout_ms is 0..1000. Any observation error latches failure. Monitor events
 * are hints followed by a fresh check of the original session. Timeout alone
 * is not loss. Once nonzero, the first loss mask remains immutable. */
unsigned ae_audio_session_guard_wait(struct ae_audio_session_guard *,int timeout_ms);
/* Borrow monitor/original process FDs for poll, and the monitor's absolute
 * CLOCK_MONOTONIC microsecond deadline (UINT64_MAX: none). Never consume, close,
 * change or transfer these FDs. Same calling thread/process; a wake is only a
 * hint followed by wait(0). Output is unchanged on failure. */
bool ae_audio_session_guard_poll(struct ae_audio_session_guard *,struct pollfd out[3],uint64_t *deadline_us);
void ae_audio_session_guard_close(struct ae_audio_session_guard **);

#endif
