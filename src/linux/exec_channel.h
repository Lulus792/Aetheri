#ifndef AETHERI_LINUX_EXEC_CHANNEL_H
#define AETHERI_LINUX_EXEC_CHANNEL_H
#include "linux/execute.h"

enum ae_exec_channel_state {
    AE_EXEC_CHANNEL_PENDING=0, AE_EXEC_CHANNEL_UNKNOWN,
    AE_EXEC_CHANNEL_FAILURE, AE_EXEC_CHANNEL_MALFORMED
};
struct ae_exec_report {
    enum ae_exec_channel_state state;
    enum ae_exec_phase phase; /* Zero unless FAILURE. */
    int error;               /* Positive Linux errno, only for FAILURE. */
};
struct ae_exec_channel;
/* Single-threaded issuer owns the reader. No imported/app-selected reader.
 * Output channel must be NULL. Success returns one NONBLOCK/CLOEXEC writer,
 * owned by the caller. Bind the channel to the admitted journal/held starter
 * BEFORE exporting only that writer. Close every issuer/transport writer copy
 * after transfer; the trusted starter keeps the sole writer until exec/exit.
 * Creation is not registration, admission or an authenticated transfer itself.
 * Both outputs unchanged on error. Standard FDs 0..2 must already be occupied. */
int ae_exec_channel_new(struct ae_exec_channel **output,int *writer);
/* Borrow for event-loop polling ONLY. Do not read/close/duplicate/change flags
 * or export this reader. Each live channel belongs to its creating process. */
int ae_exec_channel_fd(const struct ae_exec_channel *);
/* At most two nonblocking reads and 17 bytes per call, including overflow byte.
 * A complete frame stays PENDING until EOF; trailing/partial/invalid bytes are
 * MALFORMED. Empty EOF is UNKNOWN, never exec success/readiness/drain/release.
 * Terminal reports are retained; remove their event watch (no further reads).
 * I/O errors preserve output and consumed prefix;
 * no unbounded drain/retry or replacement channel is performed. */
int ae_exec_channel_read(struct ae_exec_channel *,struct ae_exec_report *);
int ae_exec_channel_close(struct ae_exec_channel **);
/* Check a borrowed writer from an independently authenticated issuer reply:
 * anonymous pipe, WRONLY/NONBLOCK/CLOEXEC, expected creator UID/GID and 0600.
 * This does not authenticate the reply, prove exclusive ownership, tie a run
 * or adopt the FD. A message-owned FD must be duplicated before message unref. */
int ae_exec_writer_check(int fd,uid_t issuer_uid,gid_t issuer_gid);
#endif
