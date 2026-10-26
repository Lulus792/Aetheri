#include "core/root.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/openat2.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/syscall.h>
#include <unistd.h>

static int directory(int fd, struct statx *status)
{
    const unsigned int required = STATX_TYPE | STATX_MODE | STATX_UID | STATX_GID | STATX_INO | STATX_MNT_ID_UNIQUE;
    int result;
    do { result = statx(fd, "", AT_EMPTY_PATH | AT_SYMLINK_NOFOLLOW, required | STATX_BTIME, status); } while (result < 0 && errno == EINTR);
    if (result < 0) return -errno;
    if ((status->stx_mask & required) != required || status->stx_mnt_id == 0) return -ENOTSUP;
    if ((status->stx_mask & STATX_BTIME) && status->stx_btime.tv_nsec >= 1000000000U) return -EBADMSG;
    return S_ISDIR(status->stx_mode) ? 0 : -ENOTDIR;
}

static int equal_stat(const struct statx *left, const struct statx *right)
{
    return left->stx_mnt_id == right->stx_mnt_id && left->stx_ino == right->stx_ino &&
           left->stx_dev_major == right->stx_dev_major && left->stx_dev_minor == right->stx_dev_minor &&
           left->stx_uid == right->stx_uid && left->stx_gid == right->stx_gid && left->stx_mode == right->stx_mode &&
           ((left->stx_mask ^ right->stx_mask) & STATX_BTIME) == 0 &&
           (!(left->stx_mask & STATX_BTIME) || (left->stx_btime.tv_sec == right->stx_btime.tv_sec && left->stx_btime.tv_nsec == right->stx_btime.tv_nsec));
}

int ae_root_observe(int fd, struct ae_root_identity *output)
{
    if (fd < 0 || output == NULL) return -EINVAL;
    struct statx first, second;
    int result = directory(fd, &first);
    struct statfs filesystem;
    if (result == 0 && fstatfs(fd, &filesystem) != 0) result = -errno;
    if (result == 0) result = directory(fd, &second);
    if (result == 0 && !equal_stat(&first, &second)) result = -ESTALE;
    if (result != 0) return result;
    _Static_assert(sizeof filesystem.f_fsid == 8, "Linux x86-64 filesystem-ID observation");
    struct ae_root_identity value = {.mount_id = first.stx_mnt_id, .inode = first.stx_ino,
        .device_major = first.stx_dev_major, .device_minor = first.stx_dev_minor,
        .uid = first.stx_uid, .gid = first.stx_gid, .mode = first.stx_mode,
        .filesystem_type = (int64_t)filesystem.f_type};
    memcpy(value.filesystem_id, &filesystem.f_fsid, sizeof value.filesystem_id);
    if (first.stx_mask & STATX_BTIME) {
        value.birth_present = 1;
        value.birth_seconds = first.stx_btime.tv_sec;
        value.birth_nanoseconds = first.stx_btime.tv_nsec;
    }
    *output = value;
    return 0;
}

int ae_root_equal(const struct ae_root_identity *left, const struct ae_root_identity *right)
{
    return left != NULL && right != NULL && left->mount_id == right->mount_id && left->inode == right->inode &&
           left->device_major == right->device_major && left->device_minor == right->device_minor &&
           left->uid == right->uid && left->gid == right->gid && left->mode == right->mode &&
           left->filesystem_type == right->filesystem_type && left->birth_present == right->birth_present &&
           left->birth_seconds == right->birth_seconds && left->birth_nanoseconds == right->birth_nanoseconds &&
           memcmp(left->filesystem_id, right->filesystem_id, sizeof left->filesystem_id) == 0;
}

int ae_root_check_path(int fd, const char *absolute, const struct ae_root_identity *expected)
{
    if (fd < 0 || absolute == NULL || absolute[0] != '/' || expected == NULL) return -EINVAL;
    struct ae_root_identity retained, named;
    int result = ae_root_observe(fd, &retained);
    if (result == 0 && !ae_root_equal(&retained, expected)) result = -ESTALE;
    if (result != 0) return result;
    struct open_how how = {.flags = O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW,
                          .resolve = RESOLVE_NO_SYMLINKS | RESOLVE_NO_MAGICLINKS};
    int reopened;
    do { reopened = (int)syscall(SYS_openat2, AT_FDCWD, absolute, &how, sizeof how); } while (reopened < 0 && errno == EINTR);
    if (reopened < 0) return errno == ENOENT || errno == ENOTDIR || errno == ELOOP ? -ESTALE : -errno;
    result = ae_root_observe(reopened, &named);
    if (result == 0 && !ae_root_equal(&named, expected)) result = -ESTALE;
    close(reopened);
    return result;
}
