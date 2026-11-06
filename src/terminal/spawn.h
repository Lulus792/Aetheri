/* Internal Linux/glibc development PTY creation after UI threads exist. */
#ifndef AETHERI_TERMINAL_SPAWN_H
#define AETHERI_TERMINAL_SPAWN_H
#include "pty.h"
#include <sys/stat.h>
struct ae_terminal_spawner {int helper,source;char **environment;char *bytes;bool welcome;struct stat helper_state,source_state;};
#define AE_TERMINAL_SPAWNER_INIT { .helper=-1,.source=-1 }
/* Prepare once before threads with independently selected held regular ELF
   inputs and a quiescent NULL-terminated environment. Owns copies, not callers'
   FDs/strings. No concurrent prepare/spawn/close, live copying or implicit retry.
   Environment has at most 1023 entries, 16 KiB per string and 256 KiB total;
   WAYLAND_SOCKET/COLORTERM are removed, TERM=dumb fixed. Source contents must
   remain trusted and immutable across prepare/spawn; holding an FD does not
   seal mutable bytes or authenticate their publisher. No foreign child reaper.
   Held metadata is rechecked before/after spawn to reject observed development
   input drift; matching metadata is not immutable-content or provenance proof.
   This is not an environment sandbox, publisher/image admission or Root launch. */
int ae_terminal_spawner_prepare(struct ae_terminal_spawner *,int helper,int source,
    char *const environment[],bool welcome);
/* Single owner thread; may run after other threads exist. Output must be empty
   (master/pin/pid = -1). Uses pidfd_spawn and fixed bootstrap descriptors/argv.
   A return error with pin >= 0 STILL OWNS the original child and PTY: hang up,
   read EOF and observe its exit before close/reuse. Never retry that attempt.
   Successful creation is not target exec/readiness or descendant drain proof. */
int ae_terminal_spawner_open(const struct ae_terminal_spawner *,struct ae_terminal_pty *,unsigned cols,unsigned rows);
/* Releases only prepared inputs; never signals/reaps an existing child. */
void ae_terminal_spawner_close(struct ae_terminal_spawner *);
#endif
