#define _GNU_SOURCE
#include "audio/manager.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/openat2.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <systemd/sd-login.h>
#include <unistd.h>

#define CHECK(x) do {if(!(x)) {fprintf(stderr,"manager line %d: %s errno=%d\n",__LINE__,#x,errno);exit(1);}} while(0)
/* Controlled bus/PID1/cgroup facts, real bus objects, process pins, current
 * executable observations and descriptor retirement. Actual trusted-bus and
 * user@ integration is separately tested in the cold-boot PAM image. Redirect
 * only this included subject, never the sanitizer/runtime's libc operations. */
enum fault {NONE,MISSING,AUGMENTED,WRONG_UID,WRONG_PID,BAD_PIN,NO_INFO,WRONG_UNIT,
    NOT_MAIN,CONTROL,INACTIVE,EMPTY_INV,DRIFT_INV,JOB,WRONG_GROUP,GROUP_INODE,OWNER,BUS_ID,BUS_ERROR};
static enum fault fault;
static pid_t actor;
static int actor_pin,group_fd;
static uint64_t group_inode;
static unsigned observed_calls,mutation_calls;
static char credential;
static const char *group="/aetheri-controlled-user/init.scope";

static sd_bus_message *response(sd_bus *bus) {
    sd_bus_message *call=NULL,*reply=NULL;
    CHECK(sd_bus_message_new_method_call(bus,&call,"org.aetheri.Test","/org/aetheri/Test","org.aetheri.Test1","Read")>=0);
    CHECK(sd_bus_message_seal(call,1,0)>=0 && sd_bus_message_new_method_return(call,&reply)>=0);
    sd_bus_message_unref(call);return reply;
}
static int seal(sd_bus_message *reply,sd_bus_message **out) {
    CHECK(sd_bus_message_seal(reply,2,0)>=0 && sd_bus_message_rewind(reply,1)>=0);*out=reply;return 1;
}
static int model_call(sd_bus *bus,const char *destination,const char *path,const char *interface,const char *member,
    sd_bus_error *error,sd_bus_message **out,const char *types,...) {
    (void)destination;(void)path;(void)interface;(void)error;(void)types;
    if(fault==BUS_ERROR) return -EIO;
    sd_bus_message *reply=response(bus);
    if(!strcmp(member,"GetNameOwner")) CHECK(sd_bus_message_append(reply,"s",fault==OWNER ? ":1.88" : ":1.77")>=0);
    else if(!strcmp(member,"GetUnitByPIDFD")) {
        ++observed_calls;char unit[64];snprintf(unit,sizeof(unit),"user@%u.service",(unsigned)getuid());
        uint8_t inv[16]={1};if(fault==EMPTY_INV) inv[0]=0;
        CHECK(sd_bus_message_append(reply,"os","/org/freedesktop/systemd1/unit/test",fault==WRONG_UNIT ? "foreign.service" : unit)>=0);
        CHECK(sd_bus_message_append_array(reply,'y',inv,sizeof(inv))>=0);
    } else {++mutation_calls;CHECK(false);}
    return seal(reply,out);
}
static int model_string(sd_bus *bus,const char *destination,const char *path,const char *interface,const char *member,sd_bus_error *error,char **out) {
    (void)bus;(void)destination;(void)path;(void)interface;(void)error;
    if(!strcmp(member,"ActiveState")) *out=strdup(fault==INACTIVE ? "inactive" : "active");
    else {CHECK(!strcmp(member,"ControlGroup"));*out=strdup("/aetheri-controlled-user");}
    CHECK(*out);return 0;
}
static int model_trivial(sd_bus *bus,const char *destination,const char *path,const char *interface,const char *member,sd_bus_error *error,char type,void *out) {
    (void)bus;(void)destination;(void)path;(void)interface;(void)error;CHECK(type=='u');
    if(!strcmp(member,"MainPID")) *(uint32_t *)out=(uint32_t)actor+(fault==NOT_MAIN ? 1u : 0u);
    else {CHECK(!strcmp(member,"ControlPID"));*(uint32_t *)out=fault==CONTROL ? 1u : 0u;}
    return 0;
}
static int model_property(sd_bus *bus,const char *destination,const char *path,const char *interface,const char *member,sd_bus_error *error,sd_bus_message **out,const char *type) {
    (void)destination;(void)path;(void)interface;(void)error;(void)type;
    sd_bus_message *reply=response(bus);
    if(!strcmp(member,"InvocationID")) {uint8_t inv[16]={1};if(fault==DRIFT_INV) inv[0]=2;CHECK(sd_bus_message_append_array(reply,'y',inv,sizeof(inv))>=0);}
    else {CHECK(!strcmp(member,"Job"));CHECK(sd_bus_message_append(reply,"(uo)",fault==JOB ? 1u : 0u,"/")>=0);}
    return seal(reply,out);
}
static int model_creds(sd_bus *bus,const char *name,uint64_t mask,sd_bus_creds **out) {(void)bus;(void)mask;CHECK(!strcmp(name,":1.77") || fault==OWNER);*out=(sd_bus_creds *)&credential;return 0;}
static uint64_t model_mask(sd_bus_creds *c) {(void)c;return SD_BUS_CREDS_EUID|SD_BUS_CREDS_PID|(fault==MISSING ? 0u : SD_BUS_CREDS_PIDFD);}
static uint64_t model_augmented(sd_bus_creds *c) {(void)c;return fault==AUGMENTED ? SD_BUS_CREDS_PID : 0;}
static int model_euid(sd_bus_creds *c,uid_t *out) {(void)c;*out=getuid()+(fault==WRONG_UID ? 1u : 0u);return 0;}
static int model_pid(sd_bus_creds *c,pid_t *out) {(void)c;*out=actor+(fault==WRONG_PID ? 1 : 0);return 0;}
static int model_pin(sd_bus_creds *c,int *out) {(void)c;*out=fault==BAD_PIN ? open("/dev/null",O_RDONLY|O_CLOEXEC) : fcntl(actor_pin,F_DUPFD_CLOEXEC,3);return *out<0 ? -errno : 0;}
static sd_bus_creds *model_unref(sd_bus_creds *c) {(void)c;return NULL;}
static int model_group(int fd,char **out) {(void)fd;*out=strdup(fault==WRONG_GROUP ? "/foreign" : group);CHECK(*out);return 0;}
static int model_bus_id(sd_bus *bus,sd_id128_t *out) {int r=sd_bus_get_bus_id(bus,out);if(!r && fault==BUS_ID) out->bytes[0]^=1;return r;}
static int model_ioctl(int fd,unsigned long request,...);
static long model_syscall(long number,...) {
    if(number==SYS_gettid) return syscall(SYS_gettid);
    CHECK(number==SYS_openat2);
    va_list args;va_start(args,number);int root=va_arg(args,int);const char *path=va_arg(args,const char *);
    struct open_how *how=va_arg(args,struct open_how *);size_t size=va_arg(args,size_t);va_end(args);
    CHECK(root>=0 && !strcmp(path,group+1) && size==sizeof(*how));
    CHECK(how->resolve==(RESOLVE_BENEATH|RESOLVE_NO_SYMLINKS|RESOLVE_NO_MAGICLINKS|RESOLVE_NO_XDEV));
    return fault==GROUP_INODE ? open("/",O_RDONLY|O_DIRECTORY|O_CLOEXEC) : fcntl(group_fd,F_DUPFD_CLOEXEC,3);
}
#define sd_bus_call_method model_call
#define sd_bus_get_property_string model_string
#define sd_bus_get_property_trivial model_trivial
#define sd_bus_get_property model_property
#define sd_bus_get_name_creds model_creds
#define sd_bus_creds_get_mask model_mask
#define sd_bus_creds_get_augmented_mask model_augmented
#define sd_bus_creds_get_euid model_euid
#define sd_bus_creds_get_pid model_pid
#define sd_bus_creds_get_pidfd_dup model_pin
#define sd_bus_creds_unref model_unref
#define sd_pidfd_get_cgroup model_group
#define sd_bus_get_bus_id model_bus_id
#define ioctl model_ioctl
#define syscall model_syscall
#include "../src/audio/manager.c"
#undef sd_bus_call_method
#undef sd_bus_get_property_string
#undef sd_bus_get_property_trivial
#undef sd_bus_get_property
#undef sd_bus_get_name_creds
#undef sd_bus_creds_get_mask
#undef sd_bus_creds_get_augmented_mask
#undef sd_bus_creds_get_euid
#undef sd_bus_creds_get_pid
#undef sd_bus_creds_get_pidfd_dup
#undef sd_bus_creds_unref
#undef sd_pidfd_get_cgroup
#undef sd_bus_get_bus_id
#undef ioctl
#undef syscall

