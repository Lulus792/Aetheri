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
#include <sys/prctl.h>
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
static enum fault audio_fault;
static pid_t audio_pid;
static int audio_pin=-1,audio_group_fd;
static uint64_t audio_inode;
static const char *audio_group="/aetheri-controlled-user/app.slice/aetheri-pipewire.service";
static const char *audio_unit="aetheri-pipewire.service";
static bool audio_destination(const char *name) {return name && name[0]==':';}
static bool audio_descriptor(int fd) {
    struct stat a,b;return audio_pin>=0 && !fstat(fd,&a) && !fstat(audio_pin,&b) && a.st_dev==b.st_dev && a.st_ino==b.st_ino;
}
static pid_t actor;
static int actor_pin=-1,group_fd;
static pid_t test_owner;
static void retire_children(void) {
    if(getpid()!=test_owner) return;
    if(audio_pin>=0) {(void)syscall(SYS_pidfd_send_signal,audio_pin,SIGKILL,NULL,0);(void)waitpid(audio_pid,NULL,0);}
    if(actor_pin>=0) {(void)syscall(SYS_pidfd_send_signal,actor_pin,SIGKILL,NULL,0);(void)waitpid(actor,NULL,0);}
}
static uint64_t group_inode;
static unsigned observed_calls,mutation_calls;
static char credential;
static const char *manager_group="/aetheri-controlled-user/init.scope";

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
    (void)path;(void)interface;(void)error;(void)types;
    bool audio=audio_destination(destination);enum fault f=audio ? audio_fault : fault;
    if(f==BUS_ERROR) return -EIO;
    sd_bus_message *reply=response(bus);
    if(!strcmp(member,"GetNameOwner")) CHECK(sd_bus_message_append(reply,"s",fault==OWNER ? ":1.88" : ":1.77")>=0);
    else if(!strcmp(member,"GetUnitByPIDFD")) {
        ++observed_calls;char unit[64];snprintf(unit,sizeof(unit),"user@%u.service",(unsigned)getuid());
        uint8_t inv[16]={audio ? 2 : 1};if(f==EMPTY_INV) inv[0]=0;
        CHECK(sd_bus_message_append(reply,"os","/org/freedesktop/systemd1/unit/test",f==WRONG_UNIT ? "foreign.service" : audio ? audio_unit : unit)>=0);
        CHECK(sd_bus_message_append_array(reply,'y',inv,sizeof(inv))>=0);
    } else {++mutation_calls;CHECK(false);}
    return seal(reply,out);
}
static int model_string(sd_bus *bus,const char *destination,const char *path,const char *interface,const char *member,sd_bus_error *error,char **out) {
    (void)bus;(void)path;(void)interface;(void)error;
    bool audio=audio_destination(destination);enum fault f=audio ? audio_fault : fault;
    if(!strcmp(member,"ActiveState")) *out=strdup(f==INACTIVE ? "inactive" : "active");
    else {CHECK(!strcmp(member,"ControlGroup"));*out=strdup(audio ? audio_group : "/aetheri-controlled-user");}
    CHECK(*out);return 0;
}
static int model_trivial(sd_bus *bus,const char *destination,const char *path,const char *interface,const char *member,sd_bus_error *error,char type,void *out) {
    (void)bus;(void)path;(void)interface;(void)error;CHECK(type=='u');
    bool audio=audio_destination(destination);enum fault f=audio ? audio_fault : fault;
    if(!strcmp(member,"MainPID")) *(uint32_t *)out=(uint32_t)(audio ? audio_pid : actor)+(f==NOT_MAIN ? 1u : 0u);
    else {CHECK(!strcmp(member,"ControlPID"));*(uint32_t *)out=f==CONTROL ? 1u : 0u;}
    return 0;
}
static int model_property(sd_bus *bus,const char *destination,const char *path,const char *interface,const char *member,sd_bus_error *error,sd_bus_message **out,const char *type) {
    (void)path;(void)interface;(void)error;(void)type;
    bool audio=audio_destination(destination);enum fault f=audio ? audio_fault : fault;
    sd_bus_message *reply=response(bus);
    if(!strcmp(member,"InvocationID")) {uint8_t inv[16]={audio ? 2 : 1};if(f==DRIFT_INV) ++inv[0];CHECK(sd_bus_message_append_array(reply,'y',inv,sizeof(inv))>=0);}
    else {CHECK(!strcmp(member,"Job"));CHECK(sd_bus_message_append(reply,"(uo)",f==JOB ? 1u : 0u,"/")>=0);}
    return seal(reply,out);
}
static int model_creds(sd_bus *bus,const char *name,uint64_t mask,sd_bus_creds **out) {(void)bus;(void)mask;CHECK(!strcmp(name,":1.77") || fault==OWNER);*out=(sd_bus_creds *)&credential;return 0;}
static uint64_t model_mask(sd_bus_creds *c) {(void)c;return SD_BUS_CREDS_EUID|SD_BUS_CREDS_PID|(fault==MISSING ? 0u : SD_BUS_CREDS_PIDFD);}
static uint64_t model_augmented(sd_bus_creds *c) {(void)c;return fault==AUGMENTED ? SD_BUS_CREDS_PID : 0;}
static int model_euid(sd_bus_creds *c,uid_t *out) {(void)c;*out=getuid()+(fault==WRONG_UID ? 1u : 0u);return 0;}
static int model_pid(sd_bus_creds *c,pid_t *out) {(void)c;*out=actor+(fault==WRONG_PID ? 1 : 0);return 0;}
static int model_pin(sd_bus_creds *c,int *out) {(void)c;*out=fault==BAD_PIN ? open("/dev/null",O_RDONLY|O_CLOEXEC) : fcntl(actor_pin,F_DUPFD_CLOEXEC,3);return *out<0 ? -errno : 0;}
static sd_bus_creds *model_unref(sd_bus_creds *c) {(void)c;return NULL;}
static int model_group(int fd,char **out) {bool audio=audio_descriptor(fd);*out=strdup((audio ? audio_fault : fault)==WRONG_GROUP ? "/foreign" : audio ? audio_group : manager_group);CHECK(*out);return 0;}
static int model_bus_id(sd_bus *bus,sd_id128_t *out) {int r=sd_bus_get_bus_id(bus,out);if(!r && fault==BUS_ID) out->bytes[0]^=1;return r;}
static int model_ioctl(int fd,unsigned long request,...);
static long model_syscall(long number,...) {
    if(number==SYS_gettid) return syscall(SYS_gettid);
    CHECK(number==SYS_openat2);
    va_list args;va_start(args,number);int root=va_arg(args,int);const char *path=va_arg(args,const char *);
    struct open_how *how=va_arg(args,struct open_how *);size_t size=va_arg(args,size_t);va_end(args);
    bool audio=strcmp(path,manager_group+1) && !strcmp(path,audio_group+1);
    CHECK(root>=0 && (audio || !strcmp(path,manager_group+1)) && size==sizeof(*how));
    CHECK(how->resolve==(RESOLVE_BENEATH|RESOLVE_NO_SYMLINKS|RESOLVE_NO_MAGICLINKS|RESOLVE_NO_XDEV));
    return (audio ? audio_fault : fault)==GROUP_INODE ? open("/",O_RDONLY|O_DIRECTORY|O_CLOEXEC) : fcntl(audio ? audio_group_fd : group_fd,F_DUPFD_CLOEXEC,3);
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
    bool audio=audio_descriptor(fd);
    if((audio ? audio_fault : fault)==NO_INFO) {errno=ENOTTY;return -1;}
    int r=ioctl(fd,request,info);if(!r) info->cgroupid=audio ? audio_inode : group_inode;return r;
}
static int descriptors(void) {DIR *d=opendir("/proc/self/fd");CHECK(d);int n=0;while(readdir(d)) ++n;CHECK(!closedir(d));return n;}
static void actor_cases(struct ae_audio_manager *g,sd_bus *user,sd_bus *system,int image,int wrong) {
    int original=descriptors();audio_group_fd=open("/tmp",O_RDONLY|O_DIRECTORY|O_CLOEXEC);
    struct stat st;CHECK(audio_group_fd>=0 && !fstat(audio_group_fd,&st));audio_inode=(uint64_t)st.st_ino;
    audio_pid=fork();CHECK(audio_pid>=0);
    if(!audio_pid) {CHECK(!prctl(PR_SET_NO_NEW_PRIVS,1,0,0,0));execl("/usr/bin/sleep","sleep","30",(char *)NULL);_exit(125);}
    audio_pin=(int)syscall(SYS_pidfd_open,audio_pid,0);CHECK(audio_pin>=0);
    bool ready=false;for(unsigned i=0;i<500;++i) {
        char path[64];snprintf(path,sizeof(path),"/proc/%d/exe",audio_pid);struct stat actual,expected;
        if(!stat(path,&actual) && !fstat(image,&expected) && actual.st_dev==expected.st_dev && actual.st_ino==expected.st_ino) {ready=true;break;}usleep(1000);
    }
    CHECK(ready);
    char context[512];int label=open("/proc/self/attr/current",O_RDONLY|O_CLOEXEC);CHECK(label>=0);
    ssize_t n=read(label,context,sizeof(context)-1);CHECK(n>0 && n<(ssize_t)sizeof(context)-1);close(label);
    context[n]=0;context[strcspn(context,"\n")]=0;
    int baseline=descriptors();
    CHECK(!ae_audio_actor_open(NULL,audio_unit,audio_pin,image,context));
    CHECK(!ae_audio_actor_open(g,"../foreign.service",audio_pin,image,context));
    CHECK(!ae_audio_actor_open(g,"foreign.service",audio_pin,image,context));
    CHECK(!ae_audio_actor_open(g,audio_unit,-1,image,context));
    CHECK(!ae_audio_actor_open(g,audio_unit,audio_pin,wrong,context));
    CHECK(!ae_audio_actor_open(g,audio_unit,audio_pin,image,"foreign:context:s0"));
    CHECK(descriptors()==baseline && !ae_audio_manager_check(g));
    const enum fault faults[]={NO_INFO,WRONG_UNIT,NOT_MAIN,CONTROL,INACTIVE,EMPTY_INV,DRIFT_INV,JOB,WRONG_GROUP,GROUP_INODE,BUS_ERROR};
    for(unsigned i=0;i<sizeof(faults)/sizeof(faults[0]);++i) {
        audio_fault=faults[i];CHECK(!ae_audio_actor_open(g,audio_unit,audio_pin,image,context));
        audio_fault=NONE;CHECK(descriptors()==baseline && !ae_audio_manager_check(g));
    }
    const char *saved=audio_group;
    const char *foreign_groups[]={"/aetheri-controlled-user-other/audio.service","/aetheri-controlled-user/init.scope","/aetheri-controlled-user/init.scope/child"};
    for(unsigned i=0;i<3;++i) {audio_group=foreign_groups[i];CHECK(!ae_audio_actor_open(g,audio_unit,audio_pin,image,context));CHECK(!ae_audio_manager_check(g));}
    audio_group=saved;CHECK(descriptors()==baseline);
    struct ae_audio_actor *a=ae_audio_actor_open(g,audio_unit,audio_pin,image,context);CHECK(a);
    for(unsigned i=0;i<16;++i) CHECK(!ae_audio_actor_check(a));
    CHECK(descriptors()==baseline+2);
    audio_fault=DRIFT_INV;CHECK(ae_audio_actor_check(a)==-ESTALE);audio_fault=NONE;
    CHECK(ae_audio_actor_check(a)==-ESTALE && !ae_audio_manager_check(g));ae_audio_actor_close(&a);
    CHECK(!a && descriptors()==baseline);
    struct ae_audio_manager *retained=ae_audio_manager_open(user,system,getuid(),image);CHECK(retained);
    a=ae_audio_actor_open(retained,audio_unit,audio_pin,image,context);CHECK(a);
    ae_audio_manager_close(&retained);CHECK(!retained && !ae_audio_actor_check(a));
    fault=OWNER;CHECK(ae_audio_actor_check(a)==-ESTALE);fault=NONE;CHECK(ae_audio_actor_check(a)==-ESTALE);
    ae_audio_actor_close(&a);CHECK(descriptors()==baseline && !ae_audio_manager_check(g));
    a=ae_audio_actor_open(g,audio_unit,audio_pin,image,context);CHECK(a);
    pid_t child=fork();CHECK(child>=0);if(!child) _exit(ae_audio_actor_check(a)==-ECHILD ? 0 : 1);
    int status=0;CHECK(waitpid(child,&status,0)==child && WIFEXITED(status) && !WEXITSTATUS(status));
    CHECK(!ae_audio_actor_check(a));
    CHECK(!syscall(SYS_pidfd_send_signal,audio_pin,SIGKILL,NULL,0));CHECK(waitpid(audio_pid,&status,0)==audio_pid);
    CHECK(ae_audio_actor_check(a)==-ESRCH && !ae_audio_manager_check(g));
    ae_audio_actor_close(&a);ae_audio_actor_close(&a);CHECK(descriptors()==baseline);
    close(audio_pin);audio_pin=-1;close(audio_group_fd);CHECK(descriptors()==original);
}
int main(void) {
    test_owner=getpid();CHECK(!atexit(retire_children));
    alarm(25);sd_bus *user=NULL,*system=NULL;
    /* Real trusted system-bus clients supply message construction/IDs only.
     * Every subject owner/credential/PID1 call below is locally redirected;
     * nothing requests a host name, unit mutation or policy change. */
    CHECK(sd_bus_open_system(&user)>=0 && sd_bus_open_system(&system)>=0);
    CHECK(sd_bus_set_method_call_timeout(user,1000000)>=0 && sd_bus_set_method_call_timeout(system,1000000)>=0);
    int image=open("/usr/bin/sleep",O_PATH|O_CLOEXEC),wrong=open("/usr/bin/true",O_PATH|O_CLOEXEC);CHECK(image>=0 && wrong>=0);
    group_fd=open(".",O_RDONLY|O_DIRECTORY|O_CLOEXEC);struct stat st;CHECK(group_fd>=0 && !fstat(group_fd,&st));group_inode=(uint64_t)st.st_ino;
    const char *ordinary="CapInh:\t0000000000000000\nCapPrm:\t0000000000000000\nCapEff:\t0000000000000000\nCapAmb:\t0000000000000000\nNoNewPrivs:\t1\n";
    CHECK(!security_text(ordinary,strlen(ordinary)));
    for(unsigned field=0;field<5;++field) {
        char changed[256];CHECK(strlen(ordinary)<sizeof(changed));strcpy(changed,ordinary);
        const char *keys[]={"CapInh:\t","CapPrm:\t","CapEff:\t","CapAmb:\t","NoNewPrivs:\t"};
        char *value=strstr(changed,keys[field])+strlen(keys[field]);*value=field==4 ? '0' : '1';
        CHECK(security_text(changed,strlen(changed))==-EPERM);
    }
    CHECK(security_text(ordinary,strlen(ordinary)-1)==-EBADMSG);
    const char *missing="CapEff:\t0000000000000000\n";
    CHECK(security_text(missing,strlen(missing))==-ENODATA);
    char duplicated[256];snprintf(duplicated,sizeof(duplicated),"%sCapEff:\t0000000000000000\n",ordinary);
    CHECK(security_text(duplicated,strlen(duplicated))==-EBADMSG);
    CHECK(security_text("bad\0\n",5)==-EBADMSG);
    actor=fork();CHECK(actor>=0);if(!actor) {CHECK(!prctl(PR_SET_NO_NEW_PRIVS,1,0,0,0));execl("/usr/bin/sleep","sleep","30",(char *)NULL);_exit(125);}
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
    actor_cases(g,user,system,image,wrong);
    pid_t forked=fork();CHECK(forked>=0);if(!forked) _exit(ae_audio_manager_check(g)==-ECHILD ? 0 : 1);
    int status=0;CHECK(waitpid(forked,&status,0)==forked && WIFEXITED(status) && !WEXITSTATUS(status));
    CHECK(!ae_audio_manager_check(g));
    CHECK(!syscall(SYS_pidfd_send_signal,actor_pin,SIGKILL,NULL,0));CHECK(waitpid(actor,&status,0)==actor);
    CHECK(ae_audio_manager_check(g)==-ESRCH);ae_audio_manager_close(&g);ae_audio_manager_close(&g);
    CHECK(!mutation_calls && observed_calls>32 && descriptors()==baseline);
    close(actor_pin);actor_pin=-1;close(group_fd);close(image);close(wrong);sd_bus_flush_close_unref(user);sd_bus_flush_close_unref(system);
    puts("audio manager/actors: original pins/images/contexts, controlled system/user units, latched owner/invocation loss, subtree/FD lifetime, fork/exit and retirement passed");return 0;
}
