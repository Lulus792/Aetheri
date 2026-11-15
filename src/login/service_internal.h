#ifndef AETHERI_LOGIN_SERVICE_INTERNAL_H
#define AETHERI_LOGIN_SERVICE_INTERNAL_H
#include "login/service.h"

/* Internal observation during the guarded service check. Callback arguments
   are borrowed for this call. Read-only operations on the selected PID1 only. */
typedef int (*ae_login_service_inspector)(sd_bus *,const char *manager,const char *unit_path,
                                          pid_t main_pid,void *userdata);
int ae_login_service_inspect(struct ae_login_service *,ae_login_service_inspector,void *userdata);
#endif
