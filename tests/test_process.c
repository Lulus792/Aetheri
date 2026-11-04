#include "linux/process.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <selinux/selinux.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/pidfd.h>
#include <sys/stat.h>
#include <unistd.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "line %d: %s\n", __LINE__, #x); exit(1); } } while (0)

static unsigned int fault, identity_calls;
static int inode_fault,identity_fd=-1;
int __real_fstat(int,struct stat *);
int __wrap_fstat(int fd,struct stat *value)
{
    if (fd==identity_fd && inode_fault==1) { errno=EACCES; return -1; }
    int r=__real_fstat(fd,value);
    if (r==0 && fd==identity_fd && inode_fault==2) value->st_ino=0;
    return r;
}
int __real_ioctl(int fd, unsigned long request, ...);
int __wrap_ioctl(int fd, unsigned long request, ...)
{
    va_list arguments;
    va_start(arguments, request);
    struct pidfd_info *info = va_arg(arguments, struct pidfd_info *);
    va_end(arguments);
    CHECK(request == PIDFD_GET_INFO);
    if (fault == 2) { errno = ENOTTY; return -1; }
    int result = __real_ioctl(fd, request, info);
    if (result == 0) {
        ++identity_calls;
        if (fault == 1) info->mask &= ~PIDFD_INFO_CREDS;
        if (fault == 3) info->cgroupid ^= 1U;
        if (fault == 4 && identity_calls == 2) info->euid ^= 1U;
    }
    return result;
}

static int short_job;
int __wrap_sd_bus_message_read(sd_bus_message *message,const char *types,...)
{
    va_list args; va_start(args,types); int r=sd_bus_message_readv(message,types,args); va_end(args);
    if (r>0 && short_job && strcmp(types,"uo")==0) { short_job=0; return 0; }
    return r;
}
static unsigned int main_fault,main_calls,job_fault,job_calls;
int __real_sd_bus_get_property_trivial(sd_bus *,const char *,const char *,const char *,const char *,sd_bus_error *,char,void *);
int __wrap_sd_bus_get_property_trivial(sd_bus *bus,const char *destination,const char *path,const char *interface,
    const char *member,sd_bus_error *error,char type,void *value)
{
    int r=__real_sd_bus_get_property_trivial(bus,destination,path,interface,member,error,type,value);
    if (r<0 || !main_fault || strcmp(interface,"org.freedesktop.systemd1.Service")!=0) return r;
    CHECK(type=='u');
    if (strcmp(member,"MainPID")==0) {
        ++main_calls;
        if (main_fault==1 || (main_fault==5 && main_calls==2)) *(uint32_t *)value=0;
        if (main_fault==2) *(uint32_t *)value^=1U;
        if (main_fault==3) *(uint32_t *)value=UINT32_MAX;
        if (main_fault==4) return -EIO;
        if (main_fault==7) return -ENODATA;
        if (main_fault==8) return -EBADMSG;
    } else if (strcmp(member,"ControlPID")==0 && main_fault==6) *(uint32_t *)value=1;
    return r;
}
static void job_entry(sd_bus_message *reply,unsigned int kind)
{
    CHECK(sd_bus_message_open_container(reply,'e',"sv")>=0 && sd_bus_message_append(reply,"s","Job")>=0);
    if (kind==4) {
        CHECK(sd_bus_message_open_container(reply,'v',"s")>=0 && sd_bus_message_append(reply,"s","wrong type")>=0);
    } else {
        CHECK(sd_bus_message_open_container(reply,'v',"(uo)")>=0);
        CHECK(sd_bus_message_append(reply,"(uo)",kind==1 ? 1U : 0U,(kind==1 || kind==5) ? "/org/freedesktop/systemd1/job/1" : "/")>=0);
    }
    CHECK(sd_bus_message_close_container(reply)>=0 && sd_bus_message_close_container(reply)>=0);
}
/* Real PID1 context throughout; only the returned Job property is controlled.
 * Copy other properties from the actual reply, not a synthetic actor envelope. */
