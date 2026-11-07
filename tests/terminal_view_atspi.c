#define _POSIX_C_SOURCE 200809L
/* Independent libatspi consumer, deliberately without NexaUI/model linkage. */
#include <atspi/atspi.h>
#include <dbus/dbus.h>
#include <errno.h>
#include <limits.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/sysinfo.h>
#include <sys/utsname.h>
#include <unistd.h>
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
static char find_trace[64][256];static size_t find_trace_count;static int find_trace_overflow;
static void find_error(AtspiAccessible *a,const char *operation,const char *wanted,unsigned depth,int index,int count,const GError *error) {
    AtspiObject *o=a?ATSPI_OBJECT(a):NULL;
    fprintf(stderr,"FIND_FAILURE op=%s bus=%s path=%s wanted=%s depth=%u index=%d count=%d domain=%s code=%d error=%s\n",
        operation,o && o->app && o->app->bus_name?o->app->bus_name:"?",o && o->path?o->path:"?",wanted,depth,index,count,
        error?g_quark_to_string(error->domain):"none",error?error->code:0,error?error->message:"none");
}
static AtspiAccessible *find(AtspiAccessible *a,const char *name,unsigned depth) {
    if(!depth) {find_trace_count=0;find_trace_overflow=0;}
    if(!a || depth>4) return NULL;
    atspi_accessible_clear_cache(a);GError *e=NULL;char *s=atspi_accessible_get_name(a,&e);
    AtspiObject *object=ATSPI_OBJECT(a);
    if(find_trace_count<64) snprintf(find_trace[find_trace_count++],sizeof(find_trace[0]),"depth=%u bus=%s path=%s name=%s error=%s",depth,
        object->app && object->app->bus_name?object->app->bus_name:"?",object->path?object->path:"?",s?s:"?",e?e->message:"none");
    else find_trace_overflow=1;
    int same=!e && s && !strcmp(s,name);g_free(s);if(e) {find_error(a,"GetName",name,depth,-1,-1,e);g_error_free(e);return NULL;}
    if(same) return g_object_ref(a);
    int n=atspi_accessible_get_child_count(a,&e);if(e) {find_error(a,"GetChildCount",name,depth,-1,n,e);g_error_free(e);return NULL;}
    if(n<0 || n>128) {find_error(a,"ChildCountBounds",name,depth,-1,n,NULL);return NULL;}
    for(int i=0;i<n;++i) {
        AtspiAccessible *c=atspi_accessible_get_child_at_index(a,i,&e);
        if(e) {find_error(a,"GetChildAtIndex",name,depth,i,n,e);g_error_free(e);return NULL;}
        if(!c) find_error(a,"NullChild",name,depth,i,n,NULL);
        AtspiAccessible *f=find(c,name,depth+1);if(c) g_object_unref(c);if(f) return f;
    }
    if(!depth) {
        find_error(a,"NameNotFound",name,depth,-1,n,NULL);
        for(size_t i=0;i<find_trace_count;++i) fprintf(stderr,"FIND_TRACE %s\n",find_trace[i]);
        fprintf(stderr,"FIND_TRACE_END count=%zu clipped=%d\n",find_trace_count,find_trace_overflow);
    }
    return NULL;
}
/* A pane header and its output group intentionally share a name. Resolve the
   role as well, so the client cannot accidentally read a button as a document. */
