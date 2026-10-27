#ifndef AETHERI_AUTHORITY_PROJECTION_H
#define AETHERI_AUTHORITY_PROJECTION_H

#include "catalogue/projection.h"
#include <systemd/sd-bus.h>

/* Bounded wire codec only. Read preserves output on any malformed/extra input;
 * write requires an empty caller-owned reply. No authentication or admission. */
int ae_projection_read(sd_bus_message *message,struct ae_install_snapshot *output);
int ae_projection_write(sd_bus_message *message,const struct ae_install_snapshot *snapshot);
/* Call fixed trusted system bus/name. Expected source epoch/runtime/boot and
 * account/install must match exactly. Caller has authenticated/checked its broker
 * registration and current root context. Never persists or retries the request. */
int ae_projection_fetch(sd_bus *system,const struct ae_projection_context *context,
                         const uint8_t account[16],const uint8_t install[16],
                         struct ae_install_snapshot *output);
#endif
