#ifndef AETHERI_CORE_ROOT_H
#define AETHERI_CORE_ROOT_H

#include <stdint.h>

/* Kernel observations of a held directory, local to this boot/mount namespace.
 * Not a persistent volume identity, serialized record, or authenticity proof.
 */
struct ae_root_identity {
    uint64_t mount_id, inode;
    uint32_t device_major, device_minor, uid, gid, mode;
    int64_t filesystem_type;
    uint8_t filesystem_id[8];
    int birth_present;
    int64_t birth_seconds;
    uint32_t birth_nanoseconds;
};

/* Borrow fd. Require statx's non-recycled STATX_MNT_ID_UNIQUE and all metadata
 * fields, with repeated observations. Birth time is reported when available;
 * allocation records require it. No legacy mount-ID/device-only fallback.
 * Outputs stay unchanged on failure; negative errno. */
int ae_root_observe(int fd, struct ae_root_identity *output);
int ae_root_equal(const struct ae_root_identity *left, const struct ae_root_identity *right);
/* Reopen an authority-selected absolute protected path without symlinks and
 * require both its named directory and the retained fd to match expected.
 * Parents/mount topology must be controlled by the caller. This is a current
 * observation, not exclusion against a privileged concurrent namespace writer.
 */
int ae_root_check_path(int fd, const char *absolute, const struct ae_root_identity *expected);

#endif
