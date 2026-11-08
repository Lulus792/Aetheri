/* Real FD ownership with controlled wlroots enumeration; no DRM admission. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include "../src/compositor/drm_selection.h"

static ssize_t result;
static struct wlr_device *fixtures[17];
static size_t closed, opened;

ssize_t wlr_session_find_gpus(struct wlr_session *session, size_t length, struct wlr_device **ret) {
    assert(length == 17);
    for (ssize_t i = 0; i < result && i < (ssize_t)length; ++i) {
        struct wlr_device *device = calloc(1, sizeof(*device));
        assert(device);
        fixtures[(size_t)i] = device;
        device->fd = open("/dev/null", O_RDONLY | O_CLOEXEC);
        assert(device->fd >= 0);
        wl_list_insert(&session->devices, &device->link);
        ret[(size_t)i] = device;
        ++opened;
    }
    return result;
}
struct wlr_device *wlr_session_open_file(struct wlr_session *session, const char *path) {
    assert(!strcmp(path, "/dev/dri/card9"));
    struct wlr_device *ret[17];
    result = 1;
    assert(wlr_session_find_gpus(session, 17, ret) == 1);
    return ret[0];
}
void wlr_session_close_file(struct wlr_session *session, struct wlr_device *device) {
    (void)session;
    assert(device->fd >= 0 && !close(device->fd));
    wl_list_remove(&device->link);
    bool found = false;
    for (size_t i = 0; i < 17; ++i) {
        if (fixtures[i] == device) { fixtures[i] = NULL; found = true; break; }
    }
    assert(found);
    free(device);
    ++closed;
}

int main(void) {
    struct wlr_session session = {0};
    wl_list_init(&session.devices);
    assert(!unsetenv("WLR_DRM_DEVICES"));
    for (ssize_t count = -1; count <= 17; ++count) {
        result = count; opened = closed = 0;
        struct wlr_device *chosen = ae_drm_select(&session, "auto");
        assert(opened == (count > 0 ? (size_t)count : 0));
        if (count > 0 && count < 17) {
            assert(chosen == fixtures[0] && fcntl(chosen->fd, F_GETFD) >= 0);
            assert(closed == (size_t)count - 1);
            uintptr_t address = (uintptr_t)chosen;
            ae_drm_close_unclaimed(&session, address);
            assert(closed == opened);
            /* A consumed constructor input must not be closed twice. */
            ae_drm_close_unclaimed(&session, address);
            assert(closed == opened);
        } else assert(!chosen && closed == opened);
        assert(wl_list_empty(&session.devices));
    }
    assert(!setenv("WLR_DRM_DEVICES", "/dev/dri/card0", 1));
    result = 1; opened = closed = 0;
    assert(!ae_drm_select(&session, "auto") && !opened && !closed);
    struct wlr_device *explicit = ae_drm_select(&session, "/dev/dri/card9");
    assert(explicit == fixtures[0] && opened == 1);
    ae_drm_close_unclaimed(&session, (uintptr_t)explicit);
    assert(closed == 1 && wl_list_empty(&session.devices));
    assert(!unsetenv("WLR_DRM_DEVICES"));
    puts("DRM selection: bounded enumeration, nonselected/consumed FD closure and override refusal passed");
    return 0;
}
