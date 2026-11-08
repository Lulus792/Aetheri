/* Native integration driver. Its text protocol is test-only, never a UI API. */
#define _GNU_SOURCE
#include "audio/control.h"
#include <errno.h>
#include <fcntl.h>
#include <glib-unix.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

struct fixture {GMainLoop *loop;struct ae_audio_control *control;bool retired;int result;char input[512];size_t used;};
static void hex(const char *text) {for(const unsigned char *p=(const unsigned char *)text;*p;++p) printf("%02x",(unsigned)*p);}
static void snapshot(const struct ae_audio_snapshot *s,void *data) {
    (void)data;
    printf("SNAP %"PRIu64" %d %zu %"PRIu64" %d %d\n",s->revision,s->state,s->count,s->command,s->command_state,s->error);
    for(size_t i=0;i<s->count;++i) {
        const struct ae_audio_output *o=&s->outputs[i];
        printf("OUT %"PRIu64" %"PRIu64" %"PRIu32" %d %d %d %.9g ",o->ref.connection,o->ref.serial,o->ref.id,o->is_default,o->volume_available,o->muted,o->volume);
        hex(o->name);putchar('\n');
    }
    puts("END");fflush(stdout);
}
static void retired(gpointer data) {struct fixture *f=data;f->retired=true;puts("CLOSED");fflush(stdout);g_main_loop_quit(f->loop);}
static void rejected_snapshot(const struct ae_audio_snapshot *s,void *data) {
    struct fixture *f=data;if(s->state==AE_AUDIO_CONNECTING) return;
    f->result=!(s->state==AE_AUDIO_DISCONNECTED && s->error==AE_AUDIO_ERROR_ENDPOINT && !s->count && s->command_state==AE_AUDIO_COMMAND_NONE);
    (void)ae_audio_control_close(&f->control);
}
static gboolean rejected_deadline(gpointer data) {struct fixture *f=data;f->result=1;if(f->control) (void)ae_audio_control_close(&f->control);return G_SOURCE_REMOVE;}
static gpointer wrong_thread(gpointer data) {
    struct fixture *f=data;struct ae_audio_snapshot s;
    bool okay=!ae_audio_control_snapshot(f->control,&s) && ae_audio_control_volume(f->control,0,(struct ae_audio_output_ref){0},0)==AE_AUDIO_INVALID;
    struct ae_audio_control *copy=f->control;okay=okay && !ae_audio_control_close(&copy) && copy==f->control;
    return GINT_TO_POINTER(okay ? 1 : 0);
}
static bool command(struct fixture *f,const char *line) {
    if(!strcmp(line,"close")) return ae_audio_control_close(&f->control);
    if(!strcmp(line,"thread")) {
        GThread *thread=g_thread_new("audio-test",wrong_thread,f);bool okay=GPOINTER_TO_INT(g_thread_join(thread))==1;
        printf("THREAD %d\n",okay);fflush(stdout);return okay;
    }
    char kind[16];uint64_t revision;struct ae_audio_output_ref ref={0};double value=0;int consumed=0;
    if(sscanf(line,"%15s %"SCNu64" %"SCNu64" %"SCNu64" %"SCNu32" %lf%n",kind,&revision,&ref.connection,&ref.serial,&ref.id,&value,&consumed)!=6 || line[consumed]) return false;
    enum ae_audio_submit result;
    if(!strcmp(kind,"volume")) result=ae_audio_control_volume(f->control,revision,ref,value);
    else if(!strcmp(kind,"mute") && (value==0 || value==1)) result=ae_audio_control_mute(f->control,revision,ref,value==1);
    else if(!strcmp(kind,"default") && value==0) result=ae_audio_control_default(f->control,revision,ref);
    else return false;
    printf("RESULT %d\n",result);fflush(stdout);return true;
}
static gboolean input(gint fd,GIOCondition condition,gpointer data) {
    struct fixture *f=data;char bytes[128];ssize_t n=read(fd,bytes,sizeof(bytes));
    if(n<0 && (errno==EINTR || errno==EAGAIN)) return G_SOURCE_CONTINUE;
    if(n<=0 || (condition&G_IO_ERR)) {f->result=1;(void)ae_audio_control_close(&f->control);return G_SOURCE_REMOVE;}
    for(ssize_t i=0;i<n;++i) {
        if(bytes[i]=='\n') {
            f->input[f->used]=0;f->used=0;
            if(!command(f,f->input)) {f->result=1;(void)ae_audio_control_close(&f->control);return G_SOURCE_REMOVE;}
            if(!f->control) return G_SOURCE_REMOVE;
        } else {
            if(!bytes[i] || f->used==sizeof(f->input)-1) {f->result=1;(void)ae_audio_control_close(&f->control);return G_SOURCE_REMOVE;}
            f->input[f->used++]=bytes[i];
        }
    }
    return G_SOURCE_CONTINUE;
}
int main(int argc,char **argv) {
    bool reject_original=argc==3 && !strcmp(argv[2],"--reject-original");
    if(argc!=2 && !(argc==3 && (!strcmp(argv[2],"--close-immediate") || reject_original))) return 2;
    GMainContext *context=g_main_context_new();struct fixture f={.loop=g_main_loop_new(context,FALSE)};
    /* Reject a context that is not installed as this thread's default. */
    if(ae_audio_control_open(context,argv[1],snapshot,&f,retired)) return 2;
    g_main_context_push_thread_default(context);
    if(ae_audio_control_open_original(context,argv[1],-1,snapshot,&f,retired)) return 2;
    int original=reject_original ? (int)syscall(SYS_pidfd_open,getpid(),0) : -1;
    if(reject_original && original<0) return 2;
    f.result=reject_original ? 1 : 0;
    f.control=reject_original ? ae_audio_control_open_original(context,argv[1],original,rejected_snapshot,&f,retired) : ae_audio_control_open(context,argv[1],snapshot,&f,retired);
    if(original>=0) close(original);
    if(!f.control) {g_main_context_pop_thread_default(context);g_main_loop_unref(f.loop);g_main_context_unref(context);return 2;}
    GSource *source=NULL;
    if(reject_original) {source=g_timeout_source_new(5000);g_source_set_callback(source,rejected_deadline,&f,NULL);g_source_attach(source,context);}
    else if(argc==3) (void)ae_audio_control_close(&f.control);
    else {
        if(fcntl(STDIN_FILENO,F_SETFL,O_NONBLOCK)<0) return 2;
        source=g_unix_fd_source_new(STDIN_FILENO,G_IO_IN|G_IO_HUP|G_IO_ERR);
        g_source_set_callback(source,G_SOURCE_FUNC(input),&f,NULL);g_source_attach(source,context);
    }
    if(!f.retired) g_main_loop_run(f.loop);
    if(source) {g_source_destroy(source);g_source_unref(source);}
    if(f.control || !f.retired) f.result=1;
    if(reject_original && !f.result) puts("ORIGINAL_REJECTED no_protocol_fallback=1");
    g_main_context_pop_thread_default(context);g_main_loop_unref(f.loop);g_main_context_unref(context);return f.result;
}
