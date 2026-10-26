#include "core/storage.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/openat2.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

static int valid_path(const char *value, size_t length)
{
    if (value == NULL || length == 0 || length > 256) return 0;
    size_t start = 0;
    for (size_t n = 0; n <= length; ++n) {
        if (n == length || value[n] == '/') {
            size_t segment = n - start;
            if (segment == 0 || segment > 64 ||
                (segment == 1 && value[start] == '.') ||
                (segment == 2 && value[start] == '.' && value[start + 1U] == '.')) return 0;
            start = n + 1U;
        } else {
            unsigned char c = (unsigned char)value[n];
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                  (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-')) return 0;
        }
    }
    return 1;
}

int ae_open_small_file(int root_fd, const char *relative, size_t length, size_t limit)
{
    if (root_fd < 0 || !valid_path(relative, length) || limit > 65536) return -EINVAL;
    char path[257];
    memcpy(path, relative, length);
    path[length] = '\0';
    struct open_how how = {0};
    how.flags = O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK;
    how.resolve = RESOLVE_BENEATH | RESOLVE_NO_SYMLINKS | RESOLVE_NO_MAGICLINKS | RESOLVE_NO_XDEV;
    int descriptor;
    do {
        descriptor = (int)syscall(SYS_openat2, root_fd, path, &how, sizeof how);
    } while (descriptor < 0 && errno == EINTR);
    if (descriptor < 0) return -errno;
    struct stat status;
    int error = 0;
    if (fstat(descriptor, &status) != 0) error = errno;
    else if (!S_ISREG(status.st_mode) || status.st_nlink != 1) error = EINVAL;
    else if (status.st_size < 0 || (uintmax_t)status.st_size > (uintmax_t)limit) error = EFBIG;
    if (error != 0) {
        close(descriptor);
        return -error;
    }
    return descriptor;
}

int ae_check_owner_header(int root_fd, uid_t expected_uid, const struct ae_owner *expected)
{
    if (expected == NULL) return -EINVAL;
    int descriptor = ae_open_small_file(root_fd, "owner", 5, AE_OWNER_BYTES);
    if (descriptor < 0) return descriptor;
    struct stat status;
    int error = 0;
    if (fstat(descriptor, &status) != 0) error = errno;
    else if (status.st_uid != expected_uid || (status.st_mode & (S_IWGRP | S_IWOTH)) != 0) error = EACCES;
    uint8_t bytes[AE_OWNER_BYTES + 1U];
    size_t used = 0;
    while (error == 0 && used < sizeof bytes) {
        ssize_t count = read(descriptor, bytes + used, sizeof bytes - used);
        if (count < 0) {
            if (errno == EINTR) continue;
            error = errno;
        } else if (count == 0) break;
        else used += (size_t)count;
    }
    close(descriptor);
    if (error != 0) return -error;
    struct ae_owner actual;
    if (ae_owner_decode(bytes, used, &actual) != 0) return -EBADMSG;
    return ae_owner_matches(&actual, expected) ? 0 : -ESTALE;
}
