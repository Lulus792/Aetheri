#ifndef AETHERI_CATALOGUE_LAUNCH_H
#define AETHERI_CATALOGUE_LAUNCH_H
#include "catalogue/reservation.h"
#include "catalogue/projection.h"

struct ae_user_launch {
    struct ae_launch_request reservation;
    struct ae_reservation_ids local;
    struct ae_install_snapshot source;
    uint8_t actor[16],request_key[16],storage[16];
    int64_t data_revision,data_gate_revision;
};
struct ae_user_launch_snapshot {
    uint8_t operation[16],launch[16],lease[16],root_operation[16],system_epoch[16],fingerprint[32];
    char state[24],operation_state[16],readiness[16];
    int root_link_present,pre_dispatch_withdrawable;
    int64_t revision,operation_revision,data_gate_revision,install_gate_revision;
};
/* Internal owner APIs. Caller authenticates action/session, obtains a CURRENT
 * authenticated root snapshot/receipt, and independently checks exact private
 * allocation/MAC before opening data admission. Structs are not provenance.
 * No transaction spans filesystem, IPC or supervisor work. No app is executed.
 * All outputs remain unchanged on error, including uncertain COMMIT outcomes. */
int ae_catalogue_admit_launch_data(struct ae_catalogue *catalogue,const struct ae_user_launch *request,int64_t *revision);
/* Persist intent, proposed run/lease and pending root correlation BEFORE RPC.
 * Exact retries preserve IDs/revisions. Never reactivate completed/recovery work.
 * Requires already open exact data gate; does not infer prepared views/readiness. */
int ae_catalogue_accept_launch(struct ae_catalogue *catalogue,const struct ae_user_launch *request,struct ae_user_launch_snapshot *output);
/* Bind actual root plan/op/digest from authenticated receipt; immutable after
 * first acknowledgement. Retain pending run on failure/unknown remote outcome. */
int ae_catalogue_ack_launch(struct ae_catalogue *catalogue,const struct ae_user_launch *request,
    const struct ae_reservation_receipt *receipt,struct ae_user_launch_snapshot *output);
int ae_catalogue_get_user_launch(struct ae_catalogue *catalogue,const uint8_t operation[16],struct ae_user_launch_snapshot *output);
/* Recovery uses the stored fingerprint and exact root withdrawal/cause after
 * broker replacement; it never resends/rebinds the original reservation. Only
 * before local exec/readiness/additional setup effects; no drain inference or
 * data gate change, and no data/run resurrection. */
int ae_catalogue_finish_withdrawn_operation(struct ae_catalogue *catalogue,const uint8_t operation[16],
    const struct ae_reservation_receipt *receipt,int64_t completed_unix_ms,struct ae_user_launch_snapshot *output);
#endif
