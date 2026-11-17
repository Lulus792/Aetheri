#ifndef AETHERI_LOGIN_CLIENT_H
#define AETHERI_LOGIN_CLIENT_H

#include "login/connection.h"
#include "login/greeter.h"

struct ae_login_client;

struct ae_login_client_expect {
    struct ae_login_service_expect service;
    struct ae_login_greeter_expect greeter;
    const struct ae_login_source_expect *sources;
    size_t source_count, fragment, program, config;
    const size_t *dropins;
    size_t dropin_count;
    const char *directory, *leaf, *label;
    /* Independently admit the effective host policy, protected directories,
       runtime and actual display/input placement. Called before observation,
       while waiting and before each transport action. No I/O to the login
       socket, reentry, process creation or mutation of the observed inputs. */
    int (*check)(void *);
    /* If set, the peer must present a version-2 handoff for the observed
       enforcing kernel policy status sequence. No version fallback. */
    int (*policy_sequence)(void *,uint32_t *);
    void *userdata;
};

/* Observe the original connected stream and this ordinary greeter before
   exposing any conversation. Move *socket_fd to -1 on success. On failure
   the caller retains the stream and must close it without protocol traffic.
   Borrow the immutable expectation/gate context until close. The trusted bus
   already has a bounded method timeout. Use the process leader's thread. */
int ae_login_client_create(int *socket_fd, sd_bus *trusted_bus,
    const struct ae_login_client_expect *, struct ae_login_client **out);

/* Advance the single initial admission. EAGAIN means only that the selected
   name has not yet appeared in the held original directory. The host owns
   one initial deadline. Other failures fence, with no reconnect or replay.
   Success borrows the connection until client_close. The borrower must not
   close it and must release its view/watchers before closing the client. */
int ae_login_client_admit(struct ae_login_client *, struct ae_login_connection **out);

/* Release local observations and descriptors only. No cancellation, process
   control or modification of a published record. */
int ae_login_client_close(struct ae_login_client **);

#endif
