#ifndef AETHERI_CATALOGUE_RECOVERY_H
#define AETHERI_CATALOGUE_RECOVERY_H
#include "store.h"
#include <stddef.h>

enum ae_recovery_record_kind {
    AE_RECOVERY_OPERATION=1,AE_RECOVERY_GATE,AE_RECOVERY_INSTALL,AE_RECOVERY_GENERATION,
    AE_RECOVERY_CODE_LEASE,AE_RECOVERY_DISPATCH,AE_RECOVERY_LAUNCH,AE_RECOVERY_GRANT,
    AE_RECOVERY_ACCESS,AE_RECOVERY_RESOURCE,AE_RECOVERY_DATA,AE_RECOVERY_CLOSURE,
    AE_RECOVERY_HANDOFF,AE_RECOVERY_INTERRUPTION,AE_RECOVERY_UNIT_ALLOCATION,AE_RECOVERY_UNIT_CUSTODY,AE_RECOVERY_UNIT_ISSUER,AE_RECOVERY_KIND_COUNT
};
enum { AE_RECOVERY_MAX_RECORDS=65536 };
struct ae_recovery_record {
    enum ae_recovery_record_kind kind;
    uint8_t id[16],related[16]; /* All zero when the source relation is NULL. */
    int has_related;
    int64_t revision,unfinished_steps;
    char state[24],detail[65],phase[16],reason[257];
};
struct ae_recovery_inventory {
    struct ae_catalogue_identity identity;
    struct ae_catalogue_info context;
    unsigned schema_version;
    size_t count,by_kind[AE_RECOVERY_KIND_COUNT];
    struct ae_recovery_record *records;
};
/* Read-only owner metadata from ONE SQLite read transaction; sorted by kind/ID.
 * Existing original lock/DB/mount and single-threaded store contract still apply.
 * Supports explicit v2-v8 owners and Root v9/v10 as open; never starts a runtime,
 * migrates, dispatches, closes a gate or settles a holder/lease. max_records is
 * 1..65536; E2BIG (record/text bound) is an error, never successful truncation. *output
 * must be NULL; all errors preserve it. Result owns its records until free.
 *
 * Includes unfinished operations/steps, open or cause-bearing gates, transitional
 * installs/generations/data, unretired leases/dispatches/launches/grants/access,
 * resources needing reselection and later closure/handoff/interruption metadata.
 * Completed START operations do not hide still-running leases. Closure rows can
 * remain historical/uncertain after a separate terminal interruption.
 * Root v9 unit allocations remain visible, including witnessed selections:
 * witnessing is not custody retirement and no unit-retirement protocol exists.
 * Root v10 includes connection custody even after every RPC is acknowledged.
 * Acknowledgement history is not independent end/holder or retirement evidence.
 *
 * This is neither current process/FD-store evidence nor a global readiness gate.
 * Empty/closed SQL never proves external holder drain, future-effect exclusion,
 * actor or machine/volume/clone admission. Revisions are observations only, not
 * capabilities or a mutation fence. Never replay effects from this inventory. */
int ae_catalogue_recovery_inventory(struct ae_catalogue *,size_t max_records,struct ae_recovery_inventory **output);
void ae_recovery_inventory_free(struct ae_recovery_inventory **);
#endif
