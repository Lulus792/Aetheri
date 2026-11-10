#ifndef AETHERI_LOGIN_SERVICE_H
#define AETHERI_LOGIN_SERVICE_H
#include <sys/types.h>
#include <systemd/sd-bus.h>

struct ae_login_service;
struct ae_login_service_expect {
    const char *unit; /* Independently selected original system .service. */
    const char *label; /* Exact independently admitted raw SELinux label. */
    const char *socket_label; /* Separate exact connection socket context. */
    uid_t uid;
    gid_t gid;
};
/* Observe an already connected local stream's kernel peer, not a bus client.
   Trusted host SYSTEM bus/proc/cgroup/PID namespace and enforcing SELinux only.
   Capture a live original active MainPID/no ControlPID/no Job, exact unit and
   invocation/cgroup/kernel credentials/label, twice. Own a bus ref and kernel
   SO_PEERPIDFD; borrow the socket only during construction, without reading,
   sending, changing flags or consuming it. No supplied PID or fake ae_peer.
   Output starts NULL; failures preserve output and caller socket.
   This is ONLY the service-context portion of login admission. Independently
   verified image/config/unit sources, loaded-unit/policy and greeter admission
   remain mandatory. Context matching is not current ELF or session authority. */
int ae_login_service_create(int socket_fd,sd_bus *trusted_system_bus,
    const struct ae_login_service_expect *,struct ae_login_service **out);
/* Suitable for composing into connection_source.check, never a complete login
   admission callback on its own. First observed loss permanently fences; no
   adoption/reconnect/rebind. Same original process/thread; bounded read-only
   synchronous bus calls inherit the caller's configured bus timeout. */
int ae_login_service_check(void *service);
/* Borrow original authenticated socket peer pidfd for connection construction;
   no numeric PID reopening. Output unchanged on error. */
int ae_login_service_peer(struct ae_login_service *,int *pidfd);
/* Local observation/FD/ref cleanup only; no stop, cancel, lease or drain effect.
   Fork child may close its local copy. Callback/bus reentry close rejects. */
int ae_login_service_close(struct ae_login_service **);
#endif
