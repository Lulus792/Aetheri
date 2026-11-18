#ifndef AETHERI_LOGIN_UNLOCK_AUTHORITY_H
#define AETHERI_LOGIN_UNLOCK_AUTHORITY_H

#include "unlock_channel.h"

struct ae_unlock_authority;
struct ae_unlock_authority_client;

/* Create this private pair in the privileged PAM session owner before forking
   its authenticated desktop child. The root end is pair[0]. */
int ae_unlock_authority_pair(int pair[2]);
/* The caller is root and has just opened the original PAM session. Its direct
   desktop child may still be dropping credentials. The account, UID and GID
   come from that session, never from a request. Takes fd on success. */
int ae_unlock_authority_open(struct ae_unlock_authority **, int fd, int child_pidfd,
    uid_t uid, gid_t gid, const char *account);
enum { AE_UNLOCK_AUTHORITY_FDS = 3 };
/* Borrow the socket, child and verifier poll hints and relative timeout. */
void ae_unlock_authority_poll(const struct ae_unlock_authority *,
    struct pollfd [AE_UNLOCK_AUTHORITY_FDS], int *timeout);
/* Nonblocking I/O. Negative means the original connection is fenced. */
int ae_unlock_authority_step(struct ae_unlock_authority *);
/* Close the connection and retire the exact verifier before dropping PAM. */
void ae_unlock_authority_close(struct ae_unlock_authority **);

/* Admit the inherited end while the caller is still the ordinary direct
   desktop child of the original root PAM owner. Takes fd on success. */
int ae_unlock_authority_client_open(struct ae_unlock_authority_client **, int fd);
void ae_unlock_authority_client_poll(const struct ae_unlock_authority_client *, struct pollfd [2]);
int ae_unlock_authority_client_begin(struct ae_unlock_authority_client *,
    uint64_t generation, const void *password, size_t size);
/* EAGAIN means pending or idle. A success delivers one current result. */
int ae_unlock_authority_client_receive(struct ae_unlock_authority_client *,
    uint64_t generation, enum ae_unlock_result *);
void ae_unlock_authority_client_close(struct ae_unlock_authority_client **);

#endif
