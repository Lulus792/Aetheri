#include "core/owner.h"

#include <string.h>

static const uint8_t magic[8] = {'A','E','T','H','O','W','N','1'};
_Static_assert(AE_OWNER_BYTES == sizeof magic + AE_OWNER_FIELDS * AE_ID_BYTES, "owner byte layout");

static int nonzero(const uint8_t id[AE_ID_BYTES])
{
    unsigned int value = 0;
    for (size_t n = 0; n < AE_ID_BYTES; ++n) value |= id[n];
    return value != 0U;
}

static int nibble(unsigned char value)
{
    if (value >= '0' && value <= '9') return (int)(value - '0');
    if (value >= 'a' && value <= 'f') return (int)(value - 'a') + 10;
    return -1;
}

int ae_id_from_hex(const char *hex, size_t length, uint8_t output[AE_ID_BYTES])
{
    uint8_t temporary[AE_ID_BYTES];
    if (hex == NULL || output == NULL || length != AE_ID_BYTES * 2U) return -1;
    for (size_t n = 0; n < AE_ID_BYTES; ++n) {
        int high = nibble((unsigned char)hex[n * 2U]);
        int low = nibble((unsigned char)hex[n * 2U + 1U]);
        if (high < 0 || low < 0) return -1;
        temporary[n] = (uint8_t)((unsigned int)high * 16U + (unsigned int)low);
    }
    if (!nonzero(temporary)) return -1;
    memcpy(output, temporary, sizeof temporary);
    return 0;
}

int ae_owner_encode(const struct ae_owner *owner, uint8_t *output, size_t capacity)
{
    uint8_t temporary[AE_OWNER_BYTES];
    if (owner == NULL || output == NULL || capacity < AE_OWNER_BYTES) return -1;
    memcpy(temporary, magic, sizeof magic);
    for (size_t n = 0; n < AE_OWNER_FIELDS; ++n) {
        if (!nonzero(owner->id[n])) return -1;
        memcpy(temporary + sizeof magic + n * AE_ID_BYTES, owner->id[n], AE_ID_BYTES);
    }
    memcpy(output, temporary, sizeof temporary);
    return 0;
}

int ae_owner_decode(const uint8_t *bytes, size_t length, struct ae_owner *output)
{
    struct ae_owner temporary;
    if (bytes == NULL || output == NULL || length != AE_OWNER_BYTES) return -1;
    if (memcmp(bytes, magic, sizeof magic) != 0) return -1;
    for (size_t n = 0; n < AE_OWNER_FIELDS; ++n) {
        memcpy(temporary.id[n], bytes + sizeof magic + n * AE_ID_BYTES, AE_ID_BYTES);
        if (!nonzero(temporary.id[n])) return -1;
    }
    *output = temporary;
    return 0;
}

int ae_owner_matches(const struct ae_owner *left, const struct ae_owner *right)
{
    if (left == NULL || right == NULL) return 0;
    for (size_t n = 0; n < AE_OWNER_FIELDS; ++n)
        if (memcmp(left->id[n], right->id[n], AE_ID_BYTES) != 0) return 0;
    return 1;
}
