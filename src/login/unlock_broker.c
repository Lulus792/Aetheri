#define _GNU_SOURCE
#include "unlock_broker.h"
#include "unlock_authority.h"
#include "unlock_channel.h"
#include "unlock_pam.h"
#include "unlock_view_contract.h"
#include "compositor/lock_wire.h"
#include "linux/pidfd_channel.h"
#include "linux/unnamed_socket.h"

#include <fcntl.h>
#include <linux/close_range.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/pidfd.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

enum phase { OPEN, COVERING, COVERED, RELEASING, FENCED };
enum { COVER_MS = 5000, ATTEMPT_MS = 15000 };

struct ae_unlock_broker {
    int control, credentials, compositor, view, worker_pin;
    struct stat compositor_id, view_id;
    struct ae_unlock_attempt *attempt;
    struct ae_unlock_authority_client *authority;
    char *worker, *account;
    bool (*session_live)(void *);
    void *session;
    enum phase phase;
    uint64_t generation, deadline;
    bool remote_pending;
};

static void close_slot(int *fd) {
    if (*fd >= 0) close(*fd);
    *fd = -1;
}

static bool live(int fd) {
    struct pollfd p = {.fd = fd, .events = POLLIN};
    return fd >= 0 && poll(&p, 1, 0) == 0;
}

static bool now_ms(uint64_t *out) {
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now)) return false;
    *out = (uint64_t)now.tv_sec * 1000 + (uint64_t)now.tv_nsec / 1000000;
    return true;
}

static bool current(const struct ae_unlock_broker *b) {
    return live(b->compositor) && live(b->view) && b->session_live(b->session);
}

static int pin_child(int pin, struct stat *identity) {
    siginfo_t info = {0};
    int copy = fcntl(pin, F_DUPFD_CLOEXEC, 3);
    if (copy < 0) return -errno;
    if (waitid(P_PIDFD, (id_t)copy, &info, WEXITED | WNOHANG | WNOWAIT) ||
            info.si_pid || !live(copy) || fstat(copy, identity)) {
        close(copy);
        return -ESTALE;
    }
    return copy;
}

static bool channel_owned(int fd) {
    struct ucred owner;
    socklen_t size = sizeof owner;
    return !ae_unnamed_socket_check(fd, SOCK_SEQPACKET) &&
        !getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &owner, &size) && size == sizeof owner &&
        owner.pid == getpid() && owner.uid == getuid() && owner.gid == getgid();
}

static bool arm_reader(int fd) {
    const int enabled = 1;
    return !fcntl(fd, F_SETFD, FD_CLOEXEC) &&
        !setsockopt(fd, SOL_SOCKET, SO_PASSPIDFD, &enabled, sizeof enabled);
}

static int cancel_worker(struct ae_unlock_broker *b) {
    ae_unlock_attempt_close(&b->attempt);
    if (live(b->worker_pin) && pidfd_send_signal(b->worker_pin, SIGKILL, NULL, 0) &&
            errno != ESRCH) return -errno;
    return 0;
}

static int reap_worker(struct ae_unlock_broker *b, int flags) {
    if (b->worker_pin < 0) return 0;
    siginfo_t info = {0};
    int status;
    do { status = waitid(P_PIDFD, (id_t)b->worker_pin, &info, WEXITED | flags); }
    while (status && errno == EINTR);
    if (status) return -errno;
    if (!info.si_pid) return -EAGAIN;
    close_slot(&b->worker_pin);
    return 0;
}

static int fence(struct ae_unlock_broker *b, int error) {
    b->phase = FENCED;
    close_slot(&b->control);
    close_slot(&b->credentials);
    (void)cancel_worker(b);
    ae_unlock_authority_client_close(&b->authority);
    return error;
}

void ae_unlock_broker_close(struct ae_unlock_broker **slot) {
    if (!slot || !*slot) return;
    struct ae_unlock_broker *b = *slot;
    *slot = NULL;
    close_slot(&b->control);
    close_slot(&b->credentials);
    ae_unlock_authority_client_close(&b->authority);
    if (!cancel_worker(b)) (void)reap_worker(b, 0);
    close_slot(&b->worker_pin);
    close_slot(&b->compositor);
    close_slot(&b->view);
    free(b->account);
    free(b->worker);
    free(b);
}

