#ifndef AETHERI_LOGIN_LOADED_H
#define AETHERI_LOGIN_LOADED_H
#include "login/service.h"
#include "login/sources.h"

struct ae_login_loaded;
/* Join an original active service to three distinct independently selected
   source entries: fragment (data), program (ELF), configuration (data).
   The loaded, non-transient unit must start exactly PROGRAM --config CONFIG
   as a single Type=exec command, with no flags or extra commands. Any drop-ins
   must correspond to distinct DATA indices in exact manager-reported order.
   Both borrowed guards must outlive the returned handle. Output starts NULL
   and stays unchanged on failure. Same original process and thread. */
int ae_login_loaded_create(struct ae_login_service *,struct ae_login_sources *,
                           size_t fragment,size_t program,size_t config,
                           const size_t *dropins,size_t dropin_count,
                           struct ae_login_loaded **out);
/* Read-only callback component. Checks source identity, the same supervised
   socket peer and its loaded command record. Permanently fences on observed
   drift. Loaded metadata can describe a configuration reloaded after launch.
   The caller still needs independent current ELF, actually consumed config,
   loaded policy and greeter admission before sending credentials. */
int ae_login_loaded_check(void *);
/* Require the consumed-config notice from the original PID1 service, with
   effective NotifyAccess=main and exact invocation and selected config digest.
   An empty notice before first observation returns -EAGAIN after the complete
   service/source recheck. Caller owns startup deadline. A nonempty mismatch,
   observed loss or wrong sender policy fences the handle. Once requested,
   ordinary and image checks also require the notice. This is one
   component of admission and requires a separately trusted running backend. */
int ae_login_loaded_check_config(void *);
/* Administrative observer in trusted host proc/PID/mount views. Recheck the
   complete loaded/source selection and compare the running peer executable
   with the original PROGRAM object. The selected authenticated peer must be
   UID/GID 0 and the caller must already hold the required proc access. Output
   unchanged on error. This observes the current executed ELF only. */
int ae_login_loaded_match_image(struct ae_login_loaded *,struct ae_image *out);
int ae_login_loaded_close(struct ae_login_loaded **);
#endif
