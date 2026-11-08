#ifndef AETHERI_DRM_SELECTION_H
#define AETHERI_DRM_SELECTION_H

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <wlr/backend/session.h>

/* The active logind/libseat session opens devices. Kernel primary-device
 * ordering comes from the selected wlroots; card numbers are not identities.
 * One spare slot detects saturation rather than accepting a truncated choice. */
static struct wlr_device *ae_drm_select(struct wlr_session *session, const char *card) {
    if (strcmp(card, "auto")) return wlr_session_open_file(session, card);
    if (getenv("WLR_DRM_DEVICES")) return NULL;
    struct wlr_device *devices[17] = {0};
    ssize_t count = wlr_session_find_gpus(session, 17, devices);
    if (count <= 0) return NULL;
    bool saturated = count >= 17;
    size_t end = count > 17 ? 17 : (size_t)count;
    for (size_t i = saturated ? 0 : 1; i < end; ++i) {
        if (devices[i]) wlr_session_close_file(session, devices[i]);
    }
    return saturated ? NULL : devices[0];
}

/* wlroots 0.20.2's DRM constructor consumes the device on late failures, but
 * leaves it with the session on early failures. Test membership before closing;
 * retain an integer address before that call, not a possibly invalid pointer. */
static void ae_drm_close_unclaimed(struct wlr_session *session, uintptr_t selected) {
    struct wlr_device *device;
    wl_list_for_each(device, &session->devices, link) {
        if ((uintptr_t)device == selected) {
            wlr_session_close_file(session, device);
            break;
        }
    }
}

#endif