int __wrap_sd_bus_call_method(sd_bus *bus,const char *destination,const char *path,const char *interface,
    const char *member,sd_bus_error *error,sd_bus_message **reply,const char *types,...)
{
    va_list args; va_start(args,types); const char *requested=NULL;
    if (types && strcmp(types,"s")==0) { va_list copy; va_copy(copy,args); requested=va_arg(copy,const char *); va_end(copy); }
    int r=sd_bus_call_methodv(bus,destination,path,interface,member,error,reply,types,args); va_end(args);
    if (r<0 || !job_fault || strcmp(destination,"org.freedesktop.systemd1")!=0 || strcmp(member,"GetAll")!=0 ||
        !requested || strcmp(requested,"org.freedesktop.systemd1.Unit")!=0) return r;
    ++job_calls; if (job_fault==6 && job_calls==1) return r;
    CHECK(reply && *reply); sd_bus_message *call=NULL,*changed=NULL,*original=*reply;
    CHECK(sd_bus_message_new_method_call(bus,&call,destination,path,interface,member)>=0 && sd_bus_message_append(call,"s",requested)>=0 &&
        sd_bus_message_seal(call,1,0)>=0 && sd_bus_message_new_method_return(call,&changed)>=0);
    CHECK(sd_bus_message_enter_container(original,'a',"{sv}")>0 && sd_bus_message_open_container(changed,'a',"{sv}")>=0);
    while ((r=sd_bus_message_enter_container(original,'e',"sv"))>0) {
        const char *key=NULL; CHECK(sd_bus_message_read(original,"s",&key)>0);
        if (strcmp(key,"Job")==0) {
            CHECK(sd_bus_message_skip(original,"v")>=0);
            unsigned int kind=job_fault==6 ? 1U : job_fault;
            if (kind!=2) job_entry(changed,kind);
            if (kind==3) job_entry(changed,kind);
        } else {
            CHECK(sd_bus_message_open_container(changed,'e',"sv")>=0 && sd_bus_message_append(changed,"s",key)>=0 &&
                sd_bus_message_copy(changed,original,0)>=0 && sd_bus_message_close_container(changed)>=0);
        }
        CHECK(sd_bus_message_exit_container(original)>0);
    }
    CHECK(r==0 && sd_bus_message_exit_container(original)>0 && sd_bus_message_close_container(changed)>=0 &&
        sd_bus_message_seal(changed,2,0)>=0 && sd_bus_message_rewind(changed,1)>=0);
    sd_bus_message_unref(call); sd_bus_message_unref(original); *reply=changed; return 1;
}

struct server {
    sd_bus *system;
    struct ae_peer retained;
    struct ae_process_context context;
    struct ae_process_identity identity;
    char unit[256];
    unsigned int calls,helpers;
};

static int descriptors(void)
{
    DIR *directory = opendir("/proc/self/fd");
    CHECK(directory != NULL);
    int count = 0;
    while (readdir(directory) != NULL) ++count;
    CHECK(closedir(directory) == 0);
    return count;
}

