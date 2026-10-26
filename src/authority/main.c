#include "catalogue/store.h"
#include "linux/peer.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/openat2.h>
#include <linux/capability.h>
#include <selinux/selinux.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <systemd/sd-daemon.h>
#include <systemd/sd-event.h>
#include <systemd/sd-id128.h>
#include <unistd.h>

#define ROOT "/var/lib/aetheri-p0"

struct authority {
    struct ae_catalogue *catalogue;
    struct ae_catalogue_identity identity;
    sd_id128_t boot, runtime, events;
    uid_t uid;
};

static int context_is(const char *context, const char *type)
{
    if (context == NULL) return 0;
    const char *first = strchr(context, ':');
    const char *second = first == NULL ? NULL : strchr(first + 1, ':');
    const char *third = second == NULL ? NULL : strchr(second + 1, ':');
    return third != NULL && (size_t)(third - second - 1) == strlen(type) &&
           memcmp(second + 1, type, strlen(type)) == 0;
}

static int enforced_domain(const char *expected)
{
    char *context = NULL;
    if (security_getenforce() != 1 || getcon_raw(&context) != 0) return -EACCES;
    int result = context_is(context, expected) ? 0 : -EACCES;
    freecon(context);
    return result;
}

static int open_trusted(const char *path, int directory)
{
    struct open_how how = {.flags = (uint64_t)(O_RDONLY | O_CLOEXEC | O_NOFOLLOW | (directory ? O_DIRECTORY : O_NONBLOCK)),
                          .resolve = RESOLVE_NO_SYMLINKS | RESOLVE_NO_MAGICLINKS};
    int fd;
    do { fd = (int)syscall(SYS_openat2, AT_FDCWD, path, &how, sizeof how); } while (fd < 0 && errno == EINTR);
    return fd < 0 ? -errno : fd;
}

static int labeled(int fd, const char *expected)
{
    char *context = NULL;
    if (fgetfilecon_raw(fd, &context) < 0) return -EACCES;
    int result = context_is(context, expected) ? 0 : -EACCES;
    freecon(context);
    return result;
}

static int restricted_process(void)
{
    if (prctl(PR_GET_NO_NEW_PRIVS, 0L, 0L, 0L, 0L) != 1) return -EPERM;
    struct __user_cap_header_struct header = {.version = _LINUX_CAPABILITY_VERSION_3, .pid = 0};
    struct __user_cap_data_struct data[2] = {{0}};
    if (syscall(SYS_capget, &header, data) != 0) return -errno;
    for (size_t n = 0; n < 2; ++n)
        if (data[n].effective != 0 || data[n].permitted != 0 || data[n].inheritable != 0) return -EPERM;
    return 0;
}

static int root_directory(const char *path, const char *type)
{
    int fd = open_trusted(path, 1);
    if (fd < 0) return fd;
    struct stat status;
    int result = fstat(fd, &status) == 0 && status.st_uid == 0 && (status.st_mode & 07777) == 0755 ? labeled(fd, type) : -EACCES;
    close(fd);
    return result;
}

static int uuid4(const uint8_t value[AE_ID_BYTES])
{
    return (value[6] >> 4) == 4 && (value[8] & 0xc0) == 0x80;
}

