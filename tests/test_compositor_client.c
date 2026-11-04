/* Independent libwayland consumer: no wlroots or compositor-private linkage. */
#include <errno.h>
#include <poll.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <wayland-client.h>
#include "xdg-shell-client-protocol.h"

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "client check failed: %s:%d: %s\n", __FILE__, __LINE__, #x); exit(1); } } while (0)
struct app;
struct view {
    struct app *app;
    struct wl_surface *surface;
    struct xdg_surface *xdg;
    struct xdg_toplevel *top;
    struct wl_callback *frame;
    unsigned index;
    int32_t width, height;
    bool fullscreen;
};
struct buffer {
    struct wl_buffer *object;
    struct wl_list link;
};
struct app {
    struct wl_display *display;
    struct wl_registry *registry;
    struct wl_compositor *compositor;
    struct wl_shm *shm;
    struct xdg_wm_base *shell;
    struct wl_output *outputs[3];
    unsigned output_count;
    struct wl_list buffers;
    struct view views[2];
    uint32_t shell_version;
};
static void release(void *data, struct wl_buffer *object) {
    struct buffer *b = data;
    wl_buffer_destroy(object);
    wl_list_remove(&b->link);
    free(b);
}
static const struct wl_buffer_listener buffer_listener = {.release = release};

static struct wl_buffer *make_buffer(struct view *v) {
    CHECK(v->width > 0 && v->width <= 3840 && v->height > 0 && v->height <= 2160);
    size_t size = (size_t)v->width * (size_t)v->height * 4;
    int fd = memfd_create("aetheri-wayland-test", MFD_CLOEXEC);
    CHECK(fd >= 0 && ftruncate(fd, (off_t)size) == 0);
    uint32_t *pixels = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    CHECK(pixels != MAP_FAILED);
    uint32_t color = v->index == 0 ? UINT32_C(0xff2963c4) : UINT32_C(0xff26b478);
    for (size_t i = 0; i < size / 4; ++i) pixels[i] = color;
    /* A contrasting inset makes stride/order/cropping mistakes observable. */
    for (int y = 12; y < 32; ++y) for (int x = 12; x < 32; ++x)
        pixels[(size_t)y * (size_t)v->width + (size_t)x] = UINT32_C(0xffe9edf5);
    struct wl_shm_pool *pool = wl_shm_create_pool(v->app->shm, fd, (int32_t)size);
    CHECK(pool);
    struct buffer *b = calloc(1, sizeof(*b));
    CHECK(b);
    b->object = wl_shm_pool_create_buffer(pool, 0, v->width, v->height, v->width * 4, WL_SHM_FORMAT_XRGB8888);
    CHECK(b->object && wl_buffer_add_listener(b->object, &buffer_listener, b) == 0);
    wl_list_insert(&v->app->buffers, &b->link);
    wl_shm_pool_destroy(pool);
    CHECK(munmap(pixels, size) == 0 && close(fd) == 0);
    return b->object;
}
static void frame_done(void *data, struct wl_callback *callback, uint32_t time) {
    (void)time;
    struct view *v = data;
    CHECK(v->frame == callback);
    wl_callback_destroy(callback);
    v->frame = NULL;
    printf("FRAME %u %d %d %u\n", v->index, v->width, v->height, v->fullscreen ? 1u : 0u);
}
static const struct wl_callback_listener frame_listener = {.done = frame_done};
static void configured(void *data, struct xdg_surface *surface, uint32_t serial) {
    struct view *v = data;
    xdg_surface_ack_configure(surface, serial);
    struct wl_buffer *buffer = make_buffer(v);
    if (v->frame) wl_callback_destroy(v->frame);
    v->frame = wl_surface_frame(v->surface);
    CHECK(v->frame && wl_callback_add_listener(v->frame, &frame_listener, v) == 0);
    wl_surface_attach(v->surface, buffer, 0, 0);
    wl_surface_damage_buffer(v->surface, 0, 0, v->width, v->height);
    wl_surface_commit(v->surface);
    printf("CONFIG %u %d %d %u\n", v->index, v->width, v->height, v->fullscreen ? 1u : 0u);
}
static const struct xdg_surface_listener surface_listener = {.configure = configured};
static void top_configure(void *data, struct xdg_toplevel *top, int32_t width, int32_t height, struct wl_array *states) {
    (void)top;
    struct view *v = data;
    v->width = width > 0 ? width : 360;
    v->height = height > 0 ? height : 220;
    v->fullscreen = false;
    uint32_t *state;
    wl_array_for_each(state, states) if (*state == XDG_TOPLEVEL_STATE_FULLSCREEN) v->fullscreen = true;
}
static void top_close(void *data, struct xdg_toplevel *top) {
    (void)data; (void)top;
    CHECK(false);
}
static void top_bounds(void *data, struct xdg_toplevel *top, int32_t width, int32_t height) {
    (void)data; (void)top; (void)width; (void)height;
}
static void top_capabilities(void *data, struct xdg_toplevel *top, struct wl_array *capabilities) {
    (void)top;
    struct view *v = data;
    unsigned mask = 0;
    uint32_t *capability;
    wl_array_for_each(capability, capabilities) {
        CHECK(*capability == XDG_TOPLEVEL_WM_CAPABILITIES_MAXIMIZE ||
              *capability == XDG_TOPLEVEL_WM_CAPABILITIES_FULLSCREEN);
        mask |= 1u << *capability;
    }
    CHECK(mask == ((1u << XDG_TOPLEVEL_WM_CAPABILITIES_MAXIMIZE) |
                   (1u << XDG_TOPLEVEL_WM_CAPABILITIES_FULLSCREEN)));
    printf("CAPABILITIES %u\n", v->index);
}
static const struct xdg_toplevel_listener top_listener = {
    .configure = top_configure, .close = top_close,
    .configure_bounds = top_bounds, .wm_capabilities = top_capabilities};
