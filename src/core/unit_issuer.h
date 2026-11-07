#ifndef AETHERI_CORE_UNIT_ISSUER_H
#define AETHERI_CORE_UNIT_ISSUER_H
#include "core/image.h"

enum { AE_UNIT_ISSUER_MAX=2048 };
/* AETHUIS1: Root UID/GID0 are implicit in this profile. Observations must come
 * from an independently admitted current main process, original bus ProcessFD,
 * pidfs identity and protected image. Metadata is never such admission itself.
 * No numeric PID, descriptor number, native padding or bearer permission. */
struct ae_unit_issuer {
    uint8_t boot[16],invocation[16];
    uint64_t pidfs_id,cgroup_id;
    char unit[256],cgroup[1024],label[512];
    struct ae_image image;
};
int ae_unit_issuer_encode(const struct ae_unit_issuer *,uint8_t *,size_t,size_t *);
int ae_unit_issuer_decode(const uint8_t *,size_t,struct ae_unit_issuer *);
#endif
