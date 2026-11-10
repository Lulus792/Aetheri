#ifndef AETHERI_LINUX_SESSION_H
#define AETHERI_LINUX_SESSION_H
#include <poll.h>
#include <stdint.h>
#include <sys/types.h>

struct ae_session;
enum ae_session_loss {
    AE_SESSION_LIVE=0,
    AE_SESSION_CONTEXT_LOST=1,
    AE_SESSION_INACTIVE=2,
    AE_SESSION_OBSERVATION_FAILED=4
};
/* Main thread of the calling process only, ordinary equal real/effective UID.
   The self pidfd's credential snapshot belongs to that thread-group leader.
   Trusted host
   proc/cgroup/logind views and policy preventing context manipulation are caller
   prerequisites. Captures a self pidfd, kernel credentials/cgroup ID and exact
   original session/scope/cgroup strings; remote/inactive/non-Wayland/non-user
   sessions reject. This is read-only observation, not authentication, remote
   ProcessFD admission, an app grant, durable identity or a process/lease drain.
   *out starts NULL and stays unchanged on error. No thread or daemon is added. */
int ae_session_open(uid_t expected_uid,const char *expected_seat,struct ae_session **out);
/* Rechecks even with no monitor event. The first observed loss is immutable;
   returning to the VT never rearms this handle. 0 writes the observed mask;
   negative errno (invalid arguments/thread/fork) leaves output/state unchanged.
   Observations are not atomic locks against future context/device changes. */
int ae_session_check(struct ae_session *,unsigned *loss);
/* Borrow one monitor hint and absolute CLOCK_MONOTONIC microsecond deadline
   (UINT64_MAX: none). Never consume/change/close/transfer its FD. A wake/deadline
   requires check; it is not admission. Lost/error returns negative errno with
   outputs unchanged. A monitor query failure latches OBSERVATION_FAILED. */
int ae_session_poll(struct ae_session *,struct pollfd *,uint64_t *deadline_us);
/* Same creating process/thread. Close only local observations; no stop,
   revocation, rollback or recovery. Wrong context preserves the live pointer. */
int ae_session_close(struct ae_session **);
#endif
