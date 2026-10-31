#include "linux/lifetime.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <selinux/selinux.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"process identity line %d: %s errno=%d\n",__LINE__,#x,errno); exit(1); } } while (0)
#define OBJECT "/org/aetheri/TestIdentity"
#define IFACE "org.aetheri.TestIdentity1"
/* Local test envelope only. AETHPID1 is the only portable record here. */
struct record { struct ae_process_context context; uint8_t wire[32]; char unique[256]; pid_t pid; };
struct observer { sd_bus *system; struct record record; struct ae_lifetime *life; int second,done,rejected; const char *work; };
static void guard(void)
{
    CHECK(getuid()==0 && geteuid()==0 && security_getenforce()==1);
    struct stat st; CHECK(stat("/sys/firmware/efi",&st)==0 && S_ISDIR(st.st_mode));
    CHECK(lstat("/etc/aetheri-p0-carrier",&st)==0 && S_ISREG(st.st_mode) && st.st_uid==0 && st.st_nlink==1 && !(st.st_mode&0022));
    FILE *f=fopen("/sys/class/dmi/id/product_name","r"); char name[128]={0};
    CHECK(f && fgets(name,sizeof name,f) && fclose(f)==0 && strcmp(name,"Aetheri P0 disposable VM\n")==0);
}
static int descriptors(void)
{ DIR *d=opendir("/proc/self/fd"); CHECK(d); int count=0; while (readdir(d)) ++count; CHECK(closedir(d)==0); return count; }
static void record_io(const char *work,struct record *record,int write_record)
{
    int directory=open(work,O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW); CHECK(directory>=0);
    int fd=openat(directory,"process.record",(write_record ? O_WRONLY|O_CREAT|O_EXCL : O_RDONLY)|O_CLOEXEC|O_NOFOLLOW,0600); CHECK(fd>=0);
    struct stat st; CHECK(fstat(fd,&st)==0 && S_ISREG(st.st_mode) && st.st_uid==0 && st.st_nlink==1 && (st.st_mode&0777)==0600);
    if (write_record) CHECK(write(fd,record,sizeof *record)==sizeof *record && fsync(fd)==0 && fsync(directory)==0);
    else { char trailing; CHECK(st.st_size==(off_t)sizeof *record && read(fd,record,sizeof *record)==sizeof *record && read(fd,&trailing,1)==0); }
    CHECK(close(fd)==0 && close(directory)==0);
}
static int observe(sd_bus_message *m,void *userdata,sd_bus_error *error)
{
    struct observer *o=userdata; uint32_t impersonate=0; CHECK(sd_bus_message_read(m,"u",&impersonate)>0 && !o->done);
    struct ae_peer peer=AE_PEER_INIT; struct ae_process_context context;
    CHECK(ae_peer_capture(m,&peer)==0 && ae_process_observe(&peer,o->system,&context)==0);
    struct ae_process_identity identity;
    if (!o->second) {
        CHECK(!impersonate && ae_process_identify(&peer,o->system,&context,&identity)==0);
        o->record.context=context; o->record.pid=peer.connection_pid; strcpy(o->record.unique,peer.unique_name);
        CHECK(ae_process_identity_encode(&identity,o->record.wire)==0);
        record_io(o->work,&o->record,1); o->done=1;
    } else {
        CHECK(ae_process_identity_decode(o->record.wire,32,&identity)==0);
        CHECK(ae_process_context_equal(&context,&o->record.context));
        int r=ae_process_reidentify(&identity,&peer,o->system,&o->record.context);
        if (impersonate) {
            CHECK(peer.connection_pid!=o->record.pid && r==-ESTALE); ++o->rejected;
            ae_peer_clear(&peer); return sd_bus_error_set_const(error,"org.aetheri.TestIdentity1.DifferentProcess","original kernel process required");
        }
        CHECK(r==0 && o->rejected==1 && peer.connection_pid==o->record.pid && strcmp(peer.unique_name,o->record.unique)!=0);
        struct ae_process_identity wrong=identity; wrong.pidfd_id^=1U;
        CHECK(ae_process_reidentify(&wrong,&peer,o->system,&context)==-ESTALE); wrong=identity; wrong.boot[0]^=1U;
        CHECK(ae_process_reidentify(&wrong,&peer,o->system,&context)==-ESTALE);
        struct ae_process_context drift=context; drift.invocation[0]^=1U;
        CHECK(ae_process_reidentify(&identity,&peer,o->system,&drift)==-ESTALE);
        int baseline=descriptors();
        for (int n=0;n<8;++n) CHECK(ae_process_reidentify(&identity,&peer,o->system,&context)==0);
        CHECK(descriptors()==baseline);
        CHECK(sd_bus_call_method(o->system,"org.freedesktop.systemd1","/org/freedesktop/systemd1","org.freedesktop.systemd1.Manager","RefUnit",NULL,NULL,"s",context.unit)>=0);
        CHECK(ae_lifetime_capture(o->system,&peer,&context,&o->life)==0); o->done=1;
    }
    ae_peer_clear(&peer); return sd_bus_reply_method_return(m,"");
}
static const sd_bus_vtable observation[]={SD_BUS_VTABLE_START(0),SD_BUS_METHOD("Observe","u","",observe,SD_BUS_VTABLE_UNPRIVILEGED),SD_BUS_VTABLE_END};
static void observer(const char *work,int second,int ready)
{
    struct observer o={.second=second,.work=work};
    if (second) record_io(work,&o.record,0);
    sd_bus *transport=NULL; CHECK(sd_bus_open_system(&o.system)>=0 && sd_bus_open_user(&transport)>=0);
    CHECK(sd_bus_set_method_call_timeout(o.system,2000000)>=0 && sd_bus_add_object_vtable(transport,NULL,OBJECT,IFACE,observation,&o)>=0);
    const char *unique=NULL; char name[256]={0}; CHECK(sd_bus_get_unique_name(transport,&unique)>=0 && strlen(unique)<sizeof name); strcpy(name,unique);
    CHECK(write(ready,name,sizeof name)==sizeof name && close(ready)==0);
    for (int n=0;!o.done && n<100;++n) { int r=sd_bus_process(transport,NULL); CHECK(r>=0); if (!r) CHECK(sd_bus_wait(transport,200000)>=0); }
    CHECK(o.done && sd_bus_flush(transport)>=0);
    if (second) {
        struct ae_lifetime_end ended; int r=-EAGAIN;
        for (int n=0;n<300 && r==-EAGAIN;++n) { r=ae_lifetime_observe_end(o.life,&ended); if (r==-EAGAIN) usleep(10000); }
        CHECK(r==0 && ended.pid==(uint32_t)o.record.pid && ended.cgroup_id==o.record.context.cgroup_id);
        CHECK(ae_lifetime_close(&o.life)==0 && sd_bus_call_method(o.system,"org.freedesktop.systemd1","/org/freedesktop/systemd1","org.freedesktop.systemd1.Manager","UnrefUnit",NULL,NULL,"s",o.record.context.unit)>=0);
    }
    sd_bus_flush_close_unref(transport); sd_bus_flush_close_unref(o.system);
}
static int locate(sd_bus_message *m,void *userdata,sd_bus_error *error)
{ (void)error; return sd_bus_reply_method_return(m,"s",(char *)userdata); }
static const sd_bus_vtable location[]={SD_BUS_VTABLE_START(0),SD_BUS_METHOD("Locate","","s",locate,SD_BUS_VTABLE_UNPRIVILEGED),SD_BUS_VTABLE_END};
static void destination(const char *controller,char out[256])
{
    sd_bus *bus=NULL; CHECK(sd_bus_open_user(&bus)>=0); int found=0;
    for (int n=0;!found && n<200;++n) {
        sd_bus_message *reply=NULL; const char *name=NULL;
        CHECK(sd_bus_call_method(bus,controller,OBJECT,IFACE,"Locate",NULL,&reply,"")>=0 && sd_bus_message_read(reply,"s",&name)>0 && strlen(name)<256);
        if (*name) { strcpy(out,name); found=1; }
        sd_bus_message_unref(reply); if (!found) usleep(10000);
    }
    CHECK(found); sd_bus_flush_close_unref(bus);
}
static void send_observation(const char *destination,int impersonate)
{
    sd_bus *bus=NULL; CHECK(sd_bus_open_user(&bus)>=0); sd_bus_error error=SD_BUS_ERROR_NULL;
    int r=sd_bus_call_method(bus,destination,OBJECT,IFACE,"Observe",&error,NULL,"u",(uint32_t)impersonate);
    CHECK(impersonate ? r<0 && sd_bus_error_has_name(&error,"org.aetheri.TestIdentity1.DifferentProcess") : r>=0);
    sd_bus_error_free(&error); sd_bus_flush_close_unref(bus);
}
static void client(const char *controller)
{
    char first[256],second[256]; destination(controller,first); send_observation(first,0);
    for (int n=0;n<200;++n) { destination(controller,second); if (strcmp(first,second)!=0) break; usleep(10000); }
    CHECK(strcmp(first,second)!=0);
    pid_t child=fork(); CHECK(child>=0);
    if (child==0) { send_observation(second,1); _exit(0); }
    int status; CHECK(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==0);
    send_observation(second,0);
}
static pid_t spawn_observer(const char *binary,const char *work,int second,char destination[256])
{
    int ready[2]; CHECK(pipe(ready)==0); pid_t pid=fork(); CHECK(pid>=0);
    if (pid==0) { CHECK(close(ready[0])==0); char fd[32]; CHECK(snprintf(fd,sizeof fd,"%d",ready[1])>0);
        execl(binary,binary,"--observer",work,second ? "2" : "1",fd,(char *)NULL); _exit(127); }
    CHECK(close(ready[1])==0); struct pollfd p={.fd=ready[0],.events=POLLIN};
    CHECK(poll(&p,1,5000)==1 && read(ready[0],destination,256)==256 && destination[0]==':' && memchr(destination,0,256));
    CHECK(close(ready[0])==0); return pid;
}
static void wait_child(sd_bus *bus,pid_t child)
{
    int status=0,done=0;
    for (int n=0;!done && n<300;++n) {
        pid_t result=waitpid(child,&status,WNOHANG); CHECK(result>=0); if (result==child) { done=1; break; }
        int r=sd_bus_process(bus,NULL); CHECK(r>=0); if (!r) CHECK(sd_bus_wait(bus,20000)>=0);
    }
    CHECK(done && WIFEXITED(status) && WEXITSTATUS(status)==0);
}
static void codec(const char *path)
{
    uint8_t raw[33],encoded[32],sentinel[32]; memset(sentinel,0xa5,sizeof sentinel);
    FILE *f=fopen(path,"rb"); CHECK(f && fread(raw,1,32,f)==32 && fgetc(f)==EOF && fclose(f)==0);
    struct ae_process_identity v,marker; memset(&marker,0xa5,sizeof marker);
    CHECK(ae_process_identity_decode(raw,32,&v)==0 && v.pidfd_id==UINT64_MAX);
    CHECK(ae_process_identity_encode(&v,encoded)==0 && memcmp(raw,encoded,32)==0);
    for (size_t n=0;n<32;++n) { v=marker; CHECK(ae_process_identity_decode(raw,n,&v)==-EBADMSG && memcmp(&v,&marker,sizeof v)==0); }
    v=marker; CHECK(ae_process_identity_decode(raw,33,&v)==-EBADMSG && memcmp(&v,&marker,sizeof v)==0);
    raw[0]^=1U; CHECK(ae_process_identity_decode(raw,32,&v)==-EBADMSG); raw[0]^=1U;
    uint8_t copy[32]; memcpy(copy,raw,32); memset(copy+8,0,16); CHECK(ae_process_identity_decode(copy,32,&v)==-EBADMSG);
    memcpy(copy,raw,32); memset(copy+24,0,8); CHECK(ae_process_identity_decode(copy,32,&v)==-EBADMSG);
    CHECK(ae_process_identity_decode(raw,32,&v)==0); v.pidfd_id=0; memcpy(encoded,sentinel,32);
    CHECK(ae_process_identity_encode(&v,encoded)==-EBADMSG && memcmp(encoded,sentinel,32)==0);
    CHECK(ae_process_identity_encode(NULL,encoded)==-EINVAL && ae_process_identity_decode(NULL,32,&v)==-EINVAL);
}
int main(int argc,char **argv)
{
    alarm(60); guard();
    if (argc==3 && strcmp(argv[1],"--codec")==0) { codec(argv[2]); return 0; }
    if (argc==3 && strcmp(argv[1],"--client")==0) { client(argv[2]); return 0; }
    if (argc==5 && strcmp(argv[1],"--observer")==0) { observer(argv[2],strcmp(argv[3],"2")==0,atoi(argv[4])); return 0; }
    CHECK(argc==2 && argv[0][0]=='/'); int baseline=descriptors(); char next[256]={0};
    sd_bus *transport=NULL,*system=NULL; CHECK(sd_bus_open_user(&transport)>=0 && sd_bus_open_system(&system)>=0);
    CHECK(sd_bus_add_object_vtable(transport,NULL,OBJECT,IFACE,location,next)>=0);
    const char *controller=NULL; CHECK(sd_bus_get_unique_name(transport,&controller)>=0);
    pid_t first=spawn_observer(argv[0],argv[1],0,next);
    sd_id128_t id; char digits[33],unit[256],unit_option[300],label_option[600],bus_option[4096];
    CHECK(sd_id128_randomize(&id)==0); sd_id128_to_string(id,digits);
    CHECK(snprintf(unit,sizeof unit,"aetheri-reidentify-test-%s.service",digits)>0 && snprintf(unit_option,sizeof unit_option,"--unit=%s",unit)>0);
    char *label=NULL; CHECK(getcon_raw(&label)==0 && snprintf(label_option,sizeof label_option,"--property=SELinuxContext=%s",label)>0); freecon(label);
    const char *address=getenv("DBUS_SESSION_BUS_ADDRESS"); CHECK(address && snprintf(bus_option,sizeof bus_option,"--setenv=DBUS_SESSION_BUS_ADDRESS=%s",address)>0 && strlen(bus_option)<sizeof bus_option-1);
    printf("Owned process reidentification fixture: %s\n",unit); fflush(stdout);
    pid_t runner=fork(); CHECK(runner>=0);
    if (runner==0) { execlp("systemd-run","systemd-run",unit_option,"--wait","--collect","--service-type=exec","--property=NoNewPrivileges=yes","--property=KillMode=control-group","--property=SendSIGKILL=no","--property=TimeoutStopSec=2s","--property=RuntimeMaxSec=40s",label_option,bus_option,"--","/usr/bin/env",argv[0],"--client",controller,(char *)NULL); _exit(127); }
    wait_child(transport,first); /* Actual original observer exit, all its FDs lost. */
    struct record record; record_io(argv[1],&record,0); CHECK(strcmp(record.context.unit,unit)==0 && record.context.uid==0);
    CHECK(sd_bus_call_method(system,"org.freedesktop.systemd1","/org/freedesktop/systemd1","org.freedesktop.systemd1.Manager","RefUnit",NULL,NULL,"s",unit)>=0);
    next[0]=0; pid_t second=spawn_observer(argv[0],argv[1],1,next);
    wait_child(transport,runner); wait_child(transport,second);
    CHECK(sd_bus_call_method(system,"org.freedesktop.systemd1","/org/freedesktop/systemd1","org.freedesktop.systemd1.Manager","UnrefUnit",NULL,NULL,"s",unit)>=0);
    int dir=open(argv[1],O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW); CHECK(dir>=0 && unlinkat(dir,"process.record",0)==0 && fsync(dir)==0 && close(dir)==0);
    sd_bus_flush_close_unref(transport); sd_bus_flush_close_unref(system); CHECK(descriptors()==baseline);
    puts("Original observer exited; new observer reidentified original live process through fresh bus ProcessFD, rejected same-context child, and observed original end; no journal/lease recovery claim."); return 0;
}
