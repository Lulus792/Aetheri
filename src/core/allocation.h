#ifndef AETHERI_CORE_ALLOCATION_H
#define AETHERI_CORE_ALLOCATION_H

#include "core/storage.h"

enum { AE_ALLOCATION_BYTES = 168 };
struct ae_allocation_object { uint64_t inode, birth_seconds; uint32_t birth_nanoseconds; };
struct ae_allocation {
    struct ae_owner owner;
    uint64_t filesystem_type;
    uint8_t filesystem_id[8];
    struct ae_allocation_object anchor, root;
};

/* Canonical AETHALC1, no native padding/endianness. Outputs unchanged on failure.
 * Captured fields witness an allocated directory; filesystem ID is not volume
 * authentication. Registered machine/volume/account/MAC provenance remains a
 * caller precondition. No allocation/operation is inferred from a candidate path.
 */
int ae_allocation_encode(const struct ae_allocation *value, uint8_t *output, size_t size);
int ae_allocation_decode(const uint8_t *bytes, size_t size, struct ae_allocation *output);
/* Trusted allocator only, after independently authorized directory/header setup.
 * This captures an effect; never offer it as app self-enrollment or adoption of
 * an arbitrary existing tree. Requires birth times and matching held/named roots.
 */
int ae_allocation_capture(int anchor_fd, int root_fd, const struct ae_owner *owner, struct ae_allocation *output);
/* Immutable create-only record keyed by StorageRef in a protected 0700 registry
 * owned by effective UID; record is 0600. Uses O_TMPFILE + create-only link + fsync.
 * A failed/lost directory-flush or close acknowledgement may leave a committed record:
 * preserve it, retry only the same intent, never replace/adopt/allocate new IDs.
 * Caller serializes lifecycle operations and records intent before creating roots.
 * Does not create directories, settle operation journals, purge or register volumes.
 */
int ae_allocation_publish(int registry_fd, int anchor_fd, int root_fd, const struct ae_owner *owner);
/* Read existing record, require independently expected owner, compare persistent
 * object witnesses, then pin the current mount identities. Never creates/repairs
 * a record. Return owned internal storage reference; output unchanged on failure.
 */
int ae_allocation_open(int registry_fd, int anchor_fd, const struct ae_owner *owner, struct ae_storage_ref **output);

#endif
