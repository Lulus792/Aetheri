#include "core/storage.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/openat2.h>
#include <stdint.h>
#include <stdlib.h>
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

static int check_owner_header(int root_fd, uid_t expected_uid, const struct ae_owner *expected, int exact_mode)
{
    if (expected == NULL) return -EINVAL;
    int descriptor = ae_open_small_file(root_fd, "owner", 5, AE_OWNER_BYTES);
    if (descriptor < 0) return descriptor;
    struct stat status;
    int error = 0;
    if (fstat(descriptor, &status) != 0) error = errno;
    else if (status.st_uid != expected_uid || (status.st_mode & (S_IWGRP | S_IWOTH)) != 0) error = EACCES;
    else if (exact_mode && (status.st_mode & 07777) != 0600) error = EACCES;
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

int ae_check_owner_header(int root_fd, uid_t expected_uid, const struct ae_owner *expected)
{
    return check_owner_header(root_fd, expected_uid, expected, 0);
}

struct ae_storage_ref {
    int anchor, root;
    pid_t process;
    uid_t uid;
    char leaf[33];
    struct ae_owner owner;
    struct ae_root_identity anchor_identity, root_identity;
};

static int open_root(int anchor, const char *leaf)
{
    struct open_how how = {.flags = O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW,
                          .resolve = RESOLVE_BENEATH | RESOLVE_NO_SYMLINKS | RESOLVE_NO_MAGICLINKS | RESOLVE_NO_XDEV};
    int fd;
    do { fd = (int)syscall(SYS_openat2, anchor, leaf, &how, sizeof how); } while (fd < 0 && errno == EINTR);
    return fd < 0 ? -errno : fd;
}

int ae_storage_ref_check(struct ae_storage_ref *reference)
{
    if (reference == NULL) return -EINVAL;
    if (reference->process != getpid()) return -ECHILD;
    struct ae_root_identity anchor, root;
    int result = ae_root_observe(reference->anchor, &anchor);
    if (result == 0 && !ae_root_equal(&anchor, &reference->anchor_identity)) result = -ESTALE;
    if (result == 0) result = ae_root_observe(reference->root, &root);
    if (result == 0 && !ae_root_equal(&root, &reference->root_identity)) result = -ESTALE;
    int named = -1;
    if (result == 0) {
        named = open_root(reference->anchor, reference->leaf);
        if (named < 0) result = named;
    }
    if (result == 0) result = ae_root_observe(named, &root);
    if (result == 0 && !ae_root_equal(&root, &reference->root_identity)) result = -ESTALE;
    if (named >= 0) close(named);
    if (result == 0) result = check_owner_header(reference->root, reference->uid, &reference->owner, 1);
    return result;
}

int ae_storage_ref_open(int anchor_fd, uid_t metadata_uid, const struct ae_owner *owner,
                        const struct ae_root_identity *expected, struct ae_storage_ref **output)
{
    uint8_t encoded[AE_OWNER_BYTES];
    if (anchor_fd < 0 || owner == NULL || expected == NULL || output == NULL ||
        ae_owner_encode(owner, encoded, sizeof encoded) != 0 || expected->mount_id == 0 || expected->inode == 0) return -EINVAL;
    if (expected->uid != metadata_uid || (expected->mode & 07777) != 0700) return -EACCES;
    struct ae_storage_ref *reference = calloc(1, sizeof *reference);
    if (reference == NULL) return -ENOMEM;
    reference->root = reference->anchor = -1;
    reference->process = getpid(); reference->uid = metadata_uid;
    reference->owner = *owner; reference->root_identity = *expected;
    static const char digits[] = "0123456789abcdef";
    for (size_t n = 0; n < AE_ID_BYTES; ++n) {
        reference->leaf[2U * n] = digits[owner->id[AE_STORAGE][n] >> 4];
        reference->leaf[2U * n + 1U] = digits[owner->id[AE_STORAGE][n] & 15U];
    }
    reference->anchor = fcntl(anchor_fd, F_DUPFD_CLOEXEC, 3);
    int result = reference->anchor < 0 ? -errno : ae_root_observe(reference->anchor, &reference->anchor_identity);
    if (result == 0) {
        reference->root = open_root(reference->anchor, reference->leaf);
        if (reference->root < 0) result = reference->root;
    }
    if (result == 0) result = ae_storage_ref_check(reference);
    if (result != 0) { ae_storage_ref_close(&reference); return result; }
    *output = reference;
    return 0;
}

int ae_storage_ref_read(struct ae_storage_ref *reference, const char *relative, size_t length, size_t limit)
{
    if (!valid_path(relative, length) || limit > 65536) return -EINVAL;
    int result = ae_storage_ref_check(reference);
    if (result != 0) return result;
    int payload = open_root(reference->root, "payload");
    if (payload < 0) return payload;
    struct stat status;
    if (fstat(payload, &status) != 0) result = -errno;
    else if (status.st_uid != reference->uid || (status.st_mode & 07777) != 0700) result = -EACCES;
    int fd = result == 0 ? ae_open_small_file(payload, relative, length, limit) : result;
    close(payload);
    if (fd < 0) return fd;
    if (fstat(fd, &status) != 0) result = -errno;
    else if (status.st_uid != reference->uid) result = -EACCES;
    if (result != 0) { close(fd); return result; }
    result = ae_storage_ref_check(reference);
    if (result != 0) { close(fd); return result; }
    return fd;
}

int ae_storage_ref_close(struct ae_storage_ref **output)
{
    if (output == NULL) return -EINVAL;
    struct ae_storage_ref *reference = *output;
    if (reference == NULL) return 0;
    if (reference->process != getpid()) return -ECHILD;
    if (reference->root >= 0) close(reference->root);
    if (reference->anchor >= 0) close(reference->anchor);
    free(reference); *output = NULL;
    return 0;
}