static AtspiAccessible *find_role(AtspiAccessible *a,const char *name,AtspiRole role,unsigned depth) {
    if(!a || depth>4) return NULL;
    atspi_accessible_clear_cache(a);GError *error=NULL;
    char *actual=atspi_accessible_get_name(a,&error);
    int same=!error && actual && !strcmp(actual,name);g_free(actual);
    AtspiRole found=error?ATSPI_ROLE_INVALID:atspi_accessible_get_role(a,&error);
    if(error) {g_error_free(error);return NULL;}
    if(same && found==role) return g_object_ref(a);
    int count=atspi_accessible_get_child_count(a,&error);
    if(error) {g_error_free(error);return NULL;}
    if(count<0 || count>128) return NULL;
    for(int i=0;i<count;++i) {
        AtspiAccessible *child=atspi_accessible_get_child_at_index(a,i,&error);
        if(error) {g_error_free(error);return NULL;}
        AtspiAccessible *match=find_role(child,name,role,depth+1);
        if(child) g_object_unref(child);
        if(match) return match;
    }
    return NULL;
}
struct retained_wire {DBusConnection *bus;char *name,*path;};
static int retain_wire(AtspiAccessible *a,struct retained_wire *out) {
    AtspiObject *object=ATSPI_OBJECT(a);
    if(out->bus || !object->app || !object->app->bus || !object->app->bus_name || !object->path) return 0;
    out->bus=dbus_connection_ref(object->app->bus);out->name=g_strdup(object->app->bus_name);out->path=g_strdup(object->path);
    return out->name && out->path;
}
static int wire_retired_text(const struct retained_wire *target) {
    if(!target->bus || !target->name || !target->path) return 0;
    DBusError error;dbus_error_init(&error);
    DBusMessage *query=dbus_message_new_method_call(target->name,target->path,"org.a11y.atspi.Text","GetText");
    dbus_int32_t first=0,last=-1;
    if(!query) return 0;
    if(!dbus_message_append_args(query,DBUS_TYPE_INT32,&first,DBUS_TYPE_INT32,&last,DBUS_TYPE_INVALID)) {dbus_message_unref(query);return 0;}
    DBusMessage *reply=dbus_connection_send_with_reply_and_block(target->bus,query,1000,&error);dbus_message_unref(query);
    int retired=!reply && dbus_error_has_name(&error,"org.a11y.atspi.Error.Defunct");
    fprintf(stderr,"RETIRED_TEXT bus=%s path=%s reply=%d error=%s\n",target->name,target->path,reply!=NULL,error.name?error.name:"none");
    if(reply) dbus_message_unref(reply);
    dbus_error_free(&error);return retired;
}
static void wire_state(AtspiAccessible *a) {
    AtspiObject *o=ATSPI_OBJECT(a);
    if(!o->app || !o->app->bus || !o->app->bus_name || !o->path) {fputs("WIRE_STATE retired client reference\n",stderr);return;}
    DBusError e;dbus_error_init(&e);
    DBusMessage *q=dbus_message_new_method_call(o->app->bus_name,o->path,"org.a11y.atspi.Accessible","GetState");
    if(!q) {fputs("WIRE_STATE allocation failure\n",stderr);return;}
    DBusMessage *r=dbus_connection_send_with_reply_and_block(o->app->bus,q,1000,&e);dbus_message_unref(q);
    if(!r) fprintf(stderr,"WIRE_STATE %s %s %s %s\n",o->app->bus_name,o->path,e.name?e.name:"no error",e.message?e.message:"");
    else {
        DBusMessageIter it,array;dbus_uint32_t bits[2]={0};unsigned count=0;
        if(dbus_message_has_signature(r,"au")) {
            dbus_message_iter_init(r,&it);dbus_message_iter_recurse(&it,&array);
            while(count<2 && dbus_message_iter_get_arg_type(&array)==DBUS_TYPE_UINT32) {dbus_message_iter_get_basic(&array,&bits[count++]);dbus_message_iter_next(&array);}
        }
        fprintf(stderr,"WIRE_STATE %s %s count=%u bits=%08x:%08x\n",o->app->bus_name,o->path,count,bits[1],bits[0]);dbus_message_unref(r);
    }
    dbus_error_free(&e);
}
static int state(AtspiAccessible *a,AtspiStateType wanted) {
    atspi_accessible_clear_cache(a);AtspiStateSet *s=atspi_accessible_get_state_set(a);
    int result=s && atspi_state_set_contains(s,wanted);if(s) g_object_unref(s);
    if(wanted==ATSPI_STATE_ACTIVE && !result) wire_state(a);
    return result;
}
static int await_state(AtspiAccessible *a,AtspiStateType wanted,int value) {
    double end=now()+4;do {if(state(a,wanted)==value) return 1;pump();}while(now()<end);return 0;
}
static void missing_text(AtspiAccessible *a,int index) {
    AtspiObject *o=ATSPI_OBJECT(a);GError *error=NULL;
    char *name=atspi_accessible_get_name(a,&error);
    fprintf(stderr,"MISSING_TEXT index=%d bus=%s path=%s name=%s error=%s\n",index,
        o->app && o->app->bus_name?o->app->bus_name:"?",o->path?o->path:"?",name?name:"?",error?error->message:"none");
    g_free(name);if(error) g_error_free(error);
    wire_state(a);
    if(!o->app || !o->app->bus || !o->app->bus_name || !o->path) return;
    DBusError e;dbus_error_init(&e);
    DBusMessage *q=dbus_message_new_method_call(o->app->bus_name,o->path,"org.a11y.atspi.Accessible","GetInterfaces");
    if(!q) return;
    DBusMessage *r=dbus_connection_send_with_reply_and_block(o->app->bus,q,1000,&e);dbus_message_unref(q);
    if(!r) fprintf(stderr,"WIRE_INTERFACES %s %s\n",e.name?e.name:"?",e.message?e.message:"");
    else {
        fprintf(stderr,"WIRE_INTERFACES signature=%s",dbus_message_get_signature(r));
        if(dbus_message_has_signature(r,"as")) {
            DBusMessageIter it,array;dbus_message_iter_init(r,&it);dbus_message_iter_recurse(&it,&array);
            for(unsigned n=0;n<32 && dbus_message_iter_get_arg_type(&array)==DBUS_TYPE_STRING;++n) {
                const char *s=NULL;dbus_message_iter_get_basic(&array,&s);fprintf(stderr," %s",s?s:"?");dbus_message_iter_next(&array);
            }
        }
        fputc('\n',stderr);dbus_message_unref(r);
    }
    dbus_error_free(&e);
}
#define CHECK(x) do {if(!(x)) {fprintf(stderr,"Terminal AT-SPI %d: %s\n",__LINE__,#x);goto done;}}while(0)
int main(int argc,char **argv) {
    if(argc!=4 && argc!=5) return 2;
    char *end;unsigned long pid=strtoul(argv[1],&end,10);if(!pid || *end || pid>2147483647ul) return 2;
    int control=-1;
    if(argc==5) {long fd=strtol(argv[4],&end,10);if(*end || fd<0 || fd>INT_MAX || (strcmp(argv[2],"retire-row") && strcmp(argv[2],"retire-tab") && strcmp(argv[2],"pane-retire"))) return 2;control=(int)fd;}
    int result=1,initialized=0;AtspiAccessible *desktop=NULL,*app=NULL,*frame=NULL,*group=NULL,*row=NULL,*button=NULL;AtspiText *text=NULL;AtspiSelection *selection=NULL;GError *e=NULL;char *pane_snapshot=NULL;struct retained_wire old_wire={0};
    CHECK(atspi_init()==0);initialized=1;atspi_set_timeout(1000,1000);desktop=atspi_get_desktop(0);CHECK(desktop);
    app=target(desktop,pid,now()+3);if(!app) {fputs("target app was not registered\n",stderr);goto done;}
    frame=atspi_accessible_get_child_at_index(app,0,&e);CHECK(frame && !e);
    CHECK(atspi_accessible_get_role(frame,&e)==ATSPI_ROLE_FRAME && !e);
    if(!strncmp(argv[2],"pane-",5)) {
        const char *separator=strchr(argv[3],'|');char name[96];
        CHECK(separator && separator>argv[3] && (size_t)(separator-argv[3])<sizeof(name) && separator[1]);
        memcpy(name,argv[3],(size_t)(separator-argv[3]));name[separator-argv[3]]='\0';
        int unfocused=!strcmp(argv[2],"pane-unfocused") || !strcmp(argv[2],"pane-background-switch"),blocked=!strcmp(argv[2],"pane-blocked");
        CHECK(await_state(frame,ATSPI_STATE_ACTIVE,!unfocused));
        button=find_role(app,name,ATSPI_ROLE_PUSH_BUTTON,0);CHECK(button);
        if(blocked) CHECK(!state(button,ATSPI_STATE_ENABLED));
        if(!strcmp(argv[2],"pane-switch") || !strcmp(argv[2],"pane-background-switch") || blocked) {
            AtspiAction *action=atspi_accessible_get_action_iface(button);
            int accepted=action?atspi_action_do_action(action,0,&e):0;
            if(action) g_object_unref(action);
            if(blocked) {CHECK(!accepted);if(e) {g_error_free(e);e=NULL;}}
            else CHECK(accepted && !e && await_state(button,ATSPI_STATE_SELECTED,1));
        }
        group=find_role(app,name,ATSPI_ROLE_GROUPING,0);CHECK(group);
        int selected=state(button,ATSPI_STATE_SELECTED),german=!strcmp(argv[2],"pane-description-de");
        const char *wanted=german?(selected?"Ausgewählter Terminal-Bereich":"Terminal-Bereich. Über die Kopfzeile diese Shell auswählen."):
            selected?"Selected terminal pane":"Terminal pane. Activate its header to select this shell.";
        char *description=atspi_accessible_get_description(group,&e);
        int described=!e && description && !strcmp(description,wanted);
        if(!described) fprintf(stderr,"PANE_DESCRIPTION selected=%d wanted=%s actual=%s error=%s\n",selected,wanted,description?description:"null",e?e->message:"none");
        g_free(description);CHECK(described);
        int count=atspi_accessible_get_child_count(group,&e);CHECK(!e && count>=2 && count<=100);
        for(int i=0;i<count;++i) {
            AtspiAccessible *candidate=atspi_accessible_get_child_at_index(group,i,&e);CHECK(candidate && !e);
            AtspiText *candidate_text=atspi_accessible_get_text_iface(candidate);CHECK(candidate_text);
            char *value=atspi_text_get_text(candidate_text,0,-1,&e);CHECK(value && !e);
            int same=!strncmp(value,separator+1,strlen(separator+1));
            if(same) {pane_snapshot=g_strdup(value);g_free(value);CHECK(pane_snapshot);row=candidate;text=candidate_text;break;}
            g_free(value);
            g_object_unref(candidate_text);g_object_unref(candidate);
        }
        CHECK(row && text && state(row,ATSPI_STATE_READ_ONLY));
        CHECK(!atspi_accessible_get_editable_text_iface(row));
        if(!strcmp(argv[2],"pane-reading")) CHECK(await_state(row,ATSPI_STATE_FOCUSED,1));
        if(!strcmp(argv[2],"pane-focus") || !strcmp(argv[2],"pane-no-focus") || !strcmp(argv[2],"pane-retire") || !strcmp(argv[2],"pane-unfocused")) {
            AtspiComponent *component=atspi_accessible_get_component_iface(row);CHECK(component);
            int accepted=atspi_component_grab_focus(component,&e);
            if(!strcmp(argv[2],"pane-no-focus") || unfocused) {CHECK(!accepted && !state(row,ATSPI_STATE_FOCUSED));if(e) {g_error_free(e);e=NULL;}}
            else {CHECK(accepted && !e && await_state(row,ATSPI_STATE_FOCUSED,1));}
            if(!strcmp(argv[2],"pane-retire")) {
                CHECK(control>=0 && retain_wire(row,&old_wire));ssize_t n;
                do {n=write(control,"HELD",4);}while(n<0 && errno==EINTR);CHECK(n==4);
                CHECK(await_state(row,ATSPI_STATE_DEFUNCT,1));
                do {n=write(control,"RETIRED",7);}while(n<0 && errno==EINTR);CHECK(n==7);
                char ack[6];do {n=read(control,ack,sizeof(ack));}while(n<0 && errno==EINTR);
                CHECK(n==6 && !memcmp(ack,"REUSED",6));
                CHECK(!atspi_component_grab_focus(component,&e));if(e) {g_error_free(e);e=NULL;}
                /* A client may still hold its original cached text. It must
                   never read replacement content through the retired path. */
                int retired=wire_retired_text(&old_wire);
                char *value=atspi_text_get_text(text,0,-1,&e);
                int original=!value || !*value || !strcmp(value,pane_snapshot);
                fprintf(stderr,"RETIRED_TEXT_CACHE length=%zu original=%d error=%s\n",value?strlen(value):0,original,e?e->message:"none");g_free(value);
                if(e) {g_error_free(e);e=NULL;}
                CHECK(retired && original);
            }
            g_object_unref(component);
        }
        if(unfocused) CHECK(!state(frame,ATSPI_STATE_ACTIVE) && !state(row,ATSPI_STATE_FOCUSED));
    } else if(!strcmp(argv[2],"tabs") || !strcmp(argv[2],"tabs-disabled") || !strcmp(argv[2],"retire-tab")) {
        int count=-1,selected=-1,next=-2;char extra;
        CHECK(sscanf(argv[3],"%d:%d:%d%c",&count,&selected,&next,&extra)==3 && count>=2 && count<=8 && selected>=0 && selected<count && next>=-1 && next<count);
        group=find(app,"Terminal tabs",0);CHECK(group && atspi_accessible_get_role(group,&e)==ATSPI_ROLE_PAGE_TAB_LIST && !e);
        CHECK(atspi_accessible_get_child_count(group,&e)==count && !e);
        selection=atspi_accessible_get_selection_iface(group);CHECK(selection);
        CHECK(atspi_selection_get_n_selected_children(selection,&e)==1 && !e);
        for(int i=0;i<count;++i) {
            CHECK(atspi_selection_is_child_selected(selection,i,&e)==(i==selected) && !e);
            row=atspi_accessible_get_child_at_index(group,i,&e);CHECK(row && !e);
            CHECK(atspi_accessible_get_role(row,&e)==ATSPI_ROLE_PAGE_TAB && !e);
            char expected[32];snprintf(expected,sizeof(expected),"Tab %d",i+1);
            char *name=atspi_accessible_get_name(row,&e);int same=name && !strcmp(name,expected);g_free(name);CHECK(same && !e);
            CHECK(state(row,ATSPI_STATE_SELECTED)==(i==selected));g_object_unref(row);row=NULL;
        }
        row=atspi_selection_get_selected_child(selection,0,&e);CHECK(row && !e);
        CHECK(atspi_accessible_get_index_in_parent(row,&e)==selected && !e);
        if(!strcmp(argv[2],"retire-tab")) {
            CHECK(control>=0);AtspiAction *action=atspi_accessible_get_action_iface(row);CHECK(action);
            ssize_t sent;do {sent=write(control,"HELD",4);}while(sent<0 && errno==EINTR);CHECK(sent==4);
            CHECK(await_state(row,ATSPI_STATE_DEFUNCT,1));
            do {sent=write(control,"RETIRED",7);}while(sent<0 && errno==EINTR);CHECK(sent==7);
            char acknowledgement[6];ssize_t got;
            do {got=read(control,acknowledgement,sizeof(acknowledgement));}while(got<0 && errno==EINTR);
            CHECK(got==6 && !memcmp(acknowledgement,"REUSED",6));
            int accepted=atspi_action_do_action(action,0,&e);g_object_unref(action);CHECK(!accepted);
            if(e) {g_error_free(e);e=NULL;}
        } else if(!strcmp(argv[2],"tabs-disabled")) {
            CHECK(!state(group,ATSPI_STATE_ENABLED));
            int accepted=atspi_selection_select_child(selection,(selected+1)%count,&e);CHECK(!accepted);
            if(e) {g_error_free(e);e=NULL;}
            CHECK(atspi_selection_is_child_selected(selection,selected,&e) && !e);
        } else if(next>=0) {
            CHECK(state(group,ATSPI_STATE_ENABLED) && await_state(frame,ATSPI_STATE_ACTIVE,1));
            CHECK(atspi_selection_select_child(selection,next,&e) && !e);
            double limit=now()+4;int observed=0;
            do {observed=atspi_selection_is_child_selected(selection,next,&e);CHECK(!e);if(!observed) pump();}while(!observed && now()<limit);
            CHECK(observed && atspi_selection_get_n_selected_children(selection,&e)==1 && !e);
        }
    } else if(!strcmp(argv[2],"startup")) {
        struct utsname kernel;struct sysinfo memory;CHECK(!uname(&kernel) && !sysinfo(&memory));
        uint64_t total=(uint64_t)memory.totalram*memory.mem_unit,gib=UINT64_C(1024)*1024*1024;
        char cpu[96],ram[96],identity[128];long online=sysconf(_SC_NPROCESSORS_ONLN);CHECK(online>0);
        snprintf(cpu,sizeof(cpu),"CPUs    %ld online",online);
        snprintf(ram,sizeof(ram),"Memory  %" PRIu64 ".%" PRIu64 " GiB total",total/gib,(total%gib)*10/gib);
        snprintf(identity,sizeof(identity),"uid %" PRIu64 " @ %s",(uint64_t)getuid(),kernel.nodename);
        group=find(app,"Terminal output",0);CHECK(group);unsigned seen=0;
        int count=atspi_accessible_get_child_count(group,&e);CHECK(!e && count>=2 && count<=100);
        for(int i=0;i<count;++i) {
            row=atspi_accessible_get_child_at_index(group,i,&e);CHECK(row && !e);
            text=atspi_accessible_get_text_iface(row);CHECK(text);
            char *value=atspi_text_get_text(text,0,-1,&e);CHECK(value && !e);
            if(strstr(value,"Aetheri development")) seen|=1u;
            if(strstr(value,"Kernel  ") && strstr(value,kernel.release)) seen|=2u;
            if(strstr(value,"Arch    ") && strstr(value,kernel.machine)) seen|=4u;
            if(strstr(value,cpu)) seen|=8u;
            if(strstr(value,ram)) seen|=16u;
            if(strstr(value,"Uptime  ")) seen|=32u;
            if(strstr(value,"A E T H E R I")) seen|=64u;
            if(!strncmp(value,"aetheri> ",9)) {int caret=atspi_text_get_caret_offset(text,&e);CHECK(!e && caret==9);seen|=128u;}
            if(strstr(value,identity)) seen|=256u;
            g_free(value);g_object_unref(text);text=NULL;g_object_unref(row);row=NULL;
        }
        CHECK(seen==511u);
    } else if(!strcmp(argv[2],"close")) {
        group=find(app,"Terminal output",0);CHECK(group);row=atspi_accessible_get_child_at_index(group,0,&e);CHECK(row && !e);
        button=find(app,argv[3],0);CHECK(button && state(button,ATSPI_STATE_ENABLED));
        AtspiAction *action=atspi_accessible_get_action_iface(button);CHECK(action);
        int accepted=atspi_action_do_action(action,0,&e);g_object_unref(action);CHECK(accepted && !e);
        double limit=now()+4;while(!state(row,ATSPI_STATE_DEFUNCT) && now()<limit) pump();
        CHECK(state(row,ATSPI_STATE_DEFUNCT));
    } else if(!strcmp(argv[2],"chrome") || !strcmp(argv[2],"disabled")) {
        button=find(app,argv[3],0);CHECK(button && atspi_accessible_get_role(button,&e)==ATSPI_ROLE_PUSH_BUTTON && !e);
        AtspiAction *action=atspi_accessible_get_action_iface(button);
        if(!strcmp(argv[2],"disabled")) {
            CHECK(!state(button,ATSPI_STATE_ENABLED));
            if(action) {int count=atspi_action_get_n_actions(action,&e);g_object_unref(action);CHECK(!e && count==0);}
        } else {
            CHECK(action && await_state(frame,ATSPI_STATE_ACTIVE,1) && state(button,ATSPI_STATE_ENABLED));
            int accepted=atspi_action_do_action(action,0,&e);g_object_unref(action);CHECK(accepted && !e);
        }
    } else {
        CHECK(!strcmp(argv[2],"focused") || !strcmp(argv[2],"active-wire-stream") || !strcmp(argv[2],"focused-now") || !strcmp(argv[2],"unfocused") || !strcmp(argv[2],"focus") ||
            !strcmp(argv[2],"focused-row") || !strcmp(argv[2],"reading-row") || !strcmp(argv[2],"no-row-focus") || !strcmp(argv[2],"reject-focus") || !strcmp(argv[2],"retire-row"));
        int active=strcmp(argv[2],"unfocused") && strcmp(argv[2],"reject-focus");double limit=now()+3;
        while(strcmp(argv[2],"focused-now") && state(frame,ATSPI_STATE_ACTIVE)!=active && now()<limit) {
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
        if(!strcmp(argv[2],"active-wire-stream")) {
            double until=now()+4;
            for(unsigned i=0;i<128;++i) {CHECK(now()<until);CHECK(state(frame,ATSPI_STATE_ACTIVE));}
        }
        group=find(app,"Terminal output",0);CHECK(group);
        int n=atspi_accessible_get_child_count(group,&e);CHECK(!e && n>=2 && n<=100);
        for(int i=0;i<n;++i) {
            AtspiAccessible *a=atspi_accessible_get_child_at_index(group,i,&e);CHECK(a && !e);
            AtspiText *t=atspi_accessible_get_text_iface(a);if(!t) missing_text(a,i);CHECK(t);
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
        if(!strcmp(argv[2],"focus") || !strcmp(argv[2],"reject-focus")) {
            AtspiComponent *c=atspi_accessible_get_component_iface(row);CHECK(c && state(row,ATSPI_STATE_FOCUSABLE));
            int accepted=atspi_component_grab_focus(c,&e);g_object_unref(c);
            if(!strcmp(argv[2],"focus")) {CHECK(accepted && !e && await_state(row,ATSPI_STATE_FOCUSED,1));}
            else {CHECK(!accepted && !state(row,ATSPI_STATE_FOCUSED));if(e) {g_error_free(e);e=NULL;}}
        } else if(!strcmp(argv[2],"reading-row")) {
            CHECK(await_state(row,ATSPI_STATE_FOCUSED,1));
            int caret=atspi_text_get_caret_offset(text,&e);CHECK(!e && caret==-1);
        } else if(!strcmp(argv[2],"focused-row")) {
            CHECK(await_state(row,ATSPI_STATE_FOCUSED,1));
            int caret=atspi_text_get_caret_offset(text,&e);CHECK(!e && caret==9);
        } else if(!strcmp(argv[2],"no-row-focus")) CHECK(await_state(row,ATSPI_STATE_FOCUSED,0));
        else if(!strcmp(argv[2],"retire-row")) {
            CHECK(control>=0 && state(row,ATSPI_STATE_FOCUSED));
            AtspiComponent *c=atspi_accessible_get_component_iface(row);CHECK(c);
            ssize_t sent;do {sent=write(control,"HELD",4);}while(sent<0 && errno==EINTR);CHECK(sent==4);
            CHECK(await_state(row,ATSPI_STATE_DEFUNCT,1));
            int accepted=atspi_component_grab_focus(c,&e);g_object_unref(c);CHECK(!accepted);
            if(e) {g_error_free(e);e=NULL;}
        }
    }
    puts("TERMINAL_ATSPI passed");result=0;
done:
    if(old_wire.bus) dbus_connection_unref(old_wire.bus);
    g_free(old_wire.name);g_free(old_wire.path);
    g_free(pane_snapshot);
    if(e) g_error_free(e);
    if(text) g_object_unref(text);
    if(selection) g_object_unref(selection);
    if(button) g_object_unref(button);
    if(row) g_object_unref(row);
    if(group) g_object_unref(group);
    if(frame) g_object_unref(frame);
    if(app) g_object_unref(app);
    if(desktop) g_object_unref(desktop);
    if(initialized) atspi_exit();
    if(control>=0) close(control);
    return result;
}
