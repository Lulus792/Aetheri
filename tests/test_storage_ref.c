#include "core/storage.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "storage ref line %d: %s\n", __LINE__, #x); exit(1); } } while (0)

static int stat_fault, stat_calls, header_reads, change_root = -1;
static uint8_t changed_header[AE_OWNER_BYTES];
int __real_statx(int fd, const char *path, int flags, unsigned int mask, struct statx *status);
int __wrap_statx(int fd, const char *path, int flags, unsigned int mask, struct statx *status)
{
    if (stat_fault == 3) { errno = EOPNOTSUPP; return -1; }
    int result = __real_statx(fd, path, flags, mask, status);
    if (result == 0) {
        if (stat_fault == 1) { status->stx_mask &= ~STATX_MNT_ID_UNIQUE; status->stx_mask |= STATX_MNT_ID; }
        if (stat_fault == 2 && ++stat_calls == 2) status->stx_uid ^= 1U;
    }
    return result;
}

static void replace_header(int root, const uint8_t *bytes)
{
    int fd = openat(root, "owner", O_WRONLY | O_CLOEXEC | O_NOFOLLOW);
    CHECK(fd >= 0 && pwrite(fd, bytes, AE_OWNER_BYTES, 0) == AE_OWNER_BYTES && close(fd) == 0);
}

ssize_t __real_read(int fd, void *data, size_t size);
ssize_t __wrap_read(int fd, void *data, size_t size)
{
    /* Change the protected test header only as the post-open validation starts.
     * This is explicit fault injection, never a permitted app write. */
    if (change_root >= 0 && size == AE_OWNER_BYTES + 1U && ++header_reads == 2) replace_header(change_root, changed_header);
    return __real_read(fd, data, size);
}

static int descriptors(void)
{
    DIR *directory = opendir("/proc/self/fd");
    CHECK(directory != NULL);
    int count = 0;
    struct dirent *entry;
    while ((entry = readdir(directory)) != NULL) if (entry->d_name[0] != '.') ++count;
    CHECK(closedir(directory) == 0);
    return count;
}

