#include "core/unit_custody.h"
#include <errno.h>
#include <string.h>

static int unique(const char *s,size_t size)
{
    const char *end=memchr(s,0,size); if (!end || end-s<4 || *s++!=':') return 0;
    unsigned int groups=0;
    while (s<end) {
        const char *start=s; while (s<end && *s>='0' && *s<='9') ++s;
        if (s==start) return 0;
        ++groups; if (s<end && *s++!='.') return 0;
        if (s==end && end[-1]=='.') return 0;
    }
    return groups>=2;
}
static int valid(const struct ae_unit_origin *p)
{
    if (!p || !unique(p->issuer,sizeof p->issuer) || !unique(p->manager,sizeof p->manager) ||
        !strcmp(p->issuer,p->manager) || !memchr(p->manager_label,0,sizeof p->manager_label) || !p->manager_label[0]) return 0;
    unsigned int boot=0,bus=0;
    for (unsigned int n=0;n<16;++n) { boot|=p->boot[n]; bus|=p->bus[n]; }
    for (const unsigned char *s=(const unsigned char *)p->manager_label;*s;++s) if (*s<32 || *s==127) return 0;
    return boot && bus;
}
int ae_unit_origin_encode(const struct ae_unit_origin *p,uint8_t out[AE_UNIT_ORIGIN_BYTES])
{
    if (!out || !valid(p)) return -EINVAL;
    uint8_t raw[AE_UNIT_ORIGIN_BYTES]={0};
    memcpy(raw,"AETHUBS1",8); memcpy(raw+8,p->boot,16); memcpy(raw+24,p->bus,16);
    memcpy(raw+40,p->issuer,strlen(p->issuer)); memcpy(raw+296,p->manager,strlen(p->manager));
    memcpy(raw+552,p->manager_label,strlen(p->manager_label)); memcpy(out,raw,sizeof raw); return 0;
}
int ae_unit_origin_decode(const uint8_t *raw,size_t size,struct ae_unit_origin *out)
{
    if (!raw || !out || size!=AE_UNIT_ORIGIN_BYTES || memcmp(raw,"AETHUBS1",8)) return -EINVAL;
    struct ae_unit_origin p; uint8_t canonical[AE_UNIT_ORIGIN_BYTES];
    memcpy(p.boot,raw+8,16); memcpy(p.bus,raw+24,16); memcpy(p.issuer,raw+40,256);
    memcpy(p.manager,raw+296,256); memcpy(p.manager_label,raw+552,512);
    if (ae_unit_origin_encode(&p,canonical) || memcmp(canonical,raw,size)) return -EINVAL;
    *out=p; return 0;
}
