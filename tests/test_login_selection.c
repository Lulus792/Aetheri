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

static void sign_body(EVP_PKEY *key, uint8_t *record, size_t body)
{
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    CHECK(ctx);
    CHECK(EVP_DigestSignInit(ctx, NULL, NULL, NULL, key) == 1);
    size_t count = AE_LOGIN_SELECTION_SIGNATURE_BYTES;
    CHECK(EVP_DigestSign(ctx, record + body, &count, record, body) == 1);
    CHECK(count == AE_LOGIN_SELECTION_SIGNATURE_BYTES);
    EVP_MD_CTX_free(ctx);
}

static void sign_record(EVP_PKEY *key, uint8_t *record)
{
    sign_body(key, record, AE_LOGIN_SELECTION_BODY_BYTES);
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

    uint8_t record[AE_LOGIN_SELECTION_BYTES] = "AETHLS02";
    if (argc >= 2 && argc <= 4) {
        uint8_t external[AE_LOGIN_SELECTION_LOCK_BYTES];
        int offset = argc == 4 ? 2 : 1;
        if (argc == 4) CHECK(!strcmp(argv[1], "--sign-body"));
        FILE *file = fopen(argv[offset], "rb");
        CHECK(file);
        size_t body = fread(external, 1, AE_LOGIN_SELECTION_LOCK_BODY_BYTES, file);
        CHECK(body == AE_LOGIN_SELECTION_BODY_BYTES || body == AE_LOGIN_SELECTION_SOUND_BODY_BYTES ||
              body == AE_LOGIN_SELECTION_LOCK_BODY_BYTES);
        CHECK(fgetc(file) == EOF && !ferror(file) && !fclose(file));
        sign_body(key, external, body);
        size_t length = body + AE_LOGIN_SELECTION_SIGNATURE_BYTES;
        struct ae_login_selection selected;
        CHECK(!ae_login_selection_verify(external, length, public_key, release, &selected));
        if (argc != 4) {
            CHECK(selected.uid == 992 && selected.gid == 993 && selected.group_count == 2);
            CHECK(selected.groups[0] == 993 && selected.groups[1] == 60000);
            CHECK(selected.files[AE_LOGIN_SELECT_SYSTEMD_DROPIN].size == 21);
            CHECK(selected.files[AE_LOGIN_SELECT_KERNEL_POLICY].size == 2);
            CHECK(selected.files[AE_LOGIN_SELECT_SETTINGS].size == 64 + sizeof("aetheri-settings-nexaui") - 1);
            CHECK(selected.files[AE_LOGIN_SELECT_EDITOR].size == 64 + sizeof("aetheri-editor") - 1);
            if (body == AE_LOGIN_SELECTION_SOUND_BODY_BYTES)
                CHECK(selected.files[AE_LOGIN_SELECT_SOUND].size ==
                      64 + sizeof("aetheri-audio-desktop-control") - 1);
            if (body == AE_LOGIN_SELECTION_LOCK_BODY_BYTES) {
                CHECK(selected.files[AE_LOGIN_SELECT_LOCK_VIEW].size ==
                      64 + sizeof("aetheri-lock-view") - 1);
                CHECK(selected.files[AE_LOGIN_SELECT_UNLOCK_PAM].size > 0);
            }
        }
        if (argc >= 3) {
            file = fopen(argv[offset + 1], "wbx");
            CHECK(file);
            CHECK(fwrite(external, 1, length, file) == length && !fclose(file));
        }
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
    for (size_t i = 0; i < AE_LOGIN_SELECT_SOUND; ++i) {
        record[115 + 36*i + 3] = (uint8_t)(i + 1);
        record[115 + 36*i + 4] = (uint8_t)(100 + i);
    }
    sign_record(key, record);

    struct ae_login_selection out;
    CHECK(!ae_login_selection_verify(record, sizeof record, public_key, release, &out));
    CHECK(out.uid == 41 && out.gid == 43 && out.group_count == 2);
    CHECK(out.groups[0] == 43 && out.groups[1] == 63 && out.groups[2] == 0);
    for (size_t i = 0; i < AE_LOGIN_SELECT_SOUND; ++i)
        CHECK(out.files[i].size == i + 1 && out.files[i].digest[0] == 100 + i);
    CHECK(!out.files[AE_LOGIN_SELECT_SOUND].size);

    uint8_t sound[AE_LOGIN_SELECTION_SOUND_BYTES] = {0};
    memcpy(sound, record, AE_LOGIN_SELECTION_BODY_BYTES);
    sound[7] = '3';
    size_t role = AE_LOGIN_SELECTION_BODY_BYTES;
    sound[role + 3] = 13;
    sound[role + 4] = 83;
    sign_body(key, sound, AE_LOGIN_SELECTION_SOUND_BODY_BYTES);
    CHECK(!ae_login_selection_verify(sound, sizeof sound, public_key, release, &out));
    CHECK(out.files[AE_LOGIN_SELECT_SOUND].size == 13 && out.files[AE_LOGIN_SELECT_SOUND].digest[0] == 83);
    refused(sound, AE_LOGIN_SELECTION_BYTES, public_key, release, -EBADMSG);
    sound[role + 4] ^= 1;
    refused(sound, sizeof sound, public_key, release, -EKEYREJECTED);
    sound[role + 3] = 0;
    sign_body(key, sound, AE_LOGIN_SELECTION_SOUND_BODY_BYTES);
    refused(sound, sizeof sound, public_key, release, -EBADMSG);
    sound[7] = '2';
    refused(sound, sizeof sound, public_key, release, -EBADMSG);

    uint8_t lock[AE_LOGIN_SELECTION_LOCK_BYTES] = {0};
    memcpy(lock, record, AE_LOGIN_SELECTION_BODY_BYTES);
    lock[7] = '4';
    size_t view = AE_LOGIN_SELECTION_SOUND_BODY_BYTES;
    size_t pam = view + 36;
    lock[view + 3] = 10;
    lock[view + 4] = 91;
    lock[pam + 3] = 11;
    lock[pam + 4] = 92;
    sign_body(key, lock, AE_LOGIN_SELECTION_LOCK_BODY_BYTES);
    CHECK(!ae_login_selection_verify(lock, sizeof lock, public_key, release, &out));
    CHECK(!out.files[AE_LOGIN_SELECT_SOUND].size && out.files[AE_LOGIN_SELECT_LOCK_VIEW].size == 10);
    CHECK(out.files[AE_LOGIN_SELECT_UNLOCK_PAM].size == 11);
    refused(lock, AE_LOGIN_SELECTION_SOUND_BYTES, public_key, release, -EBADMSG);
    lock[role + 4] = 73;
    sign_body(key, lock, AE_LOGIN_SELECTION_LOCK_BODY_BYTES);
    refused(lock, sizeof lock, public_key, release, -EBADMSG);
    lock[role + 3] = 13;
    sign_body(key, lock, AE_LOGIN_SELECTION_LOCK_BODY_BYTES);
    CHECK(!ae_login_selection_verify(lock, sizeof lock, public_key, release, &out));
    CHECK(out.files[AE_LOGIN_SELECT_SOUND].size == 13);
    lock[pam + 4] ^= 1;
    refused(lock, sizeof lock, public_key, release, -EKEYREJECTED);
    lock[pam + 3] = 0;
    sign_body(key, lock, AE_LOGIN_SELECTION_LOCK_BODY_BYTES);
    refused(lock, sizeof lock, public_key, release, -EBADMSG);

    uint8_t another_release[sizeof release] = {7};
    refused(record, sizeof record, public_key, another_release, -EKEYREJECTED);
    refused(record, sizeof record - 1, public_key, release, -EBADMSG);
    refused(record, sizeof record + 1, public_key, release, -EBADMSG);
    record[7] = '1';
    sign_record(key, record);
    refused(record, sizeof record, public_key, release, -EBADMSG);
    record[7] = '2';
    seed[0] = 12;
    EVP_PKEY *other = EVP_PKEY_new_raw_private_key_ex(NULL, "ED25519", NULL, seed, sizeof seed);
    CHECK(other);
    sign_record(other, record);
    EVP_PKEY_free(other);
    refused(record, sizeof record, public_key, release, -EKEYREJECTED);
    sign_record(key, record);
    record[115 + 36 * AE_LOGIN_SELECT_EDITOR + 4] ^= 1;
    refused(record, sizeof record, public_key, release, -EKEYREJECTED);
    record[115 + 36 * AE_LOGIN_SELECT_EDITOR + 4] ^= 1;

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
