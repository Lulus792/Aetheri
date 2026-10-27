#include "linux/peer.h"

#include <errno.h>
#include <poll.h>
#include <string.h>
#include <unistd.h>

int ae_peer_alive(const struct ae_peer *peer)
{
    if (peer == NULL || peer->pidfd < 0) return -EINVAL;
    struct pollfd item = {.fd = peer->pidfd, .events = POLLIN};
    int result;
    do { result = poll(&item, 1, 0); } while (result < 0 && errno == EINTR);
    if (result < 0) return -errno;
    if (item.revents & POLLNVAL) return -EBADF;
    return result == 0 ? 0 : -ESRCH;
}

void ae_peer_clear(struct ae_peer *peer)
{
    if (peer == NULL) return;
    if (peer->pidfd >= 0) close(peer->pidfd);
    memset(peer, 0, sizeof *peer);
    peer->pidfd = -1;
}

static int capture(sd_bus_message *message,uint8_t expected_type,struct ae_peer *output)
{
    if (message == NULL || output == NULL || output->pidfd>=0) return -EINVAL;
    uint8_t type = 0;
    int result = sd_bus_message_get_type(message, &type);
    if (result < 0) return result;
    if (type != expected_type) return -EPROTO;
    sd_bus *bus = sd_bus_message_get_bus(message);
    if (bus == NULL || sd_bus_is_bus_client(bus) <= 0) return -EPROTO;
    const char *sender = sd_bus_message_get_sender(message);
    if (sender == NULL || sender[0] != ':' || !sd_bus_service_name_is_valid(sender)) return -EPROTO;

    struct ae_peer temporary = AE_PEER_INIT;
    size_t length = strnlen(sender, sizeof temporary.unique_name);
    if (length == sizeof temporary.unique_name) return -E2BIG;
    memcpy(temporary.unique_name, sender, length + 1U);
    sd_id128_t bus_id;
    result = sd_bus_get_bus_id(bus, &bus_id);
    if (result < 0) return result;
    memcpy(temporary.bus_id, bus_id.bytes, sizeof temporary.bus_id);

    const uint64_t required = SD_BUS_CREDS_EUID | SD_BUS_CREDS_PID |
                              SD_BUS_CREDS_PIDFD | SD_BUS_CREDS_SELINUX_CONTEXT;
    sd_bus_creds *credentials = NULL;
    /* Deliberately omit SD_BUS_CREDS_AUGMENT. Missing ProcessFD must fail. */
    result = sd_bus_query_sender_creds(message, required, &credentials);
    if (result < 0) return result;
    if ((sd_bus_creds_get_mask(credentials) & required) != required ||
        (sd_bus_creds_get_augmented_mask(credentials) & required) != 0) {
        result = -ENODATA;
        goto done;
    }
    result = sd_bus_creds_get_euid(credentials, &temporary.connection_uid);
    if (result < 0) goto done;
    result = sd_bus_creds_get_pid(credentials, &temporary.connection_pid);
    if (result < 0) goto done;
    const char *label = NULL;
    result = sd_bus_creds_get_selinux_context(credentials, &label);
    if (result < 0) goto done;
    length = strnlen(label, sizeof temporary.connection_label);
    if (length == 0 || length == sizeof temporary.connection_label) {
        result = -E2BIG;
        goto done;
    }
    memcpy(temporary.connection_label, label, length + 1U);
    result = sd_bus_creds_get_pidfd_dup(credentials, &temporary.pidfd);
    if (result < 0) goto done;
    result = ae_peer_alive(&temporary);
    if (result < 0) goto done;
    *output = temporary;
    temporary.pidfd = -1;
done:
    sd_bus_creds_unref(credentials);
    ae_peer_clear(&temporary);
    return result;
}

int ae_peer_capture(sd_bus_message *message,struct ae_peer *output)
{ return capture(message,SD_BUS_MESSAGE_METHOD_CALL,output); }

int ae_peer_capture_reply(sd_bus_message *reply,sd_bus_message *request,struct ae_peer *output)
{
    if (reply==NULL || request==NULL || output==NULL || output->pidfd>=0) return -EINVAL;
    uint8_t type=0; int r=sd_bus_message_get_type(request,&type);
    if (r<0) return r;
    if (type!=SD_BUS_MESSAGE_METHOD_CALL || sd_bus_message_get_expect_reply(request)!=1 ||
        sd_bus_message_get_bus(request)==NULL || sd_bus_message_get_bus(request)!=sd_bus_message_get_bus(reply)) return -EPROTO;
    const char *destination=sd_bus_message_get_destination(request),*sender=sd_bus_message_get_sender(reply);
    if (destination==NULL || destination[0]!=':' || !sd_bus_service_name_is_valid(destination) ||
        sender==NULL || strcmp(destination,sender)!=0) return -EPROTO;
    uint64_t call_cookie=0,reply_cookie=0;
    if (sd_bus_message_get_cookie(request,&call_cookie)<0 || sd_bus_message_get_reply_cookie(reply,&reply_cookie)<0 ||
        call_cookie==0 || call_cookie!=reply_cookie) return -EPROTO;
    return capture(reply,SD_BUS_MESSAGE_METHOD_RETURN,output);
}
