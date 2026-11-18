#ifndef AETHERI_WLAN_DISCOVERY_H
#define AETHERI_WLAN_DISCOVERY_H

#include "linux/peer.h"
#include "wlan/networks.h"

#define AE_IWD_BUS "net.connman.iwd"
#define AE_IWD_CONTEXT "system_u:system_r:aetheri_wlan_t:s0"
#define AE_WLAN_PATH_CAPACITY 256

struct ae_wlan_discovery {
    struct ae_wlan_networks visible;
    char paths[AE_WLAN_NETWORK_LIMIT][AE_WLAN_PATH_CAPACITY];
};

enum ae_wlan_discovery_action { AE_WLAN_SCAN, AE_WLAN_LIST, AE_WLAN_SELECT };

/* Service and caller are independently admitted by the controller. A scan
 * checks the caller again before each radio request. No output survives a
 * failed observation. The caller rechecks its session before any reply. */
int ae_wlan_discover(sd_bus *,const struct ae_peer *,struct ae_wlan_discovery *,
                     enum ae_wlan_discovery_action,bool (*authorized)(void *),void *);

#endif
