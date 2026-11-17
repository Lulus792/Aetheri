#define _GNU_SOURCE
#include "login/greeter_runner.h"
#include "login/installed.h"
#include "login/policy_status.h"
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

struct deployment {
    const struct ae_login_greeter_profile *provider;
    struct ae_login_installed *installed;
    struct ae_login_sources *font;
    struct ae_login_policy_status *status;
};

static int policy_sequence(void *value,uint32_t *out)
{
    struct deployment *deployment=value;
    return ae_login_policy_status_sequence(deployment->status,out);
}

static int platform(const struct ae_login_greeter_profile *provider)
{
    int result = provider->backend.check(provider->backend.userdata);
    return result > 0 ? -EACCES : result;
}

static int check(void *value)
{
    struct deployment *deployment = value;
    int result = platform(deployment->provider);
    if (!result) result = ae_login_installed_check(deployment->installed);
    if (!result) result = ae_login_sources_check(deployment->font);
    uint32_t sequence;
    if (!result) result = policy_sequence(deployment,&sequence);
    return result;
}

static int credentials(const struct ae_login_greeter_expect *expected)
{
    uid_t user, effective_user, saved_user;
    gid_t group, effective_group, saved_group;
    if (getresuid(&user, &effective_user, &saved_user) ||
        getresgid(&group, &effective_group, &saved_group)) return -errno;
    if (user != expected->uid || effective_user != user || saved_user != user ||
        group != expected->gid || effective_group != group || saved_group != group)
        return -EACCES;
    return ae_login_greeter_check_groups(expected->groups, expected->group_count);
}

int ae_login_installed_greeter_run(const struct ae_login_greeter_profile *p, enum ae_login_ui_end *out)
{
    if (!p || !out || !p->installed_release || !p->backend.check || getpid() != gettid())
        return -EINVAL;
    if (!getuid() || !getgid()) return -EACCES;
    int result = platform(p);
    if (result) return result;
    struct deployment deployment = {.provider = p};
    const struct ae_login_release_anchor *trust = p->installed_release;
    result = ae_login_policy_status_open(&deployment.status);
    if (!result) result = ae_login_installed_open(trust->key, trust->release, &deployment.installed);
    struct ae_login_selection selection;
    if (!result) result = ae_login_installed_read(deployment.installed, &selection);

    static const size_t dropins[] = {AE_LOGIN_INSTALLED_SYSTEMD_DROPIN, AE_LOGIN_INSTALLED_WORKER_DROPIN};
    struct ae_login_source_expect sources[AE_LOGIN_INSTALLED_BACKEND_COUNT];
    gid_t groups[AE_LOGIN_GREETER_GROUPS_MAX];
    struct ae_login_greeter_profile profile = {
        .backend = {
            .service = {
                .unit = "aetheri-login.service",
                .label = "system_u:system_r:aetheri_login_backend_t:s0",
                .socket_label = "system_u:system_r:aetheri_login_backend_t:s0",
            },
            .greeter = {
                .groups = groups, .program = AE_LOGIN_INSTALLED_GREETER,
                .label = "system_u:system_r:aetheri_greeter_t:s0",
                .worker_label = "system_u:system_r:aetheri_login_worker_t:s0",
            },
            .sources = sources, .source_count = AE_LOGIN_INSTALLED_BACKEND_COUNT,
            .fragment = AE_LOGIN_INSTALLED_FRAGMENT, .program = AE_LOGIN_INSTALLED_BACKEND,
            .config = AE_LOGIN_INSTALLED_CONFIG,
            .dropins = dropins, .dropin_count = sizeof dropins / sizeof dropins[0],
            .directory = "/run/aetheri/login", .leaf = "backend-observation",
            .label = "system_u:object_r:aetheri_login_handoff_t:s0",
            .check = check, .policy_sequence = policy_sequence, .userdata = &deployment,
        },
        .compositor_label = "system_u:system_r:aetheri_greeter_display_t:s0",
        .compositor_socket_label = "system_u:system_r:aetheri_greeter_display_t:s0",
        .runtime = "/run/aetheri/login-greeter", .seat = "seat0", .vt = 8,
        .libraries = p->libraries, .input_quirks = p->input_quirks,
    };
    if (!result && selection.group_count > AE_LOGIN_GREETER_GROUPS_MAX) result = -EINVAL;
    if (!result) {
        for (size_t i = 0; i < selection.group_count; ++i) groups[i] = (gid_t)selection.groups[i];
        profile.backend.greeter.uid = (uid_t)selection.uid;
        profile.backend.greeter.gid = (gid_t)selection.gid;
        profile.backend.greeter.group_count = selection.group_count;
        result = credentials(&profile.backend.greeter);
    }
    if (!result) result = ae_login_installed_backend(deployment.installed, sources);
    if (!result) result = ae_login_installed_display(deployment.installed, &profile.compositor);
    struct ae_login_source_expect font_source;
    if (!result) result = ae_login_installed_font(deployment.installed, &font_source);
    if (!result) result = ae_login_sources_create(&font_source, 1, &deployment.font);
    void *font = NULL;
    if (!result) result = ae_login_sources_read_asset(deployment.font, 0, &font, &profile.font_size);
    profile.font = font;
    enum ae_login_ui_end end = AE_LOGIN_UI_STOPPED;
    if (!result) result = ae_login_greeter_run(&profile, &end);
    free(font);
    int closed = ae_login_sources_close(&deployment.font);
    if (!result) result = closed;
    closed = ae_login_installed_close(&deployment.installed);
    ae_login_policy_status_close(&deployment.status);
    if (!result) result = closed;
    if (!result) *out = end;
    return result;
}
