/* Built-in, opt-in Compose provider. No external input-method/key injection API. */
#include "text_input.h"
#include "compose.h"
#include "server.h"
#include "aetheri-text-input-v3-protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/input-event-codes.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_text_input_v3.h>
#include <xkbcommon/xkbcommon-keysyms.h>
enum { INPUT_LIMIT=64 };
struct entry {
    struct ae_text_input *owner;struct wlr_text_input_v3 *input;
    struct wl_listener destroy,enable,disable,commit;
    uint32_t serial;bool seen,retired,boundary,accepted,pending_enable;
};
struct ae_text_input {
    struct server *server;struct wlr_seat *seat;struct wlr_text_input_manager_v3 *manager;
    struct wl_listener created,focus;
    struct wl_protocol_logger *requests;
    struct entry entries[INPUT_LIMIT],*active;
    struct wlr_keyboard *keyboard;struct ae_compose *compose;
};
static bool eligible(struct ae_text_input *t,struct entry *e) {
    struct wlr_text_input_v3 *v=e->input;struct wlr_surface *surface=t->seat->keyboard_state.focused_surface;
    return v && e->accepted && !e->retired && v->seat==t->seat && surface && v->focused_surface==surface && v->current_enabled &&
        wl_resource_get_client(v->resource)==wl_resource_get_client(surface->resource) &&
        !(v->current.content_type.hint&(ZWP_TEXT_INPUT_V3_CONTENT_HINT_HIDDEN_TEXT|ZWP_TEXT_INPUT_V3_CONTENT_HINT_SENSITIVE_DATA)) &&
        v->current.content_type.purpose!=ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_PASSWORD &&
        v->current.content_type.purpose!=ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_PIN;
}
static void reset(struct ae_text_input *t,bool send) {
    if (send && ae_compose_active(t->compose) && t->active && t->active->input && t->active->input->focused_surface) {
        wlr_text_input_v3_send_preedit_string(t->active->input,"",0,0);wlr_text_input_v3_send_done(t->active->input);
    }
    ae_compose_reset(t->compose);t->keyboard=NULL;
}
static void select_active(struct ae_text_input *t) {
    struct entry *next=NULL;bool ambiguous=false;
    struct wlr_surface *surface=t->seat->keyboard_state.focused_surface;
    for (unsigned i=0;i<INPUT_LIMIT;++i) if (t->entries[i].input && t->entries[i].accepted && surface && t->entries[i].input->current_enabled &&
        t->entries[i].input->focused_surface==surface &&
        wl_resource_get_client(t->entries[i].input->resource)==wl_resource_get_client(surface->resource)) {
        if (next) ambiguous=true;
        next=&t->entries[i];
    }
    if (ambiguous || (next && !eligible(t,next))) next=NULL;
    if (next!=t->active) { reset(t,true);t->active=next;printf("COMPOSE active=%d\n",next!=NULL); }
}
static void sync_focus(struct ae_text_input *t,struct entry *e) {
    struct wlr_surface *surface=t->seat->keyboard_state.focused_surface;
    if (surface && wl_resource_get_client(surface->resource)!=wl_resource_get_client(e->input->resource)) surface=NULL;
    if (surface==e->input->focused_surface) return;
    if (e->input->focused_surface) wlr_text_input_v3_send_leave(e->input);
    if (surface) wlr_text_input_v3_send_enter(e->input,surface);
}
static void focus(struct wl_listener *listener,void *data) {
    (void)data;struct ae_text_input *t=wl_container_of(listener,t,focus);
    reset(t,true);t->active=NULL;
    for (unsigned i=0;i<INPUT_LIMIT;++i) if (t->entries[i].input) {
        t->entries[i].accepted=false;t->entries[i].boundary=false;t->entries[i].pending_enable=false;
        sync_focus(t,&t->entries[i]);
    }
    select_active(t);
}
/* wlroots 0.20.2 emits ordinary commit for enable->enable. Observe only this
   original resource's zero-argument boundary requests before its handler. Do
   not log, inspect or retain text/secret arguments. Effects wait for commit. */
