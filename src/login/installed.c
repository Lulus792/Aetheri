#define _GNU_SOURCE
#include "login/installed.h"
#include <errno.h>
#include <fcntl.h>
#include <linux/openat2.h>
#include <selinux/selinux.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

#define LOGIN_DATA "/usr/share/aetheri/login"
#define LOGIN_EXEC "/usr/libexec/aetheri"
#define SYSTEMD_UNITS "/usr/lib/systemd/system"

static const char selection_leaf[] = "selection.bin";
static const char config_label[] = "system_u:object_r:aetheri_login_config_t:s0";
static const char unit_label[] = "system_u:object_r:systemd_unit_file_t:s0";

struct ae_login_installed {
    struct ae_login_sources *anchor;
    struct ae_login_selection selection;
};

static int read_record(uint8_t bytes[AE_LOGIN_SELECTION_BYTES])
{
    struct open_how how = {
        .flags = O_RDONLY | O_NONBLOCK | O_CLOEXEC | O_NOFOLLOW,
        .resolve = RESOLVE_NO_SYMLINKS,
    };
    int fd;
    do {
        fd = (int)syscall(SYS_openat2, AT_FDCWD, LOGIN_DATA "/selection.bin", &how, sizeof how);
    } while (fd < 0 && errno == EINTR);
    if (fd < 0) return -errno;

    struct stat info;
    int result = fstat(fd, &info) ? -errno : 0;
    if (!result && (info.st_mode != (S_IFREG | 0444) || info.st_uid ||
                    info.st_gid || info.st_nlink != 1)) result = -EACCES;
    if (!result && info.st_size != AE_LOGIN_SELECTION_BYTES) result = -EBADMSG;
    size_t used = 0;
    while (!result && used < AE_LOGIN_SELECTION_BYTES) {
        ssize_t count = read(fd, bytes + used, AE_LOGIN_SELECTION_BYTES - used);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) result = count < 0 ? -errno : -EBADMSG;
        else used += (size_t)count;
    }
    if (close(fd) && !result) result = -errno;
    return result;
}

int ae_login_installed_open(const uint8_t key[AE_LOGIN_SELECTION_KEY_BYTES],
    const uint8_t release[AE_LOGIN_SELECTION_RELEASE_BYTES], struct ae_login_installed **out)
{
    if (!key || !release || !out || *out) return -EINVAL;
    if (security_getenforce() != 1) return -EACCES;
    uint8_t bytes[AE_LOGIN_SELECTION_BYTES];
    struct ae_login_selection selected;
    int result = read_record(bytes);
    if (!result) result = ae_login_selection_verify(bytes, sizeof bytes, key, release, &selected);
    struct ae_login_source_expect anchor = {
        .directory = LOGIN_DATA, .leaf = selection_leaf, .label = config_label,
        .kind = AE_LOGIN_SOURCE_DATA, .mode = 0444, .size = sizeof bytes,
    };
    if (!result) result = ae_sha256_bytes(bytes, sizeof bytes, anchor.digest);
    if (result) return result;
    struct ae_login_installed *installed = calloc(1, sizeof *installed);
    if (!installed) return -ENOMEM;
    /* Only authenticated bytes become the expectation for the held source.
       The capture independently checks protected placement, label and bytes. */
    result = ae_login_sources_create(&anchor, 1, &installed->anchor);
    if (result) {
        free(installed);
        return result;
    }
    installed->selection = selected;
    *out = installed;
    return 0;
}

int ae_login_installed_check(void *value)
{
    struct ae_login_installed *installed = value;
    return installed ? ae_login_sources_check(installed->anchor) : -EINVAL;
}

int ae_login_installed_read(struct ae_login_installed *installed, struct ae_login_selection *out)
{
    if (!out) return -EINVAL;
    int result = ae_login_installed_check(installed);
    if (!result) *out = installed->selection;
    return result;
}

int ae_login_installed_digest(struct ae_login_installed *installed, uint8_t out[AE_SHA256_BYTES])
{
    return installed ? ae_login_sources_data_digest(installed->anchor, 0, out) : -EINVAL;
}

