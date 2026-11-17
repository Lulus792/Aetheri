#ifndef AETHERI_LOGIN_SOURCES_H
#define AETHERI_LOGIN_SOURCES_H
#include "core/image.h"
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

enum { AE_LOGIN_SOURCES_MAX=10, AE_LOGIN_SOURCE_PATH_MAX=1024,
       AE_LOGIN_SOURCE_LABEL_MAX=256, AE_LOGIN_SOURCE_DATA_MAX=65536,
       AE_LOGIN_SOURCE_FULL_PATH_MAX=1280 };
enum ae_login_source_kind { AE_LOGIN_SOURCE_DATA=1, AE_LOGIN_SOURCE_ELF=2, AE_LOGIN_SOURCE_ASSET=3 };
struct ae_login_source_expect {
    /* Independently admitted canonical absolute parent and literal leaf. Never
       construct this selection/digest/label from the candidate's own metadata. */
    const char *directory;
    const char *leaf;
    const char *label; /* Exact raw SELinux FILE label, not a process label. */
    enum ae_login_source_kind kind;
    mode_t mode; /* ELF:0555; asset:0444; data:0444 or0644. Always UID0/GID0, nlink1. */
    uint64_t size; /* Exact nonzero size; ELF and asset <=16MiB, data <=64KiB. */
    uint8_t digest[32];
};
struct ae_login_sources;
/* Read-only source observation in a trusted host mount namespace, enforcing
   SELinux. Copy bounded expectations, retain original parents/files, verify
   every ancestor is Root-owned and not group/other writable or special-mode.
   Canonical paths only, depth <=16, no symlinks, no mount crossing at the leaf.
   Bind unique mount/inode/birth/metadata, labels, capability absence and SHA256;
   ELF uses the existing x86-64 image verifier. All entries are checked again
   before success. Output starts NULL and stays unchanged on every error.
   Caller excludes privileged filesystem/mount/metadata manipulation and owns
   independent deployment/policy admission. No files/services are changed. */
int ae_login_sources_create(const struct ae_login_source_expect *,size_t count,
                            struct ae_login_sources **out);
/* For composition into connection_source.check alongside service/current-image,
   loaded configuration/policy and original greeter admission. Metadata, held /
   named objects and exact labels are rechecked; content was hashed at capture.
   First observed loss permanently fences this selection, even if restored.
   Same original process/thread, no reconnect/rebind or automatic recreation. */
int ae_login_sources_check(void *sources);
/* Copy an ELF's captured observation only after checking the complete selection.
   No exported FD, execution/readiness/current-process proof or loaded-DSO proof.
   Output unchanged on failure, including DATA entries and invalid indices. */
int ae_login_sources_image(struct ae_login_sources *,size_t index,struct ae_image *);
/* Copy one exact selected absolute path, requiring the stated kind and a
   current complete source check. On failure leave output unchanged. */
int ae_login_sources_path(struct ae_login_sources *,size_t index,enum ae_login_source_kind,
                          char out[AE_LOGIN_SOURCE_FULL_PATH_MAX]);
/* Copy the independently expected digest of a currently verified DATA source. */
int ae_login_sources_data_digest(struct ae_login_sources *,size_t index,uint8_t out[32]);
/* Read and hash an ASSET from its original held file. Return an owned buffer
   only after all original source checks still pass. *out starts NULL and
   both outputs are unchanged on failure. Caller frees the successful copy. */
int ae_login_sources_read_asset(struct ae_login_sources *,size_t index,void **out,size_t *size);
/* Hash the ordered original selection, including exact expected paths/labels/
   digests and boot-local file/ancestor identities. Recheck before returning.
   This fingerprints a held observation, with no durable volume identity. */
int ae_login_sources_fingerprint(struct ae_login_sources *,uint8_t out[32]);
/* Local FD/memory cleanup only. A fork child may close its local copy. */
int ae_login_sources_close(struct ae_login_sources **);
#endif
