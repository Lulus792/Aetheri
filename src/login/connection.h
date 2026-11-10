#ifndef AETHERI_LOGIN_CONNECTION_H
#define AETHERI_LOGIN_CONNECTION_H
#include "login/protocol.h"
#include <sys/types.h>

struct ae_login_connection;
struct ae_login_connection_source {
    /* Independently selected, live original daemon pin, never pidfd_open of a
       PID learned from the socket. Borrowed only during construction. */
    int peer_pidfd;
    uid_t uid;
    gid_t gid;
    /* Mandatory compiled admission check of the original daemon/config/greeter
       context. Caller owns userdata until close. Zero admits this observation;
       negative error permanently fences. Must not log secrets, perform socket
       I/O, change/close handles, fork or reenter. This library supplies no image,
       service/policy/greeter admission merely from matching credentials. */
    int (*check)(void *userdata);
    void *userdata;
};
/* Native Linux, trusted same PID namespace/pidfs. One creating process/thread.
   Fresh connected AF_UNIX SOCK_STREAM only, with exclusive stream ownership and
   no retained aliases/concurrent users. On success moves *socket_fd to -1 and
   owns CLOEXEC socket/kernel SO_PEERPIDFD plus a fresh conversation. On failure
   output stays NULL and caller socket unchanged. No connect/path/environment/
   numeric-PID fallback, PAM or process control. Current peer credentials and
   mandatory admission are rechecked; neither is a lock against future drift. */
int ae_login_connection_create(struct ae_login_connection **out,int *socket_fd,
                               const struct ae_login_connection_source *);
/* Close only local memory/descriptors, never automatic cancel/reconnect/stop.
   Callback-time close is EBUSY. A fork child may only close its local copy. */
int ae_login_connection_close(struct ae_login_connection **);
int ae_login_connection_begin(struct ae_login_connection *,const char *,size_t);
int ae_login_connection_answer(struct ae_login_connection *,const char *,size_t);
int ae_login_connection_cancel(struct ae_login_connection *);
int ae_login_connection_start(struct ae_login_connection *);
/* Read-only local snapshot, including after a fence; borrowed text rules from
   protocol.h apply. Status is not a fresh backend admission check. */
int ae_login_connection_status(struct ae_login_connection *,struct ae_login_status *);
/* Borrow descriptors for poll only. events is POLLIN plus POLLOUT while sending;
   separately watch peer_pidfd for POLLIN. Caller owns deadlines/event loop and
   must call pump on any readiness/loss hint, or interrupt on its own timeout.
   No timer, thread, daemon, blocking wait or internal retry loop is introduced. */
int ae_login_connection_watch(struct ae_login_connection *,int *socket_fd,
                              short *events,int *peer_pidfd);
/* One bounded recvmsg (4096 bytes) and at most one nonblocking send per call.
   Receive precedes send, so queued unsolicited/early replies fence before new
   bytes leave. Returns 1 on accepted reply, 0 progress/would-block/EINTR, negative
   error on a permanent fence. All ancillary input rejects; delivered FDs close.
   MSG_NOSIGNAL, immediate sent-prefix wiping, no automatic answers or replay.
   EOF is unknown, not session failure/drain. A prior start ACK stays historical. */
int ae_login_connection_pump(struct ae_login_connection *);
int ae_login_connection_interrupt(struct ae_login_connection *);
#endif
