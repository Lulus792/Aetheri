#ifndef AETHERI_LINUX_WAYLAND_SHM_H
#define AETHERI_LINUX_WAYLAND_SHM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <wayland-client.h>

struct ae_wayland_buffer {
    struct wl_buffer *object;
    void *pixels;
    size_t size;
    int width, height, pitch;
    bool busy;
    void (*available)(void *);
    void *owner;
};

/* Start from a zero-initialized slot. The compositor keeps its own storage
   reference after attach. Only a release permits writing or reusing a slot. */
int ae_wayland_buffer_open(struct ae_wayland_buffer *buffer, struct wl_shm *shm,
    uint32_t format, int width, int height, void (*available)(void *), void *owner);
void ae_wayland_buffer_present(struct ae_wayland_buffer *buffer, struct wl_surface *surface,
    int width, int height);
/* Closing a busy slot abandons it permanently. It never makes it reusable. */
void ae_wayland_buffer_close(struct ae_wayland_buffer *buffer);

#endif
