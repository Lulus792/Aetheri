#define _GNU_SOURCE
#include "login/image_platform.h"
#include "core/root.h"
#include <errno.h>
#include <fcntl.h>
#include <linux/magic.h>
#include <linux/mount.h>
#include <linux/openat2.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

enum { MOUNT_RECORD_BYTES = 4096 };

static int directory(const char *path, struct ae_root_identity *identity, struct statmount *mount)
{
    const struct open_how how = {
        .flags = O_PATH | O_DIRECTORY | O_CLOEXEC,
        .resolve = RESOLVE_NO_SYMLINKS | RESOLVE_NO_MAGICLINKS,
    };
    int fd;
    do { fd = (int)syscall(SYS_openat2, AT_FDCWD, path, &how, sizeof how); }
    while (fd < 0 && errno == EINTR);
    if (fd < 0) return -errno;
    int result = ae_root_observe(fd, identity);
    if (!result && (identity->uid || identity->gid || (identity->mode & 07022U)))
        result = -EACCES;
    if (!result && mount) {
        const uint64_t mask = STATMOUNT_SB_BASIC | STATMOUNT_MNT_BASIC |
            STATMOUNT_MNT_ROOT | STATMOUNT_MNT_POINT;
        const struct mnt_id_req request = {
            .size = MNT_ID_REQ_SIZE_VER0, .mnt_id = identity->mount_id, .param = mask,
        };
        memset(mount, 0, MOUNT_RECORD_BYTES);
        long reply;
        do { reply = syscall(SYS_statmount, &request, mount, MOUNT_RECORD_BYTES, 0U); }
        while (reply < 0 && errno == EINTR);
        if (reply < 0) result = -errno;
        else if ((mount->mask & mask) != mask || mount->size > MOUNT_RECORD_BYTES ||
                 mount->size <= offsetof(struct statmount, str))
            result = -ENODATA;
        else if (mount->mnt_id != identity->mount_id || mount->sb_dev_major != identity->device_major ||
                 mount->sb_dev_minor != identity->device_minor)
            result = -ESTALE;
    }
    close(fd);
    return result;
}

static int image_directory(const char *path, struct ae_root_identity *identity,
                           struct statmount *mount)
{
    int result = directory(path, identity, mount);
    if (result) return result;
    if (identity->filesystem_type != EXT4_SUPER_MAGIC || mount->sb_magic != EXT4_SUPER_MAGIC)
        return -EPROTO;
    /* statmount reports the superblock's SB_RDONLY bit, distinct from a
       read-only bind view of a writable superblock. */
    if (!(mount->sb_flags & MS_RDONLY) || !(mount->mnt_attr & MOUNT_ATTR_RDONLY) ||
        (mount->mnt_attr & MOUNT_ATTR_IDMAP))
        return -EROFS;
    size_t length = mount->size - offsetof(struct statmount, str);
    size_t root = mount->mnt_root, point = mount->mnt_point;
    if (root >= length || point >= length) return -EBADMSG;
    const char *base = mount->str + root, *target = mount->str + point;
    if (!memchr(base, '\0', length - root) || !memchr(target, '\0', length - point))
        return -EBADMSG;
    /* Allow the service manager to bind a subtree back at its original path.
       A different on-volume subtree mounted over that path is a substitution. */
    size_t count = strlen(target);
    if (!count || *target != '/' || strcmp(base, target) ||
        strncmp(path, target, count) ||
        (count != 1 && path[count] && path[count] != '/'))
        return -EXDEV;
    return 0;
}

int ae_login_image_mount_check(void)
{
    static const char *const image_directories[] = {
        "/etc", "/etc/pam.d", "/usr", "/usr/lib64", "/usr/lib64/security",
        "/usr/lib/aetheri/runtime", "/usr/libexec/aetheri",
        "/usr/share/aetheri/login", "/usr/share/libinput",
    };
    struct statmount *mount = calloc(1, MOUNT_RECORD_BYTES);
    if (!mount) return -ENOMEM;
    struct ae_root_identity root, current;
    int result = image_directory("/", &root, mount);
    for (size_t index = 0; !result && index < sizeof image_directories / sizeof image_directories[0]; ++index) {
        result = image_directory(image_directories[index], &current, mount);
        if (!result && (current.device_major != root.device_major || current.device_minor != root.device_minor ||
                        memcmp(current.filesystem_id, root.filesystem_id, sizeof root.filesystem_id)))
            result = -EXDEV;
    }
    if (!result) result = directory("/proc", &current, NULL);
    if (!result && current.filesystem_type != PROC_SUPER_MAGIC) result = -EPROTO;
    if (!result) result = directory("/run", &current, NULL);
    if (!result && current.filesystem_type != TMPFS_MAGIC) result = -EPROTO;
    free(mount);
    return result;
}
