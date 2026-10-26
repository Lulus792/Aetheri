#include "core/owner.h"
#include "core/storage.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <unistd.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "line %d: %s\n", __LINE__, #x); exit(1); } } while (0)

/* Frozen independently constructed owner vector from spec/vectors/runtime-v1.json. */
static const char owner_hex[] =
    "414554484f574e31"
    "00000000000040008000000000000002"
    "00000000000040008000000000000003"
    "00000000000040008000000000000004"
    "00000000000040008000000000000005"
    "00000000000040008000000000000006"
    "00000000000040008000000000000007";

static void golden(uint8_t output[AE_OWNER_BYTES])
{
    for (size_t n = 0; n < AE_OWNER_BYTES; ++n) {
        unsigned int value = 0;
        CHECK(sscanf(owner_hex + 2U * n, "%2x", &value) == 1);
        output[n] = (uint8_t)value;
    }
}

static void test_owner(void)
{
    uint8_t bytes[AE_OWNER_BYTES + 1U], encoded[AE_OWNER_BYTES + 1U];
    golden(bytes);
    struct ae_owner owner, actual;
    memset(&owner, 0, sizeof owner);
    for (size_t n = 0; n < AE_OWNER_FIELDS; ++n) {
        owner.id[n][6] = 0x40;
        owner.id[n][8] = 0x80;
        owner.id[n][15] = (uint8_t)(n + 2U);
    }
    memset(encoded, 0xa5, sizeof encoded);
    CHECK(ae_owner_encode(&owner, encoded, sizeof encoded) == 0);
    CHECK(memcmp(bytes, encoded, AE_OWNER_BYTES) == 0);
    CHECK(encoded[AE_OWNER_BYTES] == 0xa5);
    CHECK(ae_owner_decode(bytes, AE_OWNER_BYTES, &actual) == 0);
    CHECK(ae_owner_matches(&actual, &owner));
    for (size_t n = 0; n < AE_OWNER_FIELDS; ++n) {
        struct ae_owner changed = owner;
        changed.id[n][15] ^= 0x01;
        CHECK(!ae_owner_matches(&owner, &changed));
    }
    struct ae_owner sentinel;
    memset(&sentinel, 0xa5, sizeof sentinel);
    for (size_t n = 0; n < AE_OWNER_BYTES; ++n) {
        actual = sentinel;
        CHECK(ae_owner_decode(bytes, n, &actual) == -1);
        CHECK(memcmp(&actual, &sentinel, sizeof actual) == 0);
    }
    CHECK(ae_owner_decode(bytes, sizeof bytes, &actual) == -1);
    bytes[7] = '2';
    CHECK(ae_owner_decode(bytes, AE_OWNER_BYTES, &actual) == -1);
    golden(bytes);
    memset(bytes + 8U, 0, AE_ID_BYTES);
    CHECK(ae_owner_decode(bytes, AE_OWNER_BYTES, &actual) == -1);
    memset(encoded, 0xa5, sizeof encoded);
    CHECK(ae_owner_encode(&owner, encoded, AE_OWNER_BYTES - 1U) == -1);
    CHECK(encoded[0] == 0xa5);
    memset(owner.id[AE_DATA], 0, AE_ID_BYTES);
    CHECK(ae_owner_encode(&owner, encoded, sizeof encoded) == -1);
    CHECK(encoded[0] == 0xa5);
    CHECK(ae_owner_decode(NULL, AE_OWNER_BYTES, &actual) == -1);
    CHECK(ae_owner_decode(bytes, AE_OWNER_BYTES, NULL) == -1);
    uint8_t id[AE_ID_BYTES];
    memset(id, 0xa5, sizeof id);
    CHECK(ae_id_from_hex(owner_hex + 16, 32, id) == 0);
    CHECK(id[6] == 0x40 && id[8] == 0x80 && id[15] == 2);
    memset(id, 0xa5, sizeof id);
    CHECK(ae_id_from_hex("00000000000000000000000000000000", 32, id) == -1);
    CHECK(ae_id_from_hex("0000000000004000800000000000000F", 32, id) == -1);
    CHECK(ae_id_from_hex("short", 5, id) == -1);
    CHECK(id[0] == 0xa5);
    puts("owner: frozen bytes, exact scope, bounds and failure-output preservation passed");
}

