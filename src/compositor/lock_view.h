#ifndef AETHERI_COMPOSITOR_LOCK_VIEW_H
#define AETHERI_COMPOSITOR_LOCK_VIEW_H

#include <stdbool.h>
#include <stdint.h>

struct ae_lock_view;
struct server;
struct wl_client;
struct wl_global;
struct wlr_surface;
struct ae_lock_view *ae_lock_view_create(struct server *, int fd);
bool ae_lock_view_global(const struct wl_client *, const struct wl_global *, void *);
void ae_lock_view_destroy(struct ae_lock_view *);
void ae_lock_view_show(struct ae_lock_view *);
void ae_lock_view_hide(struct ae_lock_view *);
bool ae_lock_view_ready(const struct ae_lock_view *);
struct wlr_surface *ae_lock_view_surface(const struct ae_lock_view *);
void ae_lock_view_focus(struct ae_lock_view *);
void ae_lock_view_before_commit(struct ae_lock_view *, uint32_t sequence);
void ae_lock_view_commit(struct ae_lock_view *, uint32_t sequence, bool committed);
void ae_lock_view_present(struct ae_lock_view *, uint32_t sequence, bool presented);

#endif
