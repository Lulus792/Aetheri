#include "authority/fixture.h"
#include "core/digest.h"
#include "core/fixture.h"
#include "core/import.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/openat2.h>
#include <selinux/selinux.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

#define ROOT "/var/lib/aetheri-p0"

static uint64_t number(const uint8_t *bytes, size_t size)
{
    uint64_t value = 0;
    for (size_t n = 0; n < size; ++n) value = (value << 8) | bytes[n];
    return value;
}
static void put(uint8_t *bytes, uint64_t value, size_t size)
{
    for (size_t n = 0; n < size; ++n) { bytes[size - n - 1U] = (uint8_t)value; value >>= 8; }
}
static int witness_decode(const uint8_t *bytes, struct ae_root_identity *root)
{
    uint64_t type = number(bytes, 8), inode = number(bytes + 16, 8), birth = number(bytes + 24, 8);
    uint64_t nanos = number(bytes + 32, 4);
    if (type > INT64_MAX || inode > INT64_MAX || birth > INT64_MAX || nanos >= 1000000000U) return -EBADMSG;
    root->filesystem_type = (int64_t)type; root->inode = inode;
    root->birth_seconds = (int64_t)birth; root->birth_nanoseconds = (uint32_t)nanos;
    root->birth_present = inode != 0;
    memcpy(root->filesystem_id, bytes + 8, 8);
    return 0;
}
static int witness_encode(int fd, uint8_t bytes[AE_FIXTURE_ROOT_BYTES])
{
    struct ae_root_identity root;
    int result = ae_root_observe(fd, &root);
    if (result != 0) return result;
    if (!root.birth_present || root.filesystem_type <= 0 || root.inode == 0 || root.inode > INT64_MAX || root.birth_seconds < 0) return -ENOTSUP;
    put(bytes, (uint64_t)root.filesystem_type, 8); memcpy(bytes + 8, root.filesystem_id, 8);
    put(bytes + 16, root.inode, 8); put(bytes + 24, (uint64_t)root.birth_seconds, 8); put(bytes + 32, root.birth_nanoseconds, 4);
    return 0;
}

int ae_fixture_config_decode(const uint8_t *bytes, size_t size, struct ae_fixture_config *output)
{
    if (bytes == NULL || output == NULL) return -EINVAL;
    if (size != AE_FIXTURE_CONFIG_BYTES || memcmp(bytes, "AETHFIC1", 8) != 0) return -EBADMSG;
    struct ae_fixture_config config = {0};
    struct ae_fixture_request *request = &config.request;
    uint8_t *ids[] = {request->plan, request->operation, request->actor, request->request_key, request->scope,
        request->install, request->generation, request->slot, request->publisher, request->boot};
    for (size_t n = 0; n < sizeof ids / sizeof ids[0]; ++n) {
        memcpy(ids[n], bytes + 8U + n * 16U, 16);
        if ((ids[n][6] >> 4) != 4 || (ids[n][8] & 0xc0) != 0x80) return -EBADMSG;
    }
    memcpy(request->artifact, bytes + 168, 32);
    uint64_t revision = number(bytes + 200, 8), expires = number(bytes + 208, 8), accepted = number(bytes + 216, 8);
    if (revision < 1 || revision > INT64_MAX || expires > INT64_MAX || accepted > INT64_MAX) return -EBADMSG;
    request->scope_revision = (int64_t)revision; request->expires_boottime_ns = (int64_t)expires;
    request->accepted_unix_ms = (int64_t)accepted; request->os_uid = 1000;
    memcpy(request->expected_intent, bytes + 296, 32);
    int result = witness_decode(bytes + 224, &config.source);
    if (result == 0) result = witness_decode(bytes + 260, &config.staging);
    if (result == 0) *output = config;
    return result;
}

