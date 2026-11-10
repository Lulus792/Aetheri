#define _GNU_SOURCE
#include "login/service.h"
#include <errno.h>
#include <fcntl.h>
#include <linux/magic.h>
#include <linux/openat2.h>
#include <poll.h>
#include <selinux/selinux.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/pidfd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/un.h>
#include <sys/vfs.h>
#include <systemd/sd-id128.h>
#include <systemd/sd-login.h>
#include <unistd.h>

struct service_observation {uint8_t invocation[16];uint64_t cgroup;char group[1024];};
struct ae_login_service {
    sd_bus *bus;
    sd_id128_t bus_id,boot;
    struct stat peer_object,manager_object;
    struct ucred credentials;
    struct service_observation original;
    char unit[256],label[512];
    int peer,manager,error;
    pid_t owner,thread;
    bool busy;
};
static int service_text(char *out,size_t capacity,const char *value) {
    if(!value || !*value)return -EBADMSG;
    size_t n=strnlen(value,capacity);if(n==capacity)return -E2BIG;
    memcpy(out,value,n+1);return 0;
}
static int service_live(int fd) {
    struct pollfd p={.fd=fd,.events=POLLIN};int r=poll(&p,1,0);
    return r<0?-errno:r?-ESRCH:0;
}
static int service_info(int fd,struct pidfd_info *info) {
    *info=(struct pidfd_info){.mask=PIDFD_INFO_PID|PIDFD_INFO_CREDS|PIDFD_INFO_CGROUPID};struct statfs fs;
    if(fstatfs(fd,&fs))return -errno;
    if((unsigned long)fs.f_type!=(unsigned long)PID_FS_MAGIC)return -EPROTO;
    if(ioctl(fd,PIDFD_GET_INFO,info))return -errno;
    uint64_t required=PIDFD_INFO_PID|PIDFD_INFO_CREDS|PIDFD_INFO_CGROUPID;
    if((info->mask&required)!=required || !info->pid || info->pid!=info->tgid || !info->cgroupid)return -ENODATA;
    return service_live(fd);
}
static bool service_object(const struct stat *a,const struct stat *b) {return a->st_dev==b->st_dev && a->st_ino==b->st_ino;}
static int service_manager(struct ae_login_service *s,bool capture) {
    sd_id128_t id,boot;int r=sd_bus_get_bus_id(s->bus,&id);
    if(r<0)return r;
    r=sd_id128_get_boot(&boot);if(r<0)return r;
    if(!capture && (!sd_id128_equal(id,s->bus_id) || !sd_id128_equal(boot,s->boot)))return -ESTALE;
    uint64_t mask=SD_BUS_CREDS_PID|SD_BUS_CREDS_EUID|SD_BUS_CREDS_PIDFD;sd_bus_creds *creds=NULL;
    r=sd_bus_get_name_creds(s->bus,"org.freedesktop.systemd1",mask,&creds);
    int fd=-1;uid_t uid=0;pid_t pid=0;struct stat object;struct pidfd_info info;
    if(r>=0 && ((sd_bus_creds_get_mask(creds)&mask)!=mask || (sd_bus_creds_get_augmented_mask(creds)&mask)))r=-ENODATA;
    if(r>=0)r=sd_bus_creds_get_pid(creds,&pid);
    if(r>=0)r=sd_bus_creds_get_euid(creds,&uid);
    if(r>=0 && (pid!=1 || uid!=0))r=-EACCES;
    if(r>=0)r=sd_bus_creds_get_pidfd_dup(creds,&fd);
    if(r>=0)r=service_info(fd,&info);
    if(r>=0 && (info.pid!=1 || info.ruid || info.euid || info.suid || info.fsuid))r=-EACCES;
    if(r>=0 && fstat(fd,&object))r=-errno;
    if(r>=0 && !capture && !service_object(&object,&s->manager_object))r=-ESTALE;
    if(r>=0 && capture){s->manager_object=object;s->bus_id=id;s->boot=boot;s->manager=fd;fd=-1;}
    if(r>=0 && !capture)r=service_live(s->manager);
    if(fd>=0)close(fd);
    sd_bus_creds_unref(creds);return r<0?r:0;
}
static int service_identity(struct ae_login_service *s,uint64_t *group) {
    struct stat object;struct pidfd_info info;int r=service_info(s->peer,&info);if(r<0)return r;
    if(fstat(s->peer,&object))return -errno;
    if(!service_object(&object,&s->peer_object) || info.pid!=(uint32_t)s->credentials.pid ||
       info.ruid!=s->credentials.uid || info.euid!=s->credentials.uid || info.suid!=s->credentials.uid || info.fsuid!=s->credentials.uid ||
       info.rgid!=s->credentials.gid || info.egid!=s->credentials.gid || info.sgid!=s->credentials.gid || info.fsgid!=s->credentials.gid)return -ESTALE;
    char *label=NULL;r=getpidcon_raw(s->credentials.pid,&label)==0?0:-errno;
    if(!r && (!label || strcmp(label,s->label)))r=-ESTALE;
    freecon(label);if(!r)*group=info.cgroupid;return r;
}
static int service_properties(struct ae_login_service *s,const char *path,const uint8_t invocation[16],char group[1024]) {
    char *state=NULL,*id=NULL;sd_bus_message *job=NULL,*receipt=NULL;uint32_t main=0,control=0,number=0;const char *job_path=NULL;int r;
    r=sd_bus_get_property_string(s->bus,"org.freedesktop.systemd1",path,"org.freedesktop.systemd1.Unit","ActiveState",NULL,&state);
    if(r>=0)r=sd_bus_get_property_string(s->bus,"org.freedesktop.systemd1",path,"org.freedesktop.systemd1.Unit","Id",NULL,&id);
    if(r>=0 && (!state || strcmp(state,"active") || !id || strcmp(id,s->unit)))r=-ESTALE;
    if(r>=0)r=sd_bus_get_property(s->bus,"org.freedesktop.systemd1",path,"org.freedesktop.systemd1.Unit","InvocationID",NULL,&receipt,"ay");
    const void *bytes=NULL;size_t size=0;
    if(r>=0)r=sd_bus_message_read_array(receipt,'y',&bytes,&size);
    if(r>=0 && (size!=16 || !bytes || memcmp(bytes,invocation,16)))r=-ESTALE;
    if(r>=0)r=sd_bus_get_property(s->bus,"org.freedesktop.systemd1",path,"org.freedesktop.systemd1.Unit","Job",NULL,&job,"(uo)");
    if(r>=0)r=sd_bus_message_read(job,"(uo)",&number,&job_path);
    if(r==0)r=-EBADMSG;
    if(r>0 && (number || !job_path || strcmp(job_path,"/")))r=-EAGAIN;
    if(r>=0)r=sd_bus_get_property_trivial(s->bus,"org.freedesktop.systemd1",path,"org.freedesktop.systemd1.Service","MainPID",NULL,'u',&main);
    if(r>=0)r=sd_bus_get_property_trivial(s->bus,"org.freedesktop.systemd1",path,"org.freedesktop.systemd1.Service","ControlPID",NULL,'u',&control);
    if(r>=0 && (main!=(uint32_t)s->credentials.pid || control))r=-EACCES;
    char *value=NULL;
    if(r>=0)r=sd_bus_get_property_string(s->bus,"org.freedesktop.systemd1",path,"org.freedesktop.systemd1.Service","ControlGroup",NULL,&value);
    if(r>=0)r=service_text(group,1024,value);
    free(value);free(state);free(id);sd_bus_message_unref(job);sd_bus_message_unref(receipt);return r<0?r:0;
}
static int service_group(const char *path,uint64_t expected) {
    if(path[0]!='/' || !path[1])return -EBADMSG;
    int root=open("/sys/fs/cgroup",O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW);if(root<0)return -errno;
    struct statfs fs;int r=fstatfs(root,&fs)?-errno:fs.f_type==CGROUP2_SUPER_MAGIC?0:-EPROTO;
    struct open_how how={.flags=O_RDONLY|O_DIRECTORY|O_CLOEXEC,.resolve=RESOLVE_BENEATH|RESOLVE_NO_SYMLINKS|RESOLVE_NO_MAGICLINKS|RESOLVE_NO_XDEV};
    int fd=-1;if(!r){fd=(int)syscall(SYS_openat2,root,path+1,&how,sizeof(how));if(fd<0)r=-errno;}
    struct stat object;if(!r && fstat(fd,&object))r=-errno;
    if(!r && (uint64_t)object.st_ino!=expected)r=-ESTALE;
    if(fd>=0)close(fd);
    close(root);return r;
}
static int service_snapshot(struct ae_login_service *s,struct service_observation *out) {
    int r=security_getenforce()==1?service_manager(s,false):-EACCES;
    uint64_t first=0,last=0;if(!r)r=service_identity(s,&first);
    sd_bus_message *reply=NULL;const char *path=NULL,*unit=NULL;const void *invocation=NULL;size_t size=0;
    if(!r)r=sd_bus_call_method(s->bus,"org.freedesktop.systemd1","/org/freedesktop/systemd1","org.freedesktop.systemd1.Manager","GetUnitByPIDFD",NULL,&reply,"h",s->peer);
    if(r>=0 && !sd_bus_message_has_signature(reply,"osay"))r=-EBADMSG;
    if(r>=0)r=sd_bus_message_read(reply,"os",&path,&unit);
    if(r==0)r=-EBADMSG;
    if(r>0 && (!unit || strcmp(unit,s->unit) || !path || strnlen(path,1024)==1024))r=-EACCES;
    if(r>0)r=sd_bus_message_read_array(reply,'y',&invocation,&size);
    if(r==0)r=-EBADMSG;
    if(r>0 && (size!=16 || !invocation))r=-EBADMSG;
    if(r>0){memcpy(out->invocation,invocation,16);uint8_t any=0;for(size_t i=0;i<16;++i)any|=out->invocation[i];if(!any)r=-EBADMSG;}
    if(r>0)r=service_properties(s,path,out->invocation,out->group);
    char *group=NULL;if(!r)r=sd_pidfd_get_cgroup(s->peer,&group);
    if(!r && (!group || strcmp(group,out->group)))r=-ESTALE;
    if(!r)r=service_group(out->group,first);
    if(!r)r=service_identity(s,&last);
    if(!r && first!=last)r=-ESTALE;
    if(!r)r=service_manager(s,false);
    free(group);sd_bus_message_unref(reply);if(!r)out->cgroup=first;return r<0?r:0;
}
static bool service_same(const struct service_observation *a,const struct service_observation *b) {return a->cgroup==b->cgroup && !memcmp(a->invocation,b->invocation,16) && !strcmp(a->group,b->group);}
static int service_owner(struct ae_login_service *s) {return !s?-EINVAL:getpid()!=s->owner?-ECHILD:gettid()!=s->thread?-EXDEV:s->busy?-EBUSY:0;}
int ae_login_service_check(void *data) {
    struct ae_login_service *s=data;int r=service_owner(s);if(r<0)return r;
    if(s->error)return s->error;
    s->busy=true;struct service_observation a={0},b={0};r=service_snapshot(s,&a);
    if(!r)r=service_snapshot(s,&b);
    if(!r && (!service_same(&a,&b) || !service_same(&a,&s->original)))r=-ESTALE;
    s->busy=false;if(r<0)s->error=r;return r;
}
int ae_login_service_peer(struct ae_login_service *s,int *out) {if(!out)return -EINVAL;int r=ae_login_service_check(s);if(!r)*out=s->peer;return r;}
int ae_login_service_close(struct ae_login_service **out) {
    if(!out)return -EINVAL;
    if(!*out)return 0;
    struct ae_login_service *s=*out;if(getpid()==s->owner){int r=service_owner(s);if(r<0)return r;}
    *out=NULL;if(s->peer>=0)close(s->peer);if(s->manager>=0)close(s->manager);sd_bus_unref(s->bus);free(s);return 0;
}
int ae_login_service_create(int fd,sd_bus *bus,const struct ae_login_service_expect *expected,struct ae_login_service **out) {
    if(fd<0 || !bus || !expected || !out || *out || !expected->unit || !expected->label || !expected->socket_label ||
       !*expected->socket_label || strnlen(expected->socket_label,512)==512)return -EINVAL;
    size_t n=strnlen(expected->unit,256);if(n<9 || n==256 || strcmp(expected->unit+n-8,".service"))return -EINVAL;
    if(sd_bus_is_bus_client(bus)<=0 || security_getenforce()!=1)return -EACCES;
    int kind=0;socklen_t size=sizeof(kind);if(getsockopt(fd,SOL_SOCKET,SO_DOMAIN,&kind,&size))return -errno;
    if(size!=sizeof(kind) || kind!=AF_UNIX)return -EPROTO;
    size=sizeof(kind);if(getsockopt(fd,SOL_SOCKET,SO_TYPE,&kind,&size))return -errno;
    if(size!=sizeof(kind) || kind!=SOCK_STREAM)return -EPROTO;
    struct sockaddr_un address;size=sizeof(address);if(getpeername(fd,(struct sockaddr *)&address,&size))return -errno;
    if(size>sizeof(address) || size<sizeof(address.sun_family) || address.sun_family!=AF_UNIX)return -EPROTO;
    struct ae_login_service *s=calloc(1,sizeof(*s));if(!s)return -ENOMEM;
    s->peer=s->manager=-1;s->owner=getpid();s->thread=gettid();s->bus=sd_bus_ref(bus);
    int r=service_text(s->unit,sizeof(s->unit),expected->unit);if(!r)r=service_text(s->label,sizeof(s->label),expected->label);if(r<0)goto failed;
    size=sizeof(s->credentials);if(getsockopt(fd,SOL_SOCKET,SO_PEERCRED,&s->credentials,&size)){r=-errno;goto failed;}
    if(size!=sizeof(s->credentials) || s->credentials.pid<=0 || s->credentials.uid!=expected->uid || s->credentials.gid!=expected->gid){r=-EACCES;goto failed;}
    char label[512]={0};size=sizeof(label);if(getsockopt(fd,SOL_SOCKET,SO_PEERSEC,label,&size)){r=-errno;goto failed;}
    if(size==0 || size>sizeof(label) || strnlen(label,sizeof(label))>=sizeof(label) || strcmp(label,expected->socket_label)){r=-ESTALE;goto failed;}
    size=sizeof(s->peer);if(getsockopt(fd,SOL_SOCKET,SO_PEERPIDFD,&s->peer,&size)){r=-errno;goto failed;}
    if(size!=sizeof(s->peer) || s->peer<0 || fcntl(s->peer,F_GETFD)<0 || !(fcntl(s->peer,F_GETFD)&FD_CLOEXEC)){r=-EPROTO;goto failed;}
    if(fstat(s->peer,&s->peer_object)){r=-errno;goto failed;}
    r=service_manager(s,true);struct service_observation a={0},b={0};
    if(!r)r=service_snapshot(s,&a);
    if(!r)r=service_snapshot(s,&b);
    if(!r && !service_same(&a,&b))r=-ESTALE;
    if(r<0)goto failed;
    s->original=a;*out=s;return 0;
failed:
    ae_login_service_close(&s);return r;
}
