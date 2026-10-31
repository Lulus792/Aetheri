#ifndef AETHERI_LINUX_LIFETIME_H
#define AETHERI_LINUX_LIFETIME_H
#include "linux/process.h"
#include "core/root.h"

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
/* Private observation handoff, not an IPC/persistent record format. Independently
 * protect the ORIGINAL expectation before transferring any descriptors. Never
 * derive it from incoming handles/names, a candidate directory or app packets.
 * Caller owes original profile/operation admission, trusted supervisor transport,
 * unchanged host PID/cgroup namespaces and continuous original unit retention.
 * Serialize old/new use: duplicates share the event reader's open-file state.
 * The four slots are pidfd, cgroup root, original group, original events reader.
 * No code/data views or error-channel writers are exported. */
#define AE_LIFETIME_HANDLE_COUNT 4
struct ae_lifetime_checkpoint {
    struct ae_process_context context;
    struct ae_process_identity process;
    struct ae_root_identity root,group;
    uint64_t events_inode,events_mount,pid_namespace,cgroup_namespace;
    uint32_t events_mode,events_uid,events_gid,pid;
};
/* Live original capture only. Output handles must all be -1; success owns four
 * CLOEXEC duplicates, failure changes neither output. This does not store them,
 * commit a handoff, acknowledge PID1 acceptance or relinquish the old observer. */
int ae_lifetime_export(struct ae_lifetime *,struct ae_lifetime_checkpoint *,int [AE_LIFETIME_HANDLE_COUNT]);
/* Borrow the exact received set and trusted original expectation. Success owns
 * private duplicates; failure consumes nothing. Already-dead originals work.
 * Unlike ordinary capture, this explicit path uses the exact independently bound
 * RETAINED root mount, allowing a new PrivateMounts namespace. It never substitutes
 * the new /sys/fs/cgroup mount for it. No journal adoption, readiness or release. */
int ae_lifetime_restore(sd_bus *,const struct ae_lifetime_checkpoint *,const int [AE_LIFETIME_HANDLE_COUNT],struct ae_lifetime **);
/* Read-only current end observation: original pidfd dead, original invocation
 * inactive/failed with no main process/job, and original recursive cgroup empty
 * or removed (missing name AND original kernfs reader deactivated/ENODEV).
 * No st_nlink or deleted-path-string inference. Repeat all observations;
 * substitutions latch ESTALE. EAGAIN means
 * not settled, never permission to stop/restart/free anything. Caller must close
 * future dispatch/migration and settle all external/delegated holders before
 * using this as part of full app drain. No SQLite/effect/release, no polling loop,
 * no owner-journal recovery or cached permanent proof. Output unchanged on error. */
int ae_lifetime_observe_end(struct ae_lifetime *,struct ae_lifetime_end *);
/* Local handles only, never unit Unref/stop, drain, lease release or revocation.
 * Fork use rejects ECHILD; callback-time close while observing rejects EBUSY. */
int ae_lifetime_close(struct ae_lifetime **);
#endif