static int close_owned(int fd, int result)
{
    if (fd >= 0 && close(fd) != 0 && result == 0) result = -errno;
    return result;
}
static int selected(const char *path, int directory)
{
    struct open_how how = {.flags = (uint64_t)(O_RDONLY | O_CLOEXEC | O_NOFOLLOW | (directory ? O_DIRECTORY : O_NONBLOCK)),
        .resolve = RESOLVE_NO_SYMLINKS | RESOLVE_NO_MAGICLINKS};
    int fd;
    do { fd = (int)syscall(SYS_openat2, AT_FDCWD, path, &how, sizeof how); } while (fd < 0 && errno == EINTR);
    return fd < 0 ? -errno : fd;
}
static int has_type(const char *context, const char *type)
{
    if (context == NULL) return 0;
    const char *first = strchr(context, ':');
    const char *second = first == NULL ? NULL : strchr(first + 1, ':');
    const char *third = second == NULL ? NULL : strchr(second + 1, ':');
    return third != NULL && (size_t)(third - second - 1) == strlen(type) &&
        memcmp(second + 1, type, strlen(type)) == 0 && strcmp(third, ":s0") == 0;
}
static int labeled_fd(int fd, const char *type)
{
    char *context = NULL;
    if (fgetfilecon_raw(fd, &context) < 0) return -EACCES;
    int result = has_type(context, type) ? 0 : -EACCES;
    freecon(context);
    return result;
}
static int protected_directory(const char *path, mode_t mode, const char *type)
{
    int fd = selected(path, 1);
    if (fd < 0) return fd;
    struct stat info;
    int result = fstat(fd, &info) == 0 ? 0 : -errno;
    if (result == 0 && (info.st_uid != 0 || (info.st_mode & 07777) != mode)) result = -EACCES;
    if (result == 0) result = labeled_fd(fd, type);
    if (result != 0) return close_owned(fd, result);
    return fd;
}
static int protected_bytes(const char *path, uint8_t *bytes, size_t limit, size_t *size)
{
    int fd = selected(path, 0);
    if (fd < 0) return fd;
    struct stat before, after;
    int result = fstat(fd, &before) == 0 ? 0 : -errno;
    if (result == 0 && (!S_ISREG(before.st_mode) || before.st_uid != 0 || before.st_nlink != 1 ||
        (before.st_mode & 07777) != 0444 || before.st_size < 1 || (uint64_t)before.st_size > limit)) result = -EACCES;
    if (result == 0) result = labeled_fd(fd, "aetheri_registry_t");
    size_t used = 0;
    while (result == 0 && used < (size_t)before.st_size) {
        ssize_t count = pread(fd, bytes + used, (size_t)before.st_size - used, (off_t)used);
        if (count < 0) { if (errno == EINTR) continue; result = -errno; }
        else if (count == 0) result = -EBADMSG;
        else used += (size_t)count;
    }
    if (result == 0 && fstat(fd, &after) != 0) result = -errno;
    if (result == 0 && (before.st_ino != after.st_ino || before.st_dev != after.st_dev || before.st_mode != after.st_mode ||
        before.st_size != after.st_size || before.st_ctim.tv_sec != after.st_ctim.tv_sec || before.st_ctim.tv_nsec != after.st_ctim.tv_nsec)) result = -ESTALE;
    result = close_owned(fd, result);
    if (result == 0) *size = used;
    return result;
}

