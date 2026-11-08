/* Device-policy evidence for the separate read-only development image only.
 * Both modes only open the independently selected block device read-only. */
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/statvfs.h>
#include <sys/syscall.h>
#include <sys/sysmacros.h>
#include <unistd.h>
#include <linux/capability.h>
#include <linux/fs.h>
#include <linux/magic.h>

static bool text(const char *path,char *out,size_t capacity) {
    int fd=open(path,O_RDONLY|O_CLOEXEC|O_NOFOLLOW);if(fd<0)return false;
    size_t used=0;bool ok=true;
    while(used<capacity-1){ssize_t n=read(fd,out+used,capacity-1-used);if(n<0 && errno==EINTR)continue;if(n<0){ok=false;break;}if(!n)break;used+=(size_t)n;}
    char extra;ssize_t n;do{n=read(fd,&extra,1);}while(n<0 && errno==EINTR);
    out[used]=0;if(n || memchr(out,0,used))ok=false;if(close(fd))ok=false;return ok;
}
static bool identifier(const char *s,size_t n,bool uuid) {
    if(strlen(s)!=n)return false;
    for(size_t i=0;i<n;++i){if(uuid && (i==8 || i==13 || i==18 || i==23)){if(s[i]!='-')return false;}else if(!((s[i]>='0' && s[i]<='9') || (s[i]>='a' && s[i]<='f')))return false;}
    return true;
}
static int failure(const char *reason) {printf("AETHERI_DEVICE_FAIL reason=%s\n",reason);return fflush(stdout)?2:1;}
int main(int argc,char **argv) {
    bool baseline=argc==2 && !strcmp(argv[1],"--baseline"),denied=argc==2 && !strcmp(argv[1],"--denied");
    if(!baseline && !denied)return 2;
    char profile[256],boot[64],path[128],comm[32];struct statfs fs;struct stat root,node;struct statvfs flags;
    struct __user_cap_header_struct header={.version=_LINUX_CAPABILITY_VERSION_3,.pid=0};struct __user_cap_data_struct caps[2]={{0},{0}};
    if(getuid() || geteuid() || getgid() || getegid() || getppid()!=1 || !text("/proc/1/comm",comm,sizeof(comm)) || strcmp(comm,"systemd\n") || prctl(PR_GET_NO_NEW_PRIVS,0,0,0,0)!=1 || syscall(SYS_capget,&header,caps))return failure("supervision");
    for(size_t i=0;i<2;++i)if(caps[i].effective || caps[i].permitted || caps[i].inheritable)return failure("capabilities");
    if(statfs("/",&fs) || fs.f_type!=EXT4_SUPER_MAGIC || statvfs("/",&flags) || !(flags.f_flag&ST_RDONLY) || stat("/",&root) || statfs("/sys/fs/cgroup",&fs) || fs.f_type!=CGROUP2_SUPER_MAGIC || access("/sys/firmware/efi",F_OK))return failure("image");
    if(!text("/etc/aetheri-device-proof",profile,sizeof(profile)))return failure("profile");
    char *end=strchr(profile,'\n');if(!end)return failure("profile");*end++=0;
    char *last=strchr(end,'\n');if(!last || last[1])return failure("profile");*last=0;
    if(!identifier(profile,32,false) || !identifier(end,36,true))return failure("profile");
    if(!text("/proc/sys/kernel/random/boot_id",boot,sizeof(boot)) || strlen(boot)!=37 || boot[36]!='\n')return failure("boot");
    boot[36]=0;if(!identifier(boot,36,true))return failure("boot");
    int n=snprintf(path,sizeof(path),"/dev/disk/by-partuuid/%s",end);
    if(n<0 || (size_t)n>=sizeof(path) || stat(path,&node) || !S_ISBLK(node.st_mode) || node.st_rdev!=root.st_dev || node.st_uid || !(node.st_mode&S_IRUSR))return failure("block-context");
    int null=open("/dev/null",O_RDWR|O_CLOEXEC|O_NOFOLLOW);struct stat actual;
    if(null<0)return failure("allowed-device");
    bool ok=!fstat(null,&actual) && S_ISCHR(actual.st_mode) && actual.st_rdev==makedev(1,3);if(close(null))ok=false;
    if(!ok)return failure("allowed-context");
    int fd=open(path,O_RDONLY|O_NONBLOCK|O_CLOEXEC);int error=errno;
    if(baseline){uint64_t size=0;ok=fd>=0 && !fstat(fd,&actual) && S_ISBLK(actual.st_mode) && actual.st_rdev==node.st_rdev && !ioctl(fd,BLKGETSIZE64,&size) && size==512*1024*1024;if(fd>=0 && close(fd))ok=false;if(!ok)return failure("baseline-open");}
    else{if(fd>=0){close(fd);return failure("unexpected-block-open");}if(error!=EPERM)return failure("unexpected-denial");}
    printf("AETHERI_DEVICE_PASS nonce=%s mode=%s block_read=%u null_rw=1 cap_zero=1 nnp=1 boot=%s\n",profile,baseline?"baseline":"denied",baseline?1U:0U,boot);
    return fflush(stdout)?1:0;
}
