#include "core/publish.h"
#include "core/fixture.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"publish line %d: %s\n",__LINE__,#x); exit(1); } } while (0)
static const char *fault="none";
static int phase;
static ino_t parent_inode,store_inode,payload_inode,container_inode;
static unsigned int file_syncs,payload_syncs,parent_syncs,store_syncs,container_syncs,renames;

static unsigned int descriptor_count(void)
{
    DIR *stream=opendir("/proc/self/fd");
    CHECK(stream!=NULL);
    unsigned int count=0;
    while (readdir(stream)!=NULL) ++count;
    CHECK(closedir(stream)==0);
    return count;
}
int __real_fsync(int);
int __wrap_fsync(int fd)
{
    struct stat info;
    CHECK(fstat(fd,&info)==0);
    if (S_ISREG(info.st_mode)) ++file_syncs;
    if (info.st_ino==parent_inode) ++parent_syncs;
    if (info.st_ino==store_inode) ++store_syncs;
    if (info.st_ino==payload_inode) ++payload_syncs;
    if (info.st_ino==container_inode) ++container_syncs;
    int selected=(strcmp(fault,"file-sync")==0 && S_ISREG(info.st_mode)) ||
        (strcmp(fault,"payload-sync")==0 && info.st_ino==payload_inode && phase>=1) ||
        (strcmp(fault,"old-parent-sync")==0 && info.st_ino==parent_inode && phase==1) ||
        (strcmp(fault,"container-sync")==0 && info.st_ino==container_inode && phase>=1) ||
        (strcmp(fault,"container-parent-sync")==0 && info.st_ino==parent_inode && phase==2) ||
        (strcmp(fault,"store-sync")==0 && info.st_ino==store_inode);
    if (selected) { errno=EIO; return -1; }
    return __real_fsync(fd);
}
int __real_renameat2(int,const char *,int,const char *,unsigned int);
int __wrap_renameat2(int oldfd,const char *old,int newfd,const char *next,unsigned int flags)
{
    CHECK(flags==RENAME_NOREPLACE);
    ++renames;
    int inner=strcmp(next,"payload")==0;
    if ((inner && strcmp(fault,"inner-before")==0) || (!inner && strcmp(fault,"outer-before")==0)) {
        errno=EIO; return -1;
    }
    int result=__real_renameat2(oldfd,old,newfd,next,flags);
    if (result==0) phase=inner ? 1 : 2;
    if (result==0 && ((inner && strcmp(fault,"inner-after")==0) || (!inner && strcmp(fault,"outer-after")==0))) {
        errno=EIO; return -1;
    }
    if (result==0 && ((inner && strcmp(fault,"inner-exit")==0) || (!inner && strcmp(fault,"outer-exit")==0))) _exit(73);
    return result;
}
ssize_t __real_write(int,const void *,size_t);
ssize_t __wrap_write(int fd,const void *bytes,size_t size)
{
    if (strcmp(fault,"write-eintr")==0) { fault="none"; errno=EINTR; return -1; }
    if (strcmp(fault,"write-short")==0 && size>3) size=3;
    if (strcmp(fault,"write-zero")==0) return 0;
    if (strcmp(fault,"write-io")==0) { errno=EIO; return -1; }
    return __real_write(fd,bytes,size);
}
int __real_close(int);
int __wrap_close(int fd)
{
    int writer=strcmp(fault,"writer-close")==0 && (fcntl(fd,F_GETFL)&O_ACCMODE)==O_RDWR;
    int result=__real_close(fd);
    if (writer) { fault="none"; errno=EIO; return -1; }
    return result;
}
static int open_directory(int parent,const char *name)
{
    int fd=openat(parent,name,O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    CHECK(fd>=0);
    return fd;
}
static ino_t inode(int fd)
{
    struct stat info;
    CHECK(fstat(fd,&info)==0);
    return info.st_ino;
}
static uint8_t nibble(char c)
{
    if (c>='0' && c<='9') return (uint8_t)(c-'0');
    CHECK(c>='a' && c<='f');
    return (uint8_t)(c-'a'+10);
}
int main(int argc,char **argv)
{
    CHECK(argc==7 && strlen(argv[3])==64);
    uint8_t artifact[32],*bytes=malloc(AE_FIXTURE_MAX_BYTES+1U);
    CHECK(bytes!=NULL);
    for (size_t n=0;n<32;++n) artifact[n]=(uint8_t)((nibble(argv[3][n*2U])<<4)|nibble(argv[3][n*2U+1U]));
    FILE *input=fopen(argv[2],"rb");
    CHECK(input!=NULL);
    size_t size=fread(bytes,1,AE_FIXTURE_MAX_BYTES+1U,input);
    CHECK(!ferror(input) && fclose(input)==0);
    int root=open_directory(AT_FDCWD,argv[1]);
    int parent=open_directory(root,"slot"),store=open_directory(root,"store");
    int stored=strcmp(argv[4],"stored")==0;
    int nested=stored || strcmp(argv[4],"nested")==0;
    int container=open_directory(stored ? store : parent,stored ? argv[3] : "publication");
    int payload=open_directory(nested ? container : parent,nested ? "payload" : "staging");
    parent_inode=inode(parent); store_inode=inode(store); payload_inode=inode(payload); container_inode=inode(container);
    phase=stored ? 2 : nested ? 1 : 0;
    struct ae_fixture_publication publication={parent,parent,store,payload,container,"staging","publication"};
    if (strcmp(argv[6],"bad-name")==0) publication.container_name="../publication";
    if (strcmp(argv[6],"rebound-stage")==0) {
        CHECK(renameat(parent,"staging",parent,"detached")==0 && mkdirat(parent,"staging",0700)==0);
    }
    if (strcmp(argv[6],"rebound-container")==0) {
        CHECK(renameat(parent,"publication",parent,"detached")==0 && mkdirat(parent,"publication",0700)==0);
    }
    if (strcmp(argv[6],"copied-final")==0) {
        CHECK(stored && renameat(store,argv[3],root,"detached-store")==0);
        CHECK(renameat(root,"copied-store",store,argv[3])==0);
    }
    unsigned int descriptors=descriptor_count();
    fault=argv[5];
    int result=ae_fixture_publish_namespace(&publication,bytes,size,artifact);
    fault="none";
    CHECK(descriptor_count()==descriptors);
    CHECK(inode(payload)==payload_inode && inode(container)==container_inode);
    CHECK(lseek(parent,0,SEEK_CUR)==0 && lseek(store,0,SEEK_CUR)==0 &&
          lseek(payload,0,SEEK_CUR)==0 && lseek(container,0,SEEK_CUR)==0);
    CHECK(close(root)==0 && close(parent)==0 && close(store)==0 && close(payload)==0 && close(container)==0);
    free(bytes);
    printf("%d %u %u %u %u %u %u\n",result,file_syncs,payload_syncs,parent_syncs,store_syncs,container_syncs,renames);
    return 0;
}
