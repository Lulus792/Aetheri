#ifndef AETHERI_LOGIN_RUNNER_H
#define AETHERI_LOGIN_RUNNER_H
#include "login/startup.h"

struct ae_login_run_profile {
    const char *unit,*label; /* Independently admitted startup service and raw label. */
    const char *socket_prefix; /* Reviewed backend's absolute socket-name prefix. */
    uint64_t timeout_usec; /* 100ms..15s; synchronous bus calls have their own bound. */
    struct ae_login_startup_expect startup;
};
/* Finite Root executable entry, fresh single-threaded process. Use the literal
   host system bus, capture this process's original authenticated bus/self unit
   context, then require its active MainPID/no ControlPID/job before effects.
   Freeze the selected backend's manager/name/MainPID/invocation before waiting
   for its socket. That pathname is a hint only: the actual kernel peer/service
   must agree with the original selection before namespace entry/publication.
   One initial connection and publication; no reconnect, service start/stop,
   fallback or retries of an uncertain outcome. Initial readiness waits have one
   monotonic deadline. Every return requires caller EXIT: namespace may differ.
   The compiled external deployment gate is mandatory and remains distinct from
   these observations. Inputs and gate context stay immutable/live until return. */
int ae_login_startup_run(const struct ae_login_run_profile *);

/* Defined only by an explicitly selected build-time deployment C source.
   No default provider, runtime plugin, profile path or environment selection.
   The provider owes independently selected protected deployment/runtime/policy,
   not hashes or identities inferred from the candidate being admitted. */
int ae_login_deployment_select(struct ae_login_run_profile *);
#endif
