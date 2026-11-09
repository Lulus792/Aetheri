/* Finite ordinary-user fixture; never install as a normal management service. */
#define _GNU_SOURCE
#include "audio/control.h"
#include <grp.h>
#include <inttypes.h>
#include <linux/capability.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <unistd.h>

struct probe {
    GMainLoop *loop;
    struct ae_audio_control *control;
    const char *name,*test;
    double volume;
    bool mute,retired,original,peer_announced;
    unsigned step;
    uint64_t command;
    struct ae_audio_output_ref target;
    int result;
};
static bool equal_ref(struct ae_audio_output_ref a,struct ae_audio_output_ref b) {return a.connection==b.connection && a.serial==b.serial && a.id==b.id;}
static void retired(gpointer data) {struct probe *p=data;p->retired=true;puts("AETHERI_AUDIO_NATIVE_CLOSED retired=1");g_main_loop_quit(p->loop);}
static void reject(struct probe *p,const char *reason) {
    printf("AETHERI_AUDIO_NATIVE_FAIL reason=%s step=%u\n",reason,p->step);p->result=1;
    if(p->control) (void)ae_audio_control_close(&p->control);
}
static void changed(const struct ae_audio_snapshot *s,void *data) {
    struct probe *p=data;
    if(s->state==AE_AUDIO_FAILED || s->state==AE_AUDIO_DISCONNECTED) {
        printf("AETHERI_AUDIO_NATIVE_STATE state=%d error=%d command_state=%d\n",s->state,s->error,s->command_state);
        reject(p,"backend");return;
    }
    if(s->state!=AE_AUDIO_READY) return;
    if(p->original && !p->peer_announced) {p->peer_announced=true;puts("AETHERI_AUDIO_NATIVE_PEER original=1");}
    const struct ae_audio_output *o=NULL;
    for(size_t i=0;i<s->count;++i) if(s->outputs[i].is_default && s->outputs[i].volume_available && !strcmp(s->outputs[i].name,p->name)) {
        if(o) {reject(p,"ambiguous-output");return;}
        o=&s->outputs[i];
    }
    if(!o) {if(p->step) reject(p,"output-lost");return;}
    if(p->step && !equal_ref(o->ref,p->target)) {reject(p,"output-replaced");return;}
    if(p->step && (s->command!=p->command || s->command_state==AE_AUDIO_COMMAND_UNKNOWN)) {reject(p,"uncertain-command");return;}
    if(p->step && s->command_state!=AE_AUDIO_COMMAND_OBSERVED) return;
    if(p->step==2) {
        if(fabs(o->volume-p->volume)>.00001 || o->muted!=p->mute) {reject(p,"readback");return;}
        printf("AETHERI_AUDIO_NATIVE_PASS case=%s node=%s id=%"PRIu32" serial=%"PRIu64" volume=%.6f muted=%d observed=1\n",p->test,o->name,o->ref.id,o->ref.serial,o->volume,o->muted);
        p->result=0;
        /* Deliberately close from the live change callback. Its publishing
         * source must retain the owner until this callback returns. */
        (void)ae_audio_control_close(&p->control);return;
    }
    uint64_t next=s->command+1;struct ae_audio_output_ref target=o->ref;
    enum ae_audio_submit status=p->step==0 ? ae_audio_control_volume(p->control,s->revision,target,p->volume) : ae_audio_control_mute(p->control,s->revision,target,p->mute);
    if(status==AE_AUDIO_STALE) return; /* No submission; await a new snapshot. */
    if(status!=AE_AUDIO_SUBMITTED) {reject(p,"submission");return;}
    p->target=target;p->command=next;++p->step;
}
static gboolean deadline(gpointer data) {
    struct probe *p=data;reject(p,"probe-deadline");
    return G_SOURCE_REMOVE;
}
static bool ordinary(void) {
    struct __user_cap_header_struct h={.version=_LINUX_CAPABILITY_VERSION_3,.pid=0};struct __user_cap_data_struct d[2]={{0}};
    gid_t group=0;int groups=getgroups(1,&group);
    /* PID1's normal initgroups retains the primary group. That gives no new
     * access beyond the already required GID; any other group is rejected. */
    return getuid()==1000 && geteuid()==1000 && getgid()==1000 && getegid()==1000 && (groups==0 || (groups==1 && group==1000)) && prctl(PR_GET_NO_NEW_PRIVS,0,0,0,0)==1 &&
        syscall(SYS_capget,&h,d)==0 && !(d[0].effective|d[0].permitted|d[0].inheritable|d[1].effective|d[1].permitted|d[1].inheritable);
}
int main(int argc,char **argv) {
    bool wait_start=argc==7 && !strcmp(argv[6],"--wait-for-start");
    bool original=(argc==6 || wait_start) && !strcmp(argv[4],"--server-pidfd") && !strcmp(argv[5],"3");
    if((argc!=4 && !original) || argv[1][0]!='/' || !*argv[2] || strlen(argv[2])>=256 || strspn(argv[2],"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._:-")!=strlen(argv[2]) || !ordinary()) return 2;
    struct probe p={.name=argv[2],.test=argv[3],.volume=1,.result=1,.original=original};
    if(!strcmp(p.test,"half")) p.volume=.5;
    else if(!strcmp(p.test,"muted")) p.mute=true;
    else if(strcmp(p.test,"unity")) return 2;
    setvbuf(stdout,NULL,_IOLBF,0);
    if(wait_start) {
        sigset_t signals;sigemptyset(&signals);sigaddset(&signals,SIGUSR1);
        if(sigprocmask(SIG_BLOCK,&signals,NULL)) return 2;
        alarm(20);puts("AETHERI_AUDIO_NATIVE_WAIT start=explicit");
        int number=0;if(sigwait(&signals,&number) || number!=SIGUSR1) return 2;
        alarm(0);if(sigprocmask(SIG_UNBLOCK,&signals,NULL)) return 2;
    }
    printf("AETHERI_AUDIO_NATIVE_ACTOR uid=1000 gid=1000 cap_zero=1 nnp=1 groups=%d\n",getgroups(0,NULL));
    GMainContext *context=g_main_context_new();g_main_context_push_thread_default(context);p.loop=g_main_loop_new(context,FALSE);
    p.control=original ? ae_audio_control_open_original(context,argv[1],3,changed,&p,retired) : ae_audio_control_open(context,argv[1],changed,&p,retired);
    if(original) close(3);
    if(!p.control) {g_main_loop_unref(p.loop);g_main_context_pop_thread_default(context);g_main_context_unref(context);return 1;}
    GSource *guard=g_timeout_source_new(15000);g_source_set_callback(guard,deadline,&p,NULL);g_source_attach(guard,context);
    g_main_loop_run(p.loop);g_source_destroy(guard);g_source_unref(guard);
    if(p.control) (void)ae_audio_control_close(&p.control);
    int result=p.retired ? p.result : 1;
    g_main_loop_unref(p.loop);g_main_context_pop_thread_default(context);g_main_context_unref(context);return result;
}