/* Root-provisioned fixed P0 bootstrap registry. No path/identity from a client. */
static int load_registry(struct authority *authority, char state_path[256])
{
    int root = open_trusted(ROOT, 1);
    if (root < 0) return root;
    struct stat status;
    int result = fstat(root, &status) == 0 && status.st_uid == 0 &&
                 (status.st_mode & 07777) == 0755 ? 0 : -EACCES;
    if (result == 0) result = labeled(root, "aetheri_root_t");
    close(root);
    if (result == 0) result = root_directory(ROOT "/registry", "aetheri_registry_t");
    if (result != 0) return result;
    int system = authority->identity.kind == AE_CATALOGUE_SYSTEM;
    char path[256];
    int count = system ? snprintf(path, sizeof path, ROOT "/registry/system.conf") :
                         snprintf(path, sizeof path, ROOT "/registry/user-%u.conf", (unsigned int)authority->uid);
    if (count < 0 || (size_t)count >= sizeof path) return -EINVAL;
    int fd = open_trusted(path, 0);
    if (fd < 0) return fd;
    result = fstat(fd, &status) == 0 && S_ISREG(status.st_mode) && status.st_uid == 0 &&
             status.st_nlink == 1 && (status.st_mode & 07777) == 0444 && status.st_size > 0 && status.st_size < 256 ? 0 : -EACCES;
    if (result == 0) result = labeled(fd, "aetheri_registry_t");
    char bytes[256] = {0};
    size_t used = 0;
    while (result == 0 && used < sizeof bytes - 1U) {
        ssize_t size = read(fd, bytes + used, sizeof bytes - 1U - used);
        if (size == 0) break;
        if (size < 0) { if (errno != EINTR) result = -errno; }
        else used += (size_t)size;
    }
    close(fd);
    if (result != 0) return result;
    if (used != (size_t)status.st_size || memchr(bytes, 0, used) != NULL) return -EBADMSG;
    char epoch[33] = {0}, owner[33] = {0}, uid[24] = {0};
    const char *kind = system ? "system" : "user";
    char prefix[64];
    count = snprintf(prefix, sizeof prefix, "AETHERI-P0-AUTHORITY-1\nkind=%s\nepoch=", kind);
    if (count < 0 || (size_t)count >= sizeof prefix || used < (size_t)count + 32U || memcmp(bytes, prefix, (size_t)count) != 0) return -EBADMSG;
    memcpy(epoch, bytes + count, 32);
    if (ae_id_from_hex(epoch, 32, authority->identity.authority_epoch) != 0 || !uuid4(authority->identity.authority_epoch)) return -EBADMSG;
    const char *tail = bytes + count + 32;
    if (strncmp(tail, "\nowner=", 7) != 0) return -EBADMSG;
    tail += 7;
    if (system) { if (*tail != '-') return -EBADMSG; ++tail; }
    else {
        if (strlen(tail) < 32) return -EBADMSG;
        memcpy(owner, tail, 32);
        if (ae_id_from_hex(owner, 32, authority->identity.owner) != 0 || !uuid4(authority->identity.owner)) return -EBADMSG;
        tail += 32;
    }
    snprintf(uid, sizeof uid, "\nuid=%u\n", (unsigned int)authority->uid);
    if (strcmp(tail, uid) != 0) return -EBADMSG;
    count = system ? snprintf(state_path, 256, ROOT "/system") :
                     snprintf(state_path, 256, ROOT "/users/%s/control", owner);
    if (count < 0 || count >= 256) return -EINVAL;
    if (!system) {
        result = root_directory(ROOT "/users", "aetheri_root_t");
        char parent[256];
        snprintf(parent, sizeof parent, ROOT "/users/%s", owner);
        if (result == 0) result = root_directory(parent, "aetheri_root_t");
        if (result != 0) return result;
    }
    fd = open_trusted(state_path, 1);
    if (fd < 0) return fd;
    result = labeled(fd, system ? "aetheri_system_state_t" : "aetheri_user_state_t");
    close(fd);
    return result;
}

static int authenticate(sd_bus_message *message, struct authority *authority, sd_bus_error *error)
{
    struct ae_peer peer = AE_PEER_INIT;
    int result = ae_peer_capture(message, &peer);
    if (result == 0 && authority->identity.kind == AE_CATALOGUE_USER && peer.connection_uid != authority->uid) result = -EACCES;
    ae_peer_clear(&peer);
    if (result < 0) return sd_bus_error_set_const(error, "org.aetheri.Error1.NotAuthorized", "Authenticated local caller required");
    return 0;
}

static int method_info(sd_bus_message *message, void *userdata, sd_bus_error *error)
{
    struct authority *authority = userdata;
    int result = authenticate(message, authority, error);
    if (result < 0) return result;
    struct ae_catalogue_info info;
    result = ae_catalogue_get_info(authority->catalogue, &info);
    if (result != 0 || security_getenforce() != 1 ||
        memcmp(info.boot_id, authority->boot.bytes, AE_ID_BYTES) != 0 ||
        memcmp(info.runtime_incarnation, authority->runtime.bytes, AE_ID_BYTES) != 0)
        return sd_bus_error_set_const(error, "org.aetheri.Error1.NeedsRecovery", "Authority context is unavailable");
    sd_bus_message *reply = NULL;
    result = sd_bus_message_new_method_return(message, &reply);
    if (result >= 0) result = sd_bus_message_append(reply, "uu", 1U, 1U);
    if (result >= 0) result = sd_bus_message_open_container(reply, 'a', "s");
    if (result >= 0) result = sd_bus_message_close_container(reply); /* No lifecycle capability is implemented yet. */
    if (result >= 0) result = sd_bus_message_append_array(reply, 'y', info.authority_epoch, AE_ID_BYTES);
    if (result >= 0) result = sd_bus_message_append_array(reply, 'y', authority->events.bytes, AE_ID_BYTES);
    if (result >= 0) result = sd_bus_send(NULL, reply, NULL);
    sd_bus_message_unref(reply);
    return result < 0 ? result : 1;
}

static int method_unavailable(sd_bus_message *message, void *userdata, sd_bus_error *error)
{
    int result = authenticate(message, userdata, error);
    if (result < 0) return result;
    if (strcmp(sd_bus_message_get_interface(message), "org.aetheri.Runtime1") == 0)
        return sd_bus_error_set_const(error, "org.aetheri.Error1.NotAuthorized", "Registered broker authorization is not available");
    return sd_bus_error_set_const(error, "org.aetheri.Error1.UnsupportedCapability", "This authority does not implement that capability yet");
}

