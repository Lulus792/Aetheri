#include "core/digest.h"
#include "core/fixture.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/provider.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "digest line %d: %s\n", __LINE__, #x); exit(1); } } while (0)

enum fault { NONE, RUNTIME_OLD, RUNTIME_NEW, LIBRARY, PROVIDER, FETCH, SIZE, CONTEXT, INIT, UPDATE, FINAL, FINAL_SIZE, UNLOAD,
             READ_EINTR, READ_SHORT, READ_EOF, READ_IO, STAT_IO, STAT_DRIFT };
static enum fault fault;
static int observed_fd = -1, stat_calls;

unsigned long __real_OpenSSL_version_num(void);
unsigned long __wrap_OpenSSL_version_num(void)
{
    return fault == RUNTIME_OLD ? 0x30500050UL : fault == RUNTIME_NEW ? 0x30600000UL : __real_OpenSSL_version_num();
}

OSSL_LIB_CTX *__real_OSSL_LIB_CTX_new(void);
OSSL_LIB_CTX *__wrap_OSSL_LIB_CTX_new(void) { return fault == LIBRARY ? NULL : __real_OSSL_LIB_CTX_new(); }
OSSL_PROVIDER *__real_OSSL_PROVIDER_load(OSSL_LIB_CTX *, const char *);
OSSL_PROVIDER *__wrap_OSSL_PROVIDER_load(OSSL_LIB_CTX *ctx, const char *name)
{
    CHECK(ctx != NULL && strcmp(name, "default") == 0);
    return fault == PROVIDER ? NULL : __real_OSSL_PROVIDER_load(ctx, name);
}
int __real_OSSL_PROVIDER_unload(OSSL_PROVIDER *);
int __wrap_OSSL_PROVIDER_unload(OSSL_PROVIDER *provider)
{
    int result = __real_OSSL_PROVIDER_unload(provider);
    return fault == UNLOAD ? 0 : result;
}
EVP_MD *__real_EVP_MD_fetch(OSSL_LIB_CTX *, const char *, const char *);
EVP_MD *__wrap_EVP_MD_fetch(OSSL_LIB_CTX *ctx, const char *name, const char *properties)
{
    CHECK(ctx != NULL && strcmp(name, "SHA256") == 0 && strcmp(properties, "provider=default") == 0);
    return fault == FETCH ? NULL : __real_EVP_MD_fetch(ctx, name, properties);
}
int __real_EVP_MD_get_size(const EVP_MD *);
int __wrap_EVP_MD_get_size(const EVP_MD *md) { return fault == SIZE ? 64 : __real_EVP_MD_get_size(md); }
EVP_MD_CTX *__real_EVP_MD_CTX_new(void);
EVP_MD_CTX *__wrap_EVP_MD_CTX_new(void) { return fault == CONTEXT ? NULL : __real_EVP_MD_CTX_new(); }
int __real_EVP_DigestInit_ex2(EVP_MD_CTX *, const EVP_MD *, const OSSL_PARAM *);
int __wrap_EVP_DigestInit_ex2(EVP_MD_CTX *ctx, const EVP_MD *md, const OSSL_PARAM *params)
{ return fault == INIT ? 0 : __real_EVP_DigestInit_ex2(ctx, md, params); }
int __real_EVP_DigestUpdate(EVP_MD_CTX *, const void *, size_t);
int __wrap_EVP_DigestUpdate(EVP_MD_CTX *ctx, const void *data, size_t size)
{ return fault == UPDATE ? 0 : __real_EVP_DigestUpdate(ctx, data, size); }
int __real_EVP_DigestFinal_ex(EVP_MD_CTX *, unsigned char *, unsigned int *);
int __wrap_EVP_DigestFinal_ex(EVP_MD_CTX *ctx, unsigned char *bytes, unsigned int *size)
{
    if (fault == FINAL) return 0;
    int result = __real_EVP_DigestFinal_ex(ctx, bytes, size);
    if (fault == FINAL_SIZE) *size = 0;
    return result;
}
ssize_t __real_pread(int, void *, size_t, off_t);
ssize_t __wrap_pread(int fd, void *bytes, size_t size, off_t offset)
{
    CHECK(fd == observed_fd);
    if (fault == READ_EINTR) { fault = NONE; errno = EINTR; return -1; }
    if (fault == READ_IO) { errno = EIO; return -1; }
    if (fault == READ_EOF) return 0;
    if (fault == READ_SHORT && size > 1) size = 1;
    return __real_pread(fd, bytes, size, offset);
}
int __real_fstat(int, struct stat *);
int __wrap_fstat(int fd, struct stat *stat)
{
    if (fd == observed_fd) {
        ++stat_calls;
        if (fault == STAT_IO) { errno = EIO; return -1; }
    }
    int result = __real_fstat(fd, stat);
    if (result == 0 && fd == observed_fd && fault == STAT_DRIFT && stat_calls == 2) ++stat->st_nlink;
    return result;
}

