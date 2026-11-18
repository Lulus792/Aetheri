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

static bool active_peer(const struct ae_suspend_peer *peer, const char *expected_session) {
    char *peer_session = NULL, *active = NULL, *type = NULL, *class = NULL;
    uid_t uid = 0;
    bool admitted = ae_suspend_peer_live(peer) && sd_pidfd_get_session(peer->pin, &peer_session) >= 0 &&
        sd_seat_get_active("seat0", &active, &uid) >= 0 && uid == peer->uid && active && peer_session &&
        !strcmp(active, expected_session) && !strcmp(peer_session, expected_session) &&
        sd_session_is_remote(active) == 0 && sd_session_get_type(active, &type) >= 0 &&
        sd_session_get_class(active, &class) >= 0 && type && class && !strcmp(type, "wayland") && !strcmp(class, "user") &&
        ae_suspend_peer_live(peer);
    free(class);
    free(type);
    free(active);
    free(peer_session);
    return admitted;
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
    if (sd_seat_get_active("seat0", &session, &uid) < 0 || !uid || !session) { free(session); return -EACCES; }
    struct ae_suspend_peer peer = AE_SUSPEND_PEER_INIT;
    int fd = ae_suspend_connect();
    int result = fd < 0 ? fd : ae_suspend_peer_open(&peer, fd, uid, AE_SUSPEND_DESKTOP_LABEL);
    if (result && fd >= 0) close(fd);
    int64_t now = now_ms();
    if (!result && (now < 0 || !active_peer(&peer, session))) result = -ESTALE;
    if (!result) result = wait_for(&peer, signals, now + 7000, AE_SUSPEND_HELLO);
    if (!result) result = ae_suspend_send(&peer, AE_SUSPEND_PREPARE);
    if (!result) result = wait_for(&peer, signals, now + 7000, AE_SUSPEND_READY);
    if (!result && !active_peer(&peer, session)) result = -ESTALE;
    if (!result && sd_notify(1, "READY=1\nSTATUS=Aetheri session covered") <= 0) result = -EIO;
    if (!result) {
        fputs("AETHERI_SUSPEND_GUARD_READY\n", stderr);
        result = hold(&peer, signals);
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
    if (!admitted || prctl(PR_SET_DUMPABLE, 0L, 0L, 0L, 0L) || sigprocmask(SIG_BLOCK, &mask, NULL)) return 1;
    int signals = signalfd(-1, &mask, SFD_CLOEXEC | SFD_NONBLOCK);
    if (signals < 0) return 1;
    int result = guard(signals);
    close(signals);
    if (result) fprintf(stderr, "AETHERI_SUSPEND_GUARD_FAIL error=%d\n", result);
    else fputs("AETHERI_SUSPEND_GUARD_END\n", stderr);
    return result ? 1 : 0;
}
