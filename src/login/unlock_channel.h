#ifndef AETHERI_LOGIN_UNLOCK_CHANNEL_H
#define AETHERI_LOGIN_UNLOCK_CHANNEL_H

#include <poll.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

enum ae_unlock_result {
    AE_UNLOCK_ACCEPTED = 1,
    AE_UNLOCK_DENIED,
    AE_UNLOCK_UNAVAILABLE,
};
struct ae_unlock_attempt;

/* Create the two original endpoints before spawning. The worker receives
   pair[1]. Keep the owner's end at pair[0] and close the worker end after
   spawning. Standard descriptors must already be occupied. */
int ae_unlock_worker_pair(int pair[2]);

/* Use one new inherited unnamed seqpacket pair per attempt. The caller has
   already admitted the worker binary and selects the original covered lock
   generation. The pidfd must identify the caller's still-live direct child.
   On success the attempt owns fd and a pidfd duplicate. On failure the caller
   retires both channel ends and owns its original handles, and *out is NULL.
   Never retry the same channel or password. The caller owns its secret bytes. */
int ae_unlock_attempt_begin(struct ae_unlock_attempt **out, int fd, int child_pidfd,
    uint64_t lock_generation, const void *password, size_t size);
/* Borrow only for polling on the creating thread. Never read, reap or transfer
   these descriptors. Slot order is channel, child. The channel is -1 once EOF
   has been observed. The caller observes the original session and deadline. */
int ae_unlock_attempt_poll(const struct ae_unlock_attempt *, struct pollfd sources[2]);
/* Pass the current covered generation (zero on loss). A result is delivered
   once, after one matching packet from the original child, EOF and its normal
   zero exit. -EAGAIN means pending, any other error besides caller misuse
   retires the attempt. The result stays unchanged on error. */
int ae_unlock_attempt_receive(struct ae_unlock_attempt *, uint64_t current_generation,
    enum ae_unlock_result *out);
/* Close local observations. The caller still owns worker signal/reap and must
   recheck its original session, lock and peer before any display effect. */
void ae_unlock_attempt_close(struct ae_unlock_attempt **);

/* Run by a finite ordinary-user worker inheriting its parent's end of the
   private channel. Account selection comes from the launch, never the packet.
   The caller immediately exits with status zero only on a transmitted result. */
int ae_unlock_worker_once(int fd, const char *account);

#endif
