#ifndef AETHERI_CATALOGUE_STORE_H
#define AETHERI_CATALOGUE_STORE_H

#include "core/owner.h"
#include <stdint.h>

enum ae_catalogue_kind { AE_CATALOGUE_SYSTEM = 1, AE_CATALOGUE_USER = 2 };
struct ae_catalogue;
struct ae_catalogue_identity {
    enum ae_catalogue_kind kind;
    uint8_t authority_epoch[AE_ID_BYTES];
    uint8_t owner[AE_ID_BYTES]; /* Required for user; all zero for system. */
};
struct ae_catalogue_info {
    uint8_t authority_epoch[AE_ID_BYTES];
    uint8_t boot_id[AE_ID_BYTES];
    uint8_t runtime_incarnation[AE_ID_BYTES]; /* Zero only before first start. */
    int64_t revision;
};

/* Trusted single-threaded authority API, never a client path/SQL interface.
 * Caller supplies an absolute, protected 0700 directory owned by the effective
 * UID and identity from its protected registry. The whole parent path and mount
 * must be trusted and kept stable. MAC protection from same-UID apps, volume/
 * registry identity and external quiescence remain caller obligations.
 *
 * Explicit create uses O_EXCL; open never creates/replaces/migrates a database.
 * Both retain a separate CLOEXEC writer.lock, require rollback journaling and
 * validate exact v2 or explicitly upgraded v3/v4 (Root also v5/v6) schema/metadata.
 * A runtime unique mount identity pins the
 * directory; later calls reject a named bind-mount replacement even for the same
 * inode/device. This is not durable volume registration. Outputs remain unchanged on failure.
 * Errors are negative errno (EPROTONOSUPPORT for versions requiring migration).
 * Failed creation leaves its owned files for inspection, never blind deletion.
 */
int ae_catalogue_create(const char *directory, const struct ae_catalogue_identity *identity,
                        const uint8_t boot_id[AE_ID_BYTES], struct ae_catalogue **output);
int ae_catalogue_open(const char *directory, const struct ae_catalogue_identity *identity,
                      struct ae_catalogue **output);
/* Explicit additive v2 -> v3 execution-lifetime journal migration. Caller must
 * independently quiesce old services/writers; normal create still creates v2
 * and open never migrates. Retain the same writer lock/DB object; do not copy
 * back backups after a lost COMMIT. Exact retry validates actual v3. Existing
 * rows, owner/runtime identity, revisions and gates are unchanged. */
int ae_catalogue_upgrade_lifetimes(struct ae_catalogue *);
/* Explicit v3 -> v4 immutable process-evidence extension under the same lock.
 * Exact v4 retry validates; v2 requires the separate lifetime upgrade first.
 * Caller independently quiesces writers. No existing row/runtime/gate changes. */
int ae_catalogue_upgrade_processes(struct ae_catalogue *);
/* Explicit Root-only v4 -> v5 original-handoff journal. User stays v4; no
 * automatic migration, old-row/backfill, gate/runtime change or actual handoff.
 * Same writer/DB object; lost acknowledgement inspects actual v4/v5, never copies
 * a backup over the database. Exact v5 retry validates without changing history. */
int ae_catalogue_upgrade_handoffs(struct ae_catalogue *);
/* Explicit Root v5 -> v6 recovery-owner observations and narrowly extended
 * closure admission. No rows/runtime/gates change; no automatic upgrade. */
int ae_catalogue_upgrade_recovery(struct ae_catalogue *);
/* Commit fresh runtime + boot. Existing open gates close; closing gates keep
 * their durable cause. Triggers mark recovery obligations. This does not settle
 * holders, reopen gates or establish a global service-readiness gate. Reusing
 * the previous incarnation is rejected.
 */
int ae_catalogue_begin_runtime(struct ae_catalogue *catalogue,
                               const uint8_t boot_id[AE_ID_BYTES],
                               const uint8_t incarnation[AE_ID_BYTES]);
int ae_catalogue_get_info(struct ae_catalogue *catalogue, struct ae_catalogue_info *output);
/* System-owner account enrollment from protected administrative configuration.
 * No client-selected identity/UID, account creation or private-data adoption.
 * Matching active enrollment is idempotent; retired or differently bound scopes
 * are never reactivated/rebound. A live UID collision is rejected. Insert and
 * catalogue revision advance commit together. A lost commit acknowledgement is
 * an error, not permission to allocate a replacement identity.
 */
int ae_catalogue_enroll_account(struct ae_catalogue *catalogue,
                                const uint8_t account[AE_ID_BYTES], uint32_t os_uid);
/* Require this exact active account/UID; preserve output on failure. The returned
 * revision is a read observation, not a future admission lock or process proof.
 */
int ae_catalogue_check_account(struct ae_catalogue *catalogue,
                               const uint8_t account[AE_ID_BYTES], uint32_t os_uid,
                               int64_t *revision);

/* Protected administrative P0 fixture request, not a client install API. The
 * source and empty staging slot are preallocated carrier infrastructure. Caller
 * independently authorizes publisher/artifact, scope/UID and every fresh ID;
 * IDs are never inferred from candidate files or regenerated after uncertainty.
 * The fixed install intent includes the scope read and install/generation/slot
 * effects in canonical AETHINT1 order. Existing SQL/XML contracts are unchanged.
 */
