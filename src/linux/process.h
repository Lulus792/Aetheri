#ifndef AETHERI_LINUX_PROCESS_H
#define AETHERI_LINUX_PROCESS_H

#include "linux/peer.h"
#include <stddef.h>

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
/* Registration may pin a known service while its Type=notify startup is still
 * activating. This does NOT establish active admission; verify still requires
 * active state. Use only after selecting a protected supervisor slot.
 */
int ae_process_observe_registration(const struct ae_peer *peer, sd_bus *system_bus,
                                    struct ae_process_context *output);
int ae_process_context_equal(const struct ae_process_context *a, const struct ae_process_context *b);
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

/* Stable process identity within one boot, captured only from an authenticated
 * live ProcessFD after checking the independently admitted current context.
 * This is maintenance evidence, never a bearer credential, a readiness/drain
 * result or permission to adopt a process/old run after owner replacement.
 * Requires the accepted 64-bit pidfs kernel: mandatory PIDFD_GET_INFO/current
 * context checks bracket fstat of the same held FD. No weaker kernel fallback.
 * Persist it with the original context/run in protected owner state BEFORE
 * relying on it for recovery. Do not reconstruct it from a PID or app bytes. */
enum { AE_PROCESS_IDENTITY_BYTES = 32 };
struct ae_process_identity { uint8_t boot[16]; uint64_t pidfd_id; };
int ae_process_identity_encode(const struct ae_process_identity *,uint8_t [AE_PROCESS_IDENTITY_BYTES]);
int ae_process_identity_decode(const uint8_t *,size_t,struct ae_process_identity *);
int ae_process_identify(const struct ae_peer *,sd_bus *,const struct ae_process_context *,struct ae_process_identity *);
/* The replacement observer supplies a fresh BUS-authenticated ProcessFD and
 * the independently retained original context/identity. New bus unique names
 * are permitted, but the exact original live kernel process and current
 * UID/label/unit/invocation/cgroup must agree. No pidfd_open(PID) fallback.
 * This does not recover the old bus connection, executable, leases, catalogue
 * runtime or vanished cgroup handles. Recheck image/admission separately.
 * Outputs unchanged on errors; no new handles, effects, retries or polling. */
int ae_process_reidentify(const struct ae_process_identity *,const struct ae_peer *,sd_bus *,const struct ae_process_context *);

#endif
