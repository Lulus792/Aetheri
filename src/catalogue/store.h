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
 * validate exact v2 schema/metadata. Outputs remain unchanged on failure.
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
/* Never carry SQLite connections across fork. These APIs reject a fork child;
 * callers must exec/_exit without using the inherited connection.
 */
int ae_catalogue_close(struct ae_catalogue **catalogue);

#endif
