#ifndef AETHERI_AUDIO_REMOTE_H
#define AETHERI_AUDIO_REMOTE_H
/* One explicit local socket, no environment fallback or reconnect. Returns an
 * owned connected nonblocking CLOEXEC FD, or -1. Same UID is a sanity check,
 * not service/process admission. The caller still admits path/config/server.
 */
int ae_audio_remote_connect(const char *path);
/* Strict alternative: original_pidfd is borrowed for this call and must have
 * been independently admitted against current image/unit/context. Joins the
 * connected socket's SO_PEERPIDFD to that original live pidfs object, current
 * UID/GID and cgroup snapshots. Mandatory PIDFD_GET_INFO; no numeric-PID lookup,
 * fallback, reconnect or protocol bytes before the join. Both process FDs stay
 * internal and are closed before return; the caller's original is untouched.
 * A listener inherited/delegated from another creator is rejected. Caller still
 * owes path/configuration admission, no listener/accepted-FD delegation or exec
 * drift, ongoing lifetime fencing and production MAC. Not a bearer capability,
 * complete endpoint authorization or proof of who processes later messages.
 */
int ae_audio_remote_connect_original(const char *path,int original_pidfd);
#endif
