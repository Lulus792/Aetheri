#include "input.h"
#include "server.h"
#include <drm_fourcc.h>
#include <limits.h>
#include <linux/input-event-codes.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <wlr/backend.h>
#include <wlr/interfaces/wlr_buffer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/edges.h>
#include <xkbcommon/xkbcommon.h>

struct keyboard {
    struct ae_input *input;
    struct wlr_keyboard *keyboard;
    struct wl_list link;
    struct wl_listener key, modifiers, destroy;
    bool suppressed[KEY_MAX + 1];
};
struct pointer {
    struct ae_input *input;
    struct wlr_input_device *device;
    struct wl_list link;
    struct wl_listener destroy;
};
struct ae_input {
    struct server *server;
    struct wlr_seat *seat;
    struct wlr_cursor *cursor;
    struct wl_list keyboards, pointers;
    struct wl_listener new_input, motion, absolute, button, axis, frame, cursor_request;
    struct wl_listener selection_request, drag_request;
    struct window *focused, *grabbed;
    double start_x, start_y;
    int grab_x, grab_y, width, height;
    uint32_t edges, cursor_serial;
    uint32_t clipboard_floor;
    bool clipboard_focus_valid;
};
struct cursor_image { struct wlr_buffer base; uint32_t pixels[16 * 20]; };
static void image_destroy(struct wlr_buffer *buffer) { free(buffer); }
static bool image_access(struct wlr_buffer *buffer, uint32_t flags, void **data, uint32_t *format, size_t *stride) {
    if (flags & WLR_BUFFER_DATA_PTR_ACCESS_WRITE) return false;
    struct cursor_image *image = wl_container_of(buffer, image, base);
    *data = image->pixels; *format = DRM_FORMAT_ARGB8888; *stride = 16 * 4;
    return true;
}
static void image_end(struct wlr_buffer *buffer) { (void)buffer; }
static const struct wlr_buffer_impl image_impl = {
    .destroy = image_destroy, .begin_data_ptr_access = image_access, .end_data_ptr_access = image_end};
