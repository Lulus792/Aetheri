#include "authority/handoff.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <systemd/sd-daemon.h>
#include <unistd.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"handoff service line %d: %s\n",__LINE__,#x); exit(1); } } while (0)
void ae_test_execution_commit_fault(int);
struct adapters {
    struct ae_process_context self;
    struct ae_lifetime_checkpoint original;
    int exported,restored,closed,drift,restore_error,objects,commit_fault,drift_after_restore;
    int commit_stage,store_bad,sends,removes,barriers,held,send_fail,remove_fail,barrier_fail,properties;
    int saved[4]; char labels[4][AE_HANDOFF_NAME_BYTES];
    struct ae_handoff_service **service;
};
static struct adapters *active;
static max_align_t live_original;
int __real_ae_catalogue_handoff(struct ae_catalogue *,const struct ae_execution_profile *,enum ae_handoff_stage,
    const struct ae_execution_handoff *,int64_t,struct ae_handoff_result *);
int __wrap_ae_catalogue_handoff(struct ae_catalogue *store,const struct ae_execution_profile *p,enum ae_handoff_stage stage,
    const struct ae_execution_handoff *h,int64_t stamp,struct ae_handoff_result *out)
{
    /* Scope fault to the mutation, after all read-only admission transactions. */
    if (active && active->commit_fault && (!active->commit_stage || active->commit_stage==(int)stage)) { ae_test_execution_commit_fault(active->commit_fault); active->commit_fault=0; }
    return __real_ae_catalogue_handoff(store,p,stage,h,stamp,out);
}
int __real_ae_process_observe_self(sd_bus *,struct ae_process_context *);
int __wrap_ae_process_observe_self(sd_bus *bus,struct ae_process_context *out)
{
    if (!active) return __real_ae_process_observe_self(bus,out);
    *out=active->self; if (active->drift) out->invocation[0]^=1; return 0;
}
int __real_ae_lifetime_export(struct ae_lifetime *,struct ae_lifetime_checkpoint *,int *);
int __wrap_ae_lifetime_export(struct ae_lifetime *v,struct ae_lifetime_checkpoint *out,int *fds)
{
    if (!active) return __real_ae_lifetime_export(v,out,fds);
    CHECK(v==(struct ae_lifetime *)&live_original);
    for (int n=0;n<4;++n) { CHECK(fds[n]==-1); fds[n]=open("/dev/null",O_RDONLY|O_CLOEXEC); CHECK(fds[n]>=3); }
    *out=active->original; ++active->exported;
    CHECK(ae_handoff_service_close(active->service)==-EBUSY); return 0;
}
int __real_ae_lifetime_restore(sd_bus *,const struct ae_lifetime_checkpoint *,const int *,struct ae_lifetime **);
int __wrap_ae_lifetime_restore(sd_bus *bus,const struct ae_lifetime_checkpoint *expected,const int *fds,struct ae_lifetime **out)
{
    if (!active) return __real_ae_lifetime_restore(bus,expected,fds,out);
    CHECK(!*out && expected->pid==active->original.pid &&
        expected->process.pidfd_id==active->original.process.pidfd_id &&
        memcmp(expected->process.boot,active->original.process.boot,16)==0 &&
        ae_process_context_equal(&expected->context,&active->original.context) &&
        ae_root_equal(&expected->root,&active->original.root) && ae_root_equal(&expected->group,&active->original.group) &&
        expected->pid_namespace==active->original.pid_namespace && expected->cgroup_namespace==active->original.cgroup_namespace &&
        expected->events_inode==active->original.events_inode && expected->events_mount==active->original.events_mount &&
        expected->events_mode==active->original.events_mode && expected->events_uid==active->original.events_uid && expected->events_gid==active->original.events_gid);
    for (int n=0;n<4;++n) CHECK(fcntl(fds[n],F_GETFD)&FD_CLOEXEC);
    ++active->restored;
    if (active->restore_error) return -ESTALE;
    *out=malloc(1); CHECK(*out); ++active->objects;
    if (active->drift_after_restore) active->drift=1;
    return 0;
}
int __real_ae_lifetime_close(struct ae_lifetime **);
int __wrap_ae_lifetime_close(struct ae_lifetime **out)
{
    if (!active) return __real_ae_lifetime_close(out);
    if (*out) { free(*out); *out=NULL; --active->objects; ++active->closed; }
    return 0;
}
static int descriptors(void)
{ DIR *d=opendir("/proc/self/fd"); CHECK(d); int n=0; while (readdir(d)) ++n; CHECK(closedir(d)==0); return n; }
static void close_handles(int *fds)
{ for (int n=0;n<4;++n) { CHECK(close(fds[n])==0); fds[n]=-1; } }
int __real_sd_bus_get_property_string(sd_bus *,const char *,const char *,const char *,const char *,sd_bus_error *,char **);
int __wrap_sd_bus_get_property_string(sd_bus *bus,const char *destination,const char *path,const char *interface,const char *key,sd_bus_error *error,char **out)
{
    if (!active) return __real_sd_bus_get_property_string(bus,destination,path,interface,key,error,out);
    CHECK(strcmp(interface,"org.freedesktop.systemd1.Service")==0); ++active->properties;
    const char *value=NULL;
    if (strcmp(key,"NotifyAccess")==0) value=active->store_bad==1 ? "all" : "main";
    else { CHECK(strcmp(key,"FileDescriptorStorePreserve")==0); value=active->store_bad==2 ? "no" : "yes"; }
    *out=strdup(value); CHECK(*out); return 1;
}
int __real_sd_bus_get_property_trivial(sd_bus *,const char *,const char *,const char *,const char *,sd_bus_error *,char,void *);
int __wrap_sd_bus_get_property_trivial(sd_bus *bus,const char *destination,const char *path,const char *interface,const char *key,sd_bus_error *error,char type,void *out)
{
    if (!active) return __real_sd_bus_get_property_trivial(bus,destination,path,interface,key,error,type,out);
    CHECK(type=='u' && strcmp(interface,"org.freedesktop.systemd1.Service")==0); ++active->properties;
    if (strcmp(key,"FileDescriptorStoreMax")==0) *(uint32_t *)out=active->store_bad==3 ? 3U : 4U;
    else { CHECK(strcmp(key,"NFileDescriptorStore")==0); *(uint32_t *)out=(uint32_t)active->held; }
    return 1;
}
int __real_sd_pid_notify_with_fds(pid_t,int,const char *,const int *,unsigned int);
int __wrap_sd_pid_notify_with_fds(pid_t pid,int unset,const char *message,const int *fds,unsigned int count)
{
    if (!active) return __real_sd_pid_notify_with_fds(pid,unset,message,fds,count);
    const char *prefix="FDSTORE=1\nFDPOLL=0\nFDNAME=";
    CHECK(!pid && !unset && count==1 && active->sends<4 && strncmp(message,prefix,strlen(prefix))==0);
    const char *label=strstr(message,"FDNAME="); CHECK(label && strlen(label+7)<AE_HANDOFF_NAME_BYTES);
    int slot=active->sends++; strcpy(active->labels[slot],label+7);
    active->saved[slot]=fcntl(fds[0],F_DUPFD_CLOEXEC,3); CHECK(active->saved[slot]>=3); ++active->held;
    return active->send_fail==active->sends ? -EIO : 1;
}
int __real_sd_notify(int,const char *);
int __wrap_sd_notify(int unset,const char *message)
{
    if (!active) return __real_sd_notify(unset,message);
    const char *prefix="FDSTOREREMOVE=1\nFDNAME="; CHECK(!unset && strncmp(message,prefix,strlen(prefix))==0);
    int found=0; for (int n=0;n<4;++n) if (strcmp(message+strlen(prefix),active->labels[n])==0 && active->saved[n]>=0) {
        CHECK(close(active->saved[n])==0); active->saved[n]=-1; --active->held; ++found;
    }
    CHECK(found==1); ++active->removes;
    return active->remove_fail==active->removes ? -EIO : 1;
}
int __real_sd_notify_barrier(int,uint64_t);
int __wrap_sd_notify_barrier(int unset,uint64_t timeout)
{
    if (!active) return __real_sd_notify_barrier(unset,timeout);
    CHECK(!unset && timeout==UINT64_C(2000000)); ++active->barriers;
    return active->barrier_fail==active->barriers ? -EIO : 1;
}
static void unchanged_restore(struct ae_handoff_service *s,const int *fds,size_t count,int error)
{
    struct ae_lifetime *observer=NULL; struct ae_handoff_snapshot out,marker; memset(&marker,0xa5,sizeof marker); out=marker;
    CHECK(ae_handoff_service_restore(s,fds,count,125003,&observer,&out)==error && !observer && memcmp(&out,&marker,sizeof out)==0 && !active->objects);
    for (size_t n=0;n<count;++n) if (fds[n]>=0) CHECK(fcntl(fds[n],F_GETFD)>=0);
}
/* Real SQLite v5 owner transactions/lost acknowledgements, controlled Linux
 * adapters. /dev/null handles exercise ownership ONLY, never kernel admission.
 * The separate confined integration supplies actual PREPARE observations. */
