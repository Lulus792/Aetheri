#ifndef AETHERI_LOGIN_DISPLAY_H
#define AETHERI_LOGIN_DISPLAY_H

#include "core/image.h"

struct ae_login_display;
struct ae_login_display_expect {
    struct ae_image image;
    const char *process_label;
    const char *socket_label;
};

/* The greeter owns a private unnamed seqpacket control channel to one pinned
   direct child. The pin comes from the owner's launch, before releasing the
   child to exec. Never acquire it from peer credentials. The selected image
   must come from independent protected source admission. Both processes use
   the greeter's current non-Root UID and GID. Move control only on success. */
int ae_login_display_create(int *control, int child_pidfd,
    const struct ae_login_display_expect *, struct ae_login_display **out);

/* Borrow the pending control fd and live child pidfd for poll. After admission
   the control fd is -1. No read, close or mutation of their flags. */
int ae_login_display_watch(struct ae_login_display *, int *control, int *child_pidfd);

/* Complete the single offer. EAGAIN before any received packet is pending.
   A valid client endpoint is moved to *wayland_fd (initially -1). Its kernel
   peer must be the held child and its runtime label and ELF must match. Any
   other failure fences the owner. No pathname, second offer or peer fallback. */
int ae_login_display_take_original(struct ae_login_display *, int *wayland_fd);

/* Recheck original child and running image. This does not consume Wayland
   traffic or attest its output, input, active session or frame. The host must
   check those separately and own the endpoint until native client teardown. */
int ae_login_display_check(void *);
int ae_login_display_close(struct ae_login_display **);

#endif
