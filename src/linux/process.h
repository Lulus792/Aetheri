#ifndef AETHERI_LINUX_PROCESS_H
#define AETHERI_LINUX_PROCESS_H

#include "linux/peer.h"

/* Host PID namespace, unified cgroup v2, trusted /proc and /sys/fs/cgroup.
 * A bounded observation of one live supervised invocation, not admission rights.
 */
struct ae_process_context {
    uint8_t boot[16], invocation[16];
    uint64_t cgroup_id;
    uid_t uid;
    char unit[256], cgroup[1024], label[512];
};

/* Borrow a successful ae_peer_capture and trusted host SYSTEM bus. Uses the held
 * bus ProcessFD, never pidfd_open(connection_pid). Require current UID/label,
 * systemd GetUnitByPIDFD, active system .service invocation and exact cgroup
 * inode agreement.
 * Repeat observations under liveness checks; changed context fails closed.
 * Output unchanged on error. No cache, process control or ownership transfer.
 * This is a snapshot, not a lock against future exit/exec/migration. Callers owe
 * policy preventing app credential/domain/cgroup changes and FD/ptrace attacks.
 */
int ae_process_observe(const struct ae_peer *peer, sd_bus *system_bus,
                       struct ae_process_context *output);
/* The only local pidfd_open path: pin this calling process itself. Never use
 * this to replace a caller's required bus-authenticated ProcessFD.
 */
int ae_process_observe_self(sd_bus *system_bus, struct ae_process_context *output);
/* Expected must come from the trusted supervisor/launch registry, never an app.
 * Matching does not itself register a broker/app, verify target exec, check a
 * catalogue gate or authorize a document. Recheck at admission, not once forever.
 */
int ae_process_verify(const struct ae_peer *peer, sd_bus *system_bus,
                      const struct ae_process_context *expected);

#endif
