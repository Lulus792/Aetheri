#ifndef AETHERI_AUDIO_ACTOR_H
#define AETHERI_AUDIO_ACTOR_H

#include "audio/manager.h"

struct ae_audio_actor;
/* Read-only observer of one original USER service main, not a launch/grant API.
 * Caller independently admits manager, literal unit, original live pidfd,
 * root-owned non-writable expected ELF and exact expected MAC context. Never
 * select these from app/socket-supplied claims. Protected unit/config/library
 * provenance, supplementary-group policy and exec/delegation/mutation exclusion
 * remain caller obligations.
 * Require active/no-job/no-control state, original invocation, current ordinary
 * UID/GID/cap-zero/NNP context, exact image and held cgroup object, all joined to
 * the retained manager's unique destination and delegated cgroup subtree.
 * Trusted host proc/cgroup/bus views and accepted pidfs ABI are prerequisites.
 * Borrow input FDs synchronously; retain manager and own duplicate process/
 * cgroup handles. Same calling thread/process as manager throughout. No service
 * mutation, signal, reconnect, numeric-PID opening or new background process.
 * Returns NULL with errno on failure. Close manager or caller FDs afterward
 * without invalidating this observer's independently retained handles. */
struct ae_audio_actor *ae_audio_actor_open(struct ae_audio_manager *,
    const char *unit,int original_pidfd,int expected_image_fd,const char *expected_context);
/* Zero while the original observations still agree. First failure is latched;
 * replacement/restart cannot restore it. A successful snapshot is not a lock
 * against subsequent changes, target readiness, endpoint or device permission.
 * Loss requires caller fencing/stop/holder coordination, never inferred drain. */
int ae_audio_actor_check(struct ae_audio_actor *);
/* Retire only owned handles/references; no StopUnit/UnrefUnit or lease release. */
void ae_audio_actor_close(struct ae_audio_actor **);

#endif
