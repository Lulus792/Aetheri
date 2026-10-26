#include "core/publish.h"
#include "core/fixture.h"
#include "core/import.h"
#include "core/root.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/openat2.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/xattr.h>
#include <unistd.h>

static int leaf(const char *name)
{
    if (name == NULL) return 0;
    size_t size = strnlen(name,65);
    if (size == 0 || size > 64 || strcmp(name,".") == 0 || strcmp(name,"..") == 0) return 0;
    for (size_t n=0; n<size; ++n) {
        unsigned char c=(unsigned char)name[n];
        if (!((c>='a' && c<='z') || (c>='0' && c<='9') || c=='_' || c=='-')) return 0;
    }
    return 1;
}
static int contained(int root, const char *name, uint64_t flags)
{
    struct open_how how={.flags=flags | O_CLOEXEC | O_NOFOLLOW,
        .resolve=RESOLVE_BENEATH | RESOLVE_NO_SYMLINKS | RESOLVE_NO_MAGICLINKS | RESOLVE_NO_XDEV};
    int fd;
    do { fd=(int)syscall(SYS_openat2,root,name,&how,sizeof how); } while (fd<0 && errno==EINTR);
    return fd<0 ? -errno : fd;
}
static int release(int fd,int result)
{
    if (fd>=0 && close(fd)!=0 && result==0) result=-errno;
    return result;
}
static int attrs(int fd)
{
    char names[1024];
    ssize_t size=flistxattr(fd,names,sizeof names);
    if (size<0) return -errno;
    size_t used=0;
    while (used<(size_t)size) {
        const char *end=memchr(names+used,0,(size_t)size-used);
        if (end==NULL || strcmp(names+used,"security.selinux")!=0) return -EACCES;
        used+=(size_t)(end-(names+used))+1U;
    }
    return 0;
}
static int directory(int fd,struct ae_root_identity *identity)
{
    int result=ae_root_observe(fd,identity);
    if (result==0 && (identity->uid!=geteuid() ||
        ((identity->mode&07777U)!=0700 && (identity->mode&07777U)!=0555))) result=-EACCES;
    if (result==0) result=attrs(fd);
    return result;
}
static int named(int parent,const char *name,int held,int *present)
{
    int fd=contained(parent,name,O_RDONLY | O_DIRECTORY);
    if (fd==-ENOENT) { *present=0; return 0; }
    if (fd<0) return fd;
    struct ae_root_identity current,expected;
    int result=ae_root_observe(held,&expected);
    if (result==0) result=ae_root_observe(fd,&current);
    if (result==0 && !ae_root_equal(&current,&expected)) result=-ESTALE;
    result=release(fd,result);
    if (result==0) *present=1;
    return result;
}
static int inventory(int container)
{
    int fd=contained(container,".",O_RDONLY | O_DIRECTORY);
    if (fd<0) return fd;
    DIR *stream=fdopendir(fd);
    if (stream==NULL) return release(fd,-errno);
    unsigned int seen=0;
    int result=0;
    for (;;) {
        errno=0;
        struct dirent *entry=readdir(stream);
        if (entry==NULL) { if (errno!=0) result=-errno; break; }
        if (strcmp(entry->d_name,".")==0 || strcmp(entry->d_name,"..")==0) continue;
        unsigned int bit=strcmp(entry->d_name,"manifest")==0 ? 1U : strcmp(entry->d_name,"payload")==0 ? 2U : 0U;
        if (bit==0 || (seen&bit)!=0) { result=-EBADMSG; break; }
        seen|=bit;
    }
    if (closedir(stream)!=0 && result==0) result=-errno;
    return result;
}
static int existing_manifest(int root,const uint8_t *bytes,size_t size)
{
    int pin=contained(root,"manifest",O_PATH);
    if (pin<0) return pin;
    struct stat before,after;
    int result=fstat(pin,&before)==0 ? 0 : -errno;
    if (result==0 && (!S_ISREG(before.st_mode) || before.st_nlink!=1 || before.st_size<0 ||
        (uint64_t)before.st_size!=size)) result=-EBADMSG;
    if (result==0 && (before.st_uid!=geteuid() || (before.st_mode&07777)!=0444)) result=-EACCES;
    int fd=-1;
    if (result==0) {
        char proc[64];
        int count=snprintf(proc,sizeof proc,"/proc/self/fd/%d",pin);
        if (count<0 || (size_t)count>=sizeof proc) result=-EOVERFLOW;
        else { fd=open(proc,O_RDONLY | O_CLOEXEC | O_NONBLOCK); if (fd<0) result=-errno; }
    }
    if (result==0) result=attrs(fd);
    uint8_t buffer[4096];
    size_t used=0;
    while (result==0 && used<size) {
        size_t count=size-used<sizeof buffer ? size-used : sizeof buffer;
        ssize_t received=pread(fd,buffer,count,(off_t)used);
        if (received<0) { if (errno==EINTR) continue; result=-errno; }
        else if (received==0 || (size_t)received>count) result=-EBADMSG;
        else if (memcmp(bytes+used,buffer,(size_t)received)!=0) result=-EBADMSG;
        else used+=(size_t)received;
    }
    if (result==0 && fstat(fd,&after)!=0) result=-errno;
    if (result==0 && (before.st_dev!=after.st_dev || before.st_ino!=after.st_ino || before.st_uid!=after.st_uid ||
        before.st_gid!=after.st_gid || before.st_mode!=after.st_mode || before.st_nlink!=after.st_nlink || before.st_size!=after.st_size ||
        before.st_ctim.tv_sec!=after.st_ctim.tv_sec || before.st_ctim.tv_nsec!=after.st_ctim.tv_nsec ||
        before.st_mtim.tv_sec!=after.st_mtim.tv_sec || before.st_mtim.tv_nsec!=after.st_mtim.tv_nsec)) result=-ESTALE;
    if (result==0 && fsync(fd)!=0) result=-errno;
    result=release(fd,result);
    return release(pin,result);
}
static int manifest_file(int root,const uint8_t *bytes,size_t size)
{
    int result=existing_manifest(root,bytes,size);
    if (result!=-ENOENT) return result;
    int fd=openat(root,".",O_TMPFILE | O_RDWR | O_CLOEXEC,0600);
    if (fd<0) return -errno;
    size_t used=0;
    result=0;
    while (result==0 && used<size) {
        ssize_t count=write(fd,bytes+used,size-used);
        if (count<0) { if (errno==EINTR) continue; result=-errno; }
        else if (count==0 || (size_t)count>size-used) result=-EIO;
        else used+=(size_t)count;
    }
    if (result==0 && fchmod(fd,0444)!=0) result=-errno;
    if (result==0) result=attrs(fd);
    if (result==0 && fsync(fd)!=0) result=-errno;
    if (result==0) {
        char proc[64];
        int count=snprintf(proc,sizeof proc,"/proc/self/fd/%d",fd);
        if (count<0 || (size_t)count>=sizeof proc) result=-EOVERFLOW;
        else if (linkat(AT_FDCWD,proc,root,"manifest",AT_SYMLINK_FOLLOW)!=0 && errno!=EEXIST) result=-errno;
    }
    result=release(fd,result);
    if (result==0) result=existing_manifest(root,bytes,size);
    if (result==0 && fsync(root)!=0) result=-errno;
    return result;
}

