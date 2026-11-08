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
struct profile {
    char nonce[33],partition[37],filesystem[37],kernel[97];
    char data_partition[37],data_filesystem[37];
    uint64_t disk_sectors,root_sectors,data_start,data_sectors;
    bool desktop;
};
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
static bool decimal(const char *text,uint64_t *out) {
    uint64_t value=0;if(!*text || (*text=='0' && text[1]))return false;
    for(const char *p=text;*p;++p){if(*p<'0' || *p>'9')return false;unsigned d=(unsigned)(*p-'0');if(value>(UINT64_MAX-d)/10)return false;value=value*10+d;}
    *out=value;return true;
}
static bool parse_profile(char *text,struct profile *out) {
    bool desktop=!strncmp(text,"AETHERI-USB2\n",13);size_t count=desktop?11U:5U;
    char *lines[11],*at=text;struct profile result={.disk_sectors=1048576,.root_sectors=524288,.desktop=desktop};
    for(size_t i=0;i<count;++i){char *end=strchr(at,'\n');if(!end)return false;*end=0;lines[i]=at;at=end+1;}
    if(*at || strcmp(lines[0],desktop?"AETHERI-USB2":"AETHERI-USB1") || strlen(lines[1])!=32 || !uuid_text(lines[2]) || !uuid_text(lines[3]))return false;
    for(size_t i=0;i<32;++i)if(!hex(lines[1][i]))return false;
    size_t n=strlen(lines[4]);if(!n || n>=sizeof(result.kernel))return false;
    for(size_t i=0;i<n;++i){char c=lines[4][i];if(!((c>='0' && c<='9') || (c>='a' && c<='z') || (c>='A' && c<='Z') || c=='.' || c=='_' || c=='-' || c=='+'))return false;}
    memcpy(result.nonce,lines[1],33);memcpy(result.partition,lines[2],37);memcpy(result.filesystem,lines[3],37);memcpy(result.kernel,lines[4],n+1);
    if(desktop){
        if(!decimal(lines[5],&result.disk_sectors) || !decimal(lines[6],&result.root_sectors) || !uuid_text(lines[7]) || !uuid_text(lines[8]) || !strcmp(lines[2],lines[7]) || !strcmp(lines[3],lines[8]) || !decimal(lines[9],&result.data_start) || !decimal(lines[10],&result.data_sectors))return false;
        if(result.disk_sectors<2097152 || result.disk_sectors>67108864 || result.disk_sectors%2048 || result.root_sectors!=1048576 || result.data_start!=1310720 || result.data_sectors<524288 || result.data_sectors%2048 || result.data_sectors>result.disk_sectors-result.data_start)return false;
        uint64_t tail=result.disk_sectors-result.data_start-result.data_sectors;
        if(tail<34 || tail>=2082)return false;
        memcpy(result.data_partition,lines[7],37);memcpy(result.data_filesystem,lines[8],37);
    }
    *out=result;
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
static const char *observe_partition(const struct profile *p,bool data,struct observation *out) {
    const char *partition=data?p->data_partition:p->partition,*filesystem=data?p->data_filesystem:p->filesystem;
    uint64_t sectors=data?p->data_sectors:p->root_sectors,start=data?p->data_start:262144;
    char path[128],text[4096];int fd=-1,sys=-1,parent=-1;const char *error="device-context";
    int n=snprintf(path,sizeof(path),"/dev/disk/by-partuuid/%s",partition);
    if(n<0 || (size_t)n>=sizeof(path))return error;
    fd=open(path,O_RDONLY|O_CLOEXEC);struct stat s;
    uint64_t size=0,sequence=0;
    if(fd<0 || fstat(fd,&s) || !S_ISBLK(s.st_mode) || ioctl(fd,BLKGETSIZE64,&size) || size!=sectors*512 || ioctl(fd,BLKGETDISKSEQ,&sequence) || !sequence)goto done;
    if(data){int readonly=1;if(ioctl(fd,BLKROGET,&readonly) || readonly)goto done;}
    out->partition=s.st_rdev;out->sequence=sequence;
    n=snprintf(path,sizeof(path),"/sys/dev/block/%u:%u",major(out->partition),minor(out->partition));
    if(n<0 || (size_t)n>=sizeof(path))goto done;
    sys=open(path,O_RDONLY|O_DIRECTORY|O_CLOEXEC);
    char wanted[64];n=snprintf(wanted,sizeof(wanted),"%" PRIu64 "\n",start);
    if(n<0 || (size_t)n>=sizeof(wanted) || sys<0 || !read_text_at(sys,"partition",text,sizeof(text)) || strcmp(text,data?"3\n":"2\n") || !read_text_at(sys,"start",text,sizeof(text)) || strcmp(text,wanted) || !unique_partition(partition,out->partition))goto done;
    parent=openat(sys,"..",O_RDONLY|O_DIRECTORY|O_CLOEXEC);
    n=snprintf(wanted,sizeof(wanted),"%" PRIu64 "\n",p->disk_sectors);
    if(n<0 || (size_t)n>=sizeof(wanted) || parent<0 || fstat(parent,&s) || !read_text_at(parent,"dev",text,sizeof(text)) || !device_text(text,&out->disk) || !read_text_at(parent,"size",text,sizeof(text)) || strcmp(text,wanted))goto done;
    out->parent=s.st_ino;
    n=snprintf(wanted,sizeof(wanted),"%" PRIu64 "\n",sequence);
    if(n<0 || (size_t)n>=sizeof(wanted) || !read_text_at(parent,"diskseq",text,sizeof(text)) || strcmp(text,wanted))goto done;
    error="transport";
    if(!read_text_at(parent,"removable",text,sizeof(text)) || strcmp(text,"1\n") || !usb_ancestor(parent))goto done;
    error="filesystem";if(!superblock(fd,filesystem))goto done;
    error="device-drift";
    if(ioctl(fd,BLKGETDISKSEQ,&sequence) || sequence!=out->sequence || !read_text_at(AT_FDCWD,"/proc/sys/kernel/random/boot_id",text,sizeof(text)) || strlen(text)!=37 || text[36]!='\n')goto done;
    text[36]=0;if(!uuid_text(text))goto done;memcpy(out->boot,text,37);error=NULL;
done:
    if(parent>=0 && close(parent))error="close";
    if(sys>=0 && close(sys))error="close";
    if(fd>=0 && close(fd))error="close";
    return error;
}
static const char *observe(const struct profile *p,struct observation *out) {return observe_partition(p,false,out);}
static bool record(const struct profile *p,const struct observation *o,char *out,size_t size) {
    int n;
    if(p->desktop)n=snprintf(out,size,"AETHERI-USB-OBS2\n%s\n%s\n%u:%u\n%u:%u\n%" PRIu64 "\n%ju\n%s\n%s\n%s\n%" PRIu64 "\n%" PRIu64 "\n%s\n%s\n%" PRIu64 "\n%" PRIu64 "\n",p->nonce,o->boot,major(o->partition),minor(o->partition),major(o->disk),minor(o->disk),o->sequence,(uintmax_t)o->parent,p->partition,p->filesystem,p->kernel,p->disk_sectors,p->root_sectors,p->data_partition,p->data_filesystem,p->data_start,p->data_sectors);
    else n=snprintf(out,size,"AETHERI-USB-OBS1\n%s\n%s\n%u:%u\n%u:%u\n%" PRIu64 "\n%ju\n",p->nonce,o->boot,major(o->partition),minor(o->partition),major(o->disk),minor(o->disk),o->sequence,(uintmax_t)o->parent);
    return n>0 && (size_t)n<size;
}
static bool original(const struct profile *p,const struct observation *o) {
    char saved[1024],text[1024];struct stat s;
    return record(p,o,saved,sizeof(saved)) && !lstat(RUNTIME "/observation",&s) && S_ISREG(s.st_mode) && !s.st_uid && !s.st_gid && (s.st_mode&07777)==0600 && s.st_nlink==1 && read_text_at(AT_FDCWD,RUNTIME "/observation",text,sizeof(text)) && !strcmp(text,saved);
}
static bool mounted_root(const char *path,const struct profile *p,const struct observation *o) {
    struct stat s;struct statfs fs;struct statvfs flags;char expected[256],actual[256],name[128];
    if(stat(path,&s) || s.st_dev!=o->partition || statfs(path,&fs) || fs.f_type!=EXT4_SUPER_MAGIC || statvfs(path,&flags) || !(flags.f_flag&ST_RDONLY))return false;
    int n=snprintf(name,sizeof(name),"%s/etc/aetheri-root-proof",!strcmp(path,"/")?"":path);
    int m=snprintf(expected,sizeof(expected),"%s\n%s\n",p->nonce,p->kernel);
    if(n<=0 || (size_t)n>=sizeof(name) || m<=0 || (size_t)m>=sizeof(expected) || !read_text_at(AT_FDCWD,name,actual,sizeof(actual)) || strcmp(expected,actual))return false;
    if(p->desktop){
        char configuration[1024],original_frame[1024],candidate_frame[1024];struct profile selected;
        n=snprintf(name,sizeof(name),"%s/etc/aetheri-usb-root",!strcmp(path,"/")?"":path);
        if(n<=0 || (size_t)n>=sizeof(name) || !read_text_at(AT_FDCWD,name,configuration,sizeof(configuration)) || !parse_profile(configuration,&selected) || !selected.desktop || !record(p,o,original_frame,sizeof(original_frame)) || !record(&selected,o,candidate_frame,sizeof(candidate_frame)) || strcmp(original_frame,candidate_frame))return false;
    }
    return true;
}
static bool same_parent(const struct observation *root,const struct observation *data) {
    return root->partition!=data->partition && root->disk==data->disk && root->sequence==data->sequence && root->parent==data->parent && !strcmp(root->boot,data->boot);
}
static const char *failure_prefix="AETHERI_INITRD";
static int failure(const char *reason) {printf("%s_FAIL reason=%s\n",failure_prefix,reason);return fflush(stdout)?2:1;}
int main(int argc,char **argv) {
    bool before=argc==2 && !strcmp(argv[1],"--device"),handoff=argc==2 && !strcmp(argv[1],"--handoff");
    bool after=argc==2 && (!strcmp(argv[1],"--mounted") || handoff);
    bool data=argc==2 && !strcmp(argv[1],"--data"),data_mounted=argc==2 && !strcmp(argv[1],"--data-mounted");
    bool real=data || data_mounted;if(real)failure_prefix="AETHERI_USB_DATA";
    if(!before && !after && !real)return 2;
    char text[1024],saved[1024];struct profile p;struct observation o={0};struct utsname kernel;struct statfs fs;
    if(getuid()!=0 || geteuid()!=0 || getppid()!=1 || !read_text_at(AT_FDCWD,"/proc/1/comm",text,sizeof(text)) || strcmp(text,"systemd\n") || statfs("/",&fs) || (real?fs.f_type!=EXT4_SUPER_MAGIC:(fs.f_type!=TMPFS_MAGIC && fs.f_type!=RAMFS_MAGIC)) || statfs("/sys",&fs) || fs.f_type!=SYSFS_MAGIC)return failure("supervision");
    if(statfs("/run",&fs) || fs.f_type!=TMPFS_MAGIC)return failure("runtime-filesystem");
    if(!read_text_at(AT_FDCWD,CONFIG,text,sizeof(text)) || !parse_profile(text,&p) || uname(&kernel) || strcmp(kernel.machine,"x86_64") || strcmp(kernel.release,p.kernel))return failure("profile");
    const char *error=observe(&p,&o);if(error)return failure(error);
    if(!record(&p,&o,saved,sizeof(saved)))return failure("record");
    if(real){
        struct observation selected={0};struct stat v,h;struct statvfs flags;
        if(!p.desktop || !original(&p,&o) || !mounted_root("/",&p,&o))return failure("original-root");
        error=observe_partition(&p,true,&selected);if(error)return failure(error);
        if(!same_parent(&o,&selected))return failure("data-parent");
        if(data){if(stat("/var",&v) || v.st_dev!=o.partition)return failure("already-mounted");}
        else if(stat("/var",&v) || stat("/home",&h) || v.st_dev!=selected.partition || h.st_dev!=selected.partition || statfs("/var",&fs) || fs.f_type!=EXT4_SUPER_MAGIC || statvfs("/var",&flags) || (flags.f_flag&ST_RDONLY) || !(flags.f_flag&ST_NOSUID) || !(flags.f_flag&ST_NODEV) || statvfs("/home",&flags) || (flags.f_flag&ST_RDONLY) || !(flags.f_flag&ST_NOSUID) || !(flags.f_flag&ST_NODEV))return failure("data-mount");
        printf("AETHERI_USB_DATA_%s nonce=%s same_disk=1 root_ro=1 data_rw=%u boot=%s\n",data?"PASS":"MOUNTED",p.nonce,data_mounted?1U:0U,o.boot);
        return fflush(stdout)?1:0;
    }
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
        if(!original(&p,&o))return failure("original-observation");
        if(!mounted_root("/sysroot",&p,&o))return failure("mounted-root");
        printf("AETHERI_INITRD_%s nonce=%s usb=1 ext4_ro=1 boot=%s\n",handoff?"HANDOFF":"PASS",p.nonce,o.boot);
    }
    return fflush(stdout)?1:0;
}
