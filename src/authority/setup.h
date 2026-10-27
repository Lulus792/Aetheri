#ifndef AETHERI_AUTHORITY_SETUP_H
#define AETHERI_AUTHORITY_SETUP_H
#include "catalogue/setup.h"
#include "linux/process.h"

int ae_setup_load(struct ae_setup_profile *profile,uint8_t digest[32]);
int ae_setup_sources(const struct ae_setup_profile *profile,int *code,int *data);
int ae_setup_verify_image(const struct ae_setup_profile *profile);
int ae_setup_context_digest(const struct ae_process_context *context,const uint8_t profile_digest[32],uint8_t output[32]);
struct ae_setup_service;
/* Internal trusted supervisor observation, including retained invocation and
 * no-job/process/cgroup drain. No held peer or catalogue mutation is implied. */
int ae_setup_observe_exit(struct ae_setup_service *,const struct ae_setup_profile *,const uint8_t digest[32]);
struct ae_setup_service {
    struct ae_catalogue *catalogue;
    sd_bus *bus;
    void *owner;
    int (*broker)(void *,sd_bus_message *,struct ae_launch_context *);
    uint8_t epoch[16],runtime[16],boot[16];
    struct ae_peer peer;
    struct ae_process_context context;
    uint8_t lease[16],digest[32],evidence[32];
    int pinned,terminal_only;
};
int ae_setup_method_prepare(sd_bus_message *,struct ae_setup_service *,sd_bus_error *);
int ae_setup_method_register(sd_bus_message *,struct ae_setup_service *,sd_bus_error *);
int ae_setup_method_prepared(sd_bus_message *,struct ae_setup_service *,sd_bus_error *);
int ae_setup_method_withdraw(sd_bus_message *,struct ae_setup_service *,sd_bus_error *);
/* Offline owner administration, under the exclusive catalogue lock and original
 * runtime, for a failed unregistered setup-only invocation. No target exec. */
int ae_setup_recover_unbound(struct ae_catalogue *,const uint8_t boot[16]);
void ae_setup_service_clear(struct ae_setup_service *);
#endif