static void ping(void *data, struct xdg_wm_base *shell, uint32_t serial) {
    (void)data;
    xdg_wm_base_pong(shell, serial);
}
static const struct xdg_wm_base_listener shell_listener = {.ping = ping};
static void output_geometry(void *data, struct wl_output *output, int32_t x, int32_t y,
        int32_t width, int32_t height, int32_t subpixel, const char *make, const char *model, int32_t transform) {
    (void)data; (void)output; (void)width; (void)height; (void)subpixel;
    (void)make; (void)model; (void)transform;
    printf("OUTPUT_POSITION %d %d\n", x, y);
}
static void output_mode(void *data, struct wl_output *output, uint32_t flags, int32_t width, int32_t height, int32_t refresh) {
    (void)data; (void)output; (void)refresh;
    if (flags & WL_OUTPUT_MODE_CURRENT) printf("OUTPUT_MODE %d %d\n", width, height);
}
static const struct wl_output_listener output_listener = {.geometry = output_geometry, .mode = output_mode};
static void global(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version) {
    struct app *a = data;
    if (!strcmp(interface, "wl_compositor")) {
        CHECK(!a->compositor && version >= 4);
        a->compositor = wl_registry_bind(registry, name, &wl_compositor_interface, 4);
    } else if (!strcmp(interface, "wl_shm")) {
        CHECK(!a->shm);
        a->shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
    } else if (!strcmp(interface, "xdg_wm_base")) {
        CHECK(!a->shell && version >= a->shell_version);
        a->shell = wl_registry_bind(registry, name, &xdg_wm_base_interface, a->shell_version);
        CHECK(xdg_wm_base_add_listener(a->shell, &shell_listener, a) == 0);
    } else if (!strcmp(interface, "wl_output")) {
        CHECK(a->output_count < 3);
        /* Version 1 needs no release request; no events are required for selection. */
        struct wl_output *output = wl_registry_bind(registry, name, &wl_output_interface, 1);
        CHECK(output && wl_output_add_listener(output, &output_listener, a) == 0);
        a->outputs[a->output_count++] = output;
    }
}
static void global_remove(void *data, struct wl_registry *registry, uint32_t name) {
    (void)data; (void)registry; (void)name;
    CHECK(false);
}
static const struct wl_registry_listener registry_listener = {.global = global, .global_remove = global_remove};
static void create_view(struct app *a, unsigned index) {
    struct view *v = &a->views[index];
    *v = (struct view){.app = a, .index = index, .width = 360, .height = 220};
    v->surface = wl_compositor_create_surface(a->compositor);
    v->xdg = xdg_wm_base_get_xdg_surface(a->shell, v->surface);
    v->top = xdg_surface_get_toplevel(v->xdg);
    CHECK(v->surface && v->xdg && v->top);
    CHECK(xdg_surface_add_listener(v->xdg, &surface_listener, v) == 0);
    CHECK(xdg_toplevel_add_listener(v->top, &top_listener, v) == 0);
    xdg_toplevel_set_title(v->top, index == 0 ? "Aetheri protocol test A" : "Aetheri protocol test B");
    xdg_toplevel_set_app_id(v->top, "aetheri.private.compositor-test");
    wl_surface_commit(v->surface);
}
static void destroy_view(struct view *v) {
    if (!v->surface) return;
    if (v->frame) wl_callback_destroy(v->frame);
    xdg_toplevel_destroy(v->top);
    xdg_surface_destroy(v->xdg);
    wl_surface_destroy(v->surface);
    v->surface = NULL;
    v->frame = NULL;
}
int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    CHECK(argc == 1 || (argc == 2 && !strcmp(argv[1], "--legacy")));
    struct app a = {.shell_version = argc == 2 ? 3 : 5};
    wl_list_init(&a.buffers);
    a.display = wl_display_connect(NULL);
    CHECK(a.display);
    a.registry = wl_display_get_registry(a.display);
    CHECK(a.registry && wl_registry_add_listener(a.registry, &registry_listener, &a) == 0);
    CHECK(wl_display_roundtrip(a.display) >= 0 && a.compositor && a.shm && a.shell && a.output_count == 3);
    create_view(&a, 0);
    create_view(&a, 1);
    for (;;) {
        CHECK(wl_display_dispatch_pending(a.display) >= 0);
        CHECK(wl_display_flush(a.display) >= 0 || errno == EAGAIN);
        struct pollfd fds[2] = {{wl_display_get_fd(a.display), POLLIN, 0}, {STDIN_FILENO, POLLIN, 0}};
        int r;
        do { r = poll(fds, 2, 10000); } while (r < 0 && errno == EINTR);
        CHECK(r > 0);
        if (fds[0].revents) CHECK(wl_display_dispatch(a.display) >= 0);
        if (fds[1].revents) {
            char c;
            CHECK(read(STDIN_FILENO, &c, 1) == 1);
            if (c == 'q') break;
            if (c == 'f') xdg_toplevel_set_fullscreen(a.views[0].top, a.outputs[2]);
            else if (c == 'r') xdg_toplevel_unset_fullscreen(a.views[0].top);
            else if (c == 'm') xdg_toplevel_set_maximized(a.views[0].top);
            else if (c == 'n') xdg_toplevel_unset_maximized(a.views[0].top);
            else if (c == 'p') {
                CHECK(!a.views[0].frame);
                a.views[0].frame = wl_surface_frame(a.views[0].surface);
                CHECK(a.views[0].frame && wl_callback_add_listener(a.views[0].frame, &frame_listener, &a.views[0]) == 0);
                wl_surface_commit(a.views[0].surface);
            }
            else if (c == 'd') { destroy_view(&a.views[1]); printf("DESTROY 1\n"); }
            else if (c == 'c') { CHECK(!a.views[1].surface); create_view(&a, 1); }
            else CHECK(false);
        }
    }
    for (unsigned i = 0; i < 2; ++i) destroy_view(&a.views[i]);
    CHECK(wl_display_roundtrip(a.display) >= 0);
    struct buffer *b, *next;
    wl_list_for_each_safe(b, next, &a.buffers, link) release(b, b->object);
    for (unsigned i = 0; i < a.output_count; ++i) wl_output_destroy(a.outputs[i]);
    xdg_wm_base_destroy(a.shell);
    wl_shm_destroy(a.shm);
    wl_compositor_destroy(a.compositor);
    wl_registry_destroy(a.registry);
    wl_display_disconnect(a.display);
    puts("CLOSED");
    return 0;
}
