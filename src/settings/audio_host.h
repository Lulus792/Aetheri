#ifndef AETHERI_SETTINGS_AUDIO_HOST_H
#define AETHERI_SETTINGS_AUDIO_HOST_H
#include "settings/model.h"
#include <glib.h>
struct ae_audio_session_control;
struct ae_settings_audio_host;
/* Calling thread/process owns a pushed thread-default context and an already
 * independently admitted original session handle. Both outlive this host.
 * No admission, service/device mutation, restart or new thread is performed. */
struct ae_settings_audio_host *ae_settings_audio_host_open(GMainContext *,struct ae_audio_session_control *);
bool ae_settings_audio_host_bind(struct ae_settings_audio_host *,struct ae_settings *);
/* One prepare/finish pair surrounds the native window wait. prepare returns an
 * owned internal wake multiplexer, borrowed until finish; do not read/close it.
 * It combines GLib sources, original process/session hints and the caller's
 * signal pipe. Timeout is milliseconds; UINT32_MAX means infinite. GLib is
 * acquired across the wait, checked/dispatched afterward, then released.
 * Finish or abort is mandatory even if the native window wait fails. */
int ae_settings_audio_host_prepare(struct ae_settings_audio_host *,int signal_fd,uint32_t *timeout_ms);
bool ae_settings_audio_host_finish(struct ae_settings_audio_host *,bool *dirty);
/* Retire only the wait registration, without dispatch or another UI intent. */
bool ae_settings_audio_host_abort(struct ae_settings_audio_host *);
/* Invoke after UI input too. Failed observations disable the model rather than
 * reconnecting or replaying. dirty is only ever set, never cleared. */
bool ae_settings_audio_host_update(struct ae_settings_audio_host *,bool *dirty);
/* Reject during an outstanding prepare/finish pair. Drops this adapter only;
 * caller still closes the original owner and iterates its context to retirement. */
bool ae_settings_audio_host_close(struct ae_settings_audio_host **);
#endif
