#include "core/import.h"
#include "core/fixture.h"

#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "import line %d: %s\n", __LINE__, #x); exit(1); } } while (0)

static const char *fault = "none";
static ino_t stage_inode;
static int stage_fd, source_fd;
static unsigned int files_synced, directories_synced, regular_opens;

static unsigned int descriptor_count(void)
{
    DIR *stream = opendir("/proc/self/fd");
    CHECK(stream != NULL);
    unsigned int count = 0;
    while (readdir(stream) != NULL) ++count;
    CHECK(closedir(stream) == 0);
    return count;
}

int __real_open(const char *, int, ...);
int __wrap_open(const char *path, int flags, ...)
{
    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list arguments;
        va_start(arguments, flags);
        mode = va_arg(arguments, mode_t);
        va_end(arguments);
    }
    if (strncmp(path, "/proc/self/fd/", 14) == 0) ++regular_opens;
    return __real_open(path, flags, mode);
}

ssize_t __real_pread(int, void *, size_t, off_t);
ssize_t __wrap_pread(int fd, void *bytes, size_t size, off_t offset)
{
    if (strcmp(fault, "read-eintr") == 0) { fault = "none"; errno = EINTR; return -1; }
    if (strcmp(fault, "read-short") == 0 && size > 3) size = 3;
    if (strcmp(fault, "read-eof") == 0) return 0;
    if (strcmp(fault, "read-io") == 0) { errno = EIO; return -1; }
    return __real_pread(fd, bytes, size, offset);
}

ssize_t __real_write(int, const void *, size_t);
ssize_t __wrap_write(int fd, const void *bytes, size_t size)
{
    if (strcmp(fault, "write-eintr") == 0) { fault = "none"; errno = EINTR; return -1; }
    if (strcmp(fault, "write-short") == 0 && size > 3) size = 3;
    if (strcmp(fault, "write-zero") == 0) return 0;
    if (strcmp(fault, "write-io") == 0) { errno = EIO; return -1; }
    if (strcmp(fault, "copy-corrupt") == 0 && size > 0) {
        uint8_t changed = ((const uint8_t *)bytes)[0] ^ 1U;
        fault = "none";
        return __real_write(fd, &changed, 1);
    }
    if (strcmp(fault, "source-rebind") == 0) {
        fault = "none";
        int parent = openat(source_fd, "bin", O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
        CHECK(parent >= 0 && fchmod(parent, 0755) == 0);
        struct stat original;
        CHECK(fstatat(parent, "probe", &original, AT_SYMLINK_NOFOLLOW) == 0);
        CHECK(S_ISREG(original.st_mode) && original.st_size >= 0 && original.st_size <= 16 * 1024 * 1024);
        CHECK(renameat(parent, "probe", parent, "old") == 0);
        int replacement = openat(parent, "probe", O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0555);
        CHECK(replacement >= 0);
        uint8_t changed[4096];
        memset(changed, 'X', sizeof changed);
        off_t position = 0;
        while (position < original.st_size) {
            size_t count = sizeof changed;
            if (original.st_size - position < (off_t)count) count = (size_t)(original.st_size - position);
            ssize_t written = __real_write(replacement, changed, count);
            CHECK(written > 0);
            position += (off_t)written;
        }
        CHECK(close(replacement) == 0 && fchmod(parent, 0555) == 0 && close(parent) == 0);
    }
    return __real_write(fd, bytes, size);
}

int __real_fsync(int);
int __wrap_fsync(int fd)
{
    struct stat info;
    CHECK(fstat(fd, &info) == 0);
    if (S_ISDIR(info.st_mode)) ++directories_synced; else ++files_synced;
    if (strcmp(fault, "file-sync") == 0 && S_ISREG(info.st_mode)) { errno = EIO; return -1; }
    if (strcmp(fault, "root-sync") == 0 && info.st_ino == stage_inode) { errno = EIO; return -1; }
    return __real_fsync(fd);
}

int __real_close(int);
int __wrap_close(int fd)
{
    int selected = strcmp(fault, "writer-close") == 0 && (fcntl(fd, F_GETFL) & O_ACCMODE) == O_RDWR;
    int result = __real_close(fd);
    if (selected) { fault = "none"; errno = EIO; return -1; }
    return result;
}

static uint8_t nibble(char value)
{
    if (value >= '0' && value <= '9') return (uint8_t)(value - '0');
    CHECK(value >= 'a' && value <= 'f');
    return (uint8_t)(value - 'a' + 10);
}

int main(int argc, char **argv)
{
    CHECK(argc == 7);
    uint8_t expected[32];
    CHECK(strlen(argv[5]) == 64);
    for (size_t n = 0; n < sizeof expected; ++n) expected[n] = (uint8_t)((nibble(argv[5][n * 2U]) << 4) | nibble(argv[5][n * 2U + 1U]));
    uint8_t *manifest = malloc(AE_FIXTURE_MAX_BYTES + 1U);
    CHECK(manifest != NULL);
    FILE *input = fopen(argv[4], "rb");
    CHECK(input != NULL);
    size_t size = fread(manifest, 1, AE_FIXTURE_MAX_BYTES + 1U, input);
    CHECK(!ferror(input) && fclose(input) == 0);
    source_fd = open(argv[2], O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
    stage_fd = open(argv[3], O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
    CHECK(source_fd >= 0 && stage_fd >= 0);
    struct stat info;
    CHECK(fstat(stage_fd, &info) == 0);
    stage_inode = info.st_ino;
    off_t source_offset = lseek(source_fd, 0, SEEK_CUR), stage_offset = lseek(stage_fd, 0, SEEK_CUR);
    unsigned int descriptors = descriptor_count();
    fault = argv[6];
    int result = strcmp(argv[1], "sync") == 0 ? ae_fixture_sync_staged_payload(stage_fd, manifest, size, expected) :
        ae_fixture_stage_payload(source_fd, stage_fd, manifest, size, expected);
    fault = "none";
    CHECK(descriptor_count() == descriptors);
    CHECK(fcntl(source_fd, F_GETFD) >= 0 && fcntl(stage_fd, F_GETFD) >= 0);
    CHECK(lseek(source_fd, 0, SEEK_CUR) == source_offset && lseek(stage_fd, 0, SEEK_CUR) == stage_offset);
    CHECK(close(source_fd) == 0 && close(stage_fd) == 0);
    free(manifest);
    printf("%d %u %u %u\n", result, files_synced, directories_synced, regular_opens);
    return 0;
}
