/* Private software-output bring-up. Not an installed/trusted desktop session. */
#include <drm_fourcc.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <wayland-server-core.h>
#include <wayland-client-core.h>
#include <wlr/backend.h>
#include <wlr/backend/headless.h>
#include <wlr/backend/wayland.h>
#ifdef AETHERI_DRM_SESSION
#include <wlr/backend/drm.h>
#include <wlr/backend/libinput.h>
#include <wlr/backend/multi.h>
#include <wlr/backend/session.h>
#include "drm_selection.h"
#endif
#include <wlr/render/allocator.h>
#include <wlr/render/pixman.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_viewporter.h>
#include <wlr/util/log.h>

#include "server.h"
#include "input.h"
#include "desktop.h"
#include "text_input.h"
#ifdef AETHERI_INPUT_TEST
#include "compositor_input_driver.h"
#endif

static void fail(struct server *s, const char *message) {
    fprintf(stderr, "compositor: %s\n", message);
    s->failed = true;
    wl_display_terminate(s->display);
}

static bool capture(struct output *o, struct wlr_buffer *buffer) {
    void *pixels = NULL;
    uint32_t format = 0;
    size_t stride = 0;
    if (!buffer || !wlr_buffer_begin_data_ptr_access(buffer,
            WLR_BUFFER_DATA_PTR_ACCESS_READ, &pixels, &format, &stride)) return false;
    bool ok = false;
    FILE *file = NULL;
    if ((format != DRM_FORMAT_XRGB8888 && format != DRM_FORMAT_ARGB8888) ||
            buffer->width <= 0 || buffer->width > 3840 || buffer->height <= 0 ||
            buffer->height > 2160 || stride < (size_t)buffer->width * 4) goto done;
    char name[64];
    int n = snprintf(name, sizeof(name), "capture-%u-output-%u.ppm", o->capture, o->index);
    if (n <= 0 || (size_t)n >= sizeof(name)) goto done;
    int fd = openat(o->server->capture_fd, name,
        O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
    if (fd < 0) goto done;
    file = fdopen(fd, "wb");
    if (!file) { close(fd); goto done; }
    if (fprintf(file, "P6\n%d %d\n255\n", buffer->width, buffer->height) < 0) goto done;
    unsigned char row[3840 * 3];
    for (int y = 0; y < buffer->height; ++y) {
        const unsigned char *src = (const unsigned char *)pixels + (size_t)y * stride;
        for (int x = 0; x < buffer->width; ++x) {
            row[(size_t)x * 3] = src[(size_t)x * 4 + 2];
            row[(size_t)x * 3 + 1] = src[(size_t)x * 4 + 1];
            row[(size_t)x * 3 + 2] = src[(size_t)x * 4];
        }
        if (fwrite(row, 3, (size_t)buffer->width, file) != (size_t)buffer->width) goto done;
    }
    ok = true;
done:
    if (file && fclose(file) != 0) ok = false;
    wlr_buffer_end_data_ptr_access(buffer);
    return ok;
}

static void output_frame(struct wl_listener *listener, void *data) {
    (void)data;
    struct output *o = wl_container_of(listener, o, frame);
    if (o->server->failed || o->server->shutting_down || !o->scene) return;
    if (!wlr_scene_output_needs_frame(o->scene)) {
        /* A client can request pacing without damaging any pixels. */
        struct timespec now;
        if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) fail(o->server, "frame clock failed");
        else wlr_scene_output_send_frame_done(o->scene, &now);
        return;
    }
    struct wlr_output_state state;
    wlr_output_state_init(&state);
    bool ok = wlr_scene_output_build_state(o->scene, &state, NULL);
    if (ok) ok = wlr_output_commit_state(o->output, &state);
    if (ok) {
        ++o->commits;
        if (o->capture) {
            ok = capture(o, state.buffer);
            if (ok) printf("CAPTURE %u %u\n", o->capture, o->index);
            o->capture = 0;
        }
        struct timespec now;
        if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) ok = false;
        else wlr_scene_output_send_frame_done(o->scene, &now);
    }
    wlr_output_state_finish(&state);
    if (!ok) fail(o->server, "output render/commit/capture failed");
}

