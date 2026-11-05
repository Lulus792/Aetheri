/* Original-connection development shell. No account/app admission is implied. */
#include "desktop.h"
#include "input.h"
#include "server.h"
#include "aetheri-development-desktop-v1-server.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_xdg_shell.h>

enum { DOCK_HEIGHT = 84 };
struct layer {
    struct ae_desktop *desktop;
    struct wlr_layer_surface_v1 *surface;
    struct wlr_scene_layer_surface_v1 *scene;
    struct wl_listener commit, map, unmap, destroy, popup;
    struct wl_list link;
    unsigned output;
    uint32_t width, height;
    bool expanded, committed;
};
struct ae_desktop {
    struct server *server;
    struct wl_client *client;
    struct wl_global *global;
    struct wl_resource *control;
    struct wlr_layer_shell_v1 *layers;
    struct wlr_scene_tree *tree;
    struct wl_list surfaces;
    struct wl_listener client_destroy, new_layer;
    int output_width[OUTPUT_LIMIT], output_height[OUTPUT_LIMIT];
    bool bound, retired;
};

static void fail(struct ae_desktop *d, const char *message) {
    fprintf(stderr, "compositor desktop: %s\n", message);
    d->server->failed = true;
    wl_display_terminate(d->server->display);
}
static bool filter(const struct wl_client *client, const struct wl_global *global, void *data) {
    struct ae_desktop *d = data;
    if (global == d->global || (d->layers && global == d->layers->global)) return client == d->client;
    return true;
}
/* Bound descriptive UTF-8, replacing controls/invalid scalars without splitting
   a valid character. Neither these strings nor app IDs select an authority. */
static void metadata(char dest[257], const char *source) {
    size_t used = 0;
    const unsigned char *p = (const unsigned char *)(source ? source : "");
    while (*p && used < 256) {
        unsigned char c = *p;
        size_t n = c < 0x80 ? 1u : c >= 0xc2 && c <= 0xdf ? 2u :
            c >= 0xe0 && c <= 0xef ? 3u : c >= 0xf0 && c <= 0xf4 ? 4u : 0u;
        bool valid = n != 0;
        for (size_t i = 1; valid && i < n; ++i) valid = p[i] >= 0x80 && p[i] <= 0xbf;
        if (valid && n >= 3) valid = !(c == 0xe0 && p[1] < 0xa0) && !(c == 0xed && p[1] >= 0xa0);
        if (valid && n == 4) valid = !(c == 0xf0 && p[1] < 0x90) && !(c == 0xf4 && p[1] >= 0x90);
        if (!valid) { dest[used++] = '?'; ++p; continue; }
        if (used + n > 256) break;
        if ((n == 1 && (c < 0x20 || c == 0x7f)) || (n == 2 && c == 0xc2 && p[1] < 0xa0)) {
            dest[used++] = ' '; p += n; continue;
        }
        memcpy(dest + used, p, n); used += n; p += n;
    }
    dest[used] = '\0';
}
void ae_desktop_window(struct ae_desktop *d, struct window *w) {
    if (!d || !d->control || d->retired || !w || !w->mapped || d->server->shutting_down) return;
    char title[257], app_id[257];
    metadata(title, w->toplevel->title); metadata(app_id, w->toplevel->app_id);
    aetheri_development_desktop_v1_send_window(d->control, w->id, title, app_id,
        ae_input_focused(d->server->input, w) ? 1u : 0u);
}
void ae_desktop_closed(struct ae_desktop *d, struct window *w) {
    if (d && d->control && !d->retired && !d->server->shutting_down)
        aetheri_development_desktop_v1_send_closed(d->control, w->id);
}
static void control_destroy(struct wl_resource *resource) {
    struct ae_desktop *d = wl_resource_get_user_data(resource);
    d->control = NULL;
    if (!d->retired && !d->server->shutting_down) fail(d, "desktop control lost");
}
static void destroy_request(struct wl_client *client, struct wl_resource *resource) {
    (void)client; wl_resource_destroy(resource);
}
static void activate_request(struct wl_client *client, struct wl_resource *resource, uint32_t id) {
    (void)client;
    struct ae_desktop *d = wl_resource_get_user_data(resource);
    bool found = false;
    if (!d->retired && !ae_desktop_keyboard_surface(d)) {
        struct window *w;
        wl_list_for_each(w, &d->server->windows, link) if (w->mapped && w->id == id) {
            ae_input_activate(d->server->input, w); found = true; break;
        }
    }
    aetheri_development_desktop_v1_send_activated(resource, id, found ? 1u : 0u);
}
static void retire_request(struct wl_client *client, struct wl_resource *resource) {
    (void)client;
    struct ae_desktop *d = wl_resource_get_user_data(resource);
    d->retired = true;
    aetheri_development_desktop_v1_send_retired(resource);
}
static const struct aetheri_development_desktop_v1_interface implementation = {
    .destroy = destroy_request, .activate = activate_request, .retire = retire_request};
