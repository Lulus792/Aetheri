#include "linux/lifetime.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <linux/openat2.h>
#include <selinux/selinux.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/pidfd.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/syscall.h>
#include <poll.h>
#include <unistd.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"lifetime line %d: %s errno=%d\n",__LINE__,#x,errno); exit(1); } } while (0)
static int fake_job,fake_invocation,fake_main,lookup_fault;
static struct ae_lifetime **close_slot;
long __real_syscall(long,...);
long __wrap_syscall(long number,...)
{
    CHECK(number==SYS_openat2);
    va_list args; va_start(args,number); int root=va_arg(args,int); const char *path=va_arg(args,const char *);
    const struct open_how *how=va_arg(args,const struct open_how *); size_t size=va_arg(args,size_t); va_end(args);
    if (path[0]!='/' && lookup_fault) {
        if (lookup_fault==1) { errno=ENOENT; return -1; }
        return fcntl(root,F_DUPFD_CLOEXEC,3); /* actual wrong held object */
    }
    return __real_syscall(number,root,path,how,size);
}
int __real_sd_bus_get_property(sd_bus *,const char *,const char *,const char *,const char *,sd_bus_error *,sd_bus_message **,const char *);
int __wrap_sd_bus_get_property(sd_bus *bus,const char *destination,const char *path,const char *interface,const char *member,sd_bus_error *error,sd_bus_message **reply,const char *signature)
{
    if ((fake_job && strcmp(member,"Job")==0) || (fake_invocation && strcmp(member,"InvocationID")==0)) {
        CHECK(sd_bus_message_new(bus,reply,SD_BUS_MESSAGE_METHOD_RETURN)>=0);
        if (strcmp(member,"Job")==0) CHECK(sd_bus_message_append(*reply,"(uo)",1U,"/org/freedesktop/systemd1/job/1")>=0);
        else { uint8_t wrong[16]={1}; CHECK(sd_bus_message_append_array(*reply,'y',wrong,16)>=0); }
        CHECK(sd_bus_message_seal(*reply,1,0)>=0 && sd_bus_message_rewind(*reply,1)>=0); return 0;
    }
    return __real_sd_bus_get_property(bus,destination,path,interface,member,error,reply,signature);
}
int __real_sd_bus_get_property_trivial(sd_bus *,const char *,const char *,const char *,const char *,sd_bus_error *,char,void *);
int __wrap_sd_bus_get_property_trivial(sd_bus *bus,const char *destination,const char *path,const char *interface,const char *member,sd_bus_error *error,char type,void *value)
{
    int r=__real_sd_bus_get_property_trivial(bus,destination,path,interface,member,error,type,value);
    if (strcmp(member,"MainPID")==0 && r>=0) {
        if (close_slot) CHECK(ae_lifetime_close(close_slot)==-EBUSY && *close_slot!=NULL);
        if (fake_main) *(uint32_t *)value=1;
    }
    return r;
}
struct server { sd_bus *bus; struct ae_lifetime *life,*stale,*missing,*replacement; struct ae_process_context context; char unit[256]; int calls,child_fd; };
static int descriptors(void)
{ DIR *d=opendir("/proc/self/fd"); CHECK(d); int count=0; while (readdir(d)) ++count; CHECK(closedir(d)==0); return count; }
static int capture(sd_bus_message *message,void *userdata,sd_bus_error *error)
{
    (void)error; struct server *s=userdata; struct ae_peer peer=AE_PEER_INIT; struct ae_process_context context;
    CHECK(ae_peer_capture(message,&peer)==0 && ae_process_observe(&peer,s->bus,&context)==0 && strcmp(context.unit,s->unit)==0);
    uint32_t child_pid=0; CHECK(sd_bus_message_read(message,"u",&child_pid)>0);
    if (s->calls==0) CHECK(sd_bus_call_method(s->bus,"org.freedesktop.systemd1","/org/freedesktop/systemd1","org.freedesktop.systemd1.Manager","RefUnit",NULL,NULL,"s",s->unit)>=0);
    else {
        struct ae_lifetime_end out,marker; memset(&marker,0xa5,sizeof marker); out=marker;
        CHECK(memcmp(context.invocation,s->context.invocation,16)!=0 && ae_lifetime_observe_end(s->life,&out)==-ESTALE && memcmp(&out,&marker,sizeof out)==0);
        CHECK(ae_lifetime_close(&s->life)==0 && ae_lifetime_close(&s->stale)==0);
    }
    for (int field=0;field<7;++field) {
        struct ae_process_context wrong=context;
        switch(field) { case 0: wrong.uid^=1U; break; case 1: wrong.boot[0]^=1; break; case 2: wrong.invocation[0]^=1; break;
            case 3: wrong.cgroup_id^=1U; break; case 4: wrong.unit[0]^=1; break; case 5: wrong.cgroup[0]^=1; break; default: wrong.label[0]^=1; }
        struct ae_lifetime *empty=NULL; CHECK(ae_lifetime_capture(s->bus,&peer,&wrong,&empty)==-ESTALE && empty==NULL);
    }
    int baseline=descriptors();
    for (int n=0;n<8;++n) { struct ae_lifetime *v=NULL; CHECK(ae_lifetime_capture(s->bus,&peer,&context,&v)==0 && ae_lifetime_close(&v)==0 && v==NULL); }
    CHECK(descriptors()==baseline);
    CHECK(ae_lifetime_capture(s->bus,&peer,&context,&s->life)==0 && ae_lifetime_capture(s->bus,&peer,&context,&s->stale)==0);
    if (child_pid) CHECK(ae_lifetime_capture(s->bus,&peer,&context,&s->missing)==0 && ae_lifetime_capture(s->bus,&peer,&context,&s->replacement)==0);
    struct ae_lifetime_end out,marker; memset(&marker,0xa5,sizeof marker); out=marker;
    CHECK(ae_lifetime_observe_end(s->life,&out)==-EAGAIN && memcmp(&out,&marker,sizeof out)==0);
    pid_t child=fork(); CHECK(child>=0);
    if (child==0) { CHECK(ae_lifetime_observe_end(s->life,&out)==-ECHILD && ae_lifetime_close(&s->life)==-ECHILD); _exit(0); }
    int status; CHECK(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==0);
    if (child_pid!=0) {
        /* Administrative test control only. Bind the held child pin to the
         * known fixture's nested group before ever signalling it. */
        CHECK(child_pid<=INT32_MAX && child_pid!=(uint32_t)peer.connection_pid);
        s->child_fd=pidfd_open((pid_t)child_pid,0); CHECK(s->child_fd>=0);
        struct pidfd_info info={.mask=PIDFD_INFO_PID|PIDFD_INFO_CREDS|PIDFD_INFO_CGROUPID};
        char path[2048]; struct stat st;
        CHECK(snprintf(path,sizeof path,"/sys/fs/cgroup%s/held-child",context.cgroup)>0 && lstat(path,&st)==0 && S_ISDIR(st.st_mode));
        CHECK(ioctl(s->child_fd,PIDFD_GET_INFO,&info)==0 && (info.mask&(PIDFD_INFO_PID|PIDFD_INFO_CREDS|PIDFD_INFO_CGROUPID))==(PIDFD_INFO_PID|PIDFD_INFO_CREDS|PIDFD_INFO_CGROUPID) &&
            info.pid==child_pid && info.tgid==child_pid && info.ruid==0 && info.euid==0 && info.suid==0 && info.fsuid==0 && info.cgroupid==(uint64_t)st.st_ino);
    }
    s->context=context; ++s->calls;
    ae_peer_clear(&peer); return sd_bus_reply_method_return(message,"");
}
static const sd_bus_vtable methods[]={SD_BUS_VTABLE_START(0),SD_BUS_METHOD("Capture","u","",capture,SD_BUS_VTABLE_UNPRIVILEGED),SD_BUS_VTABLE_END};
static int client(const char *destination,const char *scenario)
{
    sd_bus *bus=NULL; CHECK(sd_bus_open_user(&bus)>=0);
    uint32_t child_pid=0;
    if (strcmp(scenario,"descendant")==0) {
        sd_bus *system=NULL; CHECK(sd_bus_open_system(&system)>=0);
        struct ae_process_context self; CHECK(ae_process_observe_self(system,&self)==0); sd_bus_flush_close_unref(system);
        char group[2048],procs[2100]; CHECK(snprintf(group,sizeof group,"/sys/fs/cgroup%s/held-child",self.cgroup)>0 && mkdir(group,0700)==0);
        CHECK(snprintf(procs,sizeof procs,"%s/cgroup.procs",group)>0);
        pid_t child=fork(); CHECK(child>=0);
        if (child==0) { sleep(15); _exit(0); }
        child_pid=(uint32_t)child; int target=open(procs,O_WRONLY|O_CLOEXEC|O_NOFOLLOW); CHECK(target>=0);
        char text[32]; int count=snprintf(text,sizeof text,"%ld\n",(long)child); CHECK(count>0 && write(target,text,(size_t)count)==count && close(target)==0);
    }
    sd_bus_error failure=SD_BUS_ERROR_NULL;
    int r=sd_bus_call_method(bus,destination,"/org/aetheri/TestLifetime","org.aetheri.TestLifetime1","Capture",&failure,NULL,"u",child_pid);
    if (r<0) fprintf(stderr,"capture RPC: %d %s %s\n",r,failure.name ? failure.name : "-",failure.message ? failure.message : "-");
    CHECK(r>=0); sd_bus_error_free(&failure);
    sd_bus_flush_close_unref(bus); return strcmp(scenario,"failed")==0 ? 42 : 0;
}
static void guard(void)
{
    CHECK(geteuid()==0 && getuid()==0 && security_getenforce()==1);
    struct stat st; CHECK(stat("/sys/firmware/efi",&st)==0 && S_ISDIR(st.st_mode));
    CHECK(lstat("/etc/aetheri-p0-carrier",&st)==0 && S_ISREG(st.st_mode) && st.st_uid==0 && st.st_nlink==1 && !(st.st_mode&0022));
    FILE *f=fopen("/sys/class/dmi/id/product_name","r"); char name[128]={0}; CHECK(f && fgets(name,sizeof name,f) && fclose(f)==0 && strcmp(name,"Aetheri P0 disposable VM\n")==0);
}
static void await_call(struct server *s,sd_bus *transport,int count)
{
    int waits=0;
    while (s->calls<count) { int r=sd_bus_process(transport,NULL); CHECK(r>=0); if (!r) { CHECK(++waits<=10 && sd_bus_wait(transport,500000)>=0); } }
    CHECK(sd_bus_flush(transport)>=0);
}
static void end_checks(struct server *s,int failed)
{
    struct ae_lifetime_end value,marker; memset(&marker,0xa5,sizeof marker); value=marker;
    fake_job=1; CHECK(ae_lifetime_observe_end(s->life,&value)==-EAGAIN && memcmp(&value,&marker,sizeof value)==0); fake_job=0;
    fake_main=1; CHECK(ae_lifetime_observe_end(s->life,&value)==-EAGAIN && memcmp(&value,&marker,sizeof value)==0); fake_main=0;
    fake_invocation=1; CHECK(ae_lifetime_observe_end(s->stale,&value)==-ESTALE); fake_invocation=0;
    CHECK(ae_lifetime_observe_end(s->stale,&value)==-ESTALE);
    close_slot=&s->life;
    int r=ae_lifetime_observe_end(s->life,&value);
    if (r!=0) fprintf(stderr,"actual end: %d\n",r);
    CHECK(r==0 && value.cgroup_id==s->context.cgroup_id && value.unit_failed==failed && memcmp(value.boot,s->context.boot,16)==0 && memcmp(value.invocation,s->context.invocation,16)==0);
    close_slot=NULL;
}
static void descendant_checks(struct server *s)
{
    sd_bus_message *reply=NULL,*job=NULL; const char *path=NULL,*job_path=NULL;
    CHECK(sd_bus_call_method(s->bus,"org.freedesktop.systemd1","/org/freedesktop/systemd1","org.freedesktop.systemd1.Manager","GetUnit",NULL,&reply,"s",s->unit)>=0 && sd_bus_message_read(reply,"o",&path)>0);
    char *state=NULL; uint32_t pid=1,job_id=1;
    CHECK(sd_bus_get_property_string(s->bus,"org.freedesktop.systemd1",path,"org.freedesktop.systemd1.Unit","ActiveState",NULL,&state)>=0 && strcmp(state,"inactive")==0);
    CHECK(sd_bus_get_property_trivial(s->bus,"org.freedesktop.systemd1",path,"org.freedesktop.systemd1.Service","MainPID",NULL,'u',&pid)>=0 && pid==0);
    CHECK(sd_bus_get_property(s->bus,"org.freedesktop.systemd1",path,"org.freedesktop.systemd1.Unit","Job",NULL,&job,"(uo)")>=0 && sd_bus_message_read(job,"(uo)",&job_id,&job_path)>0 && job_id==0 && strcmp(job_path,"/")==0);
    free(state); sd_bus_message_unref(job); sd_bus_message_unref(reply);
    char group[2048],procs[2100],events[2100],text[128];
    CHECK(snprintf(group,sizeof group,"/sys/fs/cgroup%s",s->context.cgroup)>0 &&
        snprintf(procs,sizeof procs,"%s/cgroup.procs",group)>0 && snprintf(events,sizeof events,"%s/cgroup.events",group)>0);
    FILE *f=fopen(procs,"r"); CHECK(f && fgetc(f)==EOF && !ferror(f) && fclose(f)==0);
    f=fopen(events,"r"); CHECK(f && fgets(text,sizeof text,f) && strcmp(text,"populated 1\n")==0 && fclose(f)==0);
    struct ae_lifetime_end untouched,marker; memset(&marker,0xa5,sizeof marker); untouched=marker;
    CHECK(ae_lifetime_observe_end(s->life,&untouched)==-EAGAIN && memcmp(&untouched,&marker,sizeof marker)==0);
    /* cgroup-v2 rejects actual rename. Inject only the name lookup result:
     * the original stays live/readable, never deactivated. */
    lookup_fault=1;
    CHECK(ae_lifetime_observe_end(s->missing,&untouched)==-ESTALE && memcmp(&untouched,&marker,sizeof marker)==0);
    lookup_fault=2;
    CHECK(ae_lifetime_observe_end(s->replacement,&untouched)==-ESTALE && memcmp(&untouched,&marker,sizeof marker)==0);
    lookup_fault=0;
    CHECK(ae_lifetime_observe_end(s->missing,&untouched)==-ESTALE && ae_lifetime_observe_end(s->replacement,&untouched)==-ESTALE);
    CHECK(ae_lifetime_close(&s->missing)==0 && ae_lifetime_close(&s->replacement)==0);
    CHECK(pidfd_send_signal(s->child_fd,SIGTERM,NULL,0)==0); struct pollfd dead={.fd=s->child_fd,.events=POLLIN};
    CHECK(poll(&dead,1,5000)==1 && (dead.revents&POLLIN));
}
int main(int argc,char **argv)
{
    alarm(80);
    if (argc==4 && strcmp(argv[1],"--client")==0) { guard(); return client(argv[2],argv[3]); }
    CHECK(argc==1 && argv[0][0]=='/'); guard();
    int baseline=descriptors();
    const char *scenarios[]={"simple","failed","descendant","restart","namespace"};
    for (size_t n=0;n<sizeof scenarios/sizeof scenarios[0];++n) {
        struct server s={.child_fd=-1}; CHECK(sd_bus_open_system(&s.bus)>=0 && sd_bus_set_method_call_timeout(s.bus,2000000)>=0);
        sd_bus *transport=NULL; CHECK(sd_bus_open_user(&transport)>=0);
        CHECK(sd_bus_add_object_vtable(transport,NULL,"/org/aetheri/TestLifetime","org.aetheri.TestLifetime1",methods,&s)>=0);
        const char *unique=NULL; CHECK(sd_bus_get_unique_name(transport,&unique)>=0);
        const char *address=getenv("DBUS_SESSION_BUS_ADDRESS"); char bus_option[4096];
        CHECK(address && snprintf(bus_option,sizeof bus_option,"--setenv=DBUS_SESSION_BUS_ADDRESS=%s",address)>0 && strlen(bus_option)<sizeof bus_option-1);
        sd_id128_t nonce; char digits[33],option[300],label_option[600]; CHECK(sd_id128_randomize(&nonce)==0); sd_id128_to_string(nonce,digits);
        CHECK(snprintf(s.unit,sizeof s.unit,"aetheri-lifetime-test-%s.service",digits)>0 && snprintf(option,sizeof option,"--unit=%s",s.unit)>0);
        char *label=NULL; CHECK(getcon_raw(&label)==0 && snprintf(label_option,sizeof label_option,"--property=SELinuxContext=%s",label)>0); freecon(label);
        printf("Owned lifetime fixture: %s %s\n",s.unit,scenarios[n]); fflush(stdout);
        pid_t runner=fork(); CHECK(runner>=0);
        if (runner==0) { execlp("systemd-run","systemd-run",option,"--wait","--collect","--service-type=exec","--property=NoNewPrivileges=yes","--property=KillMode=process","--property=SendSIGKILL=no","--property=TimeoutStopSec=2s","--property=RuntimeMaxSec=20s",label_option,bus_option,"--","/usr/bin/env",argv[0],"--client",unique,scenarios[n],(char *)NULL); _exit(127); }
        await_call(&s,transport,1); int status; CHECK(waitpid(runner,&status,0)==runner && WIFEXITED(status) && WEXITSTATUS(status)==(strcmp(scenarios[n],"failed")==0 ? 42 : 0));
        if (strcmp(scenarios[n],"descendant")==0) {
            descendant_checks(&s);
        }
        if (s.child_fd>=0) CHECK(close(s.child_fd)==0);
        s.child_fd=-1;
        end_checks(&s,strcmp(scenarios[n],"failed")==0);
        if (strcmp(scenarios[n],"restart")==0) {
            CHECK(sd_bus_call_method(s.bus,"org.freedesktop.systemd1","/org/freedesktop/systemd1","org.freedesktop.systemd1.Manager","StartUnit",NULL,NULL,"ss",s.unit,"fail")>=0);
            await_call(&s,transport,2); CHECK(s.child_fd==-1);
            /* Bounded test-only retry; the observer has no polling loop. */
            int r=-EAGAIN; struct ae_lifetime_end ended;
            for (int attempt=0;attempt<20 && r==-EAGAIN;++attempt) { r=ae_lifetime_observe_end(s.life,&ended); if (r==-EAGAIN) usleep(10000); }
            CHECK(r==0);
        }
        if (strcmp(scenarios[n],"namespace")==0) {
            CHECK(unshare(CLONE_NEWNS)==0 && mount(NULL,"/",NULL,MS_REC|MS_PRIVATE,NULL)==0);
            struct ae_lifetime_end value; CHECK(ae_lifetime_observe_end(s.life,&value)==-ESTALE && ae_lifetime_observe_end(s.life,&value)==-ESTALE);
            /* Last case: exit this disposable administrative namespace. */
        }
        CHECK(ae_lifetime_close(&s.life)==0 && ae_lifetime_close(&s.stale)==0);
        CHECK(sd_bus_call_method(s.bus,"org.freedesktop.systemd1","/org/freedesktop/systemd1","org.freedesktop.systemd1.Manager","UnrefUnit",NULL,NULL,"s",s.unit)>=0);
        sd_bus_flush_close_unref(transport); sd_bus_flush_close_unref(s.bus);
    }
    CHECK(descriptors()==baseline);
    puts("Original ProcessFD/unit/recursive cgroup end, descendant retention, restart/namespace quarantine, job/main-PID rejection, fork/busy close and FD balance passed; no lease release."); return 0;
}
