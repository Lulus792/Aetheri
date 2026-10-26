#include "core/digest.h"

#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/provider.h>

struct digest {
    OSSL_LIB_CTX *library;
    OSSL_PROVIDER *provider;
    EVP_MD *algorithm;
    EVP_MD_CTX *context;
};

static int destroy(struct digest *digest)
{
    EVP_MD_CTX_free(digest->context);
    EVP_MD_free(digest->algorithm);
    int result = 0;
    if (digest->provider != NULL && OSSL_PROVIDER_unload(digest->provider) != 1) result = -EIO;
    if (digest->library != NULL) OPENSSL_thread_stop_ex(digest->library);
    OSSL_LIB_CTX_free(digest->library);
    return result;
}

static int begin(struct digest *digest)
{
    /* An ABI-compatible runtime replacement must still belong to the reviewed
     * family and meet its maintenance floor. No TLS or legacy digest APIs. */
    unsigned long version = OpenSSL_version_num();
    if (version < 0x30500090UL || version >= 0x30600000UL) return -EIO;
    digest->library = OSSL_LIB_CTX_new();
    if (digest->library == NULL) return -ENOMEM;
    digest->provider = OSSL_PROVIDER_load(digest->library, "default");
    if (digest->provider == NULL) return -EIO;
    digest->algorithm = EVP_MD_fetch(digest->library, "SHA256", "provider=default");
    if (digest->algorithm == NULL || EVP_MD_get_size(digest->algorithm) != AE_SHA256_BYTES) return -EIO;
    digest->context = EVP_MD_CTX_new();
    if (digest->context == NULL) return -ENOMEM;
    if (EVP_DigestInit_ex2(digest->context, digest->algorithm, NULL) != 1) return -EIO;
    return 0;
}

static int finish(struct digest *digest, int result, uint8_t output[AE_SHA256_BYTES])
{
    uint8_t value[EVP_MAX_MD_SIZE];
    unsigned int size = 0;
    if (result == 0 && (EVP_DigestFinal_ex(digest->context, value, &size) != 1 || size != AE_SHA256_BYTES)) result = -EIO;
    int cleanup = destroy(digest);
    if (result == 0) result = cleanup;
    if (result == 0) memcpy(output, value, AE_SHA256_BYTES);
    return result;
}

int ae_sha256_bytes(const void *bytes, size_t size, uint8_t output[AE_SHA256_BYTES])
{
    if (output == NULL || (bytes == NULL && size != 0)) return -EINVAL;
    if (size > AE_DIGEST_MAX_BYTES) return -E2BIG;
    struct digest digest = {0};
    int result = begin(&digest);
    if (result == 0 && size != 0 && EVP_DigestUpdate(digest.context, bytes, size) != 1) result = -EIO;
    return finish(&digest, result, output);
}

static int unchanged(const struct stat *left, const struct stat *right)
{
    return left->st_dev == right->st_dev && left->st_ino == right->st_ino &&
        left->st_mode == right->st_mode && left->st_uid == right->st_uid && left->st_gid == right->st_gid &&
        left->st_nlink == right->st_nlink && left->st_size == right->st_size &&
        left->st_mtim.tv_sec == right->st_mtim.tv_sec && left->st_mtim.tv_nsec == right->st_mtim.tv_nsec &&
        left->st_ctim.tv_sec == right->st_ctim.tv_sec && left->st_ctim.tv_nsec == right->st_ctim.tv_nsec;
}

int ae_sha256_fd(int fd, uint64_t expected_size, uint8_t output[AE_SHA256_BYTES])
{
    if (fd < 0 || output == NULL) return -EINVAL;
    if (expected_size > AE_DIGEST_MAX_BYTES) return -E2BIG;
    struct stat before, after;
    if (fstat(fd, &before) != 0) return -errno;
    if (!S_ISREG(before.st_mode)) return -EINVAL;
    if (before.st_size < 0 || (uint64_t)before.st_size != expected_size) return -EBADMSG;
    struct digest digest = {0};
    int result = begin(&digest);
    uint8_t bytes[65536];
    uint64_t position = 0;
    while (result == 0 && position < expected_size) {
        size_t count = sizeof bytes;
        if (expected_size - position < count) count = (size_t)(expected_size - position);
        ssize_t received = pread(fd, bytes, count, (off_t)position);
        if (received < 0) { if (errno == EINTR) continue; result = -errno; }
        else if (received == 0) result = -EBADMSG;
        else {
            if ((size_t)received > count) { result = -EIO; break; }
            if (EVP_DigestUpdate(digest.context, bytes, (size_t)received) != 1) result = -EIO;
            position += (uint64_t)received;
        }
    }
    if (result == 0) {
        if (fstat(fd, &after) != 0) result = -errno;
        else if (!unchanged(&before, &after)) result = -ESTALE;
    }
    return finish(&digest, result, output);
}
