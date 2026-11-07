#ifndef AETHERI_AUTHORITY_START_H
#define AETHERI_AUTHORITY_START_H
#include "authority/execution.h"
#include "linux/start.h"

struct ae_supervisor_start;
/* Compiled independent unit/provisioning admission, never app-supplied code or
 * a no-op production policy. Verify immutable selected configuration, complete
 * dependency graph, original unused unit allocation and retained Unit reference/
 * Subscribe ownership on the dedicated trusted host bus before each submission.
 * Current caller/image/private-data and mutation exclusion remain prerequisites. */
typedef int (*ae_supervisor_admit)(void *,const struct ae_execution_profile *,const char *unit);
int ae_supervisor_start_new(struct ae_catalogue *,sd_bus *,const struct ae_peer *admitted_pid1,
    const struct ae_execution_profile *,const struct ae_launch_context *,ae_supervisor_admit,void *,struct ae_supervisor_start **);
/* ae_execution_effect request adapter: only after the coordinator's confirmed
 * fresh REQUEST journal commit. Derive/verify the unit from the original launch,
 * recheck source after admission, and submit once. Success means queued local
 * transport, not a completed PID1 job, target exec or app readiness.
 * A separate cleanup bracket retains this original catalogue/runtime while the
 * submitted request/job remains unsettled, even after the source callback ends.
 * Known original reply+JobRemoved release that local hold only, never a lease. */
int ae_supervisor_start_request(void *,const struct ae_execution_profile *,const struct ae_dispatch_snapshot *);
int ae_supervisor_start_status(struct ae_supervisor_start *,struct ae_start_status *);
int ae_supervisor_start_close(struct ae_supervisor_start **);
/* Explicit local observer retirement. Does not cancel pending PID1 work, Unref
 * the caller's unit, settle SQL, restore a gate or release a generation lease. */
int ae_supervisor_start_abandon(struct ae_supervisor_start **);
#endif
