#include "login/selection.h"
#include <errno.h>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/provider.h>
#include <stdbool.h>
#include <string.h>

static const uint8_t magic[] = "AETHLS01";

static const uint32_t limits[AE_LOGIN_SELECT_COUNT] = {
    [AE_LOGIN_SELECT_UNIT] = 65536,
    [AE_LOGIN_SELECT_STARTUP_UNIT] = 65536,
    [AE_LOGIN_SELECT_SYSTEMD_DROPIN] = 65536,
    [AE_LOGIN_SELECT_WORKER_DROPIN] = 65536,
    [AE_LOGIN_SELECT_BACKEND] = AE_DIGEST_MAX_BYTES,
    [AE_LOGIN_SELECT_CONFIG] = 65536,
    [AE_LOGIN_SELECT_GREETER] = AE_DIGEST_MAX_BYTES,
    [AE_LOGIN_SELECT_DISPLAY] = AE_DIGEST_MAX_BYTES,
    [AE_LOGIN_SELECT_FONT] = AE_DIGEST_MAX_BYTES,
    [AE_LOGIN_SELECT_USER_PAM] = 65536,
    [AE_LOGIN_SELECT_GREETER_PAM] = 65536,
    [AE_LOGIN_SELECT_SESSION] = AE_DIGEST_MAX_BYTES,
    [AE_LOGIN_SELECT_COMPOSITOR] = AE_DIGEST_MAX_BYTES,
    [AE_LOGIN_SELECT_DESKTOP] = AE_DIGEST_MAX_BYTES,
    [AE_LOGIN_SELECT_STARTUP] = AE_DIGEST_MAX_BYTES,
    [AE_LOGIN_SELECT_KERNEL_POLICY] = AE_DIGEST_MAX_BYTES,
};

static uint32_t read_u32(const uint8_t *p)
{
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}

static bool nonzero(const uint8_t *bytes, size_t size)
{
    uint8_t any = 0;
    for (size_t i = 0; i < size; ++i) any |= bytes[i];
    return any != 0;
}

static int verify(const uint8_t *body, const uint8_t *public_key)
{
    unsigned long version = OpenSSL_version_num();
    if (version < 0x30500090UL || version >= 0x30600000UL) return -EIO;
    OSSL_LIB_CTX *lib = OSSL_LIB_CTX_new();
    if (!lib) return -ENOMEM;
    OSSL_PROVIDER *provider = OSSL_PROVIDER_load(lib, "default");
    EVP_PKEY *key = provider ? EVP_PKEY_new_raw_public_key_ex(lib, "ED25519",
        "provider=default", public_key, AE_LOGIN_SELECTION_KEY_BYTES) : NULL;
    EVP_MD_CTX *md = key ? EVP_MD_CTX_new() : NULL;
    int result = -EIO;
    if (key && !md) result = -ENOMEM;
    if (md && EVP_DigestVerifyInit_ex(md, NULL, NULL, lib, "provider=default", key, NULL) == 1) {
        int checked = EVP_DigestVerify(md, body + AE_LOGIN_SELECTION_BODY_BYTES,
            AE_LOGIN_SELECTION_SIGNATURE_BYTES, body, AE_LOGIN_SELECTION_BODY_BYTES);
        result = checked == 1 ? 0 : checked == 0 ? -EKEYREJECTED : -EIO;
    }
    EVP_MD_CTX_free(md);
    EVP_PKEY_free(key);
    if (provider && OSSL_PROVIDER_unload(provider) != 1 && !result) result = -EIO;
    OPENSSL_thread_stop_ex(lib);
    OSSL_LIB_CTX_free(lib);
    return result;
}

int ae_login_selection_verify(const uint8_t *bytes, size_t size,
    const uint8_t public_key[AE_LOGIN_SELECTION_KEY_BYTES],
    const uint8_t release[AE_LOGIN_SELECTION_RELEASE_BYTES], struct ae_login_selection *out)
{
    if (!bytes || !public_key || !release || !out ||
        !nonzero(public_key, AE_LOGIN_SELECTION_KEY_BYTES) ||
        !nonzero(release, AE_LOGIN_SELECTION_RELEASE_BYTES)) return -EINVAL;
    if (size != AE_LOGIN_SELECTION_BYTES || memcmp(bytes, magic, sizeof magic - 1))
        return -EBADMSG;
    const uint8_t *p = bytes + sizeof magic - 1;
    if (memcmp(p, release, AE_LOGIN_SELECTION_RELEASE_BYTES)) return -EKEYREJECTED;
    int result = verify(bytes, public_key);
    if (result) return result;

    p += AE_LOGIN_SELECTION_RELEASE_BYTES;
    struct ae_login_selection parsed = {0};
    parsed.uid = read_u32(p);
    parsed.gid = read_u32(p + 4);
    parsed.group_count = p[8];
    p += 9;
    if (!parsed.uid || parsed.uid == UINT32_MAX || !parsed.gid || parsed.gid == UINT32_MAX ||
        parsed.group_count > AE_LOGIN_SELECTION_GROUPS_MAX || p[0] || p[1]) return -EBADMSG;
    p += 2;
    uint32_t last = 0;
    for (size_t i = 0; i < AE_LOGIN_SELECTION_GROUPS_MAX; ++i, p += 4) {
        uint32_t group = read_u32(p);
        if (i >= parsed.group_count) {
            if (group) return -EBADMSG;
        } else {
            if (group <= last || group == UINT32_MAX) return -EBADMSG;
            parsed.groups[i] = last = group;
        }
    }
    for (size_t i = 0; i < AE_LOGIN_SELECT_COUNT; ++i, p += 4 + AE_SHA256_BYTES) {
        struct ae_login_selected_file *file = &parsed.files[i];
        file->size = read_u32(p);
        if (!file->size || file->size > limits[i] || !nonzero(p + 4, AE_SHA256_BYTES))
            return -EBADMSG;
        memcpy(file->digest, p + 4, sizeof file->digest);
    }
    *out = parsed;
    return 0;
}
