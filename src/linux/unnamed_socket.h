#ifndef AETHERI_LINUX_UNNAMED_SOCKET_H
#define AETHERI_LINUX_UNNAMED_SOCKET_H

#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>

static inline int ae_unnamed_socket_check(int fd, int type) {
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

#endif
