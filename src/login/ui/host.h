#ifndef AETHERI_LOGIN_UI_HOST_H
#define AETHERI_LOGIN_UI_HOST_H

#include "login/client.h"
#include "login/display.h"
#include "linux/session.h"
#include <nexaui/nexaui.h>

enum ae_login_ui_end { AE_LOGIN_UI_STOPPED, AE_LOGIN_UI_START_ACK };

struct ae_login_ui_profile {
    struct ae_login_display *display; /* Pending original child offer. */
    struct ae_session *session; /* Live greeter class and selected VT. */
    const nx_font_collection *fonts;
    const struct ae_login_client_expect *backend;
    int stop_fd; /* Borrowed readable shutdown notification. */
};

/* Run on the original greeter's process leader, in an exclusive trusted
   single-threaded bootstrap environment. The caller independently admits
   the running code, native libraries, environment, backend hint, font and
   device policy, and bounds the SDK's initial synchronous Wayland roundtrips
   through its deployment supervisor. The host has no display-name fallback.
   The backend stream moves only if the original profile admits its owner.
   The caller retains its display/session owners and retires the original
   compositor after return. The end result changes only on success. A Start
   ACK confirms scheduling, without observing a desktop or admitting any app. */
int ae_login_ui_run(const struct ae_login_ui_profile *, sd_bus *, int *backend_fd,
                    enum ae_login_ui_end *end);

#endif
