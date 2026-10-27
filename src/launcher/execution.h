#ifndef AETHERI_LAUNCHER_EXECUTION_H
#define AETHERI_LAUNCHER_EXECUTION_H
#include "catalogue/execution.h"
#include "linux/process.h"

struct ae_execution_client;
/* Fresh single-threaded Root starter, trusted host system bus and independently
 * protected current Root unit/profile selection only. No app-selected parameters.
 * Resolve System1 once, address its unique owner and verify actual reply process,
 * active unit/invocation/cgroup/boot/full domain plus discovery epoch/runtime.
 * Borrows bus; copies profile/unit; owns one original Root process pin.
 * Does not install a unit/profile/policy, reserve/arm SQL or authorize an app.
 * Output must initially be NULL and stays unchanged on every failure. */
int ae_execution_client_new(sd_bus *,const struct ae_execution_profile *,const char *selected_root_unit,
    struct ae_execution_client **);
/* At most one request per client. Return a separately owned anonymous Root pipe
 * NONBLOCK/CLOEXEC writer only after authenticated exact binding receipt. Output
 * must initially be negative. Caller owns/keeps it across preparation and seals
 * it at one-way exec; close on failure. This object retains no writer copy.
 * Timeout/unknown/invalid reply never authorizes retry or execution. */
/* The attempt guard is per client, not durable. Do not recreate a client to
 * retry uncertainty; the issuer's journal/profile must reject lifetime replay. */
int ae_execution_client_register(struct ae_execution_client *,int *writer);
/* Called once after independently checked private-view/image preparation.
 * The Root producer must hold this reply until fresh durable ARM. Success only
 * accepts that authenticated release receipt; it is not exec or app readiness.
 * Failure means no exec; preserve the Root journal/lease for reconciliation. */
int ae_execution_client_prepared(struct ae_execution_client *);
/* Local cleanup only; never stop a process, withdraw a dispatch or free a lease.
 * Cannot close the writer already transferred to the caller. */
int ae_execution_client_close(struct ae_execution_client **);
#endif
