#ifndef AETHERI_LINUX_UNIT_BYTES_H
#define AETHERI_LINUX_UNIT_BYTES_H
/* Private canonical-byte helpers shared only by unit selection encoders. */
#include "core/root.h"
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
struct ae_unit_bytes { uint8_t *data; size_t used,capacity; int error; };
static inline void ae_ub_bytes(struct ae_unit_bytes *b,const void *value,size_t size)
{
    if (b->error) return;
    if (size>b->capacity-b->used) { b->error=-E2BIG; return; }
    memcpy(b->data+b->used,value,size); b->used+=size;
}
static inline void ae_ub_number(struct ae_unit_bytes *b,uint64_t value,size_t width)
{
    uint8_t raw[8];
    for (size_t i=0;i<width;++i) raw[i]=(uint8_t)(value>>(8*(width-1-i)));
    ae_ub_bytes(b,raw,width);
}
static inline void ae_ub_text(struct ae_unit_bytes *b,const char *value)
{ size_t size=strlen(value); ae_ub_number(b,size,4); ae_ub_bytes(b,value,size); }
static inline void ae_ub_root(struct ae_unit_bytes *b,const struct ae_root_identity *r)
{
    if (!r->mount_id || !r->inode || r->uid || r->gid || !S_ISDIR(r->mode) || (r->mode&0022) ||
        r->filesystem_type<=0 || r->birth_present!=1 || r->birth_seconds<0 || r->birth_nanoseconds>=1000000000U) { b->error=-EINVAL; return; }
    ae_ub_number(b,r->mount_id,8); ae_ub_number(b,r->inode,8);
    ae_ub_number(b,r->device_major,4); ae_ub_number(b,r->device_minor,4);
    ae_ub_number(b,r->uid,4); ae_ub_number(b,r->gid,4); ae_ub_number(b,r->mode,4);
    ae_ub_number(b,(uint64_t)r->filesystem_type,8); ae_ub_bytes(b,r->filesystem_id,8);
    ae_ub_number(b,(uint64_t)r->birth_seconds,8); ae_ub_number(b,r->birth_nanoseconds,4);
}
#endif