static void request(void *data,enum wl_protocol_logger_type direction,const struct wl_protocol_logger_message *message) {
    if (direction!=WL_PROTOCOL_LOGGER_REQUEST || message->arguments_count ||
        (strcmp(message->message->name,"enable") && strcmp(message->message->name,"disable"))) return;
    struct ae_text_input *t=data;
    for (unsigned i=0;i<INPUT_LIMIT;++i) if (t->entries[i].input && t->entries[i].input->resource==message->resource) {
        struct entry *e=&t->entries[i];e->boundary=true;
        e->pending_enable=!strcmp(message->message->name,"enable") && e->input->focused_surface &&
            e->input->focused_surface==t->seat->keyboard_state.focused_surface;
        return;
    }
}
static void changed(struct entry *e,bool boundary) {
    struct ae_text_input *t=e->owner;
    bool fresh_enable=e->boundary && e->pending_enable;e->pending_enable=false;
    boundary=boundary || e->boundary;e->boundary=false;
    /* A resource's serial never wraps into another accepted transaction. */
    uint32_t serial=e->input->current_serial;
    if (e->seen && serial<=e->serial) e->retired=true;
    e->seen=true;e->serial=serial;
    if (boundary) {
        if (t->active==e) reset(t,true);
        bool admitted=fresh_enable && !e->retired && e->input->current_enabled && e->input->focused_surface &&
            e->input->focused_surface==t->seat->keyboard_state.focused_surface;
        /* Ignore a competing enable, including while the accepted field is
           secret. It cannot automatically inherit admission after disable. */
        for (unsigned i=0;i<INPUT_LIMIT;++i) if (&t->entries[i]!=e && t->entries[i].input &&
            t->entries[i].accepted && t->entries[i].input->current_enabled) admitted=false;
        e->accepted=admitted;
    }
    if (t->active==e && !eligible(t,e)) reset(t,true);
    select_active(t);
}
static void enabled(struct wl_listener *l,void *d) { (void)d;struct entry *e=wl_container_of(l,e,enable);changed(e,true); }
static void disabled(struct wl_listener *l,void *d) { (void)d;struct entry *e=wl_container_of(l,e,disable);changed(e,true); }
static void committed(struct wl_listener *l,void *d) { (void)d;struct entry *e=wl_container_of(l,e,commit);changed(e,false); }
static void destroyed(struct wl_listener *l,void *d) {
    (void)d;struct entry *e=wl_container_of(l,e,destroy);struct ae_text_input *t=e->owner;
    if (t->active==e) { reset(t,false);t->active=NULL; }
    wl_list_remove(&e->destroy.link);wl_list_remove(&e->enable.link);wl_list_remove(&e->disable.link);wl_list_remove(&e->commit.link);
    *e=(struct entry){0};select_active(t);
}
static void created(struct wl_listener *l,void *data) {
    struct ae_text_input *t=wl_container_of(l,t,created);struct wlr_text_input_v3 *input=data;
    unsigned i=0;while (i<INPUT_LIMIT && t->entries[i].input) ++i;
    if (i==INPUT_LIMIT || input->seat!=t->seat) {
        wl_client_post_implementation_error(wl_resource_get_client(input->resource),"Aetheri text input resource/seat limit");return;
    }
    struct entry *e=&t->entries[i];e->owner=t;e->input=input;
    e->destroy.notify=destroyed;e->enable.notify=enabled;e->disable.notify=disabled;e->commit.notify=committed;
    wl_signal_add(&input->events.destroy,&e->destroy);wl_signal_add(&input->events.enable,&e->enable);
    wl_signal_add(&input->events.disable,&e->disable);wl_signal_add(&input->events.commit,&e->commit);
    sync_focus(t,e);select_active(t);
}
struct ae_text_input *ae_text_input_create(struct server *server,struct wlr_seat *seat) {
    if (!server || !seat) return NULL;
    struct ae_text_input *t=calloc(1,sizeof(*t));if (!t) return NULL;t->server=server;t->seat=seat;
    t->compose=ae_compose_create();if (!t->compose) { free(t);return NULL; }
    t->requests=wl_display_add_protocol_logger(server->display,request,t);
    if (!t->requests) { ae_compose_destroy(t->compose);free(t);return NULL; }
    t->manager=wlr_text_input_manager_v3_create(server->display);
    if (!t->manager) { wl_protocol_logger_destroy(t->requests);ae_compose_destroy(t->compose);free(t);return NULL; }
    t->created.notify=created;t->focus.notify=focus;
    wl_signal_add(&t->manager->events.new_text_input,&t->created);wl_signal_add(&seat->keyboard_state.events.focus_change,&t->focus);
    return t;
}
void ae_text_input_destroy(struct ae_text_input *t) {
    if (!t) return;
    /* Stop dispatch and destroy clients before closing this owner. */
    wl_protocol_logger_destroy(t->requests);
    wl_list_remove(&t->created.link);wl_list_remove(&t->focus.link);
    for (unsigned i=0;i<INPUT_LIMIT;++i) if (t->entries[i].input) {
        struct entry *e=&t->entries[i];wl_list_remove(&e->destroy.link);wl_list_remove(&e->enable.link);wl_list_remove(&e->disable.link);wl_list_remove(&e->commit.link);
    }
    ae_compose_destroy(t->compose);free(t);
}
void ae_text_input_forget_keyboard(struct ae_text_input *t,struct wlr_keyboard *keyboard) { if (t && t->keyboard==keyboard) reset(t,true); }
void ae_text_input_modifiers(struct ae_text_input *t,struct wlr_keyboard *keyboard) {
    if (t && ae_compose_active(t->compose) && (t->keyboard!=keyboard ||
        (wlr_keyboard_get_modifiers(keyboard)&(WLR_MODIFIER_CTRL|WLR_MODIFIER_ALT|WLR_MODIFIER_LOGO)))) reset(t,true);
}
bool ae_text_input_key(struct ae_text_input *t,struct wlr_keyboard *keyboard,uint32_t keycode) {
    if (!t || !keyboard || keycode>KEY_MAX || !keyboard->xkb_state || !t->active || !eligible(t,t->active)) return false;
    if ((t->keyboard && t->keyboard!=keyboard) || (wlr_keyboard_get_modifiers(keyboard)&(WLR_MODIFIER_CTRL|WLR_MODIFIER_ALT|WLR_MODIFIER_LOGO))) {
        reset(t,true);return false;
    }
    const xkb_keysym_t *symbols;
    if (!ae_compose_active(t->compose)) for (size_t i=0;i<keyboard->num_keycodes;++i) {
        uint32_t held=keyboard->keycodes[i];if (held==keycode) continue;
        const xkb_keysym_t *keys;int count=xkb_state_key_get_syms(keyboard->xkb_state,held+8,&keys);
        if (count!=1 || !((keys[0]>=XKB_KEY_Shift_L && keys[0]<=XKB_KEY_Hyper_R) ||
            keys[0]==XKB_KEY_ISO_Level3_Shift || keys[0]==XKB_KEY_Mode_switch || keys[0]==XKB_KEY_Num_Lock)) return false;
    }
    if (xkb_state_key_get_syms(keyboard->xkb_state,keycode+8,&symbols)!=1) { reset(t,true);return false; }
    if (symbols[0]>=XKB_KEY_F1 && symbols[0]<=XKB_KEY_F35) return false;
    struct ae_compose_result result;
    if (!ae_compose_feed(t->compose,symbols[0],&result)) { reset(t,true);return false; }
    if (result.kind==AE_COMPOSE_NONE) return false;
    struct wlr_text_input_v3 *v=t->active->input;
    if (result.kind==AE_COMPOSE_EDIT) {
        t->keyboard=keyboard;
        wlr_text_input_v3_send_preedit_string(v,result.text,(int32_t)result.size,(int32_t)result.size);
    } else {
        t->keyboard=NULL;
        if (result.kind==AE_COMPOSE_COMMIT) wlr_text_input_v3_send_commit_string(v,result.text);
        wlr_text_input_v3_send_preedit_string(v,"",0,0);
    }
    wlr_text_input_v3_send_done(v);return result.consumed;
}
