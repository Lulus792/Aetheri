#ifndef AETHERI_CORE_PUBLISH_H
#define AETHERI_CORE_PUBLISH_H

#include <stddef.h>
#include <stdint.h>

/* Internal journal-bound publication on one Linux mount. Borrow all descriptors;
 * caller independently authenticates/pins payload/container/parents, excludes all
 * namespace/content writers through MAC and the owner lock, and commits executing
 * intent before entry. No client-selected roots or root adoption is performed.
 * The preallocated container initially contains nothing. The payload is either
 * still staging_name beneath staging_parent, or already container/payload. The
 * container is either container_name below container_parent, or already the
 * derived 64-hex ArtifactId beneath store_parent. Both alternatives at once,
 * unknown existing targets, links, mount transitions and unexpected entries fail.
 *
 * A create-only immutable manifest is written/flushed; the same payload inode
 * moves inside the container; RENAME_NOREPLACE publishes the whole container.
 * No copy/overwrite/weaker rename fallback exists. Every successful call, including
 * recovery of an already visible target, revalidates data/identity and repeats
 * all required file/directory/old-parent/new-parent flushes. Errors retain all
 * visible effects. No deletion, chmod, relabel, activation, lease or SQL update.
 * Caller must seal/validate MAC/modes and repeat this check after those changes
 * before committing publication metadata. Directory modes may be 0700 or 0555.
 * No physical power-loss or registered-volume authentication is implied.
 */
struct ae_fixture_publication {
    int staging_parent, container_parent, store_parent, payload, container;
    const char *staging_name, *container_name;
};
int ae_fixture_publish_namespace(const struct ae_fixture_publication *publication,
                                  const uint8_t *manifest, size_t size,
                                  const uint8_t artifact[32]);

#endif
