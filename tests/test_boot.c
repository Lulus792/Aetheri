#include "linux/boot.h"
#include "core/digest.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/magic.h>
#include <sched.h>
#include <selinux/selinux.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/wait.h>
#include <systemd/sd-id128.h>
#include <unistd.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"boot line %d: %s errno=%d\n",__LINE__,#x,errno); exit(1); } } while (0)
static int read_fault,cache_fault,allocation_fault,open_fault,open_count,mask_fault;
ssize_t __real_read(int,void *,size_t);
ssize_t __wrap_read(int fd,void *buffer,size_t size)
{
    int fault=read_fault; read_fault=0;
    if (fault==1 || fault==6) { errno=fault==6 ? EINTR : EIO; return -1; }
    if (fault==2) return 0;
    if (fault==4) { CHECK(size>=38); memset(buffer,'0',38); return 38; }
    ssize_t n=__real_read(fd,buffer,size);
    if (n>0 && fault==3) ((char *)buffer)[0]='G';
    if (n>=36 && fault==5) { char *text=buffer; text[35]=text[35]=='0' ? '1' : '0'; }
    return n;
}
int __real_sd_id128_get_boot(sd_id128_t *);
int __wrap_sd_id128_get_boot(sd_id128_t *id)
{ int r=__real_sd_id128_get_boot(id); if (r==0 && cache_fault) { --cache_fault; id->bytes[15]^=1; } return r; }
void *__real_calloc(size_t,size_t);
void *__wrap_calloc(size_t a,size_t b)
{ if (allocation_fault) { --allocation_fault; errno=ENOMEM; return NULL; } return __real_calloc(a,b); }
int __real_openat(int,const char *,int,...);
int __wrap_openat(int fd,const char *path,int flags,...)
{
    CHECK(!(flags&O_CREAT) && (flags&O_TMPFILE)!=O_TMPFILE);
    if (open_fault && ++open_count==open_fault) { open_fault=0; errno=EACCES; return -1; }
    return __real_openat(fd,path,flags);
}
int __real_statx(int,const char *,int,unsigned int,struct statx *);
int __wrap_statx(int fd,const char *path,int flags,unsigned int mask,struct statx *st)
{
    int r=__real_statx(fd,path,flags,mask,st);
    if (r==0 && mask_fault) { --mask_fault; st->stx_mask&=~STATX_MNT_ID_UNIQUE; }
    return r;
}
static int descriptors(void)
{ DIR *d=opendir("/proc/self/fd"); CHECK(d); int count=0; while (readdir(d)) ++count; CHECK(closedir(d)==0); return count; }
static void codec(const char *path)
{
    uint8_t input[80],raw[49],hash[32]; FILE *f=fopen(path,"rb");
    CHECK(f && fread(input,1,sizeof input,f)==sizeof input && fgetc(f)==EOF && fclose(f)==0);
    struct ae_boot_context value,marker; memset(&marker,0xa5,sizeof marker); value=marker;
    CHECK(ae_boot_context_decode(input,48,&value)==0 && value.pid_namespace==UINT64_MAX && value.time_namespace==(UINT64_C(1)<<63)+9);
    CHECK(ae_boot_context_encode(&value,raw)==0 && memcmp(raw,input,48)==0 && ae_sha256_bytes(raw,48,hash)==0 && memcmp(hash,input+48,32)==0);
    for (size_t n=0;n<48;++n) { value=marker; CHECK(ae_boot_context_decode(input,n,&value)==-EBADMSG && memcmp(&value,&marker,sizeof value)==0); }
    value=marker; CHECK(ae_boot_context_decode(input,49,&value)==-EBADMSG && memcmp(&value,&marker,sizeof value)==0);
    const size_t offsets[]={0,14,16};
    for (size_t n=0;n<3;++n) { memcpy(raw,input,48); raw[offsets[n]]=255; value=marker;
        CHECK(ae_boot_context_decode(raw,48,&value)==-EBADMSG && memcmp(&value,&marker,sizeof value)==0); }
    for (size_t n=0;n<3;++n) { memcpy(raw,input,48); memset(raw+24+n*8,0,8); value=marker;
        CHECK(ae_boot_context_decode(raw,48,&value)==-EBADMSG && memcmp(&value,&marker,sizeof value)==0); }
    CHECK(ae_boot_context_decode(input,48,&value)==0); value.time_namespace=0; memset(raw,0xa5,sizeof raw);
    CHECK(ae_boot_context_encode(&value,raw)==-EBADMSG); for (size_t n=0;n<sizeof raw;++n) CHECK(raw[n]==0xa5);
    CHECK(ae_boot_context_decode(NULL,0,&value)==-EINVAL && ae_boot_context_encode(NULL,raw)==-EINVAL);
}
static void observations(void)
{
    int baseline=descriptors(); struct ae_boot *v=NULL; struct ae_boot_context context,out,marker; memset(&marker,0xa5,sizeof marker);
    CHECK(ae_boot_capture(NULL)==-EINVAL && ae_boot_observe(NULL,&context)==-EINVAL && ae_boot_close(NULL)==-EINVAL);
    allocation_fault=1; CHECK(ae_boot_capture(&v)==-ENOMEM && !v && !allocation_fault && descriptors()==baseline);
    mask_fault=1; CHECK(ae_boot_capture(&v)==-ENOTSUP && !v && !mask_fault && descriptors()==baseline);
    for (int n=1;n<=8;++n) { open_count=0; open_fault=n; CHECK(ae_boot_capture(&v)==-EACCES && !v && !open_fault && descriptors()==baseline); }
    for (int n=1;n<=6;++n) {
        read_fault=n; int r=ae_boot_capture(&v);
        if (n==6) CHECK(r==0 && ae_boot_close(&v)==0);
        else CHECK(r==(n==1 ? -EIO : n==5 ? -ESTALE : -EBADMSG) && !v);
        CHECK(descriptors()==baseline && !read_fault);
    }
    CHECK(ae_boot_capture(&v)==0 && descriptors()==baseline+5);
    CHECK(ae_boot_capture(&v)==-EINVAL && ae_boot_observe(v,&context)==0);
    sd_id128_t boot; CHECK(sd_id128_get_boot(&boot)==0 && memcmp(context.boot,boot.bytes,16)==0);
    for (int n=0;n<8;++n) { CHECK(ae_boot_observe(v,&out)==0 && memcmp(&out,&context,sizeof out)==0); CHECK(descriptors()==baseline+5); }
    out=marker; CHECK(ae_boot_observe_other(v,context.boot,&out)==-EALREADY && memcmp(&out,&marker,sizeof out)==0);
    uint8_t other[16]; memcpy(other,context.boot,16); other[15]^=1;
    CHECK(ae_boot_observe_other(v,other,&out)==0 && memcmp(&out,&context,sizeof out)==0);
    memset(other,0,16); out=marker; CHECK(ae_boot_observe_other(v,other,&out)==-EBADMSG && memcmp(&out,&marker,sizeof out)==0);
    for (int n=1;n<=4;++n) {
        read_fault=n; out=marker;
        CHECK(ae_boot_observe(v,&out)==(n==1 ? -EIO : -EBADMSG) && memcmp(&out,&marker,sizeof out)==0);
        CHECK(ae_boot_observe(v,&out)==0 && descriptors()==baseline+5);
    }
    pid_t child=fork(); CHECK(child>=0);
    if (!child) { out=marker; CHECK(ae_boot_observe(v,&out)==-ECHILD && memcmp(&out,&marker,sizeof out)==0 && ae_boot_close(&v)==-ECHILD && v); _exit(0); }
    int status; CHECK(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==0);
    cache_fault=1; out=marker; CHECK(ae_boot_observe(v,&out)==-ESTALE && !cache_fault && memcmp(&out,&marker,sizeof out)==0);
    CHECK(ae_boot_observe(v,&out)==-ESTALE && ae_boot_close(&v)==0 && !v && ae_boot_close(&v)==0 && descriptors()==baseline);
    CHECK(ae_boot_capture(&v)==0); read_fault=5; out=marker;
    CHECK(ae_boot_observe(v,&out)==-ESTALE && memcmp(&out,&marker,sizeof out)==0 && ae_boot_observe(v,&out)==-ESTALE);
    CHECK(ae_boot_close(&v)==0 && descriptors()==baseline);
}
static void guard(void)
{
    CHECK(getuid()==0 && geteuid()==0 && security_getenforce()==1);
    struct stat st; CHECK(stat("/sys/firmware/efi",&st)==0 && S_ISDIR(st.st_mode));
    CHECK(lstat("/etc/aetheri-p0-carrier",&st)==0 && S_ISREG(st.st_mode) && st.st_uid==0 && st.st_nlink==1 && !(st.st_mode&0022));
    FILE *f=fopen("/sys/class/dmi/id/product_name","r"); char name[128]={0};
    CHECK(f && fgets(name,sizeof name,f) && fclose(f)==0 && strcmp(name,"Aetheri P0 disposable VM\n")==0);
}
static void namespaces(void)
{
    guard(); const int kinds[]={CLONE_NEWCGROUP,CLONE_NEWPID,CLONE_NEWTIME,CLONE_NEWNS};
    for (size_t n=0;n<5;++n) {
        printf("Private boot namespace case %zu\n",n); fflush(stdout);
        pid_t child=fork(); CHECK(child>=0);
        if (!child) {
            struct ae_boot *v=NULL; struct ae_boot_context out,marker; memset(&marker,0xa5,sizeof marker);
            if (n<4) {
                CHECK(ae_boot_capture(&v)==0 && unshare(kinds[n])==0);
                if (n==3) CHECK(mount(NULL,"/",NULL,MS_REC|MS_PRIVATE,NULL)==0);
                out=marker; int r=ae_boot_observe(v,&out);
                if (r!=-ESTALE) fprintf(stderr,"namespace case %zu returned %d\n",n,r);
                CHECK(r==-ESTALE && memcmp(&out,&marker,sizeof out)==0);
                CHECK(ae_boot_observe(v,&out)==-ESTALE && ae_boot_close(&v)==0);
            } else {
                CHECK(unshare(CLONE_NEWNS)==0 && mount(NULL,"/",NULL,MS_REC|MS_PRIVATE,NULL)==0 && ae_boot_capture(&v)==0 && ae_boot_observe(v,&out)==0);
                char path[]="/tmp/aetheri-boot-XXXXXX",text[38]; sd_id128_t boot; memcpy(boot.bytes,out.boot,16);
                char uuid[SD_ID128_UUID_STRING_MAX]; sd_id128_to_uuid_string(boot,uuid); CHECK(snprintf(text,sizeof text,"%s\n",uuid)==37);
                int fd=mkstemp(path); CHECK(fd>=0 && fchmod(fd,0444)==0 && write(fd,text,37)==37 && close(fd)==0);
                CHECK(mount(path,"/proc/sys/kernel/random/boot_id",NULL,MS_BIND,NULL)==0); out=marker;
                CHECK(ae_boot_observe(v,&out)==-ESTALE && memcmp(&out,&marker,sizeof out)==0);
                struct ae_boot *fresh=NULL; CHECK(ae_boot_capture(&fresh)==-ESTALE && !fresh);
                CHECK(umount("/proc/sys/kernel/random/boot_id")==0 && unlink(path)==0 && ae_boot_observe(v,&out)==-ESTALE && ae_boot_close(&v)==0);
                CHECK(ae_boot_capture(&fresh)==0 && ae_boot_close(&fresh)==0);
            }
            _exit(0);
        }
        int status; CHECK(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==0);
    }
}
int main(int argc,char **argv)
{
    alarm(25);
    if (argc==3 && strcmp(argv[1],"--codec")==0) { codec(argv[2]); return 0; }
    if (argc==2 && strcmp(argv[1],"--namespaces")==0) { namespaces(); puts("Actual private namespace/boot-file substitution rejection passed; no lifecycle mutation."); return 0; }
    CHECK(argc==1); observations(); puts("Raw/cached boot, held proc/self namespace checks, partial-capture cleanup, fork/drift rejection and unchanged outputs passed."); return 0;
}
