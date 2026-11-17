#ifndef AETHERI_LOGIN_GREETER_H
#define AETHERI_LOGIN_GREETER_H
#include "login/service.h"
#include "login/sources.h"

struct ae_login_greeter;
enum { AE_LOGIN_GREETER_GROUPS_MAX=16 };
/* Compare the calling process's actual supplementary groups with the
   independently selected set in strictly increasing order. */
int ae_login_greeter_check_groups(const gid_t *expected,size_t count);
struct ae_login_greeter_expect {
    uid_t uid; /* Independently selected dedicated, non-Root greeter account. */
    gid_t gid;
    /* Independently selected supplementary groups, strictly increasing and
       nonzero. Empty is permitted; never derive this list from the candidate.
       Copied at construction and compared as a set on every identity check. */
    const gid_t *groups;
    size_t group_count;
    const char *label; /* Exact current raw greeter process label. */
    const char *worker_label; /* Exact raw Root PAM-worker process label. */
    size_t program; /* Independently selected ELF in the original source set. */
};
/* Self-only, on the process leader's thread. Borrow the original socket service
   and protected source set until close. Pin self, then ONLY its kernel-observed
   immediate parent. Require that live Root parent's own kernel parent to be the
   original Root service MainPID. Recheck all UID/GID values, labels, original
   parent relationship/cgroups, exact supplementary groups, no self capabilities,
   and the actual self ELF.
   The reviewed backend/PAM/SELinux policy must independently establish that this
   ancestry is its intended greeter, exclude hostile exec/ptrace/code injection,
   and admit host proc/PID/mount views and loaded DSOs. Ancestry alone neither
   verifies the Root worker's ELF nor supplies that policy. This observer does
   not require parent/greeter to share the backend cgroup: PAM may move sessions.
   No caller PID, environment-derived identity, process control or credentials.
   Output starts NULL and remains unchanged on failure. No automatic reconnect. */
int ae_login_greeter_create(struct ae_login_service *,struct ae_login_sources *,
                            const struct ae_login_greeter_expect *,struct ae_login_greeter **);
/* Compose with loaded-config/protected-handoff and effective-policy checks in
   connection_source.check. This is not complete login admission on its own.
   Same original process/thread. First observed loss permanently fences even
   after restoration; borrowed service/source failures retain their own fences. */
int ae_login_greeter_check(void *);
/* Local cleanup only, never stop/cancel/drain. Fork child may close its copy. */
int ae_login_greeter_close(struct ae_login_greeter **);
#endif
