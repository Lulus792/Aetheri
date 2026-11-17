#define _GNU_SOURCE
#include "login/installed.h"
#include "login/runner.h"
#include <errno.h>
#include <fcntl.h>
#include <linux/magic.h>
#include <string.h>
#include <sys/vfs.h>
#include <unistd.h>

struct deployment {
    const struct ae_login_release_anchor *trust;
    struct ae_login_installed *original, *backend;
    struct ae_login_selected_file policy;
    uint8_t record_digest[AE_SHA256_BYTES];
    int (*platform_check)(void *);
    void *userdata;
};

static int check_policy(const struct ae_login_selected_file *expected)
{
    int fd = open("/sys/fs/selinux/policy", O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) return -errno;
    struct statfs fs;
    int result = fstatfs(fd, &fs) ? -errno : 0;
    if (!result && fs.f_type != SELINUX_MAGIC) result = -EPROTO;
    uint8_t actual[AE_SHA256_BYTES];
    if (!result) result = ae_sha256_fd(fd, expected->size, actual);
    if (!result && memcmp(actual, expected->digest, sizeof actual)) result = -EKEYREJECTED;
    if (close(fd) && !result) result = -errno;
    return result;
}

static int check(void *value)
{
    struct deployment *deployment = value;
    int result = deployment->platform_check(deployment->userdata);
    if (result > 0) return -EINVAL;
    if (!result)
        result = ae_login_installed_check(deployment->backend ? deployment->backend : deployment->original);
    if (!result) result = check_policy(&deployment->policy);
    return result;
}

static int enter_view(void *value)
{
    struct deployment *deployment = value;
    if (deployment->backend) return -EALREADY;
    struct ae_login_installed *backend = NULL;
    int result = ae_login_installed_open(deployment->trust->key, deployment->trust->release, &backend);
    uint8_t digest[AE_SHA256_BYTES];
    if (!result) result = ae_login_installed_digest(backend, digest);
    if (!result && memcmp(digest, deployment->record_digest, sizeof digest)) result = -ESTALE;
    if (result) {
        ae_login_installed_close(&backend);
        return result;
    }
    deployment->backend = backend;
    return 0;
}

int ae_login_installed_startup_run(const struct ae_login_release_anchor *trust,
    int (*platform_check)(void *), void *userdata)
{
    if (!trust || !platform_check) return -EINVAL;
    if (getuid() || geteuid()) return -EACCES;
    int result = platform_check(userdata);
    if (result) return result > 0 ? -EINVAL : result;
    struct deployment deployment = {
        .trust = trust, .platform_check = platform_check, .userdata = userdata,
    };
    result = ae_login_installed_open(trust->key, trust->release, &deployment.original);
    struct ae_login_selection selection;
    if (!result) result = ae_login_installed_read(deployment.original, &selection);
    if (!result) deployment.policy = selection.files[AE_LOGIN_SELECT_KERNEL_POLICY];
    if (!result) result = ae_login_installed_digest(deployment.original, deployment.record_digest);
    struct ae_login_source_expect sources[AE_LOGIN_INSTALLED_BACKEND_COUNT];
    if (!result) result = ae_login_installed_backend(deployment.original, sources);
    static const size_t dropins[] = {
        AE_LOGIN_INSTALLED_SYSTEMD_DROPIN, AE_LOGIN_INSTALLED_WORKER_DROPIN,
    };
    const struct ae_login_run_profile profile = {
        .unit = "aetheri-login-startup.service",
        .label = "system_u:system_r:aetheri_login_startup_t:s0",
        .socket_prefix = "/run/greetd-",
        .timeout_usec = 15000000,
        .startup = {
            .service = {
                .unit = "aetheri-login.service",
                .label = "system_u:system_r:aetheri_login_backend_t:s0",
                .socket_label = "system_u:system_r:aetheri_login_backend_t:s0",
            },
            .sources = sources, .source_count = AE_LOGIN_INSTALLED_BACKEND_COUNT,
            .fragment = AE_LOGIN_INSTALLED_FRAGMENT, .program = AE_LOGIN_INSTALLED_BACKEND,
            .config = AE_LOGIN_INSTALLED_CONFIG,
            .dropins = dropins, .dropin_count = sizeof dropins / sizeof dropins[0],
            .directory = "/run/aetheri/login", .leaf = "backend-observation",
            .label = "system_u:object_r:aetheri_login_handoff_t:s0",
            .check = check, .enter_view = enter_view, .userdata = &deployment,
        },
    };
    if (!result) result = ae_login_startup_run(&profile);
    int closed = ae_login_installed_close(&deployment.backend);
    if (!result) result = closed;
    closed = ae_login_installed_close(&deployment.original);
    return result ? result : closed;
}
