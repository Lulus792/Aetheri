#include "linux/process.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/magic.h>
#include <linux/openat2.h>
#include <selinux/selinux.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/pidfd.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/vfs.h>
#include <systemd/sd-login.h>
#include <unistd.h>

static int copy_text(char *output, size_t capacity, const char *input)
{
    if (input == NULL) return -EBADMSG;
    size_t size = strnlen(input, capacity);
    if (size == 0 || size == capacity) return -E2BIG;
    memcpy(output, input, size + 1U);
    return 0;
}

int ae_process_context_equal(const struct ae_process_context *a, const struct ae_process_context *b)
{
    return a != NULL && b != NULL && a->uid == b->uid && a->cgroup_id == b->cgroup_id &&
           memcmp(a->boot, b->boot, sizeof a->boot) == 0 &&
           memcmp(a->invocation, b->invocation, sizeof a->invocation) == 0 &&
           strncmp(a->unit, b->unit, sizeof a->unit) == 0 &&
           strncmp(a->cgroup, b->cgroup, sizeof a->cgroup) == 0 &&
           strncmp(a->label, b->label, sizeof a->label) == 0;
}

static int identity(const struct ae_peer *peer, struct pidfd_info *info)
{
    *info = (struct pidfd_info){.mask = PIDFD_INFO_PID | PIDFD_INFO_CREDS | PIDFD_INFO_CGROUPID};
    if (ioctl(peer->pidfd, PIDFD_GET_INFO, info) != 0) return -errno;
    uint64_t required = PIDFD_INFO_PID | PIDFD_INFO_CREDS | PIDFD_INFO_CGROUPID;
    if ((info->mask & required) != required) return -ENODATA;
    if (info->pid != (uint32_t)peer->connection_pid || info->tgid != info->pid || info->cgroupid == 0 ||
        info->ruid != peer->connection_uid || info->euid != peer->connection_uid ||
        info->suid != peer->connection_uid || info->fsuid != peer->connection_uid) return -EACCES;
    return ae_peer_alive(peer);
}

static int manager_context(const struct ae_peer *peer, sd_bus *bus, struct ae_process_context *context, int registering)
{
    sd_bus_message *reply = NULL;
    int result = sd_bus_call_method(bus, "org.freedesktop.systemd1", "/org/freedesktop/systemd1",
                                   "org.freedesktop.systemd1.Manager", "GetUnitByPIDFD", NULL, &reply, "h", peer->pidfd);
    const char *path = NULL, *unit = NULL;
    const void *invocation = NULL;
    size_t size = 0;
    if (result >= 0 && !sd_bus_message_has_signature(reply, "osay")) result = -EBADMSG;
    if (result >= 0) result = sd_bus_message_read(reply, "os", &path, &unit);
    if (result > 0) result = copy_text(context->unit, sizeof context->unit, unit);
    if (result == 0) result = sd_bus_message_read_array(reply, 'y', &invocation, &size);
    if (result > 0) {
        if (size != sizeof context->invocation) result = -EBADMSG;
        else {
            uint8_t nonzero = 0;
            const uint8_t *bytes = invocation;
            for (size_t n = 0; n < size; ++n) nonzero |= bytes[n];
            if (nonzero == 0) result = -ENODATA;
            else memcpy(context->invocation, invocation, size);
        }
    }
    sd_bus_message *properties = NULL;
    if (result > 0) result = sd_bus_call_method(bus, "org.freedesktop.systemd1", path,
                     "org.freedesktop.DBus.Properties", "GetAll", NULL, &properties, "s", "org.freedesktop.systemd1.Unit");
    if (result >= 0 && !sd_bus_message_has_signature(properties, "a{sv}")) result = -EBADMSG;
    if (result >= 0) result = sd_bus_message_enter_container(properties, 'a', "{sv}");
    unsigned int found = 0;
    while (result > 0 && (result = sd_bus_message_enter_container(properties, 'e', "sv")) > 0) {
        const char *key = NULL, *text = NULL;
        result = sd_bus_message_read(properties, "s", &key);
        if (result <= 0) break;
        if (strcmp(key, "ActiveState") == 0 || strcmp(key, "Id") == 0) {
            result = sd_bus_message_enter_container(properties, 'v', "s");
            if (result > 0) result = sd_bus_message_read(properties, "s", &text);
            if (result > 0) {
                unsigned int bit = strcmp(key, "ActiveState") == 0 ? 1U : 2U;
                if (found & bit) result = -EBADMSG;
                else if (bit == 1U && strcmp(text, "active") != 0 && (!registering || strcmp(text, "activating") != 0)) result = -EAGAIN;
                else if (bit == 2U && strcmp(text, context->unit) != 0) result = -ESTALE;
                if (result >= 0) { found |= bit; result = sd_bus_message_exit_container(properties); }
            }
        } else if (strcmp(key, "InvocationID") == 0) {
            if (found & 4U) { result = -EBADMSG; break; }
            result = sd_bus_message_enter_container(properties, 'v', "ay");
            if (result > 0) result = sd_bus_message_read_array(properties, 'y', &invocation, &size);
            if (result > 0 && (size != sizeof context->invocation || memcmp(context->invocation, invocation, size) != 0)) result = -ESTALE;
            if (result > 0) { found |= 4U; result = sd_bus_message_exit_container(properties); }
        } else result = sd_bus_message_skip(properties, "v");
        if (result >= 0) result = sd_bus_message_exit_container(properties);
    }
    if (result == 0 && found != 7U) result = -ENODATA;
    if (result >= 0 && sd_bus_message_exit_container(properties) <= 0) result = -EBADMSG;
    char *cgroup = NULL;
    size_t length = strlen(context->unit);
    if (result >= 0 && (length < 8 || strcmp(context->unit + length - 8, ".service") != 0)) result = -EPROTONOSUPPORT;
    if (result >= 0) result = sd_bus_get_property_string(bus, "org.freedesktop.systemd1", path,
                            "org.freedesktop.systemd1.Service", "ControlGroup", NULL, &cgroup);
    if (result >= 0) result = copy_text(context->cgroup, sizeof context->cgroup, cgroup);
    free(cgroup);
    sd_bus_message_unref(properties);
    sd_bus_message_unref(reply);
    return result < 0 ? result : 0;
}