static void output_destroy(struct wl_listener *listener, void *data) {
    (void)data;
    struct output *o = wl_container_of(listener, o, destroy);
    wl_list_remove(&o->frame.link);
    wl_list_remove(&o->destroy.link);
    wl_list_remove(&o->request_state.link);
    o->output = NULL;
    o->scene = NULL;
    if (o->server->nested && !o->server->shutting_down) wl_display_terminate(o->server->display);
    if (o->server->drm && !o->server->shutting_down) fail(o->server, "development output lost");
}
static void output_request_state(struct wl_listener *listener, void *data) {
    struct output *o = wl_container_of(listener, o, request_state);
    struct wlr_output_event_request_state *event = data;
    const struct wlr_output_state *state = event->state;
    if (state->committed & WLR_OUTPUT_STATE_MODE) {
        if (state->mode_type != WLR_OUTPUT_STATE_MODE_CUSTOM || state->custom_mode.width < 64 ||
                state->custom_mode.width > 3840 || state->custom_mode.height < 64 || state->custom_mode.height > 2160) {
            fail(o->server, "parent output size outside private profile"); return;
        }
    }
    if (!wlr_output_commit_state(o->output, state)) fail(o->server, "parent resize failed");
    else if (o->server->desktop) ae_desktop_arrange(o->server->desktop);
    else {
        /* Standalone app/session probes have no private desktop owner. They
           still need current bounds and maximized/fullscreen dimensions. */
        struct window *w;
        wl_list_for_each(w, &o->server->windows, link)
            if (ae_server_window_output(w) == o && w->toplevel->base->initialized)
                ae_server_configure(w);
        ae_input_recheck_pointer(o->server->input);
    }
}
static void backend_destroy(struct wl_listener *listener, void *data) {
    (void)data;
    struct server *s = wl_container_of(listener, s, backend_destroy);
    ae_input_stop_backend(s->input);
    wl_list_remove(&s->backend_destroy.link);
    s->backend = NULL;
    if (!s->shutting_down) fail(s, "parent backend lost");
}

#ifdef AETHERI_DRM_SESSION
static void session_destroyed(struct wl_listener *listener, void *data) {
    (void)data;
    struct server *s = wl_container_of(listener, s, session_destroy);
    wl_list_remove(&s->session_active.link);
    wl_list_init(&s->session_active.link);
    wl_list_remove(&s->session_destroy.link);
    wl_list_init(&s->session_destroy.link);
    s->session = NULL;
    if (!s->shutting_down) fail(s, "development session lost");
}

static void session_active(struct wl_listener *listener, void *data) {
    (void)data;
    struct server *s = wl_container_of(listener, s, session_active);
    /* No trusted lock/resume policy yet: exit instead of delivering stale input. */
    if (!s->session->active && !s->shutting_down) fail(s, "development seat deactivated");
}