static void create(int directory, const char *name, const void *bytes, size_t size)
{
    int fd = openat(directory, name, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
    CHECK(fd >= 0 && write(fd, bytes, size) == (ssize_t)size && close(fd) == 0);
}

int main(int argc, char **argv)
{
    CHECK(argc == 3 && (strcmp(argv[1], "basic") == 0 || strcmp(argv[1], "mount") == 0));
    char build[PATH_MAX], temporary[PATH_MAX], absolute[PATH_MAX];
    CHECK(realpath(argv[2], build) != NULL);
    int length = snprintf(temporary, sizeof temporary, "%s/storage-ref-XXXXXX", build);
    CHECK(length > 0 && (size_t)length < sizeof temporary && mkdtemp(temporary) != NULL);
    const char leaf[] = "00000000000040008000000000000005";
    length = snprintf(absolute, sizeof absolute, "%s/%s", temporary, leaf);
    CHECK(length > 0 && (size_t)length < sizeof absolute);
    int anchor = open(temporary, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    CHECK(anchor >= 0 && mkdirat(anchor, leaf, 0700) == 0);
    int root = openat(anchor, leaf, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    CHECK(root >= 0);
    struct ae_owner owner = {0};
    for (size_t n = 0; n < AE_OWNER_FIELDS; ++n) {
        owner.id[n][6] = 0x40; owner.id[n][8] = 0x80; owner.id[n][15] = (uint8_t)(n + 2U);
    }
    uint8_t header[AE_OWNER_BYTES];
    CHECK(ae_owner_encode(&owner, header, sizeof header) == 0);
    create(root, "owner", header, sizeof header);
    CHECK(mkdirat(root, "payload", 0700) == 0);
    int payload = openat(root, "payload", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    CHECK(payload >= 0);
    create(payload, "document", "hello\n", 6);
    struct ae_root_identity identity, unchanged, observation;
    CHECK(ae_root_observe(root, &identity) == 0 && identity.mount_id != 0 && identity.uid == geteuid());
    CHECK(ae_root_check_path(root, absolute, &identity) == 0);
    memset(&unchanged, 0xa5, sizeof unchanged);
    for (stat_fault = 1; stat_fault <= 3; ++stat_fault) {
        stat_calls = 0; memcpy(&observation, &unchanged, sizeof observation);
        CHECK(ae_root_observe(root, &observation) == (stat_fault == 1 ? -ENOTSUP : stat_fault == 2 ? -ESTALE : -EOPNOTSUPP));
        CHECK(memcmp(&observation, &unchanged, sizeof observation) == 0);
    }
    stat_fault = 0;
    struct ae_storage_ref *reference = NULL;
    CHECK(ae_storage_ref_open(anchor, geteuid(), &owner, &identity, &reference) == 0);
    if (strcmp(argv[1], "mount") == 0) {
        CHECK(geteuid() == 0 && mount(absolute, absolute, NULL, MS_BIND, NULL) == 0);
        int alias = open(absolute, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        CHECK(alias >= 0 && ae_root_observe(alias, &observation) == 0);
        CHECK(observation.inode == identity.inode && observation.device_major == identity.device_major &&
              observation.device_minor == identity.device_minor && observation.mount_id != identity.mount_id);
        CHECK(ae_root_check_path(root, absolute, &identity) == -ESTALE);
        CHECK(ae_storage_ref_check(reference) == -EXDEV);
        CHECK(close(alias) == 0 && umount(absolute) == 0);
        CHECK(ae_storage_ref_check(reference) == 0);
        CHECK(fchown(payload, 1000, 1000) == 0);
        CHECK(ae_storage_ref_read(reference, "document", 8, 65536) == -EACCES);
        CHECK(fchown(payload, 0, 0) == 0 && fchownat(payload, "document", 1000, 1000, 0) == 0);
        CHECK(ae_storage_ref_read(reference, "document", 8, 65536) == -EACCES);
        CHECK(fchownat(payload, "document", 0, 0, 0) == 0);
    } else {
        CHECK(close(anchor) == 0); /* The reference owns its own anchor descriptor. */
        anchor = open(temporary, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        CHECK(anchor >= 0);
        int fd = ae_storage_ref_read(reference, "document", 8, 65536);
        CHECK(fd >= 0 && (fcntl(fd, F_GETFD) & FD_CLOEXEC) != 0 && (fcntl(fd, F_GETFL) & O_ACCMODE) == O_RDONLY);
        CHECK(ae_storage_ref_close(&reference) == 0 && reference == NULL);
        char bytes[6];
        CHECK(read(fd, bytes, sizeof bytes) == 6 && memcmp(bytes, "hello\n", 6) == 0 && close(fd) == 0);
        /* Closing a root reference is not revocation of returned document FDs. */
        CHECK(ae_storage_ref_open(anchor, geteuid(), &owner, &identity, &reference) == 0);
        CHECK(ae_storage_ref_read(reference, "document", 8, 5) == -EFBIG);
        CHECK(ae_storage_ref_read(reference, "document", 8, 65537) == -EINVAL);
        CHECK(ae_storage_ref_read(reference, "../owner", 8, 65536) == -EINVAL);
        CHECK(ae_storage_ref_read(reference, "owner", 5, 65536) == -ENOENT);
        CHECK(fchmod(payload, 0750) == 0 && ae_storage_ref_read(reference, "document", 8, 65536) == -EACCES);
        CHECK(fchmod(payload, 0700) == 0);
        CHECK(symlinkat("../owner", payload, "escape") == 0);
        CHECK(ae_storage_ref_read(reference, "escape", 6, 65536) == -ELOOP);
        CHECK(unlinkat(payload, "escape", 0) == 0);
        for (int field = AE_MACHINE; field < AE_OWNER_FIELDS; ++field) {
            struct ae_owner wrong = owner; wrong.id[field][15] ^= 1U;
            struct ae_storage_ref *sentinel = reference;
            CHECK(ae_storage_ref_open(anchor, geteuid(), &wrong, &identity, &sentinel) < 0 && sentinel == reference);
        }
        int failures_before = descriptors();
        for (int field = 0; field < 9; ++field) {
            struct ae_root_identity wrong = identity;
            switch (field) {
            case 0: wrong.mount_id ^= 1U; break;
            case 1: wrong.inode ^= 1U; break;
            case 2: wrong.device_major ^= 1U; break;
            case 3: wrong.device_minor ^= 1U; break;
            case 4: wrong.uid ^= 1U; break;
            case 5: wrong.gid ^= 1U; break;
            case 6: wrong.mode ^= 0040; break;
            case 7: wrong.filesystem_type ^= 1; break;
            default: wrong.filesystem_id[0] ^= 1U; break;
            }
            struct ae_storage_ref *sentinel = reference;
            CHECK(ae_storage_ref_open(anchor, geteuid(), &owner, &wrong, &sentinel) < 0 && sentinel == reference);
        }
        CHECK(descriptors() == failures_before);
        CHECK(fchmod(root, 0750) == 0 && ae_storage_ref_check(reference) == -ESTALE && fchmod(root, 0700) == 0);
        CHECK(fchmodat(root, "owner", 0644, 0) == 0 && ae_storage_ref_check(reference) == -EACCES && fchmodat(root, "owner", 0600, 0) == 0);
        struct ae_owner wrong = owner; wrong.id[AE_DATA][15] ^= 1U;
        CHECK(ae_owner_encode(&wrong, changed_header, sizeof changed_header) == 0);
        int before = descriptors();
        change_root = root; header_reads = 0;
        CHECK(ae_storage_ref_read(reference, "document", 8, 65536) == -ESTALE);
        change_root = -1;
        CHECK(descriptors() == before);
        replace_header(root, header);
        CHECK(renameat(anchor, leaf, anchor, "old") == 0 && mkdirat(anchor, leaf, 0700) == 0);
        int clone = openat(anchor, leaf, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        CHECK(clone >= 0); create(clone, "owner", header, sizeof header);
        CHECK(ae_storage_ref_check(reference) == -ESTALE);
        struct ae_storage_ref *sentinel = reference;
        CHECK(ae_storage_ref_open(anchor, geteuid(), &owner, &identity, &sentinel) == -ESTALE && sentinel == reference);
        CHECK(unlinkat(clone, "owner", 0) == 0 && close(clone) == 0 && unlinkat(anchor, leaf, AT_REMOVEDIR) == 0);
        CHECK(renameat(anchor, "old", anchor, leaf) == 0 && ae_storage_ref_check(reference) == 0);
        pid_t child = fork();
        CHECK(child >= 0);
        if (child == 0) {
            CHECK(ae_storage_ref_check(reference) == -ECHILD);
            CHECK(ae_storage_ref_read(reference, "document", 8, 65536) == -ECHILD);
            CHECK(ae_storage_ref_close(&reference) == -ECHILD);
            _exit(0);
        }
        int status;
        CHECK(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0);
        CHECK(ae_storage_ref_close(&reference) == 0);
        before = descriptors();
        for (int n = 0; n < 64; ++n) {
            CHECK(ae_storage_ref_open(anchor, geteuid(), &owner, &identity, &reference) == 0);
            fd = ae_storage_ref_read(reference, "document", 8, 65536);
            CHECK(fd >= 0 && close(fd) == 0 && ae_storage_ref_close(&reference) == 0);
        }
        CHECK(descriptors() == before);
    }
    CHECK(ae_storage_ref_close(&reference) == 0 && ae_storage_ref_close(&reference) == 0);
    CHECK(unlinkat(payload, "document", 0) == 0 && close(payload) == 0 && unlinkat(root, "payload", AT_REMOVEDIR) == 0);
    CHECK(unlinkat(root, "owner", 0) == 0 && close(root) == 0 && unlinkat(anchor, leaf, AT_REMOVEDIR) == 0);
    CHECK(close(anchor) == 0 && rmdir(temporary) == 0);
    puts("storage refs: actual held roots/owner binding, namespace identity, descriptor ownership and stale/fault boundaries passed; no volume/MAC admission claim");
    return 0;
}
