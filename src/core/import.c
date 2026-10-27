#include "core/import.h"
#include "core/fixture.h"
#include "core/root.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/openat2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/xattr.h>
#include <unistd.h>

struct directory { char path[257]; struct stat observed; int seen; };
struct input_file { int fd; struct stat observed; };
struct inventory {
    struct ae_fixture fixture;
    struct directory *directories;
    size_t count;
    struct input_file files[AE_FIXTURE_FILES];
};

static int contained(int root, const char *path, uint64_t flags, uint64_t mode)
{
    struct open_how how = {0};
    how.flags = flags | O_CLOEXEC | O_NOFOLLOW;
    how.mode = mode;
    how.resolve = RESOLVE_BENEATH | RESOLVE_NO_SYMLINKS | RESOLVE_NO_MAGICLINKS | RESOLVE_NO_XDEV;
    int fd;
    do { fd = (int)syscall(SYS_openat2, root, path[0] == '\0' ? "." : path, &how, sizeof how); }
    while (fd < 0 && errno == EINTR);
    return fd < 0 ? -errno : fd;
}

static int release(int fd, int result)
{
    if (fd >= 0 && close(fd) != 0 && result == 0) result = -errno;
    return result;
}

static int root_unchanged(int fd, const struct ae_root_identity *expected)
{
    struct ae_root_identity current;
    int result = ae_root_observe(fd, &current);
    if (result == 0 && !ae_root_equal(&current, expected)) result = -ESTALE;
    return result;
}

static int metadata_equal(const struct stat *left, const struct stat *right)
{
    return left->st_dev == right->st_dev && left->st_ino == right->st_ino && left->st_mode == right->st_mode &&
        left->st_uid == right->st_uid && left->st_gid == right->st_gid && left->st_nlink == right->st_nlink &&
        left->st_size == right->st_size && left->st_mtim.tv_sec == right->st_mtim.tv_sec &&
        left->st_mtim.tv_nsec == right->st_mtim.tv_nsec && left->st_ctim.tv_sec == right->st_ctim.tv_sec &&
        left->st_ctim.tv_nsec == right->st_ctim.tv_nsec;
}

static int attributes(int fd)
{
    char names[1024];
    ssize_t count = flistxattr(fd, names, sizeof names);
    if (count < 0) return -errno;
    size_t used = 0;
    while (used < (size_t)count) {
        const char *end = memchr(names + used, '\0', (size_t)count - used);
        if (end == NULL || strcmp(names + used, "security.selinux") != 0) return -EACCES;
        used += (size_t)(end - (names + used)) + 1U;
    }
    return 0;
}

static int directory_order(const void *left, const void *right)
{
    const struct directory *a = left, *b = right;
    return strcmp(a->path, b->path);
}

static int prepare(struct inventory *inventory, const uint8_t *manifest, size_t size, const uint8_t expected[32])
{
    for (size_t n = 0; n < AE_FIXTURE_FILES; ++n) inventory->files[n].fd = -1;
    int result = ae_fixture_verify_manifest(manifest, size, expected, &inventory->fixture);
    if (result != 0) return result;
    size_t capacity = 1;
    for (uint32_t n = 0; n < inventory->fixture.file_count; ++n)
        for (size_t p = 0; p < inventory->fixture.files[n].path.size; ++p)
            if (inventory->fixture.files[n].path.data[p] == '/') ++capacity;
    /* Canonical bounds imply <=32*128 prefixes; never recurse or retain an FD
     * per directory. This also accepts the deepest valid 256-byte paths. */
    if (capacity > 1U + AE_FIXTURE_FILES * 128U) return -E2BIG;
    inventory->directories = calloc(capacity, sizeof *inventory->directories);
    if (inventory->directories == NULL) return -ENOMEM;
    size_t count = 1;
    for (uint32_t n = 0; n < inventory->fixture.file_count; ++n) {
        struct ae_fixture_text path = inventory->fixture.files[n].path;
        for (size_t p = 0; p < path.size; ++p) if (path.data[p] == '/')
            memcpy(inventory->directories[count++].path, path.data, p);
    }
    qsort(inventory->directories, count, sizeof *inventory->directories, directory_order);
    inventory->count = 1;
    for (size_t n = 1; n < count; ++n)
        if (strcmp(inventory->directories[n].path, inventory->directories[inventory->count - 1U].path) != 0)
            inventory->directories[inventory->count++] = inventory->directories[n];
    return 0;
}

