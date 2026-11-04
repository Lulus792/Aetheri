/* Private compositor ownership shared by the render and input modules. */
#ifndef AETHERI_COMPOSITOR_SERVER_H
#define AETHERI_COMPOSITOR_SERVER_H
#include <stdbool.h>
#include <stdint.h>
#include <wayland-server-core.h>
enum { OUTPUT_LIMIT = 3, WINDOW_LIMIT = 64, CAPTURE_LIMIT = 16 };
struct server;
struct ae_input;
struct output {
    struct server *server;
    struct wlr_output *output;
    struct wlr_scene_output *scene;
    struct wl_listener frame;
    struct wl_listener destroy;
    struct wl_listener request_state;
    unsigned index;
    unsigned capture;
    uint64_t commits;
};
struct window {
    struct server *server;
    struct wlr_xdg_toplevel *toplevel;
    struct wlr_scene_tree *tree;
    struct wl_list link;
    struct wl_listener commit, destroy, map, unmap, maximize, fullscreen, move, resize;
    unsigned home;
    int x, y;
    bool mapped;
};
struct server {
    struct wl_display *display;
    struct wlr_backend *backend;
    struct wlr_renderer *renderer;
    struct wlr_allocator *allocator;
    struct wlr_scene *scene;
    struct wlr_output_layout *layout;
    struct wlr_xdg_shell *shell;
    struct ae_input *input;
    struct wl_listener new_toplevel;
    struct wl_listener backend_destroy;
    struct output outputs[OUTPUT_LIMIT];
    struct wl_list windows;
    unsigned output_count, window_count, next_home, capture;
    int capture_fd;
    bool failed, nested, shutting_down;
};


#endif
