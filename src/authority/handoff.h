#ifndef AETHERI_AUTHORITY_HANDOFF_H
#define AETHERI_AUTHORITY_HANDOFF_H
#include "catalogue/handoff.h"
#include "linux/lifetime.h"

struct ae_handoff_service;
struct ae_handoff_transfer {
    struct ae_handoff_result journal;
    int handles[AE_LIFETIME_HANDLE_COUNT];
};
#define AE_HANDOFF_TRANSFER_INIT { .handles={-1,-1,-1,-1} }
/* Root-only, process-owned, single-threaded bridge. Borrow the exclusive owner
 * catalogue and trusted host bus. The caller independently admits its protected
 * current custodian image/unit/domain and continuous original-unit retention;
 * self observation corroborates that binding, never creates it. Original profile
 * and current catalogue runtime must already be admitted. No service starts,
 * configuration provisioning or wire API here. The optional four-slot supervisor
 * transport below is separate from the raw prepare/restore boundary.
 * Observed actor/runtime drift latches ESTALE; matching later is not recovery. */
int ae_handoff_service_new(struct ae_catalogue *,sd_bus *,const struct ae_execution_profile *,
    const struct ae_process_context *admitted_self,struct ae_handoff_service **);
/* Original runtime only. Export actual original objects, join them to the
 * immutable enrolled app witness, then commit PREPARE before returning handles.
 * The output handles must all start -1. ONLY a confirmed fresh intent returns
 * four owned CLOEXEC duplicates and transfer_required=1. Every existing-intent
 * retry returns no handles/permission to transfer. Unknown commit closes these
 * local duplicates, preserves the original observer/SQL obligation and changes
 * no output. A lost reply never permits automatic repeat transfer. */
int ae_handoff_service_prepare(struct ae_handoff_service *,struct ae_lifetime *,int64_t,struct ae_handoff_transfer *);
/* Replacement runtime and new invocation of the SAME admitted custodian only.
 * Caller first admits the complete exact supervisor inventory/transport (no
 * unknown/extra/partial set) and serializes old/new use. Borrow four distinct
 * CLOEXEC FDs in lifetime slot order. Load the original expectation from SQL,
 * validate actual restored objects and current self binding, then commit OBSERVE.
 * Never derive expectations from delivered names/objects. Success owns private
 * observer duplicates; incoming FDs remain caller-owned on every path. Both
 * outputs stay unchanged on errors, including lost successful commits. This
 * observes original handles, not readiness, run adoption, drain or lease release. */
int ae_handoff_service_restore(struct ae_handoff_service *,const int *,size_t,int64_t,
    struct ae_lifetime **,struct ae_handoff_snapshot *);
/* Explicit single-profile P0 transport, NOT a general store allocator. Caller
 * admits the fixed protected unit/image/environment and continuous retention.
 * Require NotifyAccess=main, preserve=yes and exactly four slots. Labels derive
 * from the original profile digest; labels/counters/barriers never admit objects.
 * SEND joins fresh PREPARE before four one-shot FDPOLL=0 notifications. An existing
 * intent returns EALREADY without sending. Unknown/partial effects stay pending;
 * do not automatically retry. Success is processing/count evidence only.
 * RECEIVE requires the complete four-name PID1 activation inventory; rejects
 * unknown/duplicate/missing names and reorders before actual restore/OBSERVE.
 * Supplied FDs remain caller-owned, including after successful receive.
 * RETIRE requires a successful RECEIVE plus independent future-effect fencing,
 * closure of all local/exported copies and native terminal/released run closure.
 * Only a fresh retirement intent sends removal notifications. A retiring retry
 * may acknowledge an already empty store, never repeat a partial removal. A
 * completed retry is historical and never touches a later store. All errors
 * preserve outputs; no function releases a lease or starts/stops a process. */
enum { AE_HANDOFF_NAME_BYTES=112 };
int ae_handoff_service_send(struct ae_handoff_service *,struct ae_lifetime *,int64_t,struct ae_handoff_snapshot *);
int ae_handoff_service_receive(struct ae_handoff_service *,const int *,const char *const *,size_t,int64_t,
    struct ae_lifetime **,struct ae_handoff_snapshot *);
int ae_handoff_service_retire_store(struct ae_handoff_service *,int64_t,struct ae_handoff_snapshot *);
/* Local object only; never close borrowed bus/store, retire PID1 entries,
 * stop/unref a unit, release a lease or reconcile an uncertain effect. */
int ae_handoff_service_close(struct ae_handoff_service **);
#endif
