#define _GNU_SOURCE
#define main ae_test_startup_main
#include "../src/login/main.c"
#undef main
#include "login/installed.h"
#include <errno.h>
#include <openssl/evp.h>
#include <sched.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>

/* Invoked only by the privileged private VM mount fixture. The executable
 * entry, release verification and kernel policy are real. The wrapper stands
 * in for the separately tested original backend/systemd admission. */
#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "installed startup line %d: %s\n", __LINE__, #condition); abort(); \
} } while (0)

static struct ae_login_release_anchor trust = {.release = {42}};
static const char *next_share;
static const char *scenario;
static int platform_calls, runner_calls;

static int platform(void *context)
{
    CHECK(context == &trust && !getuid() && !geteuid());
    ++platform_calls;
    return strcmp(scenario, "platform") ? 0 : -EACCES;
}

int ae_login_deployment_select(struct ae_login_run_profile *out)
{
    const uint8_t seed[32] = {11};
    EVP_PKEY *signer = EVP_PKEY_new_raw_private_key_ex(NULL, "ED25519", NULL, seed, sizeof seed);
    CHECK(signer);
    size_t size = sizeof trust.key;
    CHECK(EVP_PKEY_get_raw_public_key(signer, trust.key, &size) == 1 && size == sizeof trust.key);
    EVP_PKEY_free(signer);
    *out = (struct ae_login_run_profile) {
        .installed_release = &trust, .startup = {.check = platform, .userdata = &trust},
    };
    return 0;
}

int __wrap_ae_login_startup_run(const struct ae_login_run_profile *p)
{
    CHECK(++runner_calls == 1 && platform_calls == 1);
    CHECK(!strcmp(p->unit, "aetheri-login-startup.service"));
    CHECK(!strcmp(p->startup.service.unit, "aetheri-login.service"));
    CHECK(p->startup.source_count == AE_LOGIN_INSTALLED_BACKEND_COUNT);
    CHECK(p->startup.dropin_count == 2 && p->startup.dropins[0] == AE_LOGIN_INSTALLED_SYSTEMD_DROPIN);
    CHECK(p->startup.dropins[1] == AE_LOGIN_INSTALLED_WORKER_DROPIN);
    int result = p->startup.check(p->startup.userdata);
    if (result) fprintf(stderr, "initial gate: %d\n", result);
    if (!strcmp(scenario, "policy")) {
        CHECK(result == -EKEYREJECTED);
        return result;
    }
    CHECK(!result);
    CHECK(!unshare(CLONE_NEWNS));
    CHECK(!mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL));
    CHECK(!mount(next_share, "/usr/share", NULL, MS_BIND, NULL));
    result = p->startup.enter_view(p->startup.userdata);
    if (!strcmp(scenario, "different")) {
        CHECK(result == -ESTALE);
        return result;
    }
    CHECK(!result);
    CHECK(p->startup.enter_view(p->startup.userdata) == -EALREADY);
    CHECK(!p->startup.check(p->startup.userdata) && platform_calls == 3);
    return 0;
}

int main(int argc, char **argv)
{
    CHECK(argc == 3 && !getuid());
    next_share = argv[1];
    scenario = argv[2];
    CHECK(!strcmp(scenario, "same") || !strcmp(scenario, "different") ||
          !strcmp(scenario, "policy") || !strcmp(scenario, "platform"));
    int expected = strcmp(scenario, "same") ? 1 : 0;
    CHECK(ae_test_startup_main(1, argv) == expected);
    CHECK(runner_calls == (strcmp(scenario, "platform") ? 1 : 0));
    return 0;
}
