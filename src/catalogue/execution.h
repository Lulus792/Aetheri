#ifndef AETHERI_CATALOGUE_EXECUTION_H
#define AETHERI_CATALOGUE_EXECUTION_H
#include "catalogue/launch.h"

/* Separate purpose/tag from the immutable no-exec setup profile. This bounded
 * P0 profile selects the manifest action run in session p0 / seat none. The
 * caller owes protected provisioning, actual manifest/image/allocation checks,
 * current authenticated broker and held starter/supervisor provenance. Neither
 * these bytes nor an evidence digest is a capability. */
enum { AE_EXECUTION_BYTES=384 };
struct ae_execution_profile {
    uint8_t system_epoch[16],system_runtime[16],broker_epoch[16],broker_runtime[16],boot[16],account[16];
    uint8_t install[16],generation[16],data[16],storage[16],launch[16],lease[16],root_operation[16],user_operation[16];
    int64_t install_revision,install_gate_revision,data_gate_revision;
    uint8_t artifact[32],starter_digest[32],request_digest[32],allocation_digest[32];
};
int ae_execution_encode(const struct ae_execution_profile *,uint8_t output[AE_EXECUTION_BYTES]);
int ae_execution_decode(const uint8_t *,size_t,struct ae_execution_profile *);
int ae_execution_digest(const struct ae_execution_profile *,uint8_t output[32]);
/* User-owner read fence for the accepted exact run, current private data and
 * authenticated Root receipt. Caller still reopens/verifies the independent
 * allocation and raw MAC context; SQLite is not filesystem/process evidence. */
int ae_catalogue_authorize_user_execution(struct ae_catalogue *,const struct ae_execution_profile *);
struct ae_user_execution_result {
    struct ae_user_launch_snapshot launch;
    /* One only after the fresh preparation commit. A retry, including after a
     * lost acknowledgement, never permits a new external submission. */
    int submit_required;
};
/* User-owner intent BEFORE the first executing Root request. Bind the complete
 * independently verified profile to operation step 1 (stage/data/executing).
 * The exact accepted launch/root receipt, active attached data and current open
 * fence must still match. This disables ordinary pre-dispatch withdrawal without
 * claiming Root dispatch, mount readiness, exec, application readiness or drain.
 * Caller authenticates current Root/broker and its still-reserved original Root
 * receipt, and verifies private allocation/MAC immediately before this call;
 * this metadata journal neither observes the filesystem nor grants ARM rights.
 * No RPC/effect inside SQLite. Outputs unchanged on all errors/unknown COMMIT.
 * Exact current retries retain history/revisions with submit_required=0. Changed
 * profile conflicts; runtime/gate/data drift cannot reauthorize old work. Startup
 * recovery preserves the step; only a separate executing reconciliation may
 * settle it. Never use no-exec cancellation after this preparation. */
int ae_catalogue_prepare_user_execution(struct ae_catalogue *,const struct ae_execution_profile *,struct ae_user_execution_result *);

enum ae_execution_stage { AE_EXECUTION_REQUEST=1,AE_EXECUTION_BIND,AE_EXECUTION_PREPARE,AE_EXECUTION_ARM };
struct ae_execution_evidence { uint8_t invocation[16],context_digest[32]; };
enum ae_user_execution_stage { AE_USER_EXECUTION_REGISTER=1,AE_USER_EXECUTION_READY };
/* User-owner observation journal. Caller independently authenticates current
 * Root's dispatch/image observation and the app's actual ProcessFD/unit/current
 * image, retains the original connection and rechecks private data/MAC. REGISTER
 * binds that invocation/evidence before READY from the same live actor. No IPC,
 * filesystem or process proof is created by SQLite. Exact retries preserve later
 * state; changed evidence conflicts, stale gates/runtime reject. Readiness never
 * settles Root's lease or either owner's operation; drain/recovery remain separate.
 * Outputs unchanged on all errors, including an unknown COMMIT acknowledgement. */
int ae_catalogue_observe_user_execution(struct ae_catalogue *,const struct ae_execution_profile *,
    enum ae_user_execution_stage,const struct ae_execution_evidence *,struct ae_user_launch_snapshot *);
enum ae_execution_diagnostic_kind {
    AE_EXECUTION_DIAGNOSTIC_UNKNOWN=1,AE_EXECUTION_DIAGNOSTIC_FAILURE,
    AE_EXECUTION_DIAGNOSTIC_MALFORMED,AE_EXECUTION_DIAGNOSTIC_OBSERVER
};
/* Canonical internal AETHXER1 phase numbers, not a dependency on Linux APIs.
 * OBSERVER errors originate in Root's observation path, not the app/starter. */
enum { AE_EXECUTION_PHASE_MAX=10 };
struct ae_execution_diagnostic { enum ae_execution_diagnostic_kind kind; uint32_t phase,error; };
int ae_execution_diagnostic_encode(const struct ae_execution_profile *,const struct ae_execution_evidence *,
    const struct ae_execution_diagnostic *,uint8_t digest[32],char reason[64]);
struct ae_execution_result {
    struct ae_dispatch_snapshot dispatch;
    /* One only on the successful fresh REQUEST/ARM transaction. Zero on every
     * exact retry, including a retry after a lost COMMIT acknowledgement. The
     * caller may issue the external request/arm ONLY for this fresh result.
     * An error or a retry never permits a repeated effect. */
    int effect_required;
};
/* Root-owner transaction only; no process/unit/FD/IPC effect inside SQLite.
 * REQUEST records intent before requesting the supervised starter. BIND pins
 * its independently observed invocation/context. PREPARE records setup complete
 * without claiming execution. ARM freezes that invocation and the authenticated
 * User data fence BEFORE releasing the starter to exec. The lease stays held.
 *
 * Later-stage exact retries observe the current receipt; they cannot rearm.
 * Root/boot/broker replacement, changed evidence or a different profile is
 * rejected. No-exec withdrawal/cancellation cannot settle this journal. There
 * is deliberately no terminal/release API here: image/readiness/drain need their
 * own actual evidence. Outputs stay unchanged on every error. */
int ae_catalogue_execution(struct ae_catalogue *,const struct ae_execution_profile *,
    const struct ae_launch_context *,enum ae_execution_stage,const struct ae_execution_evidence *,
    struct ae_execution_result *);
/* Root observed its own issued channel under the original held starter pin.
 * Record immutable integration step 4 (invocation + AETHEXD1 digest) and bounded
 * canonical reason, atomically. This also permits reporting after an ambiguous
 * BIND commit: the original REQUEST must exist, and an existing BIND must match.
 * Never executes/closes a dispatch, completes the operation or releases a lease.
 * Exact retry preserves revisions; changed evidence conflicts. Current original
 * Root runtime/boot required. Caller owes actual process/channel provenance. */
int ae_catalogue_execution_report(struct ae_catalogue *,const struct ae_execution_profile *,
    const struct ae_launch_context *,const struct ae_execution_evidence *,const struct ae_execution_diagnostic *,
    struct ae_execution_result *);
#endif
