#ifndef AETHERI_CATALOGUE_EFFECT_H
#define AETHERI_CATALOGUE_EFFECT_H
#include "catalogue/reservation.h"

struct ae_execution_fence_status { unsigned int active; int fenced; };
/* Local, process-owned, one-way barrier shared by all execution coordinators
 * borrowing this SYSTEM catalogue. No SQL/IPC/filesystem effect and no reopen.
 * Fence acknowledgment excludes NEW locally permitted execution effects; active
 * adapters can still complete/return unknown. Neither active==0 nor this status
 * proves unit/process/transport/export-holder drain or lease release. No durable
 * fence, restart/volume admission or protection from another process is implied.
 * Caller serializes the single-threaded owner and keeps the store alive.
 * Wrong role/fork/invalid output preserves output. Even after filesystem drift,
 * fencing and balancing an existing permit need no new external observation. */
int ae_catalogue_fence_execution(struct ae_catalogue *,struct ae_execution_fence_status *);
int ae_catalogue_execution_fence_status(struct ae_catalogue *,struct ae_execution_fence_status *);
/* Internal compiled-adapter bracket, never an app capability. Check original
 * current Root/broker/account context, named store and the local fence before
 * obtaining a permit. Hold through the entire external effect, end on ALL paths.
 * No SQL transaction may span a callback. Recheck after a fresh intent commit;
 * fencing there preserves that original intent and forbids its effect/replay.
 * End is local cleanup and still works when fenced or named context is lost. */
int ae_catalogue_check_execution_effect(struct ae_catalogue *,const struct ae_launch_context *);
int ae_catalogue_begin_execution_effect(struct ae_catalogue *,const struct ae_launch_context *);
int ae_catalogue_end_execution_effect(struct ae_catalogue *);
#endif
