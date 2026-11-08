/* Read-only USB root preflight for the explicit development initrd.
 * Runtime diskseq/UUID observations are not publisher or clone authentication. */
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/statvfs.h>
#include <sys/sysmacros.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <linux/fs.h>
#include <linux/magic.h>

#define CONFIG "/etc/aetheri-usb-root"
#define RUNTIME "/run/aetheri-initrd"
struct profile {char nonce[33],partition[37],filesystem[37],kernel[97];};
struct observation {dev_t partition,disk;uint64_t sequence;ino_t parent;char boot[37];};

static bool read_text_at(int parent,const char *name,char *out,size_t capacity) {
    if(capacity<2)return false;
    int fd=openat(parent,name,O_RDONLY|O_CLOEXEC|O_NOFOLLOW);
    if(fd<0)return false;
    size_t used=0;bool ok=true;
    while(used<capacity-1){
        ssize_t n=read(fd,out+used,capacity-1-used);
        if(n<0 && errno==EINTR)continue;
        if(n<0){ok=false;break;}
        if(!n)break;
        used+=(size_t)n;
    }
    char extra;ssize_t n;do{n=read(fd,&extra,1);}while(n<0 && errno==EINTR);
    out[used]=0;
    if(n!=0 || memchr(out,0,used))ok=false;
    if(close(fd))ok=false;
    return ok;
}
static bool hex(char c) {return (c>='0' && c<='9') || (c>='a' && c<='f');}
static bool uuid_text(const char *s) {
    if(strlen(s)!=36)return false;
    for(size_t i=0;i<36;++i)if(i==8 || i==13 || i==18 || i==23){if(s[i]!='-')return false;}else if(!hex(s[i]))return false;
    return true;
}
static bool parse_profile(char *text,struct profile *out) {
    char *lines[5],*at=text;
    for(size_t i=0;i<5;++i){char *end=strchr(at,'\n');if(!end)return false;*end=0;lines[i]=at;at=end+1;}
    if(*at || strcmp(lines[0],"AETHERI-USB1") || strlen(lines[1])!=32 || !uuid_text(lines[2]) || !uuid_text(lines[3]))return false;
    for(size_t i=0;i<32;++i)if(!hex(lines[1][i]))return false;
    size_t n=strlen(lines[4]);if(!n || n>=sizeof(out->kernel))return false;
    for(size_t i=0;i<n;++i){char c=lines[4][i];if(!((c>='0' && c<='9') || (c>='a' && c<='z') || (c>='A' && c<='Z') || c=='.' || c=='_' || c=='-' || c=='+'))return false;}
    memcpy(out->nonce,lines[1],33);memcpy(out->partition,lines[2],37);memcpy(out->filesystem,lines[3],37);memcpy(out->kernel,lines[4],n+1);
    return true;
}
static bool field(const char *text,const char *line) {
    size_t n=strlen(line);
    for(const char *p=text;*p;){const char *end=strchr(p,'\n');if(!end)return false;if((size_t)(end-p)==n && !memcmp(p,line,n))return true;p=end+1;}
    return false;
}
static bool component(const char **at,char separator,unsigned *out) {
    const char *p=*at;unsigned value=0;
    if(*p<'0' || *p>'9' || (*p=='0' && p[1]>='0' && p[1]<='9'))return false;
    do{unsigned d=(unsigned)(*p-'0');if(value>(UINT_MAX-d)/10)return false;value=value*10+d;++p;}while(*p>='0' && *p<='9');
    if(*p!=separator)return false;
    *at=p+1;*out=value;return true;
}
static bool device_text(const char *text,dev_t *out) {
    unsigned a=0,b=0;const char *at=text;
    if(!component(&at,':',&a) || !component(&at,'\n',&b) || *at)return false;
    dev_t value=makedev(a,b);if(major(value)!=a || minor(value)!=b)return false;
    *out=value;return true;
}
static bool unique_partition(const char *identifier,dev_t expected) {
    DIR *all=opendir("/sys/class/block");if(!all)return false;
    unsigned count=0,total=0;bool ok=true;char wanted[64];
    int n=snprintf(wanted,sizeof(wanted),"PARTUUID=%s",identifier);
    if(n<0 || (size_t)n>=sizeof(wanted)){closedir(all);return false;}
    struct dirent *entry;
    errno=0;
    while((entry=readdir(all))){
        if(!strcmp(entry->d_name,".") || !strcmp(entry->d_name,".."))continue;
        if(++total>256){ok=false;break;}
        int fd=openat(dirfd(all),entry->d_name,O_RDONLY|O_DIRECTORY|O_CLOEXEC);
        char text[4096];
        if(fd<0){ok=false;break;}
        bool read=read_text_at(fd,"uevent",text,sizeof(text));
        if(!read)ok=false;
        else if(field(text,wanted)){
            char device[64];dev_t found=0;
            ++count;
            if(!read_text_at(fd,"dev",device,sizeof(device)) || !device_text(device,&found) || found!=expected)ok=false;
        }
        if(close(fd))ok=false;
        if(!ok)break;
        errno=0;
    }
    if(errno)ok=false;
    if(closedir(all))ok=false;
    return ok && count==1;
}
static bool usb_ancestor(int parent) {
    int usb=open("/sys/bus/usb",O_RDONLY|O_DIRECTORY|O_CLOEXEC),current=fcntl(parent,F_DUPFD_CLOEXEC,3);
    struct stat bus;bool found=false;
    if(usb<0 || current<0 || fstat(usb,&bus))goto done;
    for(unsigned depth=0;depth<32;++depth){
        char text[4096];
        if(read_text_at(current,"uevent",text,sizeof(text)) && field(text,"DEVTYPE=usb_device")){
            int subsystem=openat(current,"subsystem",O_RDONLY|O_DIRECTORY|O_CLOEXEC);struct stat s;
            if(subsystem>=0){found=fstat(subsystem,&s)==0 && s.st_dev==bus.st_dev && s.st_ino==bus.st_ino;if(close(subsystem))found=false;}
            break;
        }
        struct stat old,next;
        int above=openat(current,"..",O_RDONLY|O_DIRECTORY|O_CLOEXEC);
        if(above<0)break;
        bool same=fstat(current,&old) || fstat(above,&next) || (old.st_dev==next.st_dev && old.st_ino==next.st_ino);
        if(close(current)){close(above);current=-1;break;}
        current=above;if(same)break;
    }
done:
    if(current>=0 && close(current))found=false;
    if(usb>=0 && close(usb))found=false;
    return found;
}
static unsigned digit(char c) {return c>='0' && c<='9'?(unsigned)(c-'0'):(unsigned)(c-'a'+10);}
static bool superblock(int fd,const char *uuid) {
    unsigned char bytes[2048];size_t used=0;
    while(used<sizeof(bytes)){
        ssize_t n=pread(fd,bytes+used,sizeof(bytes)-used,(off_t)used);
        if(n<0 && errno==EINTR)continue;
        if(n<=0)return false;
        used+=(size_t)n;
    }
    if(bytes[1080]!=0x53 || bytes[1081]!=0xef)return false;
    size_t at=0;
    for(size_t i=0;i<36;){if(uuid[i]=='-'){++i;continue;}unsigned v=digit(uuid[i])*16+digit(uuid[i+1]);if(bytes[1128+at++]!=v)return false;i+=2;}
    return at==16;
}
/* Every open is read-only. The alias is joined to independently expected kernel
 * partition metadata, bounded whole-system uniqueness, USB ancestry and diskseq. */
