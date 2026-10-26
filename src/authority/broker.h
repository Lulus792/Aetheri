#ifndef AETHERI_AUTHORITY_BROKER_H
#define AETHERI_AUTHORITY_BROKER_H

#include "linux/process.h"
#include <systemd/sd-event.h>

struct ae_broker_identity { uint8_t account[16], epoch[16], runtime[16]; };
struct ae_broker {
    struct ae_peer peer;
    struct ae_process_context context;
    struct ae_broker_identity identity;
    sd_event_source *exit_source;
    sd_bus_slot *name_slot;
};
#define AE_BROKER_INIT { .peer = AE_PEER_INIT }

/* Trusted fixed P0 enrollment: caller supplies protected identity/unit, never
 * client-controlled paths or unit names. Requires UID 1000 and exact broker domain.
 * Registration may precede notify readiness; verify always requires active state.
 * Success transfers peer's FD only on a new binding (sets peer->pidfd=-1).
 * Repeat same-binding calls are idempotent; a live different binding conflicts.
 */
int ae_broker_bind(struct ae_broker *broker, struct ae_peer *peer, sd_bus *system,
                   sd_event *event, const struct ae_broker_identity *identity,
                   const char *protected_unit, const uint8_t boot[16]);
/* Borrow current message capture. Checks same bus/sender and live current context;
 * rechecks binding after nested manager calls. This is not a catalogue/launch fence.
 */
int ae_broker_verify(struct ae_broker *broker, const struct ae_peer *peer, sd_bus *system);
void ae_broker_clear(struct ae_broker *broker);
/* Call only for authenticated bus-daemon NameOwnerChanged on the matching bus. */
void ae_broker_name_lost(struct ae_broker *broker, const char *name, const char *new_owner);

#endif
