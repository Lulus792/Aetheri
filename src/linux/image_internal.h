#ifndef AETHERI_LINUX_IMAGE_INTERNAL_H
#define AETHERI_LINUX_IMAGE_INTERNAL_H
#include "core/image.h"
#include <sys/types.h>

/* Low-level observation for compiled Linux adapters that already authenticate
   the borrowed kernel pidfd and independently admit this exact current context.
   Do not expose as an interface accepting a caller PID or process descriptor. */
struct ae_image_process {
    int pidfd;
    pid_t pid;
    uid_t uid;
    gid_t gid;
    const char *label;
};
int ae_image_match_pinned(const struct ae_image_process *,const struct ae_image *expected,
                          struct ae_image *out);
#endif