static int dispose(struct inventory *inventory, int result)
{
    for (size_t n = 0; n < AE_FIXTURE_FILES; ++n) {
        result = release(inventory->files[n].fd, result);
        inventory->files[n].fd = -1;
    }
    free(inventory->directories);
    return result;
}

static size_t find_directory(const struct inventory *inventory, const char *path)
{
    size_t low = 0, high = inventory->count;
    while (low < high) {
        size_t middle = low + (high - low) / 2U;
        int order = strcmp(inventory->directories[middle].path, path);
        if (order == 0) return middle;
        if (order < 0) low = middle + 1U; else high = middle;
    }
    return inventory->count;
}

/* O_PATH classifies the pinned object before any content/device open. Reopening
 * its own proc FD then addresses that same regular inode, never a source name. */
static int regular(int root, const char *path, const struct ae_fixture_file *file, struct input_file *output)
{
    int pin = contained(root, path, O_PATH, 0);
    if (pin < 0) return pin;
    struct stat before, after;
    int result = 0, fd = -1;
    if (fstat(pin, &before) != 0) result = -errno;
    else if (!S_ISREG(before.st_mode) || before.st_nlink != 1 || before.st_size < 0 || (uint64_t)before.st_size != file->size) result = -EBADMSG;
    else if (before.st_uid != geteuid() || (before.st_mode & 07777) != file->mode) result = -EACCES;
    if (result == 0) {
        char proc[64];
        int length = snprintf(proc, sizeof proc, "/proc/self/fd/%d", pin);
        if (length < 0 || (size_t)length >= sizeof proc) result = -EOVERFLOW;
        else {
            fd = open(proc, O_RDONLY | O_CLOEXEC | O_NONBLOCK);
            if (fd < 0) result = -errno;
        }
    }
    if (result == 0) {
        if (fstat(fd, &after) != 0) result = -errno;
        else if (!metadata_equal(&before, &after)) result = -ESTALE;
        else result = attributes(fd);
    }
    result = release(pin, result);
    if (result != 0) return release(fd, result);
    output->fd = fd;
    output->observed = after;
    return 0;
}

static int scan(int root, mode_t root_mode, struct inventory *inventory)
{
    for (size_t n = 0; n < inventory->count; ++n) inventory->directories[n].seen = n == 0;
    for (size_t n = 0; n < inventory->count; ++n) {
        struct directory *directory = &inventory->directories[n];
        if (!directory->seen) return -EBADMSG;
        int fd = contained(root, directory->path, O_RDONLY | O_DIRECTORY, 0);
        if (fd < 0) return fd;
        struct stat info;
        int result = fstat(fd, &info) == 0 ? 0 : -errno;
        if (result == 0 && (info.st_uid != geteuid() || (info.st_mode & 07777) != (n == 0 ? root_mode : 0555))) result = -EACCES;
        if (result == 0) result = attributes(fd);
        if (result != 0) return release(fd, result);
        directory->observed = info;
        DIR *stream = fdopendir(fd);
        if (stream == NULL) return release(fd, -errno);
        for (;;) {
            errno = 0;
            struct dirent *entry = readdir(stream);
            if (entry == NULL) { if (errno != 0) result = -errno; break; }
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
            char path[257];
            int length = snprintf(path, sizeof path, "%s%s%s", directory->path, n == 0 ? "" : "/", entry->d_name);
            if (length < 0 || (size_t)length >= sizeof path) { result = -EBADMSG; break; }
            size_t child = find_directory(inventory, path);
            if (child != inventory->count) {
                if (inventory->directories[child].seen) { result = -EBADMSG; break; }
                inventory->directories[child].seen = 1;
            } else {
                uint32_t file = 0;
                while (file < inventory->fixture.file_count &&
                    (inventory->fixture.files[file].path.size != (size_t)length ||
                     memcmp(inventory->fixture.files[file].path.data, path, (size_t)length) != 0)) ++file;
                if (file == inventory->fixture.file_count || inventory->files[file].fd >= 0) { result = -EBADMSG; break; }
                result = regular(root, path, &inventory->fixture.files[file], &inventory->files[file]);
                if (result != 0) break;
            }
        }
        if (closedir(stream) != 0 && result == 0) result = -errno;
        if (result != 0) return result;
    }
    for (uint32_t n = 0; n < inventory->fixture.file_count; ++n) if (inventory->files[n].fd < 0) return -EBADMSG;
    return 0;
}

