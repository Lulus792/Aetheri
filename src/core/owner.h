#ifndef AETHERI_CORE_OWNER_H
#define AETHERI_CORE_OWNER_H

#include <stddef.h>
#include <stdint.h>

enum { AE_ID_BYTES = 16, AE_OWNER_BYTES = 104 };
enum ae_owner_field { AE_MACHINE, AE_VOLUME, AE_ACCOUNT, AE_STORAGE, AE_DATA, AE_ALLOCATION, AE_OWNER_FIELDS };
struct ae_owner { uint8_t id[AE_OWNER_FIELDS][AE_ID_BYTES]; };

/* No allocation. Outputs stay unchanged on failure; success is 0, failure -1. */
int ae_id_from_hex(const char *hex, size_t length, uint8_t output[AE_ID_BYTES]);
int ae_owner_encode(const struct ae_owner *owner, uint8_t *output, size_t capacity);
int ae_owner_decode(const uint8_t *bytes, size_t length, struct ae_owner *output);
int ae_owner_matches(const struct ae_owner *left, const struct ae_owner *right);

#endif
