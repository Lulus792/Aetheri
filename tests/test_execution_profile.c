#include "authority/execution_profile.h"
#include "core/digest.h"
#include "core/root.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/openat2.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"execution profile line %d: %s errno=%d\n",__LINE__,#x,errno); exit(1); } } while (0)
static const char *mapped_root;
long __real_syscall(long,...);
long __wrap_syscall(long number,...)
{
    /* Only replace the production fixed anchor with this owned scratch tree.
     * Forward the original openat2 flags/resolution unchanged to the real kernel.
     * No MAC/file/metadata/image or child path check is mocked. */
    CHECK(number==SYS_openat2);
    va_list a; va_start(a,number);
    int base=va_arg(a,int); const char *path=va_arg(a,const char *);
    struct open_how *how=va_arg(a,struct open_how *); size_t size=va_arg(a,size_t); va_end(a);
    if (base==AT_FDCWD && strcmp(path,"/var/lib/aetheri-p0")==0 && mapped_root) path=mapped_root;
    return __real_syscall(number,base,path,how,size);
}
static int descriptors(void)
{
    DIR *d=opendir("/proc/self/fd"); CHECK(d!=NULL); int count=0;
    while (readdir(d)!=NULL) ++count;
    CHECK(closedir(d)==0); return count;
}
static void read_profile(const char *path,struct ae_execution_profile *p)
{
    FILE *file=fopen(path,"rb"); CHECK(file!=NULL); uint8_t bytes[AE_EXECUTION_BYTES+1U];
    CHECK(fread(bytes,1,sizeof bytes,file)==AE_EXECUTION_BYTES && fclose(file)==0);
    CHECK(ae_execution_decode(bytes,AE_EXECUTION_BYTES,p)==0);
}
int main(int argc,char **argv)
{
    CHECK(getuid()==0 && geteuid()==0);
    if (argc==3 && strcmp(argv[1],"observe")==0) {
        int fd=open(argv[2],O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW); CHECK(fd>=0);
        struct ae_root_identity i; CHECK(ae_root_observe(fd,&i)==0 && i.birth_present);
        printf("%lld ",(long long)i.filesystem_type);
        for (size_t n=0;n<8;++n) printf("%02x",i.filesystem_id[n]);
        printf(" %llu %lld %u\n",(unsigned long long)i.inode,(long long)i.birth_seconds,i.birth_nanoseconds);
        CHECK(close(fd)==0); return 0;
    }
    CHECK(argc==6); mapped_root=strcmp(argv[1],"LIVE")==0 ? NULL : argv[1];
    struct ae_execution_profile p; read_profile(argv[2],&p);
    const char *mode=argv[3]; int expected=(int)strtol(argv[4],NULL,10);
    unsigned int repeat=(unsigned int)strtoul(argv[5],NULL,10); CHECK(repeat>=1 && repeat<=128);
    int count=descriptors();
    for (unsigned int n=0;n<repeat;++n) {
        int r;
        if (strcmp(mode,"profile")==0) {
            struct ae_execution_profile got,marker; memset(&marker,0xa5,sizeof marker); got=marker;
            uint8_t digest[32],before[32]; memset(before,0xa5,sizeof before); memcpy(digest,before,32);
            r=ae_execution_profile_load(&got,digest);
            if (r==0) {
                uint8_t a[AE_EXECUTION_BYTES],b[AE_EXECUTION_BYTES],hash[32];
                CHECK(ae_execution_encode(&got,a)==0 && ae_execution_encode(&p,b)==0 && memcmp(a,b,sizeof a)==0);
                CHECK(ae_sha256_bytes(b,sizeof b,hash)==0 && memcmp(digest,hash,32)==0);
            } else CHECK(memcmp(&got,&marker,sizeof got)==0 && memcmp(digest,before,32)==0);
        } else if (strcmp(mode,"starter")==0) {
            int fd=-1; struct ae_image image,marker; memset(&marker,0xa5,sizeof marker); image=marker;
            r=ae_execution_starter_open(&p,&fd,&image);
            if (r==0) { CHECK(fd>=3 && memcmp(image.digest,p.starter_digest,32)==0); CHECK(close(fd)==0); }
            else CHECK(fd==-1 && memcmp(&image,&marker,sizeof image)==0);
        } else {
            CHECK(strcmp(mode,"sources")==0);
            struct ae_execution_sources s=AE_EXECUTION_SOURCES_INIT,marker=s;
            memset(&s.command,0xa5,sizeof s.command); memset(&s.image,0xa5,sizeof s.image); marker=s;
            r=ae_execution_sources_open(&p,&s);
            if (r==0) {
                CHECK(s.code_fd>=3 && s.data_fd>=3 && s.image_fd>=3 && s.code_fd!=s.data_fd && s.data_fd!=s.image_fd && s.code_fd!=s.image_fd);
                CHECK(strcmp(s.command.executable,"bin/probe")==0 && s.command.argc>=1 && strcmp(s.command.arguments[0],"bin/probe")==0);
                CHECK(memcmp(s.command.sha256,s.image.digest,32)==0 && s.command.file_size==s.image.size);
                int fds[]={s.code_fd,s.data_fd,s.image_fd};
                for (size_t m=0;m<3;++m) CHECK((fcntl(fds[m],F_GETFD)&FD_CLOEXEC)!=0);
                struct stat image; CHECK(fstat(s.image_fd,&image)==0 && (uint64_t)image.st_ino==s.image.inode);
                CHECK(ae_execution_sources_open(&p,&s)==-EINVAL);
                ae_execution_sources_clear(&s); ae_execution_sources_clear(&s);
                for (size_t m=0;m<3;++m) CHECK(fcntl(fds[m],F_GETFD)==-1 && errno==EBADF);
            } else CHECK(memcmp(&s,&marker,sizeof s)==0);
        }
        if (r!=expected) fprintf(stderr,"mode=%s expected=%d observed=%d\n",mode,expected,r);
        CHECK(r==expected && descriptors()==count);
    }
    struct ae_execution_sources empty=AE_EXECUTION_SOURCES_INIT;
    CHECK(ae_execution_sources_open(NULL,&empty)==-EINVAL && ae_execution_sources_open(&p,NULL)==-EINVAL);
    CHECK(ae_execution_profile_load(NULL,p.artifact)==-EINVAL && ae_execution_profile_load(&p,NULL)==-EINVAL);
    int fd=-1; struct ae_image image;
    CHECK(ae_execution_starter_open(NULL,&fd,&image)==-EINVAL && ae_execution_starter_open(&p,NULL,&image)==-EINVAL);
    ae_execution_sources_clear(NULL);
    return 0;
}
