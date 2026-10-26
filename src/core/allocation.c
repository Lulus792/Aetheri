#include "core/allocation.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/openat2.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

static int valid(const struct ae_allocation *value)
{
    uint8_t owner[AE_OWNER_BYTES];
    return value != NULL && ae_owner_encode(&value->owner, owner, sizeof owner) == 0 &&
           value->filesystem_type > 0 && value->filesystem_type <= INT64_MAX &&
           value->anchor.inode > 0 && value->anchor.inode <= INT64_MAX &&
           value->root.inode > 0 && value->root.inode <= INT64_MAX && value->root.inode != value->anchor.inode &&
           value->anchor.birth_seconds <= INT64_MAX && value->root.birth_seconds <= INT64_MAX &&
           value->anchor.birth_nanoseconds < 1000000000U && value->root.birth_nanoseconds < 1000000000U;
}

static void put(uint8_t *bytes, uint64_t value, size_t size)
{
    for (size_t n = 0; n < size; ++n) { bytes[size - n - 1U] = (uint8_t)value; value >>= 8; }
}

static uint64_t get(const uint8_t *bytes, size_t size)
{
    uint64_t value = 0;
    for (size_t n = 0; n < size; ++n) value = (value << 8) | bytes[n];
    return value;
}

int ae_allocation_encode(const struct ae_allocation *value, uint8_t *output, size_t size)
{
    if (output == NULL || size < AE_ALLOCATION_BYTES || !valid(value)) return -EINVAL;
    uint8_t bytes[AE_ALLOCATION_BYTES];
    memcpy(bytes, "AETHALC1", 8);
    if (ae_owner_encode(&value->owner, bytes + 8, AE_OWNER_BYTES) != 0) return -EINVAL;
    put(bytes + 112, value->filesystem_type, 8); memcpy(bytes + 120, value->filesystem_id, 8);
    put(bytes + 128, value->anchor.inode, 8); put(bytes + 136, value->anchor.birth_seconds, 8);
    put(bytes + 144, value->anchor.birth_nanoseconds, 4);
    put(bytes + 148, value->root.inode, 8); put(bytes + 156, value->root.birth_seconds, 8);
    put(bytes + 164, value->root.birth_nanoseconds, 4);
    memcpy(output, bytes, sizeof bytes);
    return 0;
}

int ae_allocation_decode(const uint8_t *bytes, size_t size, struct ae_allocation *output)
{
    if (bytes == NULL || output == NULL) return -EINVAL;
    if (size != AE_ALLOCATION_BYTES || memcmp(bytes, "AETHALC1", 8) != 0) return -EBADMSG;
    struct ae_allocation value = {0};
    if (ae_owner_decode(bytes + 8, AE_OWNER_BYTES, &value.owner) != 0) return -EBADMSG;
    value.filesystem_type = get(bytes + 112, 8); memcpy(value.filesystem_id, bytes + 120, 8);
    value.anchor.inode = get(bytes + 128, 8); value.anchor.birth_seconds = get(bytes + 136, 8);
    value.anchor.birth_nanoseconds = (uint32_t)get(bytes + 144, 4);
    value.root.inode = get(bytes + 148, 8); value.root.birth_seconds = get(bytes + 156, 8);
    value.root.birth_nanoseconds = (uint32_t)get(bytes + 164, 4);
    if (!valid(&value)) return -EBADMSG;
    *output = value;
    return 0;
}

static void key(const struct ae_owner *owner, char output[33])
{
    static const char digits[] = "0123456789abcdef";
    for (size_t n = 0; n < AE_ID_BYTES; ++n) {
        output[2U * n] = digits[owner->id[AE_STORAGE][n] >> 4];
        output[2U * n + 1U] = digits[owner->id[AE_STORAGE][n] & 15U];
    }
    output[32] = 0;
}

static int beneath(int directory, const char *name, uint64_t flags)
{
    struct open_how how = {.flags = flags | O_CLOEXEC | O_NOFOLLOW,
        .mode = (flags & O_TMPFILE) == O_TMPFILE ? 0600 : 0,
        .resolve = RESOLVE_BENEATH | RESOLVE_NO_SYMLINKS | RESOLVE_NO_MAGICLINKS | RESOLVE_NO_XDEV};
    int fd;
    do { fd = (int)syscall(SYS_openat2, directory, name, &how, sizeof how); } while (fd < 0 && errno == EINTR);
    return fd < 0 ? -errno : fd;
}

