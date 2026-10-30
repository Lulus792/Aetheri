#ifndef AETHERI_AUTHORITY_EXECUTION_WIRE_H
#define AETHERI_AUTHORITY_EXECUTION_WIRE_H
#include "authority/execution.h"

struct ae_execution_endpoint;
struct ae_execution_endpoint_status {
    struct ae_execution_service_status execution;
    int prepared_pending,prepared_expired;
    int registration_reply_attempted,release_reply_attempted;
    int registration_reply_error,release_reply_error;
};
/* One protected P0 profile per endpoint/bus. Fresh single-threaded Root owner;
 * borrow catalogue/trusted system bus/event. Caller independently admits the
 * profile/image/unit/private data and current authenticated broker context.
 * Compiled request adapter only, never client code/paths/callbacks. Release is
 * owned here: only a fresh native ARM may consume the original prepared reply.
 * Installs the private Execution1 vtable on this bus, not a name/policy/service.
 * Output initially NULL; unchanged on failure. No target readiness/drain proof. */
int ae_execution_endpoint_new(struct ae_catalogue *,sd_bus *,sd_event *,
    const struct ae_execution_profile *,const struct ae_launch_context *,
    ae_execution_effect request,void *request_owner,struct ae_execution_endpoint **);
int ae_execution_endpoint_request(struct ae_execution_endpoint *,const struct ae_launch_context *,struct ae_execution_result *);
/* Internal owner-side equality check; no permission or process observation. */
int ae_execution_endpoint_check_profile(struct ae_execution_endpoint *,const struct ae_execution_profile *);
/* Caller owes fresh broker/private-data admission. No pending original prepared
 * call or an expired local deadline means no ARM. Calls do not repeat effects. */
int ae_execution_endpoint_arm(struct ae_execution_endpoint *,const struct ae_launch_context *,struct ae_execution_result *);
int ae_execution_endpoint_status(struct ae_execution_endpoint *,const struct ae_launch_context *,struct ae_execution_endpoint_status *);
/* Local detach/unwatch/unref only. No success/error retry, process stop, dispatch
 * settlement or lease release. Busy callbacks cannot destroy their endpoint. */
int ae_execution_endpoint_close(struct ae_execution_endpoint **);
#endif
