#ifndef AETHERI_CORE_STORAGE_H
#define AETHERI_CORE_STORAGE_H

#include "core/owner.h"
#include <sys/types.h>

/* Borrow root_fd; caller supplies a held, protected, verified root and identity.
 * Return a newly owned read-only regular-file FD or negative errno. No fallback
 * to weaker path resolution if openat2 or required flags are unavailable.
 * The trusted root must contain only managed regular fixtures/directories, never
 * device nodes: type checks follow open and are not a privileged file importer.
 * This does not implement SELinux policy, volume registration or durable purge.
 */
int ae_open_small_file(int root_fd, const char *relative, size_t length, size_t limit);
/* Read a protected owner header through root_fd. Return 0 or negative errno.
 * DAC checks complement, and cannot replace, the required same-UID MAC policy.
 */
int ae_check_owner_header(int root_fd, uid_t expected_uid, const struct ae_owner *expected);

#endif
