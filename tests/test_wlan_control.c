/* Exercise the installed caller check with a real Linux pidfd and scripted logind views. */
#define main wlan_control_service_main
#include "../src/wlan/control.c"
#undef main

enum observation {
    LOCAL, REMOTE, INACTIVE, GREETER, FOREIGN_SEAT, LOGIN_REPLACED, WRONG_SCOPE, EXEC_CHANGED
};
static enum observation view;
static int original_pin;

#define CHECK(expression) do { if (!(expression)) { \
    fprintf(stderr, "WLAN control test line %d: %s\n", __LINE__, #expression); return 1; \
} } while (0)

static int copy(const char *value, char **output)
{
    *output = strdup(value);
    return *output ? 0 : -ENOMEM;
}

int __wrap_getpidcon_raw(pid_t pid, char **output)
{
    if (pid != getpid()) return -ESRCH;
    return copy(view == EXEC_CHANGED ? "system_u:system_r:aetheri_files_t:s0" : SETTINGS_CONTEXT, output);
}

int __wrap_sd_pidfd_get_session(int fd, char **output)
{
    if (fd != original_pin) return -ESTALE;
    return copy(view == LOGIN_REPLACED ? "5" : "4", output);
}

int __wrap_sd_pidfd_get_unit(int fd, char **output)
{
    if (fd != original_pin) return -ESTALE;
    if (view == WRONG_SCOPE) return copy("user@1000.service", output);
    return copy(view == LOGIN_REPLACED ? "session-5.scope" : "session-4.scope", output);
}

int __wrap_sd_session_get_uid(const char *id, uid_t *output)
{
    if (strcmp(id, "4") && strcmp(id, "5")) return -ENOENT;
    *output = getuid();
    return 0;
}

int __wrap_sd_session_is_remote(const char *id)
{
    (void)id;
    return view == REMOTE;
}

int __wrap_sd_session_is_active(const char *id)
{
    (void)id;
    return view != INACTIVE;
}

int __wrap_sd_session_get_type(const char *id, char **output)
{
    (void)id;
    return copy("wayland", output);
}

int __wrap_sd_session_get_class(const char *id, char **output)
{
    (void)id;
    return copy(view == GREETER ? "greeter" : "user", output);
}

int __wrap_sd_seat_get_active(const char *seat, char **output, uid_t *owner)
{
    if (strcmp(seat, "seat0")) return -ENOENT;
    *owner = getuid();
    return copy(view == FOREIGN_SEAT ? "3" : view == LOGIN_REPLACED ? "5" : "4", output);
}

int main(void)
{
    if (getuid() == 0) return 77;
    CHECK(getuid() != 0 && getuid() == geteuid());
    struct caller app = {.peer = AE_PEER_INIT};
    original_pin = pidfd_open(getpid(), 0);
    CHECK(original_pin >= 0);
    app.peer.pidfd = original_pin;
    app.peer.connection_pid = getpid();
    app.peer.connection_uid = getuid();
    strcpy(app.peer.connection_label, "system_u:system_r:aetheri_files_t:s0");
    CHECK(observe_caller(&app) < 0 && !app.session);
    strcpy(app.peer.connection_label, SETTINGS_CONTEXT);
    for (view = REMOTE; view <= WRONG_SCOPE; ++view) {
        if (view == LOGIN_REPLACED) continue;
        CHECK(observe_caller(&app) < 0 && !app.session);
    }
    view = EXEC_CHANGED;
    CHECK(observe_caller(&app) < 0 && !app.session);
    view = LOCAL;
    ++app.peer.connection_pid;
    CHECK(observe_caller(&app) < 0 && !app.session);
    --app.peer.connection_pid;
    CHECK(observe_caller(&app) == 0 && app.cgroup != 0 && !strcmp(app.session, "4"));
    view = LOGIN_REPLACED;
    CHECK(observe_caller(&app) < 0 && !strcmp(app.session, "4"));
    view = LOCAL;
    ++app.cgroup;
    CHECK(observe_caller(&app) < 0);
    --app.cgroup;
    CHECK(observe_caller(&app) == 0);
    CHECK(!strcmp(public_state("inactive"), "off"));
    CHECK(!strcmp(public_state("activating"), "starting"));
    CHECK(!strcmp(public_state("active"), "on"));
    CHECK(!strcmp(public_state("deactivating"), "stopping"));
    CHECK(!strcmp(public_state("failed"), "failed"));
    CHECK(!strcmp(public_state("new-state"), "unavailable"));
    free(app.session);
    ae_peer_clear(&app.peer);
    return 0;
}
