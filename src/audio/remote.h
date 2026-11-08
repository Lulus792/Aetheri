#ifndef AETHERI_AUDIO_REMOTE_H
#define AETHERI_AUDIO_REMOTE_H
/* One explicit local socket, no environment fallback or reconnect. Returns an
 * owned connected nonblocking CLOEXEC FD, or -1. Same UID is a sanity check,
 * not service/process admission. The caller still admits path/config/server.
 */
int ae_audio_remote_connect(const char *path);
#endif
