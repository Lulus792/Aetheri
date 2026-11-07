#ifndef AETHERI_CORE_IMAGE_H
#define AETHERI_CORE_IMAGE_H
#include <stdint.h>
#include <stddef.h>
/* In-memory object witness, never serialized native padding or bearer
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
int ae_image_equal(const struct ae_image *,const struct ae_image *);
#endif
