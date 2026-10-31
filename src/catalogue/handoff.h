#ifndef AETHERI_CATALOGUE_HANDOFF_H
#define AETHERI_CATALOGUE_HANDOFF_H
#include "catalogue/execution.h"
#include "core/root.h"

enum { AE_HANDOFF_MAX=2560,AE_HANDOFF_FIXED=216 };
/* Original observations, not admission inferred from incoming handles. This
 * canonical metadata is boot-local; mount/pidfs facts never authenticate a
 * volume/clone. Caller separately protects the original profile/custodian and
 * actual ProcessFD/image/root/group/event observations before enrollment. */
struct ae_execution_handoff {
    struct ae_execution_process process;
    char custodian[256];
    uint8_t custodian_invocation[16];
    struct ae_root_identity root,group;
    uint64_t pid_namespace,cgroup_namespace,events_inode,events_mount;
    uint32_t events_mode,events_uid,events_gid;
};
int ae_handoff_encode(const struct ae_execution_handoff *,uint8_t *,size_t,size_t *);
int ae_handoff_decode(const uint8_t *,size_t,struct ae_execution_handoff *);
int ae_handoff_match(const struct ae_execution_profile *,const struct ae_execution_handoff *);

enum ae_handoff_stage { AE_HANDOFF_PREPARE=1,AE_HANDOFF_OBSERVE,AE_HANDOFF_RETIRE,AE_HANDOFF_RETIRED };
enum ae_handoff_state { AE_HANDOFF_PENDING=1,AE_HANDOFF_OBSERVED,AE_HANDOFF_RETIRING,AE_HANDOFF_FINISHED };
struct ae_handoff_snapshot {
    struct ae_execution_handoff original;
    enum ae_handoff_state state;
    int64_t revision,recorded_unix_ms,observed_unix_ms,retiring_unix_ms,retired_unix_ms;
    uint8_t observer_runtime[16],retirement_runtime[16],retired_runtime[16];
};
struct ae_handoff_result {
    struct ae_handoff_snapshot snapshot;
    /* One ONLY after a confirmed fresh PREPARE/RETIRE commit respectively.
     * Existing-intent retries (including lost successful commits) have zero
     * effect flags. No actual effect
     * or kernel/transport proof takes place inside a catalogue transaction. */
    int transfer_required,retire_required;
};
/* Root-only explicit v5 extension; normal creation stays v2. PREPARE requires
 * original Root runtime and its immutable app witness before any transfer.
 * OBSERVE requires a replacement current Root runtime and caller's actual exact
 * delivered-set validation under independently protected original expectations.
 * It records last observed incarnation only, never adopts the app/run or proves
 * permanent possession/readiness. Repeated replacement runtimes are supported.
 * RETIRE requires native original terminal/released closure; caller additionally
 * excludes future transfers and settles all relevant holders. RETIRED requires
 * its intent plus actual independently checked PID1 slot absence. Names/barriers
 * alone are not set acceptance/absence. Unknown dispatches preserve pending state;
 * no fresh flags on retries authorize resubmission or replacement identities.
 * Exact retries preserve later state/times/gates. Outputs unchanged on errors. */
int ae_catalogue_handoff(struct ae_catalogue *,const struct ae_execution_profile *,enum ae_handoff_stage,
    const struct ae_execution_handoff *,int64_t,struct ae_handoff_result *);
/* Protected same-authority/boot history; current owner runtime may differ.
 * Never infer held descriptors, actor admission, current image, drain or release. */
int ae_catalogue_get_handoff(struct ae_catalogue *,const struct ae_execution_profile *,struct ae_handoff_snapshot *);
#endif
