#ifndef AETHERI_SETTINGS_AUDIO_H
#define AETHERI_SETTINGS_AUDIO_H
#include "settings/model.h"
struct ae_audio_session_control;
/* Original session owner calls on its audio/session/process events, before
 * presentation and after user input. Borrow the original admitted handle on
 * its owning thread/context. No selection/open/reconnect, timer or process.
 * Consume at most one intent, once. Fresh backend checks still precede effects;
 * cached UI data is never admission. False marks observations stale/disabled.
 */
bool ae_settings_audio_pump(struct ae_settings *,struct ae_audio_session_control *);
#endif
