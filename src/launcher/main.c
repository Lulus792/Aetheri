#include "authority/setup.h"
#include "core/digest.h"
#include "linux/view.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/capability.h>
#include <linux/openat2.h>
#include <poll.h>
#include <selinux/selinux.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/signalfd.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <systemd/sd-daemon.h>
#include <systemd/sd-id128.h>
#include <unistd.h>

/* This first supervised implementation ONLY prepares views. It has no exec,
 * shell, plugin, arbitrary argv/path or privilege-changing path. Its exact
 * digest and purpose are bound before dispatch. Setup readiness is not app
 * execution or interactive readiness. A later exec implementation needs its own
 * reviewed dispatch/evidence/drain boundary, not reuse of this no-exec proof. */
static int channel(sd_bus *bus,const char *method,const struct ae_setup_profile *p,const uint8_t hash[32])
{
    sd_bus_message *call=NULL,*reply=NULL; int r=sd_bus_message_new_method_call(bus,&call,"org.aetheri.System1","/org/aetheri/Setup1","org.aetheri.Setup1",method);
    if (r>=0) r=sd_bus_message_append_array(call,'y',p->lease,16);
    if (r>=0) r=sd_bus_message_append_array(call,'y',hash,32);
    if (r>=0) r=sd_bus_call(bus,call,2000000,NULL,&reply);
    const void *op=NULL; size_t size=0; uint64_t revision=0;
    if (r>=0 && (sd_bus_message_read_array(reply,'y',&op,&size)<=0 || size!=16 || memcmp(op,p->root_operation,16)!=0 ||
        sd_bus_message_read(reply,"t",&revision)<=0 || revision!=1 || sd_bus_message_at_end(reply,1)!=1)) r=-ESTALE;
    sd_bus_message_unref(call); sd_bus_message_unref(reply); return r<0 ? r : 0;
}
static int context(void)
{
    int enforcing=security_getenforce(); int nnp=prctl(PR_GET_NO_NEW_PRIVS,0L,0L,0L,0L);
    if (geteuid()!=0 || getuid()!=0 || enforcing!=1 || nnp!=1) {
        fprintf(stderr,"setup context: uid=%u euid=%u enforcing=%d nnp=%d\n",(unsigned)getuid(),(unsigned)geteuid(),enforcing,nnp);
        return -EACCES;
    }
    char *label=NULL; if (getcon_raw(&label)<0) return -errno;
    int r=strcmp(label,"system_u:system_r:aetheri_setup_t:s0")==0 ? 0 : -EACCES; freecon(label);
    struct __user_cap_header_struct header={.version=_LINUX_CAPABILITY_VERSION_3}; struct __user_cap_data_struct caps[2]={{0}};
    if (r==0 && syscall(SYS_capget,&header,caps)!=0) r=-errno;
    uint32_t allowed=(UINT32_C(1)<<CAP_SYS_ADMIN)|(UINT32_C(1)<<CAP_DAC_READ_SEARCH);
    if (r==0 && (caps[0].effective!=allowed || caps[0].permitted!=allowed || (caps[0].inheritable&~allowed)!=0 || caps[1].effective || caps[1].permitted || caps[1].inheritable)) {
        fprintf(stderr,"setup context: capabilities effective=%x permitted=%x inheritable=%x upper=%x/%x/%x\n",caps[0].effective,caps[0].permitted,caps[0].inheritable,caps[1].effective,caps[1].permitted,caps[1].inheritable);
        r=-EACCES;
    }
    /* PID1's SELinux/NNP transition can retain SYS_ADMIN in the inheritable set.
     * Drop only those allowed inherited bits before any registration/effect.
     * No capability is added; permitted/effective remain the exact two above. */
    if (r==0 && caps[0].inheritable!=0) {
        caps[0].inheritable=0;
        if (syscall(SYS_capset,&header,caps)!=0) r=-errno;
        if (r==0 && syscall(SYS_capget,&header,caps)!=0) r=-errno;
        if (r==0 && (caps[0].inheritable!=0 || caps[1].inheritable!=0 || caps[0].effective!=allowed || caps[0].permitted!=allowed || caps[1].effective || caps[1].permitted)) r=-EACCES;
    }
    return r;
}
int main(int argc,char **argv)
{
    (void)argv; if (argc!=1) return 2;
    const char *stage="context"; int r=context(); struct ae_setup_profile p; uint8_t hash[32];
    if (r==0) stage="profile";
    if (r==0) r=ae_setup_load(&p,hash);
    int self=-1; struct stat status; uint8_t actual[32];
    if (r==0) { self=open("/proc/self/exe",O_RDONLY|O_CLOEXEC); if (self<0) r=-errno; }
    if (r==0 && fstat(self,&status)!=0) r=-errno;
    if (r==0 && (!S_ISREG(status.st_mode) || status.st_uid!=0 || (status.st_mode&07777)!=0555 || status.st_size<1)) r=-EACCES;
    if (r==0) r=ae_sha256_fd(self,(uint64_t)status.st_size,actual);
    if (r==0 && memcmp(actual,p.image_digest,32)!=0) r=-ESTALE;
    if (self>=0) close(self);
    sigset_t signals; sigemptyset(&signals); sigaddset(&signals,SIGTERM); sigaddset(&signals,SIGINT);
    if (r==0 && sigprocmask(SIG_BLOCK,&signals,NULL)!=0) r=-errno;
    int signal_fd=r==0 ? signalfd(-1,&signals,SFD_CLOEXEC) : -1;
    if (r==0 && signal_fd<0) r=-errno;
    sd_bus *bus=NULL;
    if (r==0) r=sd_bus_new(&bus);
    if (r>=0) r=sd_bus_set_address(bus,"unix:path=/run/dbus/system_bus_socket");
    if (r>=0) r=sd_bus_set_bus_client(bus,1);
    if (r>=0) r=sd_bus_set_method_call_timeout(bus,2000000);
    if (r>=0) r=sd_bus_start(bus);
    if (r>=0) { stage="register"; r=channel(bus,"RegisterSetup",&p,hash); }
    struct ae_view_namespace *space=NULL; struct ae_view *code=NULL,*data=NULL;
    if (r==0) { stage="namespace"; r=ae_view_namespace_enter(&space); }
    int source=-1,payload=-1,root=-1,app=-1,destination=-1;
    if (r==0) { stage="sources"; r=ae_setup_sources(&p,&source,&payload); }
    struct ae_root_identity code_id,data_id,app_id,destination_id;
    if (r==0) r=ae_root_observe(source,&code_id);
    if (r==0) r=ae_root_observe(payload,&data_id);
    if (r==0) { stage="clone"; r=ae_view_clone(space,source,&code_id,AE_VIEW_CODE,&code); }
    if (r==0) r=ae_view_clone(space,payload,&data_id,AE_VIEW_DATA,&data);
    char launch[33],path[256]; sd_id128_t id;
    if (r==0) {
        memcpy(id.bytes,p.launch,16); sd_id128_to_string(id,launch);
        snprintf(path,sizeof path,"/var/lib/aetheri-p0/launches/%s",launch);
        struct open_how how={.flags=O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC,.resolve=RESOLVE_NO_SYMLINKS|RESOLVE_NO_MAGICLINKS};
        root=(int)syscall(SYS_openat2,AT_FDCWD,path,&how,sizeof how); if (root<0) r=-errno;
    }
    if (r==0) { app=openat(root,"app",O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC); if (app<0) r=-errno; }
    if (r==0) { destination=openat(root,"data",O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC); if (destination<0) r=-errno; }
    if (r==0) r=ae_root_observe(app,&app_id);
    if (r==0) r=ae_root_observe(destination,&destination_id);
    if (r==0) { stage="attach"; r=ae_view_attach(code,root,&app_id,"app"); }
    if (r==0) r=ae_view_attach(data,root,&destination_id,"data");
    enum ae_view_phase phase; struct ae_root_identity observed;
    if (r==0) r=ae_view_check(code,&phase,&observed);
    if (r==0 && phase!=AE_VIEW_ATTACHED) r=-ESTALE;
    if (r==0) r=ae_view_check(data,&phase,&observed);
    if (r==0 && phase!=AE_VIEW_ATTACHED) r=-ESTALE;
    if (r==0) { stage="prepared"; r=channel(bus,"ReportPrepared",&p,hash); }
    if (r==0) {
        stage="notify";
        puts("setup: exact code/data views prepared; target execution is disabled"); fflush(stdout);
        r=sd_notify(0,"READY=1\nSTATUS=Views prepared; no target execution");
        if (r>0) r=0; else if (r==0) r=-ENOTCONN;
    }
    if (r==0) {
        struct pollfd item={.fd=signal_fd,.events=POLLIN}; int ready;
        do { ready=poll(&item,1,30000); } while (ready<0 && errno==EINTR);
        if (ready<0) r=-errno;
        /* Timeout ends this bounded no-exec setup; it is not evidence for the
         * authority. Root still observes actual process/unit drain independently. */
    }
    if (r<0) fprintf(stderr,"Setup failed at %s: %d; preserve journal until observed drain\n",stage,r);
    int fds[]={source,payload,root,app,destination,signal_fd};
    for (size_t n=0;n<sizeof fds/sizeof fds[0];++n) if (fds[n]>=0) close(fds[n]);
    (void)ae_view_close(&code); (void)ae_view_close(&data); (void)ae_view_namespace_close(&space);
    sd_bus_flush_close_unref(bus); return r<0 ? 5 : 0;
}
