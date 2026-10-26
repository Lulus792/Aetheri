#ifndef AETHERI_CORE_IMPORT_H
#define AETHERI_CORE_IMPORT_H

#include <stddef.h>
#include <stdint.h>

/* P0 trusted synthetic payload preparation; not an installation/public RPC.
 * Borrow authority-selected source/staging directory FDs. Caller must authorize
 * the operation and durably bind its identity/intent to these roots before entry,
 * protect their parents/mount namespace and genuine procfs, exclude other
 * writers (including same-UID apps through MAC), and retain immutable manifest/
 * expected ID bytes.
 * Both roots/entries must belong to the effective UID. Source root/directories
 * are 0555; source files have exact manifest modes. Staging root is empty 0700.
 * The authority's umask must preserve owner rwx on new directories (e.g. 0077).
 * Root FDs/seek offsets remain owned by the caller. No source contents, modes,
 * ownership or xattrs are written; normal read-atime updates may occur.
 *
 * Verify the independently trusted ArtifactId and complete source inventory;
 * copy through held regular FDs into exclusive files; make copies/directories
 * read-only; hash the copies and flush files/directories. Staging root stays
 * 0700 and is never published. No payload-supplied xattrs/ACLs/capabilities are
 * copied. Visible xattr names other than the host's security.selinux attribute
 * are rejected. Names hidden from an unprivileged caller cannot be audited here;
 * trusted fixture preparation must exclude those too. Actual SELinux type/
 * category validation is a caller duty, not provided here.
 *
 * Returns 0 only for fully checked/flushed staged payload, otherwise negative
 * errno (-EBADMSG for inventory/content mismatch; -EACCES for owner/mode/xattrs;
 * -EEXIST for nonempty stage). Errors, lost replies and failed flush/close retain
 * all created entries, never overwrite/rollback/adopt them. Caller must reconcile
 * its journal before any retry, publication, catalogue admission or removal.
 * This does not enroll a publisher/volume, resolve providers, execute hooks/code,
 * publish an artifact, or promise physical power-loss durability.
 */
int ae_fixture_stage_payload(int source_fd, int staging_fd,
                              const uint8_t *manifest, size_t manifest_size,
                              const uint8_t expected_artifact[32]);

/* Recheck an independently journal-bound existing 0700 staging payload and
 * repeat every file/directory flush. Files/child directories must already have
 * exact read-only modes. This never adopts an unbound candidate or completes
 * a journal/publication itself. All other trust/lifetime rules above apply.
 */
int ae_fixture_sync_staged_payload(int staging_fd,
                                    const uint8_t *manifest, size_t manifest_size,
                                    const uint8_t expected_artifact[32]);

#endif
