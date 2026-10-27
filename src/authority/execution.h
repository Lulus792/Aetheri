#ifndef AETHERI_AUTHORITY_EXECUTION_H
#define AETHERI_AUTHORITY_EXECUTION_H
#include "catalogue/execution.h"
#include "linux/process.h"
#include "linux/exec_channel.h"
#include <systemd/sd-event.h>

struct ae_execution_service;
/* Compiled trusted supervisor/IPC adapters, never client-supplied callbacks.
 * Each is called only after the corresponding fresh owner commit. Request
 * starts only the protected unit derived from launch; release addresses only
 * its already prepared original starter. Return zero or negative errno.
 * The caller owes protected image/profile provisioning and current User data
 * admission. Effects must not destroy the coordinator; close returns EBUSY
 * while one is active. This coordinator does not provision paths/policy or prove readiness. */
typedef int (*ae_execution_effect)(void *,const struct ae_execution_profile *,const struct ae_dispatch_snapshot *);
struct ae_execution_effects { void *owner; ae_execution_effect request,release; };
struct ae_execution_service_status {
    /* Persisted dispatch is authoritative. Local stage flags describe this
     * coordinator's acknowledgements; a lost commit can advance SQL while a
     * local flag stays false. None is app execution/readiness evidence. */
    struct ae_dispatch_snapshot dispatch;
    struct ae_exec_report channel;
    int request_error,bind_error,release_error,channel_error,context_error,report_error;
    int requested,bound,prepared,arm_attempted,armed;
};
/* Root UID/GID 0 only. Borrow catalogue/system bus/event for this single-threaded owner's lifetime.
 * Protected profile and authenticated broker context must be independently
 * obtained by the live authority. No wire API, installed policy or service start
 * is implied by these internal native functions. Output must initially be NULL. */
int ae_execution_service_new(struct ae_catalogue *,sd_bus *,sd_event *,
    const struct ae_execution_profile *,const struct ae_launch_context *,const struct ae_execution_effects *,
    struct ae_execution_service **);
int ae_execution_service_request(struct ae_execution_service *,const struct ae_launch_context *,struct ae_execution_result *);
/* Borrow a fresh ae_peer_capture from the selected starter's method call.
 * Re-observe the held process via PID1; exact UID/domain/boot/unit are mandatory.
 * Pin invocation before BIND, retain original ProcessFD, create/watch own reader
 * and return its sole writer only after successful journal binding. On any
 * ambiguous bind, retain all obligations and never issue another writer.
 * The writer output must not already own an FD (initial negative value).
 * Caller must close its local/message writer copies after the one reply. */
int ae_execution_service_bind(struct ae_execution_service *,const struct ae_peer *,int *writer,struct ae_execution_result *);
int ae_execution_service_prepared(struct ae_execution_service *,const struct ae_peer *,struct ae_execution_result *);
/* Release callback must already address the same pending prepared starter;
 * callback failure/unknown result is retained, with no automatic second arm.
 * These callbacks never execute inside a catalogue transaction. */
int ae_execution_service_arm(struct ae_execution_service *,const struct ae_launch_context *,struct ae_execution_result *);
int ae_execution_service_status(struct ae_execution_service *,const struct ae_launch_context *,struct ae_execution_service_status *);
/* Retry only the immutable diagnostic acknowledgement; never another process
 * effect, writer issuance or arm. Used automatically on terminal observation. */
int ae_execution_service_record(struct ae_execution_service *,const struct ae_launch_context *);
/* Stop event watch/close local pins only. Never stop a process, infer drain,
 * settle an operation or release the persistent generation lease. Observed
 * terminal diagnostics are journaled before close; unobserved state stays unknown. */
int ae_execution_service_close(struct ae_execution_service **);
#endif
