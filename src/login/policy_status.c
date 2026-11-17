#define _GNU_SOURCE
#include "login/policy_status.h"
#include <errno.h>
#include <fcntl.h>
#include <linux/magic.h>
#include <linux/openat2.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/vfs.h>
#include <unistd.h>

/* Stable version 1 prefix of Linux's selinux_kernel_status mapping.
   See security/selinux/include/security.h and libselinux/src/sestatus.c. */
enum { VERSION, SEQUENCE, ENFORCING, STATUS_WORDS };
static const size_t map_size = STATUS_WORDS * sizeof(uint32_t);

struct ae_login_policy_status {
    const uint32_t *words;
    uint32_t sequence;
    pid_t process, thread;
    int error;
};

static int sample(const uint32_t *words,uint32_t *out)
{
    if(__atomic_load_n(words+VERSION,__ATOMIC_RELAXED)!=1)return -EPROTO;
    for(unsigned attempt=0;attempt<16;++attempt) {
        uint32_t first=__atomic_load_n(words+SEQUENCE,__ATOMIC_ACQUIRE);
        if(first&1U)continue;
        uint32_t enforcing=__atomic_load_n(words+ENFORCING,__ATOMIC_RELAXED);
        __atomic_thread_fence(__ATOMIC_SEQ_CST);
        uint32_t last=__atomic_load_n(words+SEQUENCE,__ATOMIC_RELAXED);
        if(first!=last)continue;
        if(enforcing!=1)return -EACCES;
        *out=first;
        return 0;
    }
    return -ESTALE;
}

int ae_login_policy_status_open(struct ae_login_policy_status **out)
{
    if(!out || *out)return -EINVAL;
    const struct open_how how={.flags=O_RDONLY|O_CLOEXEC|O_NOFOLLOW,.resolve=RESOLVE_NO_SYMLINKS};
    int fd;
    do {fd=(int)syscall(SYS_openat2,AT_FDCWD,"/sys/fs/selinux/status",&how,sizeof how);}
    while(fd<0 && errno==EINTR);
    if(fd<0)return -errno;
    struct statfs fs;
    struct stat st;
    int r=fstatfs(fd,&fs) || fstat(fd,&st)?-errno:0;
    if(!r && (fs.f_type!=SELINUX_MAGIC || !S_ISREG(st.st_mode)))r=-EPROTO;
    void *page=MAP_FAILED;
    if(!r) {
        page=mmap(NULL,map_size,PROT_READ,MAP_SHARED,fd,0);
        if(page==MAP_FAILED)r=-errno;
    }
    if(close(fd) && !r)r=-errno;
    uint32_t sequence;
    if(!r)r=sample(page,&sequence);
    struct ae_login_policy_status *status=NULL;
    if(!r) {
        status=calloc(1,sizeof *status);
        if(!status)r=-ENOMEM;
    }
    if(r) {
        if(page!=MAP_FAILED)munmap(page,map_size);
        return r;
    }
    *status=(struct ae_login_policy_status){.words=page,.sequence=sequence,
        .process=getpid(),.thread=gettid()};
    *out=status;
    return 0;
}

int ae_login_policy_status_sequence(void *value,uint32_t *out)
{
    struct ae_login_policy_status *status=value;
    if(!status || !out)return -EINVAL;
    if(getpid()!=status->process)return -ECHILD;
    if(gettid()!=status->thread)return -EXDEV;
    if(status->error)return status->error;
    uint32_t sequence;
    int r=sample(status->words,&sequence);
    if(!r && sequence!=status->sequence)r=-ESTALE;
    if(r){status->error=r;return r;}
    *out=sequence;
    return 0;
}

void ae_login_policy_status_close(struct ae_login_policy_status **value)
{
    if(!value || !*value)return;
    struct ae_login_policy_status *status=*value;
    munmap((void *)status->words,map_size);
    free(status);
    *value=NULL;
}
