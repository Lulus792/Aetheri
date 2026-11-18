#ifndef AETHERI_BOOT_SUSPEND_LINK_H
#define AETHERI_BOOT_SUSPEND_LINK_H

#include <stdbool.h>
#include <stdint.h>
#include <sys/stat.h>

#define AE_SUSPEND_GUARD_LABEL "system_u:system_r:aetheri_suspend_guard_t:s0"
#define AE_SUSPEND_DESKTOP_LABEL "system_u:system_r:aetheri_session_t:s0"
enum ae_suspend_message { AE_SUSPEND_HELLO = 1, AE_SUSPEND_PREPARE = 2, AE_SUSPEND_READY = 3 };

struct ae_suspend_peer {
    int fd, pin;
    uid_t uid;
    gid_t gid;
    uint32_t pid;
    uint64_t cgroup;
    struct stat identity;
};
#define AE_SUSPEND_PEER_INIT {.fd = -1, .pin = -1}

int ae_suspend_listen(void);
int ae_suspend_connect(void);
/* Takes the connected socket only on success. The caller separately checks
   the live seat and decides what the authenticated message may authorize. */
int ae_suspend_peer_open(struct ae_suspend_peer *, int, uid_t, const char *);
void ae_suspend_peer_close(struct ae_suspend_peer *);
bool ae_suspend_peer_live(const struct ae_suspend_peer *);
int ae_suspend_send(const struct ae_suspend_peer *, enum ae_suspend_message);
int ae_suspend_receive(const struct ae_suspend_peer *, enum ae_suspend_message);

#endif