int ae_unlock_broker_open(struct ae_unlock_broker **out, const struct ae_unlock_broker_config *p) {
    if (!out || *out || !p || !p->session_live || (!p->authority && (!p->worker || p->worker[0] != '/')) ||
            !p->account || !*p->account || strnlen(p->account, 33) > 32 ||
            p->control < 3 || p->credentials < 3 || p->control == p->credentials ||
            !getuid() || getuid() != geteuid() || getgid() != getegid()) return -EINVAL;
    if (!channel_owned(p->control) || !channel_owned(p->credentials)) return -EPERM;
    struct ae_unlock_broker *b = calloc(1, sizeof *b);
    if (!b) return -ENOMEM;
    b->control = b->credentials = b->compositor = b->view = b->worker_pin = -1;
    b->compositor = pin_child(p->compositor, &b->compositor_id);
    b->view = pin_child(p->view, &b->view_id);
    b->session_live = p->session_live;
    b->session = p->session;
    int error = -ESTALE;
    if (!current(b) || (b->compositor_id.st_dev == b->view_id.st_dev &&
            b->compositor_id.st_ino == b->view_id.st_ino)) goto failed;
    error = -ENOMEM;
    if (!p->authority) b->worker = strdup(p->worker);
    b->account = strdup(p->account);
    if ((!p->authority && !b->worker) || !b->account) goto failed;
    error = -EPERM;
    if (prctl(PR_SET_DUMPABLE, 0L, 0L, 0L, 0L) ||
            !arm_reader(p->control) || !arm_reader(p->credentials)) goto failed;
    b->phase = OPEN;
    b->control = p->control;
    b->credentials = p->credentials;
    b->authority = p->authority;
    *out = b;
    return 0;

failed:
    ae_unlock_broker_close(&b);
    return error;
}

void ae_unlock_broker_poll(const struct ae_unlock_broker *b, struct pollfd fds[AE_UNLOCK_BROKER_FDS],
        int *timeout) {
    for (unsigned i = 0; i < AE_UNLOCK_BROKER_FDS; ++i) fds[i] = (struct pollfd){.fd = -1};
    *timeout = -1;
    if (!b) return;
    if (!b->attempt) fds[5] = (struct pollfd){.fd = b->worker_pin, .events = POLLIN};
    if (b->phase == FENCED) return;
    fds[0] = (struct pollfd){.fd = b->control, .events = POLLIN};
    fds[1] = (struct pollfd){.fd = b->credentials, .events = POLLIN};
    fds[2] = (struct pollfd){.fd = b->compositor, .events = POLLIN};
    fds[3] = (struct pollfd){.fd = b->view, .events = POLLIN};
    if (b->authority) ae_unlock_authority_client_poll(b->authority, &fds[4]);
    if (b->attempt && ae_unlock_attempt_poll(b->attempt, &fds[4])) {
        *timeout = 0;
        return;
    }
    if (b->deadline) {
        uint64_t now;
        *timeout = !now_ms(&now) || now >= b->deadline ? 0 : (int)(b->deadline - now);
    }
}

static int send_control(struct ae_unlock_broker *b, enum ae_lock_action action) {
    unsigned char frame[AE_LOCK_WIRE_SIZE];
    ae_lock_wire_write(frame, action, b->generation);
    return send(b->control, frame, sizeof frame, MSG_DONTWAIT | MSG_NOSIGNAL) ==
        (ssize_t)sizeof frame ? 0 : -EPIPE;
}

static int receive_control(struct ae_unlock_broker *b, uint64_t now) {
    unsigned char frame[AE_LOCK_WIRE_SIZE];
    ssize_t n = ae_pidfd_receive(b->control, frame, sizeof frame, &b->compositor_id);
    if (n == -EAGAIN) return 0;
    if (n != (ssize_t)sizeof frame || memcmp(frame, "AELK2", 5)) return -EPROTO;
    uint64_t generation = ae_lock_wire_generation(frame);
    if (frame[5] == AE_LOCK_REQUEST && b->phase == OPEN && b->generation < UINT64_MAX &&
            generation == b->generation + 1) {
        b->phase = COVERING;
        b->deadline = now + COVER_MS;
    } else if (frame[5] == AE_LOCK_COVERED && b->phase == COVERING && generation == b->generation + 1) {
        b->generation = generation;
        b->phase = COVERED;
        b->deadline = 0;
    } else if (frame[5] == AE_LOCK_RELEASED && b->phase == RELEASING && generation == b->generation) {
        b->phase = OPEN;
        b->deadline = 0;
    } else return -ESTALE;
    return 0;
}

static int feedback(struct ae_unlock_broker *b, unsigned char message) {
    unsigned char packet[AE_UNLOCK_VIEW_MAGIC_SIZE + 1] = AE_UNLOCK_VIEW_MAGIC;
    packet[AE_UNLOCK_VIEW_MAGIC_SIZE] = message;
    return send(b->credentials, packet, sizeof packet, MSG_DONTWAIT | MSG_NOSIGNAL) ==
        (ssize_t)sizeof packet ? 0 : -EPIPE;
}

static int verify(struct ae_unlock_broker *b, const void *password, size_t size) {
    int pair[2];
    int status = ae_unlock_worker_pair(pair);
    if (status) return status;
    char number[32];
    snprintf(number, sizeof number, "%d", pair[1]);
    char *command[] = {b->worker, number, b->account, NULL};
    char *environment[] = {"PATH=/usr/bin:/usr/sbin", "LANG=C", NULL};
    pid_t parent = getpid(), child = fork();
    if (!child) {
        close(pair[0]);
        /* A fresh attempt sends its password only after this child is pinned. */
        if (prctl(PR_SET_PDEATHSIG, SIGKILL, 0L, 0L, 0L) || getppid() != parent ||
                close_range(3, ~0u, CLOSE_RANGE_CLOEXEC) || fcntl(pair[1], F_SETFD, 0)) _exit(125);
        execve(b->worker, command, environment);
        _exit(126);
    }
    int fork_error = errno;
    close(pair[1]);
    if (child < 0) {
        status = -fork_error;
    } else {
        b->worker_pin = pidfd_open(child, 0);
        if (b->worker_pin < 0) {
            status = -errno;
            close(pair[0]); /* EOF before any request ends the original child. */
            while (waitpid(child, NULL, 0) < 0 && errno == EINTR) {}
            return status;
        }
        status = ae_unlock_attempt_begin(&b->attempt, pair[0], b->worker_pin,
            b->generation, password, size);
    }
    if (status) close(pair[0]);
    return status;
}

