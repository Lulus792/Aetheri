#ifndef AETHERI_LOGIN_LOADED_INTERNAL_H
#define AETHERI_LOGIN_LOADED_INTERNAL_H
#include "login/loaded.h"

enum { AE_LOGIN_OBSERVATION_BYTES=296 };
/* Capture performs the administrative current-image and consumed-config check.
   Compare accepts ONLY bytes already authenticated by the protected handoff
   file adapter. Neither raw bytes nor matching content grant authority. */
int ae_login_loaded_capture(struct ae_login_loaded *,uint8_t out[AE_LOGIN_OBSERVATION_BYTES]);
int ae_login_loaded_compare(struct ae_login_loaded *,const uint8_t[AE_LOGIN_OBSERVATION_BYTES]);
#endif
