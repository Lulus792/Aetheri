/* Local Settings control for the fixed WLAN service. */
#include "linux/peer.h"

#include <errno.h>
#include <selinux/selinux.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/pidfd.h>
#include <sys/prctl.h>
#include <systemd/sd-login.h>
#include <unistd.h>

#define WLAN_BUS "org.aetheri.Wlan1"
#define WLAN_PATH "/org/aetheri/Wlan1"
#define BUS_SOCKET "unix:path=/run/dbus/system_bus_socket"
#define SETTINGS_CONTEXT "system_u:system_r:aetheri_settings_t:s0"
#define CONTROL_CONTEXT "system_u:system_r:aetheri_wlan_control_t:s0"
#define MANAGER_CONTEXT "system_u:system_r:init_t:s0"
#define MANAGER "org.freedesktop.systemd1"
#define MANAGER_INTERFACE MANAGER ".Manager"
#define MANAGER_PATH "/org/freedesktop/systemd1"
#define UNIT_INTERFACE MANAGER ".Unit"
#define WLAN_UNIT "aetheri-wlan.service"
#define WLAN_UNIT_PATH MANAGER_PATH "/unit/aetheri_2dwlan_2eservice"
#define CALL_TIMEOUT UINT64_C(3000000)
#define IDLE_TIMEOUT UINT64_C(30000000)

struct caller {
    struct ae_peer peer;
    uint64_t cgroup;
    char *session;
};

static int identity(const struct ae_peer *peer, uint64_t *cgroup)
{
    const uint64_t required = PIDFD_INFO_PID | PIDFD_INFO_CREDS | PIDFD_INFO_CGROUPID;
    struct pidfd_info info = {.mask = required};
    if (ae_peer_alive(peer) < 0)
        return -ESTALE;
    if (ioctl(peer->pidfd, PIDFD_GET_INFO, &info) < 0)
        return -errno;
    if ((info.mask & required) != required || peer->connection_pid <= 0 ||
        info.pid != (uint32_t)peer->connection_pid || info.pid != info.tgid || !info.cgroupid ||
        info.ruid != peer->connection_uid || info.euid != peer->connection_uid ||
        info.suid != peer->connection_uid || info.fsuid != peer->connection_uid)
        return -EACCES;
    *cgroup = info.cgroupid;
    return ae_peer_alive(peer);
}

static int current_context(const struct ae_peer *peer)
{
    char *current = NULL;
    int result = getpidcon_raw(peer->connection_pid, &current);
    if (result >= 0)
        result = strcmp(current, peer->connection_label) == 0 ? 0 : -ESTALE;
    freecon(current);
    return result < 0 ? -EACCES : 0;
}

static int observe_caller(struct caller *caller)
{
    struct ae_peer *peer = &caller->peer;
    if (!peer->connection_uid || strcmp(peer->connection_label, SETTINGS_CONTEXT))
        return -EACCES;

    uint64_t first = 0, last = 0;
    int result = identity(peer, &first);
    if (result < 0)
        return result;
    if (caller->session && first != caller->cgroup)
        return -ESTALE;
    if (current_context(peer) < 0)
        return -EACCES;

    char *session = NULL, *scope = NULL, *active = NULL, *type = NULL, *class = NULL;
    uid_t active_uid = 0, owner = 0;
    result = -EACCES;
    if (sd_pidfd_get_session(peer->pidfd, &session) < 0 || strnlen(session, 129) >= 129 ||
        (caller->session && strcmp(session, caller->session)) ||
        sd_pidfd_get_unit(peer->pidfd, &scope) < 0)
        goto done;
    char expected[144];
    int length = snprintf(expected, sizeof expected, "session-%s.scope", session);
    if (length <= 0 || (size_t)length >= sizeof expected || strcmp(expected, scope) ||
        sd_session_get_uid(session, &owner) < 0 || owner != peer->connection_uid ||
        sd_session_is_remote(session) != 0 || sd_session_is_active(session) <= 0 ||
        sd_session_get_type(session, &type) < 0 || strcmp(type, "wayland") ||
        sd_session_get_class(session, &class) < 0 || strcmp(class, "user") ||
        sd_seat_get_active("seat0", &active, &active_uid) < 0 ||
        active_uid != owner || strcmp(active, session))
        goto done;
    if (current_context(peer) < 0 || identity(peer, &last) < 0 || first != last) {
        result = -ESTALE;
        goto done;
    }
    if (!caller->session) {
        caller->session = session;
        caller->cgroup = last;
        session = NULL;
    }
    result = 0;
done:
    free(session);
    free(scope);
    free(active);
    free(type);
    free(class);
    return result;
}

