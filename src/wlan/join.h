#ifndef AETHERI_WLAN_JOIN_H
#define AETHERI_WLAN_JOIN_H

#include "wlan/discovery.h"

#define AE_WLAN_JOIN_USEC UINT64_C(28000000)

/* Takes a dedicated bus on the already verified system broker. The caller
 * closes it at return, retiring the agent and any outstanding method slot. */
int ae_wlan_join(sd_bus *,const struct ae_peer *,const char *,enum ae_wlan_security,
                  const char *,bool (*authorized)(void *),void *);

#endif
