#define _GNU_SOURCE
#include "wayland_shm.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

static void released(void *data, struct wl_buffer *object)
{
    struct ae_wayland_buffer *buffer = data;
    (void)object;
    buffer->busy = false;
    buffer->available(buffer->owner);
}

static const struct wl_buffer_listener listener = {.release = released};

int ae_wayland_buffer_open(struct ae_wayland_buffer *buffer, struct wl_shm *shm,
    uint32_t format, int width, int height, void (*available)(void *), void *owner)
{
    if (!shm || !available || width <= 0 || height <= 0 || width > 7680 || height > 4320 ||
        (format != WL_SHM_FORMAT_ARGB8888 && format != WL_SHM_FORMAT_XRGB8888)) return -EINVAL;
    if (buffer->object || buffer->size) return -EBUSY;
    int pitch = width * 4;
    int size = pitch * height;
    int fd = open("/dev/shm", O_TMPFILE | O_RDWR | O_CLOEXEC, 0600);
    if (fd < 0) return -errno;
    int error = posix_fallocate(fd, 0, size);
    void *pixels = error ? MAP_FAILED : mmap(NULL, (size_t)size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (pixels == MAP_FAILED) {
        if (!error) error = errno;
        (void)close(fd);
        return -error;
    }
    *buffer = (struct ae_wayland_buffer){.pixels = pixels, .size = (size_t)size,
        .width = width, .height = height, .pitch = pitch, .available = available, .owner = owner};
    struct wl_shm_pool *pool = wl_shm_create_pool(shm, fd, size);
    (void)close(fd);
    if (pool) {
        buffer->object = wl_shm_pool_create_buffer(pool, 0, width, height, pitch, format);
        wl_shm_pool_destroy(pool);
    }
    if (!buffer->object || wl_buffer_add_listener(buffer->object, &listener, buffer)) {
        ae_wayland_buffer_close(buffer);
        return -EIO;
    }
    return 0;
}

void ae_wayland_buffer_present(struct ae_wayland_buffer *buffer, struct wl_surface *surface,
    int width, int height)
{
    buffer->busy = true;
    wl_surface_attach(surface, buffer->object, 0, 0);
    wl_surface_damage(surface, 0, 0, width, height);
    wl_surface_commit(surface);
}

void ae_wayland_buffer_close(struct ae_wayland_buffer *buffer)
{
    if (buffer->object) wl_buffer_destroy(buffer->object);
    if (buffer->size) (void)munmap(buffer->pixels, buffer->size);
    *buffer = (struct ae_wayland_buffer){0};
}