static int recheck(int root, struct inventory *inventory)
{
    for (size_t n = 0; n < inventory->count; ++n) {
        int fd = contained(root, inventory->directories[n].path, O_RDONLY | O_DIRECTORY, 0);
        if (fd < 0) return fd;
        struct stat info;
        int result = fstat(fd, &info) == 0 ? 0 : -errno;
        if (result == 0 && !metadata_equal(&info, &inventory->directories[n].observed)) result = -ESTALE;
        if (result == 0) result = attributes(fd);
        result = release(fd, result);
        if (result != 0) return result;
    }
    for (uint32_t n = 0; n < inventory->fixture.file_count; ++n) {
        char path[257];
        memcpy(path, inventory->fixture.files[n].path.data, inventory->fixture.files[n].path.size);
        path[inventory->fixture.files[n].path.size] = '\0';
        int fd = contained(root, path, O_PATH, 0);
        if (fd < 0) return fd;
        struct stat info;
        int result = fstat(fd, &info) == 0 ? 0 : -errno;
        if (result == 0 && !metadata_equal(&info, &inventory->files[n].observed)) result = -ESTALE;
        result = release(fd, result);
        if (result != 0) return result;
    }
    return 0;
}

static int empty(int root)
{
    int fd = contained(root, "", O_RDONLY | O_DIRECTORY, 0);
    if (fd < 0) return fd;
    DIR *stream = fdopendir(fd);
    if (stream == NULL) return release(fd, -errno);
    int result = 0;
    for (;;) {
        errno = 0;
        struct dirent *entry = readdir(stream);
        if (entry == NULL) { if (errno != 0) result = -errno; break; }
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) { result = -EEXIST; break; }
    }
    if (closedir(stream) != 0 && result == 0) result = -errno;
    return result;
}

static int copy_file(int source, int stage, const struct ae_fixture_file *file)
{
    char path[257];
    memcpy(path, file->path.data, file->path.size); path[file->path.size] = '\0';
    int fd = contained(stage, path, O_RDWR | O_CREAT | O_EXCL, 0600);
    if (fd < 0) return fd;
    int result = fchmod(fd, 0600) == 0 ? attributes(fd) : -errno;
    uint8_t bytes[65536];
    uint64_t position = 0;
    while (result == 0 && position < file->size) {
        size_t count = sizeof bytes;
        if (file->size - position < count) count = (size_t)(file->size - position);
        ssize_t received = pread(source, bytes, count, (off_t)position);
        if (received < 0) { if (errno == EINTR) continue; result = -errno; }
        else if (received == 0) result = -EBADMSG;
        else if ((size_t)received > count) result = -EIO;
        else {
            size_t used = 0;
            while (result == 0 && used < (size_t)received) {
                ssize_t written = write(fd, bytes + used, (size_t)received - used);
                if (written < 0) { if (errno == EINTR) continue; result = -errno; }
                else if (written == 0 || (size_t)written > (size_t)received - used) result = -EIO;
                else used += (size_t)written;
            }
            position += (uint64_t)received;
        }
    }
    if (result == 0 && fchmod(fd, (mode_t)file->mode) != 0) result = -errno;
    if (result == 0 && fsync(fd) != 0) result = -errno;
    return release(fd, result);
}

