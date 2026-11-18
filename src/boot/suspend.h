#ifndef AETHERI_BOOT_SUSPEND_H
#define AETHERI_BOOT_SUSPEND_H

#include <poll.h>

struct ae_suspend_server;
struct ae_unlock_broker;
enum { AE_SUSPEND_SERVER_FDS = 2 };

int ae_suspend_server_open(struct ae_suspend_server **);
void ae_suspend_server_poll(const struct ae_suspend_server *, struct pollfd [AE_SUSPEND_SERVER_FDS]);
void ae_suspend_server_step(struct ae_suspend_server *, struct ae_unlock_broker *);
void ae_suspend_server_close(struct ae_suspend_server **);

#endif
