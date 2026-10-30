/* Component ordering/fault injection only, not actual starter-domain admission. */
#undef main
#include "launcher/preparation.h"
#include "launcher/execution.h"
#include "linux/exec_channel.h"
#include "linux/execute.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"starter flow line %d: %s\n",__LINE__,#x); exit(1); } } while (0)
int ae_test_starter_entry(int,char **);
static int fault,trace_fd,issued,step,target_calls,closed;
static struct ae_execution_profile profile;
static struct ae_execution_client *client=(struct ae_execution_client *)(uintptr_t)1;
static struct ae_starter_views *views=(struct ae_starter_views *)(uintptr_t)2;
static int visit(char value,int number)
{ CHECK(write(trace_fd,&value,1)==1); CHECK(number==step+1); step=number; return fault==number ? -EIO : 0; }
int __wrap_ae_starter_context(void) { return visit('C',1); }
int __wrap_ae_execution_profile_load(struct ae_execution_profile *p,uint8_t hash[32])
{ int r=visit('P',2); if (!r) { *p=profile; memset(hash,0x34,32); } return r; }
int __wrap_ae_starter_self(const struct ae_execution_profile *p)
{ CHECK(memcmp(p,&profile,sizeof profile)==0); return visit('S',3); }
int __wrap_sd_bus_start(sd_bus *bus) { CHECK(bus); return visit('B',4); }
int __wrap_ae_execution_client_new(sd_bus *bus,const struct ae_execution_profile *p,const char *unit,struct ae_execution_client **out)
{ CHECK(bus && p && strcmp(unit,AE_STARTER_ROOT_UNIT)==0 && *out==NULL); int r=visit('N',5); if (!r) *out=client; return r; }
int __wrap_ae_execution_client_register(struct ae_execution_client *c,int *out)
{ CHECK(c==client && *out<0); int r=visit('R',6); if (!r) { *out=fcntl(issued,F_DUPFD_CLOEXEC,3); CHECK(*out>=3); } CHECK(close(issued)==0); issued=-1; return r; }
int __wrap_ae_starter_views_open(const struct ae_execution_profile *p,struct ae_starter_views **out)
{ CHECK(p && *out==NULL); int r=visit('V',7); if (!r) *out=views; return r; }
int __wrap_ae_starter_views_target(struct ae_starter_views *v,struct ae_starter_target *out)
{
    CHECK(v==views); ++target_calls; CHECK(target_calls<=2 && closed==(target_calls==2));
    int r=visit('T',target_calls==1 ? 8 : 10); if (r) return r;
    *out=(struct ae_starter_target){.image_fd=101,.data_fd=102};
    strcpy(out->command.executable,"bin/probe"); strcpy(out->command.arguments[0],"bin/probe");
    strcpy(out->command.arguments[1],"literal;$HOME"); out->command.arguments[2][0]=0; out->command.argc=fault==11 ? 10U : 3U;
    out->image.digest[0]=0x56; return 0;
}
int __wrap_ae_execution_client_prepared(struct ae_execution_client *c)
{ CHECK(c==client && !closed && target_calls==1); return visit('A',9); }
int __wrap_ae_execution_client_close(struct ae_execution_client **c)
{ if (*c) { CHECK(*c==client && !closed && write(trace_fd,"D",1)==1); closed=1; *c=NULL; } return 0; }
int __wrap_ae_starter_views_close(struct ae_starter_views **v)
{ if (*v) { CHECK(*v==views && closed && write(trace_fd,"W",1)==1); *v=NULL; } return 0; }
_Noreturn void __wrap_ae_execute_target(const struct ae_execute *p)
{
    CHECK(fault==0 && step==10 && closed && target_calls==2 && p->image_fd==101 && p->data_fd==102 &&
        p->uid==1000 && p->gid==1000 && strcmp(p->label,AE_STARTER_TARGET_CONTEXT)==0 && p->image->digest[0]==0x56);
    CHECK(strcmp(p->argv[0],"bin/probe")==0 && strcmp(p->argv[1],"literal;$HOME")==0 && p->argv[2][0]==0 && p->argv[3]==NULL);
    CHECK(strcmp(p->envp[0],"LANG=C.UTF-8")==0 && strcmp(p->envp[1],"AETHERI_EXPECTED_CONTEXT=" AE_STARTER_TARGET_CONTEXT)==0 &&
        strcmp(p->envp[2],"AETHERI_SCENARIO=p0")==0 && p->envp[3]==NULL);
    CHECK(ae_exec_writer_check(p->error_fd,geteuid(),getegid())==0 && write(trace_fd,"E",1)==1); _exit(0);
}
int main(void)
{
    const char *traces[]={"CPSBNRVTADTE","C","CP","CPS","CPSB","CPSBN","CPSBNRD","CPSBNRVD","CPSBNRVTDW","CPSBNRVTADW","CPSBNRVTADTW","CPSBNRVTADTW",""};
    for (fault=0;fault<=12;++fault) {
        struct ae_exec_channel *channel=NULL; int writer=-1,pipefd[2];
        CHECK(ae_exec_channel_new(&channel,&writer)==0 && pipe2(pipefd,O_CLOEXEC)==0);
        int reader=ae_exec_channel_fd(channel); CHECK(reader>=3);
        pid_t pid=fork(); CHECK(pid>=0);
        if (pid==0) {
            CHECK(close(pipefd[0])==0 && close(reader)==0);
            trace_fd=pipefd[1]; issued=writer; step=0; target_calls=0; closed=0;
            char *args[]={"starter",NULL}; int result=ae_test_starter_entry(fault==12 ? 2 : 1,args); CHECK(fault==12 && result==2); _exit(2);
        }
        CHECK(close(writer)==0 && close(pipefd[1])==0);
        int status; CHECK(waitpid(pid,&status,0)==pid && WIFEXITED(status) && WEXITSTATUS(status)==(fault==0 ? 0 : fault==12 ? 2 : 125));
        char got[64]={0}; size_t used=0; ssize_t n;
        while ((n=read(pipefd[0],got+used,sizeof got-1-used))>0) { used+=(size_t)n; CHECK(used<sizeof got-1); }
        CHECK(n==0 && strcmp(got,traces[fault])==0 && close(pipefd[0])==0);
        struct ae_exec_report report; CHECK(ae_exec_channel_read(channel,&report)==0);
        if (fault>=7 && fault<=11) CHECK(report.state==AE_EXEC_CHANNEL_FAILURE && report.phase==AE_EXEC_PROFILE && report.error==(fault==11 ? EBADMSG : EIO));
        else CHECK(report.state==AE_EXEC_CHANNEL_UNKNOWN);
        CHECK(ae_exec_channel_close(&channel)==0);
    }
    for (int sample=0;sample<3;++sample) {
        struct ae_exec_channel *channel=NULL; int writer=-1; CHECK(ae_exec_channel_new(&channel,&writer)==0);
        int reader=ae_exec_channel_fd(channel); pid_t pid=fork(); CHECK(pid>=0);
        if (pid==0) { CHECK(close(reader)==0); ae_execute_fail(writer,sample==1 ? (enum ae_exec_phase)99 : AE_EXEC_CONTEXT,sample==2 ? 0 : EPERM); }
        CHECK(close(writer)==0); int status; CHECK(waitpid(pid,&status,0)==pid && WIFEXITED(status) && WEXITSTATUS(status)==125);
        struct ae_exec_report report; CHECK(ae_exec_channel_read(channel,&report)==0 && report.state==AE_EXEC_CHANNEL_FAILURE &&
            report.phase==(sample==0 ? AE_EXEC_CONTEXT : AE_EXEC_PROFILE) && report.error==(sample==0 ? EPERM : EINVAL));
        CHECK(ae_exec_channel_close(&channel)==0);
    }
    puts("Starter flow: 13 controlled ordering/fault scenarios plus three real failure-frame cases; no exec before exact release/recheck, literal argv and fixed environment passed.");
    return 0;
}
