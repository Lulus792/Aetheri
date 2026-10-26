#include "authority/broker.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>

static int same_connection(const struct ae_peer *a, const struct ae_peer *b)
{
    return a->pidfd >= 0 && b->pidfd >= 0 && a->connection_pid == b->connection_pid &&
           memcmp(a->bus_id, b->bus_id, sizeof a->bus_id) == 0 && strcmp(a->unique_name, b->unique_name) == 0;
}

static int name_alive(sd_bus *bus, const char *name)
{
    sd_bus_message *reply = NULL;
    int result = sd_bus_call_method(bus, "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "NameHasOwner", NULL, &reply, "s", name);
    int owned = 0;
    if (result >= 0 && !sd_bus_message_has_signature(reply, "b")) result = -EBADMSG;
    if (result >= 0) result = sd_bus_message_read(reply, "b", &owned);
    sd_bus_message_unref(reply);
    return result < 0 ? result : (owned ? 0 : -ENOTCONN);
}

void ae_broker_clear(struct ae_broker *broker)
{
    if (broker == NULL) return;
    if (broker->exit_source != NULL) (void)sd_event_source_set_enabled(broker->exit_source, SD_EVENT_OFF);
    broker->exit_source = sd_event_source_unref(broker->exit_source);
    broker->name_slot = sd_bus_slot_unref(broker->name_slot);
    ae_peer_clear(&broker->peer);
    memset(&broker->identity, 0, sizeof broker->identity);
    memset(&broker->context, 0, sizeof broker->context);
}

static int owner_lost(sd_bus_message *message, void *userdata, sd_bus_error *error)
{
    (void)error;
    const char *name, *old_owner, *new_owner;
    int result = sd_bus_message_read(message, "sss", &name, &old_owner, &new_owner);
    (void)old_owner;
    if (result > 0) ae_broker_name_lost(userdata, name, new_owner);
    return result < 0 ? result : 0;
}

static int departed(sd_event_source *source, int fd, uint32_t events, void *userdata)
{
    (void)fd; (void)events;
    struct ae_broker *broker = userdata;
    if (broker->exit_source == source) ae_broker_clear(broker);
    return 0;
}

int ae_broker_bind(struct ae_broker *broker, struct ae_peer *peer, sd_bus *system,
                   sd_event *event, const struct ae_broker_identity *identity,
                   const char *protected_unit, const uint8_t boot[16])
{
    if (broker == NULL || peer == NULL || event == NULL || identity == NULL || protected_unit == NULL || boot == NULL) return -EINVAL;
    if (peer->connection_uid != 1000 || strcmp(peer->connection_label, "system_u:system_r:aetheri_user_t:s0") != 0) return -EACCES;
    struct ae_process_context context;
    int result = ae_process_observe_registration(peer, system, &context);
    if (result != 0) return result;
    if (strcmp(context.unit, protected_unit) != 0 || memcmp(context.boot, boot, sizeof context.boot) != 0) return -EACCES;
    if (broker->peer.pidfd >= 0) {
        result = ae_peer_alive(&broker->peer);
        if (result == -ESRCH) ae_broker_clear(broker);
        else if (result != 0) return result;
        else return same_connection(&broker->peer, peer) && ae_process_context_equal(&broker->context, &context) &&
                    memcmp(&broker->identity, identity, sizeof *identity) == 0 ? 0 : -EALREADY;
    }
    sd_event_source *source = NULL;
    sd_bus_slot *slot = NULL;
    char match[512];
    int length = snprintf(match, sizeof match, "type='signal',sender='org.freedesktop.DBus',path='/org/freedesktop/DBus',interface='org.freedesktop.DBus',member='NameOwnerChanged',arg0='%s'", peer->unique_name);
    if (length < 0 || (size_t)length >= sizeof match) return -E2BIG;
    result = sd_bus_add_match(system, &slot, match, owner_lost, broker);
    if (result < 0) return result;
    result = sd_event_add_io(event, &source, peer->pidfd, EPOLLIN, departed, broker);
    if (result >= 0) result = ae_peer_alive(peer);
    if (result >= 0) result = name_alive(system, peer->unique_name);
    /* The match precedes NameHasOwner, so a disconnect cannot fall in an
     * unobserved gap. Recheck after that nested bus call before taking ownership. */
    if (result == 0 && broker->peer.pidfd >= 0) {
        int alive = ae_peer_alive(&broker->peer);
        if (alive == -ESRCH) ae_broker_clear(broker);
        else if (alive < 0) result = alive;
        else result = same_connection(&broker->peer, peer) && ae_process_context_equal(&broker->context, &context) &&
                      memcmp(&broker->identity, identity, sizeof *identity) == 0 ? 1 : -EALREADY;
    }
    if (result != 0) { sd_event_source_unref(source); sd_bus_slot_unref(slot); return result == 1 ? 0 : result; }
    broker->peer = *peer;
    peer->pidfd = -1;
    broker->identity = *identity;
    broker->context = context;
    broker->exit_source = source;
    broker->name_slot = slot;
    return 0;
}

int ae_broker_verify(struct ae_broker *broker, const struct ae_peer *peer, sd_bus *system)
{
    if (broker == NULL || peer == NULL) return -EINVAL;
    if (!same_connection(&broker->peer, peer)) return -EACCES;
    struct ae_process_context expected = broker->context;
    struct ae_broker_identity identity = broker->identity;
    int result = ae_process_verify(peer, system, &expected);
    if (result == 0) result = name_alive(system, peer->unique_name);
    if (result == 0 && (!same_connection(&broker->peer, peer) ||
        !ae_process_context_equal(&broker->context, &expected) || memcmp(&identity, &broker->identity, sizeof identity) != 0)) result = -ESTALE;
    return result;
}

void ae_broker_name_lost(struct ae_broker *broker, const char *name, const char *new_owner)
{
    if (broker != NULL && name != NULL && new_owner != NULL && new_owner[0] == 0 &&
        broker->peer.pidfd >= 0 && strcmp(name, broker->peer.unique_name) == 0) ae_broker_clear(broker);
}
