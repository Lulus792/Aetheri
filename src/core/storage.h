#ifndef AETHERI_CORE_STORAGE_H
#define AETHERI_CORE_STORAGE_H

#include "core/owner.h"
#include "core/root.h"
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

struct ae_storage_ref;
/* Bind the independently expected allocation owner + current directory identity
 * to its derived 32-hex StorageRef below a held trusted anchor. The supplied
 * identity must come from trusted allocation/reconciliation, never from a client
 * or the very candidate being adopted. Root/header must be account UID, 0700/0600.
 * Caller owes protected parent, registered volume/account and enforcing labels;
 * these primitives neither enroll a volume nor establish same-UID MAC policy.
 * Owns duplicate anchor/root FDs on success. Outputs stay unchanged on failure.
 */
int ae_storage_ref_open(int anchor_fd, uid_t metadata_uid, const struct ae_owner *owner,
                        const struct ae_root_identity *expected, struct ae_storage_ref **output);
/* Revalidate held/named root, anchor and owner before/after a bounded open.
 * Return newly owned readonly CLOEXEC file FD, or negative errno. Relative path
 * is below account-owned 0700 payload/ only, with account-owned files;
 * owner/control metadata cannot be reached. Returned FDs
 * require explicit grant/holder tracking: checking/closing a ref cannot revoke
 * a descriptor already returned to a caller. P0 native grant transport must read
 * bounded bytes and close this internal FD, never hand it to an app. Never use
 * these APIs after fork.
 */
int ae_storage_ref_check(struct ae_storage_ref *reference);
int ae_storage_ref_read(struct ae_storage_ref *reference, const char *relative, size_t length, size_t limit);
int ae_storage_ref_close(struct ae_storage_ref **reference);

#endif
