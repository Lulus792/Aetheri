/* Explicit root-only registered-carrier test, never a default host test.
 * The isolated actor drops to UID1000 and changes only its own CAP_WAKE_ALARM.
 * Same kernel process, UID/GID, dumpability, NNP and SELinux label throughout.
 * No ptrace, policy mutation, systemd unit, device or original manager change. */
#include "../src/audio/manager.c"
#include <grp.h>
#include <linux/capability.h>
#include <signal.h>
#include <sys/prctl.h>
#include <sys/wait.h>

#define CHECK(x) do {if(!(x)) {fprintf(stderr,"manager security line %d: %s errno=%d\n",__LINE__,#x,errno);exit(1);}} while(0)
static void capabilities(bool enabled) {
    struct __user_cap_header_struct header={.version=_LINUX_CAPABILITY_VERSION_3,.pid=0};
    struct __user_cap_data_struct data[2]={{0},{0}};
    if(enabled) data[CAP_WAKE_ALARM/32].effective=data[CAP_WAKE_ALARM/32].permitted=1u<<(CAP_WAKE_ALARM%32);
    CHECK(!syscall(SYS_capset,&header,data));
    memset(data,0,sizeof(data));CHECK(!syscall(SYS_capget,&header,data));
    uint32_t expected=enabled ? 1u<<(CAP_WAKE_ALARM%32) : 0u;
    CHECK(!data[0].effective && !data[0].permitted && !data[0].inheritable && !data[1].inheritable && data[1].effective==expected && data[1].permitted==expected);
}
static void drop(bool enabled) {
    CHECK(!setgroups(0,NULL) && !prctl(PR_SET_KEEPCAPS,1,0,0,0));
    CHECK(!setresgid(1000,1000,1000) && !setresuid(1000,1000,1000));
    capabilities(enabled);
    CHECK(!prctl(PR_SET_KEEPCAPS,0,0,0,0) && !prctl(PR_SET_NO_NEW_PRIVS,1,0,0,0));
    CHECK(!prctl(PR_SET_DUMPABLE,1,0,0,0));
}
static void transfer(int fd,char value) {CHECK(write(fd,&value,1)==1);}
static void receive(int fd,char expected) {char value=0;CHECK(read(fd,&value,1)==1 && value==expected);}
static void own_label(char output[512]) {
    int fd=open("/proc/self/attr/current",O_RDONLY|O_CLOEXEC);CHECK(fd>=0);
    ssize_t n=read(fd,output,511);CHECK(n>0 && n<511 && !close(fd));output[n]=0;
}
int main(void) {
    alarm(25);CHECK(getuid()==0 && geteuid()==0);
    FILE *file=fopen("/sys/class/dmi/id/product_name","r");char name[128];
    CHECK(file && fgets(name,sizeof(name),file) && !fclose(file) && !strcmp(name,"Aetheri P0 disposable VM\n"));
    file=fopen("/sys/fs/selinux/enforce","r");CHECK(file && fgetc(file)=='1' && !fclose(file));
    int requests[2],replies[2];CHECK(!pipe2(requests,O_CLOEXEC) && !pipe2(replies,O_CLOEXEC));
    pid_t target=fork();CHECK(target>=0);
    if(!target) {
        close(requests[1]);close(replies[0]);drop(true);
        char original[512],current[512];own_label(original);
        transfer(replies[1],'p');receive(requests[0],'c');capabilities(false);
        CHECK(prctl(PR_GET_DUMPABLE,0,0,0,0)==1 && prctl(PR_GET_NO_NEW_PRIVS,0,0,0,0)==1);
        own_label(current);CHECK(!strcmp(original,current));
        transfer(replies[1],'z');receive(requests[0],'q');_exit(0);
    }
    close(requests[0]);close(replies[1]);int pin=(int)syscall(SYS_pidfd_open,target,0);CHECK(pin>=0);
    struct stat before,after;struct pidfd_info info={.mask=PIDFD_INFO_PID|PIDFD_INFO_CREDS|PIDFD_INFO_CGROUPID};
    receive(replies[0],'p');CHECK(!ioctl(pin,PIDFD_GET_INFO,&info) && info.pid==(uint32_t)target && info.euid==1000 && !fstat(pin,&before));
    for(unsigned phase=0;phase<2;++phase) {
        if(phase) {transfer(requests[1],'c');receive(replies[0],'z');}
        pid_t reader=fork();CHECK(reader>=0);
        if(!reader) {
            close(requests[1]);close(replies[0]);drop(false);
            CHECK(!live(pin));
            int profile=security_context((uint32_t)target);
            char path[64];snprintf(path,sizeof(path),"/proc/%d/exe",target);
            int fd=open(path,O_PATH|O_CLOEXEC),error=errno;
            if(!phase) CHECK(profile==-EPERM && fd<0 && error==EACCES);
            else CHECK(!profile && fd>=0);
            if(fd>=0) close(fd);
            CHECK(!live(pin));_exit(0);
        }
        int status=0;CHECK(waitpid(reader,&status,0)==reader && WIFEXITED(status) && !WEXITSTATUS(status));
        CHECK(!fstat(pin,&after) && same_object(&before,&after));
        printf("AETHERI_AUDIO_MANAGER_SECURITY phase=%s profile=%s image=%s same_original=1 uid=1000 nnp=1 dumpable=1 enforcing=1\n",phase ? "ordinary" : "wake-alarm",phase ? "accepted" : "denied",phase ? "allowed" : "denied");
    }
    transfer(requests[1],'q');int status=0;CHECK(waitpid(target,&status,0)==target && WIFEXITED(status) && !WEXITSTATUS(status));
    CHECK(live(pin)==-ESRCH);close(pin);close(requests[1]);close(replies[0]);
    puts("AETHERI_AUDIO_MANAGER_SECURITY_PASS original_exit=0 no_ptrace=1 policy_changed=0");return 0;
}
