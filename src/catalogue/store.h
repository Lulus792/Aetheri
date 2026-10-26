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
 * validate exact v2 schema/metadata. A runtime unique mount identity pins the
 * directory; later calls reject a named bind-mount replacement even for the same
 * inode/device. This is not durable volume registration. Outputs remain unchanged on failure.
 * Errors are negative errno (EPROTONOSUPPORT for versions requiring migration).
 * Failed creation leaves its owned files for inspection, never blind deletion.
 */
int ae_catalogue_create(const char *directory, const struct ae_catalogue_identity *identity,
                        const uint8_t boot_id[AE_ID_BYTES], struct ae_catalogue **output);
int ae_catalogue_open(const char *directory, const struct ae_catalogue_identity *identity,
                      struct ae_catalogue **output);
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
/* Never carry SQLite connections across fork. These APIs reject a fork child;
 * callers must exec/_exit without using the inherited connection.
 */
int ae_catalogue_close(struct ae_catalogue **catalogue);

#endif
