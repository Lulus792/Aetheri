#define _POSIX_C_SOURCE 200809L
/* Independent native accessibility peer; no NexaUI or Aetheri model linkage.
   Numeric PID selection is private-test correlation with the parent's held
   child, never production identity or caller admission. */
#include <atspi/atspi.h>
#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

static unsigned visits;
static void pump(void) {
    while (g_main_context_iteration(NULL, FALSE)) {}
    struct timespec pause = {0, 1000000};
    while (nanosleep(&pause, &pause) && errno == EINTR) {}
}
static AtspiAccessible *find_name(AtspiAccessible *node, const char *name, unsigned depth) {
    if (!node || depth > 16 || ++visits > 256) return NULL;
    atspi_accessible_clear_cache(node);
    GError *error = NULL;
    char *observed = atspi_accessible_get_name(node, &error);
    int same = !error && observed && !strcmp(observed, name);
    g_free(observed);
    if (error) { g_error_free(error); return NULL; }
    if (same) return g_object_ref(node);
    int count = atspi_accessible_get_child_count(node, &error);
    if (error) { g_error_free(error); return NULL; }
    if (count < 0 || count > 256) return NULL;
    for (int i = 0; i < count; ++i) {
        AtspiAccessible *child = atspi_accessible_get_child_at_index(node, i, &error);
        if (error) { g_error_free(error); return NULL; }
        AtspiAccessible *found = find_name(child, name, depth + 1);
        if (child) g_object_unref(child);
        if (found) return found;
    }
    return NULL;
}
static AtspiAccessible *lookup(AtspiAccessible *app, const char *name) {
    visits = 0;
    return find_name(app, name, 0);
}
static AtspiAccessible *target(AtspiAccessible *desktop, unsigned long pid) {
    for (unsigned attempt = 0; attempt < 3000; ++attempt) {
        atspi_accessible_clear_cache(desktop);
        GError *error = NULL;
        int count = atspi_accessible_get_child_count(desktop, &error);
        if (error) { g_error_free(error); return NULL; }
        if (count < 0 || count > 16) return NULL;
        for (int i = 0; i < count; ++i) {
            AtspiAccessible *app = atspi_accessible_get_child_at_index(desktop, i, &error);
            if (error) { g_error_free(error); return NULL; }
            if (!app) continue;
            guint current = atspi_accessible_get_process_id(app, &error);
            int matches = !error && current == pid;
            if (error) { g_error_free(error); error = NULL; }
            if (matches) {
                AtspiAccessible *control = lookup(app, "Darstellung");
                if (control) { g_object_unref(control); return app; }
            }
            g_object_unref(app);
        }
        pump();
    }
    return NULL;
}
static int state(AtspiAccessible *item, AtspiStateType desired) {
    atspi_accessible_clear_cache(item);
    AtspiStateSet *states = atspi_accessible_get_state_set(item);
    int result = states && atspi_state_set_contains(states, desired);
    if (states) g_object_unref(states);
    return result;
}
static int has_checked(AtspiAccessible *app, const char *name, int checked) {
    AtspiAccessible *item = lookup(app, name);
    if (!item) return 0;
    GError *error = NULL;
    AtspiRole role = atspi_accessible_get_role(item, &error);
    int okay = !error && role == ATSPI_ROLE_RADIO_BUTTON && state(item, ATSPI_STATE_ENABLED) &&
        state(item, ATSPI_STATE_CHECKED) == checked;
    if (error) g_error_free(error);
    g_object_unref(item);
    return okay;
}
static int press(AtspiAccessible *app, const char *name, int radio) {
    AtspiAccessible *item = NULL;
    for(unsigned i=0;i<2000 && !item;++i) {item=lookup(app,name);if(!item) pump();}
    if (!item) return 0;
    GError *error = NULL;
    AtspiRole role = atspi_accessible_get_role(item, &error);
    int okay = !error && role == (radio ? ATSPI_ROLE_RADIO_BUTTON : ATSPI_ROLE_PUSH_BUTTON) &&
        state(item, ATSPI_STATE_ENABLED);
    if (error) { g_error_free(error); error = NULL; }
    AtspiAction *action = okay ? atspi_accessible_get_action_iface(item) : NULL;
    okay = action && atspi_action_do_action(action, 0, &error) && !error;
    if (error) { fprintf(stderr, "native action: %s\n", error->message); g_error_free(error); }
    if (action) g_object_unref(action);
    if (okay && radio) {
        okay = 0;
        for (unsigned i = 0; i < 2000; ++i) {
            if (state(item, ATSPI_STATE_CHECKED)) { okay = 1; break; }
            pump();
        }
    }
    g_object_unref(item);
    return okay;
}
static int size_is(AtspiAccessible *app, int width, int height) {
    GError *error = NULL;
    /* Application and window may share their name. The application has no
       Component; inspect the independently registered first native window. */
    AtspiAccessible *window = atspi_accessible_get_child_at_index(app, 0, &error);
    if(error || !window) {if(error) g_error_free(error);return 0;}
    AtspiComponent *component = atspi_accessible_get_component_iface(window);
    AtspiPoint *size = component ? atspi_component_get_size(component, &error) : NULL;
    int okay = !error && size && size->x == width && size->y == height;
    if(!okay) fprintf(stderr,"native size observed %d x %d; expected %d x %d; error=%s\n",
        size?size->x:-1,size?size->y:-1,width,height,error?error->message:"none");
    if (error) g_error_free(error);
    g_free(size);
    if (component) g_object_unref(component);
    g_object_unref(window);
    return okay;
}
static int expect_checked(AtspiAccessible *app,const char *name,int checked) {
    for(unsigned i=0;i<2000;++i) {if(has_checked(app,name,checked)) return 1;pump();}
    return 0;
}
#define CHECK(test) do {if(!(test)) {fprintf(stderr,"AT-SPI view check failed at %d: %s\n",__LINE__,#test);goto done;}} while(0)
int main(int argc,char **argv) {
    int result=1, initialized=0;AtspiAccessible *desktop=NULL,*app=NULL;
    if(argc!=3) return 2;
    char *end;unsigned long pid=strtoul(argv[1],&end,10);
    if(!pid || *end || pid>2147483647ul) return 2;
    CHECK(atspi_init()==0);initialized=1;atspi_set_timeout(1000,1000);
    desktop=atspi_get_desktop(0);CHECK(desktop);app=target(desktop,pid);
    if(!app) {fputs("target app was not registered\n",stderr);goto done;}
    CHECK(expect_checked(app,"Hoher Kontrast",1));
    CHECK(press(app,"Sprache",0));CHECK(expect_checked(app,"Deutsch",1));
    CHECK(press(app,"Englisch",1));CHECK(expect_checked(app,"English",1));
    CHECK(press(app,"Appearance",0));CHECK(expect_checked(app,"High contrast",1));
    CHECK(press(app,"Light",1));CHECK(expect_checked(app,"Dark",0));
    CHECK(press(app,"High contrast",1));CHECK(expect_checked(app,"200 %",1));
    CHECK(size_is(app,1920,1080));
    CHECK(press(app,"Displays",0));CHECK(press(app,argv[2],1));
    CHECK(expect_checked(app,argv[2],1));CHECK(press(app,"Close",0));
    puts("Independent own-view AT-SPI navigation, localized radios, display selection, geometry and close passed");result=0;
done:
    if(app) g_object_unref(app);
    if(desktop) g_object_unref(desktop);
    if(initialized) atspi_exit();
    return result;
}
