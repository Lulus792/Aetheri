#include "catalogue/store.h"
#include "linux/peer.h"
#include "linux/process.h"
#include "authority/broker.h"
#include "core/allocation.h"
#include "core/fixture.h"
#include "authority/fixture.h"
#include "authority/projection.h"
#include "catalogue/data.h"
#include "catalogue/launch.h"
#include "authority/reservation.h"
#include "authority/setup.h"

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
#include <limits.h>
#include <time.h>

#define ROOT "/var/lib/aetheri-p0"

struct authority {
    struct ae_catalogue *catalogue;
    struct ae_catalogue_identity identity;
    sd_id128_t boot, runtime, events;
    uid_t uid;
    sd_event *event;
    sd_bus *system_bus;
    struct ae_broker broker;
    sd_id128_t system_runtime;
    int registered;
    struct ae_setup_service setup;
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
static int load_registry(struct authority *authority, char *state_path)
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
    if (state_path == NULL) return 0; /* Identity read only; never another owner's DB. */
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

static int broker_unit(char output[256])
{
    int fd = open_trusted(ROOT "/registry/broker.conf", 0);
    if (fd < 0) return fd;
    struct stat info;
    char bytes[256] = {0};
    int result = fstat(fd, &info) == 0 && S_ISREG(info.st_mode) && info.st_uid == 0 && info.st_nlink == 1 &&
                 (info.st_mode & 07777) == 0444 && info.st_size > 0 && info.st_size < 256 ? labeled(fd, "aetheri_registry_t") : -EACCES;
    ssize_t count = result == 0 ? read(fd, bytes, sizeof bytes - 1U) : -1;
    close(fd);
    const char prefix[] = "AETHERI-P0-BROKER-1\nunit=";
    if (result < 0) return result;
    if (count != info.st_size || count <= (ssize_t)sizeof prefix || bytes[count - 1] != '\n' ||
        memchr(bytes, 0, (size_t)count) != NULL || memcmp(bytes, prefix, sizeof prefix - 1U) != 0) return -EBADMSG;
    bytes[count - 1] = 0;
    const char *unit = bytes + sizeof prefix - 1U;
    size_t length = strlen(unit);
    if (length < 20 || length >= 256 || strncmp(unit, "aetheri-p0-", 11) != 0 ||
        strstr(unit, "-user-") == NULL || strcmp(unit + length - 8, ".service") != 0) return -ENODATA;
    for (size_t n = 0; n < length; ++n)
        if (!((unit[n] >= 'a' && unit[n] <= 'z') || (unit[n] >= '0' && unit[n] <= '9') || unit[n] == '-' || unit[n] == '.')) return -EBADMSG;
    memcpy(output, unit, length + 1U);
    return 0;
}

static int read_id(sd_bus_message *message, uint8_t output[16])
{
    const void *bytes = NULL;
    size_t size = 0;
    int result = sd_bus_message_read_array(message, 'y', &bytes, &size);
    if (result <= 0 || size != 16 || !uuid4(bytes)) return -EBADMSG;
    memcpy(output, bytes, size);
    return 0;
}

static int authority_info(struct authority *authority, struct ae_catalogue_info *info)
{
    int result = enforced_domain(authority->identity.kind == AE_CATALOGUE_SYSTEM ? "aetheri_system_t" : "aetheri_user_t");
    if (result == 0) result = ae_catalogue_get_info(authority->catalogue, info);
    if (result == 0 && (memcmp(info->boot_id, authority->boot.bytes, 16) != 0 ||
        memcmp(info->runtime_incarnation, authority->runtime.bytes, 16) != 0)) result = -ESTALE;
    return result;
}

static int runtime_reply(sd_bus_message *message, struct authority *authority)
{
    sd_bus_message *reply = NULL;
    int result = sd_bus_message_new_method_return(message, &reply);
    if (result >= 0) result = sd_bus_message_append_array(reply, 'y', authority->runtime.bytes, 16);
    if (result >= 0) result = sd_bus_send(NULL, reply, NULL);
    sd_bus_message_unref(reply);
    return result < 0 ? result : 1;
}

static int broker_current(struct authority *authority, struct ae_peer *peer, sd_bus_message *message)
{
    struct ae_catalogue_info info;
    int result = authority_info(authority, &info);
    if (result == 0) result = ae_peer_capture(message, peer);
    struct authority enrolled = {.uid = 1000, .identity.kind = AE_CATALOGUE_USER};
    char unit[256];
    if (result == 0) result = load_registry(&enrolled, NULL);
    int64_t account_revision;
    if (result == 0) result = ae_catalogue_check_account(authority->catalogue, enrolled.identity.owner, (uint32_t)enrolled.uid, &account_revision);
    if (result == 0) result = broker_unit(unit);
    if (result == 0 && (memcmp(enrolled.identity.authority_epoch, authority->broker.identity.epoch, 16) != 0 ||
        memcmp(enrolled.identity.owner, authority->broker.identity.account, 16) != 0 || strcmp(unit, authority->broker.context.unit) != 0)) result = -EACCES;
    if (result == 0) result = ae_broker_verify(&authority->broker, peer, sd_bus_message_get_bus(message));
    return result;
}

static int method_register(sd_bus_message *message, void *userdata, sd_bus_error *error)
{
    struct authority *authority = userdata;
    struct ae_peer peer = AE_PEER_INIT;
    struct ae_broker_identity claimed;
    uint8_t system_epoch[16];
    struct ae_catalogue_info info;
    int result = authority_info(authority, &info);
    if (result == 0) result = ae_peer_capture(message, &peer);
    if (result == 0) result = read_id(message, system_epoch);
    if (result == 0) result = read_id(message, claimed.account);
    if (result == 0) result = read_id(message, claimed.epoch);
    if (result == 0) result = read_id(message, claimed.runtime);
    struct authority enrolled = {.uid = 1000, .identity.kind = AE_CATALOGUE_USER};
    char unit[256];
    if (result == 0) result = load_registry(&enrolled, NULL);
    int64_t account_revision;
    if (result == 0) result = ae_catalogue_check_account(authority->catalogue, enrolled.identity.owner, (uint32_t)enrolled.uid, &account_revision);
    if (result == 0) result = broker_unit(unit);
    if (result == 0 && (memcmp(system_epoch, authority->identity.authority_epoch, 16) != 0 ||
        memcmp(claimed.account, enrolled.identity.owner, 16) != 0 || memcmp(claimed.epoch, enrolled.identity.authority_epoch, 16) != 0)) result = -EACCES;
    if (result == 0 && security_getenforce() != 1) result = -EACCES;
    if (result == 0) result = ae_broker_bind(&authority->broker, &peer, sd_bus_message_get_bus(message), authority->event,
                                           &claimed, unit, authority->boot.bytes);
    ae_peer_clear(&peer);
    if (result < 0) return sd_bus_error_set_const(error, result == -EALREADY ? "org.aetheri.Error1.Conflict" : "org.aetheri.Error1.NotAuthorized", "Protected broker enrollment required");
    return runtime_reply(message, authority);
}

static int method_check(sd_bus_message *message, void *userdata, sd_bus_error *error)
{
    struct authority *authority = userdata;
    struct ae_peer peer = AE_PEER_INIT;
    uint8_t runtime[16];
    int result = read_id(message, runtime);
    if (result == 0) result = broker_current(authority, &peer, message);
    if (result == 0 && memcmp(runtime, authority->broker.identity.runtime, 16) != 0) result = -EACCES;
    ae_peer_clear(&peer);
    if (result < 0) return sd_bus_error_set_const(error, result == -EAGAIN ? "org.aetheri.Error1.NeedsRecovery" : "org.aetheri.Error1.NotAuthorized", "Active registered broker required");
    return runtime_reply(message, authority);
}

static int owner_changed(sd_bus_message *message, void *userdata, sd_bus_error *error)
{
    (void)error;
    struct authority *authority = userdata;
    const char *name, *old_owner, *new_owner;
    int result = sd_bus_message_read(message, "sss", &name, &old_owner, &new_owner);
    (void)old_owner;
    if (result < 0) return result;
    if (authority->identity.kind == AE_CATALOGUE_SYSTEM) ae_broker_name_lost(&authority->broker, name, new_owner);
    else if (strcmp(name, "org.aetheri.System1") == 0) authority->registered = 0;
    return 0;
}

static int register_system(struct authority *authority)
{
    struct authority system = {.uid = 0, .identity.kind = AE_CATALOGUE_SYSTEM};
    int result = load_registry(&system, NULL);
    sd_bus_message *request = NULL, *reply = NULL;
    if (result == 0) result = sd_bus_message_new_method_call(authority->system_bus, &request, "org.aetheri.System1", "/org/aetheri/Broker1", "org.aetheri.Broker1", "Register");
    if (result >= 0) result = sd_bus_message_append_array(request, 'y', system.identity.authority_epoch, 16);
    if (result >= 0) result = sd_bus_message_append_array(request, 'y', authority->identity.owner, 16);
    if (result >= 0) result = sd_bus_message_append_array(request, 'y', authority->identity.authority_epoch, 16);
    if (result >= 0) result = sd_bus_message_append_array(request, 'y', authority->runtime.bytes, 16);
    if (result >= 0) result = sd_bus_call(authority->system_bus, request, 2000000, NULL, &reply);
    if (result > 0) result = read_id(reply, authority->system_runtime.bytes);
    sd_bus_message_unref(request); sd_bus_message_unref(reply);
    authority->registered = result == 0;
    return result;
}

static int check_system(struct authority *authority)
{
    int result = authority->registered ? 0 : register_system(authority);
    for (unsigned int attempt = 0; result == 0 && attempt < 2; ++attempt) {
        sd_bus_message *request = NULL, *reply = NULL;
        sd_bus_error error = SD_BUS_ERROR_NULL;
        result = sd_bus_message_new_method_call(authority->system_bus, &request, "org.aetheri.System1", "/org/aetheri/Broker1", "org.aetheri.Broker1", "Check");
        if (result >= 0) result = sd_bus_message_append_array(request, 'y', authority->runtime.bytes, 16);
        if (result >= 0) result = sd_bus_call(authority->system_bus, request, 2000000, &error, &reply);
        sd_id128_t runtime;
        if (result > 0) result = read_id(reply, runtime.bytes);
        if (result == 0 && memcmp(runtime.bytes, authority->system_runtime.bytes, 16) != 0) result = -ESTALE;
        int retry = attempt == 0 && result < 0 && sd_bus_error_has_name(&error, "org.aetheri.Error1.NotAuthorized");
        sd_bus_error_free(&error); sd_bus_message_unref(request); sd_bus_message_unref(reply);
        if (!retry) break;
        /* Explicit rejection can follow a root restart. Identity registration is
         * idempotent and still rechecks protected enrollment. Never retry timeout. */
        authority->registered = 0;
        result = register_system(authority);
    }
    if (result < 0) authority->registered = 0;
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
    result = authority_info(authority, &info);
    if (result != 0)
        return sd_bus_error_set_const(error, "org.aetheri.Error1.NeedsRecovery", "Authority context is unavailable");
    if (authority->identity.kind == AE_CATALOGUE_SYSTEM) {
        if (authority->broker.peer.pidfd >= 0 && ae_peer_alive(&authority->broker.peer) == -ESRCH) ae_broker_clear(&authority->broker);
        struct ae_process_context supervised;
        sd_id128_t invocation;
        result = ae_process_observe_self(sd_bus_message_get_bus(message), &supervised);
        if (result != 0 || sd_id128_get_invocation(&invocation) != 0 ||
            supervised.uid != authority->uid || !context_is(supervised.label, "aetheri_system_t") ||
            memcmp(invocation.bytes, supervised.invocation, sizeof supervised.invocation) != 0 ||
            memcmp(authority->boot.bytes, supervised.boot, sizeof supervised.boot) != 0)
            return sd_bus_error_set_const(error, "org.aetheri.Error1.NeedsRecovery", "Supervised authority context is unavailable");
    } else if (check_system(authority) != 0) return sd_bus_error_set_const(error, "org.aetheri.Error1.NeedsRecovery", "System broker registration is unavailable");
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
    struct authority *authority = userdata;
    int internal = strcmp(sd_bus_message_get_interface(message), "org.aetheri.Runtime1") == 0;
    struct ae_peer peer = AE_PEER_INIT;
    int result = internal ? broker_current(authority, &peer, message) : authenticate(message, userdata, error);
    ae_peer_clear(&peer);
    if (result < 0) return internal ? sd_bus_error_set_const(error, "org.aetheri.Error1.NotAuthorized", "Active registered broker required") : result;
    return sd_bus_error_set_const(error, "org.aetheri.Error1.UnsupportedCapability", "This authority does not implement that capability yet");
}

static int method_resolve_app(sd_bus_message *message,void *userdata,sd_bus_error *error)
{
    struct authority *authority=userdata;
    struct ae_peer peer=AE_PEER_INIT;
    int result=broker_current(authority,&peer,message);
    if (result<0) { ae_peer_clear(&peer); return sd_bus_error_set_const(error,"org.aetheri.Error1.NotAuthorized","Active registered broker required"); }
    uint8_t epoch[16],runtime[16],install[16];
    result=read_id(message,epoch);
    if (result==0) result=read_id(message,runtime);
    if (result==0) result=read_id(message,install);
    if (result==0 && sd_bus_message_at_end(message,1)!=1) result=-EBADMSG;
    if (result!=0) { ae_peer_clear(&peer); return sd_bus_error_set_const(error,"org.aetheri.Error1.InvalidArgument","Three UUIDv4 references required"); }
    if (memcmp(epoch,authority->identity.authority_epoch,16)!=0 || memcmp(runtime,authority->broker.identity.runtime,16)!=0) {
        ae_peer_clear(&peer); return sd_bus_error_set_const(error,"org.aetheri.Error1.StaleContext","Broker/system context changed");
    }
    struct ae_install_snapshot snapshot;
    result=ae_catalogue_resolve_app(authority->catalogue,authority->broker.identity.account,(uint32_t)peer.connection_uid,
        install,authority->boot.bytes,authority->runtime.bytes,&snapshot);
    ae_peer_clear(&peer);
    if (result!=0) return sd_bus_error_set_const(error,result==-ENOENT ? "org.aetheri.Error1.NotFound" : "org.aetheri.Error1.NeedsRecovery","Current eligible app installation unavailable");
    sd_bus_message *reply=NULL;
    result=sd_bus_message_new_method_return(message,&reply);
    if (result>=0) result=ae_projection_write(reply,&snapshot);
    if (result>=0) result=sd_bus_send(NULL,reply,NULL);
    sd_bus_message_unref(reply);
    return result<0 ? result : 1;
}

static int method_refresh_fixture(sd_bus_message *message,void *userdata,sd_bus_error *error)
{
    struct authority *authority=userdata;
    struct ae_peer peer=AE_PEER_INIT;
    int result=ae_peer_capture(message,&peer);
    if (result==0 && (authority->identity.kind!=AE_CATALOGUE_USER || peer.connection_uid!=authority->uid ||
        !context_is(peer.connection_label,"aetheri_user_t"))) result=-EACCES;
    char unit[256];
    struct ae_process_context controller;
    if (result==0) result=ae_authority_projection_unit(unit);
    if (result==0) result=ae_process_observe(&peer,authority->system_bus,&controller);
    if (result==0 && (controller.uid!=authority->uid || !context_is(controller.label,"aetheri_user_t") ||
        strcmp(controller.unit,unit)!=0 || memcmp(controller.boot,authority->boot.bytes,16)!=0)) result=-EACCES;
    if (result!=0) { ae_peer_clear(&peer); return sd_bus_error_set_const(error,"org.aetheri.Error1.NotAuthorized","Protected P0 controller unit required"); }
    struct ae_fixture_request selection;
    result=ae_authority_fixture_request(AE_FIXTURE_ORIGINAL,&selection);
    if (result==0 && memcmp(selection.scope,authority->identity.owner,16)!=0) result=-ESTALE;
    if (result==0) result=check_system(authority);
    struct authority system={.uid=0,.identity.kind=AE_CATALOGUE_SYSTEM};
    if (result==0) result=load_registry(&system,NULL);
    struct ae_projection_context context={0};
    if (result==0) {
        memcpy(context.system_epoch,system.identity.authority_epoch,16);
        memcpy(context.system_runtime,authority->system_runtime.bytes,16);
        memcpy(context.boot,authority->boot.bytes,16); memcpy(context.user_runtime,authority->runtime.bytes,16);
    }
    struct ae_install_snapshot snapshot;
    if (result==0) result=ae_projection_fetch(authority->system_bus,&context,authority->identity.owner,selection.install,&snapshot);
    if (result==0 && (memcmp(snapshot.publisher,selection.publisher,16)!=0 || memcmp(snapshot.artifact,selection.artifact,32)!=0 ||
        memcmp(snapshot.generation,selection.generation,16)!=0 || !authority->registered)) result=-ESTALE;
    if (result==0) result=ae_process_verify(&peer,authority->system_bus,&controller);
    char current_unit[256];
    if (result==0) result=ae_authority_projection_unit(current_unit);
    if (result==0 && strcmp(current_unit,unit)!=0) result=-ESTALE;
    struct ae_catalogue_info info;
    if (result==0) result=authority_info(authority,&info);
    int64_t revision;
    if (result==0) result=ae_catalogue_cache_install(authority->catalogue,&snapshot,&context,&revision);
    ae_peer_clear(&peer);
    if (result!=0) return sd_bus_error_set_const(error,"org.aetheri.Error1.NeedsRecovery","Protected fixture projection requires reconciliation");
    sd_bus_message *reply=NULL;
    result=sd_bus_message_new_method_return(message,&reply);
    if (result>=0) result=sd_bus_message_append_array(reply,'y',snapshot.install,16);
    if (result>=0) result=sd_bus_message_append(reply,"tt",(uint64_t)snapshot.install_revision,(uint64_t)revision);
    if (result>=0) result=sd_bus_send(NULL,reply,NULL);
    sd_bus_message_unref(reply);
    return result<0 ? result : 1;
}

static int app_data_config(const char *path,uint8_t *bytes,size_t size);
static int app_data_action(struct authority *authority,const char *control,int enroll);
static int fixture_runtime_call(struct authority *authority,const char *method,const struct ae_launch_request *request,const uint8_t cause[16],sd_bus_message **reply)
{
    sd_bus_message *call=NULL;
    int result=sd_bus_message_new_method_call(authority->system_bus,&call,"org.aetheri.System1","/org/aetheri/Runtime1","org.aetheri.Runtime1",method);
    if (result>=0 && strcmp(method,"ReserveLaunch")==0) result=ae_reservation_write(call,request);
    else if (result>=0 && strcmp(method,"GetDispatch")==0) result=sd_bus_message_append_array(call,'y',request->lease,16);
    else if (result>=0) {
        const uint8_t *ids[]={request->system_epoch,request->lease,request->broker_epoch,cause};
        for (size_t n=0;result>=0 && n<4;++n) result=sd_bus_message_append_array(call,'y',ids[n],16);
        if (result>=0) result=sd_bus_message_append(call,"t",UINT64_C(1));
    }
    if (result>=0) result=sd_bus_call(authority->system_bus,call,2000000,NULL,reply);
    sd_bus_message_unref(call); return result<0 ? result : 0;
}
static int fixture_dispatch_state(struct authority *authority,const struct ae_launch_request *request,const char *expected,uint64_t revision,uint64_t gate)
{
    sd_bus_message *reply=NULL;
    int result=fixture_runtime_call(authority,"GetDispatch",request,NULL,&reply);
    uint8_t launch[16]; const char *state=NULL,*unit=NULL; const void *invocation=NULL; size_t size=0;
    uint64_t actual_revision=0,actual_gate=0;
    if (result==0) result=read_id(reply,launch);
    if (result==0 && sd_bus_message_read(reply,"ss",&state,&unit)<=0) result=-EBADMSG;
    if (result==0 && sd_bus_message_read_array(reply,'y',&invocation,&size)<=0) result=-EBADMSG;
    if (result==0 && sd_bus_message_read(reply,"tt",&actual_revision,&actual_gate)<=0) result=-EBADMSG;
    char id[33],expected_unit[80]; sd_id128_t reference;
    memcpy(reference.bytes,request->launch,16); sd_id128_to_string(reference,id);
    snprintf(expected_unit,sizeof expected_unit,"aetheri-app-%s.service",id);
    if (result==0 && (memcmp(launch,request->launch,16)!=0 || strcmp(state,expected)!=0 || strcmp(unit,expected_unit)!=0 || size!=0 ||
        actual_revision!=revision || actual_gate!=gate || sd_bus_message_at_end(reply,1)!=1)) result=-ESTALE;
    sd_bus_message_unref(reply); return result;
}
static int method_test_reservation(sd_bus_message *message,void *userdata,sd_bus_error *error)
{
    struct authority *authority=userdata; struct ae_peer peer=AE_PEER_INIT;
    int result=ae_peer_capture(message,&peer);
    if (result==0 && (authority->identity.kind!=AE_CATALOGUE_USER || peer.connection_uid!=authority->uid || !context_is(peer.connection_label,"aetheri_user_t"))) result=-EACCES;
    char unit[256]; struct ae_process_context controller;
    if (result==0) result=ae_authority_projection_unit(unit);
    if (result==0) result=ae_process_observe(&peer,authority->system_bus,&controller);
    if (result==0 && (controller.uid!=authority->uid || !context_is(controller.label,"aetheri_user_t") || strcmp(controller.unit,unit)!=0 || memcmp(controller.boot,authority->boot.bytes,16)!=0)) result=-EACCES;
    if (result!=0) { ae_peer_clear(&peer); return sd_bus_error_set_const(error,"org.aetheri.Error1.NotAuthorized","Protected P0 controller unit required"); }
    uint8_t config[56],data_bytes[AE_DATA_CONFIG_BYTES];
    result=app_data_config(ROOT "/registry/reservation.conf",config,sizeof config);
    if (result==0 && memcmp(config,"AETHRSF1",8)!=0) result=-EBADMSG;
    for (size_t n=0;result==0 && n<3;++n) if (!uuid4(config+8+16*n)) result=-EBADMSG;
    struct ae_data_enrollment data;
    if (result==0) result=app_data_config(ROOT "/registry/app-data.conf",data_bytes,sizeof data_bytes);
    if (result==0) result=ae_data_enrollment_decode(data_bytes,sizeof data_bytes,&data);
    if (result==0 && (memcmp(data.owner.id[AE_ACCOUNT],authority->identity.owner,16)!=0 || memcmp(data.user_epoch,authority->identity.authority_epoch,16)!=0)) result=-ESTALE;
    if (result==0) result=check_system(authority);
    struct authority system={.uid=0,.identity.kind=AE_CATALOGUE_SYSTEM};
    if (result==0) result=load_registry(&system,NULL);
    struct ae_projection_context context={0};
    if (result==0) {
        memcpy(context.system_epoch,system.identity.authority_epoch,16); memcpy(context.system_runtime,authority->system_runtime.bytes,16);
        memcpy(context.user_runtime,authority->runtime.bytes,16); memcpy(context.boot,authority->boot.bytes,16);
    }
    struct ae_install_snapshot install;
    if (result==0) result=ae_projection_fetch(authority->system_bus,&context,authority->identity.owner,data.install,&install);
    struct ae_launch_request request={0};
    if (result==0) {
        memcpy(request.system_epoch,context.system_epoch,16); memcpy(request.broker_epoch,authority->identity.authority_epoch,16);
        memcpy(request.broker_runtime,authority->runtime.bytes,16); memcpy(request.account,authority->identity.owner,16); memcpy(request.boot,authority->boot.bytes,16);
        memcpy(request.install,install.install,16); memcpy(request.generation,install.generation,16); memcpy(request.data,data.owner.id[AE_DATA],16);
        memcpy(request.launch,config+8,16); memcpy(request.lease,config+24,16); request.install_revision=install.install_revision;
        strcpy(request.action,"main"); strcpy(request.session,"p0"); strcpy(request.seat,"none");
        result=ae_process_verify(&peer,authority->system_bus,&controller);
    }
    char selected[256];
    if (result==0) result=ae_authority_projection_unit(selected);
    if (result==0 && strcmp(unit,selected)!=0) result=-ESTALE;
    uint8_t operation[16]={0}; uint64_t gate=0,revision=0;
    for (int n=0;result==0 && n<2;++n) {
        sd_bus_message *reply=NULL;
        result=fixture_runtime_call(authority,"ReserveLaunch",&request,config+40,&reply);
        uint8_t current[16]; uint64_t current_gate=0,current_revision=0;
        if (result==0) result=read_id(reply,current);
        if (result==0 && (sd_bus_message_read(reply,"tt",&current_gate,&current_revision)<=0 || current_gate<1 || current_gate>INT64_MAX || current_revision!=1 || sd_bus_message_at_end(reply,1)!=1)) result=-EBADMSG;
        if (result==0 && n==0) { memcpy(operation,current,16); gate=current_gate; }
        else if (result==0 && (memcmp(operation,current,16)!=0 || gate!=current_gate)) result=-ESTALE;
        sd_bus_message_unref(reply);
    }
    if (result==0) result=fixture_dispatch_state(authority,&request,"reserved",1,gate);
    for (int n=0;result==0 && n<2;++n) {
        sd_bus_message *reply=NULL; const char *state=NULL;
        result=fixture_runtime_call(authority,"CloseLaunch",&request,config+40,&reply);
        if (result==0 && (sd_bus_message_read(reply,"st",&state,&revision)<=0 || strcmp(state,"terminal")!=0 || revision!=2 || sd_bus_message_at_end(reply,1)!=1)) result=-EBADMSG;
        sd_bus_message_unref(reply);
    }
    if (result==0) result=fixture_dispatch_state(authority,&request,"terminal",2,gate);
    ae_peer_clear(&peer);
    if (result!=0) return sd_bus_error_set_const(error,"org.aetheri.Error1.NeedsRecovery","Protected reservation probe requires reconciliation");
    sd_bus_message *reply=NULL;
    result=sd_bus_message_new_method_return(message,&reply);
    if (result>=0) result=sd_bus_message_append_array(reply,'y',operation,16);
    if (result>=0) result=sd_bus_message_append(reply,"tt",gate,revision);
    if (result>=0) result=sd_bus_send(NULL,reply,NULL);
    sd_bus_message_unref(reply); return result<0 ? result : 1;
}

static void launch_context(struct authority *authority,const struct ae_peer *peer,struct ae_launch_context *context)
{
    memcpy(context->system_runtime,authority->runtime.bytes,16); memcpy(context->broker_epoch,authority->broker.identity.epoch,16);
    memcpy(context->broker_runtime,authority->broker.identity.runtime,16); memcpy(context->account,authority->broker.identity.account,16);
    memcpy(context->boot,authority->boot.bytes,16); context->uid=(uint32_t)peer->connection_uid;
}
static int reservation_error(sd_bus_error *error,int result)
{
    const char *name=result==-EINVAL || result==-EBADMSG ? "org.aetheri.Error1.InvalidArgument" :
        result==-EEXIST ? "org.aetheri.Error1.RequestConflict" : result==-ENOENT ? "org.aetheri.Error1.NotFound" :
        result==-ESTALE ? "org.aetheri.Error1.StaleRevision" : "org.aetheri.Error1.NeedsRecovery";
    return sd_bus_error_set_const(error,name,"Launch reservation unavailable or requires reconciliation");
}
static int reservation_stamp(int64_t *stamp,int64_t *expires)
{
    struct timespec real,boot;
    if (clock_gettime(CLOCK_REALTIME,&real)!=0 || clock_gettime(CLOCK_BOOTTIME,&boot)!=0) return -errno;
    if (real.tv_sec<0 || real.tv_sec>(INT64_MAX-999)/1000 || boot.tv_sec<0 || boot.tv_sec>(INT64_MAX-6000000000LL)/1000000000LL) return -EOVERFLOW;
    *stamp=(int64_t)real.tv_sec*1000+real.tv_nsec/1000000;
    *expires=(int64_t)boot.tv_sec*1000000000LL+boot.tv_nsec+5000000000LL;
    return 0;
}
static int method_reserve_launch(sd_bus_message *message,void *userdata,sd_bus_error *error)
{
    struct authority *authority=userdata;
    struct ae_peer peer=AE_PEER_INIT;
    int result=broker_current(authority,&peer,message);
    if (result!=0) { ae_peer_clear(&peer); return sd_bus_error_set_const(error,"org.aetheri.Error1.NotAuthorized","Active registered broker required"); }
    struct ae_launch_context context; launch_context(authority,&peer,&context);
    struct ae_launch_request request;
    result=ae_reservation_read(message,&request);
    if (result!=0) { ae_peer_clear(&peer); return reservation_error(error,result); }
    if (memcmp(request.system_epoch,authority->identity.authority_epoch,16)!=0 || memcmp(request.broker_epoch,context.broker_epoch,16)!=0 ||
        memcmp(request.broker_runtime,context.broker_runtime,16)!=0 || memcmp(request.account,context.account,16)!=0 || memcmp(request.boot,context.boot,16)!=0) {
        ae_peer_clear(&peer); return sd_bus_error_set_const(error,"org.aetheri.Error1.StaleContext","Current registered launch context required");
    }
    /* P0 has one explicitly trusted headless action/session profile. Arbitrary
     * caller claims are not logind authorization or general installed app policy. */
    struct ae_fixture_request fixture;
    if (strcmp(request.action,"main")!=0 || strcmp(request.session,"p0")!=0 || strcmp(request.seat,"none")!=0) result=-EACCES;
    if (result==0) result=ae_authority_fixture_request(AE_FIXTURE_ORIGINAL,&fixture);
    if (result==0 && (memcmp(request.install,fixture.install,16)!=0 || memcmp(request.generation,fixture.generation,16)!=0)) result=-EACCES;
    uint8_t data_bytes[AE_DATA_CONFIG_BYTES]; struct ae_data_enrollment data;
    if (result==0) result=app_data_config(ROOT "/registry/app-data.conf",data_bytes,sizeof data_bytes);
    if (result==0) result=ae_data_enrollment_decode(data_bytes,sizeof data_bytes,&data);
    if (result==0 && (memcmp(request.data,data.owner.id[AE_DATA],16)!=0 || memcmp(request.account,data.owner.id[AE_ACCOUNT],16)!=0 ||
        memcmp(request.install,data.install,16)!=0 || memcmp(request.broker_epoch,data.user_epoch,16)!=0 || memcmp(request.system_epoch,data.system_epoch,16)!=0)) result=-EACCES;
    if (result!=0) { ae_peer_clear(&peer); return sd_bus_error_set_const(error,"org.aetheri.Error1.NotAuthorized","Protected P0 action/session profile required"); }
    struct ae_reservation_ids proposed;
    sd_id128_t plan,operation;
    result=sd_id128_randomize(&plan);
    if (result==0) result=sd_id128_randomize(&operation);
    if (result==0) { memcpy(proposed.plan,plan.bytes,16); memcpy(proposed.operation,operation.bytes,16); result=reservation_stamp(&proposed.accepted_unix_ms,&proposed.expires_boottime_ns); }
    struct ae_dispatch_snapshot snapshot;
    if (result==0) result=ae_catalogue_reserve_launch(authority->catalogue,&request,&context,&proposed,&snapshot);
    ae_peer_clear(&peer);
    if (result!=0) return reservation_error(error,result);
    sd_bus_message *reply=NULL;
    result=sd_bus_message_new_method_return(message,&reply);
    if (result>=0) result=sd_bus_message_append_array(reply,'y',snapshot.operation,16);
    if (result>=0) result=sd_bus_message_append(reply,"tt",(uint64_t)snapshot.install_gate_revision,(uint64_t)snapshot.revision);
    if (result>=0) result=sd_bus_send(NULL,reply,NULL);
    sd_bus_message_unref(reply); return result<0 ? result : 1;
}
static int method_get_dispatch(sd_bus_message *message,void *userdata,sd_bus_error *error)
{
    struct authority *authority=userdata; struct ae_peer peer=AE_PEER_INIT;
    int result=broker_current(authority,&peer,message);
    if (result!=0) { ae_peer_clear(&peer); return sd_bus_error_set_const(error,"org.aetheri.Error1.NotAuthorized","Active registered broker required"); }
    struct ae_launch_context context; launch_context(authority,&peer,&context);
    uint8_t lease[16]; result=read_id(message,lease);
    struct ae_dispatch_snapshot snapshot;
    if (result==0) result=ae_catalogue_get_dispatch(authority->catalogue,lease,&context,&snapshot);
    ae_peer_clear(&peer);
    if (result!=0) return reservation_error(error,result);
    sd_bus_message *reply=NULL;
    result=sd_bus_message_new_method_return(message,&reply);
    if (result>=0) result=sd_bus_message_append_array(reply,'y',snapshot.launch,16);
    if (result>=0) result=sd_bus_message_append(reply,"ss",snapshot.state,snapshot.unit);
    if (result>=0) result=sd_bus_message_append_array(reply,'y',snapshot.invocation,snapshot.invocation_present ? 16U : 0U);
    if (result>=0) result=sd_bus_message_append(reply,"tt",(uint64_t)snapshot.revision,(uint64_t)snapshot.install_gate_revision);
    if (result>=0) result=sd_bus_send(NULL,reply,NULL);
    sd_bus_message_unref(reply); return result<0 ? result : 1;
}
static int method_get_receipt(sd_bus_message *message,void *userdata,sd_bus_error *error)
{
    struct authority *authority=userdata; struct ae_peer peer=AE_PEER_INIT;
    int result=broker_current(authority,&peer,message);
    if (result!=0) { ae_peer_clear(&peer); return sd_bus_error_set_const(error,"org.aetheri.Error1.NotAuthorized","Active registered broker required"); }
    struct ae_launch_context context; launch_context(authority,&peer,&context);
    uint8_t lease[16]; result=read_id(message,lease);
    struct ae_reservation_receipt receipt;
    if (result==0) result=ae_catalogue_get_receipt(authority->catalogue,lease,&context,&receipt);
    ae_peer_clear(&peer);
    if (result!=0) return reservation_error(error,result);
    sd_bus_message *reply=NULL;
    result=sd_bus_message_new_method_return(message,&reply);
    if (result>=0) result=ae_receipt_write(reply,&receipt);
    if (result>=0) result=sd_bus_send(NULL,reply,NULL);
    sd_bus_message_unref(reply); return result<0 ? result : 1;
}
static int method_close_launch(sd_bus_message *message,void *userdata,sd_bus_error *error)
{
    struct authority *authority=userdata; struct ae_peer peer=AE_PEER_INIT;
    int result=broker_current(authority,&peer,message);
    if (result!=0) { ae_peer_clear(&peer); return sd_bus_error_set_const(error,"org.aetheri.Error1.NotAuthorized","Active registered broker required"); }
    struct ae_launch_context context; launch_context(authority,&peer,&context);
    uint8_t epoch[16],lease[16],cause_epoch[16],cause[16]; uint64_t revision=0;
    result=read_id(message,epoch);
    if (result==0) result=read_id(message,lease);
    if (result==0) result=read_id(message,cause_epoch);
    if (result==0) result=read_id(message,cause);
    if (result==0) { int code=sd_bus_message_read(message,"t",&revision); if (code<=0 || revision<1 || revision>INT64_MAX) result=-EBADMSG; }
    if (result==0 && memcmp(epoch,authority->identity.authority_epoch,16)!=0) result=-ESTALE;
    int64_t stamp=0,expires=0;
    if (result==0) result=reservation_stamp(&stamp,&expires);
    struct ae_dispatch_snapshot snapshot;
    if (result==0) result=ae_catalogue_cancel_reservation(authority->catalogue,lease,&context,cause_epoch,cause,(int64_t)revision,stamp,&snapshot);
    ae_peer_clear(&peer);
    if (result!=0) return reservation_error(error,result);
    return sd_bus_reply_method_return(message,"st",snapshot.state,(uint64_t)snapshot.revision);
}

struct wire_interface { int roles; const char *path, *name; const sd_bus_vtable *vtable; };
/* Development-only fixed fixture path. No arbitrary app/action/session/path is
 * accepted here; public launch remains unsupported until dispatch is complete. */
static int launch_controller(struct authority *a,sd_bus_message *m,struct ae_peer *peer,struct ae_process_context *observed,char unit[256])
{
    int r=ae_peer_capture(m,peer);
    if (r==0 && (a->identity.kind!=AE_CATALOGUE_USER || peer->connection_uid!=a->uid || !context_is(peer->connection_label,"aetheri_user_t"))) r=-EACCES;
    if (r==0) r=ae_authority_projection_unit(unit);
    if (r==0) r=ae_process_observe(peer,a->system_bus,observed);
    if (r==0 && (observed->uid!=a->uid || !context_is(observed->label,"aetheri_user_t") || strcmp(observed->unit,unit)!=0 || memcmp(observed->boot,a->boot.bytes,16)!=0)) r=-EACCES;
    return r;
}
static int launch_controller_current(struct authority *a,struct ae_peer *peer,const struct ae_process_context *observed,const char *unit)
{
    int r=ae_process_verify(peer,a->system_bus,observed); char selected[256];
    if (r==0) r=ae_authority_projection_unit(selected);
    if (r==0 && strcmp(unit,selected)!=0) r=-ESTALE;
    return r;
}
static int launch_config(uint8_t bytes[136])
{
    int r=app_data_config(ROOT "/registry/launch.conf",bytes,136);
    if (r==0 && memcmp(bytes,"AETHLNF1",8)!=0) r=-EBADMSG;
    for (size_t n=0;r==0 && n<6;++n) if (!uuid4(bytes+8+16*n)) r=-EBADMSG;
    return r;
}
static int launch_number(const uint8_t *bytes,int64_t *out)
{
    uint64_t value=0;
    for (size_t n=0;n<8;++n) value=(value<<8)|bytes[n];
    if (value>INT64_MAX) return -EOVERFLOW;
    *out=(int64_t)value; return 0;
}
static int fetch_receipt(struct authority *a,const uint8_t lease[16],struct ae_reservation_receipt *out)
{
    int r=check_system(a); struct authority root={.uid=0,.identity.kind=AE_CATALOGUE_SYSTEM};
    if (r==0) r=load_registry(&root,NULL);
    sd_id128_t expected=a->system_runtime;
    sd_bus_message *call=NULL,*reply=NULL; struct ae_reservation_receipt receipt;
    if (r==0) r=sd_bus_message_new_method_call(a->system_bus,&call,"org.aetheri.System1","/org/aetheri/Catalogue1","org.aetheri.Catalogue1","GetReservationReceipt");
    if (r>=0) r=sd_bus_message_append_array(call,'y',lease,16);
    if (r>=0) r=sd_bus_call(a->system_bus,call,2000000,NULL,&reply);
    if (r>=0) r=ae_receipt_read(reply,&receipt);
    if (r==0 && (memcmp(receipt.system_epoch,root.identity.authority_epoch,16)!=0 || memcmp(receipt.system_runtime,expected.bytes,16)!=0 ||
        memcmp(receipt.boot,a->boot.bytes,16)!=0 || memcmp(receipt.lease,lease,16)!=0)) r=-ESTALE;
    if (r==0) r=check_system(a);
    if (r==0 && memcmp(expected.bytes,a->system_runtime.bytes,16)!=0) r=-ESTALE;
    sd_bus_message_unref(call); sd_bus_message_unref(reply);
    if (r==0) *out=receipt;
    return r;
}
static int launch_reply(sd_bus_message *message,const struct ae_user_launch_snapshot *local,const struct ae_reservation_receipt *root)
{
    sd_bus_message *reply=NULL; int r=sd_bus_message_new_method_return(message,&reply);
    if (r>=0) r=sd_bus_message_append_array(reply,'y',local->operation,16);
    if (r>=0) r=sd_bus_message_append(reply,"tt",(uint64_t)local->revision,(uint64_t)root->dispatch.revision);
    if (r>=0) r=sd_bus_send(NULL,reply,NULL);
    sd_bus_message_unref(reply); return r<0 ? r : 1;
}
static int method_prepare_fixture_launch(sd_bus_message *message,void *userdata,sd_bus_error *error)
{
    struct authority *a=userdata; struct ae_peer peer=AE_PEER_INIT; struct ae_process_context controller; char unit[256];
    int r=launch_controller(a,message,&peer,&controller,unit);
    if (r!=0) { ae_peer_clear(&peer); return sd_bus_error_set_const(error,"org.aetheri.Error1.NotAuthorized","Protected P0 controller unit required"); }
    uint8_t bytes[136],data_bytes[AE_DATA_CONFIG_BYTES]; struct ae_data_enrollment data;
    r=launch_config(bytes);
    if (r==0) r=app_data_config(ROOT "/registry/app-data.conf",data_bytes,sizeof data_bytes);
    if (r==0) r=ae_data_enrollment_decode(data_bytes,sizeof data_bytes,&data);
    char account[33],control[256]; sd_id128_t owner; memcpy(owner.bytes,a->identity.owner,16); sd_id128_to_string(owner,account);
    int length=snprintf(control,sizeof control,ROOT "/users/%s/control",account);
    if (r==0 && (length<0 || length>=(int)sizeof control)) r=-EOVERFLOW;
    /* Existing exact enrollment is read-only metadata; independently reopen and
     * verify its allocation, held payload identity and MAC before gate admission. */
    if (r==0) r=app_data_action(a,control,1);
    if (r==0) r=check_system(a);
    struct authority system={.uid=0,.identity.kind=AE_CATALOGUE_SYSTEM};
    if (r==0) r=load_registry(&system,NULL);
    struct ae_projection_context context={0}; struct ae_user_launch q={0};
    if (r==0) {
        memcpy(context.system_epoch,system.identity.authority_epoch,16); memcpy(context.system_runtime,a->system_runtime.bytes,16);
        memcpy(context.boot,a->boot.bytes,16); memcpy(context.user_runtime,a->runtime.bytes,16);
        r=ae_projection_fetch(a->system_bus,&context,a->identity.owner,data.install,&q.source);
    }
    if (r==0) {
        struct ae_launch_request *request=&q.reservation;
        memcpy(request->system_epoch,context.system_epoch,16); memcpy(request->broker_epoch,a->identity.authority_epoch,16);
        memcpy(request->broker_runtime,a->runtime.bytes,16); memcpy(request->account,a->identity.owner,16); memcpy(request->boot,a->boot.bytes,16);
        memcpy(request->install,q.source.install,16); memcpy(request->generation,q.source.generation,16); memcpy(request->data,data.owner.id[AE_DATA],16);
        request->install_revision=q.source.install_revision; strcpy(request->action,"main"); strcpy(request->session,"p0"); strcpy(request->seat,"none");
        memcpy(q.local.plan,bytes+8,16); memcpy(q.local.operation,bytes+24,16); memcpy(q.actor,bytes+40,16); memcpy(q.request_key,bytes+56,16);
        memcpy(request->launch,bytes+72,16); memcpy(request->lease,bytes+88,16); memcpy(q.storage,data.owner.id[AE_STORAGE],16);
        r=launch_number(bytes+104,&q.data_revision);
        if (r==0) r=launch_number(bytes+112,&q.data_gate_revision);
        if (r==0) r=launch_number(bytes+120,&q.local.accepted_unix_ms);
        if (r==0) r=launch_number(bytes+128,&q.local.expires_boottime_ns);
    }
    if (r==0) r=launch_controller_current(a,&peer,&controller,unit);
    int64_t gate=0; struct ae_user_launch_snapshot local; struct ae_reservation_receipt receipt;
    if (r==0) r=ae_catalogue_admit_launch_data(a->catalogue,&q,&gate);
    if (r==0) { q.data_gate_revision=gate; r=ae_catalogue_accept_launch(a->catalogue,&q,&local); }
    if (r==0 && strcmp(local.state,"requested")!=0 && strcmp(local.state,"preparing")!=0) r=-ESTALE;
    sd_bus_message *reply=NULL;
    if (r==0) r=fixture_runtime_call(a,"ReserveLaunch",&q.reservation,NULL,&reply);
    sd_bus_message_unref(reply);
    if (r==0) r=fetch_receipt(a,q.reservation.lease,&receipt);
    if (r==0) r=ae_catalogue_ack_launch(a->catalogue,&q,&receipt,&local);
    ae_peer_clear(&peer);
    if (r!=0) return reservation_error(error,r);
    return launch_reply(message,&local,&receipt);
}
static int method_cancel_fixture_launch(sd_bus_message *message,void *userdata,sd_bus_error *error)
{
    struct authority *a=userdata; struct ae_peer peer=AE_PEER_INIT; struct ae_process_context controller; char unit[256];
    int r=launch_controller(a,message,&peer,&controller,unit);
    if (r!=0) { ae_peer_clear(&peer); return sd_bus_error_set_const(error,"org.aetheri.Error1.NotAuthorized","Protected P0 controller unit required"); }
    uint8_t bytes[136]; struct ae_user_launch_snapshot local; struct ae_reservation_receipt receipt;
    r=launch_config(bytes);
    if (r==0) r=ae_catalogue_get_user_launch(a->catalogue,bytes+24,&local);
    if (r==0) r=fetch_receipt(a,local.lease,&receipt);
    if (r==0 && (memcmp(local.system_epoch,receipt.system_epoch,16)!=0 || memcmp(local.launch,receipt.dispatch.launch,16)!=0 || memcmp(local.fingerprint,receipt.fingerprint,32)!=0)) r=-ESTALE;
    if (r==0) r=launch_controller_current(a,&peer,&controller,unit);
    if (r==0 && !receipt.withdrawn) {
        if (!local.pre_dispatch_withdrawable || strcmp(receipt.dispatch.state,"reserved")!=0 || receipt.dispatch.revision!=1) r=-EAGAIN;
        struct ae_launch_request request={0};
        memcpy(request.system_epoch,local.system_epoch,16); memcpy(request.lease,local.lease,16); memcpy(request.broker_epoch,a->identity.authority_epoch,16);
        sd_bus_message *reply=NULL;
        if (r==0) r=fixture_runtime_call(a,"CloseLaunch",&request,local.operation,&reply);
        sd_bus_message_unref(reply);
        if (r==0) r=fetch_receipt(a,local.lease,&receipt);
    }
    int64_t stamp=0,unused=0;
    if (r==0) r=reservation_stamp(&stamp,&unused);
    if (r==0) r=ae_catalogue_finish_withdrawn_operation(a->catalogue,local.operation,&receipt,stamp,&local);
    ae_peer_clear(&peer);
    if (r!=0) return reservation_error(error,r);
    return launch_reply(message,&local,&receipt);
}

static int setup_broker(void *userdata,sd_bus_message *message,struct ae_launch_context *context)
{
    struct authority *a=userdata; struct ae_peer peer=AE_PEER_INIT;
    int r=broker_current(a,&peer,message);
    if (r==0) launch_context(a,&peer,context);
    ae_peer_clear(&peer); return r;
}
static int method_setup_prepare(sd_bus_message *m,void *a,sd_bus_error *e) { return ae_setup_method_prepare(m,&((struct authority *)a)->setup,e); }
static int method_setup_register(sd_bus_message *m,void *a,sd_bus_error *e) { return ae_setup_method_register(m,&((struct authority *)a)->setup,e); }
static int method_setup_prepared(sd_bus_message *m,void *a,sd_bus_error *e) { return ae_setup_method_prepared(m,&((struct authority *)a)->setup,e); }
static int method_setup_withdraw(sd_bus_message *m,void *a,sd_bus_error *e) { return ae_setup_method_withdraw(m,&((struct authority *)a)->setup,e); }
static int method_fixture_setup(sd_bus_message *message,void *userdata,sd_bus_error *error)
{
    struct authority *a=userdata; struct ae_peer peer=AE_PEER_INIT; struct ae_process_context controller; char unit[256];
    int r=launch_controller(a,message,&peer,&controller,unit);
    if (r!=0) { ae_peer_clear(&peer); return sd_bus_error_set_const(error,"org.aetheri.Error1.NotAuthorized","Protected P0 controller unit required"); }
    struct ae_setup_profile p; uint8_t hash[32]; r=ae_setup_load(&p,hash);
    if (r==0) r=check_system(a);
    int cancel=strcmp(sd_bus_message_get_member(message),"CancelFixtureSetup")==0;
    struct ae_user_launch_snapshot local;
    if (r==0) r=ae_catalogue_get_user_launch(a->catalogue,p.user_operation,&local);
    if (r==0 && (memcmp(p.broker_epoch,a->identity.authority_epoch,16)!=0 || memcmp(p.account,a->identity.owner,16)!=0 ||
        memcmp(local.launch,p.launch,16)!=0 || memcmp(local.lease,p.lease,16)!=0 || memcmp(local.root_operation,p.root_operation,16)!=0 ||
        memcmp(local.system_epoch,p.system_epoch,16)!=0)) r=-ESTALE;
    if (r==0 && cancel && strcmp(local.state,"start_failed")!=0 && !local.pre_dispatch_withdrawable) r=-EAGAIN;
    if (r==0 && !cancel) r=ae_catalogue_authorize_user_setup(a->catalogue,&p);
    if (r==0) r=launch_controller_current(a,&peer,&controller,unit);
    sd_bus_message *call=NULL,*reply=NULL;
    if (r==0) r=sd_bus_message_new_method_call(a->system_bus,&call,"org.aetheri.System1","/org/aetheri/Setup1","org.aetheri.Setup1",cancel ? "WithdrawSetup" : "PrepareSetup");
    if (r>=0) r=sd_bus_message_append_array(call,'y',p.lease,16);
    if (r>=0 && cancel) r=sd_bus_message_append_array(call,'y',p.user_operation,16);
    if (r>=0 && !cancel) r=sd_bus_message_append(call,"t",(uint64_t)p.data_gate_revision);
    if (r>=0 && !cancel) r=sd_bus_message_append_array(call,'y',hash,32);
    if (r>=0) r=sd_bus_call(a->system_bus,call,2000000,NULL,&reply);
    struct ae_reservation_receipt receipt;
    if (r>=0) r=fetch_receipt(a,p.lease,&receipt);
    if (r==0 && cancel) {
        int64_t stamp=0,expires=0; r=reservation_stamp(&stamp,&expires);
        if (r==0) r=ae_catalogue_finish_withdrawn_operation(a->catalogue,p.user_operation,&receipt,stamp,&local);
    }
    if (r==0 && !cancel) r=ae_catalogue_get_user_launch(a->catalogue,p.user_operation,&local);
    sd_bus_message_unref(call); sd_bus_message_unref(reply); ae_peer_clear(&peer);
    if (r!=0) return reservation_error(error,r);
    return launch_reply(message,&local,&receipt);
}

#include "wire-tables.h"

static int storage_action(struct authority *authority, const char *control, int publish)
{
    struct ae_owner owner;
    int config = open_trusted(ROOT "/registry/allocation.conf", 0);
    if (config < 0) return config;
    struct stat status;
    uint8_t bytes[AE_OWNER_BYTES];
    int result = fstat(config, &status) == 0 && S_ISREG(status.st_mode) && status.st_uid == 0 &&
                 status.st_nlink == 1 && (status.st_mode & 07777) == 0444 && status.st_size == AE_OWNER_BYTES ?
                 labeled(config, "aetheri_registry_t") : -EACCES;
    if (result == 0 && pread(config, bytes, sizeof bytes, 0) != (ssize_t)sizeof bytes) result = -EBADMSG;
    close(config);
    if (result == 0 && (ae_owner_decode(bytes, sizeof bytes, &owner) != 0 ||
        memcmp(owner.id[AE_ACCOUNT], authority->identity.owner, AE_ID_BYTES) != 0)) result = -ESTALE;
    for (size_t n = 0; result == 0 && n < AE_OWNER_FIELDS; ++n) if (!uuid4(owner.id[n])) result = -EBADMSG;
    char account[33], storage[33], registry_path[320], anchor_path[256], root_path[320];
    sd_id128_t id;
    memcpy(id.bytes, authority->identity.owner, AE_ID_BYTES); sd_id128_to_string(id, account);
    if (result == 0) { memcpy(id.bytes, owner.id[AE_STORAGE], AE_ID_BYTES); sd_id128_to_string(id, storage); }
    if (result == 0) {
        int a = snprintf(registry_path, sizeof registry_path, "%s/allocations", control);
        int b = snprintf(anchor_path, sizeof anchor_path, ROOT "/users/%s/data", account);
        int c = snprintf(root_path, sizeof root_path, "%s/%s", anchor_path, storage);
        if (a < 0 || (size_t)a >= sizeof registry_path || b < 0 || (size_t)b >= sizeof anchor_path || c < 0 || (size_t)c >= sizeof root_path) result = -EOVERFLOW;
    }
    int registry = -1, anchor = -1, root = -1, header = -1, payload = -1, document = -1;
    if (result == 0) { registry = open_trusted(registry_path, 1); result = registry < 0 ? registry : labeled(registry, "aetheri_allocation_t"); }
    if (result == 0) { anchor = open_trusted(anchor_path, 1); result = anchor < 0 ? anchor : labeled(anchor, "aetheri_user_state_t"); }
    if (result == 0) { root = open_trusted(root_path, 1); result = root < 0 ? root : labeled(root, "aetheri_user_state_t"); }
    if (result == 0) { header = ae_open_small_file(root, "owner", 5, AE_OWNER_BYTES); result = header < 0 ? header : labeled(header, "aetheri_user_state_t"); }
    if (result == 0) {
        payload = openat(root, "payload", O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
        result = payload < 0 ? -errno : labeled(payload, "aetheri_app_data_t");
    }
    if (result == 0 && publish) result = ae_allocation_publish(registry, anchor, root, &owner);
    struct ae_storage_ref *reference = NULL;
    if (result == 0) result = ae_allocation_open(registry, anchor, &owner, &reference);
    if (result == 0) {
        document = ae_storage_ref_read(reference, "document", 8, 65536);
        result = document < 0 ? document : 0;
    }
    char text[7];
    if (result == 0 && (pread(document, text, sizeof text, 0) != 6 || memcmp(text, "hello\n", 6) != 0)) result = -EBADMSG;
    /* Independently fixed synthetic fixture digest, not a client-supplied hash.
     * Exercise libcrypto under the real cap-zero user-domain confinement. */
    static const struct ae_fixture_file expected_document = {
        .size = 6,
        .sha256 = {0x58,0x91,0xb5,0xb5,0x22,0xd5,0xdf,0x08,0x6d,0x0f,0xf0,0xb1,0x10,0xfb,0xd9,0xd2,
                   0x1b,0xb4,0xfc,0x71,0x63,0xaf,0x34,0xd0,0x82,0x86,0xa2,0xe8,0x46,0xf6,0xbe,0x03}
    };
    if (result == 0) result = ae_fixture_verify_file(&expected_document, document);
    if (document >= 0) close(document);
    ae_storage_ref_close(&reference);
    if (payload >= 0) close(payload);
    if (header >= 0) close(header);
    if (root >= 0) close(root);
    if (anchor >= 0) close(anchor);
    if (registry >= 0) close(registry);
    if (result == 0) puts("allocation: protected record and current mounted object agree; internal fixture read and SHA-256 passed");
    return result;
}

static int app_data_config(const char *path,uint8_t *bytes,size_t size)
{
    int fd=open_trusted(path,0);
    if (fd<0) return fd;
    struct stat status;
    int result=fstat(fd,&status)==0 ? 0 : -errno;
    if (result==0 && (!S_ISREG(status.st_mode) || status.st_uid!=0 || status.st_nlink!=1 ||
        (status.st_mode&07777)!=0444 || status.st_size!=(off_t)size)) result=-EACCES;
    if (result==0) result=labeled(fd,"aetheri_registry_t");
    if (result==0 && pread(fd,bytes,size,0)!=(ssize_t)size) result=-EBADMSG;
    if (close(fd)!=0 && result==0) result=-errno;
    return result;
}

/* Fixed root-provisioned P0 data infrastructure. No caller-supplied path, ID or
 * dynamic allocation/adoption. Observation freezes an independently held object
 * witness before any allocation publication or catalogue enrollment. */
static int app_data_action(struct authority *authority,const char *control,int enroll)
{
    uint8_t bytes[AE_DATA_CONFIG_BYTES],expected[AE_ALLOCATION_BYTES],actual[AE_ALLOCATION_BYTES];
    struct ae_owner owner;
    int result=app_data_config(ROOT "/registry/app-data-owner.conf",bytes,AE_OWNER_BYTES);
    if (result==0 && ae_owner_decode(bytes,AE_OWNER_BYTES,&owner)!=0) result=-EBADMSG;
    if (result==0 && memcmp(owner.id[AE_ACCOUNT],authority->identity.owner,16)!=0) result=-ESTALE;
    for (size_t n=0;result==0 && n<AE_OWNER_FIELDS;++n) if (!uuid4(owner.id[n])) result=-EBADMSG;
    struct ae_data_enrollment request;
    if (result==0 && enroll) result=app_data_config(ROOT "/registry/app-data.conf",bytes,sizeof bytes);
    if (result==0 && enroll) result=ae_data_enrollment_decode(bytes,sizeof bytes,&request);
    if (result==0 && enroll && !ae_owner_matches(&owner,&request.owner)) result=-ESTALE;
    if (result==0 && enroll) result=ae_allocation_encode(&request.allocation,expected,sizeof expected);
    char account[33],storage[33],anchor_path[256],root_path[320],registry_path[320];
    sd_id128_t id;
    if (result==0) {
        memcpy(id.bytes,owner.id[AE_ACCOUNT],16); sd_id128_to_string(id,account);
        memcpy(id.bytes,owner.id[AE_STORAGE],16); sd_id128_to_string(id,storage);
        int a=snprintf(anchor_path,sizeof anchor_path,ROOT "/users/%s/app-data",account);
        int b=snprintf(root_path,sizeof root_path,"%s/%s",anchor_path,storage);
        int c=snprintf(registry_path,sizeof registry_path,"%s/app-allocations",control);
        if (a<0 || a>=(int)sizeof anchor_path || b<0 || b>=(int)sizeof root_path || c<0 || c>=(int)sizeof registry_path) result=-EOVERFLOW;
    }
    int anchor=-1,root=-1,registry=-1,header=-1,payload=-1;
    if (result==0) { anchor=open_trusted(anchor_path,1); result=anchor<0 ? anchor : labeled(anchor,"aetheri_user_state_t"); }
    if (result==0) { root=open_trusted(root_path,1); result=root<0 ? root : labeled(root,"aetheri_user_state_t"); }
    if (result==0) { registry=open_trusted(registry_path,1); result=registry<0 ? registry : labeled(registry,"aetheri_allocation_t"); }
    if (result==0) { header=ae_open_small_file(root,"owner",5,AE_OWNER_BYTES); result=header<0 ? header : labeled(header,"aetheri_user_state_t"); }
    /* Payload is a directory, so use a no-follow directory open rather than the
     * regular-file transport helper. No app content is read or inferred here. */
    if (result==0) {
        struct open_how how={.flags=O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC,
            .resolve=RESOLVE_BENEATH|RESOLVE_NO_SYMLINKS|RESOLVE_NO_MAGICLINKS|RESOLVE_NO_XDEV};
        do { payload=(int)syscall(SYS_openat2,root,"payload",&how,sizeof how); } while (payload<0 && errno==EINTR);
        result=payload<0 ? -errno : labeled(payload,"aetheri_app_data_t");
    }
    struct ae_root_identity payload_identity;
    if (result==0) result=ae_root_observe(payload,&payload_identity);
    if (result==0 && (payload_identity.uid!=authority->uid || (payload_identity.mode&07777)!=0700)) result=-EACCES;
    struct ae_allocation observation;
    if (result==0) result=ae_allocation_capture(anchor,root,&owner,&observation);
    if (result==0) result=ae_allocation_encode(&observation,actual,sizeof actual);
    if (result==0 && enroll && memcmp(actual,expected,sizeof actual)!=0) result=-ESTALE;
    int fds[]={header,payload,root,anchor,registry};
    for (size_t n=0;result==0 && n<sizeof fds/sizeof fds[0];++n) if (fsync(fds[n])!=0) result=-errno;
    if (result==0 && enroll) result=ae_allocation_publish(registry,anchor,root,&owner);
    struct ae_storage_ref *reference=NULL;
    if (result==0 && enroll) result=ae_allocation_open(registry,anchor,&owner,&reference);
    if (result==0 && enroll) result=ae_storage_ref_check(reference);
    int64_t revision;
    if (result==0 && enroll) result=ae_catalogue_enroll_data(authority->catalogue,&request,authority->boot.bytes,&revision);
    int closed=ae_storage_ref_close(&reference);
    if (result==0) result=closed;
    for (size_t n=0;n<sizeof fds/sizeof fds[0];++n) if (fds[n]>=0 && close(fds[n])!=0 && result==0) result=-errno;
    if (result==0 && !enroll) {
        fputs("data-witness=",stdout);
        for (size_t n=0;n<sizeof actual;++n) printf("%02x",(unsigned int)actual[n]);
        putchar('\n');
    }
    if (result==0 && enroll) puts("data: exact private allocation enrolled; no launch admitted");
    return result;
}

static int stop_signal(sd_event_source *source, const struct signalfd_siginfo *signal_info, void *userdata)
{
    (void)signal_info; (void)userdata;
    return sd_event_exit(sd_event_source_get_event(source), 0);
}

int main(int argc, char **argv)
{
    enum ae_fixture_selection selection=AE_FIXTURE_ORIGINAL;
    const char *option=argc==3 ? argv[2] : "";
    char normalized[64];
    if (strncmp(option,"--exec-fixture-",15)==0) {
        int length=snprintf(normalized,sizeof normalized,"--%s",option+7);
        if (length<0 || length>=(int)sizeof normalized) return 2;
        selection=AE_FIXTURE_EXEC; option=normalized;
    }

    int admit=argc==3 && strcmp(argv[1],"system")==0 && strcmp(option,"--admit-fixture")==0;
    if ((argc != 2 && argc != 3) || (strcmp(argv[1], "system") != 0 && strcmp(argv[1], "user") != 0) ||
        (argc == 3 && !admit && strcmp(option, "--init") != 0 &&
         !(strcmp(argv[1], "system") == 0 && (strcmp(option, "--recover-setup") == 0 || strcmp(option, "--enroll-account") == 0 || strcmp(option, "--fixture-observe") == 0 || strcmp(option, "--fixture-stage") == 0 || strcmp(option, "--fixture-store-observe") == 0 || strcmp(option, "--fixture-store-publish") == 0 || strcmp(option, "--fixture-activate") == 0)) &&
         !(strcmp(argv[1], "user") == 0 && (strcmp(option, "--storage-publish") == 0 || strcmp(option, "--storage-verify") == 0 || strcmp(option,"--data-observe")==0 || strcmp(option,"--data-enroll")==0)))) {
        fputs("Usage: aetheri-authority system|user [--init]; system --recover-setup|--enroll-account|--fixture-observe|--fixture-stage|--fixture-store-observe|--fixture-store-publish|--fixture-activate; exec fixture actions use --exec-fixture-*; user --storage-publish|--storage-verify|--data-observe|--data-enroll\n", stderr); return 2;
    }
    int initialize = argc == 3 && strcmp(option, "--init") == 0;
    struct authority authority = {.identity.kind = strcmp(argv[1], "system") == 0 ? AE_CATALOGUE_SYSTEM : AE_CATALOGUE_USER,
                                  .uid = geteuid(), .broker = AE_BROKER_INIT};
    int system = authority.identity.kind == AE_CATALOGUE_SYSTEM;
    if ((system && authority.uid != 0) || (!system && authority.uid == 0)) return 3;
    int result = enforced_domain(system ? "aetheri_system_t" : "aetheri_user_t");
    if (result == 0) result = restricted_process();
    char state_path[256];
    if (result == 0) result = load_registry(&authority, state_path);
    if (result == 0) result = sd_id128_get_boot(&authority.boot);
    if (result == 0) result = initialize ? ae_catalogue_create(state_path, &authority.identity, authority.boot.bytes, &authority.catalogue) :
                                        ae_catalogue_open(state_path, &authority.identity, &authority.catalogue);
    if (result == 0 && system) {
        struct authority account = {.uid = 1000, .identity.kind = AE_CATALOGUE_USER};
        int64_t revision;
        result = load_registry(&account, NULL);
        if (result == 0) result = argc == 3 && (initialize || strcmp(option, "--enroll-account") == 0) ? ae_catalogue_enroll_account(authority.catalogue, account.identity.owner, (uint32_t)account.uid) :
                                            ae_catalogue_check_account(authority.catalogue, account.identity.owner, (uint32_t)account.uid, &revision);
    }
    if (result == 0 && system && argc == 3 && strcmp(option,"--recover-setup")==0)
        result=ae_setup_recover_unbound(authority.catalogue,authority.boot.bytes);
    else if (result == 0 && system && argc == 3 && strcmp(option,"--fixture-activate")==0)
        result = ae_authority_fixture_activate(selection,authority.catalogue,authority.boot.bytes);
    else if (result == 0 && system && argc == 3 && strncmp(option, "--fixture-store-", 16) == 0)
        result = ae_authority_fixture_publication(selection,authority.catalogue, strcmp(option, "--fixture-store-observe") == 0);
    else if (result == 0 && system && argc == 3 && strncmp(option, "--fixture-", 10) == 0)
        result = ae_authority_fixture(selection,authority.catalogue, strcmp(option, "--fixture-observe") == 0, authority.boot.bytes);
    if (result == 0 && !system && argc == 3 && strncmp(option,"--data-",7)==0) result=app_data_action(&authority,state_path,strcmp(option,"--data-enroll")==0);
    else if (result == 0 && !system && argc == 3 && !initialize) result = storage_action(&authority, state_path, strcmp(option, "--storage-publish") == 0);
    if (argc == 3 && !admit) {
        int closed = ae_catalogue_close(&authority.catalogue);
        if (result == 0) result = closed;
        if (result != 0) fprintf(stderr, "Authority administrative action failed: %d\n", result);
        return result == 0 ? 0 : 4;
    }
    if (result == 0) result = sd_id128_randomize(&authority.runtime);
    if (result == 0) result = sd_id128_randomize(&authority.events);
    if (result == 0 && (!uuid4(authority.runtime.bytes) || !uuid4(authority.events.bytes) ||
                       memcmp(authority.runtime.bytes, authority.events.bytes, AE_ID_BYTES) == 0)) result = -EIO;
    if (result == 0) result = ae_catalogue_begin_runtime(authority.catalogue, authority.boot.bytes, authority.runtime.bytes);
    if (result==0 && admit) result=ae_authority_fixture_admit(AE_FIXTURE_ORIGINAL,authority.catalogue,authority.boot.bytes,authority.runtime.bytes);
    sd_bus *bus = NULL;
    sd_event *event = NULL;
    sigset_t signals;
    sigemptyset(&signals); sigaddset(&signals, SIGTERM); sigaddset(&signals, SIGINT);
    if (result == 0 && sigprocmask(SIG_BLOCK, &signals, NULL) != 0) result = -errno;
    if (result == 0) result = sd_event_default(&event);
    if (result >= 0) authority.event = event;
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
    authority.setup=(struct ae_setup_service){.catalogue=authority.catalogue,.bus=bus,.owner=&authority,.broker=setup_broker,.peer=AE_PEER_INIT};
    memcpy(authority.setup.epoch,authority.identity.authority_epoch,16); memcpy(authority.setup.runtime,authority.runtime.bytes,16); memcpy(authority.setup.boot,authority.boot.bytes,16);
    if (result >= 0 && !system) {
        result = sd_bus_new(&authority.system_bus);
        if (result >= 0) result = sd_bus_set_address(authority.system_bus, "unix:path=/run/dbus/system_bus_socket");
        if (result >= 0) result = sd_bus_set_bus_client(authority.system_bus, 1);
        if (result >= 0) result = sd_bus_set_method_call_timeout(authority.system_bus, 2000000);
        if (result >= 0) result = sd_bus_set_exit_on_disconnect(authority.system_bus, 1);
        if (result >= 0) result = sd_bus_start(authority.system_bus);
        if (result >= 0) result = sd_bus_attach_event(authority.system_bus, event, 0);
        if (result >= 0) result = sd_bus_add_match(authority.system_bus, NULL,
            "type='signal',sender='org.freedesktop.DBus',path='/org/freedesktop/DBus',interface='org.freedesktop.DBus',member='NameOwnerChanged',arg0='org.aetheri.System1'", owner_changed, &authority);
        if (result >= 0) result = register_system(&authority);
    }
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
    ae_setup_service_clear(&authority.setup);
    ae_broker_clear(&authority.broker);
    sd_bus_flush_close_unref(authority.system_bus);
    sd_bus_flush_close_unref(bus);
    sd_event_unref(event);
    int closed = ae_catalogue_close(&authority.catalogue);
    return result < 0 || closed != 0 ? 5 : 0;
}
