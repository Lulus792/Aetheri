#define _DEFAULT_SOURCE
#include "login/unlock_pam.h"

#include <errno.h>
#include <pwd.h>
#include <security/pam_appl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "unlock check failed at line %d\n", __LINE__); exit(1); \
} } while (0)

enum test_case {
    ACCEPT, WRONG_PASSWORD, NO_PROMPT, ECHO_ON, MULTIPLE_PROMPTS, REPEATED_PROMPT,
    RENAMED_IN_AUTH, RENAMED_IN_ACCOUNT, ACCOUNT_EXPIRED, PASSWORD_EXPIRED,
    ACCOUNT_PROMPT, START_FAILURE, END_FAILURE, AUTH_UNAVAILABLE,
};

struct pam_handle { int id; };
static struct pam_handle fake_handle;
static const struct pam_conv *conv;
static enum test_case scenario;
static const char *account;
static const unsigned char *expected_secret;
static size_t expected_size;
static unsigned starts, auth_calls, account_calls, identity_calls, ends;

int __wrap_pam_start(const char *service, const char *user, const struct pam_conv *conversation,
        pam_handle_t **out) {
    CHECK(!strcmp(service, "aetheri-unlock"));
    CHECK(!strcmp(user, account));
    CHECK(!conv);
    ++starts;
    if (scenario == START_FAILURE) return PAM_SYSTEM_ERR;
    conv = conversation;
    *out = &fake_handle;
    return PAM_SUCCESS;
}

static void reply(int second_style, int expected_status) {
    const struct pam_message info = {.msg_style = PAM_TEXT_INFO, .msg = "synthetic message"};
    const struct pam_message hidden = {.msg_style = PAM_PROMPT_ECHO_OFF, .msg = "synthetic password prompt"};
    const struct pam_message second = {.msg_style = second_style, .msg = "additional prompt"};
    const struct pam_message *requests[] = {&info, &hidden, &second};
    struct pam_response *answers = NULL;
    CHECK(conv->conv(second_style ? 3 : 2, requests, &answers, conv->appdata_ptr) == expected_status);
    if (expected_status != PAM_SUCCESS) {
        CHECK(!answers);
        return;
    }
    CHECK(answers && !answers[0].resp && !answers[0].resp_retcode);
    CHECK(answers[1].resp && !answers[1].resp_retcode);
    CHECK(strlen(answers[1].resp) == expected_size);
    CHECK(!memcmp(answers[1].resp, expected_secret, expected_size));
    explicit_bzero(answers[1].resp, expected_size);
    free(answers[1].resp);
    free(answers);
}

int __wrap_pam_authenticate(pam_handle_t *handle, int flags) {
    CHECK(handle == &fake_handle && flags == PAM_DISALLOW_NULL_AUTHTOK);
    ++auth_calls;
    switch (scenario) {
    case NO_PROMPT: return PAM_SUCCESS;
    case AUTH_UNAVAILABLE: return PAM_AUTHINFO_UNAVAIL;
    case ECHO_ON:
        reply(PAM_PROMPT_ECHO_ON, PAM_CONV_ERR);
        return PAM_SUCCESS;
    case MULTIPLE_PROMPTS:
        reply(PAM_PROMPT_ECHO_OFF, PAM_CONV_ERR);
        return PAM_SUCCESS;
    case REPEATED_PROMPT:
        reply(0, PAM_SUCCESS);
        reply(0, PAM_CONV_ERR);
        return PAM_SUCCESS;
    default:
        reply(0, PAM_SUCCESS);
        return scenario == WRONG_PASSWORD ? PAM_AUTH_ERR : PAM_SUCCESS;
    }
}

int __wrap_pam_acct_mgmt(pam_handle_t *handle, int flags) {
    CHECK(handle == &fake_handle && flags == PAM_DISALLOW_NULL_AUTHTOK);
    ++account_calls;
    if (scenario == ACCOUNT_EXPIRED) return PAM_ACCT_EXPIRED;
    if (scenario == PASSWORD_EXPIRED) return PAM_NEW_AUTHTOK_REQD;
    if (scenario == ACCOUNT_PROMPT) reply(0, PAM_CONV_ERR);
    return PAM_SUCCESS;
}

