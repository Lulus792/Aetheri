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
#ifdef AETHERI_ROOT_PROBE
#include <sys/statvfs.h>
#include <sys/sysmacros.h>
#include <time.h>
#ifndef AETHERI_ROOT_DIAGNOSTIC_SECONDS
#define AETHERI_ROOT_DIAGNOSTIC_SECONDS 0
#endif
_Static_assert(AETHERI_ROOT_DIAGNOSTIC_SECONDS>=0 && AETHERI_ROOT_DIAGNOSTIC_SECONDS<=60,
    "root failure display must be bounded");
#define RESULT "AETHERI_ROOT"
#define PROFILE "/etc/aetheri-root-proof"
#define OS_ID "\nID=aetheri-root-probe\n"
#define SERVICE "/aetheri-root.service\n"
#else
#define RESULT "AETHERI_BOOT"
#define PROFILE "/etc/aetheri-boot-proof"
#define OS_ID "\nID=aetheri-boot-probe\n"
#define SERVICE "/aetheri-boot.service\n"
#endif

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
#ifdef AETHERI_ROOT_PROBE
/* Failure-only observations. These are diagnostics, never a substitute for the
   original checks, volume admission or a recovery/installation receipt. Escape
   all kernel text so it cannot inject another result line or terminal command. */
