#ifndef AETHERI_LINUX_VIEW_H
#define AETHERI_LINUX_VIEW_H

#include "core/root.h"

struct ae_view_namespace;
struct ae_view;
enum ae_view_role { AE_VIEW_CODE=1, AE_VIEW_DATA=2 };
enum ae_view_phase { AE_VIEW_DETACHED=1, AE_VIEW_ATTACHED=2, AE_VIEW_UNCERTAIN=3 };

/* Trusted fresh single-threaded setup process only, before untrusted code.
 * Enter a new mount namespace and make propagation recursively private. This
 * changes the calling process even if a later step fails: exit on failure;
 * never reuse a long-lived authority or attempt an unconfined fallback.
 * No user namespace, policy, catalogue readiness, chroot or process exec here.
 * Caller journals intent BEFORE entering; it owns source/MAC authentication,
 * inventory/content checks, namespace quiescence and source-writer exclusion.
 * Open and verify the source/target handles inside this new namespace.
 * Every object is process/namespace scoped; use after fork returns ECHILD. */
int ae_view_namespace_enter(struct ae_view_namespace **output);
/* Clone one exact held directory into an owned detached mount, without copying
 * nested mounts. Never clear existing restrictions. CODE is readonly and must
 * permit execution; DATA must be writable and is noexec. Both are nodev/nosuid/
 * nosymfollow. This protects the view, not other aliases of its backing files.
 * Output is unchanged on error. No path lookup or mount attachment occurs. */
int ae_view_clone(struct ae_view_namespace *space,int source,const struct ae_root_identity *expected,
                  enum ae_view_role role,struct ae_view **output);
/* Attach exactly once to an empty, caller-owned directory named app or data
 * below a held private setup root. The target must still match expected, be on
 * the parent's mount and not alias the source. Parent namespace/topology writers
 * must be excluded by the caller. A move attempt consumes this view's attachment
 * opportunity, including unknown/post-move failures: dispose of the setup process
 * before releasing any corresponding durable code/data obligation. No rollback
 * unmount or lazy-unmount success is inferred. Output state is observed below. */
int ae_view_attach(struct ae_view *view,int parent,const struct ae_root_identity *expected,const char *slot);
int ae_view_check(struct ae_view *view,enum ae_view_phase *phase,struct ae_root_identity *identity);
/* Close only local handles, never an attached mount or durable lease. Namespace
 * and all app/child references must actually disappear before claiming drain.
 * Linux close errors still consume the handle; never retry a possibly reused FD.
 * namespace_close refuses while view objects still reference it. */
int ae_view_close(struct ae_view **view);
int ae_view_namespace_close(struct ae_view_namespace **space);

#endif
