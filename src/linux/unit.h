#ifndef AETHERI_LINUX_UNIT_H
#define AETHERI_LINUX_UNIT_H
#include "linux/start.h"
#include "core/root.h"
#include "core/unit_custody.h"
#include <linux/stat.h>

enum { AE_UNIT_MAX=8,AE_UNIT_ARGS=8,AE_UNIT_TEXT=256,AE_UNIT_FILE_MAX=16384,AE_UNIT_DROPINS=4 };
enum ae_unit_kind { AE_UNIT_SERVICE=1,AE_UNIT_TARGET,AE_UNIT_SLICE,AE_UNIT_SYSTEM_SLICE,AE_UNIT_ROOT_SLICE };
enum ae_unit_edge { AE_UNIT_REQUIRES,AE_UNIT_WANTS,AE_UNIT_REQUISITE,AE_UNIT_BINDS_TO,AE_UNIT_PART_OF,
    AE_UNIT_UPHOLDS,AE_UNIT_CONFLICTS,AE_UNIT_ON_FAILURE,AE_UNIT_ON_SUCCESS,AE_UNIT_TRIGGERS,
    AE_UNIT_TRIGGERED_BY,AE_UNIT_STOP_TO,AE_UNIT_RELOAD_TO,AE_UNIT_NAMESPACE,AE_UNIT_AFTER,AE_UNIT_EDGE_COUNT };
/* In-memory current-boot file observation only, never serialized native layout
 * or volume/provisioning authority. Capture ONLY after independent authorization
 * and flush of the original protected allocation, against an independent digest. */
struct ae_unit_file { struct statx object; uint8_t digest[32]; };
int ae_unit_file_capture(int fd,const uint8_t expected_sha256[32],struct ae_unit_file *);
int ae_unit_file_equal(const struct ae_unit_file *,const struct ae_unit_file *);
struct ae_unit_dropin {
    char directory[4096],leaf[AE_UNIT_TEXT];
    int directory_fd,file_fd;
    struct ae_root_identity parent;
    struct ae_unit_file file;
};
struct ae_unit_node {
    enum ae_unit_kind kind;
    char name[AE_UNIT_TEXT],directory[4096];
    int directory_fd,file_fd;
    struct ae_root_identity parent;
    struct ae_unit_file file;
    unsigned int dropin_count;
    struct ae_unit_dropin dropins[AE_UNIT_DROPINS];
    uint8_t edges[AE_UNIT_EDGE_COUNT]; /* Exact sets, bits index profile.nodes. */
    char executable[4096],argv[AE_UNIT_ARGS][AE_UNIT_TEXT],environment[AE_UNIT_ARGS][AE_UNIT_TEXT];
    unsigned int argc,envc;
    char label[512];
    uint64_t capabilities,ambient;
    int no_new_privileges,private_mounts,private_network,private_devices;
    uint32_t umask;
};
struct ae_unit_profile { unsigned int count; struct ae_unit_node nodes[AE_UNIT_MAX]; };
/* Canonical AETHUNP1 digest of meaningful profile fields, excluding FDs/padding.
 * Requires birth observations on every source/parent. Current-boot metadata,
 * never durable volume trust or permission to adopt a candidate. Output unchanged
 * on error; profile/boot immutable during the synchronous bounded encoding. */
int ae_unit_profile_digest(const uint8_t boot[16],const struct ae_unit_profile *,uint8_t output[32]);
struct ae_unit;
struct ae_unit_status {
    int used,subscribe_attempted,subscribed,ready,fenced,quarantined,error;
    uint8_t ref_attempted,referenced,unref_attempted;
    int start_used,retire_used,unsubscribe_attempted,retired;
    char attribute[64]; /* First failed static check name; no document/config data. */
    struct ae_start_status start;
};
/* Root-only, single-threaded and process-owned. MOVE one independently trusted
 * EXCLUSIVE connected host-system-bus client into this object on success. No
 * other user may own RefUnit/Subscribe on that connection. PID1 pin/boot, file
 * allocation/parent/volume/MAC and global configuration/mutation exclusion are
 * independently admitted, never obtained from an app/candidate's own claims.
 * Copies the complete bounded profile and duplicates its held source FDs.
 * Constructor has no RefUnit/Subscribe/StartUnit effects; output starts NULL. */