static int labels(int root, const struct ae_fixture *fixture, const char *type)
{
    for (uint32_t n = 0; n < fixture->file_count; ++n) {
        struct ae_fixture_text path = fixture->files[n].path;
        for (size_t p = 0; p <= path.size; ++p) if (p == path.size || path.data[p] == '/') {
            char name[257]; memcpy(name, path.data, p); name[p] = '\0';
            struct open_how how = {.flags = O_PATH | O_NOFOLLOW | O_CLOEXEC,
                .resolve = RESOLVE_BENEATH | RESOLVE_NO_XDEV | RESOLVE_NO_SYMLINKS | RESOLVE_NO_MAGICLINKS};
            int fd = (int)syscall(SYS_openat2, root, name, &how, sizeof how);
            if (fd < 0) return -errno;
            struct stat info;
            int result = fstat(fd, &info) == 0 ? 0 : -errno;
            if (result == 0 && (info.st_uid != 0 || !(p == path.size ? S_ISREG(info.st_mode) : S_ISDIR(info.st_mode)))) result = -EACCES;
            char proc[64];
            int length = snprintf(proc, sizeof proc, "/proc/self/fd/%d", fd);
            if (result == 0 && (length < 0 || (size_t)length >= sizeof proc)) result = -EOVERFLOW;
            char *context = NULL;
            if (result == 0 && getfilecon_raw(proc, &context) < 0) result = -EACCES;
            if (result == 0 && !has_type(context, type)) result = -EACCES;
            freecon(context);
            result = close_owned(fd, result);
            if (result != 0) return result;
        }
    }
    return 0;
}
static int root_matches(int fd, const struct ae_root_identity *expected)
{
    uint8_t bytes[36];
    int result = witness_encode(fd, bytes);
    if (result != 0) return result;
    struct ae_root_identity current = {0};
    result = witness_decode(bytes, &current);
    if (result == 0 && (expected->inode == 0 || current.inode != expected->inode || current.filesystem_type != expected->filesystem_type ||
        memcmp(current.filesystem_id, expected->filesystem_id, 8) != 0 || current.birth_seconds != expected->birth_seconds ||
        current.birth_nanoseconds != expected->birth_nanoseconds)) result = -ESTALE;
    return result;
}
static int is_empty(int root, int *empty)
{
    int fd = openat(root, ".", O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) return -errno;
    DIR *stream = fdopendir(fd);
    if (stream == NULL) return close_owned(fd, -errno);
    int result = 0, found = 0;
    for (;;) {
        errno = 0;
        struct dirent *entry = readdir(stream);
        if (entry == NULL) { if (errno != 0) result = -errno; break; }
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) { found = 1; break; }
    }
    if (closedir(stream) != 0 && result == 0) result = -errno;
    if (result == 0) *empty = !found;
    return result;
}

/* Read the approved fixture as data only. Staging/source types must not permit
 * executable mappings, even to the system role; no mapped bytes are executed. */
static int no_executable_mapping(int root, const struct ae_fixture_file *file)
{
    char path[257];
    memcpy(path, file->path.data, file->path.size); path[file->path.size] = '\0';
    struct open_how how = {.flags=O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK,
        .resolve=RESOLVE_BENEATH | RESOLVE_NO_XDEV | RESOLVE_NO_SYMLINKS | RESOLVE_NO_MAGICLINKS};
    int fd = (int)syscall(SYS_openat2, root, path, &how, sizeof how);
    if (fd < 0) return -errno;
    void *mapping = mmap(NULL, 1, PROT_READ | PROT_EXEC, MAP_PRIVATE, fd, 0);
    int result;
    if (mapping != MAP_FAILED) { (void)munmap(mapping, 1); result = -EPERM; }
    else result = errno == EACCES || errno == EPERM ? 0 : -errno;
    return close_owned(fd, result);
}