static int check_call(sd_bus_message *message, void *userdata, sd_bus_error *error)
{
    (void)error;
    struct server *state = userdata;
    CHECK(state->calls < 2);
    struct ae_peer peer = AE_PEER_INIT;
    CHECK(ae_peer_capture(message, &peer) == 0);
    struct ae_process_context actual = {0};
    int result = ae_process_observe(&peer, state->system, &actual);
    if (result != 0) fprintf(stderr, "observe: %s (%d)\n", strerror(-result), result);
    CHECK(result == 0 && actual.uid == geteuid() && strcmp(actual.unit, state->unit) == 0);
    CHECK(ae_process_verify(&peer, state->system, &actual) == 0);
    CHECK(state->helpers==state->calls+1);
    struct ae_process_context main_context;
    CHECK(ae_process_observe_main(&peer,state->system,&main_context)==0 && ae_process_context_equal(&main_context,&actual));
    CHECK(ae_process_verify_main(&peer,state->system,&actual)==0);
    if (state->calls == 1) {
        CHECK(strcmp(actual.unit, state->context.unit) == 0 && memcmp(actual.boot, state->context.boot, sizeof actual.boot) == 0);
        CHECK(memcmp(actual.invocation, state->context.invocation, sizeof actual.invocation) != 0);
        CHECK(ae_process_verify(&peer, state->system, &state->context) == -ESTALE);
        CHECK(ae_process_verify_main(&peer,state->system,&state->context)==-ESTALE);
        CHECK(ae_peer_alive(&state->retained) == -ESRCH);
        CHECK(ae_process_reidentify(&state->identity,&peer,state->system,&actual)==-ESTALE);
        ae_peer_clear(&state->retained);
        state->context = actual;
        state->retained = peer;
        ++state->calls;
        return sd_bus_reply_method_return(message, "");
    }
    printf("process: uid=%u unit=%s label=%s cgroup-id=%llu\n", (unsigned int)actual.uid, actual.unit, actual.label, (unsigned long long)actual.cgroup_id);
    fflush(stdout);
    for (unsigned int field = 0; field < 7; ++field) {
        struct ae_process_context wrong = actual;
        switch (field) {
            case 0: wrong.uid ^= 1U; break;
            case 1: wrong.boot[0] ^= 1U; break;
            case 2: wrong.invocation[0] ^= 1U; break;
            case 3: wrong.cgroup_id ^= 1U; break;
            case 4: wrong.unit[0] ^= 1; break;
            case 5: wrong.cgroup[0] ^= 1; break;
            default: wrong.label[0] ^= 1; break;
        }
        CHECK(ae_process_verify(&peer, state->system, &wrong) == -ESTALE);
        CHECK(ae_process_verify_main(&peer,state->system,&wrong)==-ESTALE);
    }
    struct ae_process_context untouched;
    memset(&untouched, 0xa5, sizeof untouched);
    struct ae_process_context sentinel = untouched;
    ++peer.connection_pid;
    CHECK(ae_process_observe(&peer, state->system, &untouched) == -EACCES);
    CHECK(memcmp(&untouched, &sentinel, sizeof sentinel) == 0);
    --peer.connection_pid;
    peer.connection_label[0] ^= 1;
    CHECK(ae_process_observe(&peer, state->system, &untouched) == -ESTALE);
    CHECK(memcmp(&untouched, &sentinel, sizeof sentinel) == 0);
    peer.connection_label[0] ^= 1;
    const int failures[] = {0, -ENODATA, -ENOTTY, -ESTALE, -EACCES};
    for (fault = 1; fault <= 4; ++fault) {
        identity_calls = 0;
        CHECK(ae_process_observe(&peer, state->system, &untouched) == failures[fault]);
        CHECK(memcmp(&untouched, &sentinel, sizeof sentinel) == 0);
    }
    fault = 0;
    const int main_errors[]={0,-EACCES,-EACCES,-EACCES,-EIO,-EACCES,-EAGAIN,-ENODATA,-EBADMSG};
    int main_baseline=descriptors();
    for (main_fault=1;main_fault<=8;++main_fault) {
        main_calls=0; untouched=sentinel;
        CHECK(ae_process_observe_main(&peer,state->system,&untouched)==main_errors[main_fault]);
        CHECK(memcmp(&untouched,&sentinel,sizeof untouched)==0 && descriptors()==main_baseline);
        if (main_fault==5) CHECK(main_calls==2);
    }
    main_fault=0;
    const int job_errors[]={0,-EAGAIN,-ENODATA,-EBADMSG,0,-EBADMSG,-EAGAIN};
    for (job_fault=1;job_fault<=6;++job_fault) {
        job_calls=0; untouched=sentinel; int failed=ae_process_observe_main(&peer,state->system,&untouched);
        CHECK(job_fault==4 ? failed<0 : failed==job_errors[job_fault]);
        CHECK(memcmp(&untouched,&sentinel,sizeof untouched)==0 && descriptors()==main_baseline);
        if (job_fault==6) CHECK(job_calls==2);
    }
    job_fault=0;
    short_job=1; untouched=sentinel;
    CHECK(ae_process_observe_main(&peer,state->system,&untouched)==-EBADMSG && !short_job && memcmp(&untouched,&sentinel,sizeof untouched)==0);
    CHECK(descriptors()==main_baseline);
    CHECK(ae_process_observe_main(&peer,state->system,&untouched)==0);
    struct ae_process_identity identity,identity_marker; memset(&identity_marker,0xa5,sizeof identity_marker);
    identity_fd=peer.pidfd;
    for (inode_fault=1;inode_fault<=2;++inode_fault) {
        identity=identity_marker;
        CHECK(ae_process_identify(&peer,state->system,&actual,&identity)==(inode_fault==1 ? -EACCES : -ENODATA));
        CHECK(memcmp(&identity,&identity_marker,sizeof identity)==0);
    }
    inode_fault=0; identity_fd=-1;
    CHECK(ae_process_identify(&peer,state->system,&actual,&state->identity)==0);
    int baseline = descriptors();
    for (unsigned int n = 0; n < 32; ++n) CHECK(ae_process_verify(&peer, state->system, &actual) == 0);
    CHECK(descriptors() == baseline);
    state->context = actual;
    state->retained = peer;
    ++state->calls;
    return sd_bus_reply_method_return(message, "");
}

