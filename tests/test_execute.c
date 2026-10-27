#include "linux/execute.h"
#include "linux/exec_channel.h"
#include "linux/view.h"
#include "core/digest.h"
#include <errno.h>
#include <elf.h>
#include <fcntl.h>
#include <grp.h>
#include <linux/capability.h>
#include <linux/openat2.h>
#include <poll.h>
#include <selinux/selinux.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/pidfd.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <sys/xattr.h>
#include <unistd.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"execute line %d: %s\n",__LINE__,#x); exit(1); } } while (0)
static const char *scenario;
static int faults;
int __real_setgroups(size_t,const gid_t *);
int __wrap_setgroups(size_t n,const gid_t *v) { if (faults && strcmp(scenario,"groups")==0) { errno=EIO; return -1; } return __real_setgroups(n,v); }
int __real_setresgid(gid_t,gid_t,gid_t);
int __wrap_setresgid(gid_t a,gid_t b,gid_t c) { if (faults && strcmp(scenario,"gid")==0) { errno=EIO; return -1; } return __real_setresgid(a,b,c); }
int __real_setresuid(uid_t,uid_t,uid_t);
int __wrap_setresuid(uid_t a,uid_t b,uid_t c) { if (faults && strcmp(scenario,"uid")==0) { errno=EIO; return -1; } return __real_setresuid(a,b,c); }
int __real_setexeccon_raw(const char *);
int __wrap_setexeccon_raw(const char *v) { if (faults && strcmp(scenario,"context")==0) { errno=EIO; return -1; } return __real_setexeccon_raw(v); }
int __real_close_range(unsigned int,unsigned int,int);
int __wrap_close_range(unsigned int a,unsigned int b,int c) { if (faults && strcmp(scenario,"descriptors")==0) { errno=EIO; return -1; } return __real_close_range(a,b,c); }
int __real_prctl(int,...);
int __wrap_prctl(int option,...)
{
    va_list args; va_start(args,option); unsigned long a=va_arg(args,unsigned long),b=va_arg(args,unsigned long),c=va_arg(args,unsigned long),d=va_arg(args,unsigned long); va_end(args);
    if (faults && strcmp(scenario,"nnp")==0 && option==PR_GET_NO_NEW_PRIVS) return 0;
    if (faults && strcmp(scenario,"ambient")==0 && option==PR_CAP_AMBIENT && a==PR_CAP_AMBIENT_CLEAR_ALL) { errno=EIO; return -1; }
    int r=__real_prctl(option,a,b,c,d);
    if (faults && r==0 && ((strcmp(scenario,"bounds")==0 && option==PR_CAPBSET_DROP && a==CAP_SETPCAP) ||
        (strcmp(scenario,"securebits")==0 && option==PR_SET_SECUREBITS))) { errno=EIO; return -1; }
    return r;
}
int __real_sigprocmask(int,const sigset_t *,sigset_t *);
int __wrap_sigprocmask(int how,const sigset_t *set,sigset_t *old)
{
    if (faults && how==SIG_SETMASK && strcmp(scenario,"signals")==0) { errno=EIO; return -1; }
    int r=__real_sigprocmask(how,set,old);
    /* Successful reporting helpers may change errno. The record must still
     * retain the value saved immediately after the failed exec syscall. */
    if (faults && how==SIG_BLOCK) errno=EPIPE;
    return r;
}
int __real_execveat(int,const char *,char *const *,char *const *,int);
int __wrap_execveat(int fd,const char *path,char *const *a,char *const *e,int flags)
{
    if (strcmp(scenario,"before-exit")==0) _exit(125);
    if (strcmp(scenario,"exec-error")==0 || strcmp(scenario,"broken-reader")==0) { errno=ENOEXEC; return -1; }
    return __real_execveat(fd,path,a,e,flags);
}
long __real_syscall(long,...);
long __wrap_syscall(long call,...)
{
    va_list args; va_start(args,call); long r;
    if (call==SYS_capget || call==SYS_capset) {
        struct __user_cap_header_struct *h=va_arg(args,struct __user_cap_header_struct *);
        struct __user_cap_data_struct *c=va_arg(args,struct __user_cap_data_struct *);
        if (faults && call==SYS_capset && strcmp(scenario,"caps")==0) { errno=EIO; r=-1; }
        else r=__real_syscall(call,h,c);
    } else {
        CHECK(call==SYS_openat2); int fd=va_arg(args,int); const char *path=va_arg(args,const char *);
        struct open_how *how=va_arg(args,struct open_how *); size_t size=va_arg(args,size_t);
        r=__real_syscall(call,fd,path,how,size);
    }
    va_end(args); return r;
}
static int open_dir(int parent,const char *name)
{ int fd=openat(parent,name,O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW); CHECK(fd>=0); return fd; }
static void formats(int base,int source,const struct ae_image *expected)
{
    size_t size=(size_t)expected->size;
    uint8_t *original=malloc(size),*bytes=malloc(size); CHECK(original!=NULL && bytes!=NULL);
    CHECK(pread(source,original,size,0)==(ssize_t)size);
    int writer=openat(base,"invalid",O_RDWR|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600); CHECK(writer>=0 && fchmod(writer,0555)==0);
    int reader=openat(base,"invalid",O_RDONLY|O_CLOEXEC|O_NOFOLLOW); CHECK(reader>=0);
    struct ae_image marker,result; memset(&marker,0xa5,sizeof marker);
    for (int fault=0;fault<22;++fault) {
        memcpy(bytes,original,size); size_t length=size;
        Elf64_Ehdr h; memcpy(&h,bytes,sizeof h);
        if (fault==1) memcpy(h.e_ident,"abcd",4);
        if (fault==2) h.e_ident[EI_CLASS]=ELFCLASS32;
        if (fault==3) h.e_ident[EI_DATA]=ELFDATA2MSB;
        if (fault==4) h.e_machine=EM_AARCH64;
        if (fault==5) h.e_type=ET_REL;
        if (fault==6) h.e_phoff=UINT64_MAX;
        if (fault==7) h.e_phnum=129;
        if (fault==8) h.e_phentsize=1;
        if (fault==9) h.e_entry=UINT64_MAX;
        if (fault==10) h.e_ehsize=1;
        if (fault==11) h.e_ident[EI_ABIVERSION]=1;
        if (fault==12) length=sizeof h-1;
        memcpy(bytes,&h,sizeof h);
        if (fault>=13 && fault<=18) {
            for (uint16_t n=0;n<h.e_phnum;++n) {
                size_t offset=(size_t)h.e_phoff+(size_t)n*sizeof(Elf64_Phdr); Elf64_Phdr p; memcpy(&p,bytes+offset,sizeof p);
                if (fault==13 && p.p_type==PT_LOAD) p.p_filesz=UINT64_MAX;
                if (fault==14 && p.p_type==PT_LOAD && (p.p_flags&PF_X)) p.p_flags|=PF_W;
                if (fault==15 && p.p_type==PT_GNU_STACK) p.p_flags|=PF_X;
                if (fault==16 && p.p_type==PT_GNU_STACK) p.p_type=PT_NULL;
                if (fault==17 && p.p_type==PT_INTERP) bytes[p.p_offset]='x';
                if (fault==18 && p.p_type==PT_INTERP) bytes[p.p_offset+p.p_filesz-1]='x';
                memcpy(bytes+offset,&p,sizeof p);
            }
        }
        CHECK(ftruncate(writer,0)==0 && pwrite(writer,bytes,length,0)==(ssize_t)length);
        uint8_t hash[32]; CHECK(ae_sha256_fd(reader,length,hash)==0);
        if (fault==19) hash[0]^=1;
        if (fault==20) CHECK(fchmod(writer,04555)==0);
        if (fault==21) CHECK(linkat(base,"invalid",base,"alias",0)==0);
        result=marker; int r=ae_image_verify(reader,hash,&result);
        if (fault==0) CHECK(r==0 && !ae_image_equal(&result,expected));
        else CHECK(r<0 && memcmp(&result,&marker,sizeof result)==0);
        if (fault==20) CHECK(fchmod(writer,0555)==0);
        if (fault==21) CHECK(unlinkat(base,"alias",0)==0);
    }
    CHECK(ftruncate(writer,0)==0 && pwrite(writer,original,size,0)==(ssize_t)size);
    uint8_t hash[32]; CHECK(ae_sha256_fd(reader,size,hash)==0);
    struct vfs_cap_data cap={.magic_etc=VFS_CAP_REVISION_2|VFS_CAP_FLAGS_EFFECTIVE}; cap.data[0].permitted=1U<<CAP_SYS_ADMIN;
    CHECK(fsetxattr(writer,"security.capability",&cap,sizeof cap,0)==0);
    result=marker; CHECK(ae_image_verify(reader,hash,&result)==-EACCES && memcmp(&result,&marker,sizeof result)==0);
    CHECK(fremovexattr(writer,"security.capability")==0);
    CHECK(ae_image_verify(reader,hash,&result)==0);
    CHECK(ae_image_verify(writer,hash,&result)==-EACCES);
    CHECK(ae_image_verify(-1,hash,&result)==-EINVAL && ae_image_verify(reader,NULL,&result)==-EINVAL && ae_image_verify(reader,hash,NULL)==-EINVAL);
    CHECK(close(reader)==0 && close(writer)==0 && unlinkat(base,"invalid",0)==0); free(original); free(bytes);
}
static void child(const char *base_path,struct ae_image image,int error_fd,int output,const char *label)
{
    alarm(8);
    CHECK(dup2(output,STDOUT_FILENO)==STDOUT_FILENO);
    struct ae_view_namespace *space=NULL; struct ae_view *cv=NULL,*dv=NULL;
    CHECK(ae_view_namespace_enter(&space)==0);
    /* Reopen in this namespace; inherited directory FDs still refer to the
     * parent's mount instances and are not admitted source/target handles here. */
    int base=open(base_path,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC); CHECK(base>=0);
    int code=open_dir(base,"code");
    int data=open_dir(base,"payload"),root=open_dir(base,"setup"),app=open_dir(root,"app"),slot=open_dir(root,"data");
    struct ae_root_identity ci,di,ai,si;
    CHECK(ae_root_observe(code,&ci)==0 && ae_root_observe(data,&di)==0 && ae_root_observe(app,&ai)==0 && ae_root_observe(slot,&si)==0);
    int cloned=ae_view_clone(space,code,&ci,AE_VIEW_CODE,&cv);
    if (cloned!=0) fprintf(stderr,"code clone: %d\n",cloned);
    CHECK(cloned==0);
    cloned=ae_view_clone(space,data,&di,AE_VIEW_DATA,&dv);
    if (cloned!=0) fprintf(stderr,"data clone: %d\n",cloned);
    CHECK(cloned==0);
    CHECK(ae_view_attach(cv,root,&ai,"app")==0 && ae_view_attach(dv,root,&si,"data")==0);
    int view=openat(root,"app/probe",O_RDONLY|O_CLOEXEC|O_NOFOLLOW); CHECK(view>=0);
    int writable=open_dir(root,"data");
    /* Intentionally open both an authority-like extra FD and a very high one.
     * They must disappear at exec even though neither is explicitly enumerated. */
    CHECK(fcntl(base,F_DUPFD,200)>=200);
    struct __user_cap_header_struct h={.version=_LINUX_CAPABILITY_VERSION_3};
    uint32_t mask=(UINT32_C(1)<<CAP_SETUID)|(UINT32_C(1)<<CAP_SETGID)|(UINT32_C(1)<<CAP_SETPCAP);
    struct __user_cap_data_struct caps[2]={{.effective=mask,.permitted=mask}};
    int trimmed=(int)syscall(SYS_capset,&h,caps);
    if (trimmed!=0) {
        int saved=errno; struct __user_cap_data_struct before[2]={{0}}; CHECK(syscall(SYS_capget,&h,before)==0);
        fprintf(stderr,"trim capabilities failed %d: effective=%x permitted=%x inherited=%x\n",saved,before[0].effective,before[0].permitted,before[0].inheritable);
    }
    CHECK(trimmed==0);
    CHECK(prctl(PR_SET_NO_NEW_PRIVS,1L,0L,0L,0L)==0);
    struct sigaction action={0}; action.sa_handler=SIG_IGN; sigemptyset(&action.sa_mask); CHECK(sigaction(SIGTERM,&action,NULL)==0);
    sigset_t mask_signals; sigemptyset(&mask_signals); sigaddset(&mask_signals,SIGINT); CHECK(sigprocmask(SIG_BLOCK,&mask_signals,NULL)==0);
    char context[560],case_value[128]; CHECK(snprintf(context,sizeof context,"AETHERI_EXPECTED_CONTEXT=%s",label)>0);
    CHECK(snprintf(case_value,sizeof case_value,"AETHERI_SCENARIO=%s",scenario)>0);
    char *arguments[]={"aetheri-exec-probe",NULL};
    char *literal_arguments[11]={"aetheri-exec-probe"};
    for (size_t n=1;n<10;++n) literal_arguments[n]="literal;$HOME";
    if (strcmp(scenario,"arguments")==0) literal_arguments[9]=NULL;
    char *environment[]={"LANG=C.UTF-8",context,case_value,NULL};
    char *bad_environment[]={"LD_PRELOAD=/never/allowed",NULL};
    if (strcmp(scenario,"image")==0) image.digest[0]^=1;
    if (strcmp(scenario,"object")==0) image.inode++;
    struct ae_execute request={.image_fd=view,.data_fd=writable,.error_fd=error_fd,.uid=1000,.gid=1000,.image=&image,.label=label,
        .argv=strcmp(scenario,"arguments")==0 || strcmp(scenario,"too-many-arguments")==0 ? literal_arguments : arguments,
        .envp=strcmp(scenario,"environment")==0 ? bad_environment : environment};
    faults=1; ae_execute_target(&request);
}
int main(int argc,char **argv)
{
    CHECK(argc==4 && geteuid()==0);
    scenario=argv[2]; const char *label=argv[3]; char *current=NULL;
    if (strcmp(label,"current")==0) { CHECK(getcon_raw(&current)==0); label=current; }
    CHECK(strlen(label)<512);
    int base=open(argv[1],O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC); CHECK(base>=0);
    int code=open_dir(base,"code"),image_fd=openat(code,"probe",O_RDONLY|O_NOFOLLOW|O_CLOEXEC); CHECK(image_fd>=0);
    struct stat s; uint8_t hash[32]; CHECK(fstat(image_fd,&s)==0 && s.st_size>0 && ae_sha256_fd(image_fd,(uint64_t)s.st_size,hash)==0);
    struct ae_image image; CHECK(ae_image_verify(image_fd,hash,&image)==0);
    if (strcmp(scenario,"formats")==0) {
        formats(base,image_fd,&image); close(image_fd); close(code); close(base); freecon(current);
        puts("Bounded ELF/image admission rejects malformed headers, executable stack, foreign loader, aliases, set-ID and file capabilities before exec."); return 0;
    }
    struct ae_image marker,result; memset(&marker,0xa5,sizeof marker);
    struct ae_exec_channel *channel=NULL; int errors[2]={-1,-1},output[2];
    CHECK(ae_exec_channel_new(&channel,&errors[1])==0 && pipe2(output,O_CLOEXEC)==0);
    errors[0]=ae_exec_channel_fd(channel); CHECK(errors[0]>=3);
    /* Close before fork so the child cannot win a race against the reader's
     * disappearance: its reporting write necessarily encounters EPIPE. */
    if (strcmp(scenario,"broken-reader")==0) { CHECK(ae_exec_channel_close(&channel)==0); errors[0]=-1; }
    pid_t pid=fork(); CHECK(pid>=0);
    if (pid==0) { if (errors[0]>=0) close(errors[0]); close(output[0]); child(argv[1],image,errors[1],output[1],label); }
    CHECK(close(errors[1])==0 && close(output[1])==0);
    struct ae_peer peer=AE_PEER_INIT; peer.pidfd=pidfd_open(pid,0); CHECK(peer.pidfd>=0);
    peer.connection_pid=pid; peer.connection_uid=1000; strcpy(peer.connection_label,label);
    int live=strcmp(scenario,"success")==0 || strcmp(scenario,"wrong-observation")==0 || strcmp(scenario,"arguments")==0;
    if (live) {
        struct pollfd item={.fd=output[0],.events=POLLIN}; CHECK(poll(&item,1,9000)>0);
        char ready[6]; ssize_t received=read(output[0],ready,sizeof ready);
        if (received!=6 || memcmp(ready,"ready\n",6)!=0) {
            int early; CHECK(waitpid(pid,&early,0)==pid);
            struct ae_exec_report report={0}; CHECK(ae_exec_channel_read(channel,&report)==0);
            fprintf(stderr,"child before ready: wait=%d channel=%d phase=%d error=%d\n",early,report.state,report.phase,report.error);
            CHECK(0);
        }
        struct ae_exec_report report={0}; CHECK(ae_exec_channel_read(channel,&report)==0 && report.state==AE_EXEC_CHANNEL_UNKNOWN);
        /* Empty CLOEXEC EOF also occurs while the payload is actually alive.
         * It never replaces the following independent held-process/image test. */
        /* This message is deliberately insufficient on its own. Observe the
         * current kernel executable under the owned ProcessFD independently. */
        struct ae_image wrong=image; wrong.inode++;
        result=marker;
        CHECK(ae_image_match_process(&peer,1000,strcmp(scenario,"wrong-observation")==0 ? &wrong : &image,&result)==(strcmp(scenario,"wrong-observation")==0 ? -ESTALE : 0));
        if (strcmp(scenario,"success")==0) {
            CHECK(ae_image_equal(&result,&image));
            /* Also prove executable observation with final-owner credentials
             * and no permitted/effective/inheritable capabilities. The parent
             * remains privileged only for this isolated owned-child harness. */
            pid_t observer=fork(); CHECK(observer>=0);
            if (observer==0) {
                CHECK(setgroups(0,NULL)==0 && setresgid(1000,1000,1000)==0 && setresuid(1000,1000,1000)==0);
                struct __user_cap_header_struct h={.version=_LINUX_CAPABILITY_VERSION_3}; struct __user_cap_data_struct c[2]={{0}};
                CHECK(syscall(SYS_capset,&h,c)==0 && syscall(SYS_capget,&h,c)==0);
                for (size_t n=0;n<2;++n) CHECK(!c[n].effective && !c[n].permitted && !c[n].inheritable);
                struct ae_image observed;
                CHECK(ae_image_match_process(&peer,1000,&image,&observed)==0 && ae_image_equal(&observed,&image));
                _exit(0);
            }
            int observed_status; CHECK(waitpid(observer,&observed_status,0)==observer && WIFEXITED(observed_status) && WEXITSTATUS(observed_status)==0);
            struct ae_peer mismatch=peer; mismatch.connection_uid=1001; result=marker;
            CHECK(ae_image_match_process(&mismatch,1000,&image,&result)==-EACCES && memcmp(&result,&marker,sizeof result)==0);
            mismatch=peer; mismatch.connection_pid++; result=marker;
            CHECK(ae_image_match_process(&mismatch,1000,&image,&result)==-EACCES && memcmp(&result,&marker,sizeof result)==0);
            result=marker; CHECK(ae_image_match_process(&peer,1001,&image,&result)==-EACCES && memcmp(&result,&marker,sizeof result)==0);
            mismatch=peer; strcpy(mismatch.connection_label,"unrelated"); result=marker;
            CHECK(ae_image_match_process(&mismatch,1000,&image,&result)==-ESTALE && memcmp(&result,&marker,sizeof result)==0);
        } else if (strcmp(scenario,"wrong-observation")==0) CHECK(memcmp(&result,&marker,sizeof result)==0);
        else CHECK(ae_image_equal(&result,&image));
        CHECK(pidfd_send_signal(peer.pidfd,SIGTERM,NULL,0)==0);
    }
    int status; CHECK(waitpid(pid,&status,0)==pid && WIFEXITED(status) && WEXITSTATUS(status)==(live ? 0 : 125));
    result=marker; CHECK(ae_image_match_process(&peer,1000,&image,&result)<0 && memcmp(&result,&marker,sizeof result)==0);
    struct ae_exec_report report={0}; if (channel) CHECK(ae_exec_channel_read(channel,&report)==0);
    if (strcmp(scenario,"broken-reader")==0) CHECK(channel==NULL);
    else if (live || strcmp(scenario,"fast-exit")==0 || strcmp(scenario,"before-exit")==0) CHECK(report.state==AE_EXEC_CHANNEL_UNKNOWN && !report.phase && !report.error);
    else {
        CHECK(report.state==AE_EXEC_CHANNEL_FAILURE);
        int phase=strcmp(scenario,"environment")==0 || strcmp(scenario,"nnp")==0 || strcmp(scenario,"too-many-arguments")==0 ? AE_EXEC_PROFILE : strcmp(scenario,"image")==0 || strcmp(scenario,"object")==0 ? AE_EXEC_IMAGE :
            strcmp(scenario,"context")==0 ? AE_EXEC_CONTEXT : strcmp(scenario,"descriptors")==0 ? AE_EXEC_DESCRIPTORS : strcmp(scenario,"caps")==0 ? AE_EXEC_CAPABILITIES :
            strcmp(scenario,"ambient")==0 || strcmp(scenario,"bounds")==0 || strcmp(scenario,"securebits")==0 ? AE_EXEC_CAPABILITIES :
            strcmp(scenario,"groups")==0 ? AE_EXEC_GROUPS : strcmp(scenario,"gid")==0 ? AE_EXEC_GID : strcmp(scenario,"uid")==0 ? AE_EXEC_UID :
            strcmp(scenario,"signals")==0 ? AE_EXEC_SIGNALS : AE_EXEC_CALL;
        int error=strcmp(scenario,"too-many-arguments")==0 ? E2BIG : phase==AE_EXEC_PROFILE ? EACCES : strcmp(scenario,"image")==0 ? EBADMSG : strcmp(scenario,"object")==0 ? ESTALE : phase==AE_EXEC_CALL ? ENOEXEC : EIO;
        CHECK((int)report.phase==phase && report.error==error);
    }
    CHECK(close(output[0])==0 && ae_exec_channel_close(&channel)==0);
    ae_peer_clear(&peer); close(base); close(code); close(image_fd); freecon(current);
    printf("execution boundary %s: actual child drained; no EOF-based exec inference\n",scenario); return 0;
}
