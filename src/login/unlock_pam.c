#define _DEFAULT_SOURCE
#include "unlock_pam.h"

#include <errno.h>
#include <pwd.h>
#include <security/pam_appl.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

enum { ACCOUNT_LIMIT = 256, NSS_BUFFER_SIZE = 16384 };

_Static_assert(AE_UNLOCK_PASSWORD_MAX <= PAM_MAX_RESP_SIZE,
    "prevent pam_unix from truncating unlock passwords");

struct credentials {
    const void *password;
    size_t size;
    bool accepting_password;
    bool answered;
    bool rejected;
};

static int converse(int count, const struct pam_message **messages,
        struct pam_response **response, void *userdata) {
    struct credentials *credential = userdata;
    if (response) *response = NULL;
    if (!response || credential->rejected || !messages ||
            count < 1 || count > PAM_MAX_NUM_MSG) goto reject;

    int prompt = -1;
    for (int i = 0; i < count; ++i) {
        if (!messages[i]) goto reject;
        switch (messages[i]->msg_style) {
        case PAM_TEXT_INFO:
        case PAM_ERROR_MSG:
            break;
        case PAM_PROMPT_ECHO_OFF:
            if (!credential->accepting_password || credential->answered || prompt >= 0) goto reject;
            prompt = i;
            break;
        default:
            goto reject;
        }
    }
    struct pam_response *answers = calloc((size_t)count, sizeof(*answers));
    if (!answers) goto reject;
    if (prompt >= 0) {
        char *password = malloc(credential->size + 1);
        if (!password) {
            free(answers);
            goto reject;
        }
        memcpy(password, credential->password, credential->size);
        password[credential->size] = '\0';
        answers[prompt].resp = password;
        credential->answered = true;
    }
    /* PAM owns these allocations after success. The verdict never contains
       prompt text or a copy of any credential. */
    *response = answers;
    return PAM_SUCCESS;

reject:
    credential->rejected = true;
    return PAM_CONV_ERR;
}

static int account_matches(uid_t uid, const char *expected) {
    struct passwd entry, *found = NULL;
    char storage[NSS_BUFFER_SIZE];
    if (getpwuid_r(uid, &entry, storage, sizeof storage, &found)) return -EIO;
    if (!found || entry.pw_uid != uid || strcmp(entry.pw_name, expected)) return -EACCES;
    if (getpwnam_r(expected, &entry, storage, sizeof storage, &found)) return -EIO;
    if (!found || entry.pw_uid != uid || strcmp(entry.pw_name, expected)) return -EACCES;
    return 0;
}

static int check_pam_user(pam_handle_t *handle, const char *expected) {
    const void *user = NULL;
    if (pam_get_item(handle, PAM_USER, &user) != PAM_SUCCESS) return PAM_SYSTEM_ERR;
    return user && !strcmp(user, expected) ? PAM_SUCCESS : PAM_USER_UNKNOWN;
}

static int verdict(int status) {
    switch (status) {
    case PAM_SUCCESS: return 0;
    case PAM_AUTH_ERR:
    case PAM_USER_UNKNOWN:
    case PAM_PERM_DENIED:
    case PAM_ACCT_EXPIRED:
    case PAM_CRED_EXPIRED:
    case PAM_MAXTRIES:
    case PAM_NEW_AUTHTOK_REQD: return -EACCES;
    default: return -EIO;
    }
}

int ae_unlock_pam_verify(uid_t uid, const char *account, const void *password, size_t size) {
    if (!account || !password || size > AE_UNLOCK_PASSWORD_MAX || memchr(password, '\0', size))
        return -EINVAL;
    size_t length = strnlen(account, ACCOUNT_LIMIT + 1u);
    if (!length || length > ACCOUNT_LIMIT || uid == (uid_t)-1) return -EINVAL;
    if (!size || uid == 0 || !strcmp(account, "aetheri-greeter")) return -EACCES;

    char expected[ACCOUNT_LIMIT + 1u];
    memcpy(expected, account, length + 1u);
    int result = account_matches(uid, expected);
    if (result) return result;

    struct credentials credential = {.password = password, .size = size};
    struct pam_conv conversation = {.conv = converse, .appdata_ptr = &credential};
    pam_handle_t *handle = NULL;
    int status = pam_start("aetheri-unlock", expected, &conversation, &handle);
    if (status != PAM_SUCCESS) return -EIO;

    if (!credential.rejected) {
        credential.accepting_password = true;
        status = pam_authenticate(handle, PAM_DISALLOW_NULL_AUTHTOK);
        credential.accepting_password = false;
        if (status == PAM_SUCCESS && !credential.answered) status = PAM_AUTH_ERR;
        if (status == PAM_SUCCESS && !credential.rejected) status = check_pam_user(handle, expected);
        if (status == PAM_SUCCESS && !credential.rejected) {
            status = pam_acct_mgmt(handle, PAM_DISALLOW_NULL_AUTHTOK);
            if (status == PAM_SUCCESS) status = check_pam_user(handle, expected);
        }
    }
    if (credential.rejected) status = PAM_CONV_ERR;
    if (pam_end(handle, status) != PAM_SUCCESS || credential.rejected) return -EIO;
    result = verdict(status);
    return result ? result : account_matches(uid, expected);
}