void ae_test_handoff_service(struct ae_catalogue *store,const struct ae_execution_profile *p,
    const struct ae_execution_handoff *h,const char *scenario)
{
    CHECK(getuid()==0 && geteuid()==0); int baseline=descriptors();
    struct adapters a={0}; a.self.uid=0; memcpy(a.self.boot,p->boot,16); memcpy(a.self.invocation,h->custodian_invocation,16);
    a.self.cgroup_id=99; strcpy(a.self.unit,h->custodian); strcpy(a.self.cgroup,"/system.slice/test-root.service"); strcpy(a.self.label,"system_u:system_r:aetheri_system_t:s0");
    a.original=(struct ae_lifetime_checkpoint){.root=h->root,.group=h->group,.pid_namespace=h->pid_namespace,
        .cgroup_namespace=h->cgroup_namespace,.events_inode=h->events_inode,.events_mount=h->events_mount,
        .events_mode=h->events_mode,.events_uid=h->events_uid,.events_gid=h->events_gid,.pid=h->process.pid};
    a.original.context.uid=h->process.uid; a.original.context.cgroup_id=h->process.cgroup_id;
    memcpy(a.original.context.boot,h->process.boot,16); memcpy(a.original.context.invocation,h->process.invocation,16);
    strcpy(a.original.context.unit,h->process.unit); strcpy(a.original.context.label,h->process.label); strcpy(a.original.context.cgroup,h->process.cgroup);
    memcpy(a.original.process.boot,h->process.boot,16); a.original.process.pidfd_id=h->process.pidfd_id;
    struct ae_handoff_service *s=NULL; a.service=&s; active=&a;
    CHECK(ae_catalogue_upgrade_handoffs(store)==0);
    sd_bus *bus=NULL; CHECK(sd_bus_new(&bus)>=0);
    a.drift=1; CHECK(ae_handoff_service_new(store,bus,p,&a.self,&s)==-ESTALE && !s); a.drift=0;
    CHECK(ae_handoff_service_new(store,bus,p,&a.self,&s)==0);
    struct ae_handoff_transfer out=AE_HANDOFF_TRANSFER_INIT,marker=AE_HANDOFF_TRANSFER_INIT;
    memset(&marker.journal,0xa5,sizeof marker.journal); out=marker;
    struct ae_lifetime *source=(struct ae_lifetime *)&live_original;
    struct ae_lifetime_checkpoint original=a.original;
    /* Distinct original evidence fields cannot be replaced by a plausible set. */
    for (int n=0;n<10;++n) {
        a.original=original;
        if (n==0) ++a.original.pid; else if (n==1) --a.original.process.pidfd_id; else if (n==2) a.original.process.boot[0]^=1;
        else if (n==3) ++a.original.context.uid; else if (n==4) a.original.context.invocation[0]^=1;
        else if (n==5) ++a.original.context.cgroup_id; else if (n==6) a.original.context.label[0]^=1;
        else if (n==7) a.original.context.unit[0]^=1; else if (n==8) a.original.context.cgroup[0]^=1; else a.original.context.boot[0]^=1;
        CHECK(ae_handoff_service_prepare(s,source,125001,&out)==-ESTALE && memcmp(&out,&marker,sizeof out)==0);
    }
    a.original=original;
    a.commit_fault=1; CHECK(ae_handoff_service_prepare(s,source,125001,&out)==-EIO && memcmp(&out,&marker,sizeof out)==0);
    struct ae_handoff_snapshot absent; CHECK(ae_catalogue_get_handoff(store,p,&absent)==-ENOENT);
    int lost=strstr(scenario,"lost")!=NULL;
    if (lost) { a.commit_fault=2; CHECK(ae_handoff_service_prepare(s,source,125001,&out)==-EIO && memcmp(&out,&marker,sizeof out)==0); }
    CHECK(ae_handoff_service_prepare(s,source,125002,&out)==0 && out.journal.transfer_required==!lost && out.journal.snapshot.state==AE_HANDOFF_PENDING);
    int fds[4];
    for (int n=0;n<4;++n) {
        CHECK(lost ? out.handles[n]==-1 : out.handles[n]>=3);
        /* Controlled delivered-set fixture; no transfer is inferred after loss. */
        fds[n]=open("/dev/null",O_RDONLY|O_CLOEXEC); CHECK(fds[n]>=3);
        if (out.handles[n]>=0) CHECK(close(out.handles[n])==0);
        out.handles[n]=-1;
    }
    CHECK(ae_handoff_service_prepare(s,source,999999,&out)==0 && !out.journal.transfer_required);
    for (int n=0;n<4;++n) CHECK(out.handles[n]==-1);
    unchanged_restore(s,fds,4,-ESTALE); CHECK(!a.restored);
    uint8_t runtime[16]; memcpy(runtime,p->system_runtime,16); runtime[15]^=0x20;
    CHECK(ae_catalogue_begin_runtime(store,p->boot,runtime)==0);
    CHECK(ae_handoff_service_prepare(s,source,125003,&out)==-ESTALE);
    CHECK(ae_handoff_service_close(&s)==0);
    /* Same service but still-original invocation is not replacement admission. */
    CHECK(ae_handoff_service_new(store,bus,p,&a.self,&s)==0); unchanged_restore(s,fds,4,-ESTALE); CHECK(ae_handoff_service_close(&s)==0);
    a.self.invocation[15]^=0x40; CHECK(ae_handoff_service_new(store,bus,p,&a.self,&s)==0);
    unchanged_restore(s,fds,3,-EINVAL);
    int duplicate[4]={fds[0],fds[1],fds[2],fds[2]}; unchanged_restore(s,duplicate,4,-EINVAL);
    CHECK(fcntl(fds[0],F_SETFD,0)==0); unchanged_restore(s,fds,4,-EINVAL); CHECK(fcntl(fds[0],F_SETFD,FD_CLOEXEC)==0);
    a.drift=1; unchanged_restore(s,fds,4,-ESTALE); a.drift=0;
    unchanged_restore(s,fds,4,-ESTALE); CHECK(ae_handoff_service_close(&s)==0 && ae_handoff_service_new(store,bus,p,&a.self,&s)==0);
    a.restore_error=1; unchanged_restore(s,fds,4,-ESTALE); a.restore_error=0;
    a.drift_after_restore=1; unchanged_restore(s,fds,4,-ESTALE); a.drift_after_restore=0; a.drift=0;
    unchanged_restore(s,fds,4,-ESTALE); CHECK(ae_handoff_service_close(&s)==0 && ae_handoff_service_new(store,bus,p,&a.self,&s)==0);
    a.commit_fault=1; unchanged_restore(s,fds,4,-EIO);
    CHECK(ae_catalogue_get_handoff(store,p,&absent)==0 && absent.state==AE_HANDOFF_PENDING);
    a.commit_fault=2; unchanged_restore(s,fds,4,-EIO);
    CHECK(ae_catalogue_get_handoff(store,p,&absent)==0 && absent.state==AE_HANDOFF_OBSERVED && absent.revision==2);
    struct ae_lifetime *observer=NULL; struct ae_handoff_snapshot snapshot;
    CHECK(ae_handoff_service_restore(s,fds,4,999999,&observer,&snapshot)==0 && observer && snapshot.revision==2 && memcmp(snapshot.observer_runtime,runtime,16)==0);
    CHECK(ae_lifetime_close(&observer)==0 && !a.objects);
    pid_t child=fork(); CHECK(child>=0);
    if (!child) { CHECK(ae_handoff_service_prepare(s,source,125004,&out)==-ECHILD && ae_handoff_service_close(&s)==-ECHILD); _exit(0); }
    int status; CHECK(waitpid(child,&status,0)==child && WIFEXITED(status) && !WEXITSTATUS(status));
    CHECK(ae_handoff_service_close(&s)==0);
    runtime[15]^=0x10; CHECK(ae_catalogue_begin_runtime(store,p->boot,runtime)==0); a.self.invocation[15]^=0x20;
    CHECK(ae_handoff_service_new(store,bus,p,&a.self,&s)==0 && ae_handoff_service_restore(s,fds,4,125005,&observer,&snapshot)==0 && snapshot.revision==3);
    CHECK(memcmp(snapshot.original.custodian_invocation,h->custodian_invocation,16)==0 && ae_lifetime_close(&observer)==0 && !a.objects);
    close_handles(fds); CHECK(ae_handoff_service_close(&s)==0 && ae_handoff_service_close(&s)==0); sd_bus_unref(bus); active=NULL;
    CHECK(descriptors()==baseline);
    puts("Handoff bridge: actual v5 journal, immutable origins/lost commits/ownership/runtime guards; controlled kernel adapters, no transport or lease release.");
}
static struct adapters transport;
static struct ae_handoff_service *transport_service;
static sd_bus *transport_bus;
static int transport_baseline;
static void transport_init(const struct ae_execution_profile *p,const struct ae_execution_handoff *h)
{
    transport=(struct adapters){.service=&transport_service,.saved={-1,-1,-1,-1}};
    struct adapters *a=&transport; a->self.uid=0; memcpy(a->self.boot,p->boot,16); memcpy(a->self.invocation,h->custodian_invocation,16);
    a->self.cgroup_id=99; strcpy(a->self.unit,h->custodian); strcpy(a->self.cgroup,"/system.slice/test-root.service"); strcpy(a->self.label,"system_u:system_r:aetheri_system_t:s0");
    a->original=(struct ae_lifetime_checkpoint){.root=h->root,.group=h->group,.pid_namespace=h->pid_namespace,
        .cgroup_namespace=h->cgroup_namespace,.events_inode=h->events_inode,.events_mount=h->events_mount,
        .events_mode=h->events_mode,.events_uid=h->events_uid,.events_gid=h->events_gid,.pid=h->process.pid};
    a->original.context.uid=h->process.uid; a->original.context.cgroup_id=h->process.cgroup_id;
    memcpy(a->original.context.boot,h->process.boot,16); memcpy(a->original.context.invocation,h->process.invocation,16);
    strcpy(a->original.context.unit,h->process.unit); strcpy(a->original.context.label,h->process.label); strcpy(a->original.context.cgroup,h->process.cgroup);
    memcpy(a->original.process.boot,h->process.boot,16); a->original.process.pidfd_id=h->process.pidfd_id; active=a;
}
void ae_test_handoff_transport_begin(struct ae_catalogue *store,const struct ae_execution_profile *p,
    const struct ae_execution_handoff *h,const char *scenario)
{
    transport_baseline=descriptors(); transport_init(p,h); CHECK(ae_catalogue_upgrade_handoffs(store)==0 && sd_bus_new(&transport_bus)>=0);
    CHECK(ae_handoff_service_new(store,transport_bus,p,&transport.self,&transport_service)==0);
    struct ae_handoff_snapshot out,marker; memset(&marker,0xa5,sizeof marker); out=marker;
    struct ae_lifetime *source=(struct ae_lifetime *)&live_original;
    for (int n=1;n<=3;++n) {
        transport.store_bad=n;
        CHECK(ae_handoff_service_send(transport_service,source,125001,&out)==-ESTALE && memcmp(&out,&marker,sizeof out)==0 && !transport.sends);
        CHECK(ae_handoff_service_close(&transport_service)==0); transport.store_bad=0;
        CHECK(ae_handoff_service_new(store,transport_bus,p,&transport.self,&transport_service)==0);
    }
    if (strstr(scenario,"send-lost")) transport.send_fail=3;
    if (strstr(scenario,"barrier-lost")) transport.barrier_fail=1;
    int r=ae_handoff_service_send(transport_service,source,125001,&out);
    int lost=transport.send_fail || transport.barrier_fail;
    CHECK(lost ? r==-EIO && memcmp(&out,&marker,sizeof out)==0 : r==0 && out.state==AE_HANDOFF_PENDING);
    int sent=transport.sends;
    CHECK(ae_handoff_service_send(transport_service,source,125002,&out)==-EALREADY && transport.sends==sent);
    CHECK(ae_handoff_service_close(&transport_service)==0);
    puts("Store adapter submitted at most one fresh four-slot transfer; unknown effects preserved.");
}
void ae_test_handoff_transport_finish(struct ae_catalogue *store,const struct ae_execution_profile *p,const char *scenario)
{
    uint8_t runtime[16]; memcpy(runtime,p->system_runtime,16); runtime[15]^=0x20;
    CHECK(ae_catalogue_begin_runtime(store,p->boot,runtime)==0); transport.self.invocation[15]^=0x40;
    CHECK(ae_handoff_service_new(store,transport_bus,p,&transport.self,&transport_service)==0);
    struct ae_handoff_snapshot out,marker; memset(&marker,0xa5,sizeof marker); out=marker;
    struct ae_lifetime *observer=NULL;
    if (transport.held==3) {
        const char *labels[3]={transport.labels[0],transport.labels[1],transport.labels[2]};
        CHECK(ae_handoff_service_receive(transport_service,transport.saved,labels,3,125003,&observer,&out)==-EINVAL && !observer && memcmp(&out,&marker,sizeof out)==0);
        goto done;
    }
    CHECK(transport.held==4);
    const char *labels[4]={transport.labels[3],transport.labels[1],transport.labels[0],transport.labels[2]};
    int borrowed[4]={transport.saved[3],transport.saved[1],transport.saved[0],transport.saved[2]};
    const char *wrong[4]={labels[0],labels[1],labels[2],"foreign"};
    CHECK(ae_handoff_service_receive(transport_service,borrowed,wrong,4,125003,&observer,&out)==-ESTALE && !observer && memcmp(&out,&marker,sizeof out)==0);
    wrong[3]=labels[0];
    CHECK(ae_handoff_service_receive(transport_service,borrowed,wrong,4,125003,&observer,&out)==-ESTALE && !observer);
    CHECK(ae_handoff_service_receive(transport_service,borrowed,labels,4,125003,&observer,&out)==0 && observer && out.state==AE_HANDOFF_OBSERVED);
    CHECK(ae_lifetime_close(&observer)==0);
    /* Saved FDs model PID1's copies only. The restore adapter owns/frees its
     * separate observer. The test never closes a caller FD during receive. */
    if (strstr(scenario,"retire-lost")) transport.remove_fail=3;
    if (strstr(scenario,"retire-commit")) { transport.commit_fault=2; transport.commit_stage=AE_HANDOFF_RETIRE; }
    if (strstr(scenario,"retired-commit")) { transport.commit_fault=2; transport.commit_stage=AE_HANDOFF_RETIRED; }
    out=marker; int r=ae_handoff_service_retire_store(transport_service,125004,&out);
    int lost=transport.remove_fail || transport.commit_stage;
    CHECK(lost ? r==-EIO && memcmp(&out,&marker,sizeof out)==0 : r==0 && out.state==AE_HANDOFF_FINISHED);
    int removed=transport.removes;
    if (transport.held) {
        CHECK(ae_handoff_service_retire_store(transport_service,125005,&out)==-EAGAIN && transport.removes==removed);
    } else {
        transport.held=2; transport.store_bad=1; int properties=transport.properties;
        CHECK(ae_handoff_service_retire_store(transport_service,125005,&out)==0 && out.state==AE_HANDOFF_FINISHED && transport.removes==removed && transport.properties==properties);
        transport.held=0; transport.store_bad=0;
    }
done:
    for (int n=0;n<4;++n) if (transport.saved[n]>=0) CHECK(close(transport.saved[n])==0);
    CHECK(ae_handoff_service_close(&transport_service)==0 && !transport.objects); sd_bus_unref(transport_bus); transport_bus=NULL; active=NULL;
    CHECK(descriptors()==transport_baseline);
    puts("Real owner journal with controlled store notifications/counts: exact receive, lost/partial effects, fresh-only retirement and immutable completed retry passed.");
}
