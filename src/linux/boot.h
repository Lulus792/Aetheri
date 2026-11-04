#ifndef AETHERI_LINUX_BOOT_H
#define AETHERI_LINUX_BOOT_H
#include <stddef.h>
#include <stdint.h>

struct ae_boot;
struct ae_boot_context {
    uint8_t boot[16];
    uint64_t pid_namespace,cgroup_namespace,time_namespace;
};
enum { AE_BOOT_CONTEXT_BYTES=48 };
/* AETHBOT1 is observation metadata, never machine/volume/clone authentication,
 * chronological boot order, original-process end, holder drain or a capability.
 * Namespace numbers are local kernel observations and need not survive close.
 * All outputs remain unchanged on errors, including malformed input. */
int ae_boot_context_encode(const struct ae_boot_context *,uint8_t [AE_BOOT_CONTEXT_BYTES]);
int ae_boot_context_decode(const uint8_t *,size_t,struct ae_boot_context *);

/* Trusted single-threaded host setup, with independently admitted /proc and
 * PID/cgroup/time namespaces. This API cannot establish initial/host scope from
 * a candidate's own namespace IDs. No numeric-PID open or PID1 namespace reads.
 * Read-only: hold exact proc root/boot file/self namespace objects, compare raw
 * boot_id to libsystemd, and require unchanged current/child namespace bindings.
 * Output must start NULL. No database, policy, signal, bus or process effect. */
int ae_boot_capture(struct ae_boot **);
int ae_boot_observe(struct ae_boot *,struct ae_boot_context *);
/* Observe a different UUID from independently retained original provenance.
 * Equal -> EALREADY. Different is NOT proof that an older kernel ended: caller
 * still owes same-machine/catalogue/volume and snapshot/clone provenance, actor
 * admission and future-effect/holder fencing. Never release a lease from this
 * observation alone, or turn old process IDs into new live handles. */
int ae_boot_observe_other(struct ae_boot *,const uint8_t original_boot[16],struct ae_boot_context *);
/* Local FD cleanup only; inherited fork handles reject ECHILD. */
int ae_boot_close(struct ae_boot **);
#endif
