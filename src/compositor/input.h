#ifndef AETHERI_COMPOSITOR_INPUT_H
#define AETHERI_COMPOSITOR_INPUT_H
#include <stdbool.h>
struct server;
struct window;
struct wlr_input_device;
struct ae_input;
struct wlr_surface;
struct ae_input *ae_input_create(struct server *server);
void ae_input_destroy(struct ae_input *input);
void ae_input_stop_backend(struct ae_input *input);
bool ae_input_add(struct ae_input *input, struct wlr_input_device *device);
void ae_input_map_outputs(struct ae_input *input);
void ae_input_map(struct window *window);
void ae_input_unmap(struct window *window);
void ae_input_move(struct window *window, void *event);
void ae_input_resize(struct window *window, void *event);
void ae_input_activate(struct ae_input *input, struct window *window);
void ae_input_shell_focus(struct ae_input *input, struct wlr_surface *surface);
void ae_input_restore(struct ae_input *input);
bool ae_input_focused(struct ae_input *input, struct window *window);
#endif
