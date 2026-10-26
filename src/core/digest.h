#ifndef AETHERI_CORE_DIGEST_H
#define AETHERI_CORE_DIGEST_H

#include <stddef.h>
#include <stdint.h>

enum { AE_SHA256_BYTES = 32, AE_DIGEST_MAX_BYTES = 16 * 1024 * 1024 };

/* Fixed SHA-256 through the reviewed OpenSSL 3.5 default provider in a private,
 * call-local context. No caller-selected algorithms, providers or configuration.
 * Bytes are borrowed and must remain immutable during the synchronous call.
 * NULL bytes is valid only for size zero. Output must not overlap input.
 * Output stays unchanged on error. -EINVAL: arguments; -E2BIG: bound;
 * -EIO: crypto/provider failure; -ENOMEM: context allocation failure.
 * No global OpenSSL cleanup or default-context changes are performed.
 */
int ae_sha256_bytes(const void *bytes, size_t size, uint8_t output[AE_SHA256_BYTES]);

/* Hash exactly expected_size bytes from offset zero of a borrowed regular FD,
 * preserving its offset and leaving it open. Size is limited as above. -EBADMSG:
 * length/short read mismatch; -ESTALE: metadata changed during observation;
 * -EINVAL: nonregular FD; otherwise negative errno/crypto error. Output unchanged
 * on failure. Caller owns confinement, ownership and mutation exclusion. These
 * observations do not freeze a file or authorize later reopening/execution.
 */
int ae_sha256_fd(int fd, uint64_t expected_size, uint8_t output[AE_SHA256_BYTES]);

#endif
