#include "core/unit_issuer.h"
#include <errno.h>
#include <string.h>

static int present(const uint8_t *b,size_t n)
{ unsigned int value=0; for (size_t k=0;k<n;++k) value|=b[k]; return value!=0; }
static size_t text_size(const char *s,size_t maximum)
{
    const char *end=memchr(s,0,maximum); if (!end || end==s) return 0;
    for (const unsigned char *p=(const unsigned char *)s;*p;++p) if (*p<32 || *p>=127) return 0;
    return (size_t)(end-s);
}
static void put(uint8_t *out,uint64_t n,size_t width)
{ for (size_t k=0;k<width;++k) out[k]=(uint8_t)(n>>(8U*(width-k-1U))); }
static uint64_t get(const uint8_t *p,size_t width)
{ uint64_t value=0; for (size_t k=0;k<width;++k) value=(value<<8U)|p[k]; return value; }
int ae_unit_issuer_encode(const struct ae_unit_issuer *v,uint8_t *out,size_t capacity,size_t *used)
{
    if (!v || !out || !used) return -EINVAL;
    const char *text[]={v->unit,v->cgroup,v->label};
    const size_t maximum[]={sizeof v->unit,sizeof v->cgroup,sizeof v->label}; size_t lengths[3],size=168;
    if (!v->pidfs_id || !v->cgroup_id || !present(v->boot,16) || !present(v->invocation,16)) return -EBADMSG;
    for (size_t k=0;k<3;++k) { lengths[k]=text_size(text[k],maximum[k]); if (!lengths[k]) return -EBADMSG; size+=4+lengths[k]; }
    if (lengths[0]<9 || strcmp(v->unit+lengths[0]-8,".service") || v->cgroup[0]!='/' || lengths[1]<2) return -EBADMSG;
    const char *part=v->cgroup+1;
    for (const char *p=part;;++p) if (*p=='/' || !*p) {
        size_t n=(size_t)(p-part);
        if (!n || (n==1 && part[0]=='.') || (n==2 && !memcmp(part,"..",2))) return -EBADMSG;
        if (!*p) break;
        part=p+1;
    }
    for (const char *p=v->unit;*p;++p) if (!((*p>='a' && *p<='z') || (*p>='A' && *p<='Z') || (*p>='0' && *p<='9') || strchr("_.:@\\-",*p))) return -EBADMSG;
    uint8_t raw[AE_UNIT_ISSUER_MAX];
    if (size>sizeof raw) return -E2BIG;
    if (capacity<size) return -ENOSPC;
    memcpy(raw,"AETHUIS1",8); memcpy(raw+8,v->boot,16); put(raw+24,v->pidfs_id,8);
    memcpy(raw+32,v->invocation,16); put(raw+48,v->cgroup_id,8);
    int r=ae_image_encode(&v->image,raw+56); if (r) return r;
    size_t offset=168;
    for (size_t k=0;k<3;++k) { put(raw+offset,lengths[k],4); offset+=4; memcpy(raw+offset,text[k],lengths[k]); offset+=lengths[k]; }
    memcpy(out,raw,size); *used=size; return 0;
}
int ae_unit_issuer_decode(const uint8_t *raw,size_t size,struct ae_unit_issuer *out)
{
    if (!raw || !out) return -EINVAL;
    if (size<192 || size>AE_UNIT_ISSUER_MAX || memcmp(raw,"AETHUIS1",8)) return -EBADMSG;
    struct ae_unit_issuer v={0}; memcpy(v.boot,raw+8,16); v.pidfs_id=get(raw+24,8);
    memcpy(v.invocation,raw+32,16); v.cgroup_id=get(raw+48,8);
    int r=ae_image_decode(raw+56,AE_IMAGE_BYTES,&v.image); if (r) return r;
    char *texts[]={v.unit,v.cgroup,v.label}; const size_t maximum[]={sizeof v.unit,sizeof v.cgroup,sizeof v.label};
    size_t offset=168;
    for (size_t k=0;k<3;++k) {
        if (size-offset<4) return -EBADMSG;
        uint64_t n=get(raw+offset,4); offset+=4;
        if (!n || n>=maximum[k] || n>size-offset || memchr(raw+offset,0,(size_t)n)) return -EBADMSG;
        memcpy(texts[k],raw+offset,(size_t)n); offset+=(size_t)n;
    }
    if (offset!=size) return -EBADMSG;
    uint8_t canonical[AE_UNIT_ISSUER_MAX]; size_t used=0;
    r=ae_unit_issuer_encode(&v,canonical,sizeof canonical,&used);
    if (!r && (used!=size || memcmp(canonical,raw,size))) r=-EBADMSG;
    if (!r) *out=v;
    return r;
}
