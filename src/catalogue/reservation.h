#ifndef AETHERI_CATALOGUE_RESERVATION_H
#define AETHERI_CATALOGUE_RESERVATION_H

#include "catalogue/store.h"

struct ae_launch_request {
    uint8_t system_epoch[16],lease[16],launch[16],broker_epoch[16],broker_runtime[16];
    uint8_t account[16],install[16],generation[16],data[16],boot[16];
    int64_t install_revision;
    char action[65],session[129],seat[129];
};
struct ae_launch_context {
    uint8_t system_runtime[16],broker_epoch[16],broker_runtime[16],account[16],boot[16];
    uint32_t uid;
};
struct ae_reservation_ids {
    uint8_t plan[16],operation[16];
    int64_t accepted_unix_ms,expires_boottime_ns;
};
struct ae_dispatch_snapshot {
    uint8_t operation[16],launch[16],invocation[16];
    int invocation_present;
    char state[16],unit[80];
    int64_t revision,install_gate_revision;
};
struct ae_reservation_receipt {
    uint8_t system_epoch[16],system_runtime[16],boot[16],plan[16],intent[32],fingerprint[32],broker_runtime[16];
    struct ae_dispatch_snapshot dispatch;
    uint8_t lease[16],cause_epoch[16],cause[16];
    int cause_present,withdrawn;
};
/* Authenticated current source plus original immutable request/operation receipt.
 * Caller still owes transport provenance; a struct is not a bearer capability. */
int ae_catalogue_get_receipt(struct ae_catalogue *catalogue,const uint8_t lease[16],
    const struct ae_launch_context *context,struct ae_reservation_receipt *output);
int ae_launch_fingerprint(const struct ae_launch_request *request,uint8_t output[32]);
int ae_launch_request_valid(const struct ae_launch_request *request);
/* Trusted single-threaded owner API. Caller authenticates the CURRENT registered
 * broker/process and authorizes action/session/seat before entering SQLite. The
 * supplied context is not itself provenance. Root never reads the user's DB.
 * Atomically journal canonical launch intent, generation lease and RESERVED
 * dispatch. No filesystem/process effect, readiness or app execution. Proposed
 * root IDs/timestamps are used only for a new reservation; exact retries retain
 * the original IDs and bytes. Changed tuple conflicts. Recovery/terminal states
 * never become fresh reservations, even after a lost acknowledgement/restart.
 * Output unchanged on error. */
int ae_catalogue_reserve_launch(struct ae_catalogue *catalogue,const struct ae_launch_request *request,
    const struct ae_launch_context *context,const struct ae_reservation_ids *proposed,struct ae_dispatch_snapshot *output);
/* Owner/account/broker-epoch-scoped observation; a replacement broker runtime
 * may inspect old obligations, but cannot reuse an old reservation. */
int ae_catalogue_get_dispatch(struct ae_catalogue *catalogue,const uint8_t lease[16],
    const struct ae_launch_context *context,struct ae_dispatch_snapshot *output);
/* Atomic withdrawal ONLY of a still-reserved, never-dispatched tuple. Freeze the
 * cause, terminate dispatch, release lease and complete root operation cancelled.
 * A replacement current broker of the same authority/account may withdraw an
 * original still-reserved tuple; it never rebinds it. No stop/drain inference:
 * dispatching/observed/uncertain require reconciliation.
 * Exact terminal retry preserves bytes; a changed cause conflicts. */
int ae_catalogue_cancel_reservation(struct ae_catalogue *catalogue,const uint8_t lease[16],
    const struct ae_launch_context *context,const uint8_t cause_epoch[16],const uint8_t cause[16],
    int64_t expected_revision,int64_t completed_unix_ms,struct ae_dispatch_snapshot *output);
/* Explicit P0 startup admission after independent current sealed-store validation.
 * Requires current boot/runtime, active exact I/G, closed unowned gate at expected
 * revision, and no outstanding holders/dispatches. This opens metadata reservation
 * admission only; it never grants data readiness or executable mappings. */
int ae_catalogue_admit_install(struct ae_catalogue *catalogue,const uint8_t install[16],const uint8_t generation[16],
    int64_t install_revision,int64_t gate_revision,const uint8_t boot[16],const uint8_t runtime[16],int64_t *output);

#endif
