#include "catalogue/interruption.h"
#include "core/digest.h"
#include <errno.h>
#include <limits.h>
#include <string.h>
static int uuid(const uint8_t *p) { return (p[6]>>4)==4 && (p[8]&0xc0)==0x80; }
static int present(const uint8_t *p,size_t size) { unsigned int v=0; for (size_t n=0;n<size;++n) v|=p[n]; return v!=0; }
static void put(uint8_t *p,uint64_t v) { for (size_t n=0;n<8;++n) p[n]=(uint8_t)(v>>(8U*(7U-n))); }
static uint64_t get(const uint8_t *p) { uint64_t v=0; for (size_t n=0;n<8;++n) v=(v<<8)|p[n]; return v; }
int ae_interruption_binding_encode(const struct ae_execution_profile *p,const struct ae_interruption_binding *v,uint8_t out[AE_INTERRUPTION_BINDING_BYTES])
{
    if (!v || !out) return -EINVAL;
    uint8_t raw[AE_INTERRUPTION_BINDING_BYTES]; memcpy(raw,"AETHINT1",8);
    int r=ae_execution_digest(p,raw+8); if (r!=0) return r;
    const uint8_t *ids[]={v->boot,v->root_runtime,v->user_runtime};
    for (size_t n=0;n<3;++n) { if (!uuid(ids[n])) return -EBADMSG; memcpy(raw+40+16*n,ids[n],16); }
    if (memcmp(v->boot,p->boot,16)==0 || memcmp(v->root_runtime,p->system_runtime,16)==0 ||
        memcmp(v->user_runtime,p->broker_runtime,16)==0 || memcmp(v->root_runtime,v->user_runtime,16)==0) return -ESTALE;
    const uint8_t *digests[]={v->continuity_digest,v->fence_digest,v->boot_context_digest,v->root_context_digest,v->user_context_digest};
    for (size_t n=0;n<5;++n) { if (!present(digests[n],32)) return -EBADMSG; memcpy(raw+88+32*n,digests[n],32); }
    memcpy(out,raw,sizeof raw); return 0;
}
int ae_interruption_binding_decode(const struct ae_execution_profile *p,const uint8_t *raw,size_t size,struct ae_interruption_binding *out)
{
    if (!raw || !out) return -EINVAL;
    if (size!=AE_INTERRUPTION_BINDING_BYTES || memcmp(raw,"AETHINT1",8)!=0) return -EBADMSG;
    struct ae_interruption_binding v; memcpy(v.boot,raw+40,16); memcpy(v.root_runtime,raw+56,16); memcpy(v.user_runtime,raw+72,16);
    uint8_t *digests[]={v.continuity_digest,v.fence_digest,v.boot_context_digest,v.root_context_digest,v.user_context_digest};
    for (size_t n=0;n<5;++n) memcpy(digests[n],raw+88+32*n,32);
    uint8_t check[AE_INTERRUPTION_BINDING_BYTES]; int r=ae_interruption_binding_encode(p,&v,check);
    if (r==0 && memcmp(check,raw,sizeof check)!=0) r=-ESTALE;
    if (r==0) *out=v;
    return r;
}
static const char *magic(enum ae_interruption_kind k)
{ return k==AE_INTERRUPTION_BEGIN ? "AETHIBG1" : k==AE_INTERRUPTION_QUIET ? "AETHIQU1" : k==AE_INTERRUPTION_RELEASE ? "AETHIRL1" : k==AE_INTERRUPTION_EXIT ? "AETHIEX1" : NULL; }
int ae_interruption_receipt_encode(const struct ae_execution_profile *p,const struct ae_interruption_receipt *v,uint8_t out[AE_INTERRUPTION_RECEIPT_BYTES])
{
    if (!v || !out) return -EINVAL;
    const char *tag=magic(v->kind);
    if (!tag || !present(v->invocation,16) || !present(v->previous_digest,32) || !present(v->context_digest,32) || v->revision<1 || v->unix_ms<0) return -EBADMSG;
    uint8_t raw[AE_INTERRUPTION_RECEIPT_BYTES]; memcpy(raw,tag,8);
    int r=ae_interruption_binding_encode(p,&v->binding,raw+8); if (r!=0) return r;
    memcpy(raw+256,v->invocation,16); memcpy(raw+272,v->previous_digest,32); memcpy(raw+304,v->context_digest,32);
    put(raw+336,(uint64_t)v->revision); put(raw+344,(uint64_t)v->unix_ms); memcpy(out,raw,sizeof raw); return 0;
}
int ae_interruption_receipt_decode(const struct ae_execution_profile *p,const uint8_t *raw,size_t size,struct ae_interruption_receipt *out)
{
    if (!raw || !out) return -EINVAL;
    if (size!=AE_INTERRUPTION_RECEIPT_BYTES) return -EBADMSG;
    struct ae_interruption_receipt v={0};
    for (int n=1;n<=4;++n) if (memcmp(raw,magic((enum ae_interruption_kind)n),8)==0) v.kind=(enum ae_interruption_kind)n;
    int r=ae_interruption_binding_decode(p,raw+8,248,&v.binding); if (r!=0) return r;
    memcpy(v.invocation,raw+256,16); memcpy(v.previous_digest,raw+272,32); memcpy(v.context_digest,raw+304,32);
    uint64_t rev=get(raw+336),stamp=get(raw+344); if (rev>INT64_MAX || stamp>INT64_MAX) return -EBADMSG;
    v.revision=(int64_t)rev; v.unix_ms=(int64_t)stamp; uint8_t check[AE_INTERRUPTION_RECEIPT_BYTES];
    r=ae_interruption_receipt_encode(p,&v,check); if (r==0) *out=v; return r;
}
int ae_interruption_receipt_digest(const struct ae_execution_profile *p,const struct ae_interruption_receipt *v,uint8_t out[32])
{ if (!out) return -EINVAL; uint8_t raw[AE_INTERRUPTION_RECEIPT_BYTES]; int r=ae_interruption_receipt_encode(p,v,raw); return r==0 ? ae_sha256_bytes(raw,sizeof raw,out) : r; }