static int object(const struct ae_root_identity *identity, struct ae_allocation_object *output)
{
    if (!identity->birth_present) return -ENOTSUP;
    if (identity->birth_seconds < 0) return -ERANGE;
    output->inode = identity->inode; output->birth_seconds = (uint64_t)identity->birth_seconds;
    output->birth_nanoseconds = identity->birth_nanoseconds;
    return 0;
}

int ae_allocation_capture(int anchor_fd, int root_fd, const struct ae_owner *owner, struct ae_allocation *output)
{
    if (owner == NULL || output == NULL) return -EINVAL;
    struct ae_root_identity anchor, root;
    int result = ae_root_observe(anchor_fd, &anchor);
    if (result == 0) result = ae_root_observe(root_fd, &root);
    struct ae_storage_ref *reference = NULL;
    if (result == 0) result = ae_storage_ref_open(anchor_fd, geteuid(), owner, &root, &reference);
    struct ae_allocation value = {.owner = *owner};
    if (result == 0) result = object(&anchor, &value.anchor);
    if (result == 0) result = object(&root, &value.root);
    if (result == 0 && (anchor.filesystem_type <= 0 || anchor.filesystem_type != root.filesystem_type ||
        memcmp(anchor.filesystem_id, root.filesystem_id, sizeof anchor.filesystem_id) != 0)) result = -EXDEV;
    if (result == 0) {
        value.filesystem_type = (uint64_t)root.filesystem_type;
        memcpy(value.filesystem_id, root.filesystem_id, sizeof value.filesystem_id);
        struct ae_root_identity after;
        result = ae_root_observe(anchor_fd, &after);
        if (result == 0 && !ae_root_equal(&anchor, &after)) result = -ESTALE;
    }
    if (result == 0) result = ae_storage_ref_check(reference);
    ae_storage_ref_close(&reference);
    if (result == 0 && !valid(&value)) result = -ERANGE;
    if (result == 0) *output = value;
    return result;
}

static int registry(int fd)
{
    struct ae_root_identity identity;
    int result = ae_root_observe(fd, &identity);
    if (result == 0 && (identity.uid != geteuid() || (identity.mode & 07777) != 0700)) result = -EACCES;
    return result;
}

static int read_record(int registry_fd, const char *name, uint8_t bytes[AE_ALLOCATION_BYTES])
{
    int fd = beneath(registry_fd, name, O_RDONLY | O_NONBLOCK);
    if (fd < 0) return fd;
    struct stat status;
    int result = fstat(fd, &status) == 0 ? 0 : -errno;
    if (result == 0 && (!S_ISREG(status.st_mode) || status.st_nlink != 1 || status.st_uid != geteuid() ||
        (status.st_mode & 07777) != 0600 || status.st_size != AE_ALLOCATION_BYTES)) result = -EACCES;
    size_t used = 0;
    uint8_t buffer[AE_ALLOCATION_BYTES + 1U];
    while (result == 0 && used < sizeof buffer) {
        ssize_t count = read(fd, buffer + used, sizeof buffer - used);
        if (count < 0) { if (errno != EINTR) result = -errno; }
        else if (count == 0) break;
        else used += (size_t)count;
    }
    close(fd);
    if (result == 0 && used != AE_ALLOCATION_BYTES) result = -EBADMSG;
    if (result == 0) memcpy(bytes, buffer, AE_ALLOCATION_BYTES);
    return result;
}

