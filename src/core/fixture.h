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

/* Decode and hash canonical manifest bytes against an independently trusted
 * expected ArtifactId. Borrowed input/output lifetimes match decode above.
 * -EBADMSG includes a digest mismatch; output is unchanged on every error.
 * This does not authenticate the publisher or enroll the supplied expected ID.
 */
int ae_fixture_verify_manifest(const uint8_t *bytes, size_t size,
                               const uint8_t expected_artifact[32], struct ae_fixture *output);

/* Verify one decoded file's length/digest through a borrowed regular FD. No path
 * opening, ownership/policy decisions, inventory check or installation. See
 * digest.h for mutation/offset semantics. file must be from a validated manifest.
 */
int ae_fixture_verify_file(const struct ae_fixture_file *file, int fd);

/* Owned metadata for one exact declared action. argv[0] is its manifest-relative
 * executable, followed by at most eight literal arguments. No shell expansion,
 * PATH lookup, environment, fallback action or filesystem authority. Manifest
 * bytes must be immutable for the call; expected artifact is independently
 * trusted. Output does not borrow those bytes and is unchanged on error. */
struct ae_fixture_command {
    char executable[257],arguments[AE_FIXTURE_ARGUMENTS+1][257];
    uint32_t argc;
    uint64_t file_size;
    uint8_t sha256[32];
};
int ae_fixture_resolve_action(const uint8_t *bytes,size_t size,const uint8_t expected_artifact[32],
    const char *action,struct ae_fixture_command *output);

#endif
