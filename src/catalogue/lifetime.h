#ifndef AETHERI_CATALOGUE_LIFETIME_H
#define AETHERI_CATALOGUE_LIFETIME_H
#include "catalogue/execution.h"

enum { AE_RUN_RECEIPT_BYTES=136 };
enum ae_run_receipt_kind { AE_RUN_ROOT_CLOSING=1,AE_RUN_USER_QUIET,AE_RUN_ROOT_RELEASED };
struct ae_run_receipt {
    enum ae_run_receipt_kind kind;
    uint8_t profile_digest[32],invocation[16],previous_digest[32],evidence_digest[32];
    int64_t revision,unix_ms;
};
/* Canonical metadata receipts, never bearer/process/drain capabilities. */
int ae_run_receipt_encode(const struct ae_run_receipt *,uint8_t [AE_RUN_RECEIPT_BYTES]);
int ae_run_receipt_decode(const uint8_t *,size_t,struct ae_run_receipt *);
int ae_run_receipt_digest(const struct ae_run_receipt *,uint8_t [32]);
int ae_run_receipt_match(const struct ae_execution_profile *,const struct ae_execution_evidence *,enum ae_run_receipt_kind,const struct ae_run_receipt *);
struct ae_run_end {
    uint8_t boot[16],invocation[16];
    uint64_t cgroup_id;
    uint32_t pid;
    int group_removed,unit_failed;
};
/* Hash only already verified original Linux lifetime facts. It does not capture
 * a process, authenticate a caller or exclude future dispatch/migration/holders. */
int ae_run_end_digest(const struct ae_execution_profile *,const struct ae_execution_evidence *,const struct ae_run_end *,uint8_t [32]);
/* Internal v3 owner transactions. Normal v2 create/open never upgrades itself.
 * Original epochs/runtimes/boot and full profile/evidence are mandatory. The
 * completed start operation, receipt/time/revision and its steps never change.
 * Caller authenticates every cross-owner receipt on the original live connection;
 * bytes/digests alone cannot authorize a close or prove quiescence/end.
 * No IPC/process/filesystem effect in SQLite. All outputs unchanged on errors,
 * including unknown COMMIT. Exact retries return immutable original receipts.
 * Owner replacement needs separate recovery; never rearm/revive a run or gate. */
int ae_catalogue_begin_run_close(struct ae_catalogue *,const struct ae_execution_profile *,const struct ae_launch_context *,
    const struct ae_execution_evidence *,const struct ae_execution_completion *,int64_t,struct ae_run_receipt *);
/* The original User joins Root's closing receipt to its completed/ready start,
 * settles all native handles independently, and fences its run to stopping.
 * This P0 transaction requires no grants/accesses; general revocation is later. */
int ae_catalogue_quiesce_user_run(struct ae_catalogue *,const struct ae_execution_profile *,
    const struct ae_execution_evidence *,const struct ae_run_receipt *,int64_t,struct ae_run_receipt *);
/* Original Root must independently hold the original end observer, exclude new
 * supervisor/dispatch/migration work and settle all external/delegated holders.
 * Authenticate original User's quiet receipt before passing it here. Only then
 * atomically record the end, terminal dispatch and released generation lease.
 * Never use no-exec withdrawal, EOF, an absent name or a cached snapshot as proof. */
int ae_catalogue_release_run(struct ae_catalogue *,const struct ae_execution_profile *,const struct ae_launch_context *,
    const struct ae_execution_evidence *,const struct ae_run_receipt *,const uint8_t end_digest[32],int64_t,struct ae_run_receipt *);
/* Original User authenticates Root's exact release receipt before marking its
 * own already-quiescent launch exited. No data/gate deletion or resurrection. */
int ae_catalogue_end_user_run(struct ae_catalogue *,const struct ae_execution_profile *,
    const struct ae_execution_evidence *,const struct ae_run_receipt *,int64_t,struct ae_user_launch_snapshot *);
/* Read the original local immutable receipt; no new lifetime proof/effect.
 * Original owner/runtime only. ENOENT means this stage is not recorded. */
int ae_catalogue_get_run_receipt(struct ae_catalogue *,const struct ae_execution_profile *,
    const struct ae_execution_evidence *,enum ae_run_receipt_kind,struct ae_run_receipt *);
/* Separate Root-only v6 closure after a SAME-BOOT Root replacement. Original
 * profile/start/process/receipts remain immutable; context.system_runtime is the
 * current Root, while broker identity MUST still be the original live User.
 * Caller authenticates that User anew against independently retained ORIGINAL
 * process/unit/invocation evidence (not just a current unit/domain or broker
 * UUID), validates/restores the exact original observation handles and commits
 * current v5 OBSERVE before these calls. This
 * is close-only recovery, never execution/readiness adoption or fresh dispatch.
 * BEGIN atomically records an immutable current-owner observation with the
 * original closing receipt (or retains an existing uncertain closure). RELEASE
 * needs fresh original end/holder/future-effect evidence and authenticated quiet
 * receipt. A new Root after another interruption must OBSERVE again and call
 * BEGIN before RELEASE. No restarted User, cross-boot or early-start recovery.
 * GET is read-only; original APIs keep their original-runtime checks. Errors,
 * including lost successful commits, preserve outputs. Exact repeats preserve
 * original receipts/gates/start operation, including after handoff retirement. */
int ae_catalogue_recover_begin_run_close(struct ae_catalogue *,const struct ae_execution_profile *,
    const struct ae_launch_context *,const struct ae_execution_evidence *,const struct ae_execution_completion *,int64_t,struct ae_run_receipt *);
int ae_catalogue_recover_release_run(struct ae_catalogue *,const struct ae_execution_profile *,
    const struct ae_launch_context *,const struct ae_execution_evidence *,const struct ae_run_receipt *,const uint8_t [32],int64_t,struct ae_run_receipt *);
int ae_catalogue_get_recovered_run_receipt(struct ae_catalogue *,const struct ae_execution_profile *,
    const struct ae_launch_context *,const struct ae_execution_evidence *,enum ae_run_receipt_kind,struct ae_run_receipt *);
#endif
