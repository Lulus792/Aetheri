#ifndef AETHERI_WLAN_DISCOVERY_H
#define AETHERI_WLAN_DISCOVERY_H

#include "linux/peer.h"
#include "wlan/networks.h"

#define AE_IWD_BUS "net.connman.iwd"
#define AE_IWD_CONTEXT "system_u:system_r:aetheri_wlan_t:s0"

/* Service and caller are independently admitted by the controller. A scan
 * checks the caller again before each radio request. No output survives a
 * failed observation. The caller rechecks its session before any reply. */
int ae_wlan_discover(sd_bus *,const struct ae_peer *,struct ae_wlan_networks *,
                     bool start_scan,bool (*authorized)(void *),void *);

#endif
