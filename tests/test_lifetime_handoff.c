#include "linux/lifetime.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <selinux/selinux.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <systemd/sd-daemon.h>
#include <unistd.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"lifetime handoff line %d: %s errno=%d\n",__LINE__,#x,errno); exit(1); } } while (0)
#define OBJECT "/org/aetheri/TestHandoff"
#define IFACE "org.aetheri.TestHandoff1"
#define MANAGER "org.freedesktop.systemd1"
static const char *const names[4]={"aetheri-test-process","aetheri-test-anchor","aetheri-test-group","aetheri-test-events"};
/* Test-local native envelope, never an installed journal or a portable format. */
static void record_io(const char *work,struct ae_lifetime_checkpoint *record,int writing)
{
    int directory=open(work,O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW); CHECK(directory>=0);
    struct stat parent; CHECK(fstat(directory,&parent)==0 && parent.st_uid==0 && (parent.st_mode&0777)==0700);
    int fd=openat(directory,"handoff.record",(writing ? O_WRONLY|O_CREAT|O_EXCL : O_RDONLY)|O_CLOEXEC|O_NOFOLLOW,0600); CHECK(fd>=0);
    struct stat st; CHECK(fstat(fd,&st)==0 && S_ISREG(st.st_mode) && st.st_uid==0 && st.st_nlink==1 && (st.st_mode&0777)==0600);
    if (writing) CHECK(write(fd,record,sizeof *record)==sizeof *record && fsync(fd)==0 && fsync(directory)==0);
    else { char tail; CHECK(st.st_size==(off_t)sizeof *record && read(fd,record,sizeof *record)==sizeof *record && read(fd,&tail,1)==0); }
    CHECK(close(fd)==0 && close(directory)==0);
}
static int descriptors(void)
{ DIR *d=opendir("/proc/self/fd"); CHECK(d); int n=0; while (readdir(d)) ++n; CHECK(closedir(d)==0); return n; }
static void guard(void)
{
    struct stat st; CHECK(getuid()==0 && geteuid()==0 && getgid()==0 && getegid()==0 && security_getenforce()==1);
    CHECK(stat("/sys/firmware/efi",&st)==0 && S_ISDIR(st.st_mode));
    CHECK(lstat("/etc/aetheri-p0-carrier",&st)==0 && S_ISREG(st.st_mode) && st.st_uid==0 && st.st_nlink==1 && !(st.st_mode&0022));
    FILE *f=fopen("/sys/class/dmi/id/product_name","r"); char text[128]={0};
    CHECK(f && fgets(text,sizeof text,f) && fclose(f)==0 && strcmp(text,"Aetheri P0 disposable VM\n")==0);
}
static void reference(sd_bus *bus,const char *unit,int take)
{ CHECK(sd_bus_call_method(bus,MANAGER,"/org/freedesktop/systemd1",MANAGER ".Manager",take ? "RefUnit" : "UnrefUnit",NULL,NULL,"s",unit)>=0); }
static uint32_t store_size(sd_bus *bus,const char *unit)
{
    sd_bus_message *reply=NULL; const char *path=NULL; uint32_t count=UINT32_MAX;
    CHECK(sd_bus_call_method(bus,MANAGER,"/org/freedesktop/systemd1",MANAGER ".Manager","GetUnit",NULL,&reply,"s",unit)>=0 && sd_bus_message_read(reply,"o",&path)>0);
    CHECK(sd_bus_get_property_trivial(bus,MANAGER,path,MANAGER ".Service","NFileDescriptorStore",NULL,'u',&count)>=0);
    sd_bus_message_unref(reply); return count;
}
static int settled(sd_bus *bus,const char *unit)
{
    sd_bus_message *reply=NULL,*job=NULL; const char *path=NULL,*jobpath=NULL; char *state=NULL; uint32_t main=1,id=1;
    CHECK(sd_bus_call_method(bus,MANAGER,"/org/freedesktop/systemd1",MANAGER ".Manager","GetUnit",NULL,&reply,"s",unit)>=0 && sd_bus_message_read(reply,"o",&path)>0);
    CHECK(sd_bus_get_property_string(bus,MANAGER,path,MANAGER ".Unit","ActiveState",NULL,&state)>=0);
    CHECK(sd_bus_get_property_trivial(bus,MANAGER,path,MANAGER ".Service","MainPID",NULL,'u',&main)>=0);
    CHECK(sd_bus_get_property(bus,MANAGER,path,MANAGER ".Unit","Job",NULL,&job,"(uo)")>=0 && sd_bus_message_read(job,"(uo)",&id,&jobpath)>0);
    int result=(strcmp(state,"inactive")==0 || strcmp(state,"failed")==0) && !main && !id && strcmp(jobpath,"/")==0;
    free(state); sd_bus_message_unref(job); sd_bus_message_unref(reply); return result;
}
static void rejection_checks(sd_bus *system,const struct ae_lifetime_checkpoint *record,const int fds[4])
{
    int baseline=descriptors(); struct ae_lifetime *life=NULL;
    CHECK(ae_lifetime_restore(system,record,fds,&life)==0);
    struct ae_lifetime *same=life; CHECK(ae_lifetime_restore(system,record,fds,&life)==-EINVAL && life==same);
    struct ae_lifetime_end out,marker; memset(&marker,0xa5,sizeof marker); out=marker;
    CHECK(ae_lifetime_observe_end(life,&out)==-EAGAIN && memcmp(&out,&marker,sizeof out)==0);
    int empty[4]={-1,-1,-1,-1}; struct ae_lifetime_checkpoint untouched;
    memset(&untouched,0xa5,sizeof untouched); struct ae_lifetime_checkpoint original=untouched;
    CHECK(ae_lifetime_export(life,&untouched,empty)==-EALREADY && memcmp(&untouched,&original,sizeof untouched)==0);
    CHECK(ae_lifetime_close(&life)==0);
    int other=open("/dev/null",O_RDONLY|O_CLOEXEC); CHECK(other>=0);
    for (int n=0;n<4;++n) {
        int bad[4]; memcpy(bad,fds,sizeof bad); bad[n]=-1;
        CHECK(ae_lifetime_restore(system,record,bad,&life)==-EINVAL && !life);
        bad[n]=other; CHECK(ae_lifetime_restore(system,record,bad,&life)<0 && !life);
    }
    CHECK(close(other)==0);
    for (int n=0;n<12;++n) {
        struct ae_lifetime_checkpoint bad=*record;
        switch(n) {
        case 0: bad.process.boot[0]^=1; break; case 1: bad.process.pidfd_id^=1U; break;
        case 2: bad.root.mount_id^=1U; break; case 3: bad.group.inode^=1U; break;
        case 4: bad.events_inode^=1U; break; case 5: bad.events_mount^=1U; break;
        case 6: bad.pid_namespace^=1U; break; case 7: bad.cgroup_namespace^=1U; break;
        case 8: bad.context.invocation[0]^=1; break; case 9: bad.pid^=1U; break;
        case 10: bad.context.label[0]^=1; break; default: bad.context.cgroup[1]^=1;
        }
        CHECK(ae_lifetime_restore(system,&bad,fds,&life)<0 && !life);
    }
    CHECK(descriptors()==baseline);
}
struct owner { sd_bus *system,*transport; struct ae_lifetime *life; struct ae_lifetime_checkpoint record; int fds[4],done; const char *work,*controller; };
static int capture(sd_bus_message *m,void *userdata,sd_bus_error *error)
{
    (void)error; struct owner *o=userdata; CHECK(!o->done);
    struct ae_peer peer=AE_PEER_INIT; struct ae_process_context context;
    CHECK(ae_peer_capture(m,&peer)==0 && ae_process_observe(&peer,o->system,&context)==0);
    CHECK(sd_bus_call_method(o->transport,o->controller,OBJECT,IFACE,"Retain",NULL,NULL,"")>=0);
    reference(o->system,context.unit,1);
    CHECK(ae_lifetime_capture(o->system,&peer,&context,&o->life)==0);
    CHECK(ae_lifetime_export(o->life,&o->record,o->fds)==0); rejection_checks(o->system,&o->record,o->fds);
    record_io(o->work,&o->record,1); o->done=1;
    ae_peer_clear(&peer); return sd_bus_reply_method_return(m,"");
}
static const sd_bus_vtable capture_methods[]={SD_BUS_VTABLE_START(0),SD_BUS_METHOD("Capture","","",capture,SD_BUS_VTABLE_UNPRIVILEGED),SD_BUS_VTABLE_END};
static void owner_main(const char *work,const char *controller)
{
    struct owner o={.work=work,.controller=controller,.fds={-1,-1,-1,-1}}; sd_bus *bus=NULL;
    char **received=NULL; int count=sd_listen_fds_with_names(1,&received); CHECK(count==0 || count==4);
    FILE *status=fopen("/proc/self/status","r"); CHECK(status); char line[256]; int caps=0;
    while (fgets(line,sizeof line,status)) if (strncmp(line,"Cap",3)==0 && strchr(line,':')) {
        char value[32]; CHECK(sscanf(strchr(line,':')+1,"%31s",value)==1 && strcmp(value,"0000000000000000")==0); ++caps;
    }
    CHECK(fclose(status)==0 && caps==5);
    CHECK(sd_bus_open_system(&o.system)>=0 && sd_bus_open_user(&bus)>=0);
    o.transport=bus;
    CHECK(sd_bus_set_method_call_timeout(o.system,2000000)>=0);
    CHECK(sd_bus_call_method(bus,controller,OBJECT,IFACE,"Owner",NULL,NULL,"u",(uint32_t)(count==4))>=0);
    if (!count) {
        CHECK(sd_bus_add_object_vtable(bus,NULL,OBJECT,IFACE,capture_methods,&o)>=0);
        for (int n=0;!o.done && n<200;++n) { int r=sd_bus_process(bus,NULL); CHECK(r>=0); if (!r) CHECK(sd_bus_wait(bus,100000)>=0); }
        CHECK(o.done && sd_bus_flush(bus)>=0);
        for (int n=0;n<4;++n) {
            char message[160]; CHECK(snprintf(message,sizeof message,"FDSTORE=1\nFDPOLL=0\nFDNAME=%s",names[n])>0);
            CHECK(sd_pid_notify_with_fds(0,0,message,&o.fds[n],1)>0);
        }
        CHECK(sd_notify_barrier(0,2000000)>0); /* processing, not acceptance */
        CHECK(ae_lifetime_close(&o.life)==0);
    } else {
        int seen=0;
        for (int n=0;n<count;++n) {
            int index=-1; for (int k=0;k<4;++k) if (strcmp(received[n],names[k])==0) index=k;
            CHECK(index>=0 && !(seen&(1<<index))); seen|=1<<index; o.fds[index]=SD_LISTEN_FDS_START+n; free(received[n]);
        }
        free(received); CHECK(seen==15); record_io(work,&o.record,0);
        struct ae_root_identity current; int root=open("/sys/fs/cgroup",O_RDONLY|O_DIRECTORY|O_CLOEXEC); CHECK(root>=0);
        CHECK(ae_root_observe(root,&current)==0 && close(root)==0 && current.mount_id!=o.record.root.mount_id);
        int baseline=descriptors();
        for (int n=0;n<6;++n) {
            struct ae_lifetime_checkpoint wrong=o.record;
            switch(n) { case 0: wrong.events_inode^=1U; break; case 1: wrong.process.pidfd_id^=1U; break;
            case 2: wrong.context.invocation[0]^=1; break; case 3: wrong.group.mount_id^=1U; break;
            case 4: wrong.cgroup_namespace^=1U; break; default: wrong.pid^=1U; }
            CHECK(ae_lifetime_restore(o.system,&wrong,o.fds,&o.life)<0 && !o.life);
        }
        CHECK(descriptors()==baseline);
        CHECK(ae_lifetime_restore(o.system,&o.record,o.fds,&o.life)==0);
        struct ae_lifetime_end end; CHECK(ae_lifetime_observe_end(o.life,&end)==0 && end.group_removed && end.pid==o.record.pid);
        CHECK(sd_bus_call_method(bus,controller,OBJECT,IFACE,"Finish",NULL,NULL,"ut",end.pid,end.cgroup_id)>=0);
        CHECK(ae_lifetime_close(&o.life)==0);
        for (int n=0;n<4;++n) { char message[160]; CHECK(snprintf(message,sizeof message,"FDSTOREREMOVE=1\nFDNAME=%s",names[n])>0); CHECK(sd_notify(0,message)>0); }
        CHECK(sd_notify_barrier(0,2000000)>0);
    }
    for (int n=0;n<4;++n) CHECK(close(o.fds[n])==0);
    sd_bus_flush_close_unref(bus); sd_bus_flush_close_unref(o.system);
}
struct control { sd_bus *system; const char *owner_unit,*app_unit; char destination[256]; struct ae_process_context first; int owners,exit_app,finished,retained; uint32_t app_pid; uint64_t group; };
static int control_call(sd_bus_message *m,void *userdata,sd_bus_error *error)
{
    (void)error; struct control *c=userdata; const char *member=sd_bus_message_get_member(m);
    if (strcmp(member,"Locate")==0) return sd_bus_reply_method_return(m,"s",c->destination);
    if (strcmp(member,"Exit")==0) return sd_bus_reply_method_return(m,"b",c->exit_app);
    struct ae_peer peer=AE_PEER_INIT; struct ae_process_context actual;
    CHECK(ae_peer_capture(m,&peer)==0 && ae_process_observe(&peer,c->system,&actual)==0 && strcmp(actual.unit,c->owner_unit)==0 && actual.uid==0);
    sd_bus_message *reply=NULL; const char *path=NULL; uint32_t main=0;
    CHECK(sd_bus_call_method(c->system,MANAGER,"/org/freedesktop/systemd1",MANAGER ".Manager","GetUnit",NULL,&reply,"s",c->owner_unit)>=0 && sd_bus_message_read(reply,"o",&path)>0);
    CHECK(sd_bus_get_property_trivial(c->system,MANAGER,path,MANAGER ".Service","MainPID",NULL,'u',&main)>=0 && main==(uint32_t)peer.connection_pid);
    sd_bus_message_unref(reply);
    if (strcmp(member,"Owner")==0) {
        uint32_t second=2; CHECK(sd_bus_message_read(m,"u",&second)>0 && second==(uint32_t)c->owners && c->owners<2);
        if (!second) { c->first=actual; reference(c->system,c->owner_unit,1); }
        else CHECK(memcmp(c->first.invocation,actual.invocation,16)!=0);
        CHECK(strlen(peer.unique_name)<sizeof c->destination); strcpy(c->destination,peer.unique_name); ++c->owners;
    } else if (strcmp(member,"Retain")==0) {
        CHECK(c->owners==1 && !c->retained); reference(c->system,c->app_unit,1); c->retained=1;
    } else {
        uint32_t pid=0; uint64_t group=0; CHECK(c->owners==2 && strcmp(member,"Finish")==0 && sd_bus_message_read(m,"ut",&pid,&group)>0 && pid==c->app_pid && group==c->group); c->finished=1;
    }
    ae_peer_clear(&peer); return sd_bus_reply_method_return(m,"");
}
static const sd_bus_vtable control_methods[]={SD_BUS_VTABLE_START(0),SD_BUS_METHOD("Owner","u","",control_call,SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("Finish","ut","",control_call,SD_BUS_VTABLE_UNPRIVILEGED),SD_BUS_METHOD("Retain","","",control_call,SD_BUS_VTABLE_UNPRIVILEGED),SD_BUS_METHOD("Locate","","s",control_call,SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("Exit","","b",control_call,SD_BUS_VTABLE_UNPRIVILEGED),SD_BUS_VTABLE_END};
static void client_main(const char *controller)
{
    sd_bus *bus=NULL; CHECK(sd_bus_open_user(&bus)>=0); char destination[256]={0};
    for (int n=0;!destination[0] && n<300;++n) {
        sd_bus_message *reply=NULL; const char *name=NULL;
        CHECK(sd_bus_call_method(bus,controller,OBJECT,IFACE,"Locate",NULL,&reply,"")>=0 && sd_bus_message_read(reply,"s",&name)>0 && strlen(name)<sizeof destination);
        strcpy(destination,name); sd_bus_message_unref(reply); if (!destination[0]) usleep(10000);
    }
    CHECK(destination[0] && sd_bus_call_method(bus,destination,OBJECT,IFACE,"Capture",NULL,NULL,"")>=0);
    int done=0; for (int n=0;!done && n<1000;++n) {
        sd_bus_message *reply=NULL; CHECK(sd_bus_call_method(bus,controller,OBJECT,IFACE,"Exit",NULL,&reply,"")>=0 && sd_bus_message_read(reply,"b",&done)>0);
        sd_bus_message_unref(reply); if (!done) usleep(10000);
    }
    CHECK(done); sd_bus_flush_close_unref(bus);
}
static void pump(sd_bus *bus)
{ int r=sd_bus_process(bus,NULL); CHECK(r>=0); if (!r) CHECK(sd_bus_wait(bus,10000)>=0); }
static void unit_command(sd_bus *bus,const char *verb,const char *unit)
{
    pid_t pid=fork(); CHECK(pid>=0); if (!pid) { execlp("systemctl","systemctl",verb,unit,(char *)NULL); _exit(127); }
    int status=0; pid_t r=0;
    for (int n=0;!r && n<1000;++n) { r=waitpid(pid,&status,WNOHANG); CHECK(r>=0); if (!r) pump(bus); }
    CHECK(r==pid && WIFEXITED(status) && WEXITSTATUS(status)==0);
}
static pid_t start(const char *binary,const char *unit,const char *work,const char *controller,int observer)
{
    char unit_option[300],label_option[600],bus_option[4096]; const char *address=getenv("DBUS_SESSION_BUS_ADDRESS"); char *label=NULL;
    CHECK(snprintf(unit_option,sizeof unit_option,"--unit=%s",unit)>0 && getcon_raw(&label)==0);
    CHECK(snprintf(label_option,sizeof label_option,"--property=SELinuxContext=%s",label)>0); freecon(label);
    CHECK(address && snprintf(bus_option,sizeof bus_option,"--setenv=DBUS_SESSION_BUS_ADDRESS=%s",address)>0 && strlen(bus_option)<sizeof bus_option-1);
    pid_t pid=fork(); CHECK(pid>=0);
    if (!pid) {
        if (observer) execlp("systemd-run","systemd-run",unit_option,"--wait","--service-type=exec","--property=PrivateMounts=yes",
            "--property=FileDescriptorStoreMax=4","--property=FileDescriptorStorePreserve=yes","--property=NotifyAccess=main",
            "--property=CapabilityBoundingSet=","--property=AmbientCapabilities=","--property=NoNewPrivileges=yes",
            "--property=RuntimeMaxSec=40s","--property=TimeoutStopSec=2s","--property=SendSIGKILL=no",label_option,bus_option,"--","/usr/bin/env",binary,"--owner",work,controller,(char *)NULL);
        else execlp("systemd-run","systemd-run",unit_option,"--wait","--service-type=exec","--property=NoNewPrivileges=yes",
            "--property=RuntimeMaxSec=40s","--property=TimeoutStopSec=2s","--property=SendSIGKILL=no",label_option,bus_option,"--","/usr/bin/env",binary,"--client",controller,(char *)NULL);
        _exit(127);
    }
    return pid;
}
int main(int argc,char **argv)
{
    alarm(70); guard();
    if (argc==4 && strcmp(argv[1],"--owner")==0) { owner_main(argv[2],argv[3]); return 0; }
    if (argc==3 && strcmp(argv[1],"--client")==0) { client_main(argv[2]); return 0; }
    CHECK(argc==2 && argv[0][0]=='/'); int baseline=descriptors(); sd_bus *bus=NULL; struct control c={0};
    CHECK(sd_bus_open_system(&c.system)>=0 && sd_bus_open_user(&bus)>=0);
    sd_id128_t id; char digits[33],owner_unit[256],app_unit[256]; CHECK(sd_id128_randomize(&id)==0); sd_id128_to_string(id,digits);
    CHECK(snprintf(owner_unit,sizeof owner_unit,"aetheri-handoff-owner-%s.service",digits)>0 && snprintf(app_unit,sizeof app_unit,"aetheri-handoff-app-%s.service",digits)>0);
    c.owner_unit=owner_unit; c.app_unit=app_unit; CHECK(sd_bus_add_object_vtable(bus,NULL,OBJECT,IFACE,control_methods,&c)>=0);
    const char *controller=NULL; CHECK(sd_bus_get_unique_name(bus,&controller)>=0);
    printf("Owned retained-handle fixtures: %s %s\n",owner_unit,app_unit); fflush(stdout);
    pid_t owner=start(argv[0],owner_unit,argv[1],controller,1),app=start(argv[0],app_unit,argv[1],controller,0);
    int status=0; pid_t done=0;
    for (int n=0;!done && n<2000;++n) { done=waitpid(owner,&status,WNOHANG); CHECK(done>=0); if (!done) pump(bus); }
    CHECK(done==owner && WIFEXITED(status) && WEXITSTATUS(status)==0 && c.owners==1 && settled(c.system,owner_unit));
    struct ae_lifetime_checkpoint record; record_io(argv[1],&record,0);
    CHECK(strcmp(record.context.unit,app_unit)==0 && store_size(c.system,owner_unit)==4 && c.retained);
    c.app_pid=record.pid; c.group=record.context.cgroup_id; c.exit_app=1; done=0;
    for (int n=0;!done && n<1000;++n) { done=waitpid(app,&status,WNOHANG); CHECK(done>=0); if (!done) pump(bus); }
    CHECK(done==app && WIFEXITED(status) && WEXITSTATUS(status)==0);
    for (int n=0;n<300 && !settled(c.system,app_unit);++n) pump(bus);
    CHECK(settled(c.system,app_unit) && store_size(c.system,owner_unit)==4);
    char group[2048]; struct stat st; CHECK(snprintf(group,sizeof group,"/sys/fs/cgroup%s",record.context.cgroup)>0 && lstat(group,&st)<0 && errno==ENOENT);
    unit_command(bus,"start",owner_unit);
    for (int n=0;n<1000 && (!c.finished || !settled(c.system,owner_unit));++n) pump(bus);
    CHECK(c.finished && c.owners==2 && settled(c.system,owner_unit) && store_size(c.system,owner_unit)==0);
    reference(c.system,owner_unit,0); reference(c.system,app_unit,0);
    int dir=open(argv[1],O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW); CHECK(dir>=0 && unlinkat(dir,"handoff.record",0)==0 && fsync(dir)==0 && close(dir)==0);
    sd_bus_flush_close_unref(bus); sd_bus_flush_close_unref(c.system); CHECK(descriptors()==baseline);
    puts("Original observer exited, app died and cgroup disappeared with all four handles retained; actual new PrivateMounts service recovered original end then explicitly emptied its PID1 store. No owner-journal recovery claim."); return 0;
}
