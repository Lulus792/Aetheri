#define _GNU_SOURCE
#include "login/image_platform.h"
#include "login/image_release.h"
#include "login/installed.h"
#ifdef AE_LOGIN_IMAGE_GREETER
#include "login/greeter_runner.h"
#else
#include "login/runner.h"
#endif
#include <errno.h>
#include <selinux/selinux.h>
#include <string.h>
#include <sys/prctl.h>

static const struct ae_login_release_anchor release = {
    .desktop_sound = AETHERI_DESKTOP_SESSION_SOUND,
    .desktop_lock = AETHERI_DESKTOP_SESSION_LOCK,
    .desktop_suspend = AETHERI_DESKTOP_SESSION_SUSPEND,
    .key = {AE_LOGIN_IMAGE_PUBLIC_KEY},
    .release = {AE_LOGIN_IMAGE_RELEASE},
};

static int check(void *context)
{
    const char *expected = context;
    if (prctl(PR_GET_NO_NEW_PRIVS, 0, 0, 0, 0) != 1 || security_getenforce() != 1)
        return -EACCES;
    char *label = NULL;
    if (getcon_raw(&label) < 0) return -errno;
    int result = label && !strcmp(label, expected) ? 0 : -EACCES;
    freecon(label);
    return result ? result : ae_login_image_mount_check();
}

#ifndef AE_LOGIN_IMAGE_GREETER
int ae_login_deployment_select(struct ae_login_run_profile *profile)
{
    static char label[] = "system_u:system_r:aetheri_login_startup_t:s0";
    if (!profile) return -EINVAL;
    *profile = (struct ae_login_run_profile) {
        .installed_release = &release, .startup = {.check = check, .userdata = label},
    };
    return 0;
}
#else
int ae_login_greeter_deployment_select(struct ae_login_greeter_profile *profile)
{
    static char label[] = "system_u:system_r:aetheri_greeter_t:s0";
    if (!profile) return -EINVAL;
    *profile = (struct ae_login_greeter_profile) {
        .installed_release = &release, .backend = {.check = check, .userdata = label},
        .input_quirks = "/usr/share/libinput",
    };
    return 0;
}
#endif
