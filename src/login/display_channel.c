#define _GNU_SOURCE
#include "login/display_channel.h"
#include <errno.h>
#include <stdbool.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

static const char offer[] = "AGW1";

int ae_login_display_channel_check(int fd, int type)
{
    int value;
    socklen_t size = sizeof value;
    if (getsockopt(fd, SOL_SOCKET, SO_TYPE, &value, &size)) return -errno;
    if (size != sizeof value || value != type) return -EPROTO;
    struct sockaddr_un address;
    size = sizeof address;
    if (getsockname(fd, (struct sockaddr *)&address, &size)) return -errno;
    if (size != sizeof address.sun_family || address.sun_family != AF_UNIX) return -EPROTO;
    size = sizeof address;
    if (getpeername(fd, (struct sockaddr *)&address, &size)) return -errno;
    return size == sizeof address.sun_family && address.sun_family == AF_UNIX ? 0 : -EPROTO;
}

int ae_login_display_offer(int control, int stream)
{
    int result = ae_login_display_channel_check(control, SOCK_SEQPACKET);
    if (!result) result = ae_login_display_channel_check(stream, SOCK_STREAM);
    if (result) return result;
    char bytes[sizeof offer - 1];
    memcpy(bytes, offer, sizeof bytes);
    struct iovec body = { .iov_base = bytes, .iov_len = sizeof bytes };
    union { struct cmsghdr aligned; unsigned char bytes[CMSG_SPACE(sizeof(int))]; } ancillary = {0};
    struct msghdr packet = { .msg_iov = &body, .msg_iovlen = 1,
        .msg_control = ancillary.bytes, .msg_controllen = sizeof ancillary.bytes };
    struct cmsghdr *header = CMSG_FIRSTHDR(&packet);
    header->cmsg_level = SOL_SOCKET;
    header->cmsg_type = SCM_RIGHTS;
    header->cmsg_len = CMSG_LEN(sizeof stream);
    memcpy(CMSG_DATA(header), &stream, sizeof stream);
    ssize_t sent = sendmsg(control, &packet, MSG_NOSIGNAL | MSG_DONTWAIT);
    return sent < 0 ? -errno : sent == (ssize_t)sizeof bytes ? 0 : -EPROTO;
}

int ae_login_display_take(int control, int *stream)
{
    if (!stream || *stream != -1) return -EINVAL;
    char bytes[sizeof offer - 1];
    struct iovec body = { .iov_base = bytes, .iov_len = sizeof bytes };
    union { struct cmsghdr aligned; unsigned char bytes[CMSG_SPACE(4 * sizeof(int))]; } ancillary = {0};
    struct msghdr packet = { .msg_iov = &body, .msg_iovlen = 1,
        .msg_control = ancillary.bytes, .msg_controllen = sizeof ancillary.bytes };
    ssize_t received = recvmsg(control, &packet, MSG_CMSG_CLOEXEC | MSG_DONTWAIT);
    if (received < 0)
        return errno == EINTR || errno == EWOULDBLOCK ? -EAGAIN : -errno;

    int selected = -1;
    size_t count = 0;
    bool valid = received == (ssize_t)sizeof bytes && !memcmp(bytes, offer, sizeof bytes) &&
        !(packet.msg_flags & (MSG_TRUNC | MSG_CTRUNC));
    for (struct cmsghdr *header = CMSG_FIRSTHDR(&packet); header; header = CMSG_NXTHDR(&packet, header)) {
        if (header->cmsg_level != SOL_SOCKET || header->cmsg_type != SCM_RIGHTS ||
            header->cmsg_len < CMSG_LEN(0)) {
            valid = false;
            continue;
        }
        size_t size = header->cmsg_len - CMSG_LEN(0);
        if (size != sizeof(int)) valid = false;
        for (size_t offset = 0; offset + sizeof(int) <= size; offset += sizeof(int)) {
            int incoming;
            memcpy(&incoming, CMSG_DATA(header) + offset, sizeof incoming);
            if (count++ == 0) selected = incoming;
            else close(incoming);
        }
    }
    if (!valid || count != 1) {
        if (selected >= 0) close(selected);
        return received ? -EPROTO : -ECONNRESET;
    }
    *stream = selected;
    return 0;
}
