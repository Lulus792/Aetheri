#define _GNU_SOURCE
#include "login/greeter.h"
#include "login/service_internal.h"
#include <errno.h>
#include <limits.h>
#include <linux/capability.h>
#include <linux/magic.h>
#include <poll.h>
#include <selinux/selinux.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/pidfd.h>
#include <sys/syscall.h>
#include <sys/vfs.h>
#include <unistd.h>

struct ae_login_greeter {
    struct ae_login_service *service;
    struct ae_login_sources *sources;
    struct ae_image image;
    pid_t owner,thread;
    int self,parent,error;
    uint32_t parent_pid;
    uint64_t self_group,parent_group;
    uid_t uid;
    gid_t gid;
    gid_t groups[AE_LOGIN_GREETER_GROUPS_MAX];
    size_t group_count;
    char label[AE_LOGIN_SOURCE_LABEL_MAX],worker_label[AE_LOGIN_SOURCE_LABEL_MAX];
    bool busy;
};
static int greeter_live(int fd)
{
    struct pollfd item={.fd=fd,.events=POLLIN};
    int r;do {r=poll(&item,1,0);}while(r<0 && errno==EINTR);
    return r<0?-errno:r==0?0:-ESRCH;
}
static int greeter_info(int fd,struct pidfd_info *out)
{
    struct statfs fs;
    if(fstatfs(fd,&fs))return -errno;
    if((unsigned long)fs.f_type!=(unsigned long)PID_FS_MAGIC)return -EPROTO;
    *out=(struct pidfd_info){.mask=PIDFD_INFO_PID|PIDFD_INFO_CREDS|PIDFD_INFO_CGROUPID};
    if(ioctl(fd,PIDFD_GET_INFO,out))return -errno;
    uint64_t required=PIDFD_INFO_PID|PIDFD_INFO_CREDS|PIDFD_INFO_CGROUPID;
    if((out->mask&required)!=required || !out->pid || out->pid!=out->tgid || !out->cgroupid)return -ENODATA;
    return greeter_live(fd);
}
static bool greeter_ids(const struct pidfd_info *p,uid_t uid,gid_t gid)
{
    return p->ruid==uid && p->euid==uid && p->suid==uid && p->fsuid==uid &&
        p->rgid==gid && p->egid==gid && p->sgid==gid && p->fsgid==gid;
}
static int greeter_label(pid_t pid,const char *expected)
{
    char *label=NULL;
    int r=getpidcon_raw(pid,&label)<0?-errno:0;
    if(!r && (!label || strcmp(label,expected)))r=-ESTALE;
    freecon(label);return r;
}
static int greeter_caps(void)
{
    struct __user_cap_header_struct h={.version=_LINUX_CAPABILITY_VERSION_3,.pid=0};
    struct __user_cap_data_struct data[2]={{0}};
    if(syscall(SYS_capget,&h,data))return -errno;
    for(size_t i=0;i<2;++i)if(data[i].effective || data[i].permitted || data[i].inheritable)return -EACCES;
    return 0;
}
int ae_login_greeter_check_groups(const gid_t *expected,size_t length)
{
    if(length>AE_LOGIN_GREETER_GROUPS_MAX || (length && !expected))return -EINVAL;
    gid_t groups[AE_LOGIN_GREETER_GROUPS_MAX];
    int count=getgroups(AE_LOGIN_GREETER_GROUPS_MAX,groups);
    if(count<0)return -errno;
    if((size_t)count!=length)return -EACCES;
    for(size_t i=1;i<(size_t)count;++i) {
        gid_t value=groups[i];size_t j=i;
        while(j && groups[j-1]>value){groups[j]=groups[j-1];--j;}
        groups[j]=value;
    }
    return length && memcmp(groups,expected,length*sizeof(gid_t))?-EACCES:0;
}
static int greeter_identity(struct ae_login_greeter *g,const struct ae_image_process *backend)
{
    if(backend->uid || backend->gid || backend->pid<=1 || security_getenforce()!=1)return -EACCES;
    struct pidfd_info self={0},parent={0};
    int r=greeter_info(g->self,&self);
    if(!r)r=greeter_info(g->parent,&parent);
    if(!r && (self.pid!=(uint32_t)g->owner || self.ppid!=g->parent_pid || parent.pid!=g->parent_pid ||
        parent.ppid!=(uint32_t)backend->pid || !greeter_ids(&self,g->uid,g->gid) || !greeter_ids(&parent,0,0) ||
        self.cgroupid!=g->self_group || parent.cgroupid!=g->parent_group))r=-ESTALE;
    if(!r)r=greeter_label(g->owner,g->label);
    if(!r)r=greeter_label((pid_t)g->parent_pid,g->worker_label);
    if(!r)r=greeter_caps();
    if(!r)r=ae_login_greeter_check_groups(g->groups,g->group_count);
    return r;
}
static int greeter_inspect(sd_bus *bus,const char *manager,const char *path,
    const struct ae_image_process *backend,const uint8_t invocation[16],const uint8_t boot[16],uint64_t group,void *data)
{
    (void)bus;(void)manager;(void)path;(void)invocation;(void)boot;(void)group;
    struct ae_login_greeter *g=data;
    int r=greeter_identity(g,backend);
    struct ae_image observed;
    const struct ae_image_process self={g->self,g->owner,g->uid,g->gid,g->label};
    if(!r)r=ae_image_match_pinned(&self,&g->image,&observed);
    if(!r)r=greeter_identity(g,backend);
    return r;
}
static int greeter_owner(struct ae_login_greeter *g)
{
    return !g?-EINVAL:getpid()!=g->owner?-ECHILD:gettid()!=g->thread?-EXDEV:g->busy?-EBUSY:0;
}
int ae_login_greeter_check(void *data)
{
    struct ae_login_greeter *g=data;
    int r=greeter_owner(g);if(r)return r;
    if(g->error)return g->error;
    g->busy=true;
    r=ae_login_sources_check(g->sources);
    if(!r)r=ae_login_service_inspect(g->service,greeter_inspect,g);
    if(!r)r=ae_login_sources_check(g->sources);
    g->busy=false;
    if(r<0)g->error=r;
    return r;
}
int ae_login_greeter_close(struct ae_login_greeter **out)
{
    if(!out)return -EINVAL;
    if(!*out)return 0;
    struct ae_login_greeter *g=*out;
    if(getpid()==g->owner){int r=greeter_owner(g);if(r)return r;}
    *out=NULL;
    if(g->self>=0)close(g->self);
    if(g->parent>=0)close(g->parent);
    free(g);return 0;
}
int ae_login_greeter_create(struct ae_login_service *service,struct ae_login_sources *sources,
    const struct ae_login_greeter_expect *expected,struct ae_login_greeter **out)
{
    if(!service || !sources || !expected || !out || *out || !expected->uid || !expected->gid ||
        expected->uid==(uid_t)-1 || expected->gid==(gid_t)-1 || !expected->label || !*expected->label ||
        strnlen(expected->label,AE_LOGIN_SOURCE_LABEL_MAX)==AE_LOGIN_SOURCE_LABEL_MAX || !expected->worker_label ||
        !*expected->worker_label || strnlen(expected->worker_label,AE_LOGIN_SOURCE_LABEL_MAX)==AE_LOGIN_SOURCE_LABEL_MAX)return -EINVAL;
    if(expected->group_count>AE_LOGIN_GREETER_GROUPS_MAX || (expected->group_count && !expected->groups))return -EINVAL;
    for(size_t i=0;i<expected->group_count;++i)
        if(!expected->groups[i] || expected->groups[i]==(gid_t)-1 || (i && expected->groups[i]<=expected->groups[i-1]))return -EINVAL;
    if(gettid()!=getpid())return -EXDEV;
    struct ae_login_greeter *g=calloc(1,sizeof(*g));if(!g)return -ENOMEM;
    g->self=g->parent=-1;g->owner=getpid();g->thread=gettid();g->service=service;g->sources=sources;
    g->uid=expected->uid;g->gid=expected->gid;
    g->group_count=expected->group_count;
    if(g->group_count)memcpy(g->groups,expected->groups,g->group_count*sizeof(gid_t));
    strcpy(g->label,expected->label);strcpy(g->worker_label,expected->worker_label);
    int r=ae_login_sources_image(sources,expected->program,&g->image);
    if(!r){g->self=(int)syscall(SYS_pidfd_open,g->owner,0);if(g->self<0)r=-errno;}
    struct pidfd_info self={0},parent={0},again={0};
    if(!r)r=greeter_info(g->self,&self);
    if(!r && (self.pid!=(uint32_t)g->owner || self.ppid<=1 || self.ppid>INT_MAX || !greeter_ids(&self,g->uid,g->gid)))r=-EACCES;
    /* This is a self-ancestry pin, never a numeric PID learned from a socket,
       caller or environment. Bracket opening with the original self relation
       and held live parent, then require its original live backend ancestry. */
    if(!r){g->parent_pid=self.ppid;g->parent=(int)syscall(SYS_pidfd_open,(pid_t)self.ppid,0);if(g->parent<0)r=-errno;}
    if(!r)r=greeter_info(g->parent,&parent);
    if(!r)r=greeter_info(g->self,&again);
    if(!r && (again.ppid!=self.ppid || again.pid!=self.pid || again.cgroupid!=self.cgroupid ||
        !greeter_ids(&again,g->uid,g->gid) || parent.pid!=self.ppid || !greeter_ids(&parent,0,0)))r=-ESTALE;
    if(!r){g->self_group=self.cgroupid;g->parent_group=parent.cgroupid;r=ae_login_greeter_check(g);}
    if(r){ae_login_greeter_close(&g);return r;}
    *out=g;return 0;
}