static int finish_directories(int root, struct inventory *inventory, int seal)
{
    for (size_t n = inventory->count; n > 0; --n) {
        int fd = contained(root, inventory->directories[n - 1U].path, O_RDONLY | O_DIRECTORY, 0);
        if (fd < 0) return fd;
        int result = 0;
        if (seal && n != 1 && fchmod(fd, 0555) != 0) result = -errno;
        if (result == 0 && fsync(fd) != 0) result = -errno;
        result = release(fd, result);
        if (result != 0) return result;
    }
    return 0;
}

static int verify_payload(int staging_fd, unsigned int root_mode, const uint8_t *manifest, size_t size, const uint8_t expected[32],int sync)
{
    if (staging_fd < 0 || (root_mode != 0700 && root_mode != 0555)) return -EINVAL;
    struct inventory inventory = {0};
    int result = prepare(&inventory, manifest, size, expected);
    struct ae_root_identity root;
    if (result == 0) result = ae_root_observe(staging_fd, &root);
    if (result == 0) result = scan(staging_fd, (mode_t)root_mode, &inventory);
    for (uint32_t n = 0; result == 0 && n < inventory.fixture.file_count; ++n) {
        result = ae_fixture_verify_file(&inventory.fixture.files[n], inventory.files[n].fd);
        if (result == 0 && sync && fsync(inventory.files[n].fd) != 0) result = -errno;
    }
    if (result == 0) result = recheck(staging_fd, &inventory);
    if (result == 0 && sync) result = finish_directories(staging_fd, &inventory, 0);
    if (result == 0) result = root_unchanged(staging_fd, &root);
    return dispose(&inventory, result);
}

int ae_fixture_check_payload(int root,const uint8_t *manifest,size_t size,const uint8_t expected[32])
{ return verify_payload(root,0555,manifest,size,expected,0); }

int ae_fixture_sync_payload(int root,unsigned int mode,const uint8_t *manifest,size_t size,const uint8_t expected[32])
{ return verify_payload(root,mode,manifest,size,expected,1); }

int ae_fixture_sync_staged_payload(int staging_fd, const uint8_t *manifest, size_t size, const uint8_t expected[32])
{
    return ae_fixture_sync_payload(staging_fd, 0700, manifest, size, expected);
}

int ae_fixture_stage_payload(int source_fd, int staging_fd, const uint8_t *manifest, size_t size, const uint8_t expected[32])
{
    if (source_fd < 0 || staging_fd < 0) return -EINVAL;
    struct inventory inventory = {0};
    int result = prepare(&inventory, manifest, size, expected);
    struct ae_root_identity source, stage;
    if (result == 0) result = ae_root_observe(source_fd, &source);
    if (result == 0) result = ae_root_observe(staging_fd, &stage);
    if (result == 0 && (source.uid != geteuid() || stage.uid != geteuid() ||
        (source.mode & 07777U) != 0555 || (stage.mode & 07777U) != 0700)) result = -EACCES;
    if (result == 0 && source.inode == stage.inode && source.device_major == stage.device_major && source.device_minor == stage.device_minor) result = -EINVAL;
    if (result == 0) result = attributes(staging_fd);
    if (result == 0) result = empty(staging_fd);
    if (result == 0) result = scan(source_fd, 0555, &inventory);
    for (size_t n = 1; result == 0 && n < inventory.count; ++n) {
        if (mkdirat(staging_fd, inventory.directories[n].path, 0700) != 0) result = -errno;
    }
    for (uint32_t n = 0; result == 0 && n < inventory.fixture.file_count; ++n)
        result = copy_file(inventory.files[n].fd, staging_fd, &inventory.fixture.files[n]);
    if (result == 0) result = recheck(source_fd, &inventory);
    if (result == 0) result = root_unchanged(source_fd, &source);
    if (result == 0) result = finish_directories(staging_fd, &inventory, 1);
    result = dispose(&inventory, result);
    if (result == 0) result = ae_fixture_sync_staged_payload(staging_fd, manifest, size, expected);
    if (result == 0) result = root_unchanged(staging_fd, &stage);
    return result;
}
