#ifndef AETHERI_COMPOSITOR_LOCK_H
#define AETHERI_COMPOSITOR_LOCK_H

#include <stdbool.h>

struct ae_lock;
struct output;
struct server;
struct wlr_output_state;

/* An inherited, unnamed seqpacket channel. Takes fd only on success.
   The reserved channel accepts one cover request. Disconnect cannot uncover. */
struct ae_lock *ae_lock_create(struct server *server, int fd);
void ae_lock_destroy(struct ae_lock *lock);
bool ae_lock_active(const struct ae_lock *lock);

void ae_lock_add_output(struct output *output);
void ae_lock_remove_output(struct output *output);
bool ae_lock_commit(struct output *output, const struct wlr_output_state *state);

#endif
