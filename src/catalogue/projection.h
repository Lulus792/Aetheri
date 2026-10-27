#ifndef AETHERI_CATALOGUE_PROJECTION_H
#define AETHERI_CATALOGUE_PROJECTION_H

#include "catalogue/store.h"

enum ae_install_gate { AE_INSTALL_GATE_OPEN=0, AE_INSTALL_GATE_CLOSING=1, AE_INSTALL_GATE_CLOSED=2 };
/* One authenticated system-catalogue observation, never a launch ticket. Account
 * is the eligible recipient; scope is the actual account/machine install scope.
 * Persisting a user binding does not persist generation/runtime/gate authority.
 */
struct ae_install_snapshot {
    uint8_t system_epoch[16], system_runtime[16], boot[16], install[16];
    char component[129];
    uint8_t publisher[16], account[16], scope[16], generation[16], artifact[32];
    int64_t install_revision, generation_revision, gate_revision, catalogue_revision;
    enum ae_install_gate gate;
};
struct ae_projection_context {
    uint8_t system_epoch[16], system_runtime[16], boot[16], user_runtime[16];
};
/* Shape validation only: UUIDv4 identities, bounded component token, positive
 * signed-SQL revisions and known gate enum. No transport/provenance verification.
 */
int ae_install_snapshot_valid(const struct ae_install_snapshot *snapshot);
/* Read one consistent active app/committed generation/verified artifact and gate
 * observation. Requires current expected system boot/runtime, an active enrolled
 * account/UID, and either that account's installation or an active machine scope.
 * Caller owes authenticated broker/process/policy context. No transaction spans
 * IPC, no gate opens, no process/storage/readiness is inferred. Output unchanged
 * on error. Unknown/ineligible/inactive installs are ENOENT; context drift ESTALE.
 */
int ae_catalogue_resolve_app(struct ae_catalogue *catalogue,const uint8_t account[16],uint32_t os_uid,
                             const uint8_t install[16],const uint8_t boot[16],const uint8_t runtime[16],
                             struct ae_install_snapshot *output);
/* Trusted user writer after authenticating source and checked RPC context. Insert
 * only its own active install binding from this snapshot. Exact retry changes no
 * rows/revision; changed identity/epoch/revision or a revoked binding requires
 * explicit reconciliation (ESTALE), never silent rebinding/reactivation. Local
 * catalogue boot/runtime and all expected source context must match. Lost COMMIT
 * acknowledgement retains the original row; output stays unchanged on any error.
 * This creates no data epoch/attachment/grant/run, opens no gate and performs no
 * system-catalogue read, filesystem allocation or publisher enrollment.
 */
int ae_catalogue_cache_install(struct ae_catalogue *catalogue,const struct ae_install_snapshot *snapshot,
                               const struct ae_projection_context *context,int64_t *local_revision);

#endif
