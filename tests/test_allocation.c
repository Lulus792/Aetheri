#include "core/allocation.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "allocation line %d: %s\n", __LINE__, #x); exit(1); } } while (0)
static int sync_fault, no_birth;
int __real_close(int fd);
int __wrap_close(int fd)
{
    struct stat status;
    int named_writer = sync_fault == 5 && fstat(fd, &status) == 0 && S_ISREG(status.st_mode) &&
        status.st_nlink == 1 && status.st_size == AE_ALLOCATION_BYTES && (fcntl(fd, F_GETFL) & O_ACCMODE) == O_WRONLY;
    int result = __real_close(fd);
    if (named_writer) { errno = EIO; return -1; }
    return result;
}
int __real_fsync(int fd);
int __wrap_fsync(int fd)
{
    struct stat status; CHECK(fstat(fd, &status) == 0);
    if (sync_fault == 1 && S_ISREG(status.st_mode)) { errno = EIO; return -1; }
    int result = __real_fsync(fd);
    CHECK(result == 0);
    if (sync_fault == 2 && S_ISDIR(status.st_mode)) { errno = EIO; return -1; }
    if (sync_fault == 3 && S_ISREG(status.st_mode)) _exit(71);
    if (sync_fault == 4 && S_ISDIR(status.st_mode)) _exit(72);
    return result;
}
int __real_statx(int fd, const char *path, int flags, unsigned int mask, struct statx *status);
int __wrap_statx(int fd, const char *path, int flags, unsigned int mask, struct statx *status)
{
    int result = __real_statx(fd, path, flags, mask, status);
    if (result == 0 && no_birth) status->stx_mask &= ~STATX_BTIME;
    return result;
}

static struct ae_owner owner(void)
{
    struct ae_owner value = {0};
    for (size_t n = 0; n < AE_OWNER_FIELDS; ++n) {
        value.id[n][6] = 0x40; value.id[n][8] = 0x80; value.id[n][15] = (uint8_t)(n + 2U);
    }
    return value;
}

static void codec(const char *path)
{
    uint8_t frozen[AE_ALLOCATION_BYTES], encoded[AE_ALLOCATION_BYTES + 1U];
    FILE *input = fopen(path, "rb");
    CHECK(input != NULL && fread(frozen, 1, sizeof frozen, input) == sizeof frozen && fgetc(input) == EOF && fclose(input) == 0);
    struct ae_allocation value = {.owner = owner(), .filesystem_type = 0xef53,
        .filesystem_id = {1,2,3,4,5,6,7,8}, .anchor = {17,1700000000,23}, .root = {19,1700000001,29}};
    memset(encoded, 0xa5, sizeof encoded);
    CHECK(ae_allocation_encode(&value, encoded, sizeof encoded) == 0 && memcmp(encoded, frozen, sizeof frozen) == 0 && encoded[AE_ALLOCATION_BYTES] == 0xa5);
    struct ae_allocation actual, unchanged;
    memset(&unchanged, 0xa5, sizeof unchanged);
    for (size_t n = 0; n < AE_ALLOCATION_BYTES; ++n) {
        memcpy(&actual, &unchanged, sizeof actual);
        CHECK(ae_allocation_decode(frozen, n, &actual) == -EBADMSG && memcmp(&actual, &unchanged, sizeof actual) == 0);
    }
    CHECK(ae_allocation_decode(frozen, sizeof frozen, &actual) == 0);
    CHECK(actual.anchor.inode == 17 && actual.anchor.birth_seconds == 1700000000 && actual.anchor.birth_nanoseconds == 23);
    CHECK(actual.root.inode == 19 && actual.root.birth_seconds == 1700000001 && actual.root.birth_nanoseconds == 29);
    CHECK(ae_owner_matches(&actual.owner, &value.owner));
    CHECK(ae_allocation_encode(&actual, encoded, sizeof encoded) == 0 && memcmp(encoded, frozen, sizeof frozen) == 0);
    for (int fault = 0; fault < 7; ++fault) {
        struct ae_allocation bad = value;
        switch (fault) {
        case 0: bad.filesystem_type = 0; break;
        case 1: bad.anchor.inode = 0; break;
        case 2: bad.root.inode = bad.anchor.inode; break;
        case 3: bad.root.birth_seconds = UINT64_MAX; break;
        case 4: bad.anchor.birth_nanoseconds = 1000000000U; break;
        case 5: bad.root.birth_nanoseconds = 1000000000U; break;
        default: memset(bad.owner.id[AE_ALLOCATION], 0, AE_ID_BYTES); break;
        }
        memset(encoded, 0xa5, sizeof encoded);
        CHECK(ae_allocation_encode(&bad, encoded, sizeof encoded) == -EINVAL && encoded[0] == 0xa5);
    }
    memcpy(encoded, frozen, sizeof frozen); encoded[7] = '2';
    CHECK(ae_allocation_decode(encoded, sizeof frozen, &actual) == -EBADMSG);
    CHECK(ae_allocation_decode(frozen, sizeof frozen + 1U, &actual) == -EBADMSG);
    puts("allocation codec: independent frozen bytes, exact owners, birth/size/range checks and preserved failed output");
}

