#ifndef AETHERI_LINUX_IMAGE_H
#define AETHERI_LINUX_IMAGE_H
#include "linux/peer.h"

/* Private in-memory object witness, never serialized native padding or bearer
 * authority. Caller supplies an independently admitted immutable root-owned ELF
 * and expected manifest SHA-256. No publisher enrollment or runtime admission. */
struct ae_image {
    uint64_t inode,size;
    uint32_t device_major,device_minor,uid,gid,mode;
    int64_t birth_seconds,mtime_seconds,ctime_seconds;
    uint32_t birth_ns,mtime_ns,ctime_ns;
    uint8_t digest[32];
};
enum { AE_IMAGE_BYTES=112 };
/* Canonical AETHIMG1 object observation, not native padding or bearer trust.
 * Big endian; birth seconds nonnegative, mtime/ctime signed two's complement.
 * Independent authenticated source and later held-process matching still owed.
 * Outputs unchanged on error. */
int ae_image_encode(const struct ae_image *,uint8_t output[AE_IMAGE_BYTES]);
int ae_image_decode(const uint8_t *,size_t,struct ae_image *);
int ae_image_verify(int fd,const uint8_t expected_digest[32],struct ae_image *output);
int ae_image_equal(const struct ae_image *,const struct ae_image *);
/* Current executable observation of an already authenticated/owned held
 * ProcessFD. No pidfd_open from a supplied PID. Trusted host proc/PID namespace,
 * immutable backing files and separate unit/cgroup/run admission are required.
 * Same-UID/MAC protections must exclude hostile ptrace/credential/exec changes.
 * A bounded observation is not a guarantee of future liveness or readiness.
 * Mount IDs may differ across the admitted private code view; backing object,
 * birth/metadata/content must agree. Outputs unchanged on all errors. */
int ae_image_match_process(const struct ae_peer *,gid_t expected_gid,
    const struct ae_image *expected,struct ae_image *output);
#endif
