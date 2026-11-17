#ifndef AETHERI_LOGIN_STARTUP_H
#define AETHERI_LOGIN_STARTUP_H
#include "login/handoff.h"

struct ae_login_startup;
struct ae_login_startup_expect {
    struct ae_login_service_expect service;
    const struct ae_login_source_expect *sources;
    size_t source_count,fragment,program,config;
    const size_t *dropins;
    size_t dropin_count;
    const char *directory,*leaf,*label;
    /* Mandatory compiled deployment gate. Admit account/PAM/MAC/device policy,
       trusted proc/bus/namespace views, protected runtime/DSOs and the selected
       publication directory/ancestors. Called before namespace entry and again
       in the selected view, on readiness checks and before publication. No test
       callback establishes production policy. Borrow userdata until close. */
    int (*check)(void *userdata);
    /* Optional one-time handover to a new protected observation. Invoked
       after the original backend's namespace is entered and verified, and
       before any source capture there. The callback may not enter another
       namespace or publish. The same check() remains mandatory in both views. */
    int (*enter_view)(void *userdata);
    void *userdata;
};
/* Fresh single-threaded Root startup only. Observe the connected stream's actual
   active system service, obtain its mount namespace through the original kernel
   peer pidfd, retain that namespace and enter it exactly once. Then construct
   the selected source/loaded guards in that same view. Never accepts a PID or
   namespace path; no credential/protocol traffic is sent or consumed.
   IMPORTANT: every failure may leave the process in the selected namespace.
   The caller must EXIT on failure, never continue/fall back or retry startup.
   Success owns guards/FDs and borrows the external gate context. Output starts
   NULL and remains unchanged on failure. This is not installed startup/policy. */
int ae_login_startup_create(int connected_fd,sd_bus *trusted_system_bus,
    const struct ae_login_startup_expect *,struct ae_login_startup **out);
/* Read-only readiness: -EAGAIN only for the original config notice not yet seen.
   Caller owns its bounded wait/deadline; no automatic reconnect/rebind. Require
   original peer and self mount namespaces, service/source/config/current ELF
   and mandatory external gate. Other errors permanently fence this handle. */
int ae_login_startup_check(void *startup);
/* One attempt per handle, including an error. Check readiness again, then use
   create-only handoff publication. Never retry, adopt, remove or infer absence
   from an error; possible named files survive unknown acknowledgements. */
int ae_login_startup_publish(struct ae_login_startup *);
/* Local cleanup only. Never restores a namespace, stops a service or removes a
   possible observation. Fork child can close its local copy; other misuse rejects. */
int ae_login_startup_close(struct ae_login_startup **);
#endif
