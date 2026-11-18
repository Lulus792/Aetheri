#ifndef AETHERI_LOGIN_SELECTION_H
#define AETHERI_LOGIN_SELECTION_H

#include "core/digest.h"
#include <stddef.h>
#include <stdint.h>

enum ae_login_selection_role {
    AE_LOGIN_SELECT_UNIT,
    AE_LOGIN_SELECT_STARTUP_UNIT,
    AE_LOGIN_SELECT_SYSTEMD_DROPIN,
    AE_LOGIN_SELECT_WORKER_DROPIN,
    AE_LOGIN_SELECT_BACKEND,
    AE_LOGIN_SELECT_CONFIG,
    AE_LOGIN_SELECT_GREETER,
    AE_LOGIN_SELECT_DISPLAY,
    AE_LOGIN_SELECT_FONT,
    AE_LOGIN_SELECT_USER_PAM,
    AE_LOGIN_SELECT_GREETER_PAM,
    AE_LOGIN_SELECT_SESSION,
    AE_LOGIN_SELECT_COMPOSITOR,
    AE_LOGIN_SELECT_DESKTOP,
    AE_LOGIN_SELECT_STARTUP,
    AE_LOGIN_SELECT_KERNEL_POLICY,
    AE_LOGIN_SELECT_SETTINGS,
    AE_LOGIN_SELECT_FILES,
    AE_LOGIN_SELECT_TERMINAL,
    AE_LOGIN_SELECT_PTY_STARTER,
    AE_LOGIN_SELECT_SHELL,
    AE_LOGIN_SELECT_EDITOR,
    AE_LOGIN_SELECT_SOUND,
    AE_LOGIN_SELECT_LOCK_VIEW,
    AE_LOGIN_SELECT_UNLOCK_PAM,
    AE_LOGIN_SELECT_COUNT
};

enum {
    AE_LOGIN_SELECTION_KEY_BYTES = 32,
    AE_LOGIN_SELECTION_RELEASE_BYTES = 32,
    AE_LOGIN_SELECTION_SIGNATURE_BYTES = 64,
    AE_LOGIN_SELECTION_GROUPS_MAX = 16,
    AE_LOGIN_SELECTION_BODY_BYTES = 51 + 4 * AE_LOGIN_SELECTION_GROUPS_MAX +
                                    (4 + AE_SHA256_BYTES) * AE_LOGIN_SELECT_SOUND,
    AE_LOGIN_SELECTION_BYTES = AE_LOGIN_SELECTION_BODY_BYTES + AE_LOGIN_SELECTION_SIGNATURE_BYTES,
    AE_LOGIN_SELECTION_SOUND_BODY_BYTES = AE_LOGIN_SELECTION_BODY_BYTES + 4 + AE_SHA256_BYTES,
    AE_LOGIN_SELECTION_SOUND_BYTES = AE_LOGIN_SELECTION_SOUND_BODY_BYTES + AE_LOGIN_SELECTION_SIGNATURE_BYTES,
    AE_LOGIN_SELECTION_LOCK_BODY_BYTES = 51 + 4 * AE_LOGIN_SELECTION_GROUPS_MAX +
                                         (4 + AE_SHA256_BYTES) * AE_LOGIN_SELECT_COUNT,
    AE_LOGIN_SELECTION_LOCK_BYTES = AE_LOGIN_SELECTION_LOCK_BODY_BYTES + AE_LOGIN_SELECTION_SIGNATURE_BYTES
};

struct ae_login_selected_file {
    uint32_t size;
    uint8_t digest[AE_SHA256_BYTES];
};

struct ae_login_selection {
    uint32_t uid, gid;
    size_t group_count;
    uint32_t groups[AE_LOGIN_SELECTION_GROUPS_MAX];
    struct ae_login_selected_file files[AE_LOGIN_SELECT_COUNT];
};

/* Verify and decode one exact signed record. The caller supplies independently
 * selected public key and release identifier, e.g. from the protected build.
 * All inputs stay immutable during the call. Output stays unchanged on failure.
 * The result is an expectation only. Verify its fixed paths and live objects
 * separately before admitting a deployment. */
int ae_login_selection_verify(const uint8_t *bytes, size_t size,
    const uint8_t public_key[AE_LOGIN_SELECTION_KEY_BYTES],
    const uint8_t release[AE_LOGIN_SELECTION_RELEASE_BYTES], struct ae_login_selection *out);

#endif