static int subordinate_call(sd_bus_message *message,void *userdata,sd_bus_error *error)
{
    (void)error; struct server *s=userdata; struct ae_peer peer=AE_PEER_INIT;
    CHECK(s->helpers==s->calls && s->calls<2 && ae_peer_capture(message,&peer)==0);
    struct ae_process_context actual,marker,unchanged; memset(&marker,0xa5,sizeof marker); unchanged=marker;
    CHECK(ae_process_observe(&peer,s->system,&actual)==0 && actual.uid==1000 && strcmp(actual.unit,s->unit)==0);
    CHECK(ae_process_observe_main(&peer,s->system,&unchanged)==-EACCES && memcmp(&unchanged,&marker,sizeof marker)==0);
    CHECK(ae_process_verify_main(&peer,s->system,&actual)==-EACCES);
    ae_peer_clear(&peer); ++s->helpers; return sd_bus_reply_method_return(message,"");
}

static const sd_bus_vtable methods[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("Check", "", "", check_call, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("Subordinate", "", "", subordinate_call, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_VTABLE_END
};

static void subordinate(const char *destination)
{
    sd_bus *bus=NULL,*system=NULL; CHECK(sd_bus_open_user(&bus)>=0 && sd_bus_open_system(&system)>=0);
    struct ae_process_context context,marker; memset(&marker,0xa5,sizeof marker); context=marker;
    CHECK(ae_process_observe_self(system,&context)==0); context=marker;
    CHECK(ae_process_observe_self_main(system,&context)==-EACCES && memcmp(&context,&marker,sizeof marker)==0);
    sd_bus_error error=SD_BUS_ERROR_NULL;
    CHECK(sd_bus_call_method(bus,destination,"/org/aetheri/TestProcess","org.aetheri.TestProcess1","Subordinate",&error,NULL,"")>=0);
    sd_bus_error_free(&error); sd_bus_flush_close_unref(system); sd_bus_flush_close_unref(bus);
}

static int client(const char *destination)
{
    sd_bus *bus = NULL, *system = NULL;
    CHECK(sd_bus_open_user(&bus) >= 0);
    CHECK(sd_bus_open_system(&system) >= 0 && sd_bus_set_method_call_timeout(system, 2000000) >= 0);
    int baseline = descriptors();
    for (unsigned int n = 0; n < 8; ++n) {
        struct ae_process_context self;
        sd_id128_t invocation;
        CHECK(ae_process_observe_self(system, &self) == 0);
        struct ae_process_context main_self; CHECK(ae_process_observe_self_main(system,&main_self)==0 && ae_process_context_equal(&self,&main_self));
        CHECK(sd_id128_get_invocation(&invocation) == 0 && memcmp(self.invocation, invocation.bytes, sizeof self.invocation) == 0);
    }
    CHECK(descriptors() == baseline);
    pid_t helper=fork(); CHECK(helper>=0);
    if (!helper) { subordinate(destination); _exit(0); }
    int status; CHECK(waitpid(helper,&status,0)==helper && WIFEXITED(status) && WEXITSTATUS(status)==0);
    sd_bus_flush_close_unref(system);
    CHECK(sd_bus_set_method_call_timeout(bus, 15000000) >= 0);
    sd_bus_error error = SD_BUS_ERROR_NULL;
    int result = sd_bus_call_method(bus, destination, "/org/aetheri/TestProcess", "org.aetheri.TestProcess1", "Check", &error, NULL, "");
    if (result < 0) fprintf(stderr, "client: %s\n", error.message == NULL ? strerror(-result) : error.message);
    sd_bus_error_free(&error);
    sd_bus_flush_close_unref(bus);
    return result < 0 ? 1 : 0;
}

static void carrier_guard(void)
{
    int marker = open("/etc/aetheri-p0-carrier", O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    struct stat info;
    CHECK(marker >= 0 && fstat(marker, &info) == 0 && S_ISREG(info.st_mode) && info.st_uid == 0 &&
          info.st_nlink == 1 && (info.st_mode & 0022) == 0);
    CHECK(close(marker) == 0 && security_getenforce() == 1);
    CHECK(stat("/sys/firmware/efi", &info) == 0 && S_ISDIR(info.st_mode));
    FILE *product = fopen("/sys/class/dmi/id/product_name", "r");
    char name[128] = {0};
    CHECK(product != NULL && fgets(name, sizeof name, product) != NULL && fclose(product) == 0);
    CHECK(strcmp(name, "Aetheri P0 disposable VM\n") == 0);
}

int main(int argc, char **argv)
{
    alarm(25);
    if (argc == 3 && strcmp(argv[1], "--client") == 0) return client(argv[2]);
    CHECK(argc == 1 && geteuid() == 1000 && argv[0][0] == '/');
    carrier_guard();
    struct server state = {.retained = AE_PEER_INIT};
    sd_id128_t nonce;
    CHECK(sd_id128_randomize(&nonce) == 0);
    char digits[SD_ID128_STRING_MAX];
    sd_id128_to_string(nonce, digits);
    snprintf(state.unit, sizeof state.unit, "aetheri-process-test-%s.service", digits);
    char unit_option[280];
    snprintf(unit_option, sizeof unit_option, "--unit=%s", state.unit);
    printf("supervised fixture: %s\n", state.unit);
    fflush(stdout);
    sd_bus *bus = NULL;
    CHECK(sd_bus_open_user(&bus) >= 0 && sd_bus_open_system(&state.system) >= 0);
    CHECK(sd_bus_set_method_call_timeout(state.system, 2000000) >= 0);
    CHECK(sd_bus_add_object_vtable(bus, NULL, "/org/aetheri/TestProcess", "org.aetheri.TestProcess1", methods, &state) >= 0);
    const char *unique = NULL;
    CHECK(sd_bus_get_unique_name(bus, &unique) >= 0);
    char *context = NULL;
    CHECK(getcon_raw(&context) == 0);
    char context_option[600];
    int count = snprintf(context_option, sizeof context_option, "--property=SELinuxContext=%s", context);
    CHECK(count > 0 && (size_t)count < sizeof context_option);
    freecon(context);
    for (unsigned int run = 0; run < 2; ++run) {
        pid_t runner = fork();
        CHECK(runner >= 0);
        if (runner == 0) {
            execlp("sudo", "sudo", "-n", "systemd-run", unit_option, "--wait", "--collect", "--service-type=exec", "--uid=1000",
               "--property=NoNewPrivileges=yes", "--property=CapabilityBoundingSet=", "--property=SendSIGKILL=no",
               "--property=RestrictNamespaces=yes", "--property=TimeoutStopSec=5s", "--property=RuntimeMaxSec=20s",
               "--setenv=XDG_RUNTIME_DIR=/run/user/1000", context_option, "--", "/usr/bin/env", argv[0], "--client", unique, (char *)NULL);
            _exit(127);
        }
        while (state.calls <= run) {
            int result = sd_bus_process(bus, NULL);
            CHECK(result >= 0);
            if (result == 0) CHECK(sd_bus_wait(bus, UINT64_MAX) >= 0);
        }
        CHECK(sd_bus_flush(bus) >= 0);
        int status = 0;
        CHECK(waitpid(runner, &status, 0) == runner && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    }
    CHECK(state.helpers==2 && ae_process_verify(&state.retained, state.system, &state.context) == -ESRCH);
    CHECK(ae_process_verify_main(&state.retained,state.system,&state.context)==-ESRCH);
    struct ae_process_identity identity,identity_marker; memset(&identity_marker,0xa5,sizeof identity_marker); identity=identity_marker;
    CHECK(ae_process_identify(&state.retained,state.system,&state.context,&identity)==-ESRCH && memcmp(&identity,&identity_marker,sizeof identity)==0);
    ae_peer_clear(&state.retained);
    sd_bus_flush_close_unref(state.system);
    sd_bus_flush_close_unref(bus);
    puts("process: actual MainPID/self, same-unit subordinate rejection, MainPID/ControlPID/Job fault/drift checks, generic UID/label/invocation/cgroup/identity restart checks and FD balance passed");
    return 0;
}