static void selected_file(const struct ae_login_installed *installed, enum ae_login_selection_role role,
                          struct ae_login_source_expect *source)
{
    const struct ae_login_selected_file *file = &installed->selection.files[role];
    source->size = file->size;
    memcpy(source->digest, file->digest, sizeof source->digest);
}

int ae_login_installed_backend(struct ae_login_installed *installed,
    struct ae_login_source_expect out[AE_LOGIN_INSTALLED_BACKEND_COUNT])
{
    if (!out) return -EINVAL;
    int result = ae_login_installed_check(installed);
    if (result) return result;
    struct ae_login_source_expect backend[AE_LOGIN_INSTALLED_BACKEND_COUNT] = {
        [AE_LOGIN_INSTALLED_FRAGMENT] =
            {SYSTEMD_UNITS, "aetheri-login.service", unit_label, AE_LOGIN_SOURCE_DATA, 0444, 0, {0}},
        [AE_LOGIN_INSTALLED_BACKEND] =
            {LOGIN_EXEC, "aetheri-login-backend", "system_u:object_r:aetheri_login_backend_exec_t:s0",
                AE_LOGIN_SOURCE_ELF, 0555, 0, {0}},
        [AE_LOGIN_INSTALLED_CONFIG] =
            {LOGIN_DATA, "greetd.toml", config_label, AE_LOGIN_SOURCE_DATA, 0444, 0, {0}},
        [AE_LOGIN_INSTALLED_SYSTEMD_DROPIN] =
            {SYSTEMD_UNITS "/service.d", "10-timeout-abort.conf", unit_label, AE_LOGIN_SOURCE_DATA, 0644, 0, {0}},
        [AE_LOGIN_INSTALLED_WORKER_DROPIN] =
            {SYSTEMD_UNITS "/aetheri-login.service.d", "20-session-shutdown.conf", unit_label,
                AE_LOGIN_SOURCE_DATA, 0444, 0, {0}},
        [AE_LOGIN_INSTALLED_GREETER] =
            {LOGIN_EXEC, "aetheri-login-greeter", "system_u:object_r:aetheri_greeter_exec_t:s0",
                AE_LOGIN_SOURCE_ELF, 0555, 0, {0}},
    };
    static const enum ae_login_selection_role roles[AE_LOGIN_INSTALLED_BACKEND_COUNT] = {
        [AE_LOGIN_INSTALLED_FRAGMENT] = AE_LOGIN_SELECT_UNIT,
        [AE_LOGIN_INSTALLED_BACKEND] = AE_LOGIN_SELECT_BACKEND,
        [AE_LOGIN_INSTALLED_CONFIG] = AE_LOGIN_SELECT_CONFIG,
        [AE_LOGIN_INSTALLED_SYSTEMD_DROPIN] = AE_LOGIN_SELECT_SYSTEMD_DROPIN,
        [AE_LOGIN_INSTALLED_WORKER_DROPIN] = AE_LOGIN_SELECT_WORKER_DROPIN,
        [AE_LOGIN_INSTALLED_GREETER] = AE_LOGIN_SELECT_GREETER,
    };
    for (size_t i = 0; i < AE_LOGIN_INSTALLED_BACKEND_COUNT; ++i)
        selected_file(installed, roles[i], &backend[i]);
    memcpy(out, backend, sizeof backend);
    return 0;
}

int ae_login_installed_display(struct ae_login_installed *installed, struct ae_login_source_expect *out)
{
    if (!out) return -EINVAL;
    int result = ae_login_installed_check(installed);
    if (result) return result;
    struct ae_login_source_expect display = {
        .directory = LOGIN_EXEC, .leaf = "aetheri-login-display",
        .label = "system_u:object_r:aetheri_greeter_display_exec_t:s0",
        .kind = AE_LOGIN_SOURCE_ELF, .mode = 0555,
    };
    selected_file(installed, AE_LOGIN_SELECT_DISPLAY, &display);
    *out = display;
    return 0;
}

int ae_login_installed_close(struct ae_login_installed **value)
{
    if (!value) return -EINVAL;
    struct ae_login_installed *installed = *value;
    if (!installed) return 0;
    int result = ae_login_sources_close(&installed->anchor);
    if (!result) {
        free(installed);
        *value = NULL;
    }
    return result;
}
