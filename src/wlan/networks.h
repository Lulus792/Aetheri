#ifndef AETHERI_WLAN_NETWORKS_H
#define AETHERI_WLAN_NETWORKS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define AE_WLAN_NETWORK_LIMIT 32
#define AE_WLAN_NAME_CAPACITY 129
#define AE_WLAN_NETWORK_FIELDS "synb"
#define AE_WLAN_NETWORK_REPLY "bba(" AE_WLAN_NETWORK_FIELDS ")"

/* Public wire values. Display names carry no association authority. */
enum ae_wlan_security {
    AE_WLAN_UNKNOWN, AE_WLAN_OPEN, AE_WLAN_PERSONAL, AE_WLAN_ENTERPRISE
};

struct ae_wlan_network {
    char name[AE_WLAN_NAME_CAPACITY];
    enum ae_wlan_security security;
    int16_t signal;
    bool connected;
};

struct ae_wlan_networks {
    size_t count;
    bool scanning, more;
    struct ae_wlan_network rows[AE_WLAN_NETWORK_LIMIT];
};

#endif
