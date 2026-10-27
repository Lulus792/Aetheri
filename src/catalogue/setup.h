#ifndef AETHERI_CATALOGUE_SETUP_H
#define AETHERI_CATALOGUE_SETUP_H
#include "catalogue/reservation.h"

/* Internal setup-only P0 profile. No executable/path/label or arbitrary service
 * property is accepted. Root provisioning and authenticated caller provenance
 * are mandatory; decoded bytes alone grant no rights. */
enum { AE_SETUP_BYTES=320 };
struct ae_setup_profile {
    uint8_t system_epoch[16],system_runtime[16],broker_epoch[16],broker_runtime[16],boot[16],account[16];
    uint8_t install[16],generation[16],data[16],storage[16],launch[16],lease[16],root_operation[16],user_operation[16];
    int64_t install_revision,install_gate_revision,data_gate_revision;
    uint8_t artifact[32],image_digest[32];
};
int ae_setup_decode(const uint8_t *bytes,size_t size,struct ae_setup_profile *output);
int ae_catalogue_authorize_user_setup(struct ae_catalogue *store,const struct ae_setup_profile *profile);
int ae_catalogue_begin_setup(struct ae_catalogue *store,const struct ae_setup_profile *profile,
    const uint8_t digest[32],const struct ae_launch_context *context,struct ae_dispatch_snapshot *output);
/* Caller owes actual held ProcessFD + supervisor context for the fixed selected
 * setup-only service. Evidence is a canonical digest of that observed context,
 * never a caller-supplied PID or an environment variable. */
int ae_catalogue_register_setup(struct ae_catalogue *store,const struct ae_setup_profile *profile,
    const uint8_t digest[32],const uint8_t invocation[16],const uint8_t evidence[32],struct ae_dispatch_snapshot *output);
/* Distinct recovery evidence for the selected, immutable no-exec image after an
 * independently verified terminal supervisor invocation. Does NOT claim a held
 * peer was observed. Same-runtime only; never use for an executable app starter. */
int ae_catalogue_record_setup_exit(struct ae_catalogue *store,const struct ae_setup_profile *profile,
    const uint8_t digest[32],const uint8_t invocation[16],const uint8_t evidence[32],struct ae_dispatch_snapshot *output);
int ae_catalogue_prepared_setup(struct ae_catalogue *store,const struct ae_setup_profile *profile,
    const uint8_t digest[32],const uint8_t invocation[16],const uint8_t evidence[32],struct ae_dispatch_snapshot *output);
/* Only the fixed no-exec setup implementation, after actual held process death
 * and independent same-invocation unit/cgroup/job drain. No target execution is
 * inferred from setup readiness/exit. Root restart uncertainty remains held. */
int ae_catalogue_withdraw_setup(struct ae_catalogue *store,const struct ae_setup_profile *profile,
    const uint8_t digest[32],const uint8_t invocation[16],const uint8_t evidence[32],
    const struct ae_launch_context *current,int64_t completed_ms,struct ae_dispatch_snapshot *output);
#endif