struct ae_fixture_request {
    uint8_t plan[16], operation[16], actor[16], request_key[16], scope[16];
    uint8_t install[16], generation[16], slot[16], publisher[16], boot[16];
    uint8_t artifact[32], expected_intent[32];
    int64_t scope_revision, expires_boottime_ns, accepted_unix_ms;
    uint32_t os_uid;
};
enum ae_fixture_step { AE_FIXTURE_PLANNED = 0, AE_FIXTURE_EXECUTING = 1,
                       AE_FIXTURE_READY = 2, AE_FIXTURE_UNCERTAIN = 3 };
struct ae_fixture_progress {
    enum ae_fixture_step step;
    int evidence_present;
    uint8_t evidence[32], intent[32];
    int64_t revision;
    int publication_present;
    enum ae_fixture_step publication_step;
    uint8_t publication_evidence[32];
    int activation_present;
    uint8_t activation_evidence[32];
    int64_t completed_unix_ms;
    int installation_active; /* Current pointer, unlike historical step evidence. */
};
/* Atomic intent/operation/preparing metadata acceptance before payload effects.
 * Exact replay returns the same operation/progress without revision advancement;
 * changed IDs/actor/key/intent or conflicting slot ownership are rejected. A new
 * request requires the current catalogue boot and unexpired CLOCK_BOOTTIME
 * deadline. Caller supplies current_boot from the trusted kernel context, never
 * from the request. Accepted replay can cross a boot/deadline with scope checks.
 * Outputs stay unchanged on any error, including lost COMMIT acknowledgement.
 */
int ae_catalogue_fixture_accept(struct ae_catalogue *catalogue,
                                const struct ae_fixture_request *request,
                                const uint8_t *manifest, size_t size,
                                const uint8_t current_boot[16],
                                struct ae_fixture_progress *output);
/* Persist executing + immutable evidence binding before IO, uncertain on failed
 * IO, or applied stage/publish-pending only after full staged verification/flush.
 * evidence is the independently validated protected fixture configuration hash,
 * binding its source/staging object witnesses. It may never be replaced. READY
 * does not complete the operation, publish/activate a generation or admit a run.
 * This API does not perform or prove the external filesystem/policy work itself.
 */
int ae_catalogue_fixture_transition(struct ae_catalogue *catalogue,
                                    const struct ae_fixture_request *request,
                                    const uint8_t *manifest, size_t size,
                                    enum ae_fixture_step step, const uint8_t evidence[32],
                                    struct ae_fixture_progress *output);
/* Read only: require an already accepted exact request, including active scope.
 * Never create an operation or refresh its original boot/deadline/identities.
 */
int ae_catalogue_fixture_get(struct ae_catalogue *catalogue,
                             const struct ae_fixture_request *request,
                             const uint8_t *manifest, size_t size,
                             struct ae_fixture_progress *output);
/* Reserve step 1 as PLANNED with immutable publication-configuration evidence,
 * then persist EXECUTING before external IO. The protected evidence binds the
 * stage configuration, store anchor and preallocated container object witnesses.
 * READY requires caller proof of sealed, fully flushed publication; atomically
 * initially sets artifact verified/generation committed, operation running/reconcile with
 * activation_pending. Install remains preparing, no activation/result/lease.
 * Exact applied replay never resets later lifecycle state. This only journals
 * the caller's evidence, never performs filesystem/policy IO.
 * Applied publication is terminal here: later corruption must block admission,
 * not silently roll metadata back. All failures preserve output and any commit.
 */
int ae_catalogue_fixture_publication(struct ae_catalogue *catalogue,
                                     const struct ae_fixture_request *request,
                                     const uint8_t *manifest, size_t size,
                                     enum ae_fixture_step step, const uint8_t evidence[32],
                                     struct ae_fixture_progress *output);
struct ae_fixture_activation {
    uint8_t publication_evidence[32], expected_evidence[32];
    int64_t install_revision, generation_revision, gate_revision, completed_unix_ms;
};
/* Finish the first installation, never update/rearm an existing generation.
 * Before first activation, caller verifies the exact immutable published object
 * under the owner lock/MAC. Reconstruct AETHACT1 (publication evidence + four big-endian u64
 * values) and compare expected_evidence before SQL. Require current kernel boot,
 * an initialized catalogue runtime, exact scope/read revisions, a closed unowned
 * gate and no outstanding code/dependency/dispatch holders. Atomically activate
 * the original generation, add applied step 2, and complete this install operation
 * with its original InstallId. Gate remains closed; no lease, run, process, user
 * data or registration is created. This is not launch readiness or a public API.
 * Exact completed replay retains revisions/timestamp/evidence after boot, a
 * newer generation or removal; installation_active reports the current pointer,
 * not historical success. No store/policy validation is implied by receipt lookup.
 * A lost COMMIT acknowledgement never reactivates or replaces the generation.
 * Completion timestamp is the protected administrative stamp, not COMMIT time.
 */
int ae_catalogue_fixture_activate(struct ae_catalogue *catalogue,
                                  const struct ae_fixture_request *request,
                                  const uint8_t *manifest, size_t size,
                                  const struct ae_fixture_activation *activation,
                                  const uint8_t current_boot[16],
                                  struct ae_fixture_progress *output);
/* Never carry SQLite connections across fork. These APIs reject a fork child;
 * callers must exec/_exit without using the inherited connection.
 */
int ae_catalogue_close(struct ae_catalogue **catalogue);

#endif
