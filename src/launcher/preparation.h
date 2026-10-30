#ifndef AETHERI_LAUNCHER_PREPARATION_H
#define AETHERI_LAUNCHER_PREPARATION_H
#include "authority/execution_profile.h"

#define AE_STARTER_ROOT_UNIT "aetheri-p0-execution-root.service"
/* The fixed EXEC fixture only; no category/UID/environment selection by apps. */
#define AE_STARTER_TARGET_CONTEXT "system_u:system_r:aetheri_app_t:s0:c35,c36"
/* Fresh single-threaded UID/GID-0 starter, enforcing/NNP and exact five setup/
 * credential-drop capabilities. Drops allowed inheritable/ambient bits only.
 * No domain transition, capability addition, profile admission or process proof. */
int ae_starter_context(void);
/* Compare the actual current ELF to the independently protected starter object. */
int ae_starter_self(const struct ae_execution_profile *);

struct ae_starter_views;
struct ae_starter_target {
    int image_fd,data_fd; /* borrowed CLOEXEC handles; keep views alive */
    struct ae_image image;
    struct ae_fixture_command command;
};
/* Only after original Root registration/journal admission. Enter a private
 * namespace, reopen verified sources there and attach independently witnessed
 * scaffold objects selected by the protected AETHVWS1 record. No path/config
 * supplied by an app. Output initially NULL, unchanged on failure. A failure
 * may already have changed the namespace/attached mounts: EXIT, never retry or
 * fall back. Caller owes exclusion of metadata/content/topology mutation.
 * Does not admit current broker/data, arm SQL, execute, or prove readiness. */
int ae_starter_views_open(const struct ae_execution_profile *,struct ae_starter_views **);
/* Recheck exact held/named views and image; no app transport or lease change. */
int ae_starter_views_target(struct ae_starter_views *,struct ae_starter_target *);
/* Local handles only. Attached mounts remain until actual namespace drain.
 * Caller must exit after closing; never reuse this preparer for another run. */
int ae_starter_views_close(struct ae_starter_views **);
#endif