static void create(int directory, const char *name, const void *bytes, size_t size)
{
    int fd = openat(directory, name, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
    CHECK(fd >= 0 && write(fd, bytes, size) == (ssize_t)size && close(fd) == 0);
}

static int entries(int directory)
{
    int fd = openat(directory, ".", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    DIR *scan = fdopendir(fd); CHECK(scan != NULL);
    int count = 0; struct dirent *entry;
    while ((entry = readdir(scan)) != NULL) if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) ++count;
    CHECK(closedir(scan) == 0); return count;
}

static void basic(const char *build)
{
    char parent[PATH_MAX], temporary[PATH_MAX]; CHECK(realpath(build, parent) != NULL);
    int length = snprintf(temporary, sizeof temporary, "%s/allocation-XXXXXX", parent);
    CHECK(length > 0 && (size_t)length < sizeof temporary && mkdtemp(temporary) != NULL);
    int base = open(temporary, O_RDONLY | O_DIRECTORY | O_CLOEXEC); CHECK(base >= 0);
    CHECK(mkdirat(base, "registry", 0700) == 0 && mkdirat(base, "data", 0700) == 0);
    int registry = openat(base, "registry", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    int anchor = openat(base, "data", O_RDONLY | O_DIRECTORY | O_CLOEXEC); CHECK(registry >= 0 && anchor >= 0);
    const char key[] = "00000000000040008000000000000005";
    CHECK(mkdirat(anchor, key, 0700) == 0);
    int root = openat(anchor, key, O_RDONLY | O_DIRECTORY | O_CLOEXEC); CHECK(root >= 0);
    struct ae_owner expected = owner(); uint8_t header[AE_OWNER_BYTES];
    CHECK(ae_owner_encode(&expected, header, sizeof header) == 0); create(root, "owner", header, sizeof header);
    CHECK(mkdirat(root, "payload", 0700) == 0);
    int payload = openat(root, "payload", O_RDONLY | O_DIRECTORY | O_CLOEXEC); CHECK(payload >= 0);
    create(payload, "document", "hello\n", 6);
    struct ae_storage_ref *reference = NULL;
    CHECK(ae_allocation_open(registry, anchor, &expected, &reference) == -ENOENT && reference == NULL);
    no_birth = 1;
    CHECK(ae_allocation_publish(registry, anchor, root, &expected) == -ENOTSUP && entries(registry) == 0);
    no_birth = 0;
    for (int fault = 1; fault <= 5; ++fault) {
        if (fault <= 2 || fault == 5) {
            sync_fault = fault;
            CHECK(ae_allocation_publish(registry, anchor, root, &expected) == -EIO);
            sync_fault = 0;
        } else {
            pid_t child = fork(); CHECK(child >= 0);
            if (child == 0) { sync_fault = fault; ae_allocation_publish(registry, anchor, root, &expected); _exit(1); }
            int status;
            CHECK(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == (fault == 3 ? 71 : 72));
        }
        int committed = fault == 2 || fault == 4 || fault == 5;
        CHECK(entries(registry) == committed);
        CHECK(ae_allocation_open(registry, anchor, &expected, &reference) == (committed ? 0 : -ENOENT));
        CHECK(ae_storage_ref_close(&reference) == 0);
        CHECK(ae_allocation_publish(registry, anchor, root, &expected) == 0 && entries(registry) == 1);
        struct stat before, after; CHECK(fstatat(registry, key, &before, 0) == 0);
        CHECK(ae_allocation_publish(registry, anchor, root, &expected) == 0 && fstatat(registry, key, &after, 0) == 0);
        CHECK(before.st_ino == after.st_ino && before.st_mtim.tv_sec == after.st_mtim.tv_sec && before.st_mtim.tv_nsec == after.st_mtim.tv_nsec);
        CHECK(unlinkat(registry, key, 0) == 0);
    }
    pid_t children[2];
    for (size_t n = 0; n < 2; ++n) {
        children[n] = fork(); CHECK(children[n] >= 0);
        if (children[n] == 0) _exit(ae_allocation_publish(registry, anchor, root, &expected) == 0 ? 0 : 1);
    }
    for (size_t n = 0; n < 2; ++n) { int status; CHECK(waitpid(children[n], &status, 0) == children[n] && WIFEXITED(status) && WEXITSTATUS(status) == 0); }
    CHECK(entries(registry) == 1);
    for (int field = AE_MACHINE; field < AE_OWNER_FIELDS; ++field) {
        struct ae_owner wrong = expected; wrong.id[field][15] ^= 1U;
        CHECK(ae_allocation_open(registry, anchor, &wrong, &reference) < 0 && reference == NULL);
    }
    CHECK(ae_allocation_open(registry, anchor, &expected, &reference) == 0);
    int document = ae_storage_ref_read(reference, "document", 8, 65536); CHECK(document >= 0 && close(document) == 0);
    CHECK(ae_storage_ref_close(&reference) == 0);
    CHECK(fchmodat(registry, key, 0644, 0) == 0 && ae_allocation_open(registry, anchor, &expected, &reference) == -EACCES);
    CHECK(fchmodat(registry, key, 0600, 0) == 0);
    CHECK(linkat(registry, key, registry, "alias", 0) == 0 && ae_allocation_open(registry, anchor, &expected, &reference) == -EACCES);
    CHECK(unlinkat(registry, "alias", 0) == 0);
    CHECK(renameat(anchor, key, anchor, "original") == 0 && mkdirat(anchor, key, 0700) == 0);
    int clone = openat(anchor, key, O_RDONLY | O_DIRECTORY | O_CLOEXEC); CHECK(clone >= 0); create(clone, "owner", header, sizeof header);
    CHECK(ae_allocation_open(registry, anchor, &expected, &reference) == -ESTALE);
    CHECK(ae_allocation_publish(registry, anchor, clone, &expected) == -ESTALE);
    CHECK(unlinkat(clone, "owner", 0) == 0 && close(clone) == 0 && unlinkat(anchor, key, AT_REMOVEDIR) == 0);
    CHECK(renameat(anchor, "original", anchor, key) == 0);
    CHECK(close(registry) == 0 && close(anchor) == 0);
    registry = openat(base, "registry", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    anchor = openat(base, "data", O_RDONLY | O_DIRECTORY | O_CLOEXEC); CHECK(registry >= 0 && anchor >= 0);
    CHECK(ae_allocation_open(registry, anchor, &expected, &reference) == 0 && ae_storage_ref_close(&reference) == 0);
    CHECK(unlinkat(registry, key, 0) == 0 && close(registry) == 0 && unlinkat(base, "registry", AT_REMOVEDIR) == 0);
    CHECK(unlinkat(payload, "document", 0) == 0 && close(payload) == 0 && unlinkat(root, "payload", AT_REMOVEDIR) == 0);
    CHECK(unlinkat(root, "owner", 0) == 0 && close(root) == 0 && unlinkat(anchor, key, AT_REMOVEDIR) == 0);
    CHECK(close(anchor) == 0 && unlinkat(base, "data", AT_REMOVEDIR) == 0 && close(base) == 0 && rmdir(temporary) == 0);
    puts("allocation IO: immutable publication, real reopen/concurrent retries, pre/post-link faults, copied-root denial; volume authentication remains external");
}

int main(int argc, char **argv)
{
    CHECK(argc == 3);
    if (strcmp(argv[1], "codec") == 0) codec(argv[2]);
    else { CHECK(strcmp(argv[1], "basic") == 0); basic(argv[2]); }
    return 0;
}
