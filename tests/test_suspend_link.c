#define _GNU_SOURCE
#include "boot/suspend_link.h"

#include <errno.h>
#include <poll.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "suspend link check failed at line %d\n", __LINE__); exit(1); \
} } while (0)

/* The enclosing development carrier does not load the installed guard
   policy. Only its label/enforcement observation is substituted here. */
int __wrap_security_getenforce(void) { return 1; }
int __wrap_getpeercon_raw(int fd, char **label) {
    (void)fd;
    *label = strdup(AE_SUSPEND_DESKTOP_LABEL);
    return *label ? 0 : -1;
}

static void incoming(int fd) {
    struct pollfd ready = {.fd = fd, .events = POLLIN};
    CHECK(poll(&ready, 1, 3000) == 1 && (ready.revents & POLLIN));
}

int main(void) {
    struct sockaddr_un address = {.sun_family = AF_UNIX};
    int length = snprintf(address.sun_path + 1, sizeof address.sun_path - 1, "aetheri.suspend.test.%ld", (long)getpid());
    CHECK(length > 0 && (size_t)length < sizeof address.sun_path - 1);
    socklen_t size = (socklen_t)(offsetof(struct sockaddr_un, sun_path) + 1 + (size_t)length);
    int listener = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    CHECK(listener >= 0 && !bind(listener, (const struct sockaddr *)&address, size) && !listen(listener, 1));
    pid_t child = fork();
    CHECK(child >= 0);
    if (!child) {
        close(listener);
        int fd = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
        const int enabled = 1;
        CHECK(fd >= 0 && !setsockopt(fd, SOL_SOCKET, SO_PASSPIDFD, &enabled, sizeof enabled));
        CHECK(!connect(fd, (const struct sockaddr *)&address, size));
        struct ae_suspend_peer peer = AE_SUSPEND_PEER_INIT;
        CHECK(!ae_suspend_peer_open(&peer, fd, getuid(), AE_SUSPEND_DESKTOP_LABEL));
        incoming(fd);
        CHECK(!ae_suspend_receive(&peer, AE_SUSPEND_HELLO));
        CHECK(!ae_suspend_send(&peer, AE_SUSPEND_PREPARE));
        incoming(fd);
        CHECK(!ae_suspend_receive(&peer, AE_SUSPEND_READY));
        pid_t copy = fork();
        CHECK(copy >= 0);
        if (!copy) {
            CHECK(ae_suspend_send(&peer, AE_SUSPEND_PREPARE) == 0);
            _exit(0);
        }
        int status;
        CHECK(waitpid(copy, &status, 0) == copy && WIFEXITED(status) && !WEXITSTATUS(status));
        struct pollfd closed = {.fd = fd, .events = POLLRDHUP};
        CHECK(poll(&closed, 1, 3000) == 1 && (closed.revents & POLLRDHUP));
        CHECK(!ae_suspend_peer_live(&peer));
        ae_suspend_peer_close(&peer);
        _exit(0);
    }

    incoming(listener);
    int fd = accept4(listener, NULL, NULL, SOCK_CLOEXEC | SOCK_NONBLOCK);
    CHECK(fd >= 0);
    struct ae_suspend_peer peer = AE_SUSPEND_PEER_INIT;
    CHECK(ae_suspend_peer_open(&peer, fd, getuid() ^ 1u, AE_SUSPEND_DESKTOP_LABEL) == -EACCES && peer.fd == -1);
    CHECK(ae_suspend_peer_open(&peer, fd, getuid(), AE_SUSPEND_GUARD_LABEL) == -EACCES && peer.fd == -1);
    CHECK(!ae_suspend_peer_open(&peer, fd, getuid(), AE_SUSPEND_DESKTOP_LABEL));
    CHECK(peer.pid == (uint32_t)child && ae_suspend_peer_live(&peer));
    CHECK(!ae_suspend_send(&peer, AE_SUSPEND_HELLO));
    incoming(fd);
    CHECK(!ae_suspend_receive(&peer, AE_SUSPEND_PREPARE));
    CHECK(!ae_suspend_send(&peer, AE_SUSPEND_READY));
    incoming(fd);
    CHECK(ae_suspend_receive(&peer, AE_SUSPEND_PREPARE) == -EPROTO);
    ae_suspend_peer_close(&peer);
    close(listener);
    int status;
    CHECK(waitpid(child, &status, 0) == child && WIFEXITED(status) && !WEXITSTATUS(status));
    return 0;
}
