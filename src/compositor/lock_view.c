#include "lock_view.h"
#include "input.h"
#include "lock.h"
#include "server.h"
#include "login/unlock_view_contract.h"

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_scene.h>

struct ae_lock_view {
    struct server *server;
    struct wl_client *client;
    struct wl_listener client_end, new_surface, commit, surface_end;
    struct wlr_surface *surface;
    struct wlr_scene_tree *tree;
    uint32_t sequence;
    bool shown, pending, committed, presented, ready, mapped, render_locked;
};

static void schedule(struct ae_lock_view *view) {
    struct output *output = &view->server->outputs[0];
    if (!output->scene) return;
    wlr_damage_ring_add_whole(&output->scene->damage_ring);
    wlr_output_schedule_frame(output->output);
}

bool ae_lock_view_ready(const struct ae_lock_view *view) { return !view || view->ready; }

struct wlr_surface *ae_lock_view_surface(const struct ae_lock_view *view) {
    return view && view->ready ? view->surface : NULL;
}

void ae_lock_view_focus(struct ae_lock_view *view) {
    struct wlr_surface *surface = ae_lock_view_surface(view);
    if (surface) ae_input_lock_focus(view->server->input, surface);
}

void ae_lock_view_hide(struct ae_lock_view *view) {
    if (!view) return;
    if (view->surface) ae_input_lock_blur(view->server->input, view->surface);
    view->shown = view->pending = view->committed = view->presented = view->ready = false;
    wlr_scene_node_set_enabled(&view->tree->node, false);
    if (view->render_locked) {
        struct wlr_output *output = view->server->outputs[0].output;
        if (output) wlr_output_lock_attach_render(output, false);
        view->render_locked = false;
    }
}

static void end_view(struct ae_lock_view *view) {
    ae_lock_view_hide(view);
    if (!view->server->shutting_down) ae_lock_view_lost(view->server->lock);
}

static void display_surface(struct ae_lock_view *view) {
    if (!view->shown || !view->surface || !view->surface->mapped) return;
    struct output *primary = &view->server->outputs[0];
    if (!primary->output || !primary->scene) { end_view(view); return; }
    int width, height;
    wlr_output_effective_resolution(primary->output, &width, &height);
    if (width < AE_UNLOCK_VIEW_WIDTH || height < AE_UNLOCK_VIEW_HEIGHT) { end_view(view); return; }
    /* Present one composed output image containing both the panel and the cover. */
    if (!view->render_locked) {
        wlr_output_lock_attach_render(primary->output, true);
        view->render_locked = true;
    }
    wlr_scene_node_set_position(&view->tree->node,
        primary->scene->x + (width - AE_UNLOCK_VIEW_WIDTH) / 2,
        primary->scene->y + (height - AE_UNLOCK_VIEW_HEIGHT) / 2);
    wlr_scene_node_set_enabled(&view->tree->node, true);
    if (!view->pending && !view->ready) schedule(view);
}

void ae_lock_view_show(struct ae_lock_view *view) {
    if (!view) return;
    view->shown = true;
    display_surface(view);
}

static void note_ready(struct ae_lock_view *view) {
    if (!view->shown || !view->pending || !view->committed || !view->presented || view->ready) return;
    view->ready = true;
    ae_lock_view_focus(view);
}

void ae_lock_view_before_commit(struct ae_lock_view *view, uint32_t sequence) {
    if (!view || !view->shown || view->pending || view->ready || !view->tree->node.enabled) return;
    view->sequence = sequence;
    view->pending = true;
    view->committed = view->presented = false;
}

void ae_lock_view_commit(struct ae_lock_view *view, uint32_t sequence, bool committed) {
    if (!view || !view->pending || view->sequence != sequence) return;
    if (!committed) {
        view->pending = false;
        schedule(view);
        return;
    }
    view->committed = true;
    note_ready(view);
}