static void drm_output(struct wl_listener *listener, void *data) {
    struct server *s = wl_container_of(listener, s, new_output);
    struct wlr_output *raw = data;
    struct wlr_output_mode *mode = wlr_output_preferred_mode(raw);
    if (!wlr_output_is_drm(raw) || s->output_count >= OUTPUT_LIMIT || !mode ||
            mode->width < 64 || mode->width > 3840 || mode->height < 64 || mode->height > 2160) {
        fail(s, "DRM output outside development profile"); return;
    }
    unsigned index = s->output_count++;
    struct output *o = &s->outputs[index];
    o->server = s; o->index = index; o->output = raw;
    o->destroy.notify = output_destroy;
    wl_signal_add(&raw->events.destroy, &o->destroy);
    o->request_state.notify = output_request_state;
    wl_signal_add(&raw->events.request_state, &o->request_state);
    o->frame.notify = output_frame;
    wl_signal_add(&raw->events.frame, &o->frame);
    if (!wlr_output_init_render(raw, s->allocator, s->renderer)) {
        fail(s, "DRM rendering initialization failed"); return;
    }
    struct wlr_output_state state;
    wlr_output_state_init(&state);
    wlr_output_state_set_enabled(&state, true);
    wlr_output_state_set_mode(&state, mode);
    bool ok = wlr_output_commit_state(raw, &state);
    wlr_output_state_finish(&state);
    int x = 0;
    for (unsigned i = 0; i < index; ++i) {
        if (!s->outputs[i].output) { fail(s, "earlier DRM output lost"); return; }
        x += s->outputs[i].output->width;
    }
    if (!ok || !wlr_output_layout_add(s->layout, raw, x, 0) ||
            !(o->scene = wlr_scene_output_create(s->scene, raw))) {
        fail(s, "DRM output commit/layout failed"); return;
    }
    wlr_scene_output_set_position(o->scene, x, 0);
    ae_input_map_outputs(s->input);
    printf("OUTPUT %u %dx%d refresh=%d\n", index, raw->width, raw->height, raw->refresh);
}

static struct wlr_backend *session_backend(struct server *s, struct wl_event_loop *loop, const char *card) {
    const char *seat_backend = getenv("LIBSEAT_BACKEND");
    if (!seat_backend || strcmp(seat_backend, "logind")) return NULL;
    s->session = wlr_session_create(loop);
    if (s->session) {
        s->session_destroy.notify = session_destroyed;
        wl_signal_add(&s->session->events.destroy, &s->session_destroy);
    }
    /* Activation is asynchronous. Wait only for this same session; never reopen
       or substitute another backend after a delayed/lost activation. */
    struct timespec deadline, now;
    if (clock_gettime(CLOCK_MONOTONIC, &deadline) != 0) return NULL;
    deadline.tv_sec += 5;
    while (s->session && !s->session->active) {
        if (clock_gettime(CLOCK_MONOTONIC, &now) != 0 || now.tv_sec > deadline.tv_sec ||
                (now.tv_sec == deadline.tv_sec && now.tv_nsec >= deadline.tv_nsec)) break;
        if (wl_event_loop_dispatch(loop, 100) < 0) break;
    }
    if (!s->session || !s->session->active || strcmp(s->session->seat, "seat0")) {
        fprintf(stderr, "development session requires the active local seat0 VT\n");
        return NULL;
    }
    s->session_active.notify = session_active;
    wl_signal_add(&s->session->events.active, &s->session_active);
    struct wlr_backend *multi = wlr_multi_backend_create(loop);
    if (!multi) return NULL;
    struct wlr_device *device = ae_drm_select(s->session, card);
    uintptr_t candidate = (uintptr_t)device;
    struct wlr_backend *drm = device ? wlr_drm_backend_create(s->session, device, NULL) : NULL;
    if (!drm || !wlr_multi_backend_add(multi, drm)) {
        if (drm) wlr_backend_destroy(drm);
        else if (candidate) ae_drm_close_unclaimed(s->session, candidate);
        wlr_backend_destroy(multi); return NULL;
    }
    struct wlr_backend *input = wlr_libinput_backend_create(s->session);
    if (!input || !wlr_multi_backend_add(multi, input)) {
        if (input) wlr_backend_destroy(input);
        wlr_backend_destroy(multi); return NULL;
    }
    return multi;
}
#endif

