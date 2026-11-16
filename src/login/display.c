#define _GNU_SOURCE
#include "login/display.h"
#include "login/display_channel.h"
#include "linux/image_internal.h"
#include <errno.h>
#include <fcntl.h>
#include <linux/magic.h>
#include <poll.h>
#include <selinux/selinux.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/pidfd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <unistd.h>

struct ae_login_display {
    struct ae_image image;
    struct stat original;
    int control, child, error;
    pid_t parent, pid;
    uid_t uid;
    gid_t gid;
    char process_label[512], socket_label[512];
    bool admitted;
};

static int owner(const struct ae_login_display *display)
{
    if (!display) return -EINVAL;
    if (getpid() != display->parent) return -ECHILD;
    return gettid() == display->parent ? 0 : -EXDEV;
}

static int fence(struct ae_login_display *display, int error)
{
    if (display->control >= 0) close(display->control);
    display->control = -1;
    display->error = error == -EAGAIN ? -ESTALE : error;
    return display->error;
}

static int identity(struct ae_login_display *display, pid_t *pid)
{
    struct pollfd watch = { .fd = display->child, .events = POLLIN };
    int result = poll(&watch, 1, 0);
    if (result != 0) return result < 0 ? -errno : -ESRCH;
    uint64_t required = PIDFD_INFO_PID | PIDFD_INFO_CREDS;
    struct pidfd_info info = { .mask = required };
    if (ioctl(display->child, PIDFD_GET_INFO, &info)) return -errno;
    if ((info.mask & required) != required || info.pid == 0 || info.pid > INT32_MAX ||
        info.tgid != info.pid || info.ppid != (uint32_t)display->parent ||
        info.ruid != display->uid || info.euid != display->uid ||
        info.suid != display->uid || info.fsuid != display->uid ||
        info.rgid != display->gid || info.egid != display->gid ||
        info.sgid != display->gid || info.fsgid != display->gid)
        return -EACCES;
    *pid = (pid_t)info.pid;
    return 0;
}

static int current(struct ae_login_display *display, bool checked_image)
{
    if (security_getenforce() != 1) return -EACCES;
    pid_t pid;
    int result = identity(display, &pid);
    if (!result && pid != display->pid) result = -ESTALE;
    if (!result && checked_image) {
        struct ae_image actual;
        const struct ae_image_process process = { .pidfd = display->child,
            .pid = pid, .uid = display->uid, .gid = display->gid, .label = display->process_label };
        result = ae_image_match_pinned(&process, &display->image, &actual);
        if (!result) result = identity(display, &pid);
        if (!result && pid != display->pid) result = -ESTALE;
    }
    return result;
}

int ae_login_display_create(int *control, int child, const struct ae_login_display_expect *expected,
    struct ae_login_display **out)
{
    if (!control || *control < 0 || child < 0 || !expected || !out || *out ||
        !expected->process_label || !*expected->process_label || !expected->socket_label ||
        !*expected->socket_label || strnlen(expected->process_label, 512) == 512 ||
        strnlen(expected->socket_label, 512) == 512)
        return -EINVAL;
    if (getpid() != gettid() || !getuid() || getuid() != geteuid() || !getgid() || getgid() != getegid() ||
        security_getenforce() != 1) return -EACCES;
    int result = ae_login_display_channel_check(*control, SOCK_SEQPACKET);
    if (result) return result;
    struct ae_login_display *display = calloc(1, sizeof *display);
    if (!display) return -ENOMEM;
    display->control = -1;
    display->parent = getpid();
    display->uid = getuid();
    display->gid = getgid();
    display->image = expected->image;
    strcpy(display->process_label, expected->process_label);
    strcpy(display->socket_label, expected->socket_label);
    display->child = fcntl(child, F_DUPFD_CLOEXEC, 3);
    struct statfs fs;
    if (display->child < 0 || fstat(display->child, &display->original) || fstatfs(display->child, &fs))
        result = -errno;
    if (!result && (unsigned long)fs.f_type != (unsigned long)PID_FS_MAGIC) result = -EPROTO;
    if (!result) result = identity(display, &display->pid);
    if (!result) {
        display->control = fcntl(*control, F_DUPFD_CLOEXEC, 3);
        if (display->control < 0) result = -errno;
    }
    if (result) {
        ae_login_display_close(&display);
        return result;
    }
    close(*control);
    *control = -1;
    *out = display;
    return 0;
}

static int match_peer(struct ae_login_display *display, int fd)
{
    int result = ae_login_display_channel_check(fd, SOCK_STREAM);
    if (result) return result;
    struct ucred peer;
    socklen_t size = sizeof peer;
    if (getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &peer, &size)) return -errno;
    if (size != sizeof peer || peer.pid != display->pid || peer.uid != display->uid ||
        peer.gid != display->gid) return -ESTALE;
    int pin = -1;
    size = sizeof pin;
    if (getsockopt(fd, SOL_SOCKET, SO_PEERPIDFD, &pin, &size)) return -errno;
    struct stat object;
    if (size != sizeof pin || pin < 0) result = -EPROTO;
    else if (fstat(pin, &object)) result = -errno;
    else if (object.st_dev != display->original.st_dev || object.st_ino != display->original.st_ino)
        result = -ESTALE;
    if (pin >= 0) close(pin);
    if (result) return result;
    char label[512];
    size = sizeof label;
    if (getsockopt(fd, SOL_SOCKET, SO_PEERSEC, label, &size)) return -errno;
    if (!size || size > sizeof label) return -EPROTO;
    if (label[size - 1] == '\0') --size;
    return size == strlen(display->socket_label) &&
        !memcmp(label, display->socket_label, size) ? 0 : -ESTALE;
}

int ae_login_display_take_original(struct ae_login_display *display, int *wayland)
{
    if (!wayland || *wayland != -1) return -EINVAL;
    int result = owner(display);
    if (result) return result;
    if (display->error) return display->error;
    if (display->admitted) return -EALREADY;
    result = current(display, false);
    if (result) return fence(display, result);
    int fd = -1;
    result = ae_login_display_take(display->control, &fd);
    if (result == -EAGAIN) return result;
    if (!result) result = match_peer(display, fd);
    if (!result) result = current(display, true);
    if (result) {
        if (fd >= 0) close(fd);
        return fence(display, result);
    }
    close(display->control);
    display->control = -1;
    display->admitted = true;
    *wayland = fd;
    return 0;
}

int ae_login_display_check(void *data)
{
    struct ae_login_display *display = data;
    int result = owner(display);
    if (result) return result;
    if (display->error) return display->error;
    if (!display->admitted) return -EAGAIN;
    result = current(display, true);
    return result ? fence(display, result) : 0;
}

int ae_login_display_watch(struct ae_login_display *display, int *control, int *child)
{
    if (!control || !child) return -EINVAL;
    int result = owner(display);
    if (result) return result;
    if (display->error) return display->error;
    *control = display->control;
    *child = display->child;
    return 0;
}

int ae_login_display_close(struct ae_login_display **value)
{
    if (!value) return -EINVAL;
    struct ae_login_display *display = *value;
    if (!display) return 0;
    int result = owner(display);
    if (result && result != -ECHILD) return result;
    if (display->control >= 0) close(display->control);
    if (display->child >= 0) close(display->child);
    free(display);
    *value = NULL;
    return 0;
}
