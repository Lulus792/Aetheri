#define _GNU_SOURCE
#define main ae_test_greeter_main
#include "../src/login/greeter_main.c"
#undef main
#include "login/installed.h"
#include <errno.h>
#include <openssl/evp.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "installed greeter line %d: %s\n", __LINE__, #condition); abort(); \
} } while (0)

static struct ae_login_release_anchor trust = {.release = {42}};
static const char *scenario;
static int runner_calls, platform_calls;

static int platform(void *context)
{
    CHECK(context == &trust && getuid() == 1000);
    ++platform_calls;
    return strcmp(scenario, "platform") ? 0 : -EACCES;
}

int ae_login_greeter_deployment_select(struct ae_login_greeter_profile *out)
{
    const uint8_t seed[32] = {11};
    EVP_PKEY *signer = EVP_PKEY_new_raw_private_key_ex(NULL, "ED25519", NULL, seed, sizeof seed);
    CHECK(signer);
    size_t size = sizeof trust.key;
    CHECK(EVP_PKEY_get_raw_public_key(signer, trust.key, &size) == 1 && size == sizeof trust.key);
    EVP_PKEY_free(signer);
    *out = (struct ae_login_greeter_profile) {
        .installed_release = &trust, .backend = {.check = platform, .userdata = &trust},
    };
    return 0;
}

int __wrap_ae_login_greeter_run(const struct ae_login_greeter_profile *profile,
                               enum ae_login_ui_end *out)
{
    CHECK(++runner_calls == 1 && platform_calls == 1);
    CHECK(!strcmp(scenario, "same") || !strcmp(scenario, "font-replaced"));
    CHECK(profile->backend.source_count == AE_LOGIN_INSTALLED_BACKEND_COUNT);
    CHECK(!strcmp(profile->backend.sources[AE_LOGIN_INSTALLED_SELECTION].leaf, "selection.bin"));
    CHECK(profile->backend.sources[AE_LOGIN_INSTALLED_SELECTION].size == AE_LOGIN_SELECTION_BYTES);
    CHECK(profile->backend.greeter.uid == 1000 && profile->backend.greeter.gid == 1000);
    CHECK(!profile->backend.greeter.group_count);
    CHECK(profile->backend.greeter.program == AE_LOGIN_INSTALLED_GREETER);
    CHECK(!strcmp(profile->compositor.leaf, "aetheri-login-display"));
    CHECK(!strcmp(profile->runtime, "/run/aetheri/login-greeter"));
    CHECK(!strcmp(profile->seat, "seat0") && profile->vt == 8);
    CHECK(profile->font_size > 65536 && !memcmp(profile->font, "\0\1\0\0", 4));
    CHECK(!profile->backend.check(profile->backend.userdata));
    if (!strcmp(scenario, "font-replaced")) {
        CHECK(puts("font-ready") >= 0 && !fflush(stdout));
        CHECK(getchar() == 'C');
        CHECK(profile->backend.check(profile->backend.userdata) == -ESTALE);
        CHECK(puts("font-fenced") >= 0 && !fflush(stdout));
        CHECK(getchar() == 'R');
        CHECK(profile->backend.check(profile->backend.userdata) == -ESTALE);
        return -ESTALE;
    }
    *out = AE_LOGIN_UI_STOPPED;
    return 0;
}

int main(int argc, char **argv)
{
    CHECK(argc == 2 && getuid() == 1000);
    scenario = argv[1];
    CHECK(!strcmp(scenario, "same") || !strcmp(scenario, "font-replaced") ||
          !strcmp(scenario, "font-content") || !strcmp(scenario, "font-label") ||
          !strcmp(scenario, "uid") || !strcmp(scenario, "groups") || !strcmp(scenario, "platform"));
    int expected = strcmp(scenario, "same") ? 1 : 0;
    CHECK(ae_test_greeter_main(1, argv) == expected);
    CHECK(runner_calls == (!strcmp(scenario, "same") || !strcmp(scenario, "font-replaced")));
    return 0;
}
