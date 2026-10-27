#include "linux/execute.h"
#include "linux/exec_channel.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <limits.h>
#include <linux/capability.h>
#include <linux/close_range.h>
#include <linux/securebits.h>
#include <selinux/selinux.h>
#include <signal.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/syscall.h>
#include <sys/sysmacros.h>
#include <unistd.h>

static _Noreturn void failure(int fd,enum ae_exec_phase phase,int error)
{
    /* SIGPIPE may otherwise kill the failing child before its defined exit.
     * Save the actual syscall errno at the call site; these helpers may change it. */
    sigset_t set; sigemptyset(&set); sigaddset(&set,SIGPIPE); (void)sigprocmask(SIG_BLOCK,&set,NULL);
    uint8_t bytes[16]={0}; memcpy(bytes,"AETHXER1",8);
    uint32_t fields[]={(uint32_t)phase,(uint32_t)(error>0 ? error : EIO)};
    for (size_t n=0;n<2;++n) for (size_t m=0;m<4;++m) bytes[8+4*n+m]=(uint8_t)(fields[n]>>(24-8*m));
    if (fd>=3) { ssize_t written; do { written=write(fd,bytes,sizeof bytes); } while (written<0 && errno==EINTR); }
    _exit(125);
}
static int vectors(char *const *v,size_t maximum,int environment)
{
    if (v==NULL) return -EINVAL;
    size_t total=0,n=0;
    for (;n<=maximum && v[n]!=NULL;++n) {
        if (n==maximum) return -E2BIG;
        size_t length=strnlen(v[n],4097); if (length>4096 || (!environment && n==0 && length==0)) return -E2BIG;
        if (length>8192-total) return -E2BIG;
        total+=length;
        if (environment) {
            const char *eq=strchr(v[n],'='); if (eq==NULL || eq==v[n]) return -EINVAL;
            size_t key=(size_t)(eq-v[n]);
            if (!((key==4 && memcmp(v[n],"LANG",4)==0) || (key==6 && memcmp(v[n],"LC_ALL",6)==0) ||
                (key==4 && memcmp(v[n],"TERM",4)==0) || (key==4 && memcmp(v[n],"HOME",4)==0) ||
                (key==4 && memcmp(v[n],"PATH",4)==0) || (key==13 && memcmp(v[n],"NOTIFY_SOCKET",13)==0) ||
                (key>8 && memcmp(v[n],"AETHERI_",8)==0))) return -EACCES;
            for (size_t m=0;m<key;++m) if (!((v[n][m]>='A' && v[n][m]<='Z') || v[n][m]=='_' || (v[n][m]>='0' && v[n][m]<='9'))) return -EINVAL;
            for (size_t m=0;m<n;++m) if (strncmp(v[m],v[n],key)==0 && v[m][key]=='=') return -EINVAL;
        }
    }
    return !environment && n==0 ? -EINVAL : 0;
}
static int single_thread(void)
{
    DIR *d=opendir("/proc/self/task"); if (d==NULL) return -errno;
    size_t count=0; int r=0; struct dirent *e;
    for (;;) { errno=0; e=readdir(d); if (e==NULL) { r=errno ? -errno : 0; break; }
        if (strcmp(e->d_name,".")!=0 && strcmp(e->d_name,"..")!=0) ++count; }
    closedir(d); return r==0 && count!=1 ? -EBUSY : r;
}
static int profile(const struct ae_execute *p)
{
    if (p==NULL || p->image==NULL || p->label==NULL || p->image_fd<3 || p->data_fd<3 || p->error_fd<3 ||
        p->image_fd==p->data_fd || p->image_fd==p->error_fd || p->data_fd==p->error_fd || p->uid==0 || p->uid==(uid_t)-1 || p->gid==0 || p->gid==(gid_t)-1 ||
        strnlen(p->label,512)==0 || strnlen(p->label,512)>=512) return -EINVAL;
    if (getuid()!=0 || geteuid()!=0 || getgid()!=0 || getegid()!=0 || security_getenforce()!=1 || prctl(PR_GET_NO_NEW_PRIVS,0L,0L,0L,0L)!=1) return -EACCES;
    int r=single_thread(); if (r==0) r=vectors(p->argv,9,0); if (r==0) r=vectors(p->envp,16,1);
    struct stat s; struct statvfs fs;
    if (r==0) r=ae_exec_writer_check(p->error_fd,0,0);
    if (r==0 && fstat(p->data_fd,&s)!=0) r=-errno;
    if (r==0 && (!S_ISDIR(s.st_mode) || s.st_uid!=p->uid || s.st_gid!=p->gid || (s.st_mode&07777)!=0700)) r=-EACCES;
    if (r==0 && fstatvfs(p->data_fd,&fs)!=0) r=-errno;
    if (r==0 && ((fs.f_flag&(ST_NOEXEC|ST_NOSUID|ST_NODEV))!=(ST_NOEXEC|ST_NOSUID|ST_NODEV) || (fs.f_flag&ST_RDONLY))) r=-EACCES;
    if (r==0 && fstatvfs(p->image_fd,&fs)!=0) r=-errno;
    if (r==0 && ((fs.f_flag&(ST_RDONLY|ST_NOSUID|ST_NODEV))!=(ST_RDONLY|ST_NOSUID|ST_NODEV) || (fs.f_flag&ST_NOEXEC))) r=-EACCES;
    for (int fd=0;r==0 && fd<3;++fd) {
        if (fstat(fd,&s)!=0) r=-errno;
        else if (!(S_ISFIFO(s.st_mode) || S_ISSOCK(s.st_mode) || (S_ISCHR(s.st_mode) && major(s.st_rdev)==1 && minor(s.st_rdev)==3))) r=-EACCES;
    }
    return r;
}
_Noreturn void ae_execute_target(const struct ae_execute *p)
{
    int error_fd=p!=NULL ? p->error_fd : -1;
    int r=profile(p); if (r!=0) failure(error_fd,AE_EXEC_PROFILE,-r);
    struct ae_image actual; r=ae_image_verify(p->image_fd,p->image->digest,&actual);
    if (r!=0 || !ae_image_equal(&actual,p->image)) failure(error_fd,AE_EXEC_IMAGE,r!=0 ? -r : ESTALE);
    if (setexeccon_raw(p->label)!=0) { int saved=errno; failure(error_fd,AE_EXEC_CONTEXT,saved); }
    char *pending=NULL; if (getexeccon_raw(&pending)!=0) { int saved=errno; failure(error_fd,AE_EXEC_CONTEXT,saved); }
    int same=pending!=NULL && strcmp(pending,p->label)==0; freecon(pending);
    if (!same) failure(error_fd,AE_EXEC_CONTEXT,ESTALE);
    if (close_range(3,UINT_MAX,CLOSE_RANGE_CLOEXEC|CLOSE_RANGE_UNSHARE)!=0) { int saved=errno; failure(error_fd,AE_EXEC_DESCRIPTORS,saved); }
    for (int fd=0;fd<3;++fd) if (fcntl(fd,F_SETFD,0)!=0) { int saved=errno; failure(error_fd,AE_EXEC_DESCRIPTORS,saved); }
    struct rlimit core={0,0}; if (setrlimit(RLIMIT_CORE,&core)!=0) { int saved=errno; failure(error_fd,AE_EXEC_CAPABILITIES,saved); }
    struct __user_cap_header_struct header={.version=_LINUX_CAPABILITY_VERSION_3}; struct __user_cap_data_struct caps[2]={{0}};
    if (syscall(SYS_capget,&header,caps)!=0) { int saved=errno; failure(error_fd,AE_EXEC_CAPABILITIES,saved); }
    uint32_t required=(UINT32_C(1)<<CAP_SETUID)|(UINT32_C(1)<<CAP_SETGID)|(UINT32_C(1)<<CAP_SETPCAP);
    uint32_t allowed=required|(UINT32_C(1)<<CAP_SYS_ADMIN)|(UINT32_C(1)<<CAP_DAC_READ_SEARCH);
    if ((caps[0].effective&required)!=required || (caps[0].effective&~allowed) || (caps[0].permitted&~allowed) || (caps[0].inheritable&~allowed) ||
        caps[1].effective || caps[1].permitted || caps[1].inheritable) failure(error_fd,AE_EXEC_CAPABILITIES,EACCES);
    caps[0].inheritable=0;
    if (syscall(SYS_capset,&header,caps)!=0) { int saved=errno; failure(error_fd,AE_EXEC_CAPABILITIES,saved); }
    if (prctl(PR_CAP_AMBIENT,(long)PR_CAP_AMBIENT_CLEAR_ALL,0L,0L,0L)!=0) { int saved=errno; failure(error_fd,AE_EXEC_CAPABILITIES,saved); }
    unsigned long bits=SECBIT_NOROOT|SECBIT_NOROOT_LOCKED|SECBIT_KEEP_CAPS_LOCKED|SECBIT_NO_SETUID_FIXUP_LOCKED;
    if (prctl(PR_SET_SECUREBITS,bits,0L,0L,0L)!=0) { int saved=errno; failure(error_fd,AE_EXEC_CAPABILITIES,saved); }
    int last=-1;
    for (int cap=0;cap<64;++cap) {
        int bound=prctl(PR_CAPBSET_READ,(long)cap,0L,0L,0L);
        if (bound<0) { if (errno==EINVAL && cap>CAP_LAST_CAP) break; int saved=errno; failure(error_fd,AE_EXEC_CAPABILITIES,saved); }
        last=cap;
        if (prctl(PR_CAPBSET_DROP,(long)cap,0L,0L,0L)!=0) { int saved=errno; failure(error_fd,AE_EXEC_CAPABILITIES,saved); }
    }
    if (last<CAP_LAST_CAP || last==63) failure(error_fd,AE_EXEC_CAPABILITIES,ENOTSUP);
    for (int cap=0;cap<=last;++cap) if (prctl(PR_CAPBSET_READ,(long)cap,0L,0L,0L)!=0) failure(error_fd,AE_EXEC_CAPABILITIES,EACCES);
    if (setgroups(0,NULL)!=0) { int saved=errno; failure(error_fd,AE_EXEC_GROUPS,saved); }
    int groups=getgroups(0,NULL);
    if (groups<0) { int saved=errno; failure(error_fd,AE_EXEC_GROUPS,saved); }
    if (groups!=0) failure(error_fd,AE_EXEC_GROUPS,EACCES);
    if (setresgid(p->gid,p->gid,p->gid)!=0) { int saved=errno; failure(error_fd,AE_EXEC_GID,saved); }
    if (setresuid(p->uid,p->uid,p->uid)!=0) { int saved=errno; failure(error_fd,AE_EXEC_UID,saved); }
    uid_t ru,eu,su; gid_t rg,eg,sg;
    if (getresuid(&ru,&eu,&su)!=0 || getresgid(&rg,&eg,&sg)!=0) { int saved=errno; failure(error_fd,AE_EXEC_UID,saved); }
    groups=getgroups(0,NULL);
    if (groups<0) { int saved=errno; failure(error_fd,AE_EXEC_GROUPS,saved); }
    if (ru!=p->uid || eu!=p->uid || su!=p->uid || rg!=p->gid || eg!=p->gid || sg!=p->gid || groups!=0) failure(error_fd,AE_EXEC_UID,EACCES);
    if (syscall(SYS_capget,&header,caps)!=0) { int saved=errno; failure(error_fd,AE_EXEC_CAPABILITIES,saved); }
    for (size_t n=0;n<2;++n) if (caps[n].effective || caps[n].permitted || caps[n].inheritable) failure(error_fd,AE_EXEC_CAPABILITIES,EACCES);
    for (int cap=0;cap<=last;++cap) if (prctl(PR_CAP_AMBIENT,(long)PR_CAP_AMBIENT_IS_SET,(long)cap,0L,0L)!=0) failure(error_fd,AE_EXEC_CAPABILITIES,EACCES);
    /* Enter private data with the final owner's credentials, without relying on
     * a root DAC bypass. The already held FD remains CLOEXEC until the syscall. */
    if (fchdir(p->data_fd)!=0) { int saved=errno; failure(error_fd,AE_EXEC_DESCRIPTORS,saved); }
    struct sigaction action={0}; action.sa_handler=SIG_DFL; sigemptyset(&action.sa_mask);
    for (int sig=1;sig<NSIG;++sig) if (sig!=SIGKILL && sig!=SIGSTOP && sigaction(sig,&action,NULL)!=0 && errno!=EINVAL) { int saved=errno; failure(error_fd,AE_EXEC_SIGNALS,saved); }
    sigset_t empty; sigemptyset(&empty);
    if (sigprocmask(SIG_SETMASK,&empty,NULL)!=0) { int saved=errno; failure(error_fd,AE_EXEC_SIGNALS,saved); }
    if (prctl(PR_GET_NO_NEW_PRIVS,0L,0L,0L,0L)!=1) failure(error_fd,AE_EXEC_CAPABILITIES,EACCES);
    /* No success return, shell, alternate interpreter or unconfined fallback. */
    (void)execveat(p->image_fd,"",p->argv,p->envp,AT_EMPTY_PATH);
    int saved=errno; failure(error_fd,AE_EXEC_CALL,saved);
}
