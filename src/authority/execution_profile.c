#include "authority/execution_profile.h"
#include "authority/fixture.h"
#include "catalogue/data.h"
#include "core/digest.h"
#include "core/import.h"
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
#define LABEL(type) "system_u:object_r:" type ":s0"

static int label(int fd,const char *expected)
{
    char *actual=NULL; if (fgetfilecon_raw(fd,&actual)<0) return -errno;
    int r=strcmp(actual,expected)==0 ? 0 : -EACCES; freecon(actual); return r;
}
static int opened(int base,const char *path,int directory,uid_t uid,mode_t mode,const char *context,int cross)
{
    struct open_how how={.flags=(uint64_t)(O_RDONLY|O_NOFOLLOW|O_CLOEXEC|(directory ? O_DIRECTORY : O_NONBLOCK)),
        .resolve=RESOLVE_NO_SYMLINKS|RESOLVE_NO_MAGICLINKS};
    if (base!=AT_FDCWD) how.resolve|=RESOLVE_BENEATH|(cross ? 0U : RESOLVE_NO_XDEV);
    int fd; do { fd=(int)syscall(SYS_openat2,base,path,&how,sizeof how); } while (fd<0 && errno==EINTR);
    if (fd<0) return -errno;
    struct stat s; int r=fstat(fd,&s)==0 ? 0 : -errno;
    if (r==0 && ((directory ? !S_ISDIR(s.st_mode) : !S_ISREG(s.st_mode)) || s.st_uid!=uid || s.st_gid!=uid ||
        (s.st_mode&07777)!=mode || (!directory && s.st_nlink!=1))) r=-EACCES;
    if (r==0) r=label(fd,context);
    if (r!=0) { close(fd); return r; } return fd;
}
static int root_open(void)
{ return opened(AT_FDCWD,ROOT,1,0,0755,LABEL("aetheri_root_t"),0); }
static int bytes(int base,const char *path,uid_t uid,mode_t mode,const char *context,uint8_t *output,size_t size)
{
    int fd=opened(base,path,0,uid,mode,context,0); if (fd<0) return fd;
    struct stat s; int r=fstat(fd,&s)==0 ? 0 : -errno;
    if (r==0 && s.st_size!=(off_t)size) r=-EBADMSG;
    size_t used=0;
    while (r==0 && used<size) {
        ssize_t n=pread(fd,output+used,size-used,(off_t)used);
        if (n<0) { if (errno!=EINTR) r=-errno; }
        else if (n==0) r=-EBADMSG; else used+=(size_t)n;
    }
    if (close(fd)!=0 && r==0) r=-errno;
    return r;
}
static int registry(int root,const char *absolute,uint8_t *output,size_t size)
{
    int fd=opened(root,"registry",1,0,0755,LABEL("aetheri_registry_t"),0); if (fd<0) return fd;
    const char *leaf=strrchr(absolute,'/');
    int r=bytes(fd,leaf+1,0,0444,LABEL("aetheri_registry_t"),output,size);
    if (close(fd)!=0 && r==0) r=-errno;
    return r;
}
static void hex_id(const uint8_t value[16],char output[33])
{ sd_id128_t id; memcpy(id.bytes,value,16); sd_id128_to_string(id,output); }
static int object(int fd,const struct ae_root_identity *expected)
{
    struct ae_root_identity actual; int r=ae_root_observe(fd,&actual);
    if (r==0 && (!actual.birth_present || actual.filesystem_type!=expected->filesystem_type ||
        memcmp(actual.filesystem_id,expected->filesystem_id,8)!=0 || actual.inode!=expected->inode ||
        actual.birth_seconds!=expected->birth_seconds || actual.birth_nanoseconds!=expected->birth_nanoseconds)) r=-ESTALE;
    return r;
}
static int allocated(int fd,const struct ae_allocation *a,const struct ae_allocation_object *o)
{
    struct ae_root_identity expected={.filesystem_type=(int64_t)a->filesystem_type,.inode=o->inode,
        .birth_seconds=(int64_t)o->birth_seconds,.birth_nanoseconds=o->birth_nanoseconds};
    memcpy(expected.filesystem_id,a->filesystem_id,8); return object(fd,&expected);
}
int ae_execution_profile_load(struct ae_execution_profile *output,uint8_t digest[32])
{
    if (output==NULL || digest==NULL) return -EINVAL;
    int root=root_open(); if (root<0) return root;
    uint8_t input[AE_EXECUTION_BYTES],hash[32]; struct ae_execution_profile p;
    int r=registry(root,ROOT "/registry/execution.conf",input,sizeof input);
    if (r==0) r=ae_execution_decode(input,sizeof input,&p);
    if (r==0) r=ae_sha256_bytes(input,sizeof input,hash);
    if (close(root)!=0 && r==0) r=-errno;
    if (r==0) { *output=p; memcpy(digest,hash,32); } return r;
}
int ae_execution_starter_open(const struct ae_execution_profile *p,int *output,struct ae_image *image)
{
    if (p==NULL || output==NULL || *output>=0 || image==NULL) return -EINVAL;
    uint8_t hash[32]; int r=ae_execution_digest(p,hash); int root=-1,fd=-1; struct ae_image actual;
    if (r==0) { root=root_open(); if (root<0) r=root; }
    if (r==0) { fd=opened(root,"execution-starter",0,0,0555,LABEL("aetheri_starter_exec_t"),0); if (fd<0) r=fd; }
    if (r==0) r=ae_image_verify(fd,p->starter_digest,&actual);
    if (root>=0 && close(root)!=0 && r==0) r=-errno;
    if (r==0) { *output=fd; *image=actual; } else if (fd>=0) close(fd);
    return r;
}
void ae_execution_sources_clear(struct ae_execution_sources *s)
{
    if (s==NULL) return;
    int fds[]={s->code_fd,s->data_fd,s->image_fd};
    for (size_t n=0;n<3;++n) if (fds[n]>=0) close(fds[n]);
    *s=(struct ae_execution_sources)AE_EXECUTION_SOURCES_INIT;
}
int ae_execution_data_open(const struct ae_execution_profile *p,const uint8_t publisher[16],int *payload,struct ae_data_enrollment *output)
{
    if (p==NULL || publisher==NULL || payload==NULL || *payload>=0 || output==NULL) return -EINVAL;
    uint8_t hash[32],data_bytes[AE_DATA_CONFIG_BYTES]; int r=ae_execution_digest(p,hash);
    const struct ae_fixture_registry *selected=ae_fixture_registry(AE_FIXTURE_EXEC);
    struct ae_data_enrollment data; int root=-1,anchor=-1,data_root=-1,fd=-1;
    if (r==0) { root=root_open(); if (root<0) r=root; }
    if (r==0) r=registry(root,selected->data_config,data_bytes,sizeof data_bytes);
    if (r==0) r=ae_data_enrollment_decode(data_bytes,sizeof data_bytes,&data);
    if (r==0 && (memcmp(data.owner.id[AE_ACCOUNT],p->account,16)!=0 || memcmp(data.owner.id[AE_DATA],p->data,16)!=0 ||
        memcmp(data.owner.id[AE_STORAGE],p->storage,16)!=0 || memcmp(data.install,p->install,16)!=0 ||
        memcmp(data.system_epoch,p->system_epoch,16)!=0 || memcmp(data.user_epoch,p->broker_epoch,16)!=0 ||
        memcmp(data.publisher,publisher,16)!=0 || data.install_revision!=p->install_revision)) r=-ESTALE;
    if (r==0) r=ae_sha256_bytes(data_bytes+200,AE_ALLOCATION_BYTES,hash);
    if (r==0 && memcmp(hash,p->allocation_digest,32)!=0) r=-ESTALE;
    uint8_t expected[AE_OWNER_BYTES],actual[AE_OWNER_BYTES];
    if (r==0 && ae_owner_encode(&data.owner,expected,sizeof expected)!=0) r=-EBADMSG;
    if (r==0) r=registry(root,selected->data_owner,actual,sizeof actual);
    if (r==0 && memcmp(actual,expected,sizeof actual)!=0) r=-ESTALE;
    char account[33],storage[33],path[256]; hex_id(p->account,account); hex_id(p->storage,storage);
    uint8_t allocation[AE_ALLOCATION_BYTES];
    snprintf(path,sizeof path,"users/%s/control/app-allocations/%s",account,storage);
    if (r==0) r=bytes(root,path,1000,0600,LABEL("aetheri_allocation_t"),allocation,sizeof allocation);
    if (r==0 && memcmp(allocation,data_bytes+200,sizeof allocation)!=0) r=-ESTALE;
    snprintf(path,sizeof path,"users/%s/app-data",account);
    if (r==0) { anchor=opened(root,path,1,1000,0700,LABEL("aetheri_user_state_t"),0); r=anchor<0 ? anchor : allocated(anchor,&data.allocation,&data.allocation.anchor); }
    if (r==0) { data_root=opened(anchor,storage,1,1000,0700,LABEL("aetheri_user_state_t"),0); r=data_root<0 ? data_root : allocated(data_root,&data.allocation,&data.allocation.root); }
    if (r==0) r=bytes(data_root,"owner",1000,0600,LABEL("aetheri_user_state_t"),actual,sizeof actual);
    if (r==0 && memcmp(actual,expected,sizeof actual)!=0) r=-ESTALE;
    /* Only this authority-selected payload may be a supervisor-provided writable
     * bind mount. Its ancestry/owner/witness/header and full MCS label are checked;
     * arbitrary mount traversal inside code/config/private metadata is denied. */
    if (r==0) { fd=opened(data_root,"payload",1,1000,0700,selected->data_context,1); if (fd<0) r=fd; }
    int fds[]={root,anchor,data_root};
    for (size_t n=0;n<sizeof fds/sizeof fds[0];++n) if (fds[n]>=0 && close(fds[n])!=0 && r==0) r=-errno;
    if (r==0) { *payload=fd; *output=data; } else if (fd>=0) close(fd);
    return r;
}
int ae_execution_code_open(const struct ae_execution_profile *p,struct ae_execution_sources *output,uint8_t publisher[16])
{
    if (p==NULL || output==NULL || publisher==NULL || output->code_fd>=0 || output->data_fd>=0 || output->image_fd>=0) return -EINVAL;
    uint8_t hash[32]; int r=ae_execution_digest(p,hash);
    const struct ae_fixture_registry *selected=ae_fixture_registry(AE_FIXTURE_EXEC);
    struct ae_execution_sources result=AE_EXECUTION_SOURCES_INIT;
    int root=-1,store=-1,container=-1,manifest=-1;
    uint8_t config_bytes[AE_FIXTURE_CONFIG_BYTES],pub_bytes[AE_FIXTURE_STORE_CONFIG_BYTES];
    struct ae_fixture_config config; struct ae_fixture_store_config publication;
    uint8_t *input=NULL; struct stat status;
    if (r==0) { root=root_open(); if (root<0) r=root; }
    if (r==0) r=registry(root,selected->config,config_bytes,sizeof config_bytes);
    if (r==0) r=ae_fixture_config_decode(config_bytes,sizeof config_bytes,&config);
    if (r==0 && (memcmp(config.request.scope,p->account,16)!=0 || memcmp(config.request.install,p->install,16)!=0 || memcmp(config.request.generation,p->generation,16)!=0 ||
        memcmp(config.request.artifact,p->artifact,32)!=0)) r=-ESTALE;
    if (r==0) r=registry(root,selected->publication,pub_bytes,sizeof pub_bytes);
    if (r==0) r=ae_fixture_store_config_decode(pub_bytes,sizeof pub_bytes,&publication);
    if (r==0) r=ae_sha256_bytes(config_bytes,sizeof config_bytes,hash);
    if (r==0 && memcmp(publication.stage_digest,hash,32)!=0) r=-ESTALE;
    if (r==0) { store=opened(root,"store/sha256",1,0,0700,LABEL("aetheri_store_parent_t"),0); r=store<0 ? store : object(store,&publication.store); }
    char artifact[65]; for (size_t n=0;n<32;++n) snprintf(artifact+2*n,3,"%02x",p->artifact[n]);
    if (r==0) { container=opened(store,artifact,1,0,0555,LABEL("aetheri_store_dir_t"),0); r=container<0 ? container : object(container,&publication.container); }
    if (r==0) { manifest=opened(container,"manifest",0,0,0444,LABEL("aetheri_store_manifest_t"),0); if (manifest<0) r=manifest; }
    if (r==0 && fstat(manifest,&status)!=0) r=-errno;
    if (r==0 && (status.st_size<1 || status.st_size>AE_FIXTURE_MAX_BYTES)) r=-E2BIG;
    if (r==0) { input=malloc((size_t)status.st_size); if (input==NULL) r=-ENOMEM; }
    if (r==0) r=bytes(container,"manifest",0,0444,LABEL("aetheri_store_manifest_t"),input,(size_t)status.st_size);
    struct ae_fixture fixture;
    if (r==0) r=ae_fixture_verify_manifest(input,(size_t)status.st_size,p->artifact,&fixture);
    if (r==0 && (fixture.component.size!=15 || memcmp(fixture.component.data,"aetheri.p0.exec",15)!=0 ||
        memcmp(fixture.publisher,config.request.publisher,16)!=0)) r=-ESTALE;
    if (r==0) r=ae_fixture_resolve_action(input,(size_t)status.st_size,p->artifact,"run",&result.command);
    if (r==0) { result.code_fd=opened(container,"payload",1,0,0555,LABEL("aetheri_store_dir_t"),0); if (result.code_fd<0) r=result.code_fd; }
    if (r==0) r=ae_fixture_check_payload(result.code_fd,input,(size_t)status.st_size,p->artifact);
    for (uint32_t n=0;r==0 && n<fixture.file_count;++n) {
        const struct ae_fixture_file *f=&fixture.files[n]; char relative[257];
        memcpy(relative,f->path.data,f->path.size); relative[f->path.size]=0;
        /* Inventory/content is already pinned and rechecked above. Verify every
         * declared file and directory prefix has its complete sealed MAC label. */
        for (size_t m=0;r==0 && m<f->path.size;++m) if (relative[m]=='/') {
            relative[m]=0; int fd=opened(result.code_fd,relative,1,0,0555,LABEL("aetheri_store_dir_t"),0);
            if (fd<0) r=fd; else close(fd); relative[m]='/';
        }
        int fd=r==0 ? opened(result.code_fd,relative,0,0,(mode_t)f->mode,f->mode==0555 ? LABEL("aetheri_store_code_t") : LABEL("aetheri_store_data_t"),0) : -1;
        if (r==0 && fd<0) r=fd;
        if (fd>=0) close(fd);
    }
    if (r==0) { result.image_fd=opened(result.code_fd,result.command.executable,0,0,0555,LABEL("aetheri_store_code_t"),0); if (result.image_fd<0) r=result.image_fd; }
    if (r==0) r=ae_image_verify(result.image_fd,result.command.sha256,&result.image);
    int fds[]={root,store,container,manifest};
    for (size_t n=0;n<sizeof fds/sizeof fds[0];++n) if (fds[n]>=0 && close(fds[n])!=0 && r==0) r=-errno;
    free(input);
    if (r==0) { *output=result; memcpy(publisher,fixture.publisher,16); } else ae_execution_sources_clear(&result);
    return r;
}

int ae_execution_sources_open(const struct ae_execution_profile *p,struct ae_execution_sources *output)
{
    if (output==NULL || output->code_fd>=0 || output->data_fd>=0 || output->image_fd>=0) return -EINVAL;
    struct ae_execution_sources result=AE_EXECUTION_SOURCES_INIT;
    uint8_t publisher[16]; struct ae_data_enrollment data;
    int r=ae_execution_code_open(p,&result,publisher);
    if (r==0) r=ae_execution_data_open(p,publisher,&result.data_fd,&data);
    if (r==0) *output=result; else ae_execution_sources_clear(&result);
    return r;
}
