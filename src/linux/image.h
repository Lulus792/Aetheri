#ifndef AETHERI_LINUX_IMAGE_H
#define AETHERI_LINUX_IMAGE_H
#include "linux/peer.h"

#include "core/image.h"

int ae_image_verify(int fd,const uint8_t expected_digest[32],struct ae_image *output);
/* Current executable observation of an already authenticated/owned held
 * ProcessFD. No pidfd_open from a supplied PID. Trusted host proc/PID namespace,
 * immutable backing files and separate unit/cgroup/run admission are required.
 * Same-UID/MAC protections must exclude hostile ptrace/credential/exec changes.
 * A bounded observation is not a guarantee of future liveness or readiness.
 * Mount IDs may differ across the admitted private code view; backing object,
 * birth/metadata/content must agree. Outputs unchanged on all errors. */
int ae_image_match_process(const struct ae_peer *,gid_t expected_gid,
    const struct ae_image *expected,struct ae_image *output);
/* Current CALLING process only, including capability-empty UID/GID 0. Pin self
 * with pidfd_open(getpid()), require all real/effective/saved/filesystem IDs,
 * then join the current proc executable to an independently selected immutable
 * image with the same repeated object/content/label checks as the peer path.
 * Never accepts a caller PID/connection or creates remote Root-image admission.
 * Trusted single-threaded host setup, proc/PID scope and mutation/exec/ptrace
 * exclusion remain prerequisites.
 * This observes one ELF, not loaded libraries, service role, future execution,
 * volume continuity or recovery authority. Output unchanged on every failure. */
int ae_image_match_self(uid_t expected_uid,gid_t expected_gid,
    const struct ae_image *expected,struct ae_image *output);
#endif
