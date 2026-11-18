#ifndef AETHERI_SETTINGS_WLAN_H
#define AETHERI_SETTINGS_WLAN_H

#include "settings/model.h"
#include <systemd/sd-bus.h>

/* Owned by one window. The bus and wait descriptor exist only during a
 * request and its bounded readback. A queued change is consumed once. */
struct ae_settings_wlan_client {
    struct ae_settings_wlan *model;
    sd_bus *bus;
    uint64_t deadline, next_read;
    int wait_fd, parent_fd;
    enum ae_settings_wlan_request sent;
    enum ae_settings_wlan_state target, reply_state;
    bool reply_ready, refused, drain;
};

int ae_settings_wlan_prepare(struct ae_settings_wlan_client *,int parent_fd,uint32_t *timeout,int *dirty);
bool ae_settings_wlan_process(struct ae_settings_wlan_client *);
void ae_settings_wlan_close(struct ae_settings_wlan_client *);

#endif
