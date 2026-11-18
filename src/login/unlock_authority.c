#define _GNU_SOURCE
#include "unlock_authority.h"
#include "unlock_pam.h"
#include "linux/pidfd_channel.h"
#include "linux/unnamed_socket.h"

#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/pidfd.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static const unsigned char magic[] = "AEUA1";
enum { PREFIX = sizeof magic - 1, HEADER = PREFIX + 16, VERIFY_MS = 12000 };

struct association {
    uint64_t generation, ticket;
};
struct ae_unlock_authority {
    struct stat desktop;
    struct association request;
    uint64_t cgroup, deadline;
    uid_t uid;
    gid_t gid;
    char account[33];
    int fd, child, worker, result;
    bool expired;
};
struct ae_unlock_authority_client {
    struct stat owner;
    struct association request;
    int fd, parent;
    bool pending;
};

static void close_fd(int *fd) {
    if (*fd >= 0) close(*fd);
    *fd = -1;
}

static bool live(int fd) {
    struct pollfd p = {.fd = fd, .events = POLLIN};
    return fd >= 0 && poll(&p, 1, 0) == 0;
}

static uint64_t monotonic_ms(void) {
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) || now.tv_sec < 0) return 0;
    return (uint64_t)now.tv_sec * 1000 + (uint64_t)now.tv_nsec / 1000000;
}

static void encode64(unsigned char out[8], uint64_t number) {
    for (unsigned i = 0; i < 8; ++i) out[i] = (unsigned char)(number >> ((7 - i) * 8));
}
static uint64_t decode64(const unsigned char bytes[8]) {
    uint64_t value = 0;
    for (unsigned i = 0; i < 8; ++i) value = (value << 8) | bytes[i];
    return value;
}
static void encode(unsigned char header[HEADER], struct association a) {
    memcpy(header, magic, PREFIX);
    encode64(header + PREFIX, a.generation);
    encode64(header + PREFIX + 8, a.ticket);
}

static bool process_info(int fd, struct pidfd_info *out) {
    const uint64_t mask = PIDFD_INFO_PID | PIDFD_INFO_CREDS | PIDFD_INFO_CGROUPID;
    *out = (struct pidfd_info){.mask = mask};
    return !ioctl(fd, PIDFD_GET_INFO, out) && (out->mask & mask) == mask &&
        out->pid && out->pid == out->tgid && out->cgroupid && live(fd);
}

static bool current(const struct ae_unlock_authority *a) {
    struct pidfd_info info;
    return process_info(a->child, &info) && info.ppid == (uint32_t)getpid() &&
        info.cgroupid == a->cgroup && info.ruid == a->uid && info.euid == a->uid &&
        info.suid == a->uid && info.fsuid == a->uid && info.rgid == a->gid &&
        info.egid == a->gid && info.sgid == a->gid && info.fsgid == a->gid;
}

static bool creator(int fd, pid_t expected) {
    struct ucred cred;
    socklen_t size = sizeof cred;
    int enabled = 0;
    if (fd < 3 || ae_unnamed_socket_check(fd, SOCK_SEQPACKET) ||
            getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &cred, &size) ||
            size != sizeof cred || cred.pid != expected || cred.uid != 0) return false;
    size = sizeof enabled;
    return !getsockopt(fd, SOL_SOCKET, SO_PASSPIDFD, &enabled, &size) &&
        size == sizeof enabled && enabled == 1;
}

int ae_unlock_authority_pair(int pair[2]) {
    if (!pair || getuid() || geteuid()) return -EPERM;
    int descriptors[2];
    if (socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, descriptors)) return -errno;
    const int enabled = 1;
    if (descriptors[0] < 3 ||
            setsockopt(descriptors[0], SOL_SOCKET, SO_PASSPIDFD, &enabled, sizeof enabled) ||
            setsockopt(descriptors[1], SOL_SOCKET, SO_PASSPIDFD, &enabled, sizeof enabled)) {
        int error = descriptors[0] < 3 ? -EBADF : -errno;
        close(descriptors[0]); close(descriptors[1]);
        return error;
    }
    pair[0] = descriptors[0]; pair[1] = descriptors[1];
    return 0;
}

