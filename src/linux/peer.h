#ifndef AETHERI_LINUX_PEER_H
#define AETHERI_LINUX_PEER_H

#include <stdint.h>
#include <sys/types.h>
#include <systemd/sd-bus.h>

struct ae_peer {
    uint8_t bus_id[16];
    char unique_name[256];
    char connection_label[512];
    uid_t connection_uid;
    pid_t connection_pid;
    int pidfd;
};

#define AE_PEER_INIT { .pidfd = -1 }

/* Capture a received method call's bus-authenticated connection credentials.
 * Caller supplies a trusted local bus connection; this does not authenticate
 * an arbitrary bus server/address supplied by an app. Borrow message.
 * On success output owns one CLOEXEC process FD; output must
 * not already own a descriptor. On failure output is unchanged. No /proc
 * augmentation, numeric-PID reopening, same-UID authorization or weaker fallback.
 * This is NOT app/run authorization: the caller must still check current label,
 * supervised invocation/cgroup, registry, policy and admission under this pin.
 */
int ae_peer_capture(sd_bus_message *message, struct ae_peer *output);
/* Zero-time liveness observation, not a promise of future life or exec identity. */
int ae_peer_alive(const struct ae_peer *peer);
/* Clear an initialized capture. Does not stop/signal the process. */
void ae_peer_clear(struct ae_peer *peer);

#endif
