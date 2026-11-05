/* Controlled native v3 producer, only in the inherited-socket test executable.
   No production input-method implementation, provider admission or service. */
#include "compositor_text_input.h"
#include "compositor/server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_text_input_v3.h>
enum { INPUTS=8 };
struct entry {
    struct wlr_text_input_v3 *input;struct wl_listener destroy,focus,commit,enable,disable;
};
struct ae_test_text_input {
    struct wlr_text_input_manager_v3 *manager;struct wl_listener created;
    struct entry entries[INPUTS];
};
static void update_focus(struct entry *e) {
    struct wlr_text_input_v3 *t=e->input;
    struct wlr_surface *surface=t->seat ? t->seat->keyboard_state.focused_surface : NULL;
    if (surface && wl_resource_get_client(surface->resource)!=wl_resource_get_client(t->resource)) surface=NULL;
    if (surface==t->focused_surface) return;
    if (t->focused_surface) wlr_text_input_v3_send_leave(t);
    if (surface) wlr_text_input_v3_send_enter(t,surface);
}
static void focus(struct wl_listener *listener,void *data) {
    (void)data;struct entry *e=wl_container_of(listener,e,focus);update_focus(e);
}
static void report(struct entry *e) {
    struct wlr_text_input_v3 *t=e->input;
    printf("IME commit enabled=%d serial=%u area=%d,%d,%d,%d\n",t->current_enabled,t->current_serial,
        t->current.cursor_rectangle.x,t->current.cursor_rectangle.y,t->current.cursor_rectangle.width,t->current.cursor_rectangle.height);
}
static void committed(struct wl_listener *listener,void *data) { (void)data;struct entry *e=wl_container_of(listener,e,commit);report(e); }
static void enabled(struct wl_listener *listener,void *data) { (void)data;struct entry *e=wl_container_of(listener,e,enable);report(e); }
static void disabled(struct wl_listener *listener,void *data) { (void)data;struct entry *e=wl_container_of(listener,e,disable);report(e); }
static void destroyed(struct wl_listener *listener,void *data) {
    (void)data;struct entry *e=wl_container_of(listener,e,destroy);
    wl_list_remove(&e->focus.link);wl_list_remove(&e->commit.link);wl_list_remove(&e->enable.link);wl_list_remove(&e->disable.link);wl_list_remove(&e->destroy.link);*e=(struct entry){0};
}
static void created(struct wl_listener *listener,void *data) {
    struct ae_test_text_input *f=wl_container_of(listener,f,created);struct wlr_text_input_v3 *input=data;
    unsigned i=0;while (i<INPUTS && f->entries[i].input) ++i;
    if (i==INPUTS || !input->seat) { wl_client_post_implementation_error(wl_resource_get_client(input->resource),"test text-input bound");return; }
    struct entry *e=&f->entries[i];e->input=input;e->destroy.notify=destroyed;e->focus.notify=focus;e->commit.notify=committed;e->enable.notify=enabled;e->disable.notify=disabled;
    wl_signal_add(&input->events.destroy,&e->destroy);wl_signal_add(&input->events.commit,&e->commit);
    wl_signal_add(&input->events.enable,&e->enable);wl_signal_add(&input->events.disable,&e->disable);
    wl_signal_add(&input->seat->keyboard_state.events.focus_change,&e->focus);update_focus(e);
}
struct ae_test_text_input *ae_test_text_input_create(struct server *server) {
    struct ae_test_text_input *f=calloc(1,sizeof(*f));if (!f) return NULL;
    f->manager=wlr_text_input_manager_v3_create(server->display);if (!f->manager) { free(f);return NULL; }
    f->created.notify=created;wl_signal_add(&f->manager->events.new_text_input,&f->created);return f;
}
bool ae_test_text_input_command(struct ae_test_text_input *f,unsigned command) {
    if (!f || command>6) return false;
    struct wlr_text_input_v3 *t=NULL;
    for (unsigned i=0;i<INPUTS;++i) if (f->entries[i].input && f->entries[i].input->current_enabled && f->entries[i].input->focused_surface) {
        if (t) return false;
        t=f->entries[i].input;
    }
    if (!t) return false;
    if (command==0) wlr_text_input_v3_send_preedit_string(t,"Gr\xc3\xbc\xc3\x9f" "e",2,6);
    else if (command==1) wlr_text_input_v3_send_commit_string(t,"\xc3\xa9");
    else if (command==2) wlr_text_input_v3_send_preedit_string(t,"",0,0);
    else if (command==3) {
        char long_text[1001];memset(long_text,'x',1000);long_text[1000]=0;wlr_text_input_v3_send_preedit_string(t,long_text,1000,1000);
    } else if (command==4) wlr_text_input_v3_send_preedit_string(t,"bad\033draft",0,0);
    else if (command==5) wlr_text_input_v3_send_commit_string(t,"literal\ncommand");
    else if (command==6) wlr_text_input_v3_send_preedit_string(t,"e\xcc\x81",3,3);
    wlr_text_input_v3_send_done(t);printf("IME produced=%u\n",command);return true;
}
void ae_test_text_input_destroy(struct ae_test_text_input *f) {
    if (!f) return;
    wl_list_remove(&f->created.link);
    for (unsigned i=0;i<INPUTS;++i) if (f->entries[i].input) {
        wl_list_remove(&f->entries[i].destroy.link);wl_list_remove(&f->entries[i].focus.link);wl_list_remove(&f->entries[i].commit.link);
        wl_list_remove(&f->entries[i].enable.link);wl_list_remove(&f->entries[i].disable.link);
    }
    /* Manager remains display-owned; no callbacks retain this fixture now. */
    free(f);
}