int ae_unlock_authority_open(struct ae_unlock_authority **out, int fd, int child,
        uid_t uid, gid_t gid, const char *account) {
    if (!out || *out || !uid || !gid || !account || !*account || strnlen(account, 33) > 32 ||
            !strcmp(account, "aetheri-greeter") || getuid() || geteuid() || !creator(fd, getpid()))
        return -EINVAL;
    struct ae_unlock_authority *a = calloc(1, sizeof *a);
    if (!a) return -ENOMEM;
    a->fd = a->worker = a->result = -1;
    a->child = fcntl(child, F_DUPFD_CLOEXEC, 3);
    a->uid = uid; a->gid = gid;
    memcpy(a->account, account, strlen(account) + 1);
    struct pidfd_info own, desktop;
    int self = pidfd_open(getpid(), 0);
    bool valid = process_info(self, &own) && process_info(a->child, &desktop) &&
        desktop.ppid == own.pid && desktop.cgroupid == own.cgroupid && !fstat(a->child, &a->desktop);
    if (self >= 0) close(self);
    if (!valid || fcntl(fd, F_SETFD, FD_CLOEXEC)) {
        ae_unlock_authority_close(&a);
        return -ESTALE;
    }
    a->cgroup = own.cgroupid;
    a->fd = fd;
    *out = a;
    return 0;
}

static void abort_verifier(struct ae_unlock_authority *a) {
    close_fd(&a->result);
    if (live(a->worker)) (void)pidfd_send_signal(a->worker, SIGKILL, NULL, 0);
    a->deadline = 0;
    a->expired = true;
}

static int fence(struct ae_unlock_authority *a, int error) {
    close_fd(&a->fd);
    abort_verifier(a);
    return error;
}

void ae_unlock_authority_close(struct ae_unlock_authority **slot) {
    if (!slot || !*slot) return;
    struct ae_unlock_authority *a = *slot;
    *slot = NULL;
    (void)fence(a, -ESTALE);
    if (a->worker >= 0) {
        siginfo_t status;
        while (waitid(P_PIDFD, (id_t)a->worker, &status, WEXITED) && errno == EINTR) {}
    }
    close_fd(&a->worker);
    close_fd(&a->child);
    free(a);
}

void ae_unlock_authority_poll(const struct ae_unlock_authority *a,
        struct pollfd fds[AE_UNLOCK_AUTHORITY_FDS], int *timeout) {
    *timeout = -1;
    fds[0] = (struct pollfd){.fd = a ? a->fd : -1, .events = POLLIN};
    fds[1] = (struct pollfd){.fd = a && a->fd >= 0 ? a->child : -1, .events = POLLIN};
    fds[2] = (struct pollfd){.fd = a ? a->worker : -1, .events = POLLIN};
    if (a && a->deadline) {
        uint64_t now = monotonic_ms();
        *timeout = !now || now >= a->deadline ? 0 : (int)(a->deadline - now);
    }
}

static int send_result(struct ae_unlock_authority *a, unsigned char result) {
    if (!current(a)) return -ESTALE;
    unsigned char frame[HEADER + 1];
    encode(frame, a->request);
    frame[HEADER] = result;
    return send(a->fd, frame, sizeof frame, MSG_DONTWAIT | MSG_NOSIGNAL) ==
        (ssize_t)sizeof frame ? 0 : -EPIPE;
}

static _Noreturn void verify_child(struct ae_unlock_authority *a, int result, int gate, pid_t parent,
        unsigned char *packet, size_t length) {
    if (prctl(PR_SET_PDEATHSIG, SIGKILL, 0L, 0L, 0L) || getppid() != parent ||
            prctl(PR_SET_DUMPABLE, 0L, 0L, 0L, 0L)) _exit(125);
    char admitted = 0;
    ssize_t count;
    do { count = read(gate, &admitted, 1); } while (count < 0 && errno == EINTR);
    if (count != 1 || admitted != 'A') _exit(125);
    /* The process stays privileged but receives only stdio and its one-way
       result pipe. The preexisting PAM session remains in the parent. */
    if ((result != 3 && dup3(result, 3, O_CLOEXEC) < 0) || close_range(4, ~0u, 0)) _exit(125);
    sigset_t unblocked;
    sigemptyset(&unblocked);
    if (sigprocmask(SIG_SETMASK, &unblocked, NULL) || signal(SIGPIPE, SIG_IGN) == SIG_ERR) _exit(125);
    int status = ae_unlock_pam_verify(a->uid, a->account, packet + HEADER, length - HEADER);
    explicit_bzero(packet, HEADER + AE_UNLOCK_PASSWORD_MAX);
    unsigned char answer = !status ? AE_UNLOCK_ACCEPTED :
        status == -EACCES || status == -EINVAL ? AE_UNLOCK_DENIED : AE_UNLOCK_UNAVAILABLE;
    _exit(write(3, &answer, 1) == 1 ? 0 : 125);
}

