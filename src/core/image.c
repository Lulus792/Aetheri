#include "core/image.h"
#include <elf.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <sys/stat.h>

static int image_shape(const struct ae_image *v)
{
    return v!=NULL && v->inode>0 && v->size>=sizeof(Elf64_Ehdr) && v->size<=16U*1024U*1024U &&
        v->uid==0 && v->gid==0 && v->mode==(S_IFREG|0555) && v->birth_seconds>=0 &&
        v->birth_ns<1000000000U && v->mtime_ns<1000000000U && v->ctime_ns<1000000000U;
}
static void number_put(uint8_t *out,uint64_t value,size_t bytes)
{ for (size_t n=0;n<bytes;++n) out[n]=(uint8_t)(value>>(8*(bytes-n-1))); }
static uint64_t number_get(const uint8_t *in,size_t bytes)
{ uint64_t value=0; for (size_t n=0;n<bytes;++n) value=(value<<8)|in[n]; return value; }
static int64_t signed_get(const uint8_t *in)
{ uint64_t v=number_get(in,8); return v<=INT64_MAX ? (int64_t)v : -1-(int64_t)(UINT64_MAX-v); }
int ae_image_encode(const struct ae_image *v,uint8_t output[AE_IMAGE_BYTES])
{
    if (v==NULL || output==NULL) return -EINVAL;
    if (!image_shape(v)) return -EBADMSG;
    uint8_t b[AE_IMAGE_BYTES]; memcpy(b,"AETHIMG1",8);
    number_put(b+8,v->inode,8); number_put(b+16,v->size,8);
    const uint32_t fields[]={v->device_major,v->device_minor,v->uid,v->gid,v->mode};
    for (size_t n=0;n<5;++n) number_put(b+24+4*n,fields[n],4);
    number_put(b+44,(uint64_t)v->birth_seconds,8); number_put(b+52,v->birth_ns,4);
    number_put(b+56,(uint64_t)v->mtime_seconds,8); number_put(b+64,v->mtime_ns,4);
    number_put(b+68,(uint64_t)v->ctime_seconds,8); number_put(b+76,v->ctime_ns,4);
    memcpy(b+80,v->digest,32); memcpy(output,b,sizeof b); return 0;
}
int ae_image_decode(const uint8_t *b,size_t length,struct ae_image *output)
{
    if (b==NULL || output==NULL) return -EINVAL;
    if (length!=AE_IMAGE_BYTES || memcmp(b,"AETHIMG1",8)!=0) return -EBADMSG;
    struct ae_image v={.inode=number_get(b+8,8),.size=number_get(b+16,8),.device_major=(uint32_t)number_get(b+24,4),
        .device_minor=(uint32_t)number_get(b+28,4),.uid=(uint32_t)number_get(b+32,4),.gid=(uint32_t)number_get(b+36,4),.mode=(uint32_t)number_get(b+40,4),
        .birth_seconds=signed_get(b+44),.birth_ns=(uint32_t)number_get(b+52,4),.mtime_seconds=signed_get(b+56),
        .mtime_ns=(uint32_t)number_get(b+64,4),.ctime_seconds=signed_get(b+68),.ctime_ns=(uint32_t)number_get(b+76,4)};
    memcpy(v.digest,b+80,32); if (!image_shape(&v)) return -EBADMSG;
    *output=v; return 0;
}

int ae_image_equal(const struct ae_image *a,const struct ae_image *b)
{
    return a!=NULL && b!=NULL && a->inode==b->inode && a->size==b->size && a->device_major==b->device_major && a->device_minor==b->device_minor &&
        a->uid==b->uid && a->gid==b->gid && a->mode==b->mode && a->birth_seconds==b->birth_seconds && a->birth_ns==b->birth_ns &&
        a->mtime_seconds==b->mtime_seconds && a->mtime_ns==b->mtime_ns && a->ctime_seconds==b->ctime_seconds && a->ctime_ns==b->ctime_ns && memcmp(a->digest,b->digest,32)==0;
}
