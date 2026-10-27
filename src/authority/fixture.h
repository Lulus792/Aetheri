#ifndef AETHERI_AUTHORITY_FIXTURE_H
#define AETHERI_AUTHORITY_FIXTURE_H

#include "catalogue/store.h"
#include "core/root.h"

enum { AE_FIXTURE_CONFIG_BYTES = 328, AE_FIXTURE_ROOT_BYTES = 36 };
enum { AE_FIXTURE_STORE_CONFIG_BYTES = 112 };
enum { AE_FIXTURE_ACTIVATION_CONFIG_BYTES = 72 };
/* Trusted administrative selection only. No caller-supplied registry paths. */
enum ae_fixture_selection { AE_FIXTURE_ORIGINAL, AE_FIXTURE_EXEC };
struct ae_fixture_registry {
    const char *config,*manifest,*publication,*activation,*admission;
    const char *data_owner,*data_config,*data_context;
};
const struct ae_fixture_registry *ae_fixture_registry(enum ae_fixture_selection selection);
struct ae_fixture_store_config {
    uint8_t stage_digest[32];
    struct ae_root_identity store, container;
};
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
int ae_authority_fixture(enum ae_fixture_selection selection, struct ae_catalogue *catalogue, int observe, const uint8_t current_boot[16]);
int ae_fixture_store_config_decode(const uint8_t *bytes,size_t size,struct ae_fixture_store_config *output);
int ae_authority_fixture_publication(enum ae_fixture_selection selection,struct ae_catalogue *catalogue,int observe);
/* AETHACT1: publication-config hash, install/generation/closed-gate revisions,
 * administrative completion stamp (four big-endian u64). Caller hashes the
 * protected bytes into expected_evidence after decoding; decoder leaves it zero.
 */
int ae_fixture_activation_config_decode(const uint8_t *bytes,size_t size,struct ae_fixture_activation *output);
int ae_authority_fixture_activate(enum ae_fixture_selection selection,struct ae_catalogue *catalogue,const uint8_t current_boot[16]);
/* Read only the fixed protected administrative selection; no fixture enrollment,
 * root observation, catalogue mutation or installation status is inferred. */
int ae_authority_fixture_request(enum ae_fixture_selection selection,struct ae_fixture_request *output);
int ae_authority_projection_unit(char output[256]);
int ae_authority_fixture_admit(enum ae_fixture_selection selection,struct ae_catalogue *catalogue,const uint8_t boot[16],const uint8_t runtime[16]);

#endif
