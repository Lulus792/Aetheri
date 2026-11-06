#define _POSIX_C_SOURCE 200809L
/* Independent libatspi consumer, deliberately without NexaUI/model linkage. */
#include <atspi/atspi.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
static double now(void) {struct timespec t;if(clock_gettime(CLOCK_MONOTONIC,&t)) exit(2);return (double)t.tv_sec+(double)t.tv_nsec/1e9;}
static void pump(void) {while(g_main_context_iteration(NULL,FALSE)) {}struct timespec t={0,1000000};while(nanosleep(&t,&t) && errno==EINTR) {}}
static AtspiAccessible *target(AtspiAccessible *desktop,unsigned long pid,double end) {
    do {
        atspi_accessible_clear_cache(desktop);GError *e=NULL;int n=atspi_accessible_get_child_count(desktop,&e);
        if(e) {g_error_free(e);pump();continue;}if(n<0 || n>64) return NULL;
        for(int i=0;i<n;++i) {
            AtspiAccessible *a=atspi_accessible_get_child_at_index(desktop,i,&e);
            if(e) {g_error_free(e);e=NULL;continue;}
            guint id=a?atspi_accessible_get_process_id(a,&e):0;
            int same=!e && id==pid;if(e) {g_error_free(e);e=NULL;}
            if(same) return a;
            if(a) g_object_unref(a);
        }
        pump();
    }while(now()<end);return NULL;
}
static AtspiAccessible *find(AtspiAccessible *a,const char *name,unsigned depth) {
    if(!a || depth>4) return NULL;
    atspi_accessible_clear_cache(a);GError *e=NULL;char *s=atspi_accessible_get_name(a,&e);
    int same=!e && s && !strcmp(s,name);g_free(s);if(e) {g_error_free(e);return NULL;}
    if(same) return g_object_ref(a);
    int n=atspi_accessible_get_child_count(a,&e);if(e) {g_error_free(e);return NULL;}
    if(n<0 || n>128) return NULL;
    for(int i=0;i<n;++i) {
        AtspiAccessible *c=atspi_accessible_get_child_at_index(a,i,&e);
        if(e) {g_error_free(e);return NULL;}
        AtspiAccessible *f=find(c,name,depth+1);if(c) g_object_unref(c);if(f) return f;
    }
    return NULL;
}
static int state(AtspiAccessible *a,AtspiStateType wanted) {
    atspi_accessible_clear_cache(a);AtspiStateSet *s=atspi_accessible_get_state_set(a);
    int result=s && atspi_state_set_contains(s,wanted);if(s) g_object_unref(s);return result;
}
#define CHECK(x) do {if(!(x)) {fprintf(stderr,"Terminal AT-SPI %d: %s\n",__LINE__,#x);goto done;}}while(0)
int main(int argc,char **argv) {
    if(argc!=4) return 2;
    char *end;unsigned long pid=strtoul(argv[1],&end,10);if(!pid || *end || pid>2147483647ul) return 2;
    int result=1,initialized=0;AtspiAccessible *desktop=NULL,*app=NULL,*frame=NULL,*group=NULL,*row=NULL,*button=NULL;AtspiText *text=NULL;GError *e=NULL;
    CHECK(atspi_init()==0);initialized=1;atspi_set_timeout(1000,1000);desktop=atspi_get_desktop(0);CHECK(desktop);
    app=target(desktop,pid,now()+3);if(!app) {fputs("target app was not registered\n",stderr);goto done;}
    frame=atspi_accessible_get_child_at_index(app,0,&e);CHECK(frame && !e);
    CHECK(atspi_accessible_get_role(frame,&e)==ATSPI_ROLE_FRAME && !e);
    if(!strcmp(argv[2],"close")) {
        group=find(app,"Terminal output",0);CHECK(group);row=atspi_accessible_get_child_at_index(group,0,&e);CHECK(row && !e);
        button=find(app,argv[3],0);CHECK(button && state(button,ATSPI_STATE_ENABLED));
        AtspiAction *action=atspi_accessible_get_action_iface(button);CHECK(action);
        int accepted=atspi_action_do_action(action,0,&e);g_object_unref(action);CHECK(accepted && !e);
        double limit=now()+4;while(!state(row,ATSPI_STATE_DEFUNCT) && now()<limit) pump();
        CHECK(state(row,ATSPI_STATE_DEFUNCT));
    } else {
        CHECK(!strcmp(argv[2],"focused") || !strcmp(argv[2],"unfocused"));
        int active=!strcmp(argv[2],"focused");double limit=now()+3;
        while(state(frame,ATSPI_STATE_ACTIVE)!=active && now()<limit) {
            /* Reconnect may retire an object after registry discovery. Only an
               explicitly defunct frame permits fresh discovery; a live wrong
               focus state must still fail within the original deadline. */
            if(state(frame,ATSPI_STATE_DEFUNCT)) {
                fputs("retired native frame; refreshing registry identity\n",stderr);
                g_object_unref(frame);frame=NULL;g_object_unref(app);app=NULL;
                app=target(desktop,pid,limit);CHECK(app);
                frame=atspi_accessible_get_child_at_index(app,0,&e);CHECK(frame && !e);
                CHECK(atspi_accessible_get_role(frame,&e)==ATSPI_ROLE_FRAME && !e);
            }
            pump();
        }
        if(state(frame,ATSPI_STATE_ACTIVE)!=active)
            fprintf(stderr,"native frame wanted=%d defunct=%d visible=%d showing=%d\n",active,
                state(frame,ATSPI_STATE_DEFUNCT),state(frame,ATSPI_STATE_VISIBLE),state(frame,ATSPI_STATE_SHOWING));
        CHECK(state(frame,ATSPI_STATE_ACTIVE)==active);
        group=find(app,"Terminal output",0);CHECK(group);
        int n=atspi_accessible_get_child_count(group,&e);CHECK(!e && n>=2 && n<=100);
        for(int i=0;i<n;++i) {
            AtspiAccessible *a=atspi_accessible_get_child_at_index(group,i,&e);CHECK(a && !e);
            AtspiText *t=atspi_accessible_get_text_iface(a);CHECK(t);
            char *value=atspi_text_get_text(t,0,-1,&e);CHECK(value && !e);
            int match=!strncmp(value,argv[3],strlen(argv[3]));
            if(match) {row=a;text=t;g_free(value);break;}
            g_free(value);g_object_unref(t);g_object_unref(a);
        }
        CHECK(row && text && state(row,ATSPI_STATE_READ_ONLY));
        char *value=atspi_text_get_text(text,0,-1,&e);CHECK(value && !e);
        int count=atspi_text_get_character_count(text,&e);int same=!e && count==g_utf8_strlen(value,-1);g_free(value);CHECK(same);
        CHECK(!atspi_accessible_get_editable_text_iface(row));
        int changed=atspi_text_set_caret_offset(text,0,&e);CHECK(!changed);if(e) {g_error_free(e);e=NULL;}
        changed=atspi_text_add_selection(text,0,1,&e);CHECK(!changed);if(e) {g_error_free(e);e=NULL;}
        AtspiComponent *component=atspi_accessible_get_component_iface(row);CHECK(component);
        AtspiRect *rect=atspi_component_get_extents(component,ATSPI_COORD_TYPE_WINDOW,&e);g_object_unref(component);
        CHECK(rect && !e && rect->x>=0 && rect->y>=0 && rect->width>0 && rect->height>0);g_boxed_free(ATSPI_TYPE_RECT,rect);
        value=atspi_text_get_text(text,0,-1,&e);CHECK(value && !e && !strncmp(value,argv[3],strlen(argv[3])));g_free(value);
    }
    puts("TERMINAL_ATSPI passed");result=0;
done:
    if(e) g_error_free(e);
    if(text) g_object_unref(text);
    if(button) g_object_unref(button);
    if(row) g_object_unref(row);
    if(group) g_object_unref(group);
    if(frame) g_object_unref(frame);
    if(app) g_object_unref(app);
    if(desktop) g_object_unref(desktop);
    if(initialized) atspi_exit();
    return result;
}