static int start_verifier(struct ae_unlock_authority *a, unsigned char *packet, size_t count) {
    int result[2], gate[2];
    if (pipe2(result, O_CLOEXEC | O_NONBLOCK)) return -errno;
    if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, gate)) {
        int error = -errno;
        close(result[0]); close(result[1]);
        return error;
    }
    pid_t parent = getpid();
    pid_t child = fork();
    int failure = -errno;
    if (child == 0) {
        close(result[0]); close(gate[0]);
        verify_child(a, result[1], gate[1], parent, packet, count);
    }
    close(result[1]); close(gate[1]);
    if (child > 0) {
        a->worker = pidfd_open(child, 0);
        failure = -errno;
        if (a->worker < 0) {
            close(gate[0]);
            while (waitpid(child, NULL, 0) < 0 && errno == EINTR) {}
            close(result[0]);
            return failure;
        }
        if (send(gate[0], "A", 1, MSG_NOSIGNAL) == 1) {
            a->result = result[0];
            close(gate[0]);
            return 0;
        }
        failure = -EPIPE;
    }
    close(gate[0]); close(result[0]);
    return failure;
}

static int receive_request(struct ae_unlock_authority *a, uint64_t now) {
    unsigned char packet[HEADER + AE_UNLOCK_PASSWORD_MAX];
    ssize_t count = ae_pidfd_receive(a->fd, packet, sizeof packet, &a->desktop);
    int status = 0;
    if (count != -EAGAIN) {
        if (count <= HEADER || a->worker >= 0 || memcmp(packet, magic, PREFIX) ||
                memchr(packet + HEADER, 0, (size_t)count - HEADER) || !current(a)) {
            status = -EPROTO;
        } else {
            struct association next = {decode64(packet + PREFIX), decode64(packet + PREFIX + 8)};
            if (!next.generation || next.generation < a->request.generation ||
                    a->request.ticket == UINT64_MAX || next.ticket != a->request.ticket + 1) {
                status = -ESTALE;
            } else {
                a->request = next;
                a->expired = false;
                if (!start_verifier(a, packet, (size_t)count)) a->deadline = now + VERIFY_MS;
                else if (a->worker >= 0) abort_verifier(a);
                else status = send_result(a, AE_UNLOCK_UNAVAILABLE);
            }
        }
    }
    explicit_bzero(packet, sizeof packet);
    return status;
}

static int collect(struct ae_unlock_authority *a, uint64_t now) {
    if (a->worker < 0) return 0;
    if (a->deadline && now >= a->deadline) abort_verifier(a);
    siginfo_t info = {0};
    if (waitid(P_PIDFD, (id_t)a->worker, &info, WEXITED | WNOHANG)) return errno == EINTR ? 0 : -errno;
    if (!info.si_pid) return 0;
    close_fd(&a->worker);
    a->deadline = 0;
    unsigned char reply = AE_UNLOCK_UNAVAILABLE, message[2];
    if (!a->expired && info.si_code == CLD_EXITED && !info.si_status &&
            read(a->result, message, sizeof message) == 1 &&
            message[0] >= AE_UNLOCK_ACCEPTED && message[0] <= AE_UNLOCK_UNAVAILABLE) {
        unsigned char extra;
        if (read(a->result, &extra, 1) == 0) reply = message[0];
    }
    close_fd(&a->result);
    return a->fd < 0 ? 0 : send_result(a, reply);
}

