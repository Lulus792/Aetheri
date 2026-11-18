#define _GNU_SOURCE
#include "unlock_channel.h"
#include "unlock_pam.h"
#include "linux/unnamed_socket.h"

#include <fcntl.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/random.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

static const unsigned char request[] = "AEUP1Q";
static const unsigned char reply[] = "AEUP1R";
enum { MAGIC_SIZE = sizeof request - 1, TICKET_SIZE = 16, HEADER_SIZE = MAGIC_SIZE + TICKET_SIZE };

struct ae_unlock_attempt {
    int fd, pin;
    pid_t thread;
    uint64_t generation;
    unsigned char ticket[TICKET_SIZE];
    struct stat child;
    enum ae_unlock_result result;
};

static void retire(struct ae_unlock_attempt *a) {
    if (a->fd >= 0) close(a->fd);
    if (a->pin >= 0) close(a->pin);
    a->fd = a->pin = -1;
}

static int sample_exit(int pin, siginfo_t *info) {
    *info = (siginfo_t){0};
    if (!waitid(P_PIDFD, (id_t)pin, info, WEXITED | WNOHANG | WNOWAIT)) return 0;
    return errno == EINTR ? -EAGAIN : -errno;
}

static int enable_pidfd(int fd) {
    const int enabled = 1;
    return setsockopt(fd, SOL_SOCKET, SO_PASSPIDFD, &enabled, sizeof enabled) ? -errno : 0;
}

int ae_unlock_worker_pair(int pair[2]) {
    if (!pair) return -EINVAL;
    int fd[2];
    if (socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, fd)) return -errno;
    int status = fd[0] < 3 || fd[1] < 3 ? -EBADF : enable_pidfd(fd[1]);
    if (status) {
        close(fd[0]);
        close(fd[1]);
        return status;
    }
    pair[0] = fd[0];
    pair[1] = fd[1];
    return 0;
}

int ae_unlock_attempt_begin(struct ae_unlock_attempt **out, int fd, int child,
        uint64_t generation, const void *password, size_t size) {
    if (!out || *out || fd < 3 || child < 3 || fd == child || !generation ||
            !password || !size || size > AE_UNLOCK_PASSWORD_MAX || memchr(password, 0, size))
        return -EINVAL;
    int status = ae_unnamed_socket_check(fd, SOCK_SEQPACKET);
    if (status) return status;
    int held = fcntl(child, F_DUPFD_CLOEXEC, 3);
    if (held < 0) return -errno;
    struct ae_unlock_attempt *a = calloc(1, sizeof(*a));
    if (!a) { close(held); return -ENOMEM; }
    a->fd = -1;
    a->pin = held;
    a->thread = gettid();
    a->generation = generation;
    siginfo_t exit;
    status = sample_exit(held, &exit);
    if (status) goto failed;
    if (exit.si_pid != 0) { status = -ESRCH; goto failed; }
    if (fstat(held, &a->child) || fcntl(fd, F_SETFD, FD_CLOEXEC)) {
        status = -errno;
        goto failed;
    }
    ssize_t bytes = getrandom(a->ticket, sizeof a->ticket, GRND_NONBLOCK);
    if (bytes != (ssize_t)sizeof a->ticket) {
        status = bytes < 0 ? -errno : -EIO;
        goto failed;
    }
    unsigned char header[HEADER_SIZE];
    memcpy(header, request, MAGIC_SIZE);
    memcpy(header + MAGIC_SIZE, a->ticket, sizeof a->ticket);
    /* sendmsg borrows the caller's bytes. The owner must wipe them after return. */
    struct iovec parts[] = {{header, sizeof header}, {(void *)password, size}};
    struct msghdr message = {.msg_iov = parts, .msg_iovlen = 2};
    bytes = sendmsg(fd, &message, MSG_DONTWAIT | MSG_NOSIGNAL);
    if (bytes != (ssize_t)(sizeof header + size)) {
        status = bytes < 0 ? -errno : -EPROTO;
        goto failed;
    }
    /* With SO_PASSPIDFD already enabled on the sending socket, Linux assigns
       an abstract address on the first send. Arm the owner's reader after
       sending, so the worker still sees the original unnamed pair. Its own
       receive option supplies kernel sender identity to both packet sends. */
    status = enable_pidfd(fd);
    if (status) goto failed;
    a->fd = fd;
    *out = a;
    return 0;

failed:
    retire(a);
    free(a);
    return status;
}