static void create_file(int directory, const char *name, const void *data, size_t length)
{
    int fd = openat(directory, name, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
    CHECK(fd >= 0);
    size_t used = 0;
    while (used < length) {
        ssize_t count = write(fd, (const uint8_t *)data + used, length - used);
        if (count < 0 && errno == EINTR) continue;
        CHECK(count > 0);
        used += (size_t)count;
    }
    CHECK(close(fd) == 0);
}

static void test_storage(const char *build)
{
    char resolved[PATH_MAX], temporary[PATH_MAX];
    CHECK(realpath(build, resolved) != NULL);
    int count = snprintf(temporary, sizeof temporary, "%s/core-fixture-XXXXXX", resolved);
    CHECK(count > 0 && (size_t)count < sizeof temporary);
    CHECK(mkdtemp(temporary) != NULL);
    int root = open(temporary, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    CHECK(root >= 0);
    uint8_t bytes[AE_OWNER_BYTES + 1U];
    golden(bytes);
    struct ae_owner expected;
    CHECK(ae_owner_decode(bytes, AE_OWNER_BYTES, &expected) == 0);
    create_file(root, "owner", bytes, AE_OWNER_BYTES);
    CHECK(ae_check_owner_header(root, geteuid(), &expected) == 0);
    struct ae_owner changed = expected;
    changed.id[AE_DATA][15] ^= 1;
    CHECK(ae_check_owner_header(root, geteuid(), &changed) == -ESTALE);
    CHECK(ae_check_owner_header(root, geteuid() == 0 ? 1 : 0, &expected) == -EACCES);
    CHECK(fchmodat(root, "owner", 0620, 0) == 0);
    CHECK(ae_check_owner_header(root, geteuid(), &expected) == -EACCES);
    CHECK(fchmodat(root, "owner", 0600, 0) == 0);
    create_file(root, "document", "hello\n", 6);
    int fd = ae_open_small_file(root, "document", 8, 65536);
    CHECK(fd >= 0);
    CHECK((fcntl(fd, F_GETFD) & FD_CLOEXEC) != 0);
    CHECK((fcntl(fd, F_GETFL) & O_ACCMODE) == O_RDONLY);
    char readback[6];
    CHECK(read(fd, readback, sizeof readback) == (ssize_t)sizeof readback);
    CHECK(memcmp(readback, "hello\n", 6) == 0);
    CHECK(close(fd) == 0);
    CHECK(ae_open_small_file(root, "document", 8, 5) == -EFBIG);
    CHECK(ae_open_small_file(root, "document", 8, 65537) == -EINVAL);
    const char *bad[] = {"", "/etc/passwd", "../outside", "a/../b", "a//b", "a/", "./a", "a\\b"};
    for (size_t n = 0; n < sizeof bad / sizeof bad[0]; ++n)
        CHECK(ae_open_small_file(root, bad[n], strlen(bad[n]), 65536) == -EINVAL);
    CHECK(ae_open_small_file(root, "document\0else", 13, 65536) == -EINVAL);
    CHECK(symlinkat("/etc/passwd", root, "escape") == 0);
    CHECK(ae_open_small_file(root, "escape", 6, 65536) == -ELOOP);
    CHECK(mkdirat(root, "directory", 0700) == 0);
    CHECK(ae_open_small_file(root, "directory", 9, 65536) == -EINVAL);
    CHECK(mkfifoat(root, "pipe", 0600) == 0);
    CHECK(ae_open_small_file(root, "pipe", 4, 65536) == -EINVAL);
    CHECK(linkat(root, "document", root, "linked", 0) == 0);
    CHECK(ae_open_small_file(root, "linked", 6, 65536) == -EINVAL);
    CHECK(unlinkat(root, "linked", 0) == 0);
    CHECK(unlinkat(root, "owner", 0) == 0);
    bytes[AE_OWNER_BYTES] = 0;
    create_file(root, "owner", bytes, sizeof bytes);
    CHECK(ae_check_owner_header(root, geteuid(), &expected) == -EFBIG);
    const char *files[] = {"owner", "document", "escape", "pipe"};
    for (size_t n = 0; n < sizeof files / sizeof files[0]; ++n) CHECK(unlinkat(root, files[n], 0) == 0);
    CHECK(unlinkat(root, "directory", AT_REMOVEDIR) == 0);
    CHECK(close(root) == 0);
    CHECK(rmdir(temporary) == 0);
    puts("storage: actual openat2/read, symlink, FIFO, hardlink, owner and bounds checks passed");
}

static void test_mount(const char *build)
{
    /* Refuse mount changes in PID 1's namespace. Test data only, no real disks. */
    struct stat own_ns, initial_ns;
    CHECK(geteuid() == 0);
    CHECK(stat("/proc/self/ns/mnt", &own_ns) == 0);
    CHECK(stat("/proc/1/ns/mnt", &initial_ns) == 0);
    CHECK(own_ns.st_dev != initial_ns.st_dev || own_ns.st_ino != initial_ns.st_ino);
    CHECK(mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL) == 0);
    char resolved[PATH_MAX], temporary[PATH_MAX], source[PATH_MAX], target[PATH_MAX];
    CHECK(realpath(build, resolved) != NULL);
    int count = snprintf(temporary, sizeof temporary, "%s/mount-fixture-XXXXXX", resolved);
    CHECK(count > 0 && (size_t)count < sizeof temporary);
    CHECK(mkdtemp(temporary) != NULL);
    count = snprintf(source, sizeof source, "%s/source", temporary);
    CHECK(count > 0 && (size_t)count < sizeof source);
    count = snprintf(target, sizeof target, "%s/root/mounted", temporary);
    CHECK(count > 0 && (size_t)count < sizeof target);
    int fixture = open(temporary, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    CHECK(fixture >= 0);
    CHECK(mkdirat(fixture, "source", 0700) == 0);
    CHECK(mkdirat(fixture, "root", 0700) == 0);
    int root = openat(fixture, "root", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    int external = openat(fixture, "source", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    CHECK(root >= 0 && external >= 0);
    CHECK(mkdirat(root, "mounted", 0700) == 0);
    create_file(external, "document", "outside\n", 8);
    CHECK(mount(source, target, NULL, MS_BIND, NULL) == 0);
    struct stat root_stat, mounted_stat;
    CHECK(fstat(root, &root_stat) == 0);
    CHECK(fstatat(root, "mounted", &mounted_stat, 0) == 0);
    CHECK(root_stat.st_dev == mounted_stat.st_dev); /* Device equality is insufficient. */
    int baseline = openat(root, "mounted/document", O_RDONLY | O_CLOEXEC);
    CHECK(baseline >= 0);
    char readback[8];
    CHECK(read(baseline, readback, sizeof readback) == (ssize_t)sizeof readback);
    CHECK(memcmp(readback, "outside\n", 8) == 0);
    CHECK(close(baseline) == 0);
    CHECK(ae_open_small_file(root, "mounted/document", 16, 65536) == -EXDEV);
    CHECK(umount(target) == 0);
    CHECK(unlinkat(external, "document", 0) == 0);
    CHECK(unlinkat(root, "mounted", AT_REMOVEDIR) == 0);
    CHECK(close(external) == 0 && close(root) == 0);
    CHECK(unlinkat(fixture, "source", AT_REMOVEDIR) == 0);
    CHECK(unlinkat(fixture, "root", AT_REMOVEDIR) == 0);
    CHECK(close(fixture) == 0);
    CHECK(rmdir(temporary) == 0);
    puts("mount: private namespace, real same-filesystem bind, baseline read and EXDEV denial passed");
}

int main(int argc, char **argv)
{
    CHECK(argc == 3);
    if (strcmp(argv[1], "owner") == 0) test_owner();
    else if (strcmp(argv[1], "mount") == 0) test_mount(argv[2]);
    else { CHECK(strcmp(argv[1], "storage") == 0); test_storage(argv[2]); }
    return 0;
}