int ae_allocation_publish(int registry_fd, int anchor_fd, int root_fd, const struct ae_owner *owner)
{
    int result = registry(registry_fd);
    struct ae_allocation value;
    if (result == 0) result = ae_allocation_capture(anchor_fd, root_fd, owner, &value);
    uint8_t bytes[AE_ALLOCATION_BYTES], existing[AE_ALLOCATION_BYTES];
    if (result == 0) result = ae_allocation_encode(&value, bytes, sizeof bytes);
    if (result != 0) return result;
    char name[33]; key(owner, name);
    result = read_record(registry_fd, name, existing);
    if (result == 0) {
        if (memcmp(bytes, existing, sizeof bytes) != 0) return -ESTALE;
        return fsync(registry_fd) == 0 ? 0 : -errno;
    }
    if (result != -ENOENT) return result;
    int fd = beneath(registry_fd, ".", O_TMPFILE | O_WRONLY);
    if (fd < 0) return fd;
    if (fchmod(fd, 0600) != 0) result = -errno;
    else result = 0;
    for (size_t used = 0; result == 0 && used < sizeof bytes;) {
        ssize_t count = write(fd, bytes + used, sizeof bytes - used);
        if (count < 0) { if (errno != EINTR) result = -errno; }
        else if (count == 0) result = -EIO;
        else used += (size_t)count;
    }
    if (result == 0 && fsync(fd) != 0) result = -errno;
    char source[64];
    int length = snprintf(source, sizeof source, "/proc/self/fd/%d", fd);
    if (result == 0 && (length < 0 || (size_t)length >= sizeof source)) result = -EOVERFLOW;
    if (result == 0 && linkat(AT_FDCWD, source, registry_fd, name, AT_SYMLINK_FOLLOW) != 0) {
        if (errno != EEXIST) result = -errno;
        else {
            result = read_record(registry_fd, name, existing);
            if (result == 0 && memcmp(bytes, existing, sizeof bytes) != 0) result = -ESTALE;
        }
    }
    if (result == 0 && fsync(registry_fd) != 0) result = -errno;
    /* Anonymous inode disappears on failure before publication. A close error
     * after link is an uncertain acknowledgement, never a reason to unlink. */
    if (close(fd) != 0 && result == 0) result = -errno;
    return result;
}

static int matches(const struct ae_allocation_object *expected, const struct ae_root_identity *actual)
{
    return actual->birth_present && actual->birth_seconds >= 0 && expected->inode == actual->inode &&
           expected->birth_seconds == (uint64_t)actual->birth_seconds && expected->birth_nanoseconds == actual->birth_nanoseconds;
}

int ae_allocation_open(int registry_fd, int anchor_fd, const struct ae_owner *owner, struct ae_storage_ref **output)
{
    uint8_t encoded_owner[AE_OWNER_BYTES];
    if (owner == NULL || output == NULL || ae_owner_encode(owner, encoded_owner, sizeof encoded_owner) != 0) return -EINVAL;
    int result = registry(registry_fd);
    char name[33]; key(owner, name);
    uint8_t bytes[AE_ALLOCATION_BYTES];
    if (result == 0) result = read_record(registry_fd, name, bytes);
    struct ae_allocation value;
    if (result == 0) result = ae_allocation_decode(bytes, sizeof bytes, &value);
    if (result == 0 && !ae_owner_matches(&value.owner, owner)) result = -ESTALE;
    struct ae_root_identity anchor, root;
    if (result == 0) result = ae_root_observe(anchor_fd, &anchor);
    if (result == 0 && (!matches(&value.anchor, &anchor) || anchor.filesystem_type < 0 ||
        value.filesystem_type != (uint64_t)anchor.filesystem_type ||
        memcmp(value.filesystem_id, anchor.filesystem_id, sizeof value.filesystem_id) != 0)) result = -ESTALE;
    int fd = -1;
    if (result == 0) {
        fd = beneath(anchor_fd, name, O_RDONLY | O_DIRECTORY);
        if (fd < 0) result = fd;
    }
    if (result == 0) result = ae_root_observe(fd, &root);
    if (result == 0 && (!matches(&value.root, &root) || root.filesystem_type < 0 ||
        value.filesystem_type != (uint64_t)root.filesystem_type ||
        memcmp(value.filesystem_id, root.filesystem_id, sizeof value.filesystem_id) != 0)) result = -ESTALE;
    if (result == 0) result = ae_storage_ref_open(anchor_fd, geteuid(), owner, &root, output);
    if (fd >= 0) close(fd);
    return result;
}
