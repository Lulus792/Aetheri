#ifndef AETHERI_LOGIN_SERVICE_INTERNAL_H
#define AETHERI_LOGIN_SERVICE_INTERNAL_H
#include "login/service.h"
#include "linux/image_internal.h"

/* Internal observation during the guarded service check. Callback arguments
   are borrowed for this call. Read-only operations on the selected PID1 only. */
typedef int (*ae_login_service_inspector)(sd_bus *,const char *manager,const char *unit_path,
                                          const struct ae_image_process *,const uint8_t invocation[16],
                                          const uint8_t boot[16],uint64_t cgroup,void *userdata);
int ae_login_service_inspect(struct ae_login_service *,ae_login_service_inspector,void *userdata);
#endif