static void diagnostic_value(const char *key,const char *value) {
    printf("AETHERI_ROOT_DETAIL %s=",key);
    putchar('"');
    for(size_t i=0;value[i];++i) {
        unsigned char c=(unsigned char)value[i];
        if(c>=32 && c<=126 && c!='"' && c!='\\') putchar((int)c);
        else printf("\\x%02x",(unsigned)c);
    }
    puts("\"");
}
static void diagnostic_text(const char *key,const char *path) {
    char value[1024];
    if(text(path,value,sizeof(value))) diagnostic_value(key,value);
    else printf("AETHERI_ROOT_DETAIL %s=unavailable\n",key);
}
static void diagnostic_command_line(void) {
    char command[16384],expected[256];
    if(!text("/proc/cmdline",command,sizeof(command))) {
        puts("AETHERI_ROOT_DETAIL command_line=unavailable");return;
    }
    /* Count only fixed public profile keys. Never print arbitrary arguments:
       a future boot profile may contain credentials or personal information. */
    size_t bytes=strlen(command);unsigned nonces=0,matching=0,roots=0;
    bool profile=text(PROFILE,expected,sizeof(expected)) && strlen(expected)>=33 &&
        hex(expected,32) && expected[32]=='\n';
    if(profile) expected[32]=0;
    char *save=NULL;
    for(char *p=strtok_r(command," \t\r\n",&save);p;p=strtok_r(NULL," \t\r\n",&save)) {
        static const char nonce_prefix[]="aetheri.boot_nonce=";
        static const char root_prefix[]="root=PARTUUID=";
        if(!strncmp(p,nonce_prefix,sizeof(nonce_prefix)-1)) {
            ++nonces;if(profile && !strcmp(p+sizeof(nonce_prefix)-1,expected)) ++matching;
        }
        if(!strncmp(p,root_prefix,sizeof(root_prefix)-1)) ++roots;
    }
    printf("AETHERI_ROOT_DETAIL cmdline_bytes=%zu nonce_tokens=%u matching_nonce_tokens=%u root_partuuid_tokens=%u profile_readable=%u\n",
        bytes,nonces,matching,roots,profile?1u:0u);
}
static void root_diagnostic(void) {
    diagnostic_command_line();
    printf("AETHERI_ROOT_DETAIL uid=%lu euid=%lu ppid=%ld\n",
        (unsigned long)getuid(),(unsigned long)geteuid(),(long)getppid());
    struct utsname u;
    if(!uname(&u)) {diagnostic_value("kernel",u.release);diagnostic_value("architecture",u.machine);}
    else puts("AETHERI_ROOT_DETAIL uname=unavailable");
    diagnostic_text("expected_profile",PROFILE);
    diagnostic_text("pid1","/proc/1/comm");
    diagnostic_text("cgroup","/proc/self/cgroup");
    diagnostic_text("consoles","/sys/class/tty/console/active");
    static const char *const paths[]={"/","/proc","/sys","/sys/fs/cgroup","/run"};
    for(size_t i=0;i<sizeof(paths)/sizeof(paths[0]);++i) {
        struct statfs fs;
        if(!statfs(paths[i],&fs)) printf("AETHERI_ROOT_DETAIL fs[%s]=0x%lx\n",paths[i],(unsigned long)fs.f_type);
        else printf("AETHERI_ROOT_DETAIL fs[%s]=unavailable\n",paths[i]);
    }
    struct statvfs flags;
    if(!statvfs("/",&flags)) printf("AETHERI_ROOT_DETAIL root_readonly=%u\n",(flags.f_flag&ST_RDONLY)?1u:0u);
    struct stat info;
    printf("AETHERI_ROOT_DETAIL efi_directory=%u\n",!stat("/sys/firmware/efi",&info) && S_ISDIR(info.st_mode)?1u:0u);
    if(!stat("/",&info)) {
        char path[128];
        int n=snprintf(path,sizeof(path),"/sys/dev/block/%u:%u/uevent",major(info.st_dev),minor(info.st_dev));
        printf("AETHERI_ROOT_DETAIL root_device=%u:%u\n",major(info.st_dev),minor(info.st_dev));
        if(n>0 && (size_t)n<sizeof(path)) diagnostic_text("root_uevent",path);
    }
}
static void root_failure_display(const char *reason) {
    if(AETHERI_ROOT_DIAGNOSTIC_SECONDS==0) return;
    struct timespec deadline;
    if(clock_gettime(CLOCK_MONOTONIC,&deadline)) return;
    for(unsigned remaining=AETHERI_ROOT_DIAGNOSTIC_SECONDS;remaining;) {
        printf("AETHERI_ROOT_DETAIL BOOT BLOCKED reason=%s remaining=%u\n",reason,remaining);
        fflush(stdout);
        unsigned step=remaining<5?remaining:5;deadline.tv_sec+=(time_t)step;
        int result;
        do {result=clock_nanosleep(CLOCK_MONOTONIC,TIMER_ABSTIME,&deadline,NULL);} while(result==EINTR);
        if(result) break;
        remaining-=step;
    }
}
#endif
static int failure(const char *reason) {
    printf(RESULT "_FAIL reason=%s\n",reason);
#ifdef AETHERI_ROOT_PROBE
    root_diagnostic();
    fflush(stdout);root_failure_display(reason);
#endif
    fflush(stdout);return 1;
}
int main(void) {
    char expected[256],command[16384],parent[64],boot[64],group[1024],os[512];
    if(getuid()!=0 || geteuid()!=0 || getppid()!=1) return failure("supervision");
    if(!text("/proc/1/comm",parent,sizeof(parent)) || strcmp(parent,"systemd\n")) return failure("pid1");
    if(!text(PROFILE,expected,sizeof(expected))) return failure("profile");
    size_t n=strlen(expected);if(n<35 || !hex(expected,32) || expected[32]!='\n' || expected[n-1]!='\n') return failure("profile-format");
    expected[32]=0;expected[n-1]=0;
    if(!text("/proc/cmdline",command,sizeof(command))) return failure("cmdline");
    unsigned matches=0;char *save=NULL;
#ifdef AETHERI_ROOT_PROBE
    char partuuid[37]={0};unsigned roots=0;
#endif
    for(char *p=strtok_r(command," \t\r\n",&save);p;p=strtok_r(NULL," \t\r\n",&save)) {
        static const char prefix[]="aetheri.boot_nonce=";
        if(!strncmp(p,prefix,sizeof(prefix)-1)) {if(strcmp(p+sizeof(prefix)-1,expected)) return failure("nonce");++matches;}
#ifdef AETHERI_ROOT_PROBE
        static const char root_prefix[]="root=PARTUUID=";
        if(!strncmp(p,root_prefix,sizeof(root_prefix)-1)) {
            const char *id=p+sizeof(root_prefix)-1;
            if(strlen(id)!=36) return failure("root-argument");
            for(size_t i=0;i<36;++i) {
                bool dash=i==8 || i==13 || i==18 || i==23;
                if(dash?id[i]!='-':!hex(id+i,1)) return failure("root-argument");
            }
            memcpy(partuuid,id,37);++roots;
        }
#endif
    }
    if(matches!=1) return failure("nonce-count");
    if(!text("/etc/os-release",os,sizeof(os)) || !strstr(os,OS_ID)) return failure("os-release");
    struct utsname u;if(uname(&u) || strcmp(u.machine,"x86_64") || strcmp(u.release,expected+33)) return failure("kernel");
    struct stat s;if(stat("/sys/firmware/efi",&s) || !S_ISDIR(s.st_mode)) return failure("uefi");
    if(!filesystem("/proc",PROC_SUPER_MAGIC) || !filesystem("/sys",SYSFS_MAGIC) ||
       !filesystem("/sys/fs/cgroup",CGROUP2_SUPER_MAGIC) || !filesystem("/run",TMPFS_MAGIC)) return failure("mounts");
#ifdef AETHERI_ROOT_PROBE
    struct statfs root;if(statfs("/",&root)) return failure("root-stat");
    struct statvfs flags;
    if(root.f_type!=EXT4_SUPER_MAGIC || statvfs("/",&flags) || !(flags.f_flag&ST_RDONLY)) return failure("readonly-root");
    if(roots!=1 || stat("/",&s)) return failure("root-device");
    char path[128],device[2048],field[64];
    int written=snprintf(path,sizeof(path),"/sys/dev/block/%u:%u/uevent",major(s.st_dev),minor(s.st_dev));
    if(written<0 || (size_t)written>=sizeof(path) || !text(path,device,sizeof(device))) return failure("root-device");
    written=snprintf(field,sizeof(field),"\nPARTUUID=%s\n",partuuid);
    if(written<0 || (size_t)written>=sizeof(field) || !strstr(device,field)) return failure("root-partition");
    int file=open("/.aetheri-root-write-probe",O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);
    if(file>=0) {close(file);return failure("root-writable");}
    if(errno!=EROFS) return failure("root-write-error");
#else
    struct statfs root;if(statfs("/",&root) || (root.f_type!=TMPFS_MAGIC && root.f_type!=RAMFS_MAGIC)) return failure("ram-root");
#endif
    if(!text("/proc/self/cgroup",group,sizeof(group)) || !strstr(group,SERVICE)) return failure("service");
    if(!text("/proc/sys/kernel/random/boot_id",boot,sizeof(boot)) || strlen(boot)!=37) return failure("boot-id");
    boot[36]=0;
#ifdef AETHERI_ROOT_PROBE
    printf("AETHERI_ROOT_PASS nonce=%s kernel=%s arch=%s pid1=systemd uefi=1 ext4_ro=1 write_erofs=1 cgroup2=1 boot=%s\n",expected,u.release,u.machine,boot);
#else
    printf("AETHERI_BOOT_PASS nonce=%s kernel=%s arch=%s pid1=systemd uefi=1 ram_root=1 cgroup2=1 boot=%s\n",expected,u.release,u.machine,boot);
#endif
    return fflush(stdout)==0?0:1;
}