static int cgroup_identity(const char *path, uint64_t expected)
{
    if (path[0] != '/' || path[1] == '\0') return -EBADMSG;
    int root = open("/sys/fs/cgroup", O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
    if (root < 0) return -errno;
    struct statfs fs;
    int result = fstatfs(root, &fs) == 0 && fs.f_type == CGROUP2_SUPER_MAGIC ? 0 : -EACCES;
    struct open_how how = {.flags = O_RDONLY | O_DIRECTORY | O_CLOEXEC,
                          .resolve = RESOLVE_BENEATH | RESOLVE_NO_SYMLINKS | RESOLVE_NO_MAGICLINKS | RESOLVE_NO_XDEV};
    int fd = -1;
    if (result == 0) {
        do { fd = (int)syscall(SYS_openat2, root, path + 1, &how, sizeof how); } while (fd < 0 && errno == EINTR);
        if (fd < 0) result = -errno;
    }
    struct stat status;
    if (result == 0 && (fstat(fd, &status) != 0 || (uint64_t)status.st_ino != expected)) result = -ESTALE;
    if (fd >= 0) close(fd);
    close(root);
    return result;
}

static int observe_once(const struct ae_peer *peer, sd_bus *bus, struct ae_process_context *context, int registering)
{
    struct pidfd_info info;
    int result = identity(peer, &info);
    char *label = NULL, *cgroup = NULL;
    if (result == 0 && getpidcon_raw(peer->connection_pid, &label) != 0) result = -errno;
    if (result == 0) result = copy_text(context->label, sizeof context->label, label);
    if (result == 0 && strcmp(context->label, peer->connection_label) != 0) result = -ESTALE;
    if (result == 0) result = manager_context(peer, bus, context, registering);
    if (result == 0) result = sd_pidfd_get_cgroup(peer->pidfd, &cgroup);
    if (result == 0 && strcmp(cgroup, context->cgroup) != 0) result = -ESTALE;
    if (result == 0) result = cgroup_identity(cgroup, info.cgroupid);
    if (result == 0) {
        context->uid = info.euid;
        context->cgroup_id = info.cgroupid;
        sd_id128_t boot;
        result = sd_id128_get_boot(&boot);
        if (result == 0) memcpy(context->boot, boot.bytes, sizeof context->boot);
    }
    freecon(label);
    free(cgroup);
    if (result == 0) result = ae_peer_alive(peer);
    return result;
}

static int observe(const struct ae_peer *peer, sd_bus *system_bus, struct ae_process_context *output, int registering)
{
    if (peer == NULL || system_bus == NULL || output == NULL || peer->pidfd < 0 || peer->connection_pid <= 0 ||
        memchr(peer->connection_label, 0, sizeof peer->connection_label) == NULL || peer->connection_label[0] == 0) return -EINVAL;
    struct ae_process_context first = {0}, second = {0};
    int result = observe_once(peer, system_bus, &first, registering);
    if (result == 0) result = observe_once(peer, system_bus, &second, registering);
    if (result == 0 && !ae_process_context_equal(&first, &second)) result = -ESTALE;
    if (result == 0) *output = second;
    return result;
}

int ae_process_observe(const struct ae_peer *peer, sd_bus *system_bus, struct ae_process_context *output)
{
    return observe(peer, system_bus, output, 0);
}

int ae_process_observe_registration(const struct ae_peer *peer, sd_bus *system_bus, struct ae_process_context *output)
{
    return observe(peer, system_bus, output, 1);
}

int ae_process_verify(const struct ae_peer *peer, sd_bus *system_bus, const struct ae_process_context *expected)
{
    if (expected == NULL) return -EINVAL;
    struct ae_process_context actual;
    int result = ae_process_observe(peer, system_bus, &actual);
    if (result == 0 && !ae_process_context_equal(expected, &actual)) result = -ESTALE;
    return result;
}

int ae_process_observe_self(sd_bus *system_bus, struct ae_process_context *output)
{
    if (system_bus == NULL || output == NULL) return -EINVAL;
    struct ae_peer self = AE_PEER_INIT;
    self.connection_pid = getpid();
    self.connection_uid = geteuid();
    self.pidfd = pidfd_open(self.connection_pid, 0);
    if (self.pidfd < 0) return -errno;
    char *label = NULL;
    int result = getcon_raw(&label) == 0 ? 0 : -errno;
    if (result == 0) result = copy_text(self.connection_label, sizeof self.connection_label, label);
    freecon(label);
    if (result == 0) result = ae_process_observe(&self, system_bus, output);
    ae_peer_clear(&self);
    return result;
}