int ae_unit_new(sd_bus **exclusive_bus,const struct ae_peer *pid1,const uint8_t boot[16],
    const struct ae_unit_profile *,struct ae_unit **);
/* Compare the copied selection with an INDEPENDENT protected selection. Ignores
 * descriptor numbers and native padding; includes source object witnesses and
 * every meaningful graph/service field. Read-only historical equality, not
 * fresh source/loaded-state or allocation/volume admission. */
int ae_unit_match_profile(struct ae_unit *,const uint8_t boot[16],const struct ae_unit_profile *);
/* Install compiled durable before/confirmed hooks only on an unused owner.
 * before must return zero ONLY after a fresh confirmed durable intent commit;
 * it must refuse retries/lost acknowledgements. Hook failure quarantines the
 * connection owner. Every effect is guarded again after before returns. Start
 * confirmation means only the authenticated job reply, never target execution.
 * Hooks are optional for isolated Linux tests; the Root adapter requires them.
 * Original observation below is local read-only, not actor/process admission. */
struct ae_unit_journal {
    int (*before)(void *,enum ae_unit_effect,unsigned int);
    int (*confirmed)(void *,enum ae_unit_effect,unsigned int);
    /* Called once on first quarantine, under the unit's callback busy guard.
     * Must fence the compiled owner, never close/retry/control a process here. */
    void (*failed)(void *,int);
    void *data;
};
int ae_unit_origin(struct ae_unit *,struct ae_unit_origin *);
int ae_unit_set_journal(struct ae_unit *,const struct ae_unit_journal *);
/* Caller journals custody intent before entry. Each subscription/reference is
 * attempted at most once and authenticates the original reply. Unknown results
 * retain the object; never retry/adopt from matching names or Refs observations.
 * Guard is compiled current-owner/source admission, not app-supplied code. */
int ae_unit_acquire(struct ae_unit *,ae_start_guard,void *);
/* Read-only fresh-start check: exact source objects/content/MAC, loaded fragment,
 * aliases/drop-ins, complete effectful forward graph and ordering prerequisites,
 * no previous invocation/job, plus the bounded service/ExecStartEx contract.
 * Built-in perpetual root/system slices are the only source-file-free leaves.
 * Their reverse Before edges are not activation edges and may grow dynamically.
 * Source drift permanently fences fresh use but does not itself prohibit cleanup. */
int ae_unit_check_fresh(struct ae_unit *);
/* One original StartUnit through the existing authenticated job observer.
 * Caller owes fresh execution intent, target/image/data and owner admission.
 * Returns queued transport only; neither job completion nor READY settles refs.
 * Drive the borrowed bus with the normal owner event loop. */
int ae_unit_start(struct ae_unit *,ae_start_guard,void *,ae_start_changed,void *);
sd_bus *ae_unit_bus(struct ae_unit *);
int ae_unit_status(struct ae_unit *,struct ae_unit_status *);
/* Independent compiled original-run/end/holder admission is REQUIRED before
 * retirement; inactive/MainPID=0 are not such proof. Rechecks original unit/ref
 * and terminal/no-job state, then attempts each UnrefUnit/Unsubscribe once.
 * Unknown requests/jobs or reference releases remain unsettled. This never
 * stops a process, cancels a job, settles SQL or releases a generation lease. */
int ae_unit_retire(struct ae_unit *,ae_start_guard admitted_end,void *);
int ae_unit_close(struct ae_unit **);
/* Explicit LOCAL observer/connection retirement, including unknown effects.
 * Connection closure drops its server-owned subscriptions/references but cannot
 * retract queued work or establish process/holder drain or durable settlement. */
int ae_unit_abandon(struct ae_unit **);
#endif