static uint32_t word(FILE *input)
{
    uint8_t bytes[4];
    CHECK(fread(bytes, 1, sizeof bytes, input) == sizeof bytes);
    return ((uint32_t)bytes[0] << 24) | ((uint32_t)bytes[1] << 16) | ((uint32_t)bytes[2] << 8) | bytes[3];
}

static void replace(int fd, const uint8_t *bytes, size_t size)
{
    CHECK(ftruncate(fd, 0) == 0);
    size_t position = 0;
    while (position < size) {
        ssize_t count = pwrite(fd, bytes + position, size - position, (off_t)position);
        CHECK(count > 0);
        position += (size_t)count;
    }
    CHECK(lseek(fd, 1, SEEK_SET) == 1);
}

int main(int argc, char **argv)
{
    CHECK(argc == 3);
    FILE *input = fopen(argv[1], "rb");
    CHECK(input != NULL);
    int fd = open(argv[2], O_RDWR | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
    CHECK(fd >= 0);
    observed_fd = fd;
    uint8_t digest[AE_SHA256_BYTES], expected[AE_SHA256_BYTES], marker[AE_SHA256_BYTES];
    memset(marker, 0xa5, sizeof marker);
    uint8_t *bytes = malloc(AE_DIGEST_MAX_BYTES + 1U);
    CHECK(bytes != NULL);
    uint32_t cases = word(input);
    CHECK(cases > 0 && cases <= 64);
    for (uint32_t n = 0; n < cases; ++n) {
        uint32_t size = word(input);
        CHECK(size <= AE_DIGEST_MAX_BYTES);
        CHECK(fread(expected, 1, sizeof expected, input) == sizeof expected);
        CHECK(fread(bytes, 1, size, input) == size);
        CHECK(ae_sha256_bytes(size == 0 ? NULL : bytes, size, digest) == 0);
        CHECK(memcmp(digest, expected, sizeof digest) == 0);
        replace(fd, bytes, size);
        CHECK(ae_sha256_fd(fd, size, digest) == 0);
        CHECK(memcmp(digest, expected, sizeof digest) == 0);
        CHECK(lseek(fd, 0, SEEK_CUR) == 1 && fcntl(fd, F_GETFD) >= 0);
        struct ae_fixture_file file = {.size = size};
        memcpy(file.sha256, expected, sizeof expected);
        CHECK(ae_fixture_verify_file(&file, fd) == 0);
        file.sha256[0] ^= 1;
        CHECK(ae_fixture_verify_file(&file, fd) == -EBADMSG);
    }
    /* Canonical manifest + independent trusted ID, not a newly supplied hash. */
    uint32_t size = word(input);
    CHECK(size <= AE_FIXTURE_MAX_BYTES);
    CHECK(fread(expected, 1, sizeof expected, input) == sizeof expected);
    CHECK(fread(bytes, 1, size, input) == size);
    struct ae_fixture fixture, unchanged;
    memset(&unchanged, 0xa5, sizeof unchanged);
    CHECK(ae_fixture_verify_manifest(bytes, size, expected, &fixture) == 0);
    CHECK(fixture.file_count == 2 && fixture.files[0].size == 28 && fixture.files[1].size == 6);
    fixture = unchanged;
    bytes[13] ^= 1; /* Still valid component text, different canonical bytes. */
    CHECK(ae_fixture_decode(bytes, size, &fixture) == 0);
    fixture = unchanged;
    CHECK(ae_fixture_verify_manifest(bytes, size, expected, &fixture) == -EBADMSG);
    CHECK(memcmp(&fixture, &unchanged, sizeof fixture) == 0);
    bytes[13] ^= 1;
    CHECK(ae_fixture_verify_manifest(bytes, size, NULL, &fixture) == -EINVAL);
    CHECK(ae_fixture_verify_manifest(bytes, size, expected, NULL) == -EINVAL);
    CHECK(ae_fixture_verify_file(NULL, fd) == -EINVAL);
    CHECK(fgetc(input) == EOF && !ferror(input) && fclose(input) == 0);

    memcpy(bytes, "abc", 3);
    replace(fd, bytes, 3);
    CHECK(ae_sha256_bytes(bytes, 3, expected) == 0);
    struct ae_fixture_file file = {.size = 3};
    memcpy(file.sha256, expected, sizeof expected);
    CHECK(pwrite(fd, "abd", 3, 0) == 3);
    CHECK(ae_fixture_verify_file(&file, fd) == -EBADMSG);
    replace(fd, bytes, 3);
    for (enum fault value = RUNTIME_OLD; value <= UNLOAD; value++) {
        fault = value;
        int error = value == LIBRARY || value == CONTEXT ? -ENOMEM : -EIO;
        memcpy(digest, marker, sizeof digest);
        CHECK(ae_sha256_bytes(bytes, 3, digest) == error);
        CHECK(memcmp(digest, marker, sizeof digest) == 0);
        CHECK(ae_sha256_fd(fd, 3, digest) == error);
        CHECK(memcmp(digest, marker, sizeof digest) == 0);
    }
    for (enum fault value = READ_EINTR; value <= STAT_DRIFT; value++) {
        fault = value;
        stat_calls = 0;
        memcpy(digest, marker, sizeof digest);
        int result = ae_sha256_fd(fd, 3, digest);
        if (value == READ_EINTR || value == READ_SHORT) {
            CHECK(result == 0 && memcmp(digest, expected, sizeof digest) == 0);
        } else {
            int error = value == READ_EOF ? -EBADMSG : value == STAT_DRIFT ? -ESTALE : -EIO;
            CHECK(result == error && memcmp(digest, marker, sizeof digest) == 0);
        }
        CHECK(lseek(fd, 0, SEEK_CUR) == 1 && fcntl(fd, F_GETFD) >= 0);
    }
    fault = NONE;
    memcpy(digest, marker, sizeof digest);
    CHECK(ae_sha256_bytes(NULL, 1, digest) == -EINVAL);
    CHECK(ae_sha256_bytes(bytes, 1, NULL) == -EINVAL);
    CHECK(ae_sha256_bytes(bytes, AE_DIGEST_MAX_BYTES + 1U, digest) == -E2BIG);
    CHECK(ae_sha256_fd(fd, AE_DIGEST_MAX_BYTES + 1U, digest) == -E2BIG);
    CHECK(ae_sha256_fd(fd, 2, digest) == -EBADMSG);
    CHECK(ae_sha256_fd(fd, 4, digest) == -EBADMSG);
    CHECK(ae_sha256_fd(-1, 0, digest) == -EINVAL);
    CHECK(ae_sha256_fd(fd, 3, NULL) == -EINVAL);
    int pipefd[2];
    CHECK(pipe(pipefd) == 0);
    CHECK(ae_sha256_fd(pipefd[0], 0, digest) == -EINVAL);
    CHECK(close(pipefd[0]) == 0 && close(pipefd[1]) == 0);
    CHECK(memcmp(digest, marker, sizeof digest) == 0);
    CHECK(close(fd) == 0);
    CHECK(ae_sha256_fd(fd, 3, digest) == -EBADF);
    CHECK(unlink(argv[2]) == 0);
    free(bytes);
    puts("Native SHA-256 vectors, held-FD verification and fault paths passed.");
    return 0;
}
