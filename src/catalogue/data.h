#ifndef AETHERI_CATALOGUE_DATA_H
#define AETHERI_CATALOGUE_DATA_H

#include "catalogue/store.h"
#include "core/allocation.h"

enum { AE_DATA_CONFIG_BYTES = 368 };
struct ae_data_enrollment {
    struct ae_owner owner;
    uint8_t install[16], publisher[16], system_epoch[16], user_epoch[16];
    int64_t install_revision, base_revision, schema_version;
    struct ae_allocation allocation;
};
/* Internal protected AETHDTA1, not an app package or public request. Exact bytes,
 * UUIDv4 identities, independent object witness and signed SQL bounds required.
 * Outputs unchanged on failure. Does not inspect any filesystem object. */
int ae_data_enrollment_decode(const uint8_t *bytes,size_t size,struct ae_data_enrollment *output);
int ae_data_enrollment_valid(const struct ae_data_enrollment *request);
/* Explicit P0 first enrollment of independently preallocated, flushed, verified
 * storage. Caller must verify the immutable allocation and enforcing policy
 * before entering this all-SQL transaction. Never adopts an existing epoch.
 * New rows require original protected catalogue baseline, current boot/user epoch
 * and the exact active cached install observation. Insert D, attachment and CLOSED
 * data gate atomically; no launch, grant or installation gate is opened.
 * Exact live retry is read-only, including after later gate changes. Detached,
 * retained, purged, changed or conflicting rows fail rather than being restored.
 * Missing rows after an advanced baseline cannot recreate an old epoch. This is
 * not a general allocator, allocation journal or live system admission check. */
int ae_catalogue_enroll_data(struct ae_catalogue *catalogue,const struct ae_data_enrollment *request,
                             const uint8_t current_boot[16],int64_t *revision);

#endif
