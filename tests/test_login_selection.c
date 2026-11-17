#include "login/selection.h"
#include <errno.h>
#include <openssl/evp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "line %d: %s\n", __LINE__, #condition); abort(); \
} } while (0)

static const uint8_t release[AE_LOGIN_SELECTION_RELEASE_BYTES] = {42};

static void sign_record(EVP_PKEY *key, uint8_t *record)
{
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    CHECK(ctx);
    CHECK(EVP_DigestSignInit(ctx, NULL, NULL, NULL, key) == 1);
    size_t count = AE_LOGIN_SELECTION_SIGNATURE_BYTES;
    CHECK(EVP_DigestSign(ctx, record + AE_LOGIN_SELECTION_BODY_BYTES, &count,
                        record, AE_LOGIN_SELECTION_BODY_BYTES) == 1);
    CHECK(count == AE_LOGIN_SELECTION_SIGNATURE_BYTES);
    EVP_MD_CTX_free(ctx);
}

static void refused(const uint8_t *record, size_t size, const uint8_t *key,
                    const uint8_t *release_id, int error)
{
    struct ae_login_selection out;
    unsigned char before[sizeof out];
    memset(&out, 0xa5, sizeof out);
    memcpy(before, &out, sizeof before);
    CHECK(ae_login_selection_verify(record, size, key, release_id, &out) == error);
    CHECK(!memcmp(before, &out, sizeof before));
}

int main(int argc, char **argv)
{
    uint8_t seed[32] = {11}, public_key[AE_LOGIN_SELECTION_KEY_BYTES];
    EVP_PKEY *key = EVP_PKEY_new_raw_private_key_ex(NULL, "ED25519", NULL, seed, sizeof seed);
    CHECK(key);
    size_t key_size = sizeof public_key;
    CHECK(EVP_PKEY_get_raw_public_key(key, public_key, &key_size) == 1);
    CHECK(key_size == sizeof public_key);

    uint8_t record[AE_LOGIN_SELECTION_BYTES] = "AETHLS01";
    if (argc == 2) {
        FILE *file = fopen(argv[1], "rb");
        CHECK(file);
        CHECK(fread(record, 1, AE_LOGIN_SELECTION_BODY_BYTES, file) == AE_LOGIN_SELECTION_BODY_BYTES);
        CHECK(fgetc(file) == EOF && !ferror(file) && !fclose(file));
        sign_record(key, record);
        struct ae_login_selection selected;
        CHECK(!ae_login_selection_verify(record, sizeof record, public_key, release, &selected));
        CHECK(selected.uid == 992 && selected.gid == 993 && selected.group_count == 2);
        CHECK(selected.groups[0] == 993 && selected.groups[1] == 60000);
        CHECK(selected.files[AE_LOGIN_SELECT_SYSTEMD_DROPIN].size == 21);
        CHECK(selected.files[AE_LOGIN_SELECT_KERNEL_POLICY].size == 2);
        EVP_PKEY_free(key);
        return 0;
    }
    CHECK(argc == 1);
    memcpy(record + 8, release, sizeof release);
    record[43] = 41; /* UID */
    record[47] = 43; /* GID */
    record[48] = 2;
    record[54] = 43;
    record[58] = 63;
    for (size_t i = 0; i < AE_LOGIN_SELECT_COUNT; ++i) {
        record[115 + 36*i + 3] = (uint8_t)(i + 1);
        record[115 + 36*i + 4] = (uint8_t)(100 + i);
    }
    sign_record(key, record);

    struct ae_login_selection out;
    CHECK(!ae_login_selection_verify(record, sizeof record, public_key, release, &out));
    CHECK(out.uid == 41 && out.gid == 43 && out.group_count == 2);
    CHECK(out.groups[0] == 43 && out.groups[1] == 63 && out.groups[2] == 0);
    for (size_t i = 0; i < AE_LOGIN_SELECT_COUNT; ++i)
        CHECK(out.files[i].size == i + 1 && out.files[i].digest[0] == 100 + i);

    uint8_t another_release[sizeof release] = {7};
    refused(record, sizeof record, public_key, another_release, -EKEYREJECTED);
    refused(record, sizeof record - 1, public_key, release, -EBADMSG);
    refused(record, sizeof record + 1, public_key, release, -EBADMSG);
    seed[0] = 12;
    EVP_PKEY *other = EVP_PKEY_new_raw_private_key_ex(NULL, "ED25519", NULL, seed, sizeof seed);
    CHECK(other);
    sign_record(other, record);
    EVP_PKEY_free(other);
    refused(record, sizeof record, public_key, release, -EKEYREJECTED);
    sign_record(key, record);
    record[115 + 36 * AE_LOGIN_SELECT_GREETER + 4] ^= 1;
    refused(record, sizeof record, public_key, release, -EKEYREJECTED);
    record[115 + 36 * AE_LOGIN_SELECT_GREETER + 4] ^= 1;

    const struct { size_t offset; uint8_t value; } invalid[] = {
        {43, 0}, {47, 0}, {48, 17}, {49, 1}, {54, 0}, {58, 43}, {62, 1},
        {115, 1}, {118, 0}, {119, 0},
    };
    for (size_t i = 0; i < sizeof invalid / sizeof invalid[0]; ++i) {
        size_t offset = invalid[i].offset;
        uint8_t saved = record[offset];
        CHECK(saved != invalid[i].value);
        record[offset] = invalid[i].value;
        sign_record(key, record);
        refused(record, sizeof record, public_key, release, -EBADMSG);
        record[offset] = saved;
    }

    memset(record + 48, 0, 3 + 4 * AE_LOGIN_SELECTION_GROUPS_MAX);
    sign_record(key, record);
    CHECK(!ae_login_selection_verify(record, sizeof record, public_key, release, &out));
    CHECK(!out.group_count && !out.groups[0]);
    EVP_PKEY_free(key);
    return 0;
}