struct output *ae_server_window_output(struct window *w) {
    struct server *s = w->server;
    struct output *o = &s->outputs[w->home];
    struct wlr_output *at = wlr_output_layout_output_at(s->layout, w->x, w->y);
    for (unsigned i = 0; i < s->output_count; ++i) if (s->outputs[i].output == at) o = &s->outputs[i];
    if (w->is_fullscreen && w->toplevel->requested.fullscreen_output) {
        for (unsigned i = 0; i < s->output_count; ++i) {
            if (s->outputs[i].output == w->toplevel->requested.fullscreen_output) o = &s->outputs[i];
        }
    }
    return o->output ? o : NULL;
}
void ae_server_configure(struct window *w) {
    struct server *s = w->server;
    struct wlr_xdg_toplevel *top = w->toplevel;
    struct output *o = ae_server_window_output(w);
    bool full = w->is_fullscreen;
    bool max = w->maximized && !full;
    if (!o) { fail(s, "selected output disappeared"); return; }
    struct wlr_output_layout_output *place = wlr_output_layout_get(s->layout, o->output);
    if (!place) { fail(s, "selected output layout disappeared"); return; }
    int usable_height = o->output->height - o->reserved_bottom;
    int padding_x = o->output->width >= 400 ? 40 : 0;
    int padding_y = usable_height >= 400 ? 40 : 0;
    /* These are advisory normal-window bounds, not a mandatory clip or proof
       that an arbitrary client follows them. Legacy xdg versions omit the event. */
    wlr_xdg_toplevel_set_bounds(top, o->output->width - 2 * padding_x, usable_height - 2 * padding_y);
    if (!w->mapped && top->base->initial_commit && !full && !max) {
        w->x = place->x + padding_x; w->y = place->y + padding_y;
    }
    wlr_scene_node_set_position(&w->tree->node, full || max ? place->x : w->x,
        full || max ? place->y : w->y);
    wlr_xdg_toplevel_set_fullscreen(top, full);
    wlr_xdg_toplevel_set_maximized(top, max);
    wlr_xdg_toplevel_set_size(top, full || max ? o->output->width : 0,
        full ? o->output->height : max ? usable_height : 0);
    wlr_xdg_surface_schedule_configure(top->base);
}
void ae_server_maximize(struct window *w, bool maximized) {
    if (!w || !w->mapped || w->is_fullscreen || w->maximized == maximized) return;
    w->maximized = maximized;
    ae_server_configure(w);
}

