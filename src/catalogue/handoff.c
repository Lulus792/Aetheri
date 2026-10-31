#include "catalogue/handoff.h"
#include <errno.h>
#include <linux/magic.h>
#include <string.h>
#include <sys/stat.h>

static void put(uint8_t *p,uint64_t v,size_t size)
{ for (size_t n=0;n<size;++n) p[n]=(uint8_t)(v>>(8U*(size-1U-n))); }
static uint64_t get(const uint8_t *p,size_t size)
{ uint64_t v=0; for (size_t n=0;n<size;++n) v=(v<<8)|p[n]; return v; }
static int64_t signed_get(const uint8_t *p)
{ uint64_t v=get(p,8); return v<=INT64_MAX ? (int64_t)v : -1-(int64_t)(UINT64_MAX-v); }
static int root_valid(const struct ae_root_identity *v)
{
    return v->mount_id && v->inode && v->mode<=UINT16_MAX && S_ISDIR(v->mode) && v->filesystem_type==CGROUP2_SUPER_MAGIC &&
        (v->birth_present==0 || v->birth_present==1) && v->birth_nanoseconds<1000000000U &&
        (v->birth_present || (!v->birth_seconds && !v->birth_nanoseconds));
}
static void root_put(uint8_t *p,const struct ae_root_identity *v)
{
    put(p,v->mount_id,8); put(p+8,v->inode,8);
    const uint32_t fields[]={v->device_major,v->device_minor,v->uid,v->gid,v->mode};
    for (size_t n=0;n<5;++n) put(p+16+4*n,fields[n],4);
    put(p+36,(uint64_t)v->filesystem_type,8); memcpy(p+44,v->filesystem_id,8);
    p[52]=(uint8_t)v->birth_present; put(p+56,(uint64_t)v->birth_seconds,8); put(p+64,v->birth_nanoseconds,4);
}
static int root_get(const uint8_t *p,struct ae_root_identity *v)
{
    if (p[53] || p[54] || p[55] || p[68] || p[69] || p[70] || p[71] || get(p+36,8)>INT64_MAX) return -EBADMSG;
    *v=(struct ae_root_identity){.mount_id=get(p,8),.inode=get(p+8,8),.device_major=(uint32_t)get(p+16,4),
        .device_minor=(uint32_t)get(p+20,4),.uid=(uint32_t)get(p+24,4),.gid=(uint32_t)get(p+28,4),.mode=(uint32_t)get(p+32,4),
        .filesystem_type=(int64_t)get(p+36,8),.birth_present=p[52],.birth_seconds=signed_get(p+56),.birth_nanoseconds=(uint32_t)get(p+64,4)};
    memcpy(v->filesystem_id,p+44,8); return root_valid(v) ? 0 : -EBADMSG;
}
static int valid(const struct ae_execution_handoff *v,size_t *unit)
{
    size_t length=strnlen(v->custodian,sizeof v->custodian); unsigned int invocation=0;
    for (size_t n=0;n<16;++n) invocation|=v->custodian_invocation[n];
    if (length<=8 || length>=sizeof v->custodian || strcmp(v->custodian+length-8,".service")!=0 || !invocation) return -EBADMSG;
    for (size_t n=0;n<length;++n) if ((unsigned char)v->custodian[n]<0x21 || (unsigned char)v->custodian[n]>0x7e || v->custodian[n]=='/') return -EBADMSG;
    if (v->process.role!=AE_EXECUTION_APP || !root_valid(&v->root) || !root_valid(&v->group) ||
        v->root.mount_id!=v->group.mount_id || v->root.inode==v->group.inode || v->group.inode!=v->process.cgroup_id ||
        v->root.device_major!=v->group.device_major || v->root.device_minor!=v->group.device_minor ||
        memcmp(v->root.filesystem_id,v->group.filesystem_id,8)!=0 || !v->pid_namespace || !v->cgroup_namespace ||
        !v->events_inode || v->events_mount!=v->root.mount_id || v->events_mode>UINT16_MAX || !S_ISREG(v->events_mode)) return -EBADMSG;
    *unit=length; return 0;
}
int ae_handoff_encode(const struct ae_execution_handoff *v,uint8_t *output,size_t capacity,size_t *written)
{
    if (!v || !output || !written) return -EINVAL;
    size_t unit=0,length=0; int r=valid(v,&unit);
    uint8_t bytes[AE_HANDOFF_MAX]={0};
    if (r==0) r=ae_execution_process_encode(&v->process,bytes+AE_HANDOFF_FIXED,sizeof bytes-AE_HANDOFF_FIXED,&length);
    if (r!=0) return r;
    _Static_assert(AE_HANDOFF_FIXED+AE_EXECUTION_PROCESS_MAX+255<=AE_HANDOFF_MAX,"bounded handoff frame");
    size_t size=AE_HANDOFF_FIXED+length+unit; if (capacity<size) return -ENOBUFS;
    memcpy(bytes,"AETHHND1",8); memcpy(bytes+8,v->custodian_invocation,16); put(bytes+24,unit,2); put(bytes+26,length,2);
    put(bytes+28,v->pid_namespace,8); put(bytes+36,v->cgroup_namespace,8);
    root_put(bytes+44,&v->root); root_put(bytes+116,&v->group);
    put(bytes+188,v->events_inode,8); put(bytes+196,v->events_mount,8); put(bytes+204,v->events_mode,4);
    put(bytes+208,v->events_uid,4); put(bytes+212,v->events_gid,4);
    memcpy(bytes+AE_HANDOFF_FIXED+length,v->custodian,unit); memcpy(output,bytes,size); *written=size; return 0;
}
int ae_handoff_decode(const uint8_t *bytes,size_t size,struct ae_execution_handoff *output)
{
    if (!bytes || !output) return -EINVAL;
    if (size<374 || size>AE_HANDOFF_MAX || memcmp(bytes,"AETHHND1",8)!=0) return -EBADMSG;
    size_t unit=(size_t)get(bytes+24,2),length=(size_t)get(bytes+26,2);
    if (!unit || unit>=256 || length<149 || length>AE_EXECUTION_PROCESS_MAX || AE_HANDOFF_FIXED+length+unit!=size ||
        memchr(bytes+AE_HANDOFF_FIXED+length,0,unit)) return -EBADMSG;
    struct ae_execution_handoff v={0}; memcpy(v.custodian_invocation,bytes+8,16);
    v.pid_namespace=get(bytes+28,8); v.cgroup_namespace=get(bytes+36,8);
    int r=root_get(bytes+44,&v.root); if (r==0) r=root_get(bytes+116,&v.group);
    if (r==0) r=ae_execution_process_decode(bytes+AE_HANDOFF_FIXED,length,&v.process);
    v.events_inode=get(bytes+188,8); v.events_mount=get(bytes+196,8); v.events_mode=(uint32_t)get(bytes+204,4);
    v.events_uid=(uint32_t)get(bytes+208,4); v.events_gid=(uint32_t)get(bytes+212,4);
    memcpy(v.custodian,bytes+AE_HANDOFF_FIXED+length,unit);
    size_t check=0; if (r==0) r=valid(&v,&check); if (r==0) *output=v; return r;
}
int ae_handoff_match(const struct ae_execution_profile *p,const struct ae_execution_handoff *v)
{
    if (!v) return -EINVAL;
    uint8_t bytes[AE_HANDOFF_MAX]; size_t length=0; int r=ae_handoff_encode(v,bytes,sizeof bytes,&length);
    struct ae_execution_evidence e={0}; memcpy(e.invocation,v->process.invocation,16); memcpy(e.context_digest,v->process.evidence_digest,32);
    if (r==0) r=ae_execution_process_match(p,&e,&v->process);
    return r;
}
