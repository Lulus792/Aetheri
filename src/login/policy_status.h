#ifndef AETHERI_LOGIN_POLICY_STATUS_H
#define AETHERI_LOGIN_POLICY_STATUS_H

#include <stdint.h>

struct ae_login_policy_status;

/* Hold the enforcing kernel's original selinuxfs status page. The sequence is
   boot-local and changes on policy reload and enforcing-mode transitions.
   Single original process/thread. The caller admits the host kernel view. */
int ae_login_policy_status_open(struct ae_login_policy_status **);
/* The original sequence is returned only while unchanged and enforcing.
   Loss permanently fences this handle. Output stays unchanged on error. */
int ae_login_policy_status_sequence(void *,uint32_t *);
void ae_login_policy_status_close(struct ae_login_policy_status **);

#endif
