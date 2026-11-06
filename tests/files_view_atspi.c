#define _POSIX_C_SOURCE 200809L
/* Independent native peer: no NexaUI or Files model linkage. PID is private
   test correlation with the parent's retained child, never caller admission. */
#include <atspi/atspi.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
static unsigned visits;
static double scan_deadline;
static double now(void) {struct timespec t;if(clock_gettime(CLOCK_MONOTONIC,&t)) exit(2);return (double)t.tv_sec+(double)t.tv_nsec/1e9;}
static void pump(void) {
    while(g_main_context_iteration(NULL,FALSE)) {}
    struct timespec t={0,1000000};while(nanosleep(&t,&t) && errno==EINTR) {}
}
static AtspiAccessible *find(AtspiAccessible *node,const char *name,AtspiRole role,unsigned depth) {
    if(!node || depth>16 || ++visits>512 || now()>=scan_deadline) return NULL;
    atspi_accessible_clear_cache(node);GError *e=NULL;
    char *value=atspi_accessible_get_name(node,&e);
    int match=!e && value && !strcmp(value,name);g_free(value);
    if(e) {g_error_free(e);return NULL;}
    AtspiRole observed=atspi_accessible_get_role(node,&e);
    if(e) {g_error_free(e);return NULL;}
    if(match && observed==role) return g_object_ref(node);
    int count=atspi_accessible_get_child_count(node,&e);
    if(e) {g_error_free(e);return NULL;}
    if(count<0 || count>512) return NULL;
    for(int i=0;i<count;++i) {
        AtspiAccessible *child=atspi_accessible_get_child_at_index(node,i,&e);
        if(e) {g_error_free(e);return NULL;}
        AtspiAccessible *found=find(child,name,role,depth+1);if(child) g_object_unref(child);
        if(found) return found;
    }
    return NULL;
}
static AtspiAccessible *lookup(AtspiAccessible *app,const char *name,AtspiRole role) {visits=0;scan_deadline=now()+2;return find(app,name,role,0);}
static AtspiAccessible *await(AtspiAccessible *app,const char *name,AtspiRole role) {
    double end=now()+3;do {AtspiAccessible *n=lookup(app,name,role);if(n) return n;pump();}while(now()<end);return NULL;
}
static AtspiAccessible *target(AtspiAccessible *desktop,unsigned long pid) {
    double end=now()+3;
    while(now()<end) {
        atspi_accessible_clear_cache(desktop);GError *e=NULL;
        int count=atspi_accessible_get_child_count(desktop,&e);
        if(e) {g_error_free(e);pump();continue;}
        if(count<0 || count>64) return NULL;
        for(int i=0;i<count;++i) {
            AtspiAccessible *app=atspi_accessible_get_child_at_index(desktop,i,&e);
            if(e) {g_error_free(e);e=NULL;continue;}
            guint value=app?atspi_accessible_get_process_id(app,&e):0;
            int same=!e && value==pid;if(e) {g_error_free(e);e=NULL;}
            if(same) return app;
            if(app) g_object_unref(app);
        }
        pump();
    }
    return NULL;
}
static int state(AtspiAccessible *node,AtspiStateType wanted) {
    atspi_accessible_clear_cache(node);AtspiStateSet *s=atspi_accessible_get_state_set(node);
    int value=s && atspi_state_set_contains(s,wanted);if(s) g_object_unref(s);return value;
}
static int expect_state(AtspiAccessible *node,AtspiStateType wanted) {
    for(unsigned i=0;i<2000;++i) {if(state(node,wanted)) return 1;pump();}return 0;
}
static int activate(AtspiAccessible *node) {
    GError *e=NULL;AtspiAction *a=atspi_accessible_get_action_iface(node);
    int okay=a && atspi_action_do_action(a,0,&e) && !e;
    if(e) {g_error_free(e);} if(a) {g_object_unref(a);} return okay;
}
static int press(AtspiAccessible *app,const char *name) {
    AtspiAccessible *n=await(app,name,ATSPI_ROLE_PUSH_BUTTON);
    int okay=n && state(n,ATSPI_STATE_ENABLED) && activate(n);if(n) g_object_unref(n);return okay;
}
static int selected(AtspiAccessible *app,const char *name) {
    AtspiAccessible *list=lookup(app,"Entries",ATSPI_ROLE_LIST);if(!list) return 0;
    AtspiSelection *selection=atspi_accessible_get_selection_iface(list);GError *e=NULL;
    int count=selection?atspi_selection_get_n_selected_children(selection,&e):0;
    AtspiAccessible *child=count==1 && !e?atspi_selection_get_selected_child(selection,0,&e):NULL;
    char *observed=child && !e?atspi_accessible_get_name(child,&e):NULL;
    int okay=!e && observed && !strcmp(observed,name);g_free(observed);
    if(e) {g_error_free(e);} if(child) {g_object_unref(child);} if(selection) {g_object_unref(selection);} g_object_unref(list);return okay;
}
#define CHECK(x) do {if(!(x)) {fprintf(stderr,"Files AT-SPI %d: %s\n",__LINE__,#x);goto done;}}while(0)
int main(int argc,char **argv) {
    if(argc!=3) return 2;
    char *end;unsigned long pid=strtoul(argv[1],&end,10);if(!pid || *end || pid>2147483647ul) return 2;
    int result=1,initialized=0;AtspiAccessible *desktop=NULL,*app=NULL,*a=NULL,*b=NULL;GError *error=NULL;
    CHECK(atspi_init()==0);initialized=1;atspi_set_timeout(1000,1000);desktop=atspi_get_desktop(0);CHECK(desktop);
    app=target(desktop,pid);if(!app) {fputs("target app was not registered\n",stderr);goto done;}
    if(!strcmp(argv[2],"initial")) {
        a=await(app,"Information",ATSPI_ROLE_PUSH_BUTTON);CHECK(a);g_object_unref(a);a=NULL;
        CHECK(selected(app,"existing"));CHECK(press(app,"Grid"));
        a=await(app,"Grid",ATSPI_ROLE_PUSH_BUTTON);CHECK(a && expect_state(a,ATSPI_STATE_SELECTED));g_object_unref(a);a=NULL;
        a=await(app,"Entries",ATSPI_ROLE_LIST);CHECK(a);
        AtspiSelection *s=atspi_accessible_get_selection_iface(a);CHECK(s);
        int accepted=atspi_selection_select_child(s,1,&error);g_object_unref(s);CHECK(accepted && !error);
        int okay=0;for(unsigned i=0;i<2000;++i) {if(selected(app,"seed.txt")) {okay=1;break;}pump();}CHECK(okay);
    } else if(!strcmp(argv[2],"dialog")) {
        a=await(app,"New file",ATSPI_ROLE_PUSH_BUTTON);CHECK(a && activate(a));
        b=await(app,"New file",ATSPI_ROLE_DIALOG);CHECK(b);g_object_unref(b);b=NULL;
        CHECK(expect_state(a,ATSPI_STATE_DEFUNCT) && !activate(a));g_object_unref(a);a=NULL;
        a=await(app,"Name",ATSPI_ROLE_ENTRY);CHECK(a && expect_state(a,ATSPI_STATE_FOCUSED));
        AtspiEditableText *edit=atspi_accessible_get_editable_text_iface(a);int unsupported=edit==NULL;if(edit) g_object_unref(edit);CHECK(unsupported);
        g_object_unref(a);a=await(app,"OK",ATSPI_ROLE_PUSH_BUTTON);CHECK(a && !state(a,ATSPI_STATE_ENABLED) && !activate(a));
    } else if(!strcmp(argv[2],"invalid")) {
        a=await(app,"Name",ATSPI_ROLE_ENTRY);CHECK(a && expect_state(a,ATSPI_STATE_INVALID_ENTRY));
        char *description=atspi_accessible_get_description(a,&error);
        int described=!error && description && strstr(description,"clear");g_free(description);CHECK(described);
        g_object_unref(a);a=await(app,"OK",ATSPI_ROLE_PUSH_BUTTON);CHECK(a && !state(a,ATSPI_STATE_ENABLED) && !activate(a));
    } else if(!strcmp(argv[2],"cancel")) {
        CHECK(press(app,"Cancel"));a=await(app,"seed.txt",ATSPI_ROLE_LIST_ITEM);CHECK(a && expect_state(a,ATSPI_STATE_FOCUSED));
        CHECK(selected(app,"seed.txt"));
    } else if(!strcmp(argv[2],"info")) {
        a=await(app,"Informationen",ATSPI_ROLE_DIALOG);CHECK(a);
        b=await(app,"Informationen schließen",ATSPI_ROLE_PUSH_BUTTON);CHECK(b && expect_state(b,ATSPI_STATE_FOCUSED));
        AtspiAccessible *parent=atspi_accessible_get_parent(b,&error);CHECK(parent && !error);
        char *name=atspi_accessible_get_name(parent,&error);int okay=name && !strcmp(name,"Informationen") && !error;g_free(name);
        if(okay) okay=atspi_accessible_get_role(parent,&error)==ATSPI_ROLE_DIALOG && !error;
        g_object_unref(parent);CHECK(okay);
        CHECK(activate(b));CHECK(expect_state(a,ATSPI_STATE_DEFUNCT));
    } else if(!strcmp(argv[2],"close")) {CHECK(press(app,"Schließen"));}
    else {fputs("invalid peer mode\n",stderr);goto done;}
    printf("Independent Files AT-SPI %s passed\n",argv[2]);result=0;
done:
    if(error) {g_error_free(error);} if(b) {g_object_unref(b);} if(a) {g_object_unref(a);}
    if(app) {g_object_unref(app);} if(desktop) {g_object_unref(desktop);} if(initialized) {atspi_exit();} return result;
}
