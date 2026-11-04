#ifndef AETHERI_CATALOGUE_INTERRUPTION_H
#define AETHERI_CATALOGUE_INTERRUPTION_H
#include "catalogue/lifetime.h"
enum { AE_INTERRUPTION_BINDING_BYTES=248,AE_INTERRUPTION_RECEIPT_BYTES=352 };
enum ae_interruption_kind { AE_INTERRUPTION_BEGIN=1,AE_INTERRUPTION_QUIET,AE_INTERRUPTION_RELEASE,AE_INTERRUPTION_EXIT };
struct ae_interruption_binding {
    uint8_t boot[16],root_runtime[16],user_runtime[16];
    uint8_t continuity_digest[32],fence_digest[32],boot_context_digest[32];
    uint8_t root_context_digest[32],user_context_digest[32];
};
struct ae_interruption_receipt {
    enum ae_interruption_kind kind;
    struct ae_interruption_binding binding;
    uint8_t invocation[16],previous_digest[32],context_digest[32];
    int64_t revision,unix_ms;
};
/* AETHINT1 is bounded metadata, NOT machine/volume/anti-clone provenance,
 * process end, actor admission or holder/future-effect fencing. Caller verifies
 * all those facts independently, authenticates/rechecks both CURRENT owners and
 * confirms the exact binding at the counterpart. Boot must differ from original;
 * both owner runtimes are new. AETHBOT1 alone cannot justify this admission.
 * All codecs preserve outputs on errors; receipt kinds are distinct from v3. */
int ae_interruption_binding_encode(const struct ae_execution_profile *,const struct ae_interruption_binding *,uint8_t [AE_INTERRUPTION_BINDING_BYTES]);
int ae_interruption_binding_decode(const struct ae_execution_profile *,const uint8_t *,size_t,struct ae_interruption_binding *);
int ae_interruption_receipt_encode(const struct ae_execution_profile *,const struct ae_interruption_receipt *,uint8_t [AE_INTERRUPTION_RECEIPT_BYTES]);
int ae_interruption_receipt_decode(const struct ae_execution_profile *,const uint8_t *,size_t,struct ae_interruption_receipt *);
int ae_interruption_receipt_digest(const struct ae_execution_profile *,const struct ae_interruption_receipt *,uint8_t [32]);
/* Explicit v8 owner transactions for already successfully started original runs.
 * Original profiles/process/start results and v3/v6/v7 receipts remain immutable.
 * No process/IPC effect; no original handles are reconstructed. Normal create is
 * still v2 and open never migrates. Independent provenance and fresh admission
 * precede EVERY new owner pair, including another restart during interruption.
 * Root requires current authenticated context; User requires context==NULL.
 * Fresh enrollment/stages require closed gates; User also requires zero grants/
 * accesses. Unknown commits preserve outputs; exact retry cannot replay effects.
 * Successful-start history is mandatory: early/unfinished starts are separate.
 * There is no installed wire, boot coordinator or production admission here. */
int ae_catalogue_enroll_interruption(struct ae_catalogue *,const struct ae_execution_profile *,const struct ae_launch_context *,
    const struct ae_execution_evidence *,const struct ae_interruption_binding *,int64_t);
/* BEGIN: Root, started exact original completion, prior NULL. QUIET: User, prior
 * authenticated Root BEGIN. RELEASE: Root, prior authenticated User QUIET plus
 * current independently rechecked end/all-holder/future-effect proof. EXIT: User,
 * prior authenticated Root RELEASE. started must be NULL for other stages.
 * Historical receipts correlate only; they are never fresh quiescence/end proof.
 * Further owner/boot replacement enrolls a fresh binding, retaining prior stages.
 * Real-time stamps are observations, never cross-boot chronology. No purge. */
int ae_catalogue_interrupt_run(struct ae_catalogue *,const struct ae_execution_profile *,const struct ae_launch_context *,
    const struct ae_execution_evidence *,const struct ae_interruption_binding *,enum ae_interruption_kind,
    const struct ae_interruption_receipt *prior,const struct ae_execution_completion *started,int64_t,struct ae_interruption_receipt *);
#endif
