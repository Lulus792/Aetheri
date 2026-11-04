#include "linux/boot.h"
#include "core/root.h"
#include <errno.h>
#include <fcntl.h>
#include <linux/magic.h>
#include <linux/openat2.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/syscall.h>
#include <systemd/sd-id128.h>
#include <unistd.h>

struct ae_boot {
    pid_t owner;
    int proc,boot,namespaces[3],stale;
    struct ae_root_identity root;
    struct statx file;
    struct ae_boot_context context;
};
static int uuid(const uint8_t *id)
{ return id && (id[6]>>4)==4 && (id[8]&0xc0)==0x80; }
static void put(uint8_t *out,uint64_t value)
{ for (size_t n=0;n<8;++n) out[n]=(uint8_t)(value>>(8U*(7U-n))); }
static uint64_t get(const uint8_t *in)
{ uint64_t value=0; for (size_t n=0;n<8;++n) value=(value<<8)|in[n]; return value; }
int ae_boot_context_encode(const struct ae_boot_context *v,uint8_t output[AE_BOOT_CONTEXT_BYTES])
{
    if (!v || !output) return -EINVAL;
    if (!uuid(v->boot) || !v->pid_namespace || !v->cgroup_namespace || !v->time_namespace) return -EBADMSG;
    uint8_t raw[AE_BOOT_CONTEXT_BYTES]; memcpy(raw,"AETHBOT1",8); memcpy(raw+8,v->boot,16);
    put(raw+24,v->pid_namespace); put(raw+32,v->cgroup_namespace); put(raw+40,v->time_namespace);
    memcpy(output,raw,sizeof raw); return 0;
}
int ae_boot_context_decode(const uint8_t *raw,size_t size,struct ae_boot_context *output)
{
    if (!raw || !output) return -EINVAL;
    if (size!=AE_BOOT_CONTEXT_BYTES || memcmp(raw,"AETHBOT1",8)!=0) return -EBADMSG;
    struct ae_boot_context v={.pid_namespace=get(raw+24),.cgroup_namespace=get(raw+32),.time_namespace=get(raw+40)};
    memcpy(v.boot,raw+8,16); uint8_t check[AE_BOOT_CONTEXT_BYTES]; int r=ae_boot_context_encode(&v,check);
    if (r==0) *output=v;
    return r;
}
static int owner(struct ae_boot *v)
{ return !v ? -EINVAL : v->owner!=getpid() ? -ECHILD : v->stale ? -ESTALE : 0; }
static int boot_open(int proc)
{
    struct open_how how={.flags=O_RDONLY|O_CLOEXEC|O_NOFOLLOW,
        .resolve=RESOLVE_BENEATH|RESOLVE_NO_SYMLINKS|RESOLVE_NO_MAGICLINKS};
    int fd; do { fd=(int)syscall(SYS_openat2,proc,"sys/kernel/random/boot_id",&how,sizeof how); } while (fd<0 && errno==EINTR);
    return fd<0 ? -errno : fd;
}
static int file_identity(int fd,struct statx *out)
{
    const unsigned int required=STATX_TYPE|STATX_MODE|STATX_UID|STATX_GID|STATX_INO|STATX_MNT_ID_UNIQUE;
    struct statx st; int r;
    do { r=statx(fd,"",AT_EMPTY_PATH|AT_SYMLINK_NOFOLLOW,required,&st); } while (r<0 && errno==EINTR);
    if (r<0) return -errno;
    if ((st.stx_mask&required)!=required || !st.stx_mnt_id || !st.stx_ino) return -ENOTSUP;
    struct statfs fs; if (fstatfs(fd,&fs)<0) return -errno;
    if (fs.f_type!=PROC_SUPER_MAGIC || st.stx_mode!=(S_IFREG|0444) || st.stx_uid || st.stx_gid) return -ESTALE;
    *out=st; return 0;
}
static int file_equal(const struct statx *a,const struct statx *b)
{
    return a->stx_mnt_id==b->stx_mnt_id && a->stx_ino==b->stx_ino && a->stx_dev_major==b->stx_dev_major &&
        a->stx_dev_minor==b->stx_dev_minor && a->stx_uid==b->stx_uid && a->stx_gid==b->stx_gid && a->stx_mode==b->stx_mode;
}
static int hex(unsigned char c)
{ return c>='0' && c<='9' ? c-'0' : c>='a' && c<='f' ? c-'a'+10 : -1; }
static int raw_boot(int fd,uint8_t out[16])
{
    if (lseek(fd,0,SEEK_SET)<0) return -errno;
    unsigned char text[38]; size_t used=0;
    while (used<sizeof text) {
        ssize_t got; do { got=read(fd,text+used,sizeof text-used); } while (got<0 && errno==EINTR);
        if (got<0) return -errno;
        if (!got) break;
        used+=(size_t)got;
    }
    if (used!=37 || text[36]!='\n') return -EBADMSG;
    uint8_t value[16]; size_t digit=0;
    for (size_t n=0;n<36;++n) {
        if (n==8 || n==13 || n==18 || n==23) { if (text[n]!='-') return -EBADMSG; continue; }
        int h=hex(text[n]); if (h<0) return -EBADMSG;
        if (!(digit&1U)) value[digit/2]=(uint8_t)((unsigned int)h<<4); else value[digit/2]|=(uint8_t)h;
        ++digit;
    }
    if (!uuid(value)) return -EBADMSG;
    memcpy(out,value,16); return 0;
}
static int namespace_identity(int fd,uint64_t *out)
{
    struct stat st; struct statfs fs;
    if (fstat(fd,&st)<0 || fstatfs(fd,&fs)<0) return -errno;
    if (fs.f_type!=NSFS_MAGIC || !st.st_ino) return -ESTALE;
    *out=(uint64_t)st.st_ino; return 0;
}
static const char *const names[]={"self/ns/pid","self/ns/cgroup","self/ns/time","self/ns/pid_for_children","self/ns/time_for_children"};
static int namespace_open(int proc,size_t n)
{
    /* These exact self namespace magic links are intentional; no caller paths. */
    int fd; do { fd=openat(proc,names[n],O_RDONLY|O_CLOEXEC); } while (fd<0 && errno==EINTR);
    return fd<0 ? -errno : fd;
}
static int namespace_check(struct ae_boot *v)
{
    const uint64_t ids[]={v->context.pid_namespace,v->context.cgroup_namespace,v->context.time_namespace};
    for (size_t n=0;n<5;++n) {
        size_t kind=n==3 ? 0 : n==4 ? 2 : n;
        uint64_t held=0,named=0; int r=namespace_identity(v->namespaces[kind],&held);
        int fd=r==0 ? namespace_open(v->proc,n) : -1;
        if (r==0) r=fd<0 ? fd : namespace_identity(fd,&named);
        if (fd>=0) close(fd);
        if (r==0 && (held!=ids[kind] || named!=held)) r=-ESTALE;
        /* After CLONE_NEWPID, pid_for_children is empty until its first child.
         * Losing that required named binding is drift, not a retryable absence. */
        if (r==-ENOENT) r=-ESTALE;
        if (r!=0) return r;
    }
    return 0;
}
int ae_boot_observe(struct ae_boot *v,struct ae_boot_context *output)
{
    if (!output) return -EINVAL;
    int r=owner(v); if (r!=0) return r;
    r=ae_root_check_path(v->proc,"/proc",&v->root);
    if (r==0) r=namespace_check(v);
    struct statx held,named; uint8_t a[16],b[16]; sd_id128_t cached;
    if (r==0) r=file_identity(v->boot,&held);
    int fd=r==0 ? boot_open(v->proc) : -1;
    if (r==0) r=fd<0 ? fd : file_identity(fd,&named);
    if (r==0 && (!file_equal(&held,&v->file) || !file_equal(&named,&v->file))) r=-ESTALE;
    if (r==0) r=raw_boot(v->boot,a);
    if (r==0) r=raw_boot(fd,b);
    if (r==0) r=sd_id128_get_boot(&cached);
    if (r==0 && (memcmp(a,b,16)!=0 || memcmp(a,cached.bytes,16)!=0 || memcmp(a,v->context.boot,16)!=0)) r=-ESTALE;
    if (fd>=0) close(fd);
    if (r==0) r=namespace_check(v);
    if (r==0) r=ae_root_check_path(v->proc,"/proc",&v->root);
    if (r==-ESTALE) v->stale=1;
    if (r==0) *output=v->context;
    return r;
}
int ae_boot_capture(struct ae_boot **output)
{
    if (!output || *output) return -EINVAL;
    struct ae_boot *v=calloc(1,sizeof *v); if (!v) return -ENOMEM;
    v->owner=getpid(); v->proc=v->boot=-1; for (size_t n=0;n<3;++n) v->namespaces[n]=-1;
    v->proc=open("/proc",O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW); int r=v->proc<0 ? -errno : ae_root_observe(v->proc,&v->root);
    if (r==0 && (v->root.filesystem_type!=PROC_SUPER_MAGIC || v->root.uid || v->root.gid)) r=-ESTALE;
    uint64_t *ids[]={&v->context.pid_namespace,&v->context.cgroup_namespace,&v->context.time_namespace};
    for (size_t n=0;r==0 && n<3;++n) {
        v->namespaces[n]=namespace_open(v->proc,n);
        r=v->namespaces[n]<0 ? v->namespaces[n] : namespace_identity(v->namespaces[n],ids[n]);
    }
    if (r==0) { v->boot=boot_open(v->proc); r=v->boot<0 ? v->boot : file_identity(v->boot,&v->file); }
    if (r==0) r=raw_boot(v->boot,v->context.boot);
    struct ae_boot_context checked; if (r==0) r=ae_boot_observe(v,&checked);
    if (r==0) *output=v; else (void)ae_boot_close(&v);
    return r;
}
int ae_boot_observe_other(struct ae_boot *v,const uint8_t original[16],struct ae_boot_context *output)
{
    if (!original || !output) return -EINVAL;
    if (!uuid(original)) return -EBADMSG;
    struct ae_boot_context result; int r=ae_boot_observe(v,&result);
    if (r==0 && memcmp(original,result.boot,16)==0) r=-EALREADY;
    if (r==0) *output=result;
    return r;
}
int ae_boot_close(struct ae_boot **output)
{
    if (!output) return -EINVAL;
    struct ae_boot *v=*output; if (!v) return 0;
    if (v->owner!=getpid()) return -ECHILD;
    if (v->proc>=0) close(v->proc);
    if (v->boot>=0) close(v->boot);
    for (size_t n=0;n<3;++n) if (v->namespaces[n]>=0) close(v->namespaces[n]);
    free(v); *output=NULL; return 0;
}
