#include "catalogue/lifetime.h"
#include "core/digest.h"
#include <errno.h>
#include <limits.h>
#include <string.h>

static const char *magic(enum ae_run_receipt_kind kind)
{ return kind==AE_RUN_ROOT_CLOSING ? "AETHCLS1" : kind==AE_RUN_USER_QUIET ? "AETHQSC1" : kind==AE_RUN_ROOT_RELEASED ? "AETHEND1" : NULL; }
static void put(uint8_t *out,uint64_t v,size_t length)
{ for (size_t n=0;n<length;++n) out[n]=(uint8_t)(v>>(8U*(length-1U-n))); }
static uint64_t get(const uint8_t *in,size_t length)
{ uint64_t v=0; for (size_t n=0;n<length;++n) v=(v<<8)|in[n]; return v; }
int ae_run_receipt_encode(const struct ae_run_receipt *v,uint8_t output[AE_RUN_RECEIPT_BYTES])
{
    if (!v || !output) return -EINVAL;
    const char *tag=magic(v->kind); unsigned int nonzero=0;
    for (size_t n=0;n<16;++n) nonzero|=v->invocation[n];
    if (!tag || !nonzero || v->revision<1 || v->unix_ms<0) return -EBADMSG;
    uint8_t bytes[AE_RUN_RECEIPT_BYTES]; memcpy(bytes,tag,8); memcpy(bytes+8,v->profile_digest,32);
    memcpy(bytes+40,v->invocation,16); memcpy(bytes+56,v->previous_digest,32); memcpy(bytes+88,v->evidence_digest,32);
    put(bytes+120,(uint64_t)v->revision,8); put(bytes+128,(uint64_t)v->unix_ms,8); memcpy(output,bytes,sizeof bytes); return 0;
}
int ae_run_receipt_decode(const uint8_t *bytes,size_t size,struct ae_run_receipt *output)
{
    if (!bytes || !output) return -EINVAL;
    if (size!=AE_RUN_RECEIPT_BYTES) return -EBADMSG;
    struct ae_run_receipt v={0};
    for (int kind=1;kind<=3;++kind) if (memcmp(bytes,magic((enum ae_run_receipt_kind)kind),8)==0) v.kind=(enum ae_run_receipt_kind)kind;
    memcpy(v.profile_digest,bytes+8,32); memcpy(v.invocation,bytes+40,16); memcpy(v.previous_digest,bytes+56,32); memcpy(v.evidence_digest,bytes+88,32);
    uint64_t revision=get(bytes+120,8),stamp=get(bytes+128,8);
    if (revision>INT64_MAX || stamp>INT64_MAX) return -EBADMSG;
    v.revision=(int64_t)revision; v.unix_ms=(int64_t)stamp;
    uint8_t check[AE_RUN_RECEIPT_BYTES]; int r=ae_run_receipt_encode(&v,check);
    if (r==0) *output=v;
    return r;
}
int ae_run_receipt_digest(const struct ae_run_receipt *v,uint8_t output[32])
{
    if (!output) return -EINVAL;
    uint8_t bytes[AE_RUN_RECEIPT_BYTES]; int r=ae_run_receipt_encode(v,bytes);
    return r==0 ? ae_sha256_bytes(bytes,sizeof bytes,output) : r;
}
int ae_run_receipt_match(const struct ae_execution_profile *p,const struct ae_execution_evidence *e,enum ae_run_receipt_kind kind,const struct ae_run_receipt *v)
{
    if (!e || !v || !magic(kind)) return -EINVAL;
    uint8_t hash[32],bytes[AE_RUN_RECEIPT_BYTES]; int r=ae_run_receipt_encode(v,bytes);
    if (r==0) r=ae_execution_digest(p,hash);
    if (r==0 && (v->kind!=kind || memcmp(v->profile_digest,hash,32)!=0 || memcmp(v->invocation,e->invocation,16)!=0)) r=-ESTALE;
    return r;
}
int ae_run_end_digest(const struct ae_execution_profile *p,const struct ae_execution_evidence *e,const struct ae_run_end *end,uint8_t output[32])
{
    if (!e || !end || !output) return -EINVAL;
    uint8_t bytes[120]={0}; memcpy(bytes,"AETHREN1",8);
    int r=ae_execution_digest(p,bytes+8); if (r!=0) return r;
    unsigned int nonzero=0; for (size_t n=0;n<16;++n) nonzero|=e->invocation[n];
    if (!nonzero || !end->cgroup_id || !end->pid || end->pid>INT32_MAX || (end->group_removed!=0 && end->group_removed!=1) || (end->unit_failed!=0 && end->unit_failed!=1)) return -EBADMSG;
    if (memcmp(p->boot,end->boot,16)!=0 || memcmp(e->invocation,end->invocation,16)!=0) return -ESTALE;
    memcpy(bytes+40,e->context_digest,32); memcpy(bytes+72,end->boot,16); memcpy(bytes+88,end->invocation,16);
    put(bytes+104,end->cgroup_id,8); put(bytes+112,end->pid,4); bytes[116]=(uint8_t)end->group_removed; bytes[117]=(uint8_t)end->unit_failed;
    return ae_sha256_bytes(bytes,sizeof bytes,output);
}
