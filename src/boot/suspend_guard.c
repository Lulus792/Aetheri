#define _GNU_SOURCE
#include "suspend_link.h"

#include <errno.h>
#include <poll.h>
#include <selinux/selinux.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/signalfd.h>
#include <systemd/sd-daemon.h>
#include <systemd/sd-login.h>
#include <time.h>
#include <unistd.h>

enum guard_exit {
    GUARD_ROLE = 10,
    GUARD_SIGNALS,
    GUARD_SEAT,
    GUARD_CONNECT,
    GUARD_PEER,
    GUARD_SESSION,
    GUARD_SESSION_PROPERTIES,
    GUARD_HELLO,
    GUARD_PREPARE,
    GUARD_COVER,
    GUARD_NOTIFY,
    GUARD_HOLD,
};

static int fail(enum guard_exit phase, int error) {
    fprintf(stderr, "AETHERI_SUSPEND_GUARD_FAIL phase=%d error=%d\n", (int)phase, error);
    return phase;
}

static int active_peer(const struct ae_suspend_peer *peer, const char *expected_session) {
    char *peer_session = NULL, *active = NULL, *type = NULL, *class = NULL;
    uid_t uid = 0;
    int result = 0;
    if (!ae_suspend_peer_live(peer)) {
        result = fail(GUARD_PEER, -ESTALE);
    } else {
        int error = sd_pidfd_get_session(peer->pin, &peer_session);
        if (error >= 0) error = sd_seat_get_active("seat0", &active, &uid);
        if (error < 0) result = fail(GUARD_SESSION, error);
        else if (uid != peer->uid || !active || !peer_session || strcmp(active, expected_session) ||
                strcmp(peer_session, expected_session)) result = fail(GUARD_SESSION, -ESTALE);
        else {
            error = sd_session_is_remote(active);
            if (error > 0) error = -EACCES;
            if (!error) error = sd_session_get_type(active, &type);
            if (error >= 0) error = sd_session_get_class(active, &class);
            if (error < 0) result = fail(GUARD_SESSION_PROPERTIES, error);
            else if (!type || !class || strcmp(type, "wayland") || strcmp(class, "user"))
                result = fail(GUARD_SESSION_PROPERTIES, -EACCES);
            else if (!ae_suspend_peer_live(peer)) result = fail(GUARD_PEER, -ESTALE);
        }
    }
    free(class);
    free(type);
    free(active);
    free(peer_session);
    return result;
}

static int64_t now_ms(void) {
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) || now.tv_sec < 0 || now.tv_sec > INT64_MAX / 1000 - 8) return -1;
    return (int64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000;
}

static int wait_for(const struct ae_suspend_peer *peer, int signals, int64_t deadline, enum ae_suspend_message message) {
    struct pollfd wait[] = {
        {.fd = peer->fd, .events = POLLIN | POLLRDHUP}, {.fd = peer->pin, .events = POLLIN},
        {.fd = signals, .events = POLLIN},
    };
    for (;;) {
        int64_t now = now_ms();
        if (now < 0 || now >= deadline) return -ETIMEDOUT;
        int n = poll(wait, 3, (int)(deadline - now));
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return n ? -errno : -ETIMEDOUT;
        if (wait[2].revents) return -ECANCELED;
        if (wait[1].revents || wait[0].revents & (POLLERR | POLLHUP | POLLRDHUP | POLLNVAL)) return -ESTALE;
        int result = ae_suspend_receive(peer, message);
        if (result != -EAGAIN) return result;
    }
}

static int hold(const struct ae_suspend_peer *peer, int signals) {
    struct pollfd wait[] = {
        {.fd = peer->fd, .events = POLLIN | POLLRDHUP}, {.fd = peer->pin, .events = POLLIN},
        {.fd = signals, .events = POLLIN},
    };
    int n;
    do { n = poll(wait, 3, -1); } while (n < 0 && errno == EINTR);
    if (n < 0) return -errno;
    if (!(wait[2].revents & POLLIN)) return -ESTALE;
    struct signalfd_siginfo info;
    if (read(signals, &info, sizeof info) != (ssize_t)sizeof info || info.ssi_signo != SIGTERM) return -EIO;
    return 0;
}

static int guard(int signals) {
    char *session = NULL;
    uid_t uid = 0;
    int error = sd_seat_get_active("seat0", &session, &uid);
    if (error < 0 || !uid || !session) {
        free(session);
        return fail(GUARD_SEAT, error < 0 ? error : -EACCES);
    }
    struct ae_suspend_peer peer = AE_SUSPEND_PEER_INIT;
    int fd = ae_suspend_connect();
    int result = 0;
    if (fd < 0) result = fail(GUARD_CONNECT, fd);
    else if ((error = ae_suspend_peer_open(&peer, fd, uid, AE_SUSPEND_DESKTOP_LABEL))) {
        close(fd);
        result = fail(GUARD_PEER, error);
    }
    int64_t now = now_ms();
    if (!result && now < 0) result = fail(GUARD_HELLO, -EIO);
    if (!result) result = active_peer(&peer, session);
    if (!result && (error = wait_for(&peer, signals, now + 7000, AE_SUSPEND_HELLO))) result = fail(GUARD_HELLO, error);
    if (!result && (error = ae_suspend_send(&peer, AE_SUSPEND_PREPARE))) result = fail(GUARD_PREPARE, error);
    if (!result && (error = wait_for(&peer, signals, now + 7000, AE_SUSPEND_READY))) result = fail(GUARD_COVER, error);
    if (!result) result = active_peer(&peer, session);
    if (!result && (error = sd_notify(1, "READY=1\nSTATUS=Aetheri session covered")) <= 0)
        result = fail(GUARD_NOTIFY, error < 0 ? error : -ENOTCONN);
    if (!result) {
        fputs("AETHERI_SUSPEND_GUARD_READY\n", stderr);
        if ((error = hold(&peer, signals))) result = fail(GUARD_HOLD, error);
    }
    ae_suspend_peer_close(&peer);
    free(session);
    return result;
}

int main(void) {
    char *label = NULL;
    bool admitted = !getuid() && !geteuid() && !getgid() && !getegid() && security_getenforce() == 1 &&
        !getcon_raw(&label) && label && !strcmp(label, AE_SUSPEND_GUARD_LABEL);
    freecon(label);
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGTERM);
    if (!admitted) return fail(GUARD_ROLE, -EACCES);
    if (prctl(PR_SET_DUMPABLE, 0L, 0L, 0L, 0L) || sigprocmask(SIG_BLOCK, &mask, NULL)) return fail(GUARD_SIGNALS, -errno);
    int signals = signalfd(-1, &mask, SFD_CLOEXEC | SFD_NONBLOCK);
    if (signals < 0) return fail(GUARD_SIGNALS, -errno);
    int result = guard(signals);
    close(signals);
    if (!result) fputs("AETHERI_SUSPEND_GUARD_END\n", stderr);
    return result;
}
