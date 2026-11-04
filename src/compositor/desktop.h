#ifndef AETHERI_COMPOSITOR_DESKTOP_H
#define AETHERI_COMPOSITOR_DESKTOP_H
#include <stdbool.h>
struct server;
struct window;
struct wlr_surface;
struct ae_desktop;
/* Takes the original connected stream FD on success; caller closes on failure. */
struct ae_desktop *ae_desktop_create(struct server *server, int fd);
void ae_desktop_destroy(struct ae_desktop *desktop);
void ae_desktop_arrange(struct ae_desktop *desktop);
void ae_desktop_window(struct ae_desktop *desktop, struct window *window);
void ae_desktop_closed(struct ae_desktop *desktop, struct window *window);
bool ae_desktop_surface(struct ae_desktop *desktop, struct wlr_surface *surface);
struct wlr_surface *ae_desktop_keyboard_surface(struct ae_desktop *desktop);
bool ae_desktop_toggle(struct ae_desktop *desktop, unsigned output);
#endif