int __wrap_pam_get_item(const pam_handle_t *handle, int type, const void **out) {
    CHECK(handle == &fake_handle && type == PAM_USER);
    ++identity_calls;
    bool changed = scenario == RENAMED_IN_AUTH ||
        (scenario == RENAMED_IN_ACCOUNT && identity_calls == 2);
    *out = changed ? "different-account" : account;
    return PAM_SUCCESS;
}

int __wrap_pam_end(pam_handle_t *handle, int status) {
    CHECK(handle == &fake_handle && conv);
    if (scenario == ECHO_ON || scenario == MULTIPLE_PROMPTS || scenario == REPEATED_PROMPT ||
            scenario == ACCOUNT_PROMPT) CHECK(status == PAM_CONV_ERR);
    ++ends;
    conv = NULL;
    return scenario == END_FAILURE ? PAM_SYSTEM_ERR : PAM_SUCCESS;
}

int main(void) {
    uid_t uid = getuid();
    if (!uid) return 77;
    const struct passwd *entry = getpwuid(uid);
    CHECK(entry && entry->pw_name);
    char *name = strdup(entry->pw_name);
    CHECK(name);
    account = name;
    unsigned char secret[AE_UNLOCK_PASSWORD_MAX + 1];
    memset(secret, 'x', sizeof secret);
    CHECK(ae_unlock_pam_verify(uid, name, secret, sizeof secret) == -EINVAL);
    CHECK(ae_unlock_pam_verify(uid, name, "x\0y", 3) == -EINVAL);
    CHECK(ae_unlock_pam_verify(uid, name, secret, 0) == -EACCES);
    CHECK(ae_unlock_pam_verify(0, name, secret, 5) == -EACCES);
    CHECK(ae_unlock_pam_verify(uid, "root", secret, 5) == -EACCES);
    CHECK(ae_unlock_pam_verify(uid, "aetheri-greeter", secret, 5) == -EACCES);
    CHECK(!starts);

    const struct {
        enum test_case kind;
        int result;
        unsigned auth_count, account_count, end_count;
    } cases[] = {
        {ACCEPT, 0, 1, 1, 1},
        {WRONG_PASSWORD, -EACCES, 1, 0, 1},
        {NO_PROMPT, -EACCES, 1, 0, 1},
        {ECHO_ON, -EIO, 1, 0, 1},
        {MULTIPLE_PROMPTS, -EIO, 1, 0, 1},
        {REPEATED_PROMPT, -EIO, 1, 0, 1},
        {RENAMED_IN_AUTH, -EACCES, 1, 0, 1},
        {RENAMED_IN_ACCOUNT, -EACCES, 1, 1, 1},
        {ACCOUNT_EXPIRED, -EACCES, 1, 1, 1},
        {PASSWORD_EXPIRED, -EACCES, 1, 1, 1},
        {ACCOUNT_PROMPT, -EIO, 1, 1, 1},
        {START_FAILURE, -EIO, 0, 0, 0},
        {END_FAILURE, -EIO, 1, 1, 1},
        {AUTH_UNAVAILABLE, -EIO, 1, 0, 1},
        {ACCEPT, 0, 1, 1, 1},
    };
    expected_secret = secret;
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; ++i) {
        scenario = cases[i].kind;
        expected_size = i == 0 ? AE_UNLOCK_PASSWORD_MAX : 5;
        auth_calls = account_calls = identity_calls = ends = 0;
        CHECK(ae_unlock_pam_verify(uid, name, expected_secret, expected_size) == cases[i].result);
        CHECK(starts == i + 1 && !conv);
        CHECK(auth_calls == cases[i].auth_count && account_calls == cases[i].account_count &&
            ends == cases[i].end_count);
    }
    explicit_bzero(secret, sizeof secret);
    free(name);
    return 0;
}
