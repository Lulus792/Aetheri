#include "linux/image.h"
#include "core/digest.h"
#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/magic.h>
#include <selinux/selinux.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/pidfd.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <sys/xattr.h>
#include <unistd.h>

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

#if !defined(__x86_64__) || __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "The P0 executable profile requires native little-endian x86-64"
#endif

enum { MAX_IMAGE=16*1024*1024 };
static int snapshot(int fd,struct ae_image *output)
{
    struct statx s;
    unsigned int fields=STATX_TYPE|STATX_MODE|STATX_UID|STATX_GID|STATX_INO|STATX_NLINK|STATX_SIZE|STATX_BTIME|STATX_MTIME|STATX_CTIME;
    if (statx(fd,"",AT_EMPTY_PATH|AT_SYMLINK_NOFOLLOW,fields,&s)!=0) return -errno;
    if ((s.stx_mask&fields)!=fields) return -ENOTSUP;
    if (!S_ISREG(s.stx_mode) || s.stx_uid!=0 || s.stx_gid!=0 || (s.stx_mode&07777)!=0555 || s.stx_nlink!=1) return -EACCES;
    ssize_t attributes=fgetxattr(fd,"security.capability",NULL,0);
    if (attributes>=0) return -EACCES;
    if (errno!=ENODATA) return -errno;
    if (s.stx_size<sizeof(Elf64_Ehdr) || s.stx_size>MAX_IMAGE || s.stx_ino==0 || s.stx_btime.tv_sec<0 ||
        s.stx_btime.tv_nsec>=1000000000U || s.stx_mtime.tv_nsec>=1000000000U || s.stx_ctime.tv_nsec>=1000000000U) return -EBADMSG;
    *output=(struct ae_image){.inode=s.stx_ino,.size=s.stx_size,.device_major=s.stx_dev_major,.device_minor=s.stx_dev_minor,
        .uid=s.stx_uid,.gid=s.stx_gid,.mode=s.stx_mode,.birth_seconds=s.stx_btime.tv_sec,.birth_ns=s.stx_btime.tv_nsec,
        .mtime_seconds=s.stx_mtime.tv_sec,.mtime_ns=s.stx_mtime.tv_nsec,.ctime_seconds=s.stx_ctime.tv_sec,.ctime_ns=s.stx_ctime.tv_nsec};
    return 0;
}
int ae_image_equal(const struct ae_image *a,const struct ae_image *b)
{
    return a!=NULL && b!=NULL && a->inode==b->inode && a->size==b->size && a->device_major==b->device_major && a->device_minor==b->device_minor &&
        a->uid==b->uid && a->gid==b->gid && a->mode==b->mode && a->birth_seconds==b->birth_seconds && a->birth_ns==b->birth_ns &&
        a->mtime_seconds==b->mtime_seconds && a->mtime_ns==b->mtime_ns && a->ctime_seconds==b->ctime_seconds && a->ctime_ns==b->ctime_ns && memcmp(a->digest,b->digest,32)==0;
}
static int read_at(int fd,void *output,size_t size,uint64_t offset)
{
    ssize_t n; do { n=pread(fd,output,size,(off_t)offset); } while (n<0 && errno==EINTR);
    return n<0 ? -errno : (size_t)n==size ? 0 : -EBADMSG;
}
static int elf(int fd,uint64_t size)
{
    _Static_assert(sizeof(void *)==8,"x86-64 ELF execution profile");
    Elf64_Ehdr h; int r=read_at(fd,&h,sizeof h,0);
    if (r!=0) return r;
    if (memcmp(h.e_ident,ELFMAG,SELFMAG)!=0 || h.e_ident[EI_CLASS]!=ELFCLASS64 || h.e_ident[EI_DATA]!=ELFDATA2LSB ||
        h.e_ident[EI_VERSION]!=EV_CURRENT || (h.e_ident[EI_OSABI]!=ELFOSABI_SYSV && h.e_ident[EI_OSABI]!=ELFOSABI_LINUX) ||
        h.e_ident[EI_ABIVERSION]!=0 || h.e_machine!=EM_X86_64 || h.e_version!=EV_CURRENT || (h.e_type!=ET_DYN && h.e_type!=ET_EXEC) ||
        h.e_ehsize!=sizeof h || h.e_phentsize!=sizeof(Elf64_Phdr) || h.e_phnum<1 || h.e_phnum>128 ||
        h.e_phoff<sizeof h || h.e_phoff>size || (uint64_t)h.e_phnum*sizeof(Elf64_Phdr)>size-h.e_phoff) return -ENOEXEC;
    unsigned int interpreters=0,stacks=0,entries=0;
    for (uint16_t n=0;n<h.e_phnum;++n) {
        Elf64_Phdr p; r=read_at(fd,&p,sizeof p,h.e_phoff+(uint64_t)n*sizeof p); if (r!=0) return r;
        if (p.p_offset>size || p.p_filesz>size-p.p_offset) return -ENOEXEC;
        if (p.p_type==PT_LOAD) {
            if (p.p_filesz>p.p_memsz || ((p.p_flags&PF_W) && (p.p_flags&PF_X))) return -ENOEXEC;
            if ((p.p_flags&PF_X) && h.e_entry>=p.p_vaddr && h.e_entry-p.p_vaddr<p.p_memsz) ++entries;
        }
        if (p.p_type==PT_GNU_STACK && (++stacks!=1 || (p.p_flags&PF_X))) return -ENOEXEC;
        if (p.p_type==PT_INTERP) {
            static const char expected[]="/lib64/ld-linux-x86-64.so.2"; char actual[sizeof expected];
            if (++interpreters!=1 || p.p_filesz!=sizeof expected) return -ENOEXEC;
            r=read_at(fd,actual,sizeof actual,p.p_offset); if (r!=0) return r;
            if (memcmp(actual,expected,sizeof actual)!=0) return -ENOEXEC;
        }
    }
    return stacks==1 && entries>0 ? 0 : -ENOEXEC;
}
int ae_image_verify(int fd,const uint8_t expected[32],struct ae_image *output)
{
    if (fd<0 || expected==NULL || output==NULL) return -EINVAL;
    int flags=fcntl(fd,F_GETFL);
    if (flags<0) return -errno;
    if ((flags&O_ACCMODE)!=O_RDONLY || (flags&O_PATH)) return -EACCES;
    struct ae_image first,last; int r=snapshot(fd,&first);
    if (r==0) r=elf(fd,first.size);
    if (r==0) r=ae_sha256_fd(fd,first.size,first.digest);
    if (r==0 && memcmp(first.digest,expected,32)!=0) r=-EBADMSG;
    if (r==0) r=snapshot(fd,&last);
    if (r==0) { memcpy(last.digest,first.digest,32); if (!ae_image_equal(&first,&last)) r=-ESTALE; }
    if (r==0) *output=first;
    return r;
}
static int identity(const struct ae_peer *p,gid_t gid,uint64_t *cgroup)
{
    struct pidfd_info i={.mask=PIDFD_INFO_PID|PIDFD_INFO_CREDS|PIDFD_INFO_CGROUPID};
    if (ioctl(p->pidfd,PIDFD_GET_INFO,&i)!=0) return -errno;
    uint64_t required=PIDFD_INFO_PID|PIDFD_INFO_CREDS|PIDFD_INFO_CGROUPID;
    if ((i.mask&required)!=required) return -ENODATA;
    uint32_t uid=(uint32_t)p->connection_uid;
    if (i.pid!=(uint32_t)p->connection_pid || i.tgid!=i.pid || i.ruid!=uid || i.euid!=uid || i.suid!=uid || i.fsuid!=uid ||
        i.rgid!=(uint32_t)gid || i.egid!=(uint32_t)gid || i.sgid!=(uint32_t)gid || i.fsgid!=(uint32_t)gid || i.cgroupid==0) return -EACCES;
    *cgroup=i.cgroupid; return ae_peer_alive(p);
}
int ae_image_match_process(const struct ae_peer *p,gid_t gid,const struct ae_image *expected,struct ae_image *output)
{
    if (p==NULL || expected==NULL || output==NULL || p->pidfd<0 || p->connection_pid<1 || p->connection_uid==0 || gid==0 ||
        strnlen(p->connection_label,sizeof p->connection_label)==0 || strnlen(p->connection_label,sizeof p->connection_label)>=sizeof p->connection_label) return -EINVAL;
    uint64_t first=0,last=0; int r=identity(p,gid,&first);
    char path[64]; int count=snprintf(path,sizeof path,"/proc/%ld",(long)p->connection_pid);
    if (r==0 && (count<1 || count>=(int)sizeof path)) r=-EOVERFLOW;
    int process=-1,executable=-1,again=-1; struct statfs fs;
    if (r==0) { process=open(path,O_PATH|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW); if (process<0) r=-errno; }
    if (r==0 && fstatfs(process,&fs)!=0) r=-errno;
    if (r==0 && fs.f_type!=PROC_SUPER_MAGIC) r=-ENOTSUP;
    char *label=NULL;
    if (r==0 && getpidcon_raw(p->connection_pid,&label)!=0) r=-errno;
    if (r==0 && strcmp(label,p->connection_label)!=0) r=-ESTALE;
    freecon(label);
    if (r==0) { executable=openat(process,"exe",O_RDONLY|O_CLOEXEC); if (executable<0) r=-errno; }
    struct ae_image actual,second;
    if (r==0) r=ae_image_verify(executable,expected->digest,&actual);
    if (r==0 && !ae_image_equal(&actual,expected)) r=-ESTALE;
    if (r==0) { again=openat(process,"exe",O_RDONLY|O_CLOEXEC); if (again<0) r=-errno; }
    if (r==0) r=snapshot(again,&second);
    if (r==0) { memcpy(second.digest,actual.digest,32); if (!ae_image_equal(&actual,&second)) r=-ESTALE; }
    if (r==0) r=identity(p,gid,&last);
    if (r==0 && first!=last) r=-ESTALE;
    label=NULL;
    if (r==0 && getpidcon_raw(p->connection_pid,&label)!=0) r=-errno;
    if (r==0 && strcmp(label,p->connection_label)!=0) r=-ESTALE;
    freecon(label);
    if (r==0) r=ae_peer_alive(p);
    int fds[]={process,executable,again}; for (size_t n=0;n<3;++n) if (fds[n]>=0) close(fds[n]);
    if (r==0) *output=actual;
    return r;
}
