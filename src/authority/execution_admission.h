#ifndef AETHERI_AUTHORITY_EXECUTION_ADMISSION_H
#define AETHERI_AUTHORITY_EXECUTION_ADMISSION_H
#include "authority/broker.h"
#include "authority/execution_wire.h"

struct ae_execution_admission;
/* Root-only process-owned adapter; borrow the original endpoint, registered
 * broker and trusted system bus. Protected profile/source/unit provisioning and
 * User's durable private-data preparation remain independent prerequisites.
 * No method table, name, policy, process or readiness capability is installed. */
int ae_execution_admission_new(struct ae_execution_endpoint *,struct ae_broker *,sd_bus *,
    const struct ae_execution_profile *,struct ae_execution_admission **);
/* Borrow an actual captured request from the original registered connection.
 * Re-observe active process/unit/invocation and bind profile epochs/account before
 * native REQUEST. Retain an independent original pin before any commit/effect.
 * Unknown request results retain that obligation and prohibit another attempt. */
int ae_execution_admission_request(struct ae_execution_admission *,const struct ae_peer *,struct ae_execution_result *);
/* Reverify the retained ORIGINAL connection against the still-current registered
 * broker before ARM; a replacement or departure never inherits delegation.
 * User's original request delegates only this profile/data fence. Mutation/stop/
 * revocation coordination and actual data readiness remain caller obligations. */
int ae_execution_admission_arm(struct ae_execution_admission *,struct ae_execution_result *);
/* Local pin cleanup only; no stop, withdrawal, drain or lease settlement. */
int ae_execution_admission_close(struct ae_execution_admission **);
#endif
