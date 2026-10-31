#ifndef AETHERI_LINUX_LIFETIME_H
#define AETHERI_LINUX_LIFETIME_H
#include "linux/process.h"

struct ae_lifetime;
struct ae_lifetime_end {
    uint8_t boot[16],invocation[16];
    uint64_t cgroup_id;
    uint32_t pid;
    int group_removed,unit_failed;
};
/* Root-owned, single-threaded observer of the ORIGINAL live service leader.
 * Borrow the trusted host system bus. Caller independently holds the original
 * supervisor unit reference for the entire lifetime; this object neither takes
 * nor releases another reference on that shared connection. Context must come
 * from actual admitted peer/unit/image evidence, never an app-selected path.
 * Capture verifies it again, duplicates the actual ProcessFD and holds exact
 * cgroup-v2 root/group objects with non-recycled mount identities and the
 * original cgroup.events reader. Output must start NULL. */
int ae_lifetime_capture(sd_bus *,const struct ae_peer *,const struct ae_process_context *,struct ae_lifetime **);
/* Read-only current end observation: original pidfd dead, original invocation
 * inactive/failed with no main process/job, and original recursive cgroup empty
 * or removed (missing name AND original kernfs reader deactivated/ENODEV).
 * No st_nlink or deleted-path-string inference. Repeat all observations;
 * substitutions latch ESTALE. EAGAIN means
 * not settled, never permission to stop/restart/free anything. Caller must close
 * future dispatch/migration and settle all external/delegated holders before
 * using this as part of full app drain. No SQLite/effect/release, no polling loop,
 * no restart recovery or cached permanent proof. Output unchanged on error. */
int ae_lifetime_observe_end(struct ae_lifetime *,struct ae_lifetime_end *);
/* Local handles only, never unit Unref/stop, drain, lease release or revocation.
 * Fork use rejects ECHILD; callback-time close while observing rejects EBUSY. */
int ae_lifetime_close(struct ae_lifetime **);
#endif
