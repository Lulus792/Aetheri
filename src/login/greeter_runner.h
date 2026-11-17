#ifndef AETHERI_LOGIN_GREETER_RUNNER_H
#define AETHERI_LOGIN_GREETER_RUNNER_H

#include "login/ui/host.h"

struct ae_login_greeter_profile {
    struct ae_login_client_expect backend;
    struct ae_login_source_expect compositor;
    const char *compositor_label, *compositor_socket_label;
    const char *runtime; /* Independently selected greeter-owned 0700 directory. */
    const char *libraries; /* Optional protected runtime directory for the child. */
    const char *input_quirks; /* Optional protected libinput data directory. */
    const char *seat;
    unsigned vt;
    const void *font; /* Independently verified, resident font bytes. */
    size_t font_size;
};

/* Fresh main-thread-only greeter process. The compiled profile selects actual
   deployment, including MAC/namespace/runtime and its backend host gate. The
   socket hint names a single candidate; authentication still requires the
   original kernel peer and the protected handoff. Returns only for exit.
   No reconnect, session replay or authority to stop the backend. */
int ae_login_greeter_run(const struct ae_login_greeter_profile *, enum ae_login_ui_end *);

/* Explicit build-selected provider. It must bound the startup under the
   deployment supervisor and return -errno on any missing admission. Its
   expectations and gate context remain available until process exit. */
int ae_login_greeter_deployment_select(struct ae_login_greeter_profile *);

#endif