static void window_request(struct wl_listener *listener, void *data) {
    (void)data;
    struct window *w = wl_container_of(listener, w, maximize);
    w->maximized = w->toplevel->requested.maximized;
    if (w->toplevel->base->initialized) ae_server_configure(w);
}
static void window_fullscreen(struct wl_listener *listener, void *data) {
    (void)data;
    struct window *w = wl_container_of(listener, w, fullscreen);
    w->is_fullscreen = w->toplevel->requested.fullscreen;
    if (w->toplevel->base->initialized) ae_server_configure(w);
    ae_desktop_refresh_visibility(w->server->desktop);
}
static void window_commit(struct wl_listener *listener, void *data) {
    (void)data;
    struct window *w = wl_container_of(listener, w, commit);
    if (w->toplevel->base->initial_commit) {
        wlr_xdg_toplevel_set_wm_capabilities(w->toplevel,
            WLR_XDG_TOPLEVEL_WM_CAPABILITIES_MAXIMIZE | WLR_XDG_TOPLEVEL_WM_CAPABILITIES_FULLSCREEN);
        ae_server_configure(w);
    }
    if (w->mapped) ae_desktop_refresh_visibility(w->server->desktop);
}
static void window_map(struct wl_listener *listener, void *data) {
    (void)data;
    struct window *w = wl_container_of(listener, w, map);
    ae_input_map(w);
    ae_desktop_window(w->server->desktop, w);
    printf("MAP %u\n", w->home);
}
static void window_unmap(struct wl_listener *listener, void *data) {
    (void)data;
    struct window *w = wl_container_of(listener, w, unmap);
    ae_input_unmap(w);
    ae_desktop_closed(w->server->desktop, w);
    printf("UNMAP %u\n", w->home);
}
static void window_destroy(struct wl_listener *listener, void *data) {
    (void)data;
    struct window *w = wl_container_of(listener, w, destroy);
    if (w->mapped) { ae_input_unmap(w); ae_desktop_closed(w->server->desktop, w); }
    wl_list_remove(&w->commit.link);
    wl_list_remove(&w->destroy.link);
    wl_list_remove(&w->map.link);
    wl_list_remove(&w->unmap.link);
    wl_list_remove(&w->maximize.link);
    wl_list_remove(&w->fullscreen.link);
    wl_list_remove(&w->move.link);
    wl_list_remove(&w->resize.link);
    wl_list_remove(&w->title.link);
    wl_list_remove(&w->app_id.link);
    wl_list_remove(&w->link);
    --w->server->window_count;
    /* The xdg scene helper independently retires its tree on surface destroy. */
    free(w);
}
static void window_move(struct wl_listener *listener, void *data) {
    struct window *w = wl_container_of(listener, w, move);
    ae_input_move(w, data);
}
static void window_resize(struct wl_listener *listener, void *data) {
    struct window *w = wl_container_of(listener, w, resize);
    ae_input_resize(w, data);
}
static void window_title(struct wl_listener *listener, void *data) {
    (void)data;
    struct window *w = wl_container_of(listener, w, title);
    ae_desktop_window(w->server->desktop, w);
}
static void window_app_id(struct wl_listener *listener, void *data) {
    (void)data;
    struct window *w = wl_container_of(listener, w, app_id);
    ae_desktop_window(w->server->desktop, w);
}
static void new_toplevel(struct wl_listener *listener, void *data) {
    struct server *s = wl_container_of(listener, s, new_toplevel);
    struct wlr_xdg_toplevel *top = data;
    if (!s->output_count || s->window_count >= WINDOW_LIMIT || s->next_window_id == UINT32_MAX) {
        wl_client_post_implementation_error(wl_resource_get_client(top->resource), "private window limit");
        return;
    }
    struct window *w = calloc(1, sizeof(*w));
    if (!w) { wl_client_post_no_memory(wl_resource_get_client(top->resource)); return; }
    w->server = s;
    w->toplevel = top;
    w->id = ++s->next_window_id;
    w->home = s->next_home++ % s->output_count;
    if (!s->outputs[w->home].scene) {
        free(w);
        wl_client_post_implementation_error(wl_resource_get_client(top->resource), "output unavailable");
        return;
    }
    w->x = s->outputs[w->home].scene->x + 40;
    w->y = 40;
    w->tree = wlr_scene_xdg_surface_create(s->windows_tree, top->base);
    if (!w->tree) { free(w); wl_client_post_no_memory(wl_resource_get_client(top->resource)); return; }
    w->tree->node.data = w;
    w->commit.notify = window_commit;
    w->destroy.notify = window_destroy;
    w->map.notify = window_map;
    w->unmap.notify = window_unmap;
    w->maximize.notify = window_request;
    w->fullscreen.notify = window_fullscreen;
    w->move.notify = window_move;
    w->resize.notify = window_resize;
    w->title.notify = window_title;
    w->app_id.notify = window_app_id;
    wl_signal_add(&top->base->surface->events.commit, &w->commit);
    wl_signal_add(&top->events.destroy, &w->destroy);
    wl_signal_add(&top->base->surface->events.map, &w->map);
    wl_signal_add(&top->base->surface->events.unmap, &w->unmap);
    wl_signal_add(&top->events.request_maximize, &w->maximize);
    wl_signal_add(&top->events.request_fullscreen, &w->fullscreen);
    wl_signal_add(&top->events.request_move, &w->move);
    wl_signal_add(&top->events.request_resize, &w->resize);
    wl_signal_add(&top->events.set_title, &w->title);
    wl_signal_add(&top->events.set_app_id, &w->app_id);
    wl_list_insert(&s->windows, &w->link);
    ++s->window_count;
}

