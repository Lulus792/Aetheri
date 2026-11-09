#ifndef AETHERI_SETTINGS_NEXAUI_MAIN_H
#define AETHERI_SETTINGS_NEXAUI_MAIN_H
struct ae_settings_audio_host;
/* Original host entry. The ordinary executable passes NULL. Only the explicit
 * audio-linked host build accepts an independently admitted original owner;
 * no CLI/socket/PID option grants admission. The host outlives this call. */
int ae_settings_nexaui_run(int argc,char **argv,struct ae_settings_audio_host *);
#endif