int ae_unlock_attempt_poll(const struct ae_unlock_attempt *a, struct pollfd sources[2]) {
    if (!a || !sources || a->thread != gettid()) return -EINVAL;
    if (a->pin < 0) return -ESTALE;
    sources[0] = (struct pollfd){.fd = a->fd, .events = POLLIN};
    sources[1] = (struct pollfd){.fd = a->pin, .events = POLLIN};
    return 0;
}

static bool same_process(int fd, const struct stat *expected) {
    struct stat info;
    return !fstat(fd, &info) && info.st_dev == expected->st_dev && info.st_ino == expected->st_ino;
}

/* Receive one packet from the original process. A datagram with an empty body
   still carries a pidfd, while orderly EOF carries no control record. */
static ssize_t receive_bound(int fd, void *data, size_t capacity, const struct stat *peer) {
    struct iovec part = {data, capacity};
    union {
        struct cmsghdr alignment;
        unsigned char data[CMSG_SPACE(sizeof(int)) + CMSG_SPACE(4 * sizeof(int))];
    } control = {0};
    struct msghdr message = {.msg_iov = &part, .msg_iovlen = 1,
        .msg_control = control.data, .msg_controllen = sizeof control.data};
    ssize_t bytes = recvmsg(fd, &message, MSG_DONTWAIT | MSG_CMSG_CLOEXEC);
    if (bytes < 0) return errno == EINTR || errno == EAGAIN ? -EAGAIN : -errno;
    if (!bytes && !message.msg_controllen && !(message.msg_flags & (MSG_TRUNC | MSG_CTRUNC)))
        return 0;

    bool valid = !(message.msg_flags & (MSG_TRUNC | MSG_CTRUNC));
    unsigned pins = 0;
    for (struct cmsghdr *c = CMSG_FIRSTHDR(&message); c; c = CMSG_NXTHDR(&message, c)) {
        if (c->cmsg_level != SOL_SOCKET || c->cmsg_len < CMSG_LEN(0) ||
                (c->cmsg_type != SCM_RIGHTS && c->cmsg_type != SCM_PIDFD)) {
            valid = false;
            continue;
        }
        size_t size = c->cmsg_len - CMSG_LEN(0);
        bool pin = c->cmsg_type == SCM_PIDFD && size == sizeof(int);
        if (!pin) valid = false;
        for (size_t i = 0; i + sizeof(int) <= size; i += sizeof(int)) {
            int incoming;
            memcpy(&incoming, CMSG_DATA(c) + i, sizeof incoming);
            if (pin && (++pins != 1 || !same_process(incoming, peer))) valid = false;
            close(incoming);
        }
    }
    if (!valid || pins != 1 || !bytes) return -EPROTO;
    return bytes;
}

/* 0 means EOF, 1 records a complete reply, -EAGAIN means pending. */
static int read_reply(struct ae_unlock_attempt *a) {
    unsigned char data[HEADER_SIZE + 1];
    ssize_t bytes = receive_bound(a->fd, data, sizeof data, &a->child);
    if (bytes <= 0) return (int)bytes;
    if (a->result || bytes != (ssize_t)sizeof data ||
            memcmp(data, reply, MAGIC_SIZE) ||
            memcmp(data + MAGIC_SIZE, a->ticket, sizeof a->ticket) ||
            data[HEADER_SIZE] < AE_UNLOCK_ACCEPTED || data[HEADER_SIZE] > AE_UNLOCK_UNAVAILABLE)
        return -EPROTO;
    a->result = (enum ae_unlock_result)data[HEADER_SIZE];
    return 1;
}