void ae_lock_view_present(struct ae_lock_view *view, uint32_t sequence, bool presented) {
    if (!view || !view->pending || view->sequence != sequence || view->ready) return;
    if (!presented) {
        view->pending = false;
        schedule(view);
        return;
    }
    view->presented = true;
    note_ready(view);
}

static void surface_commit(struct wl_listener *listener, void *data) {
    (void)data;
    struct ae_lock_view *view = wl_container_of(listener, view, commit);
    struct wlr_surface *surface = view->surface;
    if (!wlr_surface_has_buffer(surface)) {
        if (view->mapped) end_view(view);
        return;
    }
    if (surface->current.width != AE_UNLOCK_VIEW_WIDTH || surface->current.height != AE_UNLOCK_VIEW_HEIGHT) {
        end_view(view);
        wl_client_post_implementation_error(view->client, "invalid private lock view size");
        return;
    }
    view->mapped = true;
    wlr_surface_map(surface);
    display_surface(view);
}

static void surface_destroyed(struct wl_listener *listener, void *data) {
    (void)data;
    struct ae_lock_view *view = wl_container_of(listener, view, surface_end);
    end_view(view);
    wl_list_remove(&view->commit.link);
    wl_list_remove(&view->surface_end.link);
    view->surface = NULL;
}

static const struct wlr_surface_role role = {.name = "aetheri-lock-view", .no_object = true};

static void new_surface(struct wl_listener *listener, void *data) {
    struct ae_lock_view *view = wl_container_of(listener, view, new_surface);
    struct wlr_surface *surface = data;
    if (wl_resource_get_client(surface->resource) != view->client) return;
    if (view->surface || !wlr_surface_set_role(surface, &role, surface->resource, WL_SURFACE_ERROR_INVALID_SIZE)) {
        end_view(view);
        wl_client_post_implementation_error(view->client, "private lock view already assigned");
        return;
    }
    if (!wlr_scene_surface_create(view->tree, surface)) {
        wl_client_post_no_memory(view->client);
        return;
    }
    view->surface = surface;
    view->commit.notify = surface_commit;
    view->surface_end.notify = surface_destroyed;
    wl_signal_add(&surface->events.commit, &view->commit);
    wl_signal_add(&surface->events.destroy, &view->surface_end);
}

static void client_destroyed(struct wl_listener *listener, void *data) {
    (void)data;
    struct ae_lock_view *view = wl_container_of(listener, view, client_end);
    wl_list_remove(&view->client_end.link);
    view->client = NULL;
    end_view(view);
}

bool ae_lock_view_global(const struct wl_client *client, const struct wl_global *global, void *data) {
    const struct ae_lock_view *view = data;
    if (client != view->client) return true;
    const char *name = wl_global_get_interface(global)->name;
    return !strcmp(name, "wl_compositor") || !strcmp(name, "wl_shm") || !strcmp(name, "wl_seat");
}

struct ae_lock_view *ae_lock_view_create(struct server *s, int fd) {
    if (!ae_lock_view_endpoint(s->lock, fd) || fcntl(fd, F_SETFD, FD_CLOEXEC)) return NULL;
    struct ae_lock_view *view = calloc(1, sizeof *view);
    if (!view) return NULL;
    view->server = s;
    view->tree = wlr_scene_tree_create(&s->scene->tree);
    if (view->tree) {
        wlr_scene_node_set_enabled(&view->tree->node, false);
        view->client = wl_client_create(s->display, fd);
    }
    if (!view->client) {
        if (view->tree) wlr_scene_node_destroy(&view->tree->node);
        free(view);
        return NULL;
    }
    view->new_surface.notify = new_surface;
    wl_signal_add(&s->compositor->events.new_surface, &view->new_surface);
    view->client_end.notify = client_destroyed;
    wl_client_add_destroy_listener(view->client, &view->client_end);
    return view;
}

void ae_lock_view_destroy(struct ae_lock_view *view) {
    if (!view) return;
    wl_list_remove(&view->new_surface.link);
    wlr_scene_node_destroy(&view->tree->node);
    free(view);
}
