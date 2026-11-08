/* Controlled native syscall observations around the actual root preflight.
   This never opens a host root/device, starts a unit or admits an installation. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#define AETHERI_ROOT_PROBE
#define main ae_root_preflight_main
#include "../src/boot/probe.c"
#undef main
#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>

static const char *const nonce="aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
static const char *const partition="11111111-1111-4111-8111-111111111111";
static char profile[256],cmdline[1024],uevent[2048];
static const char *pid1,*cgroup,*boot_id;
static uid_t uid,euid;static pid_t parent;
static bool bad_kernel,bad_efi,bad_mount,bad_root,readwrite_root,bad_device,write_error;
static unsigned writes,waits,clock_calls;static bool interrupt_wait;
static const char *current_text;static size_t current_at;
static int current_fd=-1;
static char captured[32768];

uid_t __wrap_getuid(void) {return uid;}
uid_t __wrap_geteuid(void) {return euid;}
pid_t __wrap_getppid(void) {return parent;}
int __wrap_uname(struct utsname *value) {
    memset(value,0,sizeof(*value));strcpy(value->machine,"x86_64");
    strcpy(value->release,bad_kernel?"other-kernel":"fixture-kernel");return 0;
}
int __wrap_stat(const char *path,struct stat *value) {
    memset(value,0,sizeof(*value));
    if(!strcmp(path,"/sys/firmware/efi")) {if(bad_efi) {errno=ENOENT;return -1;}value->st_mode=S_IFDIR|0755;return 0;}
    assert(!strcmp(path,"/"));value->st_dev=makedev(8,18);value->st_mode=S_IFDIR|0755;return 0;
}
int __wrap_statfs(const char *path,struct statfs *value) {
    memset(value,0,sizeof(*value));
    if(!strcmp(path,"/")) value->f_type=bad_root?TMPFS_MAGIC:EXT4_SUPER_MAGIC;
    else if(!strcmp(path,"/proc")) value->f_type=PROC_SUPER_MAGIC;
    else if(!strcmp(path,"/sys")) value->f_type=SYSFS_MAGIC;
    else if(!strcmp(path,"/sys/fs/cgroup")) value->f_type=bad_mount?TMPFS_MAGIC:CGROUP2_SUPER_MAGIC;
    else {assert(!strcmp(path,"/run"));value->f_type=TMPFS_MAGIC;}
    return 0;
}
int __wrap_statvfs(const char *path,struct statvfs *value) {
    assert(!strcmp(path,"/"));memset(value,0,sizeof(*value));value->f_flag=readwrite_root?0:ST_RDONLY;return 0;
}
int __wrap_open(const char *path,int flags,...) {
    assert(current_fd<0);
    if(!strcmp(path,"/.aetheri-root-write-probe")) {
        assert((flags&(O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW))==(O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW));
        ++writes;errno=write_error?EACCES:EROFS;return -1;
    }
    assert(flags==(O_RDONLY|O_CLOEXEC|O_NOFOLLOW));
    if(!strcmp(path,PROFILE)) current_text=profile;
    else if(!strcmp(path,"/proc/1/comm")) current_text=pid1;
    else if(!strcmp(path,"/proc/cmdline")) current_text=cmdline;
    else if(!strcmp(path,"/etc/os-release")) current_text="NAME=fixture\nID=aetheri-root-probe\n";
    else if(!strcmp(path,"/proc/self/cgroup")) current_text=cgroup;
    else if(!strcmp(path,"/proc/sys/kernel/random/boot_id")) current_text=boot_id;
    else if(!strcmp(path,"/sys/class/tty/console/active")) current_text="tty0\n";
    else {assert(!strcmp(path,"/sys/dev/block/8:18/uevent"));current_text=bad_device?NULL:uevent;}
    if(!current_text) {errno=ENOENT;return -1;}
    current_fd=12345;current_at=0;return current_fd;
}
ssize_t __wrap_read(int fd,void *buffer,size_t count) {
    assert(fd==current_fd && fd==12345);
    size_t left=strlen(current_text)-current_at,size=left<count?left:count;
    memcpy(buffer,current_text+current_at,size);current_at+=size;return (ssize_t)size;
}
int __wrap_close(int fd) {assert(fd==current_fd && fd==12345);current_fd=-1;return 0;}
int __wrap_clock_gettime(clockid_t clock,struct timespec *value) {
    assert(clock==CLOCK_MONOTONIC);++clock_calls;*value=(struct timespec){.tv_sec=100,.tv_nsec=4321};return 0;
}
int __wrap_clock_nanosleep(clockid_t clock,int flags,const struct timespec *deadline,struct timespec *remaining) {
    assert(clock==CLOCK_MONOTONIC && flags==TIMER_ABSTIME && !remaining);
    assert(deadline->tv_nsec==4321 && deadline->tv_sec==(waits?106:105));
    if(interrupt_wait) {interrupt_wait=false;return EINTR;}
    ++waits;return 0;
}
static void reset(void) {
    snprintf(profile,sizeof(profile),"%s\nfixture-kernel\n",nonce);
    snprintf(cmdline,sizeof(cmdline),"root=PARTUUID=%s aetheri.boot_nonce=%s\n",partition,nonce);
    snprintf(uevent,sizeof(uevent),"MAJOR=8\nMINOR=18\nPARTUUID=%s\n",partition);
    pid1="systemd\n";cgroup="0::/system.slice/aetheri-root.service\n";
    boot_id="22222222-2222-4222-8222-222222222222\n";uid=euid=0;parent=1;
    bad_kernel=bad_efi=bad_mount=bad_root=readwrite_root=bad_device=write_error=false;
    writes=waits=clock_calls=0;interrupt_wait=true;assert(current_fd<0);
}
/* Capture actual stdio; only the probe's low-level OS calls above are wrapped. */
static void check(const char *reason) {
    fflush(stdout);int saved=dup(STDOUT_FILENO);assert(saved>=0);
    FILE *out=tmpfile();assert(out && dup2(fileno(out),STDOUT_FILENO)>=0);
    int result=ae_root_preflight_main();assert(!fflush(stdout));
    assert(dup2(saved,STDOUT_FILENO)>=0); /* raw syscall avoids wrapped close */
    rewind(out);char *output=captured;size_t count=fread(output,1,sizeof(captured)-1,out);
    assert(!ferror(out) && fgetc(out)==EOF);output[count]=0;
    assert(!fclose(out));
    /* A real duplicate must not be passed to the probe-only wrapped close. */
    extern int __real_close(int);assert(!__real_close(saved));
    assert(current_fd<0);
    if(!reason) {
        assert(result==0 && writes==1 && waits==0 && clock_calls==0);
        assert(strstr(output,"AETHERI_ROOT_PASS ")==output && !strstr(output,"AETHERI_ROOT_DETAIL"));return;
    }
    assert(result==1);
    char marker[128];snprintf(marker,sizeof(marker),"AETHERI_ROOT_FAIL reason=%s\n",reason);
    assert(strstr(output,marker)==output && !strstr(output+strlen(marker),marker));
    assert(!strstr(output,"\nAETHERI_ROOT_PASS "));
    assert(strstr(output,"AETHERI_ROOT_DETAIL fs[/]=0x") && strstr(output,"AETHERI_ROOT_DETAIL root_device=8:18\n"));
    assert(waits==(AETHERI_ROOT_DIAGNOSTIC_SECONDS?2u:0u));
    assert(clock_calls==(AETHERI_ROOT_DIAGNOSTIC_SECONDS?1u:0u));
}
int main(void) {
    reset();check(NULL);
    reset();uid=1000;check("supervision");assert(!writes);
    reset();euid=1000;check("supervision");assert(!writes);
    reset();parent=2;check("supervision");assert(!writes);
    reset();pid1="other\n";check("pid1");assert(!writes);
    reset();profile[0]='!';check("profile-format");assert(!writes);
    reset();cmdline[0]=0;check("nonce-count");assert(!writes);
    assert(strstr(captured,"cmdline_bytes=0 nonce_tokens=0 matching_nonce_tokens=0 root_partuuid_tokens=0 profile_readable=1\n"));
    reset();snprintf(cmdline,sizeof(cmdline),"root=PARTUUID=%s firmware_option=private-value\n",partition);
    check("nonce-count");assert(!writes && !strstr(captured,"private-value"));
    assert(strstr(captured,"nonce_tokens=0 matching_nonce_tokens=0 root_partuuid_tokens=1 "));
    reset();snprintf(cmdline,sizeof(cmdline),"aetheri.boot_nonce=%s root=PARTUUID=%s aetheri.boot_nonce=%s\n",nonce,partition,nonce);
    check("nonce-count");assert(!writes);
    assert(strstr(captured,"nonce_tokens=2 matching_nonce_tokens=2 root_partuuid_tokens=1 "));
    reset();snprintf(cmdline,sizeof(cmdline),"aetheri.boot_nonce=bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb root=PARTUUID=%s\n",partition);
    check("nonce");assert(!writes);
    assert(strstr(captured,"nonce_tokens=1 matching_nonce_tokens=0 root_partuuid_tokens=1 "));
    reset();snprintf(cmdline,sizeof(cmdline),"root=PARTUUID=%s aetheri.boot_nonce=%s root=PARTUUID=%s\n",partition,nonce,partition);
    check("root-device");assert(!writes);
    assert(strstr(captured,"nonce_tokens=1 matching_nonce_tokens=1 root_partuuid_tokens=2 "));
    reset();bad_kernel=true;check("kernel");assert(!writes);
    reset();bad_efi=true;check("uefi");assert(!writes);
    reset();bad_mount=true;check("mounts");assert(!writes);
    reset();bad_root=true;check("readonly-root");assert(!writes);
    reset();readwrite_root=true;check("readonly-root");assert(!writes);
    reset();bad_device=true;check("root-device");assert(!writes);
    reset();uevent[0]=0;check("root-partition");assert(!writes);
    reset();write_error=true;check("root-write-error");assert(writes==1);
    reset();cgroup="0::/foreign.service\n";check("service");
    reset();cgroup="0::/foreign\nAETHERI_ROOT_PASS forged\033[2J\n";check("service");
    reset();boot_id="invalid\n";check("boot-id");
    puts("Root preflight: successful baseline and failure fences/observations exercised");
    return 0;
}
