#ifndef AETHERI_WLAN_NETWORKS_H
#define AETHERI_WLAN_NETWORKS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define AE_WLAN_NETWORK_LIMIT 32
#define AE_WLAN_NAME_CAPACITY 129
#define AE_WLAN_NETWORK_FIELDS "synb"
#define AE_WLAN_NETWORK_REPLY "bba(" AE_WLAN_NETWORK_FIELDS ")"
#define AE_WLAN_SELECTION_FIELDS "t" AE_WLAN_NETWORK_FIELDS
#define AE_WLAN_SELECTION_REPLY "bba(" AE_WLAN_SELECTION_FIELDS ")"
#define AE_WLAN_PASSPHRASE_CAPACITY 64

static inline bool ae_wlan_passphrase_valid(const char *passphrase,size_t size) {
    if(!passphrase || size<8 || size>=AE_WLAN_PASSPHRASE_CAPACITY) return false;
    for(size_t i=0;i<size;++i)
        if((unsigned char)passphrase[i]<32 || (unsigned char)passphrase[i]>126) return false;
    return true;
}

/* Public wire values. Display names carry no association authority. */
enum ae_wlan_security {
    AE_WLAN_UNKNOWN, AE_WLAN_OPEN, AE_WLAN_PERSONAL, AE_WLAN_ENTERPRISE
};

struct ae_wlan_network {
    uint64_t selection;
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
