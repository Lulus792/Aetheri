/* Disposable RAM-only boot evidence. This is not an installed authority. */
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <linux/magic.h>

static bool text(const char *path,char *out,size_t capacity) {
    int fd=open(path,O_RDONLY|O_CLOEXEC|O_NOFOLLOW);if(fd<0) return false;
    size_t used=0;bool ok=true;
    while(used<capacity-1) {
        ssize_t count=read(fd,out+used,capacity-1-used);
        if(count<0 && errno==EINTR) continue;
        if(count<0) {ok=false;break;}
        if(!count) break;
        used+=(size_t)count;
    }
    char extra;ssize_t count;
    do {count=read(fd,&extra,1);} while(count<0 && errno==EINTR);
    if(count!=0) ok=false;
    out[used]=0;if(close(fd)) ok=false;return ok && !memchr(out,0,used);
}
static bool hex(const char *value,size_t size) {
    for(size_t i=0;i<size;++i) if(!((value[i]>='0' && value[i]<='9') || (value[i]>='a' && value[i]<='f'))) return false;
    return true;
}
static bool filesystem(const char *path,long type) {
    struct statfs s;return statfs(path,&s)==0 && s.f_type==type;
}
static int failure(const char *reason) {printf("AETHERI_BOOT_FAIL reason=%s\n",reason);fflush(stdout);return 1;}
int main(void) {
    char expected[256],command[16384],parent[64],boot[64],group[1024],os[512];
    if(getuid()!=0 || geteuid()!=0 || getppid()!=1) return failure("supervision");
    if(!text("/proc/1/comm",parent,sizeof(parent)) || strcmp(parent,"systemd\n")) return failure("pid1");
    if(!text("/etc/aetheri-boot-proof",expected,sizeof(expected))) return failure("profile");
    size_t n=strlen(expected);if(n<35 || !hex(expected,32) || expected[32]!='\n' || expected[n-1]!='\n') return failure("profile-format");
    expected[32]=0;expected[n-1]=0;
    if(!text("/proc/cmdline",command,sizeof(command))) return failure("cmdline");
    unsigned matches=0;char *save=NULL;
    for(char *p=strtok_r(command," \t\r\n",&save);p;p=strtok_r(NULL," \t\r\n",&save)) {
        static const char prefix[]="aetheri.boot_nonce=";
        if(!strncmp(p,prefix,sizeof(prefix)-1)) {if(strcmp(p+sizeof(prefix)-1,expected)) return failure("nonce");++matches;}
    }
    if(matches!=1) return failure("nonce-count");
    if(!text("/etc/os-release",os,sizeof(os)) || !strstr(os,"\nID=aetheri-boot-probe\n")) return failure("os-release");
    struct utsname u;if(uname(&u) || strcmp(u.machine,"x86_64") || strcmp(u.release,expected+33)) return failure("kernel");
    struct stat s;if(stat("/sys/firmware/efi",&s) || !S_ISDIR(s.st_mode)) return failure("uefi");
    if(!filesystem("/proc",PROC_SUPER_MAGIC) || !filesystem("/sys",SYSFS_MAGIC) ||
       !filesystem("/sys/fs/cgroup",CGROUP2_SUPER_MAGIC) || !filesystem("/run",TMPFS_MAGIC)) return failure("mounts");
    struct statfs root;if(statfs("/",&root) || (root.f_type!=TMPFS_MAGIC && root.f_type!=RAMFS_MAGIC)) return failure("ram-root");
    if(!text("/proc/self/cgroup",group,sizeof(group)) || !strstr(group,"/aetheri-boot.service\n")) return failure("service");
    if(!text("/proc/sys/kernel/random/boot_id",boot,sizeof(boot)) || strlen(boot)!=37) return failure("boot-id");
    boot[36]=0;
    printf("AETHERI_BOOT_PASS nonce=%s kernel=%s arch=%s pid1=systemd uefi=1 ram_root=1 cgroup2=1 boot=%s\n",expected,u.release,u.machine,boot);
    return fflush(stdout)==0?0:1;
}
