#include "linux/unit.h"
#include "core/digest.h"
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
#include <unistd.h>

#define MANAGER "org.freedesktop.systemd1"
#define PATH "/org/freedesktop/systemd1"
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"unit %s line %d: %s errno=%d\n",mode,__LINE__,#x,errno); exit(1); } } while (0)
static const char *mode="initial";
static struct ae_unit *active;
static const char *fault_method;
static unsigned int fault_ordinal,fault_count,starts,notices,guards;
static int fault_after,fault_enabled;
static char unit_name[256];
static sd_bus *owned_bus;
static unsigned int owned_flushes,drift_at;
static int source_directory=-1,mutated;
static unsigned int close_at;
sd_bus *__real_sd_bus_flush_close_unref(sd_bus *);
sd_bus *__wrap_sd_bus_flush_close_unref(sd_bus *bus)
{ if (bus==owned_bus) ++owned_flushes; return __real_sd_bus_flush_close_unref(bus); }
static void mutate_source(void)
{
    int fd=openat(source_directory,unit_name,O_WRONLY|O_CLOEXEC|O_NOFOLLOW);
    CHECK(fd>=0 && pwrite(fd,"#",1,0)==1 && fsync(fd)==0 && close(fd)==0); mutated=1;
}
int __real_sd_bus_call(sd_bus *,sd_bus_message *,uint64_t,sd_bus_error *,sd_bus_message **);
int __wrap_sd_bus_call(sd_bus *bus,sd_bus_message *q,uint64_t time,sd_bus_error *error,sd_bus_message **out)
{
    int selected=fault_enabled && fault_method && sd_bus_message_is_method_call(q,MANAGER ".Manager",fault_method);
    if (selected) selected=++fault_count==fault_ordinal;
    if (selected && !fault_after) return -EIO;
    int r=__real_sd_bus_call(bus,q,time,error,out);
    return selected && fault_after && r>=0 ? -EIO : r;
}
int __real_sd_bus_call_async(sd_bus *,sd_bus_slot **,sd_bus_message *,sd_bus_message_handler_t,void *,uint64_t);
int __wrap_sd_bus_call_async(sd_bus *bus,sd_bus_slot **slot,sd_bus_message *q,sd_bus_message_handler_t fn,void *data,uint64_t time)
{
    if (sd_bus_message_is_method_call(q,MANAGER ".Manager","StartUnit")) {
        ++starts;
        const char *suppress=getenv("AETHERI_UNIT_TEST_SUPPRESS_START");
        if (suppress && !strcmp(suppress,"1")) { fputs("counterexample target submission suppressed\n",stderr); return -ECANCELED; }
    }
    return __real_sd_bus_call_async(bus,slot,q,fn,data,time);
}
static int descriptors(void)
{ DIR *d=opendir("/proc/self/fd"); CHECK(d); int n=0; while (readdir(d)) ++n; CHECK(closedir(d)==0); return n; }
static void carrier(void)
{
    struct stat st; CHECK(!getuid() && !geteuid() && !getgid() && !getegid() && security_getenforce()==1);
    CHECK(lstat("/etc/aetheri-p0-carrier",&st)==0 && S_ISREG(st.st_mode) && st.st_uid==0 && st.st_nlink==1 && !(st.st_mode&0022));
    CHECK(stat("/sys/firmware/efi",&st)==0 && S_ISDIR(st.st_mode));
    FILE *f=fopen("/sys/class/dmi/id/product_name","r"); char value[128]={0};
    CHECK(f && fgets(value,sizeof value,f) && fclose(f)==0 && !strcmp(value,"Aetheri P0 disposable VM\n"));
}
static void connect_bus(sd_bus **out)
{ CHECK(sd_bus_new(out)>=0 && sd_bus_set_address(*out,"unix:path=/run/dbus/system_bus_socket")>=0 && sd_bus_set_bus_client(*out,1)>=0 && sd_bus_start(*out)>=0); }
static void manager(sd_bus *bus,struct ae_peer *out)
{
    sd_bus_creds *creds=NULL; const char *name=NULL;
    CHECK(sd_bus_get_name_creds(bus,MANAGER,SD_BUS_CREDS_UNIQUE_NAME,&creds)>=0 && sd_bus_creds_get_unique_name(creds,&name)>=0);
    sd_bus_message *q=NULL,*reply=NULL;
    CHECK(sd_bus_message_new_method_call(bus,&q,name,PATH,"org.freedesktop.DBus.Peer","Ping")>=0 && sd_bus_call(bus,q,1000000,NULL,&reply)>=0);
    CHECK(ae_peer_capture_reply(reply,q,out)==0 && out->connection_uid==0 && out->connection_pid==1);
    sd_bus_message_unref(reply); sd_bus_message_unref(q); sd_bus_creds_unref(creds);
}
static int guard(void *data)
{
    (void)data; ++guards;
    if (drift_at && guards==drift_at) mutate_source();
    if (close_at && guards==close_at) sd_bus_close(ae_unit_bus(active));
    if (active) {
        struct ae_unit *same=active; CHECK(ae_unit_close(&same)==-EBUSY && same==active);
        CHECK(ae_unit_retire(active,guard,NULL)==-EBUSY);
    }
    return 0;
}
static int deny(void *data) { (void)data; return -EPERM; }
static void changed(void *data,const struct ae_start_status *status)
{
    (void)data; ++notices; CHECK(!status->quarantined);
    struct ae_unit *same=active; CHECK(ae_unit_close(&same)==-EBUSY && same==active);
    CHECK(ae_unit_retire(active,guard,NULL)==-EBUSY);
}
static int pump_deny(void *data)
{
    (void)data;
    for (unsigned int i=0;i<500;++i) {
        CHECK(sd_bus_process(ae_unit_bus(active),NULL)>=0); struct ae_unit_status state; CHECK(ae_unit_status(active,&state)==0);
        if (state.start.job_removed) break;
        CHECK(sd_bus_wait(ae_unit_bus(active),10000)>=0);
    }
    struct ae_unit_status state; CHECK(ae_unit_status(active,&state)==0 && state.start.job_removed && notices && !strcmp(state.start.result,"done"));
    CHECK(ae_unit_retire(active,guard,NULL)==-EBUSY); /* Outer busy survives nested changed callback. */
    return -EPERM;
}
static void decode(const char *text,uint8_t out[32])
{
    const char digits[]="0123456789abcdef"; CHECK(strlen(text)==64);
    for (unsigned int i=0;i<32;++i) { const char *a=strchr(digits,text[2*i]),*b=strchr(digits,text[2*i+1]); CHECK(a && b); out[i]=(uint8_t)((a-digits)*16+(b-digits)); }
}
static int dead(sd_bus *bus)
{
    sd_bus_message *m=NULL; sd_bus_error error=SD_BUS_ERROR_NULL; const char *path=NULL; char *state=NULL;
    int r=sd_bus_call_method(bus,MANAGER,PATH,MANAGER ".Manager","GetUnit",&error,&m,"s",unit_name);
    if (r<0) { CHECK(sd_bus_error_has_name(&error,"org.freedesktop.systemd1.NoSuchUnit")); sd_bus_error_free(&error); sd_bus_message_unref(m); return 1; }
    CHECK(sd_bus_message_read(m,"o",&path)>0);
    CHECK(sd_bus_get_property_string(bus,MANAGER,path,MANAGER ".Unit","ActiveState",NULL,&state)>=0);
    int okay=!strcmp(state,"inactive") || !strcmp(state,"failed"); free(state);
    sd_bus_message *job=NULL; uint32_t id=1; const char *jobpath=NULL;
    CHECK(sd_bus_get_property(bus,MANAGER,path,MANAGER ".Unit","Job",NULL,&job,"(uo)")>=0 && sd_bus_message_read(job,"(uo)",&id,&jobpath)>0);
    okay=okay && !id && !strcmp(jobpath,"/"); sd_bus_message_unref(job); sd_bus_message_unref(m); return okay;
}
static void true_result(sd_bus *bus)
{
    sd_bus_message *m=NULL; const char *path=NULL; int code=0,status=-1; uint32_t pid=0; uint64_t start=0,end=0;
    CHECK(sd_bus_call_method(bus,MANAGER,PATH,MANAGER ".Manager","GetUnit",NULL,&m,"s",unit_name)>=0 && sd_bus_message_read(m,"o",&path)>0);
    CHECK(sd_bus_get_property_trivial(bus,MANAGER,path,MANAGER ".Service","ExecMainCode",NULL,'i',&code)>=0 && code==CLD_EXITED);
    CHECK(sd_bus_get_property_trivial(bus,MANAGER,path,MANAGER ".Service","ExecMainStatus",NULL,'i',&status)>=0 && !status);
    CHECK(sd_bus_get_property_trivial(bus,MANAGER,path,MANAGER ".Service","ExecMainPID",NULL,'u',&pid)>=0 && pid);
    CHECK(sd_bus_get_property_trivial(bus,MANAGER,path,MANAGER ".Service","ExecMainStartTimestampMonotonic",NULL,'t',&start)>=0 && start);
    CHECK(sd_bus_get_property_trivial(bus,MANAGER,path,MANAGER ".Service","ExecMainExitTimestampMonotonic",NULL,'t',&end)>=0 && end>=start);
    sd_bus_message_unref(m);
}
int main(int argc,char **argv)
{
    CHECK(argc==3); mode=argv[1]; carrier(); int baseline=descriptors();
    struct ae_unit_profile *p=calloc(1,sizeof *p); CHECK(p); p->count=3;
    struct ae_unit_node *n=&p->nodes[0]; n->kind=AE_UNIT_SERVICE;
    sd_id128_t token,boot; CHECK(sd_id128_randomize(&token)>=0 && sd_id128_get_boot(&boot)>=0); char id[SD_ID128_STRING_MAX]; sd_id128_to_string(token,id);
    CHECK(snprintf(n->name,sizeof n->name,"aetheri-unit-test-%s.service",id)>0); strcpy(unit_name,n->name);
    strcpy(n->directory,"/run/systemd/system"); n->directory_fd=open(n->directory,O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW); CHECK(n->directory_fd>=0);
    CHECK(ae_root_observe(n->directory_fd,&n->parent)==0 && !n->parent.uid && !(n->parent.mode&0022));
    char text[2048]; int size=snprintf(text,sizeof text,
        "[Unit]\nDescription=Aetheri unit custody administrative true test\nDefaultDependencies=no\n"
        "[Service]\nType=exec\nExitType=main\nUser=root\nGroup=root\nRestart=%s\n%sKillMode=control-group\nSlice=system.slice\n"
        "SELinuxContext=system_u:system_r:unconfined_service_t:s0\nNoNewPrivileges=yes\nPrivateMounts=no\nPrivateNetwork=no\nPrivateDevices=no\n"
        "FileDescriptorStoreMax=0\nFileDescriptorStorePreserve=no\nStandardInput=null\nStandardOutput=null\nStandardError=null\nCapabilityBoundingSet=\nAmbientCapabilities=\nUMask=0077\nExecStart=%s/usr/bin/true\n",
        !strcmp(mode,"restart") ? "always" : "no",!strcmp(mode,"force-restart") ? "RestartForceExitStatus=0\n" : "",
        !strcmp(mode,"privilege-flag") ? "+" : ""); CHECK(size>0 && size<(int)sizeof text);
    int fd=openat(n->directory_fd,n->name,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600); CHECK(fd>=0);
    CHECK(fsetfilecon_raw(fd,"system_u:object_r:systemd_unit_file_t:s0")==0 && write(fd,text,(size_t)size)==size && fchmod(fd,0444)==0 && fsync(fd)==0 && close(fd)==0 && fsync(n->directory_fd)==0);
    n->file_fd=openat(n->directory_fd,n->name,O_RDONLY|O_CLOEXEC|O_NOFOLLOW); CHECK(n->file_fd>=0);
    uint8_t hash[32]; CHECK(ae_sha256_bytes((const uint8_t *)text,(size_t)size,hash)==0 && ae_unit_file_capture(n->file_fd,hash,&n->file)==0);
    char record[4096]; CHECK(snprintf(record,sizeof record,"%s/fixture-%s.txt",argv[2],mode)>0);
    FILE *receipt=fopen(record,"wx"); CHECK(receipt);
    CHECK(fprintf(receipt,"unit=%s\ninode=%llu\nmount=%llu\nmode=%u\nsha256=",n->name,(unsigned long long)n->file.object.stx_ino,(unsigned long long)n->file.object.stx_mnt_id,(unsigned int)n->file.object.stx_mode)>0);
    for (unsigned int i=0;i<32;++i) CHECK(fprintf(receipt,"%02x",hash[i])==2);
    CHECK(fprintf(receipt,"\n")>0 && fflush(receipt)==0 && fsync(fileno(receipt))==0 && fclose(receipt)==0);
    n->dropin_count=1; struct ae_unit_dropin *d=&n->dropins[0]; strcpy(d->directory,"/usr/lib/systemd/system/service.d"); strcpy(d->leaf,"10-timeout-abort.conf");
    d->directory_fd=open(d->directory,O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW); CHECK(d->directory_fd>=0 && ae_root_observe(d->directory_fd,&d->parent)==0);
    d->file_fd=openat(d->directory_fd,d->leaf,O_RDONLY|O_CLOEXEC|O_NOFOLLOW); CHECK(d->file_fd>=0);
    decode("ae6b234f92bc22f1201a7572b59b454c9809f33c80d13f361b9674e1801acc37",hash); CHECK(ae_unit_file_capture(d->file_fd,hash,&d->file)==0);
    strcpy(n->executable,"/usr/bin/true"); strcpy(n->argv[0],n->executable); n->argc=1;
    strcpy(n->label,"system_u:system_r:unconfined_service_t:s0"); n->no_new_privileges=1; n->umask=0077;
    p->nodes[1].kind=AE_UNIT_SYSTEM_SLICE; strcpy(p->nodes[1].name,"system.slice");
    p->nodes[2].kind=AE_UNIT_ROOT_SLICE; strcpy(p->nodes[2].name,"-.slice");
    n->edges[AE_UNIT_REQUIRES]=n->edges[AE_UNIT_AFTER]=2;
    p->nodes[1].edges[AE_UNIT_REQUIRES]=p->nodes[1].edges[AE_UNIT_AFTER]=4;
    if (!strcmp(mode,"graph")) n->edges[AE_UNIT_REQUIRES]=0;
    if (!strcmp(mode,"argv")) strcpy(n->argv[0],"foreign");
    if (!strcmp(mode,"dropin")) n->dropin_count=0;
    sd_bus *bus=NULL,*control=NULL; connect_bus(&bus); owned_bus=bus; connect_bus(&control); struct ae_peer pid1=AE_PEER_INIT; manager(bus,&pid1);
    struct ae_unit *empty=NULL; unsigned int saved=p->count; p->count=AE_UNIT_MAX+1;
    CHECK(ae_unit_new(&bus,&pid1,boot.bytes,p,&empty)==-EINVAL && !empty && bus==owned_bus); p->count=saved;
    CHECK(ae_unit_new(&bus,&pid1,boot.bytes,p,&active)==0 && !bus); ae_peer_clear(&pid1);
    source_directory=n->directory_fd;
    if (!strcmp(mode,"copy")) strcpy(n->argv[0],"caller-modified-after-move");
    fault_after=strstr(mode,"after")!=NULL; fault_ordinal=1;
    if (strstr(mode,"subscribe-")) fault_method="Subscribe";
    if (strncmp(mode,"ref",3)==0) { fault_method="RefUnit"; if (mode[3]=='2') fault_ordinal=2; }
    int release_fault=strncmp(mode,"unref-",6)==0 || strncmp(mode,"unsubscribe-",12)==0;
    if (release_fault) fault_method=!strncmp(mode,"unref-",6) ? "UnrefUnit" : "Unsubscribe";
    fault_enabled=fault_method && !release_fault;
    int mismatch=!strcmp(mode,"graph") || !strcmp(mode,"argv") || !strcmp(mode,"dropin") || !strcmp(mode,"privilege-flag") || !strcmp(mode,"restart") || !strcmp(mode,"force-restart");
    int denied=!strcmp(mode,"guard-deny");
    int r=ae_unit_acquire(active,denied ? deny : guard,NULL); struct ae_unit_status status; CHECK(ae_unit_status(active,&status)==0);
    if (r) fprintf(stderr,"admission result=%d attribute=%s refs=%u\n",r,status.attribute,(unsigned int)status.referenced);
    CHECK(r==(denied ? -EPERM : fault_enabled ? -EIO : mismatch ? -ESTALE : 0));
    CHECK(ae_unit_acquire(active,guard,NULL)==-EALREADY);
    if (!denied) CHECK(ae_unit_close(&active)==-EBUSY && active);
    fault_enabled=0;
    if (denied) { CHECK(!status.subscribe_attempted && !status.ref_attempted && !starts && ae_unit_close(&active)==0); }
    else if (mismatch || (fault_method && !release_fault)) {
        CHECK(!status.ready && !starts);
        CHECK(ae_unit_abandon(&active)==0);
    } else {
        CHECK(status.ready && status.referenced==7 && ae_unit_check_fresh(active)==0);
        if (!strcmp(mode,"fork")) {
            pid_t child=fork(); CHECK(child>=0); if (!child) { CHECK(ae_unit_acquire(active,guard,NULL)==-ECHILD && ae_unit_abandon(&active)==-ECHILD); _exit(0); }
            int code; CHECK(waitpid(child,&code,0)==child && WIFEXITED(code) && WEXITSTATUS(code)==0);
        }
        if (!strcmp(mode,"normal") || !strcmp(mode,"nested")) {
            CHECK(ae_unit_start(active,guard,NULL,changed,NULL)==0 && starts==1);
            if (!strcmp(mode,"nested")) CHECK(ae_unit_retire(active,pump_deny,NULL)==-EPERM && active);
            else {
                for (unsigned int i=0;i<500;++i) {
                    CHECK(sd_bus_process(ae_unit_bus(active),NULL)>=0 && ae_unit_status(active,&status)==0);
                    if (status.start.job_removed) break;
                    CHECK(sd_bus_wait(ae_unit_bus(active),10000)>=0);
                }
                CHECK(status.start.job_removed && !strcmp(status.start.result,"done"));
            }
            int ended=0; for (unsigned int i=0;i<200 && !ended;++i) { ended=dead(control); if (!ended) usleep(10000); } CHECK(ended);
            true_result(control);
            CHECK(ae_unit_check_fresh(active)<0); /* Historical invocation cannot regain fresh admission. */
        }
        if (!strcmp(mode,"source")) {
            mutate_source();
            CHECK(ae_unit_check_fresh(active)<0 && ae_unit_status(active,&status)==0 && status.fenced && !starts);
        }
        if (!strcmp(mode,"guard-source")) {
            drift_at=guards+4;
            CHECK(ae_unit_start(active,guard,NULL,changed,NULL)==-EBADMSG && mutated && !starts);
            CHECK(ae_unit_start(active,guard,NULL,changed,NULL)==-EALREADY);
        }
        if (!strcmp(mode,"guard-bus")) {
            close_at=guards+4;
            CHECK(ae_unit_start(active,guard,NULL,changed,NULL)==-ENOTCONN && !starts && ae_unit_status(active,&status)==0 && status.quarantined);
            CHECK(ae_unit_retire(active,guard,NULL)==-EAGAIN && ae_unit_close(&active)==-EBUSY);
            CHECK(ae_unit_abandon(&active)==0);
        } else {
            CHECK(ae_unit_retire(active,deny,NULL)==-EPERM && active);
            fault_enabled=release_fault; fault_count=0;
            r=ae_unit_retire(active,guard,NULL); CHECK(r==(release_fault ? -EIO : 0)); fault_enabled=0;
            if (release_fault) { CHECK(ae_unit_close(&active)==-EBUSY && ae_unit_retire(active,guard,NULL)==-EAGAIN); CHECK(ae_unit_abandon(&active)==0); }
            else { CHECK(ae_unit_retire(active,deny,NULL)==0 && ae_unit_status(active,&status)==0 && status.retired && !status.referenced && !status.subscribed && ae_unit_close(&active)==0); }
        }
    }
    CHECK(dead(control) && !owned_flushes);
    /* Only this exact independently created administrative file is retired.
     * The source case deliberately changed its own first byte; never a vendor file. */
    struct statx current; CHECK(statx(n->file_fd,"",AT_EMPTY_PATH,STATX_BASIC_STATS|STATX_MNT_ID_UNIQUE,&current)==0 && current.stx_ino==n->file.object.stx_ino && current.stx_mnt_id==n->file.object.stx_mnt_id && current.stx_nlink==1);
    if (mutated) text[0]='#';
    CHECK(ae_sha256_bytes((const uint8_t *)text,(size_t)size,hash)==0); struct ae_unit_file after; CHECK(ae_unit_file_capture(n->file_fd,hash,&after)==0);
    int named=openat(n->directory_fd,n->name,O_RDONLY|O_CLOEXEC|O_NOFOLLOW); CHECK(named>=0);
    struct ae_unit_file named_file; CHECK(ae_unit_file_capture(named,hash,&named_file)==0 && named_file.object.stx_ino==current.stx_ino && named_file.object.stx_mnt_id==current.stx_mnt_id && close(named)==0);
    CHECK(unlinkat(n->directory_fd,n->name,0)==0 && fsync(n->directory_fd)==0);
    CHECK(close(n->file_fd)==0 && close(n->directory_fd)==0 && close(d->file_fd)==0 && close(d->directory_fd)==0);
    sd_bus_flush_close_unref(control); free(p); CHECK(descriptors()==baseline);
    printf("unit custody %s: exact source/dropin/loaded service/forward graph, original references and bounded cleanup passed; administrative starts=%u\n",mode,starts); return 0;
}