static int capture_manager(sd_bus *bus, struct ae_peer *manager)
{
    const uint64_t required = SD_BUS_CREDS_UNIQUE_NAME | SD_BUS_CREDS_EUID | SD_BUS_CREDS_PID |
                              SD_BUS_CREDS_PIDFD | SD_BUS_CREDS_SELINUX_CONTEXT;
    sd_bus_creds *creds = NULL;
    const char *name = NULL, *context = NULL;
    int result = sd_bus_get_name_creds(bus, MANAGER, required, &creds);
    if (result < 0)
        return result;
    result = -EACCES;
    if ((sd_bus_creds_get_mask(creds) & required) != required ||
        (sd_bus_creds_get_augmented_mask(creds) & required) ||
        sd_bus_creds_get_euid(creds, &manager->connection_uid) < 0 || manager->connection_uid != 0 ||
        sd_bus_creds_get_pid(creds, &manager->connection_pid) < 0 || manager->connection_pid != 1 ||
        sd_bus_creds_get_selinux_context(creds, &context) < 0 || strcmp(context, MANAGER_CONTEXT) ||
        sd_bus_creds_get_unique_name(creds, &name) < 0 || name[0] != ':' ||
        !sd_bus_service_name_is_valid(name) || strnlen(name, sizeof manager->unique_name) >= sizeof manager->unique_name)
        goto done;
    result = sd_bus_creds_get_pidfd_dup(creds, &manager->pidfd);
    if (result >= 0) {
        strcpy(manager->unique_name, name);
        strcpy(manager->connection_label, MANAGER_CONTEXT);
        result = ae_peer_alive(manager);
    }
done:
    sd_bus_creds_unref(creds);
    return result;
}

static int load_wlan(sd_bus *bus, const struct ae_peer *manager)
{
    sd_bus_message *reply = NULL;
    const char *path = NULL;
    int result = sd_bus_call_method(bus, manager->unique_name, MANAGER_PATH, MANAGER_INTERFACE,
                                    "LoadUnit", NULL, &reply, "s", WLAN_UNIT);
    if (result >= 0) {
        const char *sender = sd_bus_message_get_sender(reply);
        result = sd_bus_message_has_signature(reply, "o") &&
                 sender && !strcmp(sender, manager->unique_name) &&
                 sd_bus_message_read(reply, "o", &path) > 0 &&
                 strcmp(path, WLAN_UNIT_PATH) == 0 ? ae_peer_alive(manager) : -EBADMSG;
    }
    sd_bus_message_unref(reply);
    return result;
}

static const char *public_state(const char *state)
{
    if (!strcmp(state, "active")) return "on";
    if (!strcmp(state, "inactive")) return "off";
    if (!strcmp(state, "activating")) return "starting";
    if (!strcmp(state, "deactivating")) return "stopping";
    if (!strcmp(state, "failed")) return "failed";
    return "unavailable";
}

