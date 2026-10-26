#ifndef AETHERI_CORE_FIXTURE_H
#define AETHERI_CORE_FIXTURE_H

#include "core/owner.h"

enum { AE_FIXTURE_MAX_BYTES = 131072, AE_FIXTURE_FILES = 32,
       AE_FIXTURE_ACTIONS = 8, AE_FIXTURE_ARGUMENTS = 8, AE_FIXTURE_DEPENDENCIES = 8 };

/* Borrowed, validated UTF-8 bytes; not NUL-terminated. */
struct ae_fixture_text { const char *data; size_t size; };
struct ae_fixture_file {
    struct ae_fixture_text path;
    uint32_t mode;
    uint64_t size;
    uint8_t sha256[32];
};
struct ae_fixture_action {
    struct ae_fixture_text id;
    uint32_t file_index, argument_count;
    struct ae_fixture_text arguments[AE_FIXTURE_ARGUMENTS];
};
struct ae_fixture {
    struct ae_fixture_text component, version, target;
    uint8_t publisher[AE_ID_BYTES];
    uint32_t file_count, action_count, dependency_count;
    struct ae_fixture_file files[AE_FIXTURE_FILES];
    struct ae_fixture_action actions[AE_FIXTURE_ACTIONS];
    uint8_t dependencies[AE_FIXTURE_DEPENDENCIES][32];
};

/* Decode only the canonical AETHFIX1 byte contract in spec/runtime.md.
 * No allocation, hashing, filesystem access, publisher trust or installation.
 * Input must remain immutable/alive while the returned views are used; output
 * must not overlap input. Outputs stay unchanged on error. Unknown/malformed
 * content: -EBADMSG; oversized input: -E2BIG; invalid pointer arguments: -EINVAL.
 * Empty unused slots are zeroed. Never pass text.data directly to a C string API.
 */
int ae_fixture_decode(const uint8_t *bytes, size_t size, struct ae_fixture *output);

#endif
