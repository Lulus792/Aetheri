#ifndef AETHERI_AUTHORITY_SYSTEM_CONNECTION_H
#define AETHERI_AUTHORITY_SYSTEM_CONNECTION_H

#include "authority/broker.h"

struct ae_system_connection;

/* Process-owned, single-threaded reciprocal P0 identity. Borrow bus/event until
 * close. The caller independently selects the protected unit, epoch and boot;
 * User's account/epoch/runtime are not claims about Root. No app adoption, lease
 * release, unit mutation or recovery follows from connection departure. Use the
 * dedicated outbound bus: verification dispatches its bounded pending messages
 * to observe queued name loss; nested use/close is rejected with EBUSY. */
int ae_system_connection_new(sd_bus *bus, sd_event *event,
    const char *unit, const uint8_t epoch[16], const uint8_t boot[16],
    const struct ae_broker_identity *user, struct ae_system_connection **output);
/* Reobserve original MainPID/unit/invocation/current credentials and name owner.
 * A live actor's context/protocol drift latches; actual departure is separate. */
int ae_system_connection_verify(struct ae_system_connection *connection);
/* Reread protected configuration outside the module; changed selection cannot
 * authorize adoption of another unit while this original actor still exists. */
int ae_system_connection_verify_selection(struct ae_system_connection *connection,
    const char *unit, const uint8_t epoch[16], const uint8_t boot[16]);
int ae_system_connection_departed(const struct ae_system_connection *connection);
int ae_system_connection_runtime(struct ae_system_connection *connection, uint8_t output[16]);
int ae_system_connection_check(struct ae_system_connection *connection);
/* Construct only unique-destination requests. Call never retries; successful
 * replies must be correlated to the retained request and original current peer.
 * Output stays NULL on failure. Errors are not authenticated method results. */
int ae_system_connection_request(struct ae_system_connection *connection,
    const char *path, const char *interface, const char *method, sd_bus_message **output);
int ae_system_connection_call(struct ae_system_connection *connection,
    sd_bus_message *request, sd_bus_message **output);
/* EBUSY during a nested callback, ECHILD after fork. No borrowed resource closes. */
int ae_system_connection_close(struct ae_system_connection **connection);

#endif
