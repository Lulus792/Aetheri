#include "authority/setup.h"
#include "catalogue/data.h"
#include "core/digest.h"
#include "core/fixture.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/openat2.h>
#include <selinux/selinux.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <systemd/sd-id128.h>
#include <unistd.h>

#define ROOT "/var/lib/aetheri-p0"
static int label_is(int fd,const char *type)
{
    char *text=NULL; if (fgetfilecon_raw(fd,&text)<0) return -errno;
    char wanted[80]; int n=snprintf(wanted,sizeof wanted,":%s:",type);
    int result=n>0 && n<(int)sizeof wanted && strstr(text,wanted)!=NULL ? 0 : -EACCES;
    freecon(text); return result;
}
static int open_checked(const char *path,int directory,const char *type,uid_t uid,mode_t mode)
{
    struct open_how how={.flags=(uint64_t)(O_RDONLY|O_CLOEXEC|O_NOFOLLOW|(directory ? O_DIRECTORY : O_NONBLOCK)),.resolve=RESOLVE_NO_SYMLINKS|RESOLVE_NO_MAGICLINKS};
    int fd=(int)syscall(SYS_openat2,AT_FDCWD,path,&how,sizeof how); if (fd<0) return -errno;
    struct stat status; int result=fstat(fd,&status)==0 ? 0 : -errno;
    if (result==0 && ((directory ? !S_ISDIR(status.st_mode) : !S_ISREG(status.st_mode)) || status.st_uid!=uid || (status.st_mode&07777)!=mode || (!directory && status.st_nlink!=1))) result=-EACCES;
    if (result==0) result=label_is(fd,type);
    if (result!=0) { close(fd); return result; } return fd;
}
static int bytes(const char *path,uint8_t *output,size_t size)
{
    int fd=open_checked(path,0,"aetheri_registry_t",0,0444); if (fd<0) return fd;
    struct stat status; int result=fstat(fd,&status)==0 ? 0 : -errno;
    if (result==0 && status.st_size!=(off_t)size) result=-EBADMSG;
    if (result==0 && pread(fd,output,size,0)!=(ssize_t)size) result=-EBADMSG;
    if (close(fd)!=0 && result==0) result=-errno;
    return result;
}
int ae_setup_load(struct ae_setup_profile *output,uint8_t digest[32])
{
    if (output==NULL || digest==NULL) return -EINVAL;
    uint8_t input[AE_SETUP_BYTES],hash[32]; struct ae_setup_profile p;
    int result=bytes(ROOT "/registry/setup.conf",input,sizeof input);
    if (result==0) result=ae_setup_decode(input,sizeof input,&p);
    if (result==0) result=ae_sha256_bytes(input,sizeof input,hash);
    if (result==0) { *output=p; memcpy(digest,hash,32); } return result;
}
int ae_setup_verify_image(const struct ae_setup_profile *p)
{
    if (p==NULL) return -EINVAL;
    int fd=open_checked(ROOT "/setup-helper",0,"aetheri_setup_exec_t",0,0555);
    if (fd<0) return fd;
    struct stat status; uint8_t hash[32]; int r=fstat(fd,&status)==0 ? 0 : -errno;
    if (r==0 && status.st_size<1) r=-EBADMSG;
    if (r==0) r=ae_sha256_fd(fd,(uint64_t)status.st_size,hash);
    if (r==0 && memcmp(hash,p->image_digest,32)!=0) r=-ESTALE;
    close(fd); return r;
}
static uint64_t number(const uint8_t *p,size_t n)
{ uint64_t v=0; for (size_t i=0;i<n;++i) v=(v<<8)|p[i]; return v; }
static int witness(int fd,const uint8_t *p)
{
    struct ae_root_identity identity; int result=ae_root_observe(fd,&identity);
    if (result==0 && (!identity.birth_present || (uint64_t)identity.filesystem_type!=number(p,8) || memcmp(identity.filesystem_id,p+8,8)!=0 ||
        identity.inode!=number(p+16,8) || (uint64_t)identity.birth_seconds!=number(p+24,8) || identity.birth_nanoseconds!=number(p+32,4))) result=-ESTALE;
    return result;
}
static int allocation_object(int fd,const struct ae_allocation *a,const struct ae_allocation_object *object)
{
    struct ae_root_identity identity; int result=ae_root_observe(fd,&identity);
    if (result==0 && (!identity.birth_present || (uint64_t)identity.filesystem_type!=a->filesystem_type || memcmp(identity.filesystem_id,a->filesystem_id,8)!=0 ||
        identity.inode!=object->inode || (uint64_t)identity.birth_seconds!=object->birth_seconds || identity.birth_nanoseconds!=object->birth_nanoseconds)) result=-ESTALE;
    return result;
}
int ae_setup_sources(const struct ae_setup_profile *p,int *code,int *data)
{
    if (p==NULL || code==NULL || data==NULL || code==data) return -EINVAL;
    char artifact[65],account[33],storage[33],path[512];
    for (size_t n=0;n<32;++n) snprintf(artifact+2*n,3,"%02x",p->artifact[n]);
    sd_id128_t id; memcpy(id.bytes,p->account,16); sd_id128_to_string(id,account); memcpy(id.bytes,p->storage,16); sd_id128_to_string(id,storage);
    uint8_t publication[112],input[AE_DATA_CONFIG_BYTES];
    int result=bytes(ROOT "/registry/fixture-store.conf",publication,sizeof publication);
    if (result==0 && memcmp(publication,"AETHSTO1",8)!=0) result=-EBADMSG;
    int container=-1,manifest=-1,source=-1,anchor=-1,root=-1,payload=-1,owner=-1;
    snprintf(path,sizeof path,ROOT "/store/sha256/%s",artifact);
    if (result==0) { container=open_checked(path,1,"aetheri_store_dir_t",0,0555); result=container<0 ? container : witness(container,publication+76); }
    uint8_t *manifest_bytes=malloc(AE_FIXTURE_MAX_BYTES); if (result==0 && manifest_bytes==NULL) result=-ENOMEM;
    snprintf(path,sizeof path,ROOT "/store/sha256/%s/manifest",artifact);
    if (result==0) { manifest=open_checked(path,0,"aetheri_store_manifest_t",0,0444); result=manifest<0 ? manifest : 0; }
    struct stat status; struct ae_fixture fixture;
    if (result==0 && fstat(manifest,&status)!=0) result=-errno;
    if (result==0 && (status.st_size<1 || status.st_size>AE_FIXTURE_MAX_BYTES)) result=-E2BIG;
    if (result==0 && pread(manifest,manifest_bytes,(size_t)status.st_size,0)!=status.st_size) result=-EBADMSG;
    if (result==0) result=ae_fixture_verify_manifest(manifest_bytes,(size_t)status.st_size,p->artifact,&fixture);
    snprintf(path,sizeof path,ROOT "/store/sha256/%s/payload",artifact);
    if (result==0) { source=open_checked(path,1,"aetheri_store_dir_t",0,0555); result=source<0 ? source : 0; }
    /* Fixed trusted fixture: manifest names are validated bounded text. openat2
     * still independently excludes child mount/symlink traversal for every file. */
    for (uint32_t n=0;result==0 && n<fixture.file_count;++n) {
        const struct ae_fixture_file *file=&fixture.files[n]; char relative[4097];
        if (file->path.size>=sizeof relative) { result=-E2BIG; break; }
        memcpy(relative,file->path.data,file->path.size); relative[file->path.size]=0;
        struct open_how how={.flags=O_RDONLY|O_CLOEXEC|O_NOFOLLOW,.resolve=RESOLVE_BENEATH|RESOLVE_NO_SYMLINKS|RESOLVE_NO_MAGICLINKS|RESOLVE_NO_XDEV};
        int fd=(int)syscall(SYS_openat2,source,relative,&how,sizeof how);
        if (fd<0) result=-errno;
        if (result==0 && (fstat(fd,&status)!=0 || !S_ISREG(status.st_mode) || status.st_uid!=0 || status.st_nlink!=1 || (status.st_mode&07777)!=file->mode)) result=-EACCES;
        if (result==0) result=label_is(fd,file->mode==0555 ? "aetheri_store_code_t" : "aetheri_store_data_t");
        if (result==0) result=ae_fixture_verify_file(file,fd);
        if (fd>=0) close(fd);
    }
    if (result==0) result=bytes(ROOT "/registry/app-data.conf",input,sizeof input);
    struct ae_data_enrollment d;
    if (result==0) result=ae_data_enrollment_decode(input,sizeof input,&d);
    if (result==0 && (memcmp(d.owner.id[AE_ACCOUNT],p->account,16)!=0 || memcmp(d.owner.id[AE_DATA],p->data,16)!=0 || memcmp(d.owner.id[AE_STORAGE],p->storage,16)!=0 ||
        memcmp(d.install,p->install,16)!=0 || memcmp(d.system_epoch,p->system_epoch,16)!=0 || memcmp(d.user_epoch,p->broker_epoch,16)!=0 ||
        memcmp(d.publisher,fixture.publisher,16)!=0 || d.install_revision!=p->install_revision)) result=-ESTALE;
    snprintf(path,sizeof path,ROOT "/users/%s/app-data",account);
    if (result==0) { anchor=open_checked(path,1,"aetheri_user_state_t",1000,0700); result=anchor<0 ? anchor : allocation_object(anchor,&d.allocation,&d.allocation.anchor); }
    snprintf(path,sizeof path,ROOT "/users/%s/app-data/%s",account,storage);
    if (result==0) { root=open_checked(path,1,"aetheri_user_state_t",1000,0700); result=root<0 ? root : allocation_object(root,&d.allocation,&d.allocation.root); }
    snprintf(path,sizeof path,ROOT "/users/%s/app-data/%s/owner",account,storage);
    uint8_t expected[AE_OWNER_BYTES],actual[AE_OWNER_BYTES];
    if (result==0 && ae_owner_encode(&d.owner,expected,sizeof expected)!=0) result=-EBADMSG;
    if (result==0) { owner=open_checked(path,0,"aetheri_user_state_t",1000,0600); result=owner<0 ? owner : 0; }
    if (result==0 && (fstat(owner,&status)!=0 || status.st_size!=AE_OWNER_BYTES || pread(owner,actual,sizeof actual,0)!=AE_OWNER_BYTES || memcmp(expected,actual,sizeof actual)!=0)) result=-ESTALE;
    snprintf(path,sizeof path,ROOT "/users/%s/app-data/%s/payload",account,storage);
    if (result==0) { payload=open_checked(path,1,"aetheri_app_data_t",1000,0700); result=payload<0 ? payload : 0; }
    /* Data allocation's serialized witness excludes mount IDs intentionally.
     * The controlled service's writable payload bind is checked by object/owner,
     * then pinned with current unique mount identity by the view primitive. */
    if (result==0) { *code=source; *data=payload; source=-1; payload=-1; }
    int fds[]={container,manifest,source,anchor,root,payload,owner};
    for (size_t n=0;n<sizeof fds/sizeof fds[0];++n) if (fds[n]>=0) close(fds[n]);
    free(manifest_bytes); return result;
}
int ae_setup_context_digest(const struct ae_process_context *c,const uint8_t h[32],uint8_t output[32])
{
    _Static_assert(90+sizeof c->unit+sizeof c->label+sizeof c->cgroup<=2048,"setup context encoding capacity");
    if (c==NULL || h==NULL || output==NULL || strnlen(c->unit,sizeof c->unit)>=sizeof c->unit || strnlen(c->label,sizeof c->label)>=sizeof c->label || strnlen(c->cgroup,sizeof c->cgroup)>=sizeof c->cgroup) return -EINVAL;
    uint8_t bytes[2048]={0}; size_t used=0;
#define COPY(p,n) do { memcpy(bytes+used,p,n); used+=(n); } while (0)
    COPY("AETHSPC1",8); COPY(h,32); COPY(c->boot,16); COPY(c->invocation,16);
    for (size_t n=0;n<8;++n) bytes[used++]=(uint8_t)(c->cgroup_id>>(56-8*n));
    for (size_t n=0;n<4;++n) bytes[used++]=(uint8_t)((uint32_t)c->uid>>(24-8*n));
    const char *texts[]={c->unit,c->label,c->cgroup};
    for (size_t n=0;n<3;++n) { size_t size=strlen(texts[n]); bytes[used++]=(uint8_t)(size>>8); bytes[used++]=(uint8_t)size; COPY(texts[n],size); }
#undef COPY
    return ae_sha256_bytes(bytes,used,output);
}
