#define _GNU_SOURCE
#include "manager.h"
#include <errno.h>
#include <fcntl.h>
#include <linux/magic.h>
#include <linux/openat2.h>
#include <poll.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/pidfd.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/vfs.h>
#include <systemd/sd-login.h>
#include <unistd.h>

static const char *service="org.freedesktop.systemd1",*object="/org/freedesktop/systemd1";
static const char *unit_interface="org.freedesktop.systemd1.Unit",*service_interface="org.freedesktop.systemd1.Service";
/* Optional finite-fixture diagnostics; normal library builds remain silent. */
static void trace_failure(const char *phase,int result) {
#ifdef AETHERI_AUDIO_MANAGER_TRACE
    if(result<0) fprintf(stderr,"AETHERI_AUDIO_MANAGER_TRACE phase=%s error=%d\n",phase,-result);
#else
    (void)phase;(void)result;
#endif
}
#define STEP(label,expression) do {if(!r) {phase=(label);r=(expression);if(r>=0) r=0;}} while(0)
struct snapshot {
    struct pidfd_info info;
    struct stat pin,image;
    uint8_t invocation[16];
    char cgroup[1024];
};
struct ae_audio_manager {
    sd_bus *user,*system;
    sd_id128_t user_id,system_id;
    char name[256],unit[64];
    uid_t uid;
    pid_t thread;
    int pin,loss;
    struct stat expected_image;
    struct snapshot original;
};
static int live(int fd) {
    struct pollfd p={fd,POLLIN,0};int r;
    do {r=poll(&p,1,0);} while(r<0 && errno==EINTR);
    return r<0 ? -errno : r ? -ESRCH : 0;
}
static int identity(struct ae_audio_manager *g,struct pidfd_info *info) {
    uint64_t required=PIDFD_INFO_PID|PIDFD_INFO_CREDS|PIDFD_INFO_CGROUPID;
    *info=(struct pidfd_info){.mask=required};
    if(ioctl(g->pin,PIDFD_GET_INFO,info)) return -errno;
    if((info->mask&required)!=required || !info->pid || info->pid>INT32_MAX || info->pid!=info->tgid || !info->cgroupid ||
       info->ruid!=g->uid || info->euid!=g->uid || info->suid!=g->uid || info->fsuid!=g->uid ||
       info->rgid!=getgid() || info->egid!=getgid() || info->sgid!=getgid() || info->fsgid!=getgid()) return -EACCES;
    return live(g->pin);
}
static bool same_info(const struct pidfd_info *a,const struct pidfd_info *b) {
    return a->pid==b->pid && a->tgid==b->tgid && a->cgroupid==b->cgroupid &&
        a->ruid==b->ruid && a->euid==b->euid && a->suid==b->suid && a->fsuid==b->fsuid &&
        a->rgid==b->rgid && a->egid==b->egid && a->sgid==b->sgid && a->fsgid==b->fsgid;
}
static bool same_object(const struct stat *a,const struct stat *b) {return a->st_dev==b->st_dev && a->st_ino==b->st_ino;}
static bool same_image(const struct stat *a,const struct stat *b) {
    return same_object(a,b) && a->st_size==b->st_size && a->st_mode==b->st_mode && a->st_uid==b->st_uid && a->st_gid==b->st_gid &&
        a->st_mtim.tv_sec==b->st_mtim.tv_sec && a->st_mtim.tv_nsec==b->st_mtim.tv_nsec &&
        a->st_ctim.tv_sec==b->st_ctim.tv_sec && a->st_ctim.tv_nsec==b->st_ctim.tv_nsec;
}
static int owner(sd_bus *bus,char name[256]) {
    sd_bus_message *reply=NULL;const char *text=NULL;
    int r=sd_bus_call_method(bus,"org.freedesktop.DBus","/org/freedesktop/DBus","org.freedesktop.DBus","GetNameOwner",NULL,&reply,"s",service);
    if(r>=0 && !sd_bus_message_has_signature(reply,"s")) r=-EBADMSG;
    if(r>=0) r=sd_bus_message_read(reply,"s",&text);
    if(r>0 && (!text || text[0]!=':' || !sd_bus_service_name_is_valid(text) || strlen(text)>=256)) r=-EBADMSG;
    if(r>0) memcpy(name,text,strlen(text)+1);
    sd_bus_message_unref(reply);return r>0 ? 0 : r<0 ? r : -EBADMSG;
}
static int supervisor(struct ae_audio_manager *g,struct snapshot *s) {
    sd_bus_message *reply=NULL;const char *path=NULL,*unit=NULL;const void *bytes=NULL;size_t size=0;
    int r=sd_bus_call_method(g->system,service,object,"org.freedesktop.systemd1.Manager","GetUnitByPIDFD",NULL,&reply,"h",g->pin);
    if(r>=0 && !sd_bus_message_has_signature(reply,"osay")) r=-EBADMSG;
    if(r>=0) r=sd_bus_message_read(reply,"os",&path,&unit);
    if(r>0 && strcmp(unit,g->unit)) r=-EACCES;
    if(r>0) r=sd_bus_message_read_array(reply,'y',&bytes,&size);
    if(r>0 && size!=16) r=-EBADMSG;
    if(r>0) {unsigned nonzero=0;for(unsigned i=0;i<16;++i) nonzero|=((const uint8_t *)bytes)[i];if(!nonzero) r=-ENODATA;else memcpy(s->invocation,bytes,16);}
    char *state=NULL,*cgroup=NULL;uint32_t main_pid=0,control_pid=0;
    if(r>0) r=sd_bus_get_property_string(g->system,service,path,unit_interface,"ActiveState",NULL,&state);
    if(r>=0 && (!state || strcmp(state,"active"))) r=-EAGAIN;
    if(r>=0) r=sd_bus_get_property_trivial(g->system,service,path,service_interface,"MainPID",NULL,'u',&main_pid);
    if(r>=0 && main_pid!=s->info.pid) r=-EACCES;
    if(r>=0) r=sd_bus_get_property_trivial(g->system,service,path,service_interface,"ControlPID",NULL,'u',&control_pid);
    if(r>=0 && control_pid) r=-EAGAIN;
    if(r>=0) r=sd_bus_get_property_string(g->system,service,path,service_interface,"ControlGroup",NULL,&cgroup);
    if(r>=0 && (!cgroup || cgroup[0]!='/' || !cgroup[1] || strlen(cgroup)+12>=sizeof(s->cgroup))) r=-EBADMSG;
    if(r>=0) snprintf(s->cgroup,sizeof(s->cgroup),"%s/init.scope",cgroup);
    sd_bus_message *property=NULL;
    if(r>=0) r=sd_bus_get_property(g->system,service,path,unit_interface,"InvocationID",NULL,&property,"ay");
    if(r>=0) r=sd_bus_message_read_array(property,'y',&bytes,&size);
    if(r>0 && (size!=16 || memcmp(bytes,s->invocation,16))) r=-ESTALE;
    sd_bus_message_unref(property);property=NULL;
    if(r>0) r=sd_bus_get_property(g->system,service,path,unit_interface,"Job",NULL,&property,"(uo)");
    if(r>=0) r=sd_bus_message_enter_container(property,'r',"uo");
    uint32_t job=0;const char *job_path=NULL;
    if(r>0) r=sd_bus_message_read(property,"uo",&job,&job_path);
    if(r>0 && (job || !job_path || strcmp(job_path,"/"))) r=-EAGAIN;
    sd_bus_message_unref(property);sd_bus_message_unref(reply);free(state);free(cgroup);
    return r>0 ? 0 : r<0 ? r : -EBADMSG;
}
static int cgroup_check(struct ae_audio_manager *g,struct snapshot *s) {
    char *actual=NULL;int r=sd_pidfd_get_cgroup(g->pin,&actual);
    if(r>=0 && (!actual || strcmp(actual,s->cgroup))) r=-ESTALE;
    free(actual);if(r<0) return r;
    r=0;
    int root=open("/sys/fs/cgroup",O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW);if(root<0) return -errno;
    struct statfs fs;struct stat st;
    r=fstatfs(root,&fs) ? -errno : fs.f_type!=CGROUP2_SUPER_MAGIC ? -EACCES : 0;
    struct open_how how={.flags=O_RDONLY|O_DIRECTORY|O_CLOEXEC,.resolve=RESOLVE_BENEATH|RESOLVE_NO_SYMLINKS|RESOLVE_NO_MAGICLINKS|RESOLVE_NO_XDEV};
    int fd=-1;if(!r) {fd=(int)syscall(SYS_openat2,root,s->cgroup+1,&how,sizeof(how));if(fd<0) r=-errno;}
    if(!r && fstat(fd,&st)) r=-errno;
    if(!r && (uint64_t)st.st_ino!=s->info.cgroupid) r=-ESTALE;
    if(fd>=0) close(fd);
    close(root);return r;
}
static int observe(struct ae_audio_manager *g,struct snapshot *s) {
    char name[256];sd_id128_t user,system;const char *phase="pin-live";int r=live(g->pin);
    STEP("user-bus-id",sd_bus_get_bus_id(g->user,&user));
    STEP("system-bus-id",sd_bus_get_bus_id(g->system,&system));
    if(!r && (!sd_id128_equal(user,g->user_id) || !sd_id128_equal(system,g->system_id))) r=-ESTALE;
    STEP("owner",owner(g->user,name));
    if(!r && strcmp(name,g->name)) r=-ESTALE;
    STEP("kernel-identity",identity(g,&s->info));
    if(!r && fstat(g->pin,&s->pin)) r=-errno;
    STEP("supervisor",supervisor(g,s));
    STEP("cgroup",cgroup_check(g,s));
    char path[64];snprintf(path,sizeof(path),"/proc/%u/exe",s->info.pid);
    int image=-1;if(!r) {phase="current-image";image=open(path,O_PATH|O_CLOEXEC);if(image<0) r=-errno;}
    if(!r && fstat(image,&s->image)) r=-errno;
    if(image>=0) close(image);
    if(!r && !same_image(&s->image,&g->expected_image)) r=-ESTALE;
    struct pidfd_info after;STEP("kernel-recheck",identity(g,&after));
    if(!r && !same_info(&s->info,&after)) r=-ESTALE;
    STEP("owner-recheck",owner(g->user,name));
    if(!r && strcmp(name,g->name)) r=-ESTALE;
    trace_failure(phase,r);return r;
}
static bool equal(const struct snapshot *a,const struct snapshot *b) {
    return same_info(&a->info,&b->info) && same_object(&a->pin,&b->pin) && same_image(&a->image,&b->image) &&
        !memcmp(a->invocation,b->invocation,16) && !strcmp(a->cgroup,b->cgroup);
}
int ae_audio_manager_check(struct ae_audio_manager *g) {
    if(!g) return -EINVAL;
    if(g->loss) return g->loss;
    if(g->thread!=(pid_t)syscall(SYS_gettid)) return g->loss=-ECHILD;
    struct snapshot current={0};int r=observe(g,&current);
    if(!r && !equal(&current,&g->original)) r=-ESTALE;
    return g->loss=r;
}
const char *ae_audio_manager_name(const struct ae_audio_manager *g) {return g ? g->name : NULL;}
void ae_audio_manager_close(struct ae_audio_manager **pointer) {
    if(!pointer || !*pointer) return;
    struct ae_audio_manager *g=*pointer;*pointer=NULL;
    if(g->pin>=0) close(g->pin);
    sd_bus_unref(g->user);sd_bus_unref(g->system);free(g);
}
struct ae_audio_manager *ae_audio_manager_open(sd_bus *user,sd_bus *system,uid_t uid,int image) {
    if(!user || !system || user==system || uid!=getuid() || uid!=geteuid() || getgid()!=getegid() || image<0) {errno=EINVAL;return NULL;}
    struct ae_audio_manager *g=calloc(1,sizeof(*g));if(!g) return NULL;
    g->pin=-1;g->uid=uid;g->thread=(pid_t)syscall(SYS_gettid);g->user=sd_bus_ref(user);g->system=sd_bus_ref(system);
    snprintf(g->unit,sizeof(g->unit),"user@%u.service",(unsigned)uid);
    const char *phase="bus-client";
    int r=sd_bus_is_bus_client(user)>0 && sd_bus_is_bus_client(system)>0 ? 0 : -EPROTO;
    uint64_t timeout=0;
    STEP("user-timeout",sd_bus_get_method_call_timeout(user,&timeout));
    if(!r && (!timeout || timeout>5000000u)) r=-EINVAL;
    STEP("system-timeout",sd_bus_get_method_call_timeout(system,&timeout));
    if(!r && (!timeout || timeout>5000000u)) r=-EINVAL;
    if(!r) {phase="expected-image";if(fstat(image,&g->expected_image)) r=-errno;}
    if(!r && (!S_ISREG(g->expected_image.st_mode) || g->expected_image.st_uid || (g->expected_image.st_mode&0022) || g->expected_image.st_size<=0)) r=-EACCES;
    STEP("user-bus-id",sd_bus_get_bus_id(user,&g->user_id));
    STEP("system-bus-id",sd_bus_get_bus_id(system,&g->system_id));
    STEP("owner",owner(user,g->name));
    sd_bus_creds *creds=NULL;uint64_t mask=SD_BUS_CREDS_EUID|SD_BUS_CREDS_PID|SD_BUS_CREDS_PIDFD;
    STEP("connection-credentials",sd_bus_get_name_creds(user,g->name,mask,&creds));
    if(!r && ((sd_bus_creds_get_mask(creds)&mask)!=mask || (sd_bus_creds_get_augmented_mask(creds)&mask))) r=-ENODATA;
    uid_t connection_uid=0;pid_t pid=0;
    STEP("connection-uid",sd_bus_creds_get_euid(creds,&connection_uid));
    STEP("connection-pid",sd_bus_creds_get_pid(creds,&pid));
    if(!r && (connection_uid!=uid || pid<=0)) r=-EACCES;
    STEP("connection-pin",sd_bus_creds_get_pidfd_dup(creds,&g->pin));
    sd_bus_creds_unref(creds);
    if(!r) {int flags=fcntl(g->pin,F_GETFD);if(flags<0) r=-errno;else if(!(flags&FD_CLOEXEC)) r=-EPERM;}
    STEP("original-observation",observe(g,&g->original));
    if(!r && g->original.info.pid!=(uint32_t)pid) r=-ESTALE;
    STEP("original-recheck",ae_audio_manager_check(g));
    if(!r) return g;
    trace_failure(phase,r);ae_audio_manager_close(&g);errno=-r;return NULL;
}