static int method(sd_bus_message *message, void *data, sd_bus_error *error)
{
    sd_bus *backend = data;
    struct caller caller = {.peer = AE_PEER_INIT};
    struct ae_peer manager = AE_PEER_INIT;
    char *state = NULL;
    sd_bus_message *reply = NULL;
    int enabled = 0;
    const char *member = sd_bus_message_get_member(message);
    const int setting = strcmp(member, "SetEnabled") == 0;
    int result = ae_peer_capture(message, &caller.peer);
    if (result < 0 || observe_caller(&caller) < 0)
        goto denied;
    if (setting && sd_bus_message_read(message, "b", &enabled) <= 0) {
        result = sd_bus_error_set_const(error, SD_BUS_ERROR_INVALID_ARGS, "Expected an enabled flag");
        goto done;
    }
    if (capture_manager(backend, &manager) < 0 || load_wlan(backend, &manager) < 0)
        goto unavailable;
    if (observe_caller(&caller) < 0)
        goto denied;
    if (!setting) {
        result = sd_bus_get_property_string(backend, manager.unique_name, WLAN_UNIT_PATH, UNIT_INTERFACE,
                                            "ActiveState", NULL, &state);
        if (result < 0 || ae_peer_alive(&manager) < 0)
            goto unavailable;
        if (observe_caller(&caller) < 0)
            goto denied;
        result = sd_bus_reply_method_return(message, "s", public_state(state));
        goto done;
    }
    const char *job = NULL;
    result = sd_bus_call_method(backend, manager.unique_name, MANAGER_PATH, MANAGER_INTERFACE,
                                enabled ? "StartUnit" : "StopUnit", NULL, &reply, "ss", WLAN_UNIT, "fail");
    const char *sender = result >= 0 ? sd_bus_message_get_sender(reply) : NULL;
    const char prefix[] = MANAGER_PATH "/job/";
    if (!sender || strcmp(sender, manager.unique_name) || !sd_bus_message_has_signature(reply, "o") ||
        sd_bus_message_read(reply, "o", &job) <= 0 || strncmp(job, prefix, sizeof prefix - 1) ||
        ae_peer_alive(&manager) < 0) {
        result = sd_bus_error_set_const(error, "org.aetheri.Error1.OutcomeUnknown", "Read state before requesting another change");
        goto done;
    }
    result = sd_bus_reply_method_return(message, "");
    goto done;
denied:
    result = sd_bus_error_set_const(error, "org.aetheri.Error1.NotAuthorized", "Active local Settings session required");
    goto done;
unavailable:
    result = sd_bus_error_set_const(error, "org.aetheri.Error1.Unavailable", "Wireless service unavailable");
done:
    ae_peer_clear(&manager);
    ae_peer_clear(&caller.peer);
    sd_bus_message_unref(reply);
    free(caller.session);
    free(state);
    return result;
}

static const sd_bus_vtable methods[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("GetState", "", "s", method, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("SetEnabled", "b", "", method, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_VTABLE_END,
};

static int open_system(sd_bus **bus)
{
    int result = sd_bus_new(bus);
    if (result >= 0) result = sd_bus_set_address(*bus, BUS_SOCKET);
    if (result >= 0) result = sd_bus_set_bus_client(*bus, 1);
    if (result >= 0) result = sd_bus_set_method_call_timeout(*bus, CALL_TIMEOUT);
    if (result >= 0) result = sd_bus_start(*bus);
    return result;
}

int main(void)
{
    char *context = NULL;
    if (getuid() || geteuid() || security_getenforce() != 1 ||
        prctl(PR_GET_NO_NEW_PRIVS, 0L, 0L, 0L, 0L) != 1 || getcon_raw(&context) < 0)
        return EXIT_FAILURE;
    int admitted = strcmp(context, CONTROL_CONTEXT) == 0;
    freecon(context);
    if (!admitted)
        return EXIT_FAILURE;
    sd_bus *incoming = NULL, *backend = NULL;
    sd_bus_slot *slot = NULL;
    int result = open_system(&incoming);
    if (result >= 0) result = open_system(&backend);
    if (result >= 0) result = sd_bus_add_object_vtable(incoming, &slot, WLAN_PATH, WLAN_BUS, methods, backend);
    if (result >= 0) result = sd_bus_request_name(incoming, WLAN_BUS, 0);
    if (result == 0) result = -EADDRINUSE;
    while (result >= 0) {
        result = sd_bus_process(incoming, NULL);
        if (result != 0)
            continue;
        result = sd_bus_wait(incoming, IDLE_TIMEOUT);
        if (result == 0)
            break;
    }
    sd_bus_slot_unref(slot);
    sd_bus_flush_close_unref(incoming);
    sd_bus_close_unref(backend);
    return result < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