static int signal_event(int number, void *data) {
    struct server *s = data;
    if (number == SIGINT || number == SIGTERM) wl_display_terminate(s->display);
    else if (number == SIGUSR2) {
        uint64_t count = 0;
        for (unsigned i = 0; i < s->output_count; ++i) count += s->outputs[i].commits;
        printf("STATS windows=%u commits=%" PRIu64 "\n", s->window_count, count);
    } else if (s->capture_fd >= 0 && s->capture < CAPTURE_LIMIT) {
        ++s->capture;
        for (unsigned i = 0; i < s->output_count; ++i) {
            struct output *o = &s->outputs[i];
            if (!o->output || !o->scene) { fail(s, "capture output disappeared"); return 0; }
            if (o->capture) { fail(s, "capture already pending"); return 0; }
            o->capture = s->capture;
            wlr_damage_ring_add_whole(&o->scene->damage_ring);
            wlr_output_schedule_frame(o->output);
        }
    }
    return 0;
}

static int private_directory(const char *path) {
    if (!path || path[0] != '/') return -1;
    int fd = open(path, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) return -1;
    struct stat st;
    if (fstat(fd, &st) != 0 || st.st_uid != getuid() || (st.st_mode & 0777) != 0700) {
        close(fd);
        return -1;
    }
    return fd;
}