static int receive_password(struct ae_unlock_broker *b, uint64_t now) {
    unsigned char packet[AE_UNLOCK_VIEW_MAGIC_SIZE + AE_UNLOCK_PASSWORD_MAX];
    ssize_t count = ae_pidfd_receive(b->credentials, packet, sizeof packet, &b->view_id);
    int status = 0;
    if (count != -EAGAIN) {
        if (b->phase != COVERED || b->attempt || b->remote_pending || count <= AE_UNLOCK_VIEW_MAGIC_SIZE ||
                memcmp(packet, AE_UNLOCK_VIEW_MAGIC, AE_UNLOCK_VIEW_MAGIC_SIZE) ||
                memchr(packet + AE_UNLOCK_VIEW_MAGIC_SIZE, 0, (size_t)count - AE_UNLOCK_VIEW_MAGIC_SIZE)) {
            status = -EPROTO;
        } else if (b->authority) {
            status = ae_unlock_authority_client_begin(b->authority, b->generation,
                packet + AE_UNLOCK_VIEW_MAGIC_SIZE, (size_t)count - AE_UNLOCK_VIEW_MAGIC_SIZE);
            if (!status) {
                b->remote_pending = true;
                b->deadline = now + ATTEMPT_MS;
            }
        } else if (b->worker_pin >= 0 ||
                verify(b, packet + AE_UNLOCK_VIEW_MAGIC_SIZE, (size_t)count - AE_UNLOCK_VIEW_MAGIC_SIZE)) {
            status = cancel_worker(b);
            if (!status) status = feedback(b, AE_UNLOCK_VIEW_ERROR);
        } else {
            b->deadline = now + ATTEMPT_MS;
        }
    }
    explicit_bzero(packet, sizeof packet);
    return status;
}

static int publish(struct ae_unlock_broker *b, enum ae_unlock_result value) {
    b->deadline = 0;
    if (b->phase != COVERED || !current(b)) return -ESTALE;
    if (value == AE_UNLOCK_UNAVAILABLE) return feedback(b, AE_UNLOCK_VIEW_ERROR);
    if (value == AE_UNLOCK_DENIED) return feedback(b, AE_UNLOCK_VIEW_DENIED);
    if (send_control(b, AE_LOCK_RELEASE)) return -EPIPE;
    b->phase = RELEASING;
    /* Release can legitimately wait for a held key or mouse button. */
    return 0;
}

static int result(struct ae_unlock_broker *b, uint64_t now) {
    enum ae_unlock_result value = AE_UNLOCK_UNAVAILABLE;
    int status = now >= b->deadline ? -ETIMEDOUT :
        ae_unlock_attempt_receive(b->attempt, b->generation, &value);
    if (status == -EAGAIN) return 0;
    int cancelled = cancel_worker(b);
    int reaped = reap_worker(b, WNOHANG);
    if (cancelled || (reaped && (!status || reaped != -EAGAIN))) return -ESTALE;
    return publish(b, status ? AE_UNLOCK_UNAVAILABLE : value);
}

static int remote_result(struct ae_unlock_broker *b, uint64_t now) {
    if (b->remote_pending && now >= b->deadline) return -ETIMEDOUT;
    enum ae_unlock_result value = AE_UNLOCK_UNAVAILABLE;
    int status = ae_unlock_authority_client_receive(b->authority, b->generation, &value);
    if (status == -EAGAIN) return 0;
    if (status || !b->remote_pending) return -ESTALE;
    b->remote_pending = false;
    return publish(b, value);
}

int ae_unlock_broker_step(struct ae_unlock_broker *b) {
    if (!b) return -EINVAL;
    if (!b->attempt) {
        int status = reap_worker(b, WNOHANG);
        if (status && status != -EAGAIN) return fence(b, status);
    }
    if (b->phase == FENCED) return -ESTALE;
    uint64_t now;
    if (!current(b) || !now_ms(&now)) return fence(b, -ESTALE);
    if (b->phase == COVERING && now >= b->deadline) return fence(b, -ETIMEDOUT);
    int status = receive_control(b, now);
    if (!status) status = receive_password(b, now);
    if (!status && b->attempt) status = result(b, now);
    if (!status && b->authority) status = remote_result(b, now);
    return status ? fence(b, status) : 0;
}

bool ae_unlock_broker_open_desktop(const struct ae_unlock_broker *b) {
    return b && b->phase == OPEN;
}
