#include "login/installed.h"
#include <errno.h>
#include <openssl/evp.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "line %d: %s\n", __LINE__, #condition); abort(); \
} } while (0)

int main(int argc, char **argv)
{
    CHECK(argc == 2);
    CHECK(getuid() && getuid() == geteuid());
    const uint8_t test_seed[32] = {11}, release[AE_LOGIN_SELECTION_RELEASE_BYTES] = {42};
    uint8_t key[AE_LOGIN_SELECTION_KEY_BYTES];
    EVP_PKEY *signer = EVP_PKEY_new_raw_private_key_ex(NULL, "ED25519", NULL, test_seed, sizeof test_seed);
    CHECK(signer);
    size_t size = sizeof key;
    CHECK(EVP_PKEY_get_raw_public_key(signer, key, &size) == 1 && size == sizeof key);
    EVP_PKEY_free(signer);

    struct ae_login_installed *installed = NULL;
    int result = ae_login_installed_open(key, release, &installed);
    bool complete = !strcmp(argv[1], "backend");
    if (complete || !strncmp(argv[1], "pam-", 4)) {
        CHECK(!result && installed);
        struct ae_login_source_expect inputs[AE_LOGIN_INSTALLED_BACKEND_COUNT];
        CHECK(!ae_login_installed_backend(installed, inputs));
        CHECK(!strcmp(inputs[AE_LOGIN_INSTALLED_USER_PAM].directory, "/etc/pam.d"));
        CHECK(!strcmp(inputs[AE_LOGIN_INSTALLED_USER_PAM].leaf, "aetheri-login"));
        CHECK(!strcmp(inputs[AE_LOGIN_INSTALLED_GREETER_PAM].leaf, "aetheri-greeter"));
        CHECK(!strcmp(inputs[AE_LOGIN_INSTALLED_DEFAULT_PAM].directory, "/etc/pam.d"));
        CHECK(!strcmp(inputs[AE_LOGIN_INSTALLED_DEFAULT_PAM].leaf, "other"));
        struct ae_login_sources *sources = NULL;
        int expected = complete ? 0 : -atoi(argv[1] + 4);
        result = complete ? ae_login_sources_create(inputs, AE_LOGIN_INSTALLED_BACKEND_COUNT, &sources)
                          : ae_login_sources_create(&inputs[AE_LOGIN_INSTALLED_USER_PAM],
                              AE_LOGIN_INSTALLED_DEFAULT_PAM - AE_LOGIN_INSTALLED_USER_PAM + 1, &sources);
        CHECK(result == expected && (sources != NULL) == (expected == 0));
        if (sources) {
            uint8_t first[AE_SHA256_BYTES], second[AE_SHA256_BYTES];
            CHECK(!ae_login_sources_fingerprint(sources, first));
            CHECK(!ae_login_sources_fingerprint(sources, second));
            CHECK(!memcmp(first, second, sizeof first));
        }
        CHECK(!ae_login_sources_close(&sources) && !ae_login_installed_close(&installed));
        return 0;
    }
    if (strcmp(argv[1], "replacement")) {
        CHECK(result == -atoi(argv[1]) && !installed);
        return 0;
    }
    if (result) fprintf(stderr, "installed open: %d\n", result);
    CHECK(!result && installed);
    struct ae_login_selection selected;
    CHECK(!ae_login_installed_read(installed, &selected));
    CHECK(selected.uid == 992 && selected.gid == 993 && selected.group_count == 2);
    CHECK(selected.groups[0] == 993 && selected.groups[1] == 60000);
    struct ae_login_source_expect backend[AE_LOGIN_INSTALLED_BACKEND_COUNT], display;
    CHECK(!ae_login_installed_backend(installed, backend));
    CHECK(!ae_login_installed_display(installed, &display));
    CHECK(!strcmp(backend[AE_LOGIN_INSTALLED_FRAGMENT].directory, "/usr/lib/systemd/system"));
    CHECK(!strcmp(backend[AE_LOGIN_INSTALLED_CONFIG].directory, "/usr/share/aetheri/login"));
    CHECK(!strcmp(backend[AE_LOGIN_INSTALLED_SYSTEMD_DROPIN].leaf, "10-timeout-abort.conf"));
    CHECK(backend[AE_LOGIN_INSTALLED_SYSTEMD_DROPIN].mode == 0644);
    CHECK(!strcmp(backend[AE_LOGIN_INSTALLED_WORKER_DROPIN].leaf, "20-session-shutdown.conf"));
    CHECK(backend[AE_LOGIN_INSTALLED_GREETER].kind == AE_LOGIN_SOURCE_ELF);
    CHECK(!memcmp(backend[AE_LOGIN_INSTALLED_GREETER].digest,
                  selected.files[AE_LOGIN_SELECT_GREETER].digest, AE_SHA256_BYTES));
    CHECK(!strcmp(display.directory, "/usr/libexec/aetheri"));
    CHECK(!strcmp(display.leaf, "aetheri-login-display"));
    CHECK(!memcmp(display.digest, selected.files[AE_LOGIN_SELECT_DISPLAY].digest, AE_SHA256_BYTES));
    CHECK(puts("ready") >= 0 && !fflush(stdout));
    CHECK(getchar() == 'C');
    CHECK(ae_login_installed_check(installed) == -ESTALE);
    CHECK(puts("fenced") >= 0 && !fflush(stdout));
    CHECK(getchar() == 'R');
    memset(&backend, 0xa5, sizeof backend);
    memset(&display, 0xa5, sizeof display);
    memset(&selected, 0xa5, sizeof selected);
    CHECK(ae_login_installed_backend(installed, backend) == -ESTALE);
    CHECK(ae_login_installed_display(installed, &display) == -ESTALE);
    CHECK(ae_login_installed_read(installed, &selected) == -ESTALE);
    CHECK(((const unsigned char *)&backend)[0] == 0xa5);
    CHECK(((const unsigned char *)&display)[0] == 0xa5);
    CHECK(((const unsigned char *)&selected)[0] == 0xa5);
    CHECK(!ae_login_installed_close(&installed) && !installed);
    return 0;
}
