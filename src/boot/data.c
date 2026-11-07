/* Fixed writable-boot fixture, never a general volume or app authority. */
#include <errno.h>
#include <fcntl.h>
#include <linux/fs.h>
#include <linux/magic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/statvfs.h>
#include <sys/sysmacros.h>
#include <unistd.h>

struct profile { char nonce[33],uuid[37],serial[64]; };
struct record { char nonce[33],boot[37];unsigned count; };
static bool read_fd(int fd,char *out,size_t capacity) {
    ssize_t n;size_t used=0;
    while(used<capacity-1) {
        n=read(fd,out+used,capacity-1-used);
        if(n<0 && errno==EINTR) continue;
        if(n<0) return false;
        if(!n) break;
        used+=(size_t)n;
    }
    char extra;do {n=read(fd,&extra,1);} while(n<0 && errno==EINTR);
    out[used]=0;return n==0 && !memchr(out,0,used);
}
static bool text(const char *path,char *out,size_t size) {
    int fd=open(path,O_RDONLY|O_CLOEXEC|O_NOFOLLOW);if(fd<0) return false;
    bool ok=read_fd(fd,out,size);if(close(fd)) ok=false;return ok;
}
static int fail(const char *reason) {printf("AETHERI_DATA_FAIL reason=%s\n",reason);fflush(stdout);return 1;}
static bool profile(struct profile *p) {
    char data[256],canonical[256],command[16384];int n=0;
    if(!text("/etc/aetheri-data-proof",data,sizeof(data)) || sscanf(data,"%32s\n%36s\n%63s\n%n",p->nonce,p->uuid,p->serial,&n)!=3 || data[n]) return false;
    int wrote=snprintf(canonical,sizeof(canonical),"%s\n%s\n%s\n",p->nonce,p->uuid,p->serial);
    if(wrote<0 || (size_t)wrote>=sizeof(canonical) || strcmp(data,canonical) || strlen(p->nonce)!=32 || strlen(p->uuid)!=36) return false;
    if(!text("/proc/cmdline",command,sizeof(command))) return false;
    char *save=NULL;unsigned matches=0;
    for(char *s=strtok_r(command," \r\n\t",&save);s;s=strtok_r(NULL," \r\n\t",&save)) {
        if(!strncmp(s,"aetheri.boot_nonce=",19)) {if(strcmp(s+19,p->nonce)) return false;++matches;}
    }
    return matches==1;
}
static bool context(const char *unit,uid_t uid) {
    char parent[64],group[2048],expected[128];
    if(getuid()!=uid || geteuid()!=uid || getgid()!=(gid_t)uid || getegid()!=(gid_t)uid || getppid()!=1) return false;
    if(!text("/proc/1/comm",parent,sizeof(parent)) || strcmp(parent,"systemd\n") || !text("/proc/self/cgroup",group,sizeof(group))) return false;
    int n=snprintf(expected,sizeof(expected),"/%s\n",unit);return n>0 && (size_t)n<sizeof(expected) && strstr(group,expected);
}
static bool volume(const struct profile *p,dev_t *device) {
    int fd=open("/dev/vdb",O_RDONLY|O_CLOEXEC|O_NONBLOCK|O_NOFOLLOW);if(fd<0) return false;
    struct stat s={0};unsigned long long bytes=0;int readonly=1;unsigned char super[1024]={0};
    bool ok=fstat(fd,&s)==0 && S_ISBLK(s.st_mode) && ioctl(fd,BLKGETSIZE64,&bytes)==0 && bytes==256ULL*1024*1024 && ioctl(fd,BLKROGET,&readonly)==0 && !readonly;
    ssize_t got=pread(fd,super,sizeof(super),1024);if(got!=(ssize_t)sizeof(super) || super[56]!=0x53 || super[57]!=0xef) ok=false;
    char uuid[37];int n=snprintf(uuid,sizeof(uuid),"%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",super[104],super[105],super[106],super[107],super[108],super[109],super[110],super[111],super[112],super[113],super[114],super[115],super[116],super[117],super[118],super[119]);
    if(n!=36 || strcmp(uuid,p->uuid)) ok=false;
    char path[128],serial[128];
    n=snprintf(path,sizeof(path),"/sys/dev/block/%u:%u/serial",major(s.st_rdev),minor(s.st_rdev));
    if(n<0 || (size_t)n>=sizeof(path) || !text(path,serial,sizeof(serial))) ok=false;
    else {
        serial[strcspn(serial,"\n")]=0;
        if(strcmp(serial,p->serial)) {
            fprintf(stderr,"AETHERI_DATA_DETAIL serial expected=%s observed=%s\n",p->serial,serial);
            ok=false;
        }
    }
    if(close(fd)) ok=false;
    if(ok) *device=s.st_rdev;
    return ok;
}
static bool mounted(dev_t device) {
    struct stat v,h;struct statfs fs;struct statvfs flags;
    return stat("/var",&v)==0 && stat("/home",&h)==0 && v.st_dev==device && h.st_dev==device && statfs("/var",&fs)==0 && fs.f_type==EXT4_SUPER_MAGIC && statvfs("/var",&flags)==0 && !(flags.f_flag&ST_RDONLY);
}
static bool boot_id(char out[37]) {
    char data[64];if(!text("/proc/sys/kernel/random/boot_id",data,sizeof(data)) || strlen(data)!=37) return false;
    memcpy(out,data,36);out[36]=0;return true;
}
static bool record_read(int parent,struct record *r,struct stat *s,bool *missing) {
    int fd=openat(parent,"boot-persistence.txt",O_RDONLY|O_CLOEXEC|O_NOFOLLOW);*missing=fd<0 && errno==ENOENT;
    if(fd<0) return *missing;
    char bytes[160],canonical[160];int n=0;
    bool ok=fstat(fd,s)==0 && S_ISREG(s->st_mode) && s->st_nlink==1 && s->st_uid==1000 && s->st_gid==1000 && (s->st_mode&0777)==0600 && read_fd(fd,bytes,sizeof(bytes));
    if(close(fd)) ok=false;
    if(!ok || sscanf(bytes,"AETHERI-DATA1 %32s %6u %36s\n%n",r->nonce,&r->count,r->boot,&n)!=3 || bytes[n] || !r->count || r->count>=1000000 || strlen(r->nonce)!=32 || strlen(r->boot)!=36) return false;
    n=snprintf(canonical,sizeof(canonical),"AETHERI-DATA1 %s %u %s\n",r->nonce,r->count,r->boot);
    return n>0 && (size_t)n<sizeof(canonical) && !strcmp(bytes,canonical);
}
static bool write_all(int fd,const char *bytes,size_t size) {
    while(size) {ssize_t n=write(fd,bytes,size);if(n<0 && errno==EINTR) continue;if(n<=0) return false;bytes+=(size_t)n;size-=(size_t)n;}return true;
}
static int user(const struct profile *p) {
    if(!context("aetheri-data-user.service",1000)) return fail("user-context");
    char status[8192],boot[37];
    if(!text("/proc/self/status",status,sizeof(status)) || !strstr(status,"\nCapEff:\t0000000000000000\n") || !boot_id(boot)) return fail("user-capabilities");
    const char *runtime=getenv("XDG_RUNTIME_DIR");struct stat st;struct statfs fs;
    if(!runtime || stat(runtime,&st) || st.st_uid!=1000 || (st.st_mode&0777)!=0700 || statfs(runtime,&fs) || fs.f_type!=TMPFS_MAGIC) return fail("runtime");
    if(statfs("/tmp",&fs) || fs.f_type!=TMPFS_MAGIC || stat("/tmp",&st) || (st.st_mode&07777)!=01777) return fail("tmp");
    int temporary=open("/tmp/aetheri-data-ephemeral",O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);
    if(temporary<0) return fail("tmp-not-fresh");
    if(close(temporary)) return fail("tmp-close");
    int parent=open("/home/aetheri/Documents",O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW);if(parent<0) return fail("documents");
    if(fstat(parent,&st) || st.st_uid!=1000 || st.st_gid!=1000 || (st.st_mode&0777)!=0700) {close(parent);return fail("documents-owner");}
    struct record old={0};struct stat original={0};bool missing=false;
    if(!record_read(parent,&old,&original,&missing) || (!missing && (strcmp(old.nonce,p->nonce) || !strcmp(old.boot,boot)))) {close(parent);return fail("record");}
    unsigned count=missing?1:old.count+1;char data[160];int length=snprintf(data,sizeof(data),"AETHERI-DATA1 %s %u %s\n",p->nonce,count,boot);
    if(length<0 || (size_t)length>=sizeof(data)) {close(parent);return fail("record-size");}
    int fd=openat(parent,"boot-persistence.next",O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);
    if(fd<0) {close(parent);return fail("pending-record");}
    bool ok=write_all(fd,data,(size_t)length) && fsync(fd)==0;if(close(fd)) ok=false;
    if(ok && missing) ok=linkat(parent,"boot-persistence.next",parent,"boot-persistence.txt",0)==0 && unlinkat(parent,"boot-persistence.next",0)==0;
    else if(ok) {struct stat named;ok=fstatat(parent,"boot-persistence.txt",&named,AT_SYMLINK_NOFOLLOW)==0 && named.st_ino==original.st_ino && named.st_dev==original.st_dev;
        if(ok) ok=renameat(parent,"boot-persistence.next",parent,"boot-persistence.txt")==0;}
    if(ok) ok=fsync(parent)==0;
    if(close(parent)) ok=false;
    if(!ok) return fail("record-uncertain");
    printf("AETHERI_DATA_USER nonce=%s counter=%u user=1000 cap_zero=1 tmp=1 runtime=1 boot=%s\n",p->nonce,count,boot);return fflush(stdout)==0?0:1;
}
static int finish(const struct profile *p) {
    dev_t device=0;if(!context("aetheri-data-finish.service",0) || !volume(p,&device) || !mounted(device)) return fail("finish-context");
    int parent=open("/home/aetheri/Documents",O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW);if(parent<0) return fail("finish-documents");
    struct record r={0};struct stat s;bool missing=false;char boot[37];
    bool ok=record_read(parent,&r,&s,&missing) && !missing && !strcmp(r.nonce,p->nonce) && boot_id(boot) && !strcmp(r.boot,boot);
    if(close(parent)) ok=false;
    int var=open("/var",O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW);if(var<0) return fail("finish-var");
    if(syncfs(var)) ok=false;
    if(close(var)) ok=false;
    if(!ok) return fail("finish-record");
    if(umount2("/home",0) || umount2("/var",0) || umount2("/tmp",0)) return fail("unmount");
    printf("AETHERI_DATA_PASS nonce=%s counter=%u user=1000 cap_zero=1 persisted=1 unmounted=1 boot=%s\n",p->nonce,r.count,boot);return fflush(stdout)==0?0:1;
}
int main(int argc,char **argv) {
    if(argc!=2) return fail("arguments");
    struct profile p={0};
    if(!profile(&p)) return fail("profile");
    if(!strcmp(argv[1],"--check")) {dev_t device=0;
        if(!context("aetheri-data-check.service",0) || !volume(&p,&device)) return fail("volume-context");
        printf("AETHERI_DATA_CHECK nonce=%s\n",p.nonce);return fflush(stdout)==0?0:1;}
    if(!strcmp(argv[1],"--user")) return user(&p);
    if(!strcmp(argv[1],"--finish")) return finish(&p);
    return fail("arguments");
}
