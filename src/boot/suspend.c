#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "suspend.h"
#include "suspend_link.h"
#include "login/unlock_broker.h"

#include <errno.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

enum phase { LISTENING, WAITING, PREPARING, HELD };

struct ae_suspend_server {
    int listener;
    enum phase phase;
    struct ae_suspend_peer peer;
};

int ae_suspend_server_open(struct ae_suspend_server **out) {
    if (!out || *out) return -EINVAL;
    struct ae_suspend_server *server = calloc(1, sizeof *server);
    if (!server) return -ENOMEM;
    server->peer = (struct ae_suspend_peer)AE_SUSPEND_PEER_INIT;
    int fd = ae_suspend_listen();
    if (fd < 0) {
        free(server);
        return fd;
    }
    server->listener = fd;
    *out = server;
    return 0;
}

void ae_suspend_server_close(struct ae_suspend_server **slot) {
    if (!slot || !*slot) return;
    struct ae_suspend_server *server = *slot;
    *slot = NULL;
    ae_suspend_peer_close(&server->peer);
    close(server->listener);
    free(server);
}

void ae_suspend_server_poll(const struct ae_suspend_server *server, struct pollfd fds[AE_SUSPEND_SERVER_FDS]) {
    int socket = server->phase == LISTENING ? server->listener : server->peer.fd;
    fds[0] = (struct pollfd){.fd = socket, .events = POLLIN | POLLRDHUP};
    fds[1] = (struct pollfd){.fd = server->peer.pin, .events = POLLIN};
}

static void accept_guard(struct ae_suspend_server *server) {
    int fd = accept4(server->listener, NULL, NULL, SOCK_NONBLOCK | SOCK_CLOEXEC);
    if (fd < 0) return;
    if (ae_suspend_peer_open(&server->peer, fd, 0, AE_SUSPEND_GUARD_LABEL)) {
        close(fd);
        return;
    }
    if (ae_suspend_send(&server->peer, AE_SUSPEND_HELLO)) ae_suspend_peer_close(&server->peer);
    else server->phase = WAITING;
}

void ae_suspend_server_step(struct ae_suspend_server *server, struct ae_unlock_broker *broker) {
    if (!server || !broker) return;
    if (server->phase == LISTENING) {
        accept_guard(server);
        return;
    }
    if (server->phase == WAITING) {
        int result = ae_suspend_receive(&server->peer, AE_SUSPEND_PREPARE);
        if (result == -EAGAIN) return;
        if (!result && !ae_unlock_broker_prepare_sleep(broker)) {
            server->phase = PREPARING;
        } else {
            ae_suspend_peer_close(&server->peer);
            server->phase = LISTENING;
        }
        return;
    }

    if (server->peer.fd >= 0) {
        /* The guard sends one request. Unexpected traffic and disconnect both
           abandon the original transaction. */
        struct pollfd socket = {.fd = server->peer.fd, .events = POLLIN | POLLRDHUP};
        if (poll(&socket, 1, 0) != 0 || !ae_suspend_peer_live(&server->peer))
            ae_suspend_peer_close(&server->peer);
    }
    if (!ae_unlock_broker_sleep_ready(broker)) return;
    if (server->phase == PREPARING && server->peer.fd >= 0 && !ae_suspend_send(&server->peer, AE_SUSPEND_READY)) {
        server->phase = HELD;
    } else if (server->peer.fd < 0 || server->phase == PREPARING) {
        ae_suspend_peer_close(&server->peer);
        (void)ae_unlock_broker_finish_sleep(broker);
        server->phase = LISTENING;
    }
}