struct wire_interface { int roles; const char *path, *name; const sd_bus_vtable *vtable; };
#include "wire-tables.h"

static int stop_signal(sd_event_source *source, const struct signalfd_siginfo *signal_info, void *userdata)
{
    (void)signal_info; (void)userdata;
    return sd_event_exit(sd_event_source_get_event(source), 0);
}

int main(int argc, char **argv)
{
    if ((argc != 2 && argc != 3) || (strcmp(argv[1], "system") != 0 && strcmp(argv[1], "user") != 0) ||
        (argc == 3 && strcmp(argv[2], "--init") != 0)) {
        fputs("Usage: aetheri-authority system|user [--init]\n", stderr); return 2;
    }
    struct authority authority = {.identity.kind = strcmp(argv[1], "system") == 0 ? AE_CATALOGUE_SYSTEM : AE_CATALOGUE_USER, .uid = geteuid()};
    int system = authority.identity.kind == AE_CATALOGUE_SYSTEM;
    if ((system && authority.uid != 0) || (!system && authority.uid == 0)) return 3;
    int result = enforced_domain(system ? "aetheri_system_t" : "aetheri_user_t");
    if (result == 0) result = restricted_process();
    char state_path[256];
    if (result == 0) result = load_registry(&authority, state_path);
    if (result == 0) result = sd_id128_get_boot(&authority.boot);
    if (result == 0) result = argc == 3 ? ae_catalogue_create(state_path, &authority.identity, authority.boot.bytes, &authority.catalogue) :
                                        ae_catalogue_open(state_path, &authority.identity, &authority.catalogue);
    if (argc == 3) {
        if (result == 0) result = ae_catalogue_close(&authority.catalogue);
        if (result != 0) fprintf(stderr, "Catalogue initialization failed: %d\n", result);
        return result == 0 ? 0 : 4;
    }
    if (result == 0) result = sd_id128_randomize(&authority.runtime);
    if (result == 0) result = sd_id128_randomize(&authority.events);
    if (result == 0 && (!uuid4(authority.runtime.bytes) || !uuid4(authority.events.bytes) ||
                       memcmp(authority.runtime.bytes, authority.events.bytes, AE_ID_BYTES) == 0)) result = -EIO;
    if (result == 0) result = ae_catalogue_begin_runtime(authority.catalogue, authority.boot.bytes, authority.runtime.bytes);
    sd_bus *bus = NULL;
    sd_event *event = NULL;
    sigset_t signals;
    sigemptyset(&signals); sigaddset(&signals, SIGTERM); sigaddset(&signals, SIGINT);
    if (result == 0 && sigprocmask(SIG_BLOCK, &signals, NULL) != 0) result = -errno;
    if (result == 0) result = sd_event_default(&event);
    if (result >= 0) result = sd_event_add_signal(event, NULL, SIGTERM, stop_signal, NULL);
    if (result >= 0) result = sd_event_add_signal(event, NULL, SIGINT, stop_signal, NULL);
    if (result >= 0) result = sd_bus_new(&bus);
    char address[128];
    if (system) snprintf(address, sizeof address, "unix:path=/run/dbus/system_bus_socket");
    else snprintf(address, sizeof address, "unix:path=" ROOT "/user-bus");
    if (result >= 0) result = sd_bus_set_address(bus, address);
    if (result >= 0) result = sd_bus_set_bus_client(bus, 1);
    if (result >= 0) result = sd_bus_set_method_call_timeout(bus, 2000000);
    if (result >= 0) result = sd_bus_set_exit_on_disconnect(bus, 1);
    if (result >= 0) result = sd_bus_start(bus);
    for (size_t n = 0; result >= 0 && wire_interfaces[n].roles != 0; ++n) {
        const struct wire_interface *wire = &wire_interfaces[n];
        if ((wire->roles & (int)authority.identity.kind) != 0)
            result = sd_bus_add_object_vtable(bus, NULL, wire->path, wire->name, wire->vtable, &authority);
    }
    if (result >= 0) result = sd_bus_attach_event(bus, event, 0);
    if (result >= 0) result = sd_bus_request_name(bus, system ? "org.aetheri.System1" : "org.aetheri.User1", 0);
    if (result >= 0) result = sd_notify(0, "READY=1\nSTATUS=Protocol endpoint available; lifecycle capabilities not implemented");
    if (result >= 0) result = sd_event_loop(event);
    if (result < 0) fprintf(stderr, "Authority startup/runtime failed: %d\n", result);
    sd_bus_flush_close_unref(bus);
    sd_event_unref(event);
    int closed = ae_catalogue_close(&authority.catalogue);
    return result < 0 || closed != 0 ? 5 : 0;
}
