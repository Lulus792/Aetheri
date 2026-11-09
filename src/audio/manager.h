#ifndef AETHERI_AUDIO_MANAGER_H
#define AETHERI_AUDIO_MANAGER_H

#include <systemd/sd-bus.h>
#include <sys/types.h>

struct ae_audio_manager;
/* Read-only observer of the original systemd USER manager. Both bus connections
 * and the root-owned, non-writable expected systemd ELF are independently trusted
 * caller inputs. Host proc/cgroup views, x86-64 pidfs and protected unit/image
 * configuration are prerequisites. This is not MAC/app authorization, loaded
 * library verification, an exec/delegation lock, recovery or holder drain.
 * Borrow image_fd; retain bus references and duplicate the bus-authenticated
 * ProcessFD. No numeric-PID opening, credential augmentation or fallback.
 * The manager must have zero effective/permitted/inheritable/ambient capabilities
 * and NoNewPrivs=1. This observes the selected protected user@ profile; it never
 * changes that process or grants inspection privileges to the caller.
 * Single calling thread/process throughout; buses must have bounded timeouts.
 * Failures return NULL and set errno. Open/check make no service mutations. */
struct ae_audio_manager *ae_audio_manager_open(sd_bus *user_bus,sd_bus *system_bus,
    uid_t expected_uid,int image_fd);
/* Borrow the pinned unique destination, valid only until close. Never select a
 * new owner on check failure. Recheck immediately before a caller's effect;
 * successful observations cannot exclude changes after they return. */
const char *ae_audio_manager_name(const struct ae_audio_manager *);
/* Zero while original bus/owner/process/image/PID1 invocation still agree.
 * First negative result is latched, including observation failure. No restart,
 * reconnect, stop, signal, UnrefUnit or implicit acceptance of a replacement. */
int ae_audio_manager_check(struct ae_audio_manager *);
/* Drop this caller's reference. An audio actor retains the same original
 * observer until its own close; never substitute another manager for it. */
void ae_audio_manager_close(struct ae_audio_manager **);

#endif