static int model_ioctl(int fd,unsigned long request,...) {
    va_list args;va_start(args,request);struct pidfd_info *info=va_arg(args,struct pidfd_info *);va_end(args);CHECK(request==PIDFD_GET_INFO);
    if(fault==NO_INFO) {errno=ENOTTY;return -1;}
    int r=ioctl(fd,request,info);if(!r) info->cgroupid=group_inode;return r;
}
static int descriptors(void) {DIR *d=opendir("/proc/self/fd");CHECK(d);int n=0;while(readdir(d)) ++n;CHECK(!closedir(d));return n;}
int main(void) {
    alarm(25);sd_bus *user=NULL,*system=NULL;
    /* Real trusted system-bus clients supply message construction/IDs only.
     * Every subject owner/credential/PID1 call below is locally redirected;
     * nothing requests a host name, unit mutation or policy change. */
    CHECK(sd_bus_open_system(&user)>=0 && sd_bus_open_system(&system)>=0);
    CHECK(sd_bus_set_method_call_timeout(user,1000000)>=0 && sd_bus_set_method_call_timeout(system,1000000)>=0);
    int image=open("/usr/bin/sleep",O_PATH|O_CLOEXEC),wrong=open("/usr/bin/true",O_PATH|O_CLOEXEC);CHECK(image>=0 && wrong>=0);
    group_fd=open(".",O_RDONLY|O_DIRECTORY|O_CLOEXEC);struct stat st;CHECK(group_fd>=0 && !fstat(group_fd,&st));group_inode=(uint64_t)st.st_ino;
    actor=fork();CHECK(actor>=0);if(!actor) {execl("/usr/bin/sleep","sleep","30",(char *)NULL);_exit(125);}
    actor_pin=(int)syscall(SYS_pidfd_open,actor,0);CHECK(actor_pin>=0);
    bool ready=false;for(unsigned i=0;i<500;++i) {char path[64];snprintf(path,sizeof(path),"/proc/%d/exe",actor);struct stat actual,expected;
        if(!stat(path,&actual) && !fstat(image,&expected) && actual.st_dev==expected.st_dev && actual.st_ino==expected.st_ino) {ready=true;break;}usleep(1000);}
    CHECK(ready);int baseline=descriptors();
    CHECK(!ae_audio_manager_open(NULL,system,getuid(),image));
    CHECK(!ae_audio_manager_open(user,user,getuid(),image));
    CHECK(!ae_audio_manager_open(user,system,getuid(),wrong));
    CHECK(descriptors()==baseline);
    for(unsigned f=MISSING;f<=BUS_ERROR;++f) {
        if(f==OWNER || f==BUS_ID) continue; /* Changes need an original baseline. */
        fault=(enum fault)f;CHECK(!ae_audio_manager_open(user,system,getuid(),image));CHECK(descriptors()==baseline);
    }
    fault=NONE;struct ae_audio_manager *g=ae_audio_manager_open(user,system,getuid(),image);CHECK(g);
    CHECK(!strcmp(ae_audio_manager_name(g),":1.77"));
    for(unsigned i=0;i<32;++i) CHECK(!ae_audio_manager_check(g));
    CHECK(descriptors()==baseline+1);
    fault=OWNER;CHECK(ae_audio_manager_check(g)==-ESTALE);fault=NONE;
    CHECK(ae_audio_manager_check(g)==-ESTALE);ae_audio_manager_close(&g);CHECK(!g && descriptors()==baseline);
    g=ae_audio_manager_open(user,system,getuid(),image);CHECK(g);fault=BUS_ID;CHECK(ae_audio_manager_check(g)==-ESTALE);fault=NONE;CHECK(ae_audio_manager_check(g)==-ESTALE);ae_audio_manager_close(&g);
    g=ae_audio_manager_open(user,system,getuid(),image);CHECK(g);fault=DRIFT_INV;CHECK(ae_audio_manager_check(g)==-ESTALE);fault=NONE;CHECK(ae_audio_manager_check(g)==-ESTALE);ae_audio_manager_close(&g);
    g=ae_audio_manager_open(user,system,getuid(),image);CHECK(g);
    pid_t forked=fork();CHECK(forked>=0);if(!forked) _exit(ae_audio_manager_check(g)==-ECHILD ? 0 : 1);
    int status=0;CHECK(waitpid(forked,&status,0)==forked && WIFEXITED(status) && !WEXITSTATUS(status));
    CHECK(!ae_audio_manager_check(g));
    CHECK(!syscall(SYS_pidfd_send_signal,actor_pin,SIGKILL,NULL,0));CHECK(waitpid(actor,&status,0)==actor);
    CHECK(ae_audio_manager_check(g)==-ESRCH);ae_audio_manager_close(&g);ae_audio_manager_close(&g);
    CHECK(!mutation_calls && observed_calls>32 && descriptors()==baseline);
    close(actor_pin);close(group_fd);close(image);close(wrong);sd_bus_flush_close_unref(user);sd_bus_flush_close_unref(system);
    puts("audio manager: original pin/image, controlled PID1/credentials/cgroup faults, latched owner/bus/invocation loss, fork/exit and FD retirement passed");return 0;
}