static void default_cursor(struct ae_input *in) {
    struct cursor_image *image = calloc(1, sizeof(*image));
    if (!image) { wlr_cursor_unset_image(in->cursor); return; }
    for (int y = 0; y < 18; ++y) for (int x = 0; x <= y / 2 && x < 12; ++x)
        image->pixels[y * 16 + x] = x == 0 || x == y / 2 || y == 17 ? UINT32_C(0xff10141c) : UINT32_C(0xffe9edf5);
    wlr_buffer_init(&image->base, &image_impl, 16, 20);
    wlr_cursor_set_buffer(in->cursor, &image->base, 0, 0, 1.0f);
    wlr_buffer_drop(&image->base);
}
static void focus(struct ae_input *in, struct window *w) {
    if (w && !w->mapped) return;
    bool changed = in->focused != w;
    if (in->focused && in->focused != w) wlr_xdg_toplevel_set_activated(in->focused->toplevel, false);
    in->focused = w;
    if (!w) { in->clipboard_focus_valid = false; wlr_seat_keyboard_notify_clear_focus(in->seat); return; }
    wlr_scene_node_raise_to_top(&w->tree->node);
    if (changed) wlr_xdg_toplevel_set_activated(w->toplevel, true);
    struct wlr_keyboard *kb = wlr_seat_get_keyboard(in->seat);
    if (!kb) return;
    uint32_t keys[WLR_KEYBOARD_KEYS_CAP];
    size_t count = 0;
    struct keyboard *entry;
    wl_list_for_each(entry, &in->keyboards, link) {
        if (entry->keyboard != kb) continue;
        for (size_t i = 0; i < kb->num_keycodes; ++i) {
            uint32_t code = kb->keycodes[i];
            if (code <= KEY_MAX && !entry->suppressed[code]) keys[count++] = code;
        }
    }
    if (changed || !in->clipboard_focus_valid ||
            in->seat->keyboard_state.focused_surface != w->toplevel->base->surface) {
        in->clipboard_floor = wl_display_get_serial(in->server->display) + 1;
        in->clipboard_focus_valid = true;
    }
    wlr_seat_keyboard_notify_enter(in->seat, w->toplevel->base->surface, keys, count, &kb->modifiers);
}
static void fallback(struct ae_input *in, struct window *exclude) {
    struct window *w;
    wl_list_for_each(w, &in->server->windows, link) if (w != exclude && w->mapped) { focus(in, w); return; }
    focus(in, NULL);
}
void ae_input_map(struct window *w) {
    w->mapped = true;
    struct ae_input *in = w->server->input;
    if (!wl_list_empty(&in->keyboards) || !wl_list_empty(&in->pointers)) focus(in, w);
}
void ae_input_unmap(struct window *w) {
    struct ae_input *in = w->server->input;
    w->mapped = false;
    if (in->grabbed == w) in->grabbed = NULL;
    if (in->focused == w) fallback(in, w);
}
static struct window *node_window(struct wlr_scene_node *node) {
    while (node && !node->data) node = node->parent ? &node->parent->node : NULL;
    return node ? node->data : NULL;
}
struct held_point { struct wlr_surface *surface; double x, y; bool found; };
static void locate_surface(struct wlr_scene_buffer *buffer, int x, int y, void *data) {
    struct held_point *p = data;
    struct wlr_scene_surface *surface = wlr_scene_surface_try_from_buffer(buffer);
    if (!p->found && surface && surface->surface == p->surface) {
        p->x -= x;
        p->y -= y;
        /* Use the scene helper's public coordinate hook, including xdg clipping.
         * A held implicit grab keeps motion even outside the input region. */
        if (buffer->point_accepts_input) (void)buffer->point_accepts_input(buffer, &p->x, &p->y);
        p->found = true;
    }
}
static void route(struct ae_input *in, uint32_t time) {
    if (in->grabbed) {
        struct window *w = in->grabbed;
        double dx = in->cursor->x - in->start_x, dy = in->cursor->y - in->start_y;
        if (!in->edges) {
            w->x = in->grab_x + (int)dx; w->y = in->grab_y + (int)dy;
        } else {
            int width = in->width + (int)((in->edges & WLR_EDGE_LEFT) ? -dx : (in->edges & WLR_EDGE_RIGHT) ? dx : 0);
            int height = in->height + (int)((in->edges & WLR_EDGE_TOP) ? -dy : (in->edges & WLR_EDGE_BOTTOM) ? dy : 0);
            int minw = w->toplevel->current.min_width > 64 ? w->toplevel->current.min_width : 64;
            int minh = w->toplevel->current.min_height > 64 ? w->toplevel->current.min_height : 64;
            if (minw > 3840) minw = 3840;
            if (minh > 2160) minh = 2160;
            if (width < minw) width = minw;
            if (width > 3840) width = 3840;
            if (height < minh) height = minh;
            if (height > 2160) height = 2160;
            int maxw = w->toplevel->current.max_width, maxh = w->toplevel->current.max_height;
            if (maxw >= minw && width > maxw) width = maxw;
            if (maxh >= minh && height > maxh) height = maxh;
            if (in->edges & WLR_EDGE_LEFT) w->x = in->grab_x + in->width - width;
            if (in->edges & WLR_EDGE_TOP) w->y = in->grab_y + in->height - height;
            wlr_xdg_toplevel_set_size(w->toplevel, width, height);
        }
        wlr_scene_node_set_position(&w->tree->node, w->x, w->y);
        return;
    }
    if (in->seat->pointer_state.button_count) {
        struct held_point p = {.surface = in->seat->pointer_state.focused_surface, .x = in->cursor->x, .y = in->cursor->y};
        if (p.surface) wlr_scene_node_for_each_buffer(&in->server->scene->tree.node, locate_surface, &p);
        if (p.found) wlr_seat_pointer_notify_motion(in->seat, time, p.x, p.y);
        return;
    }
    double x = 0, y = 0;
    struct wlr_scene_node *node = wlr_scene_node_at(&in->server->scene->tree.node, in->cursor->x, in->cursor->y, &x, &y);
    struct wlr_scene_surface *surface = node && node->type == WLR_SCENE_NODE_BUFFER ?
        wlr_scene_surface_try_from_buffer(wlr_scene_buffer_from_node(node)) : NULL;
    if (!surface) {
        if (in->seat->pointer_state.focused_surface) {
            wlr_seat_pointer_notify_clear_focus(in->seat);
            default_cursor(in);
        }
        return;
    }
    bool changed = in->seat->pointer_state.focused_surface != surface->surface;
    if (changed) default_cursor(in);
    wlr_seat_pointer_notify_enter(in->seat, surface->surface, x, y);
    if (changed && in->seat->pointer_state.focused_surface == surface->surface)
        in->cursor_serial = wl_display_get_serial(in->server->display);
    wlr_seat_pointer_notify_motion(in->seat, time, x, y);
}
static void relative(struct wl_listener *listener, void *data) {
    struct ae_input *in = wl_container_of(listener, in, motion);
    struct wlr_pointer_motion_event *e = data;
    if (!isfinite(e->delta_x) || !isfinite(e->delta_y)) return;
    wlr_cursor_move(in->cursor, &e->pointer->base, e->delta_x, e->delta_y);
    route(in, e->time_msec);
}
static void absolute(struct wl_listener *listener, void *data) {
    struct ae_input *in = wl_container_of(listener, in, absolute);
    struct wlr_pointer_motion_absolute_event *e = data;
    if (!isfinite(e->x) || !isfinite(e->y)) return;
    wlr_cursor_warp_absolute(in->cursor, &e->pointer->base, e->x, e->y);
    route(in, e->time_msec);
}
static void button(struct wl_listener *listener, void *data) {
    struct ae_input *in = wl_container_of(listener, in, button);
    struct wlr_pointer_button_event *e = data;
    if (e->state == WL_POINTER_BUTTON_STATE_PRESSED && !in->seat->pointer_state.button_count) {
        double x, y;
        struct wlr_scene_node *node = wlr_scene_node_at(&in->server->scene->tree.node, in->cursor->x, in->cursor->y, &x, &y);
        focus(in, node_window(node));
    }
    wlr_seat_pointer_notify_button(in->seat, e->time_msec, e->button, e->state);
    if (!in->seat->pointer_state.button_count) {
        if (in->grabbed) wlr_xdg_toplevel_set_resizing(in->grabbed->toplevel, false);
        in->grabbed = NULL;
        route(in, e->time_msec);
    }
}
static void axis(struct wl_listener *listener, void *data) {
    struct ae_input *in = wl_container_of(listener, in, axis);
    struct wlr_pointer_axis_event *e = data;
    if (isfinite(e->delta)) wlr_seat_pointer_notify_axis(in->seat, e->time_msec, e->orientation,
        e->delta, e->delta_discrete, e->source, e->relative_direction);
}
static void frame(struct wl_listener *listener, void *data) {
    (void)data;
    struct ae_input *in = wl_container_of(listener, in, frame);
    wlr_seat_pointer_notify_frame(in->seat);
}
static void cursor_request(struct wl_listener *listener, void *data) {
    struct ae_input *in = wl_container_of(listener, in, cursor_request);
    struct wlr_seat_pointer_request_set_cursor_event *e = data;
    if (in->seat->pointer_state.focused_client == e->seat_client && e->serial == in->cursor_serial)
        wlr_cursor_set_surface(in->cursor, e->surface, e->hotspot_x, e->hotspot_y);
}
static void modifiers(struct wl_listener *listener, void *data) {
    (void)data;
    struct keyboard *k = wl_container_of(listener, k, modifiers);
    wlr_seat_set_keyboard(k->input->seat, k->keyboard);
    wlr_seat_keyboard_notify_modifiers(k->input->seat, &k->keyboard->modifiers);
}
static void key(struct wl_listener *listener, void *data) {
    struct keyboard *k = wl_container_of(listener, k, key);
    struct wlr_keyboard_key_event *e = data;
    struct ae_input *in = k->input;
    if (e->keycode > KEY_MAX) return;
    wlr_seat_set_keyboard(in->seat, k->keyboard);
    if (e->state == WL_KEYBOARD_KEY_STATE_RELEASED && k->suppressed[e->keycode]) {
        k->suppressed[e->keycode] = false; return;
    }
    if (e->state == WL_KEYBOARD_KEY_STATE_PRESSED && e->keycode == KEY_TAB &&
            (wlr_keyboard_get_modifiers(k->keyboard) & WLR_MODIFIER_ALT)) {
        k->suppressed[e->keycode] = true;
        struct window *w, *first = NULL;
        bool after = false;
        wl_list_for_each(w, &in->server->windows, link) {
            if (!w->mapped) continue;
            if (!first) first = w;
            if (after) { focus(in, w); return; }
            if (w == in->focused) after = true;
        }
        if (first) focus(in, first);
        return;
    }
    wlr_seat_keyboard_notify_key(in->seat, e->time_msec, e->keycode, e->state);
}
static void capabilities(struct ae_input *in) {
    uint32_t caps = (!wl_list_empty(&in->keyboards) ? WL_SEAT_CAPABILITY_KEYBOARD : 0u) |
        (!wl_list_empty(&in->pointers) ? WL_SEAT_CAPABILITY_POINTER : 0u);
    wlr_seat_set_capabilities(in->seat, caps);
}
static void keyboard_destroy(struct wl_listener *listener, void *data) {
    (void)data;
    struct keyboard *k = wl_container_of(listener, k, destroy);
    struct ae_input *in = k->input;
    wl_list_remove(&k->key.link); wl_list_remove(&k->modifiers.link);
    wl_list_remove(&k->destroy.link); wl_list_remove(&k->link);
    free(k);
    if (!wl_list_empty(&in->keyboards)) {
        k = wl_container_of(in->keyboards.next, k, link);
        wlr_seat_set_keyboard(in->seat, k->keyboard);
        if (in->focused) focus(in, in->focused);
    } else {
        in->clipboard_focus_valid = false;
        wlr_seat_keyboard_notify_clear_focus(in->seat);
    }
    capabilities(in);
}
static void pointer_destroy(struct wl_listener *listener, void *data) {
    (void)data;
    struct pointer *p = wl_container_of(listener, p, destroy);
    struct ae_input *in = p->input;
    wl_list_remove(&p->destroy.link); wl_list_remove(&p->link); free(p);
    if (wl_list_empty(&in->pointers)) {
        if (in->grabbed) wlr_xdg_toplevel_set_resizing(in->grabbed->toplevel, false);
        in->grabbed = NULL;
        wlr_seat_pointer_notify_clear_focus(in->seat);
        wlr_cursor_unset_image(in->cursor);
    }
    capabilities(in);
}
bool ae_input_add(struct ae_input *in, struct wlr_input_device *device) {
    if (device->type == WLR_INPUT_DEVICE_KEYBOARD) {
        struct wlr_keyboard *kb = wlr_keyboard_from_input_device(device);
        struct keyboard *k = calloc(1, sizeof(*k));
        if (!k) return false;
        if (!kb->keymap) {
            struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_ENVIRONMENT_NAMES);
            struct xkb_rule_names names = {.layout = "us"};
            struct xkb_keymap *map = context ? xkb_keymap_new_from_names(context, &names, XKB_KEYMAP_COMPILE_NO_FLAGS) : NULL;
            bool ok = map && wlr_keyboard_set_keymap(kb, map);
            xkb_keymap_unref(map); xkb_context_unref(context);
            if (!ok) { free(k); return false; }
        }
        k->input = in; k->keyboard = kb;
        k->key.notify = key; k->modifiers.notify = modifiers; k->destroy.notify = keyboard_destroy;
        wl_signal_add(&kb->events.key, &k->key); wl_signal_add(&kb->events.modifiers, &k->modifiers);
        wl_signal_add(&device->events.destroy, &k->destroy); wl_list_insert(&in->keyboards, &k->link);
        wlr_seat_set_keyboard(in->seat, kb);
        if (in->focused && in->focused->mapped) focus(in, in->focused);
        else fallback(in, NULL);
    } else if (device->type == WLR_INPUT_DEVICE_POINTER) {
        struct pointer *p = calloc(1, sizeof(*p));
        if (!p) return false;
        p->input = in; p->device = device; p->destroy.notify = pointer_destroy;
        wl_signal_add(&device->events.destroy, &p->destroy); wl_list_insert(&in->pointers, &p->link);
        wlr_cursor_attach_input_device(in->cursor, device);
        ae_input_map_outputs(in);
        default_cursor(in);
    }
    capabilities(in);
    return true;
}
void ae_input_map_outputs(struct ae_input *in) {
    struct pointer *p;
    wl_list_for_each(p, &in->pointers, link) {
        const char *name = wlr_pointer_from_input_device(p->device)->output_name;
        if (!name) continue;
        for (unsigned i = 0; i < in->server->output_count; ++i) {
            struct wlr_output *output = in->server->outputs[i].output;
            if (output && !strcmp(name, output->name))
                wlr_cursor_map_input_to_output(in->cursor, p->device, output);
        }
    }
}
static void new_input(struct wl_listener *listener, void *data) {
    struct ae_input *in = wl_container_of(listener, in, new_input);
    if (!ae_input_add(in, data)) wl_display_terminate(in->server->display);
}
static void begin(struct window *w, struct wlr_seat_client *seat, uint32_t serial, uint32_t edges) {
    struct ae_input *in = w->server->input;
    struct wlr_surface *surface = in->seat->pointer_state.focused_surface;
    if (!w->mapped || in->grabbed || !seat || seat->seat != in->seat || !surface ||
            wlr_surface_get_root_surface(surface) != w->toplevel->base->surface ||
            !wlr_seat_validate_pointer_grab_serial(in->seat, surface, serial) ||
            w->toplevel->current.fullscreen || w->toplevel->current.maximized) return;
    if (w->toplevel->base->geometry.width < 1 || w->toplevel->base->geometry.width > 3840 ||
            w->toplevel->base->geometry.height < 1 || w->toplevel->base->geometry.height > 2160) return;
    in->grabbed = w; in->edges = edges;
    in->start_x = in->cursor->x; in->start_y = in->cursor->y;
    in->grab_x = w->x; in->grab_y = w->y;
    in->width = w->toplevel->base->geometry.width; in->height = w->toplevel->base->geometry.height;
    if (edges) wlr_xdg_toplevel_set_resizing(w->toplevel, true);
}
void ae_input_move(struct window *w, void *data) {
    struct wlr_xdg_toplevel_move_event *e = data;
    begin(w, e->seat, e->serial, 0);
}
void ae_input_resize(struct window *w, void *data) {
    struct wlr_xdg_toplevel_resize_event *e = data;
    uint32_t edges = e->edges;
    if (!edges || edges & ~(uint32_t)(WLR_EDGE_TOP | WLR_EDGE_BOTTOM | WLR_EDGE_LEFT | WLR_EDGE_RIGHT) ||
            (edges & WLR_EDGE_TOP && edges & WLR_EDGE_BOTTOM) || (edges & WLR_EDGE_LEFT && edges & WLR_EDGE_RIGHT)) return;
    begin(w, e->seat, e->serial, edges);
}
static void selection_request(struct wl_listener *listener, void *data) {
    struct ae_input *in = wl_container_of(listener, in, selection_request);
    struct wlr_seat_request_set_selection_event *event = data;
    struct wlr_seat_client *client = wlr_aetheri_selection_client_v1(event);
    bool fresh = in->clipboard_focus_valid && event->serial - in->clipboard_floor < UINT32_MAX / 2;
    if (client && client == in->seat->keyboard_state.focused_client &&
            in->focused && in->focused->mapped && wlr_seat_get_keyboard(in->seat) && fresh &&
            in->seat->keyboard_state.focused_surface == in->focused->toplevel->base->surface) {
        wlr_seat_set_selection(in->seat, event->source, event->serial);
    } else if (event->source && event->source != in->seat->selection_source) {
        wlr_data_source_destroy(event->source);
    }
}
static void drag_request(struct wl_listener *listener, void *data) {
    struct ae_input *in = wl_container_of(listener, in, drag_request);
    struct wlr_seat_request_start_drag_event *event = data;
    struct wlr_data_source *source = event->drag->source;
    /* Clipboard support does not silently admit drag/grab/icon behavior. */
    if (!wlr_aetheri_reject_unstarted_drag_v1(event->drag)) {
        in->server->failed = true; wl_display_terminate(in->server->display); return;
    }
    if (source && source != in->seat->selection_source) wlr_data_source_destroy(source);
}
struct ae_input *ae_input_create(struct server *s) {
    struct ae_input *in = calloc(1, sizeof(*in));
    if (!in) return NULL;
    in->server = s;
    /* Required private symbol: an old incompatible runtime fails at startup. */
    if (wlr_aetheri_selection_client_v1(NULL) != NULL) { free(in); return NULL; }
    wl_list_init(&in->keyboards); wl_list_init(&in->pointers);
    in->seat = wlr_seat_create(s->display, "seat0"); in->cursor = wlr_cursor_create();
    if (!in->seat || !in->cursor) {
        if (in->seat) wlr_seat_destroy(in->seat);
        if (in->cursor) wlr_cursor_destroy(in->cursor);
        free(in); return NULL;
    }
    wlr_cursor_attach_output_layout(in->cursor, s->layout);
    in->new_input.notify = new_input; wl_signal_add(&s->backend->events.new_input, &in->new_input);
    in->motion.notify = relative; wl_signal_add(&in->cursor->events.motion, &in->motion);
    in->absolute.notify = absolute; wl_signal_add(&in->cursor->events.motion_absolute, &in->absolute);
    in->button.notify = button; wl_signal_add(&in->cursor->events.button, &in->button);
    in->axis.notify = axis; wl_signal_add(&in->cursor->events.axis, &in->axis);
    in->frame.notify = frame; wl_signal_add(&in->cursor->events.frame, &in->frame);
    in->cursor_request.notify = cursor_request; wl_signal_add(&in->seat->events.request_set_cursor, &in->cursor_request);
    in->selection_request.notify = selection_request; wl_signal_add(&in->seat->events.request_set_selection, &in->selection_request);
    in->drag_request.notify = drag_request; wl_signal_add(&in->seat->events.request_start_drag, &in->drag_request);
    return in;
}
void ae_input_destroy(struct ae_input *in) {
    if (!in) return;
    /* Backend device destruction must run first, while these listeners are live. */
    wl_list_remove(&in->new_input.link); wl_list_remove(&in->motion.link);
    wl_list_remove(&in->absolute.link); wl_list_remove(&in->button.link);
    wl_list_remove(&in->axis.link); wl_list_remove(&in->frame.link); wl_list_remove(&in->cursor_request.link);
    wl_list_remove(&in->selection_request.link); wl_list_remove(&in->drag_request.link);
    wlr_cursor_destroy(in->cursor); wlr_seat_destroy(in->seat); free(in);
}
void ae_input_stop_backend(struct ae_input *in) {
    if (!in) return;
    wl_list_remove(&in->new_input.link);
    wl_list_init(&in->new_input.link);
}