int ae_fixture_publish_namespace(const struct ae_fixture_publication *publication,
                                  const uint8_t *manifest,size_t size,const uint8_t artifact[32])
{
    if (publication==NULL || !leaf(publication->staging_name) || !leaf(publication->container_name) ||
        publication->staging_parent<0 || publication->container_parent<0 || publication->store_parent<0 ||
        publication->payload<0 || publication->container<0) return -EINVAL;
    const struct ae_fixture_publication *p=publication;
    struct ae_fixture fixture;
    int result=ae_fixture_verify_manifest(manifest,size,artifact,&fixture);
    struct ae_root_identity roots[5];
    const int descriptors[]={p->staging_parent,p->container_parent,p->store_parent,p->payload,p->container};
    for (size_t n=0; result==0 && n<5; ++n) result=directory(descriptors[n],&roots[n]);
    if (result==0 && (roots[0].mount_id!=roots[1].mount_id || roots[0].mount_id!=roots[2].mount_id ||
        roots[0].mount_id!=roots[3].mount_id || roots[0].mount_id!=roots[4].mount_id)) result=-EXDEV;
    if (result==0 && (roots[3].inode==roots[4].inode || roots[2].inode==roots[0].inode || roots[2].inode==roots[1].inode)) result=-EINVAL;
    char target[65];
    for (size_t n=0; result==0 && n<32; ++n) snprintf(target+n*2U,3,"%02x",(unsigned int)artifact[n]);
    int local=0,stored=0,staged=0,nested=0;
    if (result==0) result=named(p->container_parent,p->container_name,p->container,&local);
    if (result==0) result=named(p->store_parent,target,p->container,&stored);
    if (result==0 && local+stored!=1) result=-ESTALE;
    if (result==0) result=named(p->staging_parent,p->staging_name,p->payload,&staged);
    if (result==0) result=named(p->container,"payload",p->payload,&nested);
    if (result==0 && staged+nested!=1) result=-ESTALE;
    if (result==0 && stored && staged) result=-ESTALE;
    if (result==0) result=inventory(p->container);
    if (result==0) result=ae_fixture_sync_payload(p->payload,roots[3].mode&07777U,manifest,size,artifact);
    if (result==0) result=stored ? existing_manifest(p->container,manifest,size) : manifest_file(p->container,manifest,size);
    if (result==0 && staged && renameat2(p->staging_parent,p->staging_name,p->container,"payload",RENAME_NOREPLACE)!=0) result=-errno;
    if (result==0) result=named(p->container,"payload",p->payload,&nested);
    if (result==0 && !nested) result=-ESTALE;
    if (result==0) result=named(p->staging_parent,p->staging_name,p->payload,&staged);
    if (result==0 && staged) result=-ESTALE;
    if (result==0 && fsync(p->payload)!=0) result=-errno;
    if (result==0 && fsync(p->staging_parent)!=0) result=-errno;
    if (result==0 && fsync(p->container)!=0) result=-errno;
    if (result==0 && local && renameat2(p->container_parent,p->container_name,p->store_parent,target,RENAME_NOREPLACE)!=0) result=-errno;
    if (result==0) result=named(p->store_parent,target,p->container,&stored);
    if (result==0 && !stored) result=-ESTALE;
    if (result==0) result=named(p->container_parent,p->container_name,p->container,&local);
    if (result==0 && local) result=-ESTALE;
    if (result==0) result=inventory(p->container);
    if (result==0 && fsync(p->container)!=0) result=-errno;
    if (result==0 && fsync(p->container_parent)!=0) result=-errno;
    if (result==0 && fsync(p->store_parent)!=0) result=-errno;
    return result;
}
