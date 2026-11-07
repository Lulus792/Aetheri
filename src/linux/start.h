#ifndef AETHERI_LINUX_START_H
#define AETHERI_LINUX_START_H
#include "linux/peer.h"

struct ae_start;
enum { AE_START_JOB_PATH=96,AE_START_RESULT=32 };
struct ae_start_status {
    int used,attempted,pending,reply_confirmed,job_removed,quarantined,error;
    uint32_t job_id;
    uint64_t cookie;
    char job[AE_START_JOB_PATH],result[AE_START_RESULT];
    unsigned int buffered;
};
typedef int (*ae_start_guard)(void *);
typedef void (*ae_start_changed)(void *,const struct ae_start_status *);
/* Root-owned original StartUnit request, not a unit installer or app admission.
 * Independently admit the trusted HOST bus/PID1 pin, current boot, immutable unit
 * configuration/dependency graph and exclusive original unit allocation. Caller
 * holds its OWN unit reference and Subscribe for the entire observation, on a
 * dedicated connection; do not share reference/subscription ownership implicitly.
 * The caller drives that bus through its event loop. No worker/polling thread.
 * This object owns only local references, signal watches and the pending call.
 * Signals are authenticated under the ORIGINAL PID1 connection before use.
 * Output initially NULL. Single-threaded, process-owned; callback close EBUSY. */
int ae_start_new(sd_bus *,const struct ae_peer *admitted_pid1,const uint8_t boot[16],const char *admitted_unit,
    ae_start_changed,void *,struct ae_start **);
/* One entry only, including pre-send errors. The COMPILED guard rechecks current
 * owner/source/unit admission immediately before the sole async StartUnit/fail.
 * Caller journals fresh intent BEFORE entry. Once attempted, any send/error/
 * timeout stays an original obligation; never retry or infer absence from it.
 * JobRemoved can precede the reply: retain a bounded exact-unit event set and
 * bind only the authenticated reply's exact job path. Never adopt by unit/name.
 * Terminal job observation retires both signal watches; later Manager changes
 * do not rewrite that original historical result. Quarantine never auto-settles.
 * Queued work can start after fencing. A job result is not app exec/readiness,
 * process/unit/holder drain or lease-release evidence. No Stop/Cancel/Unref here. */
int ae_start_request(struct ae_start *,ae_start_guard,void *);
int ae_start_status(struct ae_start *,struct ae_start_status *);
/* Ordinary close refuses an unsettled attempted call/job. Abandon explicitly
 * retires LOCAL observation only; it cannot cancel a queued request/job or settle
 * the caller's journal, retained unit/transport/holder obligations. */
int ae_start_close(struct ae_start **);
int ae_start_abandon(struct ae_start **);
#endif
