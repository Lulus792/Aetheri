#ifndef AETHERI_LOGIN_UNLOCK_PAM_H
#define AETHERI_LOGIN_UNLOCK_PAM_H

#include <stddef.h>
#include <sys/types.h>

#define AE_UNLOCK_PASSWORD_MAX 512u

/* Verify one password against the account already selected by the trusted
   session owner. The verifier needs the protected local NSS and PAM views.
   This result alone never authorizes an unlock. Bind it to the original live
   lock and attempt before acting. The caller owns and wipes the password.
   Return zero, -EACCES (denied), -EINVAL (invalid input), or -EIO (unavailable). */
int ae_unlock_pam_verify(uid_t uid, const char *account, const void *password, size_t size);

#endif
