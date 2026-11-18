#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "pidfd_channel.h"

#include <errno.h>
#include <stdbool.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static bool same_process(int fd, const struct stat *expected) {
    struct stat info;
    return !fstat(fd, &info) && info.st_dev == expected->st_dev && info.st_ino == expected->st_ino;
}

/* Receive one packet from the original process. A datagram with an empty body
   still carries a pidfd, while orderly EOF carries no control record. */
ssize_t ae_pidfd_receive(int fd, void *data, size_t capacity, const struct stat *peer) {
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
