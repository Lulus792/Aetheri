#define _GNU_SOURCE
#include "login/installed.h"
#include "login/policy_status.h"
#include "login/runner.h"
#include "linux/image.h"
#include <errno.h>
#include <fcntl.h>
#include <linux/magic.h>
#include <string.h>
#include <sys/vfs.h>
#include <unistd.h>

struct runtime_observation {
    struct ae_login_sources *core, *apps, *lock;
};

struct deployment {
    const struct ae_login_release_anchor *trust;
    struct ae_login_installed *original, *backend;
    struct runtime_observation original_runtime, backend_runtime;
    struct ae_login_policy_status *status;
    struct ae_login_selected_file policy;
    uint8_t record_digest[AE_SHA256_BYTES];
    int (*platform_check)(void *);
    void *userdata;
};

static int close_runtime(struct runtime_observation *value)
{
    int result = ae_login_sources_close(&value->lock);
    int closed_apps = ae_login_sources_close(&value->apps);
    int closed = ae_login_sources_close(&value->core);
    return result ? result : closed_apps ? closed_apps : closed;
}

static int observe_runtime(struct ae_login_installed *installed, struct runtime_observation *out)
{
    struct ae_login_source_expect runtime[AE_LOGIN_INSTALLED_RUNTIME_COUNT];
    int result = ae_login_installed_runtime(installed, runtime);
    if (!result) result = ae_login_sources_create(runtime, AE_LOGIN_INSTALLED_RUNTIME_COUNT, &out->core);
    struct ae_image expected, current;
    if (!result) result = ae_login_sources_image(out->core, AE_LOGIN_INSTALLED_STARTUP_PROGRAM, &expected);
    if (!result) result = ae_image_match_self(0, 0, &expected, &current);
    struct ae_login_source_expect apps[AE_LOGIN_INSTALLED_DESKTOP_APP_MAX];
    size_t count;
    if (!result) result = ae_login_installed_desktop_apps(installed, apps, &count);
    if (!result) result = ae_login_sources_create(apps, count, &out->apps);
    struct ae_login_source_expect lock[AE_LOGIN_INSTALLED_LOCK_COUNT];
    if (!result) result = ae_login_installed_lock(installed, lock, &count);
    if (!result && count) result = ae_login_sources_create(lock, count, &out->lock);
    if (result) close_runtime(out);
    return result;
}

static int policy_sequence(void *value,uint32_t *out)
{
    struct deployment *deployment=value;
    return ae_login_policy_status_sequence(deployment->status,out);
}

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
    struct runtime_observation *runtime = deployment->backend ?
        &deployment->backend_runtime : &deployment->original_runtime;
    if (!result) result = ae_login_sources_check(runtime->core);
    if (!result) result = ae_login_sources_check(runtime->apps);
    if (!result && runtime->lock) result = ae_login_sources_check(runtime->lock);
    uint32_t sequence;
    if (!result) result = policy_sequence(deployment,&sequence);
    if (!result) result = check_policy(&deployment->policy);
    if (!result) result = policy_sequence(deployment,&sequence);
    return result;
}

static int enter_view(void *value)
{
    struct deployment *deployment = value;
    if (deployment->backend) return -EALREADY;
    struct ae_login_installed *backend = NULL;
    int result = ae_login_installed_open(deployment->trust, &backend);
    uint8_t digest[AE_SHA256_BYTES];
    if (!result) result = ae_login_installed_digest(backend, digest);
    if (!result && memcmp(digest, deployment->record_digest, sizeof digest)) result = -ESTALE;
    if (!result) result = observe_runtime(backend, &deployment->backend_runtime);
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
    result = ae_login_policy_status_open(&deployment.status);
    if (!result) result = ae_login_installed_open(trust, &deployment.original);
    struct ae_login_selection selection;
    if (!result) result = ae_login_installed_read(deployment.original, &selection);
    if (!result) deployment.policy = selection.files[AE_LOGIN_SELECT_KERNEL_POLICY];
    if (!result) result = ae_login_installed_digest(deployment.original, deployment.record_digest);
    if (!result) result = observe_runtime(deployment.original, &deployment.original_runtime);
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
            .check = check, .enter_view = enter_view, .policy_sequence = policy_sequence,
            .userdata = &deployment,
        },
    };
    if (!result) result = ae_login_startup_run(&profile);
    int closed = close_runtime(&deployment.backend_runtime);
    if (!result) result = closed;
    closed = close_runtime(&deployment.original_runtime);
    if (!result) result = closed;
    closed = ae_login_installed_close(&deployment.backend);
    if (!result) result = closed;
    closed = ae_login_installed_close(&deployment.original);
    ae_login_policy_status_close(&deployment.status);
    return result ? result : closed;
}
