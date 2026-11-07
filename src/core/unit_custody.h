#ifndef AETHERI_CORE_UNIT_CUSTODY_H
#define AETHERI_CORE_UNIT_CUSTODY_H
#include <stddef.h>
#include <stdint.h>

enum ae_unit_effect { AE_UNIT_SUBSCRIBE=1, AE_UNIT_REF, AE_UNIT_START, AE_UNIT_UNREF, AE_UNIT_UNSUBSCRIBE };
enum { AE_UNIT_ORIGIN_BYTES=1064, AE_UNIT_EFFECTS_MAX=19 };
/* Original trusted-bus connection observations. Not process identity, current
 * supervisor/image admission, an authenticated volume or permission to recover
 * a vanished connection. Retain independently admitted original process evidence
 * before any future owner-replacement reconciliation. */
struct ae_unit_origin {
    uint8_t boot[16],bus[16];
    char issuer[256],manager[256],manager_label[512];
};
int ae_unit_origin_encode(const struct ae_unit_origin *,uint8_t [AE_UNIT_ORIGIN_BYTES]);
int ae_unit_origin_decode(const uint8_t *,size_t,struct ae_unit_origin *);
#endif