static const char *observe(const struct profile *p,struct observation *out) {
    char path[128],text[4096];int fd=-1,sys=-1,parent=-1;const char *error="device-context";
    int n=snprintf(path,sizeof(path),"/dev/disk/by-partuuid/%s",p->partition);
    if(n<0 || (size_t)n>=sizeof(path))return error;
    fd=open(path,O_RDONLY|O_CLOEXEC);struct stat s;
    uint64_t size=0,sequence=0;
    if(fd<0 || fstat(fd,&s) || !S_ISBLK(s.st_mode) || ioctl(fd,BLKGETSIZE64,&size) || size!=256*1024*1024 || ioctl(fd,BLKGETDISKSEQ,&sequence) || !sequence)goto done;
    out->partition=s.st_rdev;out->sequence=sequence;
    n=snprintf(path,sizeof(path),"/sys/dev/block/%u:%u",major(out->partition),minor(out->partition));
    if(n<0 || (size_t)n>=sizeof(path))goto done;
    sys=open(path,O_RDONLY|O_DIRECTORY|O_CLOEXEC);
    if(sys<0 || !read_text_at(sys,"partition",text,sizeof(text)) || strcmp(text,"2\n") || !read_text_at(sys,"start",text,sizeof(text)) || strcmp(text,"262144\n") || !unique_partition(p->partition,out->partition))goto done;
    parent=openat(sys,"..",O_RDONLY|O_DIRECTORY|O_CLOEXEC);
    if(parent<0 || fstat(parent,&s) || !read_text_at(parent,"dev",text,sizeof(text)) || !device_text(text,&out->disk) || !read_text_at(parent,"size",text,sizeof(text)) || strcmp(text,"1048576\n"))goto done;
    out->parent=s.st_ino;
    char wanted[64];n=snprintf(wanted,sizeof(wanted),"%" PRIu64 "\n",sequence);
    if(n<0 || (size_t)n>=sizeof(wanted) || !read_text_at(parent,"diskseq",text,sizeof(text)) || strcmp(text,wanted))goto done;
    error="transport";
    if(!read_text_at(parent,"removable",text,sizeof(text)) || strcmp(text,"1\n") || !usb_ancestor(parent))goto done;
    error="filesystem";if(!superblock(fd,p->filesystem))goto done;
    error="device-drift";
    if(ioctl(fd,BLKGETDISKSEQ,&sequence) || sequence!=out->sequence || !read_text_at(AT_FDCWD,"/proc/sys/kernel/random/boot_id",text,sizeof(text)) || strlen(text)!=37 || text[36]!='\n')goto done;
    text[36]=0;if(!uuid_text(text))goto done;memcpy(out->boot,text,37);error=NULL;
done:
    if(parent>=0 && close(parent))error="close";
    if(sys>=0 && close(sys))error="close";
    if(fd>=0 && close(fd))error="close";
    return error;
}
static bool record(const struct profile *p,const struct observation *o,char *out,size_t size) {
    int n=snprintf(out,size,"AETHERI-USB-OBS1\n%s\n%s\n%u:%u\n%u:%u\n%" PRIu64 "\n%ju\n",p->nonce,o->boot,major(o->partition),minor(o->partition),major(o->disk),minor(o->disk),o->sequence,(uintmax_t)o->parent);
    return n>0 && (size_t)n<size;
}
static int failure(const char *reason) {printf("AETHERI_INITRD_FAIL reason=%s\n",reason);return fflush(stdout)?2:1;}
int main(int argc,char **argv) {
    bool before=argc==2 && !strcmp(argv[1],"--device"),handoff=argc==2 && !strcmp(argv[1],"--handoff");
    bool after=argc==2 && (!strcmp(argv[1],"--mounted") || handoff);
    if(!before && !after)return 2;
    char text[512],saved[512];struct profile p;struct observation o={0};struct utsname kernel;struct statfs fs;
    if(getuid()!=0 || geteuid()!=0 || getppid()!=1 || !read_text_at(AT_FDCWD,"/proc/1/comm",text,sizeof(text)) || strcmp(text,"systemd\n") || statfs("/",&fs) || (fs.f_type!=TMPFS_MAGIC && fs.f_type!=RAMFS_MAGIC) || statfs("/sys",&fs) || fs.f_type!=SYSFS_MAGIC)return failure("supervision");
    if(statfs("/run",&fs) || fs.f_type!=TMPFS_MAGIC)return failure("runtime-filesystem");
    if(!read_text_at(AT_FDCWD,CONFIG,text,sizeof(text)) || !parse_profile(text,&p) || uname(&kernel) || strcmp(kernel.machine,"x86_64") || strcmp(kernel.release,p.kernel))return failure("profile");
    const char *error=observe(&p,&o);if(error)return failure(error);
    if(!record(&p,&o,saved,sizeof(saved)))return failure("record");
    if(before){
        if(mkdir(RUNTIME,0700))return failure("runtime-collision");
        int fd=open(RUNTIME "/observation",O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);
        if(fd<0)return failure("record-open");
        size_t size=strlen(saved),used=0;bool ok=true;
        while(used<size){ssize_t n=write(fd,saved+used,size-used);if(n<0 && errno==EINTR)continue;if(n<=0){ok=false;break;}used+=(size_t)n;}
        if(close(fd))ok=false;
        if(!ok)return failure("record-write");
        printf("AETHERI_INITRD_DEVICE nonce=%s diskseq=%" PRIu64 " boot=%s\n",p.nonce,o.sequence,o.boot);
    }else{
        struct stat s;struct statvfs flags;
        if(lstat(RUNTIME "/observation",&s) || !S_ISREG(s.st_mode) || s.st_uid || s.st_gid || (s.st_mode&07777)!=0600 || s.st_nlink!=1 || !read_text_at(AT_FDCWD,RUNTIME "/observation",text,sizeof(text)) || strcmp(text,saved))return failure("original-observation");
        if(stat("/sysroot",&s) || s.st_dev!=o.partition || statfs("/sysroot",&fs) || fs.f_type!=EXT4_SUPER_MAGIC || statvfs("/sysroot",&flags) || !(flags.f_flag&ST_RDONLY))return failure("mounted-root");
        int n=snprintf(saved,sizeof(saved),"%s\n%s\n",p.nonce,p.kernel);
        if(n<0 || (size_t)n>=sizeof(saved) || !read_text_at(AT_FDCWD,"/sysroot/etc/aetheri-root-proof",text,sizeof(text)) || strcmp(text,saved))return failure("root-profile");
        printf("AETHERI_INITRD_%s nonce=%s usb=1 ext4_ro=1 boot=%s\n",handoff?"HANDOFF":"PASS",p.nonce,o.boot);
    }
    return fflush(stdout)?1:0;
}