int main(int argc, char **argv) {
    bool nested = argc > 1 && !strcmp(argv[1], "--nested");
    bool drm = false;
#ifdef AETHERI_DRM_SESSION
    drm = argc > 1 && !strcmp(argv[1], "--drm");
#endif
    int expected_argc = nested || drm ? 5 : 4;
#ifdef AETHERI_INPUT_TEST
    if (nested) return 2;
    expected_argc = 5;
    struct input_driver *driver = NULL;
#endif
    int shell_fd = -1;bool compose=false,options_valid=true;
    const char *keyboard_layout = NULL;
    for (int at=expected_argc;at<argc;++at) {
        if (!strcmp(argv[at],"--shell-fd") && shell_fd<0 && at+1<argc) {
            const char *value=argv[++at];char *end=NULL;errno=0;long parsed=strtol(value,&end,10);
            if (!*value || strspn(value,"0123456789")!=strlen(value) || errno || !end || *end || parsed<3 || parsed>INT32_MAX) { options_valid=false;break; }
            shell_fd=(int)parsed;
        }
        else if (!strcmp(argv[at],"--keyboard-layout") && !keyboard_layout && !nested && at+1<argc) {
            const char *value=argv[++at];
            if (!strcmp(value,"us")) keyboard_layout="us";
            else if (!strcmp(value,"de")) keyboard_layout="de";
            else { options_valid=false;break; }
        }
#ifdef AETHERI_COMPOSE
        else if (!strcmp(argv[at],"--compose") && !compose) compose=true;
#endif
        else { options_valid=false;break; }
    }
    if (argc<expected_argc || !options_valid ||
            (!nested && !drm && strcmp(argv[1], "--headless")) || strlen(argv[2]) > 48 ||
            strspn(argv[2], "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_") != strlen(argv[2]) ||
            argv[2][0] == '\0' || getuid() == 0 || getuid() != geteuid() || getgid() != getegid()) {
        fprintf(stderr, "usage (unprivileged): aetheri-compositor --headless SOCKET CAPTURE_DIR_OR_-\n"
                        "                     aetheri-compositor --nested SOCKET CAPTURE_DIR_OR_- PARENT_SOCKET\n"
                        "  session build only: aetheri-compositor --drm SOCKET CAPTURE_DIR_OR_- auto|/dev/dri/cardN\n"
                        "  optional private desktop connection: --shell-fd FD\n"
                        "  headless/session startup layout: --keyboard-layout us|de\n"
                        "  reviewed Compose build only: --compose\n");
        return 2;
    }
    if (nested && (getenv("WAYLAND_SOCKET") || strlen(argv[4]) > 48 || !argv[4][0] ||
            strspn(argv[4], "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_") != strlen(argv[4]) ||
            !strcmp(argv[2], argv[4]))) return 2;
    if (drm && strcmp(argv[4], "auto") && (strncmp(argv[4], "/dev/dri/card", 13) || !argv[4][13] ||
            strlen(argv[4] + 13) > 2 || strspn(argv[4] + 13, "0123456789") != strlen(argv[4] + 13))) return 2;
    int runtime = private_directory(getenv("XDG_RUNTIME_DIR"));
    if (runtime < 0) { fprintf(stderr, "private 0700 XDG_RUNTIME_DIR required\n"); return 2; }
    close(runtime);
    struct server s = {.capture_fd = -1, .output_count = drm ? 0 : OUTPUT_LIMIT, .nested = nested, .drm = drm,
        .keyboard_layout = keyboard_layout};
    wl_list_init(&s.session_active.link);
    wl_list_init(&s.session_destroy.link);
    wl_list_init(&s.new_output.link);
    struct wl_display *parent = NULL;
    if (strcmp(argv[3], "-")) {
        s.capture_fd = private_directory(argv[3]);
        if (s.capture_fd < 0) return 2;
    }
    setvbuf(stdout, NULL, _IOLBF, 0);
    wlr_log_init(drm ? WLR_INFO : WLR_ERROR, NULL);
    wl_list_init(&s.windows);
    s.display = wl_display_create();
    if (!s.display) { if (s.capture_fd >= 0) close(s.capture_fd); return 1; }
    struct wl_event_loop *loop = wl_display_get_event_loop(s.display);
    struct wl_event_source *signals[4] = {0};
    const int numbers[] = {SIGINT, SIGTERM, SIGUSR1, SIGUSR2};
    int result = 1;
    for (unsigned i = 0; i < 4; ++i) {
        signals[i] = wl_event_loop_add_signal(loop, numbers[i], signal_event, &s);
        if (!signals[i]) goto done;
    }
#ifdef AETHERI_DRM_SESSION
    if (drm) s.backend = session_backend(&s, loop, argv[4]);
    else
#endif
    if (nested) {
        parent = wl_display_connect(argv[4]);
        if (!parent) goto done;
        s.backend = wlr_wl_backend_create(loop, parent);
    } else s.backend = wlr_headless_backend_create(loop);
    s.renderer = wlr_pixman_renderer_create();
    if (!s.backend || !s.renderer) goto done;
    s.backend_destroy.notify = backend_destroy;
    wl_signal_add(&s.backend->events.destroy, &s.backend_destroy);
    s.allocator = wlr_allocator_autocreate(s.backend, s.renderer);
    s.scene = wlr_scene_create();
    s.layout = wlr_output_layout_create(s.display);
    if (!s.allocator || !s.scene || !s.layout || !wlr_renderer_init_wl_display(s.renderer, s.display) ||
            !wlr_compositor_create(s.display, 6, s.renderer) || !wlr_subcompositor_create(s.display) ||
            !wlr_data_device_manager_create(s.display) || !wlr_viewporter_create(s.display)) goto done;
    const int widths[] = {1920, 2560, 3840}, heights[] = {1080, 1440, 2160};
    const float background[] = {0.0627451f, 0.0784314f, 0.109804f, 1.0f};
    if (!wlr_scene_rect_create(&s.scene->tree, drm ? 11520 : 8320, 2160, background)) goto done;
    s.windows_tree = wlr_scene_tree_create(&s.scene->tree);
    if (!s.windows_tree) goto done;
    s.input = ae_input_create(&s);
    if (!s.input || (nested && !wlr_backend_start(s.backend))) goto done;
    if (compose && !(s.text_input=ae_text_input_create(&s,ae_input_seat(s.input)))) goto done;
    int x = 0;
    for (unsigned i = 0; !drm && i < OUTPUT_LIMIT; ++i) {
        struct output *o = &s.outputs[i];
        o->server = &s;
        o->index = i;
        o->output = nested ? wlr_wl_output_create(s.backend) :
            wlr_headless_add_output(s.backend, (unsigned)widths[i], (unsigned)heights[i]);
        if (!o->output) goto done;
        o->destroy.notify = output_destroy;
        wl_signal_add(&o->output->events.destroy, &o->destroy);
        o->request_state.notify = output_request_state;
        wl_signal_add(&o->output->events.request_state, &o->request_state);
        if (nested) wlr_wl_output_set_title(o->output, "Aetheri private desktop");
        o->frame.notify = output_frame;
        wl_signal_add(&o->output->events.frame, &o->frame);
        if (!wlr_output_init_render(o->output, s.allocator, s.renderer)) goto done;
        struct wlr_output_state state;
        wlr_output_state_init(&state);
        wlr_output_state_set_enabled(&state, true);
        wlr_output_state_set_custom_mode(&state, widths[i], heights[i], nested ? 0 : 60000);
        bool ok = wlr_output_commit_state(o->output, &state);
        wlr_output_state_finish(&state);
        if (!ok || !wlr_output_layout_add(s.layout, o->output, x, 0)) goto done;
        o->scene = wlr_scene_output_create(s.scene, o->output);
        if (!o->scene) goto done;
        wlr_scene_output_set_position(o->scene, x, 0);
        ae_input_map_outputs(s.input);
        x += widths[i];
    }
    s.shell = wlr_xdg_shell_create(s.display, 5);
    if (!s.shell) goto done;
    s.new_toplevel.notify = new_toplevel;
    wl_signal_add(&s.shell->events.new_toplevel, &s.new_toplevel);
#ifdef AETHERI_DRM_SESSION
    if (drm) {
        s.new_output.notify = drm_output;
        wl_signal_add(&s.backend->events.new_output, &s.new_output);
    }
#endif
#ifdef AETHERI_INPUT_TEST
    char *end = NULL;
    long input_fd = strtol(argv[4], &end, 10);
    if (!end || *end || input_fd < 3 || input_fd > INT32_MAX || input_fd == shell_fd) goto done;
    driver = input_driver_start(&s, (int)input_fd);
    if (!driver) goto done;
#endif
    if (shell_fd >= 0) {
        s.desktop = ae_desktop_create(&s, shell_fd);
        if (!s.desktop) goto done;
        shell_fd = -1;
    }
    if (wl_display_add_socket(s.display, argv[2]) != 0 || (!nested && !wlr_backend_start(s.backend))) goto done;
    if (s.failed || !s.output_count) goto done;
    printf("READY %s outputs=%u renderer=pixman\n", argv[2], s.output_count);
    wl_display_run(s.display);
    result = s.failed ? 1 : 0;
done:
    s.shutting_down = true;
    wl_display_destroy_clients(s.display);
    ae_text_input_destroy(s.text_input);s.text_input=NULL;
    ae_desktop_destroy(s.desktop);
    s.desktop = NULL;
    if (shell_fd >= 0) close(shell_fd);
#ifdef AETHERI_INPUT_TEST
    input_driver_close(driver);
#endif
    if (s.shell) wl_list_remove(&s.new_toplevel.link);
    wl_list_remove(&s.new_output.link);
    ae_input_stop_backend(s.input);
    if (s.backend) wlr_backend_destroy(s.backend);
    ae_input_destroy(s.input);
    if (s.scene) wlr_scene_node_destroy(&s.scene->tree.node);
    if (s.layout) wlr_output_layout_destroy(s.layout);
    if (s.allocator) wlr_allocator_destroy(s.allocator);
    if (s.renderer) wlr_renderer_destroy(s.renderer);
#ifdef AETHERI_DRM_SESSION
    wl_list_remove(&s.session_active.link);
    wl_list_remove(&s.session_destroy.link);
    if (s.session) wlr_session_destroy(s.session);
#endif
    for (unsigned i = 0; i < 4; ++i) if (signals[i]) wl_event_source_remove(signals[i]);
    wl_display_destroy(s.display);
    if (parent) wl_display_disconnect(parent);
    if (s.capture_fd >= 0) close(s.capture_fd);
    return result;
}