int ae_authority_fixture(struct ae_catalogue *catalogue, int observe, const uint8_t current_boot[16])
{
    uint8_t bytes[AE_FIXTURE_CONFIG_BYTES];
    uint8_t *manifest = malloc(AE_FIXTURE_MAX_BYTES);
    if (manifest == NULL) return -ENOMEM;
    size_t size, config_size;
    struct ae_fixture_config config;
    struct ae_fixture fixture;
    int result = protected_bytes(ROOT "/registry/fixture.conf", bytes, sizeof bytes, &config_size);
    if (result == 0) result = ae_fixture_config_decode(bytes, config_size, &config);
    if (result == 0) result = protected_bytes(ROOT "/registry/fixture.manifest", manifest, AE_FIXTURE_MAX_BYTES, &size);
    if (result == 0) result = ae_fixture_verify_manifest(manifest, size, config.request.artifact, &fixture);
    if (result == 0 && memcmp(fixture.publisher, config.request.publisher, 16) != 0) result = -ESTALE;
    char slot[33], parent_path[256], source_path[288], stage_path[288];
    for (size_t n = 0; result == 0 && n < 16; ++n) snprintf(slot + n * 2U, 3, "%02x", (unsigned int)config.request.slot[n]);
    if (result == 0) {
        int length = snprintf(parent_path, sizeof parent_path, ROOT "/fixtures/%s", slot);
        if (length < 0 || (size_t)length >= sizeof parent_path) result = -EOVERFLOW;
        else {
            snprintf(source_path, sizeof source_path, "%s/source", parent_path);
            snprintf(stage_path, sizeof stage_path, "%s/staging", parent_path);
        }
    }
    int parent = -1, source = -1, stage = -1;
    if (result == 0) { parent = protected_directory(ROOT "/fixtures", 0700, "aetheri_root_t"); result = parent < 0 ? parent : 0; }
    result = close_owned(parent, result); parent = -1;
    if (result == 0) { parent = protected_directory(parent_path, 0700, "aetheri_root_t"); result = parent < 0 ? parent : 0; }
    if (result == 0) { source = protected_directory(source_path, 0555, "aetheri_fixture_source_t"); result = source < 0 ? source : 0; }
    if (result == 0) { stage = protected_directory(stage_path, 0700, "aetheri_staging_t"); result = stage < 0 ? stage : 0; }
    if (result == 0) result = labels(source, &fixture, "aetheri_fixture_source_t");
    if (result == 0 && observe) {
        uint8_t roots[72];
        result = witness_encode(source, roots);
        if (result == 0) result = witness_encode(stage, roots + 36);
        if (result == 0) {
            fputs("fixture-roots ", stdout);
            for (size_t n = 0; n < sizeof roots; ++n) printf("%02x", (unsigned int)roots[n]);
            putchar('\n');
        }
    } else if (result == 0) {
        result = root_matches(source, &config.source);
        if (result == 0) result = root_matches(stage, &config.staging);
        struct ae_fixture_progress progress;
        uint8_t evidence[32];
        if (result == 0) result = ae_sha256_bytes(bytes, sizeof bytes, evidence);
        if (result == 0) result = ae_catalogue_fixture_accept(catalogue, &config.request, manifest, size, current_boot, &progress);
        if (result == 0 && progress.evidence_present && memcmp(progress.evidence, evidence, 32) != 0) result = -ESTALE;
        if (result == 0 && progress.step == AE_FIXTURE_PLANNED) {
            int empty = 0;
            result = is_empty(stage, &empty);
            if (result == 0 && !empty) result = -EEXIST;
        }
        if (result == 0 && progress.step != AE_FIXTURE_READY)
            result = ae_catalogue_fixture_transition(catalogue, &config.request, manifest, size, AE_FIXTURE_EXECUTING, evidence, &progress);
        if (result == 0) {
            int empty = 0;
            result = is_empty(stage, &empty);
            if (result == 0) result = empty && progress.step != AE_FIXTURE_READY ?
                ae_fixture_stage_payload(source, stage, manifest, size, config.request.artifact) :
                ae_fixture_sync_staged_payload(stage, manifest, size, config.request.artifact);
            if (result == 0) result = labels(stage, &fixture, "aetheri_staging_t");
            for (uint32_t n = 0; result == 0 && n < fixture.file_count; ++n) if (fixture.files[n].mode == 0555) {
                result = no_executable_mapping(source, &fixture.files[n]);
                if (result == 0) result = no_executable_mapping(stage, &fixture.files[n]);
            }
            if (result == 0) result = root_matches(stage, &config.staging);
            if (result == 0) result = ae_catalogue_fixture_transition(catalogue, &config.request, manifest, size, AE_FIXTURE_READY, evidence, &progress);
            else {
                struct ae_fixture_progress ignored;
                int marked = ae_catalogue_fixture_transition(catalogue, &config.request, manifest, size, AE_FIXTURE_UNCERTAIN, evidence, &ignored);
                if (marked != 0) fprintf(stderr, "Fixture uncertainty acknowledgement failed: %d\n", marked);
            }
        }
        if (result == 0) puts("fixture-stage ready; operation remains running/publish; artifact/generation remain staged");
    }
    result = close_owned(stage, result); result = close_owned(source, result); result = close_owned(parent, result);
    free(manifest);
    return result;
}
