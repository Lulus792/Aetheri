#include "authority/fixture.h"
#include "core/digest.h"
#include "core/fixture.h"
#include "core/import.h"
#include "core/publish.h"

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

int ae_fixture_store_config_decode(const uint8_t *bytes,size_t size,struct ae_fixture_store_config *output)
{
    if (bytes==NULL || output==NULL) return -EINVAL;
    if (size!=AE_FIXTURE_STORE_CONFIG_BYTES || memcmp(bytes,"AETHSTO1",8)!=0) return -EBADMSG;
    struct ae_fixture_store_config config={0};
    memcpy(config.stage_digest,bytes+8,32);
    int result=witness_decode(bytes+40,&config.store);
    if (result==0) result=witness_decode(bytes+76,&config.container);
    if (result==0 && (config.store.inode==0 || config.container.inode==0)) result=-EBADMSG;
    if (result==0) *output=config;
    return result;
}

int ae_fixture_activation_config_decode(const uint8_t *bytes,size_t size,struct ae_fixture_activation *output)
{
    if (bytes==NULL || output==NULL) return -EINVAL;
    if (size!=AE_FIXTURE_ACTIVATION_CONFIG_BYTES || memcmp(bytes,"AETHACT1",8)!=0) return -EBADMSG;
    struct ae_fixture_activation activation={0};
    const uint64_t install=number(bytes+40,8),generation=number(bytes+48,8),gate=number(bytes+56,8),stamp=number(bytes+64,8);
    if (install<1 || install>=INT64_MAX || generation<1 || generation>=INT64_MAX || gate<1 || gate>INT64_MAX || stamp>INT64_MAX) return -EBADMSG;
    memcpy(activation.publication_evidence,bytes+8,32);
    activation.install_revision=(int64_t)install; activation.generation_revision=(int64_t)generation;
    activation.gate_revision=(int64_t)gate; activation.completed_unix_ms=(int64_t)stamp;
    *output=activation;
    return 0;
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
    if (result == 0) { parent = protected_directory(parent_path, 0700, "aetheri_stage_parent_t"); result = parent < 0 ? parent : 0; }
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

static int publication_directory(const char *path,const char *old_type)
{
    int fd=selected(path,1);
    if (fd<0) return fd;
    struct stat info;
    int result=fstat(fd,&info)==0 ? 0 : -errno;
    if (result==0 && (info.st_uid!=0 || ((info.st_mode&07777)!=0700 && (info.st_mode&07777)!=0555))) result=-EACCES;
    if (result==0 && labeled_fd(fd,old_type)!=0) result=labeled_fd(fd,"aetheri_store_dir_t");
    if (result!=0) return close_owned(fd,result);
    return fd;
}

static int seal_node(int fd,const char *old_type,const char *final_type,mode_t mode,int root)
{
    struct stat info;
    int result=fstat(fd,&info)==0 ? 0 : -errno;
    char *context=NULL;
    if (result==0 && fgetfilecon_raw(fd,&context)<0) result=-EACCES;
    int sealed=result==0 && has_type(context,final_type);
    if (result==0 && (!sealed && !has_type(context,old_type))) result=-EACCES;
    if (result==0 && (info.st_uid!=0 || (info.st_mode&07777)!=mode)) {
        if (!sealed && root && (info.st_mode&07777)==0700 && info.st_uid==0) {
            if (fchmod(fd,mode)!=0) result=-errno;
        } else result=-EACCES;
    }
    if (result==0 && !sealed) {
        char target[128];
        int length=snprintf(target,sizeof target,"system_u:object_r:%s:s0",final_type);
        if (length<0 || (size_t)length>=sizeof target) result=-EOVERFLOW;
        else if (fsetfilecon_raw(fd,target)!=0) result=-errno;
    }
    freecon(context);
    if (result==0) result=labeled_fd(fd,final_type);
    return result;
}

static int seal_publication(int container,int payload,const struct ae_fixture *fixture)
{
    for (uint32_t n=0;n<fixture->file_count;++n) {
        struct ae_fixture_text path=fixture->files[n].path;
        /* Children first, then their directories. Repeated shared prefixes are
         * exact final-label checks, never a relabel-from-final permission. */
        for (size_t p=path.size;p>0;--p) if (p==path.size || path.data[p]=='/') {
            char name[257]; memcpy(name,path.data,p); name[p]='\0';
            int file=p==path.size;
            struct open_how how={.flags=(uint64_t)(O_RDONLY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK | (file ? 0 : O_DIRECTORY)),
                .resolve=RESOLVE_BENEATH | RESOLVE_NO_XDEV | RESOLVE_NO_SYMLINKS | RESOLVE_NO_MAGICLINKS};
            int fd=(int)syscall(SYS_openat2,payload,name,&how,sizeof how);
            if (fd<0) return -errno;
            const char *type=file ? (fixture->files[n].mode==0555 ? "aetheri_store_code_t" : "aetheri_store_data_t") : "aetheri_store_dir_t";
            int result=seal_node(fd,"aetheri_staging_t",type,file ? (mode_t)fixture->files[n].mode : 0555,0);
            result=close_owned(fd,result);
            if (result!=0) return result;
        }
    }
    int result=seal_node(payload,"aetheri_staging_t","aetheri_store_dir_t",0555,1);
    struct open_how how={.flags=O_RDONLY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK,
        .resolve=RESOLVE_BENEATH | RESOLVE_NO_XDEV | RESOLVE_NO_SYMLINKS | RESOLVE_NO_MAGICLINKS};
    int metadata=-1;
    if (result==0) { metadata=(int)syscall(SYS_openat2,container,"manifest",&how,sizeof how); if (metadata<0) result=-errno; }
    if (result==0) result=seal_node(metadata,"aetheri_publication_t","aetheri_store_manifest_t",0444,0);
    result=close_owned(metadata,result);
    if (result==0) result=seal_node(container,"aetheri_publication_t","aetheri_store_dir_t",0555,1);
    return result;
}

/* The inode owner could chmod a read-only inode under DAC. Even a no-op chmod
 * must therefore fail under the final MAC type. Unexpected permission changes
 * only ctime, never the requested mode; report failure and preserve evidence. */
static int no_store_setattr(int root,mode_t mode)
{
    if (fchmod(root,mode)==0) return -EPERM;
    return errno==EACCES || errno==EPERM ? 0 : -errno;
}

int ae_authority_fixture_publication(struct ae_catalogue *catalogue,int observe)
{
    uint8_t bytes[AE_FIXTURE_CONFIG_BYTES],publication_bytes[AE_FIXTURE_STORE_CONFIG_BYTES],stage_digest[32],evidence[32];
    uint8_t *manifest=malloc(AE_FIXTURE_MAX_BYTES);
    if (manifest==NULL) return -ENOMEM;
    size_t config_size,size,publication_size;
    struct ae_fixture_config config;
    struct ae_fixture_store_config publication_config;
    struct ae_fixture fixture;
    struct ae_fixture_progress progress={0};
    int result=protected_bytes(ROOT "/registry/fixture.conf",bytes,sizeof bytes,&config_size);
    if (result==0) result=ae_fixture_config_decode(bytes,config_size,&config);
    if (result==0) result=ae_sha256_bytes(bytes,sizeof bytes,stage_digest);
    if (result==0) result=protected_bytes(ROOT "/registry/fixture.manifest",manifest,AE_FIXTURE_MAX_BYTES,&size);
    if (result==0) result=ae_fixture_verify_manifest(manifest,size,config.request.artifact,&fixture);
    if (result==0) result=ae_catalogue_fixture_get(catalogue,&config.request,manifest,size,&progress);
    if (result==0 && (progress.step!=AE_FIXTURE_READY || !progress.evidence_present || memcmp(progress.evidence,stage_digest,32)!=0)) result=-ESTALE;
    if (result==0 && !observe) result=protected_bytes(ROOT "/registry/fixture-store.conf",publication_bytes,sizeof publication_bytes,&publication_size);
    if (result==0 && !observe) result=ae_fixture_store_config_decode(publication_bytes,publication_size,&publication_config);
    if (result==0 && !observe && memcmp(publication_config.stage_digest,stage_digest,32)!=0) result=-ESTALE;
    if (result==0 && !observe) result=ae_sha256_bytes(publication_bytes,sizeof publication_bytes,evidence);
    if (result==0 && !observe && progress.publication_present && memcmp(progress.publication_evidence,evidence,32)!=0) result=-ESTALE;
    char slot[33],artifact[65],parent_path[256],local_path[288],stored_path[288],payload_path[320];
    for (size_t n=0;result==0 && n<16;++n) snprintf(slot+n*2U,3,"%02x",(unsigned int)config.request.slot[n]);
    for (size_t n=0;result==0 && n<32;++n) snprintf(artifact+n*2U,3,"%02x",(unsigned int)config.request.artifact[n]);
    if (result==0) {
        snprintf(parent_path,sizeof parent_path,ROOT "/fixtures/%s",slot);
        snprintf(local_path,sizeof local_path,"%s/publication",parent_path);
        snprintf(stored_path,sizeof stored_path,ROOT "/store/sha256/%s",artifact);
        snprintf(payload_path,sizeof payload_path,"%s/staging",parent_path);
    }
    int parent=-1,store=-1,container=-1,payload=-1;
    int local=1,staged=1;
    if (result==0) { parent=protected_directory(parent_path,0700,"aetheri_stage_parent_t"); if (parent<0) result=parent; }
    if (result==0) { store=protected_directory(ROOT "/store/sha256",0700,"aetheri_store_parent_t"); if (store<0) result=store; }
    if (result==0) {
        container=publication_directory(local_path,"aetheri_publication_t");
        if (container==-ENOENT && !observe) { local=0; container=publication_directory(stored_path,"aetheri_publication_t"); }
        if (container<0) result=container;
    }
    if (result==0) {
        payload=publication_directory(payload_path,"aetheri_staging_t");
        if (payload==-ENOENT && !observe) {
            staged=0;
            snprintf(payload_path,sizeof payload_path,"%s/payload",local ? local_path : stored_path);
            payload=publication_directory(payload_path,"aetheri_staging_t");
        }
        if (payload<0) result=payload;
    }
    if (result==0) result=root_matches(payload,&config.staging);
    if (result==0 && observe) {
        uint8_t roots[72]; int empty=0;
        result=is_empty(container,&empty);
        if (result==0 && !empty) result=-EEXIST;
        if (result==0) result=witness_encode(store,roots);
        if (result==0) result=witness_encode(container,roots+36);
        if (result==0) {
            fputs("fixture-store-roots ",stdout);
            for (size_t n=0;n<sizeof roots;++n) printf("%02x",(unsigned int)roots[n]);
            putchar('\n');
        }
    } else if (result==0) {
        result=root_matches(store,&publication_config.store);
        if (result==0) result=root_matches(container,&publication_config.container);
        if (result==0 && !progress.publication_present) {
            int empty=0;
            result=is_empty(container,&empty);
            struct stat info;
            if (result==0 && (!local || !staged || !empty)) result=-ESTALE;
            if (result==0 && fstatat(store,artifact,&info,AT_SYMLINK_NOFOLLOW)==0) result=-EEXIST;
            else if (result==0 && errno!=ENOENT) result=-errno;
            if (result==0) result=ae_catalogue_fixture_publication(catalogue,&config.request,manifest,size,AE_FIXTURE_PLANNED,evidence,&progress);
        }
        if (result==0 && progress.publication_step!=AE_FIXTURE_READY)
            result=ae_catalogue_fixture_publication(catalogue,&config.request,manifest,size,AE_FIXTURE_EXECUTING,evidence,&progress);
        if (result==0) {
            struct ae_fixture_publication publication={parent,parent,store,payload,container,"staging","publication"};
            result=ae_fixture_publish_namespace(&publication,manifest,size,config.request.artifact);
            if (result==0) result=seal_publication(container,payload,&fixture);
            if (result==0) result=ae_fixture_publish_namespace(&publication,manifest,size,config.request.artifact);
            for (uint32_t n=0;result==0 && n<fixture.file_count;++n) if (fixture.files[n].mode==0555)
                result=no_executable_mapping(payload,&fixture.files[n]);
            if (result==0) result=no_store_setattr(container,0555);
            if (result==0) result=no_store_setattr(payload,0555);
            for (uint32_t n=0;result==0 && n<fixture.file_count;++n) {
                char name[257];
                memcpy(name,fixture.files[n].path.data,fixture.files[n].path.size); name[fixture.files[n].path.size]='\0';
                struct open_how how={.flags=O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK,
                    .resolve=RESOLVE_BENEATH | RESOLVE_NO_XDEV | RESOLVE_NO_SYMLINKS | RESOLVE_NO_MAGICLINKS};
                int fd=(int)syscall(SYS_openat2,payload,name,&how,sizeof how);
                if (fd<0) result=-errno;
                else result=no_store_setattr(fd,(mode_t)fixture.files[n].mode);
                result=close_owned(fd,result);
            }
            if (result==0) result=root_matches(container,&publication_config.container);
            if (result==0) result=root_matches(payload,&config.staging);
            if (result==0) result=ae_catalogue_fixture_publication(catalogue,&config.request,manifest,size,AE_FIXTURE_READY,evidence,&progress);
            if (result!=0 && progress.publication_step!=AE_FIXTURE_READY) {
                struct ae_fixture_progress ignored;
                int marked=ae_catalogue_fixture_publication(catalogue,&config.request,manifest,size,AE_FIXTURE_UNCERTAIN,evidence,&ignored);
                if (marked!=0) fprintf(stderr,"Fixture publication uncertainty acknowledgement failed: %d\n",marked);
            }
        }
        if (result==0) puts(progress.activation_present ? "fixture-store verified; original install operation completed; current launch readiness not implied" :
            "fixture-store verified; generation committed; install preparing; operation running/reconcile; activation pending");
    }
    result=close_owned(payload,result); result=close_owned(container,result);
    result=close_owned(store,result); result=close_owned(parent,result);
    free(manifest);
    return result;
}

int ae_authority_fixture_activate(struct ae_catalogue *catalogue,const uint8_t current_boot[16])
{
    uint8_t bytes[AE_FIXTURE_CONFIG_BYTES],store_bytes[AE_FIXTURE_STORE_CONFIG_BYTES],activation_bytes[AE_FIXTURE_ACTIVATION_CONFIG_BYTES],store_digest[32];
    uint8_t *manifest=malloc(AE_FIXTURE_MAX_BYTES);
    if (manifest==NULL) return -ENOMEM;
    size_t config_size,size,store_size,activation_size;
    struct ae_fixture_config config;
    struct ae_fixture_activation activation;
    struct ae_fixture_progress progress={0};
    int result=protected_bytes(ROOT "/registry/fixture.conf",bytes,sizeof bytes,&config_size);
    if (result==0) result=ae_fixture_config_decode(bytes,config_size,&config);
    if (result==0) result=protected_bytes(ROOT "/registry/fixture.manifest",manifest,AE_FIXTURE_MAX_BYTES,&size);
    if (result==0) result=ae_catalogue_fixture_get(catalogue,&config.request,manifest,size,&progress);
    if (result==0 && (!progress.publication_present || progress.publication_step!=AE_FIXTURE_READY)) result=-EINVAL;
    if (result==0) result=protected_bytes(ROOT "/registry/fixture-store.conf",store_bytes,sizeof store_bytes,&store_size);
    if (result==0 && store_size!=sizeof store_bytes) result=-EBADMSG;
    if (result==0) result=ae_sha256_bytes(store_bytes,store_size,store_digest);
    if (result==0) result=protected_bytes(ROOT "/registry/fixture-activation.conf",activation_bytes,sizeof activation_bytes,&activation_size);
    if (result==0) result=ae_fixture_activation_config_decode(activation_bytes,activation_size,&activation);
    if (result==0) result=ae_sha256_bytes(activation_bytes,activation_size,activation.expected_evidence);
    if (result==0 && memcmp(activation.publication_evidence,store_digest,32)!=0) result=-ESTALE;
    /* Before first activation, repeat exact object/label/mode/hash/denial/flush
     * checks on already applied publication. Completed retries return the same
     * historical receipt, even after a newer generation or removal; no rearm. */
    if (result==0 && !progress.activation_present) result=ae_authority_fixture_publication(catalogue,0);
    if (result==0) result=ae_catalogue_fixture_activate(catalogue,&config.request,manifest,size,&activation,current_boot,&progress);
    if (result==0) puts("fixture-activation receipt verified; no app dispatched by this action; receipt is not launch readiness");
    free(manifest);
    return result;
}
