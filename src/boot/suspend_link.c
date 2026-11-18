#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "suspend_link.h"
#include "linux/pidfd_channel.h"

#include <errno.h>
#include <poll.h>
#include <selinux/selinux.h>
#include <stddef.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/pidfd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

static const struct sockaddr_un endpoint = {.sun_family = AF_UNIX, .sun_path = "\0aetheri.suspend.seat0"};
static const socklen_t endpoint_size = offsetof(struct sockaddr_un, sun_path) + sizeof "\0aetheri.suspend.seat0" - 1;

int ae_suspend_listen(void) {
    int fd = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (fd < 0) return -errno;
    if (!bind(fd, (const struct sockaddr *)&endpoint, endpoint_size) && !listen(fd, 1)) return fd;
    int error = -errno;
    close(fd);
    return error;
}

int ae_suspend_connect(void) {
    int fd = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (fd < 0) return -errno;
    const int enabled = 1;
    if (!setsockopt(fd, SOL_SOCKET, SO_PASSPIDFD, &enabled, sizeof enabled) &&
            !connect(fd, (const struct sockaddr *)&endpoint, endpoint_size)) return fd;
    int error = -errno;
    close(fd);
    return error;
}

static bool credentials(const struct ae_suspend_peer *peer, uint64_t *cgroup) {
    const uint64_t required = PIDFD_INFO_PID | PIDFD_INFO_CREDS | PIDFD_INFO_CGROUPID;
    struct pidfd_info info = {.mask = required};
    if (ioctl(peer->pin, PIDFD_GET_INFO, &info) || (info.mask & required) != required ||
            info.pid != peer->pid || info.tgid != info.pid || !info.cgroupid ||
            info.ruid != peer->uid || info.euid != peer->uid || info.suid != peer->uid || info.fsuid != peer->uid ||
            info.rgid != peer->gid || info.egid != peer->gid || info.sgid != peer->gid || info.fsgid != peer->gid)
        return false;
    *cgroup = info.cgroupid;
    return true;
}

bool ae_suspend_peer_live(const struct ae_suspend_peer *peer) {
    if (peer->fd < 0 || peer->pin < 0) return false;
    struct pollfd ends[] = {{.fd = peer->fd, .events = POLLRDHUP}, {.fd = peer->pin, .events = POLLIN}};
    uint64_t cgroup;
    return poll(ends, 2, 0) == 0 && credentials(peer, &cgroup) && cgroup == peer->cgroup && poll(ends, 2, 0) == 0;
}

int ae_suspend_peer_open(struct ae_suspend_peer *out, int fd, uid_t uid, const char *expected_label) {
    if (out->fd >= 0 || fd < 0 || security_getenforce() != 1) return -EACCES;
    struct ucred cred;
    socklen_t size = sizeof cred;
    if (getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &cred, &size) || size != sizeof cred ||
            cred.pid <= 1 || cred.uid != uid || (!uid && cred.gid)) return -EACCES;
    char *label = NULL;
    if (getpeercon_raw(fd, &label)) return -EACCES;
    bool agrees = label && !strcmp(label, expected_label);
    freecon(label);
    if (!agrees) return -EACCES;

    struct ae_suspend_peer peer = {.fd = fd, .pin = -1, .uid = uid, .gid = cred.gid, .pid = (uint32_t)cred.pid};
    size = sizeof peer.pin;
    if (getsockopt(fd, SOL_SOCKET, SO_PEERPIDFD, &peer.pin, &size)) return -errno;
    const int enabled = 1;
    bool valid = size == sizeof peer.pin && !fstat(peer.pin, &peer.identity) &&
        credentials(&peer, &peer.cgroup) && ae_suspend_peer_live(&peer) &&
        !setsockopt(fd, SOL_SOCKET, SO_PASSPIDFD, &enabled, sizeof enabled);
    if (!valid) {
        if (peer.pin >= 0) close(peer.pin);
        return -ESTALE;
    }
    *out = peer;
    return 0;
}

void ae_suspend_peer_close(struct ae_suspend_peer *peer) {
    if (peer->fd >= 0) close(peer->fd);
    if (peer->pin >= 0) close(peer->pin);
    *peer = (struct ae_suspend_peer)AE_SUSPEND_PEER_INIT;
}

int ae_suspend_send(const struct ae_suspend_peer *peer, enum ae_suspend_message message) {
    unsigned char packet[6] = "AESS1";
    packet[5] = (unsigned char)message;
    if (!ae_suspend_peer_live(peer)) return -ESTALE;
    return send(peer->fd, packet, sizeof packet, MSG_DONTWAIT | MSG_NOSIGNAL) == (ssize_t)sizeof packet ? 0 : -EPIPE;
}

int ae_suspend_receive(const struct ae_suspend_peer *peer, enum ae_suspend_message expected) {
    unsigned char packet[6];
    if (!ae_suspend_peer_live(peer)) return -ESTALE;
    ssize_t got = ae_pidfd_receive(peer->fd, packet, sizeof packet, &peer->identity);
    if (got < 0) return (int)got;
    if (!got) return -EPIPE;
    if (got != (ssize_t)sizeof packet || memcmp(packet, "AESS1", 5) || packet[5] != (unsigned char)expected) return -EPROTO;
    return ae_suspend_peer_live(peer) ? 0 : -ESTALE;
}
