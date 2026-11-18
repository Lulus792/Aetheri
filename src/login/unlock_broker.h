#ifndef AETHERI_LOGIN_UNLOCK_BROKER_H
#define AETHERI_LOGIN_UNLOCK_BROKER_H

#include <stdbool.h>
#include <poll.h>

struct ae_unlock_broker;
struct ae_unlock_authority_client;

struct ae_unlock_broker_config {
    int control, credentials;
    int compositor, view; /* Original direct children, as pidfds. */
    const char *worker, *account;
    bool (*session_live)(void *);
    void *session;
    /* Takes ownership on successful open. A null authority selects the
       development verifier launched as the ordinary session user. */
    struct ae_unlock_authority_client *authority;
};

enum { AE_UNLOCK_BROKER_FDS = 6 };

/* Use the same owning thread throughout, with ordinary child reaping enabled.
   On success takes the original socket ends and retains duplicate pidfds.
   The account, worker and session are independently selected by the caller. */
int ae_unlock_broker_open(struct ae_unlock_broker **, const struct ae_unlock_broker_config *);
/* Borrow descriptors for one poll only. Timeout is relative milliseconds. */
void ae_unlock_broker_poll(const struct ae_unlock_broker *, struct pollfd [AE_UNLOCK_BROKER_FDS], int *);
/* Drain bounded nonblocking input, recheck the live session, and apply results.
   A negative return permanently fences the broker. */
int ae_unlock_broker_step(struct ae_unlock_broker *);
bool ae_unlock_broker_open_desktop(const struct ae_unlock_broker *);
/* Discard the current attempt, close both channels and retire the owned worker. */
void ae_unlock_broker_close(struct ae_unlock_broker **);

#endif