static void bind_control(struct wl_client *client, void *data, uint32_t version, uint32_t id) {
    struct ae_desktop *d = data;
    if (client != d->client || d->bound) {
        wl_client_post_implementation_error(client, "desktop connection already bound or not admitted"); return;
    }
    struct wl_resource *resource = wl_resource_create(client, &aetheri_development_desktop_v1_interface, (int)version, id);
    if (!resource) { wl_client_post_no_memory(client); return; }
    d->bound = true; d->control = resource;
    wl_resource_set_implementation(resource, &implementation, d, control_destroy);
    for (unsigned i = 0; i < d->server->output_count; ++i) {
        struct wlr_output *o = d->server->outputs[i].output;
        if (o) aetheri_development_desktop_v1_send_output(resource, i, o->name);
    }
    struct window *w;
    wl_list_for_each(w, &d->server->windows, link) ae_desktop_window(d, w);
    aetheri_development_desktop_v1_send_done(resource);
}
struct wlr_surface *ae_desktop_keyboard_surface(struct ae_desktop *d) {
    if (!d || d->retired) return NULL;
    struct layer *l;
    wl_list_for_each(l, &d->surfaces, link)
        if (l->surface->surface->mapped && l->expanded) return l->surface->surface;
    return NULL;
}
bool ae_desktop_surface(struct ae_desktop *d, struct wlr_surface *surface) {
    if (!d || !surface) return false;
    struct wlr_surface *root = wlr_surface_get_root_surface(surface);
    struct layer *l;
    wl_list_for_each(l, &d->surfaces, link) if (l->surface->surface == root) return true;
    return false;
}
bool ae_desktop_toggle(struct ae_desktop *d, unsigned output) {
    if (!d || !d->control || d->retired || output >= d->server->output_count) return false;
    aetheri_development_desktop_v1_send_toggle(d->control, output);
    return true;
}
static bool profile(struct layer *l) {
    const struct wlr_layer_surface_v1_state *s = &l->surface->current;
    uint32_t dock = ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM | ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
    bool expanded = s->anchor == (dock | ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP) && !s->desired_width && !s->desired_height &&
        s->keyboard_interactive == ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE;
    bool compact = s->anchor == dock && !s->desired_width && s->desired_height == DOCK_HEIGHT &&
        s->keyboard_interactive == ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE;
    if ((!compact && !expanded) || s->layer != ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY ||
            s->exclusive_zone != DOCK_HEIGHT || s->exclusive_edge != ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM ||
            s->margin.top || s->margin.bottom || s->margin.left || s->margin.right) return false;
    if (l->surface->surface->mapped &&
            (l->surface->surface->current.width != (int)s->actual_width ||
             l->surface->surface->current.height != (int)s->actual_height)) return false;
    if (expanded) {
        struct layer *other;
        wl_list_for_each(other, &l->desktop->surfaces, link)
            if (other != l && other->surface->surface->mapped && other->expanded) return false;
    }
    l->expanded = expanded;
    return true;
}
void ae_desktop_arrange(struct ae_desktop *d) {
    if (!d || d->server->shutting_down || d->server->failed) return;
    bool reserved[OUTPUT_LIMIT] = {false};
    struct layer *l;
    wl_list_for_each(l, &d->surfaces, link) {
        struct output *o = &d->server->outputs[l->output];
        struct wlr_output_layout_output *place = o->output ? wlr_output_layout_get(d->server->layout, o->output) : NULL;
        if (!place || o->output->width < 320 || o->output->width > 3840 ||
                o->output->height < 320 || o->output->height > 2160) { fail(d, "desktop output unavailable"); return; }
        uint32_t width = (uint32_t)o->output->width;
        uint32_t height = l->expanded ? (uint32_t)o->output->height : DOCK_HEIGHT;
        if (l->committed && (l->width != width || l->height != height)) {
            if (!wlr_layer_surface_v1_configure(l->surface, width, height)) { fail(d, "desktop configure failed"); return; }
            l->width = width; l->height = height;
        }
        wlr_scene_node_set_position(&l->scene->tree->node, place->x,
            place->y + (l->expanded ? 0 : o->output->height - DOCK_HEIGHT));
        if (l->surface->surface->mapped) reserved[l->output] = true;
    }
    for (unsigned i = 0; i < d->server->output_count; ++i) {
        struct output *o = &d->server->outputs[i];
        int bottom = reserved[i] ? DOCK_HEIGHT : 0;
        bool resized = o->output && (d->output_width[i] != o->output->width || d->output_height[i] != o->output->height);
        if (o->output) { d->output_width[i] = o->output->width; d->output_height[i] = o->output->height; }
        if (o->reserved_bottom == bottom && !resized) continue;
        o->reserved_bottom = bottom;
        struct window *w;
        wl_list_for_each(w, &d->server->windows, link)
            if (w->home == i && w->toplevel->base->initialized &&
                    (w->toplevel->requested.maximized || w->toplevel->requested.fullscreen))
                ae_server_configure(w);
    }
    struct wlr_surface *keyboard = ae_desktop_keyboard_surface(d);
    if (keyboard) ae_input_shell_focus(d->server->input, keyboard);
    else ae_input_restore(d->server->input);
}
static void layer_commit(struct wl_listener *listener, void *data) {
    (void)data;
    struct layer *l = wl_container_of(listener, l, commit);
    if (!profile(l)) { wl_client_post_implementation_error(l->desktop->client, "unsupported development desktop surface state"); return; }
    l->committed = l->surface->initialized;
    ae_desktop_arrange(l->desktop);
}
static void layer_map(struct wl_listener *listener, void *data) {
    (void)data;
    struct layer *l = wl_container_of(listener, l, map);
    if (!profile(l)) {
        wl_client_post_implementation_error(l->desktop->client, "unsupported desktop map state: logical=%dx%d acknowledged=%ux%u",
            l->surface->surface->current.width,l->surface->surface->current.height,
            l->surface->current.actual_width,l->surface->current.actual_height); return;
    }
    ae_desktop_arrange(l->desktop);
}
static void layer_unmap(struct wl_listener *listener, void *data) {
    (void)data;
    struct layer *l = wl_container_of(listener, l, unmap);
    l->committed = false; l->width = 0; l->height = 0;
    ae_desktop_arrange(l->desktop);
}
static void layer_destroy(struct wl_listener *listener, void *data) {
    (void)data;
    struct layer *l = wl_container_of(listener, l, destroy);
    struct ae_desktop *d = l->desktop;
    wl_list_remove(&l->commit.link); wl_list_remove(&l->map.link); wl_list_remove(&l->unmap.link);
    wl_list_remove(&l->destroy.link); wl_list_remove(&l->popup.link); wl_list_remove(&l->link);
    free(l);
    ae_desktop_arrange(d);
}
static void layer_popup(struct wl_listener *listener, void *data) {
    (void)data;
    struct layer *l = wl_container_of(listener, l, popup);
    wl_client_post_implementation_error(l->desktop->client, "desktop popups not supported in this profile");
}
static void new_layer(struct wl_listener *listener, void *data) {
    struct ae_desktop *d = wl_container_of(listener, d, new_layer);
    struct wlr_layer_surface_v1 *surface = data;
    unsigned index = OUTPUT_LIMIT;
    for (unsigned i = 0; i < d->server->output_count; ++i)
        if (surface->output && surface->output == d->server->outputs[i].output) index = i;
    struct layer *other;
    wl_list_for_each(other, &d->surfaces, link) if (other->output == index) index = OUTPUT_LIMIT;
    if (wl_resource_get_client(surface->resource) != d->client || d->retired || index == OUTPUT_LIMIT ||
            strcmp(surface->namespace, "aetheri.desktop")) {
        wl_client_post_implementation_error(wl_resource_get_client(surface->resource), "unadmitted desktop surface"); return;
    }
    struct layer *l = calloc(1, sizeof(*l));
    if (!l) { wl_client_post_no_memory(d->client); return; }
    l->desktop = d; l->surface = surface; l->output = index;
    l->scene = wlr_scene_layer_surface_v1_create(d->tree, surface);
    if (!l->scene) { free(l); wl_client_post_no_memory(d->client); return; }
    l->commit.notify = layer_commit; l->map.notify = layer_map; l->unmap.notify = layer_unmap;
    l->destroy.notify = layer_destroy; l->popup.notify = layer_popup;
    wl_signal_add(&surface->surface->events.commit, &l->commit);
    wl_signal_add(&surface->surface->events.map, &l->map);
    wl_signal_add(&surface->surface->events.unmap, &l->unmap);
    wl_signal_add(&surface->events.destroy, &l->destroy);
    wl_signal_add(&surface->events.new_popup, &l->popup);
    wl_list_insert(&d->surfaces, &l->link);
}
static void client_destroy(struct wl_listener *listener, void *data) {
    (void)data;
    struct ae_desktop *d = wl_container_of(listener, d, client_destroy);
    wl_list_remove(&d->client_destroy.link); wl_list_init(&d->client_destroy.link);
    d->client = NULL;
    if (!d->server->shutting_down) {
        if (!d->retired) fail(d, "desktop connection lost");
        else wl_display_terminate(d->server->display);
    }
}
struct ae_desktop *ae_desktop_create(struct server *s, int fd) {
    int type = 0; socklen_t size = sizeof(type);
    struct sockaddr_storage peer; socklen_t peer_size = sizeof(peer);
    if (fd < 3 || getsockopt(fd, SOL_SOCKET, SO_TYPE, &type, &size) || type != SOCK_STREAM ||
            getpeername(fd, (struct sockaddr *)&peer, &peer_size) || peer.ss_family != AF_UNIX ||
            fcntl(fd, F_SETFD, FD_CLOEXEC)) return NULL;
    struct ae_desktop *d = calloc(1, sizeof(*d));
    if (!d) return NULL;
    d->server = s; wl_list_init(&d->surfaces); wl_list_init(&d->client_destroy.link);
    wl_list_init(&d->new_layer.link);
    d->tree = wlr_scene_tree_create(&s->scene->tree);
    d->layers = wlr_layer_shell_v1_create(s->display, 5);
    d->global = wl_global_create(s->display, &aetheri_development_desktop_v1_interface, 1, d, bind_control);
    if (!d->tree || !d->layers || !d->global) { ae_desktop_destroy(d); return NULL; }
    d->new_layer.notify = new_layer; wl_signal_add(&d->layers->events.new_surface, &d->new_layer);
    wl_display_set_global_filter(s->display, filter, d);
    d->client = wl_client_create(s->display, fd);
    if (!d->client) { ae_desktop_destroy(d); return NULL; }
    d->client_destroy.notify = client_destroy; wl_client_add_destroy_listener(d->client, &d->client_destroy);
    return d;
}
void ae_desktop_destroy(struct ae_desktop *d) {
    if (!d) return;
    /* Caller has stopped dispatch and destroyed all clients before this point. */
    wl_list_remove(&d->new_layer.link); wl_list_remove(&d->client_destroy.link);
    wl_display_set_global_filter(d->server->display, NULL, NULL);
    if (d->global) wl_global_destroy(d->global);
    if (d->tree) wlr_scene_node_destroy(&d->tree->node);
    free(d);
}