int ae_unlock_attempt_receive(struct ae_unlock_attempt *a, uint64_t generation,
        enum ae_unlock_result *out) {
    if (!a || !out || a->thread != gettid()) return -EINVAL;
    if (a->pin < 0) return -ESTALE;
    int status = -ESTALE;
    if (generation != a->generation) goto failed;
    if (a->fd >= 0) {
        status = read_reply(a);
        if (status == -EAGAIN) {
            siginfo_t before;
            status = sample_exit(a->pin, &before);
            if (status) goto failed;
            if (!before.si_pid) return -EAGAIN;
            /* One final read after observing the original terminal child.
               No EOF after exit indicates an extra holder of the peer. */
            status = read_reply(a);
            if (status == -EAGAIN) status = -EPROTO;
        }
        if (status > 0) status = read_reply(a);
        if (status < 0) goto failed;
        close(a->fd);
        a->fd = -1;
        if (!a->result) { status = -ECONNRESET; goto failed; }
    }
    siginfo_t exit;
    status = sample_exit(a->pin, &exit);
    if (status) goto failed;
    if (!exit.si_pid) return -EAGAIN;
    if (exit.si_code != CLD_EXITED || exit.si_status != 0) { status = -ECHILD; goto failed; }
    *out = a->result;
    retire(a);
    return 0;

failed:
    if (status != -EAGAIN) retire(a);
    return status;
}

void ae_unlock_attempt_close(struct ae_unlock_attempt **slot) {
    if (!slot || !*slot || (*slot)->thread != gettid()) return;
    retire(*slot);
    free(*slot);
    *slot = NULL;
}

static bool admitted_parent(int fd) {
    struct ucred owner;
    socklen_t size = sizeof owner;
    pid_t parent = getppid();
    return fd >= 3 && parent > 1 && getuid() != 0 && getuid() == geteuid() && getgid() == getegid() &&
        !ae_unnamed_socket_check(fd, SOCK_SEQPACKET) &&
        !getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &owner, &size) && size == sizeof owner &&
        owner.pid == parent && owner.uid == getuid() && owner.gid == getgid();
}

int ae_unlock_worker_once(int fd, const char *account) {
    if (!account || !admitted_parent(fd)) return -EPERM;
    if (prctl(PR_SET_DUMPABLE, 0L, 0L, 0L, 0L) || fcntl(fd, F_SETFD, FD_CLOEXEC)) return -errno;
    int parent = -1;
    socklen_t length = sizeof parent;
    if (getsockopt(fd, SOL_SOCKET, SO_PEERPIDFD, &parent, &length)) return -errno;
    struct stat identity;
    int status = length != sizeof parent || parent < 0 ? -EPROTO :
        fstat(parent, &identity) ? -errno : 0;
    if (parent >= 0) close(parent);
    if (status) return status;
    int enabled = 0;
    length = sizeof enabled;
    if (getsockopt(fd, SOL_SOCKET, SO_PASSPIDFD, &enabled, &length)) return -errno;
    if (length != sizeof enabled || enabled != 1) return -EPROTO;
    struct pollfd wait = {.fd = fd, .events = POLLIN};
    int ready = poll(&wait, 1, 15000);
    if (ready <= 0) return ready < 0 ? -errno : -ETIMEDOUT;

    unsigned char payload[HEADER_SIZE + AE_UNLOCK_PASSWORD_MAX];
    ssize_t bytes = receive_bound(fd, payload, sizeof payload, &identity);
    int failure = bytes < 0 ? (int)bytes : -EPROTO;
    unsigned char answer[HEADER_SIZE + 1];
    if (bytes <= HEADER_SIZE || memcmp(payload, request, MAGIC_SIZE)) {
        explicit_bzero(payload, sizeof payload);
        return failure;
    }
    memcpy(answer, reply, MAGIC_SIZE);
    memcpy(answer + MAGIC_SIZE, payload + MAGIC_SIZE, TICKET_SIZE);
    status = ae_unlock_pam_verify(getuid(), account, payload + HEADER_SIZE,
        (size_t)bytes - HEADER_SIZE);
    explicit_bzero(payload, sizeof payload);
    if (!admitted_parent(fd)) return -EPIPE;
    answer[HEADER_SIZE] = !status ? AE_UNLOCK_ACCEPTED :
        status == -EACCES || status == -EINVAL ? AE_UNLOCK_DENIED : AE_UNLOCK_UNAVAILABLE;
    bytes = send(fd, answer, sizeof answer, MSG_DONTWAIT | MSG_NOSIGNAL);
    return bytes == (ssize_t)sizeof answer ? 0 : bytes < 0 ? -errno : -EPROTO;
}
