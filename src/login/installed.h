#ifndef AETHERI_LOGIN_INSTALLED_H
#define AETHERI_LOGIN_INSTALLED_H

#include "login/selection.h"
#include "login/sources.h"

struct ae_login_installed;

struct ae_login_release_anchor {
    uint8_t key[AE_LOGIN_SELECTION_KEY_BYTES];
    uint8_t release[AE_LOGIN_SELECTION_RELEASE_BYTES];
};

enum {
    AE_LOGIN_INSTALLED_FRAGMENT,
    AE_LOGIN_INSTALLED_BACKEND,
    AE_LOGIN_INSTALLED_CONFIG,
    AE_LOGIN_INSTALLED_SYSTEMD_DROPIN,
    AE_LOGIN_INSTALLED_WORKER_DROPIN,
    AE_LOGIN_INSTALLED_GREETER,
    AE_LOGIN_INSTALLED_BACKEND_COUNT
};

/* Providers supply independent build-selected trust anchors. Open the literal
 * installed selection path, authenticate the bytes, then retain and observe the
 * protected file and ancestry. The caller admits the original host view and
 * excludes concurrent privileged namespace writers. Output starts NULL. */
int ae_login_installed_open(const uint8_t key[AE_LOGIN_SELECTION_KEY_BYTES],
    const uint8_t release[AE_LOGIN_SELECTION_RELEASE_BYTES], struct ae_login_installed **out);

/* Follow the original observation. Any observed loss permanently fences access.
 * Read returns copied claims only, including the expected effective policy.
 * Backend returns the common startup/greeter source order. Its drop-ins follow
 * PID1's loaded order. The actual live observations remain a caller obligation.
 * All output stays unchanged on failure. Same original process/thread as open. */
int ae_login_installed_check(void *);
int ae_login_installed_read(struct ae_login_installed *, struct ae_login_selection *);
/* Digest of the authenticated record bytes, independent of mount identities. */
int ae_login_installed_digest(struct ae_login_installed *, uint8_t out[AE_SHA256_BYTES]);
int ae_login_installed_backend(struct ae_login_installed *,
    struct ae_login_source_expect out[AE_LOGIN_INSTALLED_BACKEND_COUNT]);
int ae_login_installed_display(struct ae_login_installed *, struct ae_login_source_expect *);
int ae_login_installed_close(struct ae_login_installed **);

#endif
