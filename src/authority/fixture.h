#ifndef AETHERI_AUTHORITY_FIXTURE_H
#define AETHERI_AUTHORITY_FIXTURE_H

#include "catalogue/store.h"
#include "core/root.h"

enum { AE_FIXTURE_CONFIG_BYTES = 328, AE_FIXTURE_ROOT_BYTES = 36 };
struct ae_fixture_config {
    struct ae_fixture_request request;
    struct ae_root_identity source, staging;
};
/* Internal protected P0 carrier configuration, not client input or package ABI.
 * AETHFIC1: ten IDs in request order, artifact, three u64 request numbers,
 * source/staging 36-byte object witnesses, expected canonical intent digest.
 * Root witnesses may be zero only during the read-only provisioning observation;
 * actual staging requires exact existing filesystem/inode/birth witnesses.
 */
int ae_fixture_config_decode(const uint8_t *bytes, size_t size, struct ae_fixture_config *output);
int ae_authority_fixture(struct ae_catalogue *catalogue, int observe, const uint8_t current_boot[16]);

#endif
