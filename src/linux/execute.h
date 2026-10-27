#ifndef AETHERI_LINUX_EXECUTE_H
#define AETHERI_LINUX_EXECUTE_H
#include "linux/image.h"

enum ae_exec_phase { AE_EXEC_PROFILE=1,AE_EXEC_IMAGE,AE_EXEC_CONTEXT,AE_EXEC_DESCRIPTORS,AE_EXEC_CAPABILITIES,
    AE_EXEC_GROUPS,AE_EXEC_GID,AE_EXEC_UID,AE_EXEC_SIGNALS,AE_EXEC_CALL };
struct ae_execute {
    int image_fd,data_fd,error_fd;
    uid_t uid; gid_t gid;
    const struct ae_image *image;
    const char *label;
    char *const *argv,*const *envp;
};
/* Internal one-way boundary for a fresh single-threaded trusted launcher. Caller
 * already journaled dispatch, admitted immutable image/data views and selected
 * account/context/argv/environment/stdio. No paths or policy supplied by an app.
 * No shell/search/interpreter fallback. The global filesystem/loader remains a
 * trusted caller prerequisite; this is not a complete app sandbox or launcher.
 * Failure may follow irreversible credential/FD changes: always exits 125.
 * Error pipe must be an anonymous pipe created by UID/GID 0, with one
 * nonblocking CLOEXEC writer owned only by this process after transfer.
 * AETHXER1 + phase u32 + positive errno u32, big endian, at most one 16-byte write.
 * EOF/no record is UNKNOWN, never exec success/readiness or permission to release.
 * External supervisor/process/image/drain evidence remains mandatory. */
_Noreturn void ae_execute_target(const struct ae_execute *);
#endif