int ae_unlock_authority_step(struct ae_unlock_authority *a) {
    if (!a) return -EINVAL;
    uint64_t now = monotonic_ms();
    if (a->fd < 0) {
        (void)collect(a, now);
        return -ESTALE;
    }
    if (!now || !live(a->child) || (a->worker >= 0 && !current(a))) return fence(a, -ESTALE);
    int status = receive_request(a, now);
    if (!status) status = collect(a, now);
    return status ? fence(a, status) : 0;
}

int ae_unlock_authority_client_open(struct ae_unlock_authority_client **out, int fd) {
    pid_t parent = getppid();
    if (!out || *out || !getuid() || getuid() != geteuid() || getgid() != getegid() ||
            parent <= 1 || !creator(fd, parent)) return -EPERM;
    struct ae_unlock_authority_client *c = calloc(1, sizeof *c);
    if (!c) return -ENOMEM;
    c->parent = -1;
    socklen_t size = sizeof c->parent;
    struct pidfd_info info;
    if (getsockopt(fd, SOL_SOCKET, SO_PEERPIDFD, &c->parent, &size) || size != sizeof c->parent ||
            !process_info(c->parent, &info) || info.pid != (uint32_t)parent || info.euid != 0 ||
            getppid() != parent || fstat(c->parent, &c->owner) || fcntl(fd, F_SETFD, FD_CLOEXEC)) {
        close_fd(&c->parent);
        free(c);
        return -ESTALE;
    }
    c->fd = fd;
    *out = c;
    return 0;
}

void ae_unlock_authority_client_poll(const struct ae_unlock_authority_client *c, struct pollfd fds[2]) {
    fds[0] = (struct pollfd){.fd = c ? c->fd : -1, .events = POLLIN};
    fds[1] = (struct pollfd){.fd = c && c->fd >= 0 ? c->parent : -1, .events = POLLIN};
}
static int client_lost(struct ae_unlock_authority_client *c) {
    close_fd(&c->fd);
    c->pending = false;
    return -ESTALE;
}

int ae_unlock_authority_client_begin(struct ae_unlock_authority_client *c, uint64_t generation,
        const void *password, size_t size) {
    if (!c || !password || !size || size > AE_UNLOCK_PASSWORD_MAX || memchr(password, 0, size))
        return -EINVAL;
    if (c->fd < 0 || !live(c->parent) || c->pending || !generation ||
            generation < c->request.generation || c->request.ticket == UINT64_MAX) return client_lost(c);
    struct association request = {generation, c->request.ticket + 1};
    unsigned char header[HEADER];
    encode(header, request);
    struct iovec parts[] = {{header, sizeof header}, {(void *)password, size}};
    struct msghdr message = {.msg_iov = parts, .msg_iovlen = 2};
    if (sendmsg(c->fd, &message, MSG_DONTWAIT | MSG_NOSIGNAL) !=
            (ssize_t)(sizeof header + size)) return client_lost(c);
    c->request = request;
    c->pending = true;
    return 0;
}

int ae_unlock_authority_client_receive(struct ae_unlock_authority_client *c, uint64_t generation,
        enum ae_unlock_result *out) {
    if (!c || !out) return -EINVAL;
    if (c->fd < 0 || !live(c->parent) ||
            (c->pending && generation != c->request.generation)) return client_lost(c);
    unsigned char frame[HEADER + 1];
    ssize_t n = ae_pidfd_receive(c->fd, frame, sizeof frame, &c->owner);
    if (n == -EAGAIN) return -EAGAIN;
    unsigned char expected[HEADER];
    encode(expected, c->request);
    if (n != (ssize_t)sizeof frame || !c->pending || memcmp(frame, expected, sizeof expected) ||
            frame[HEADER] < AE_UNLOCK_ACCEPTED || frame[HEADER] > AE_UNLOCK_UNAVAILABLE ||
            !live(c->parent)) return client_lost(c);
    *out = (enum ae_unlock_result)frame[HEADER];
    c->pending = false;
    return 0;
}

void ae_unlock_authority_client_close(struct ae_unlock_authority_client **slot) {
    if (!slot || !*slot) return;
    struct ae_unlock_authority_client *c = *slot;
    *slot = NULL;
    close_fd(&c->fd);
    close_fd(&c->parent);
    free(c);
}
