#ifndef AETHERI_LINUX_PIDFD_CHANNEL_H
#define AETHERI_LINUX_PIDFD_CHANNEL_H

#include <stddef.h>
#include <sys/stat.h>
#include <sys/types.h>

/* Nonblocking receive with SO_PASSPIDFD enabled. expected identifies the
   original pinned process. Accept one kernel sender pidfd and close all
   received descriptors. Returns packet size, zero for EOF, or -errno.
   The caller must treat the buffer as untrusted even when bytes were copied
   before a rejection. */
ssize_t ae_pidfd_receive(int fd, void *data, size_t capacity, const struct stat *expected);

#endif
