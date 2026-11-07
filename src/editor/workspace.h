#ifndef AETHERI_EDITOR_WORKSPACE_H
#define AETHERI_EDITOR_WORKSPACE_H
#include "direct.h"
#include "file.h"
enum { AE_EDITOR_TABS=8 };
enum ae_editor_workspace_result { AE_EDITOR_WORKSPACE_OK, AE_EDITOR_WORKSPACE_INVALID,
    AE_EDITOR_WORKSPACE_LIMIT, AE_EDITOR_WORKSPACE_NOMEM, AE_EDITOR_WORKSPACE_CONFLICT,
    AE_EDITOR_WORKSPACE_UNSUPPORTED, AE_EDITOR_WORKSPACE_IO, AE_EDITOR_WORKSPACE_UNKNOWN,
    AE_EDITOR_WORKSPACE_REVIEW, AE_EDITOR_WORKSPACE_NEEDS_PATH, AE_EDITOR_WORKSPACE_BUSY };
enum ae_editor_close_choice { AE_EDITOR_CLOSE_CANCEL, AE_EDITOR_CLOSE_DISCARD, AE_EDITOR_CLOSE_SAVE };
struct ae_editor_workspace;
struct ae_editor_tab {
    uint64_t id;const char *leaf,*recovery;const struct ae_document *document;
    bool active,uncertain,saving;
};
struct ae_editor_close_token { uint64_t id,revision,serial; };
struct ae_editor_destination_token { uint64_t id,revision,interaction,review; };
/* Single synchronous development owner. Owns documents/direct controllers and
   local file adapters; inherits file.h's filesystem/metadata/trust restrictions.
   No window, process, broker, persistence, production grant or removal effect.
   Destroy releases local memory/FDs, never deletes files/recovery artifacts and
   does not approve user close. Destroy refuses active worker ownership; finish
   or abort that exact work first. Normal UI shutdown must close every tab. */
struct ae_editor_workspace *ae_editor_workspace_create(void);
void ae_editor_workspace_destroy(struct ae_editor_workspace *);
unsigned ae_editor_workspace_count(const struct ae_editor_workspace *);
uint64_t ae_editor_workspace_active(const struct ae_editor_workspace *);
bool ae_editor_workspace_at(const struct ae_editor_workspace *,unsigned index,struct ae_editor_tab *);
bool ae_editor_workspace_get(const struct ae_editor_workspace *,uint64_t id,struct ae_editor_tab *);
bool ae_editor_workspace_selection(const struct ae_editor_workspace *,uint64_t id,struct ae_direct_selection *);
/* Borrowed only until the next workspace operation; never retain in callbacks.
   NULL while a close review blocks editing or when no tab is active. Use normal
   direct-controller methods, not document casts. Tab IDs are the non-reused
   direct-controller identities: process-local UI correlations, not authorities. */
struct ae_direct *ae_editor_workspace_input(struct ae_editor_workspace *);
enum ae_editor_workspace_result ae_editor_workspace_new(struct ae_editor_workspace *,uint64_t *id);
/* Exact still-current local source reuses the original tab, preserving unsaved
   edits. A changed/replaced source conflicts instead of silently reloading it.
   Capacity/failed reads never discard another tab. Output unchanged on error. */
enum ae_editor_workspace_result ae_editor_workspace_open(struct ae_editor_workspace *,int directory,const char *leaf,uint64_t *id);
/* Local picker handoff; original held regular-file witness is required both
   for a new read and reuse of an existing original tab. FDs are borrowed. */
enum ae_editor_workspace_result ae_editor_workspace_open_selected(struct ae_editor_workspace *,int directory,const char *leaf,int witness,uint64_t *id);
enum ae_editor_workspace_result ae_editor_workspace_select(struct ae_editor_workspace *,uint64_t id);
enum ae_editor_workspace_result ae_editor_workspace_save(struct ae_editor_workspace *,uint64_t id);
/* Snapshot the original active untitled document before requesting a picker.
   No path permission is conveyed. Switching/editing/new close reviews invalidate
   this correlation; a delayed picker result must not write a later document. */
enum ae_editor_workspace_result ae_editor_workspace_destination(struct ae_editor_workspace *,uint64_t id,struct ae_editor_destination_token *);
/* First destination for an untitled document, always create-only. Known failure
   leaves it untitled so another explicit destination can be chosen. UNKNOWN
   retains that exact file adapter/name and blocks further save/discard close.
   Existing named Save As/export/recovery remain separate integration work. */
enum ae_editor_workspace_result ae_editor_workspace_save_new(struct ae_editor_workspace *,const struct ae_editor_destination_token *,int directory,const char *leaf);
/* Clean known tabs close immediately (OK, zero token). Dirty/uncertain tabs
   become active and freeze workspace edits/new/open/switch until a correlated
   decision (REVIEW). Exact pending request returns its original token. Cancel
   preserves the tab; Discard never deletes its file; Save closes only after the
   file adapter actually succeeds. NEEDS_PATH keeps the same pending review;
   save_new may supply that original tab's first destination before resolution.
   UNKNOWN cannot be resolved by Discard or inferred from clean content.
   Wrong/stale/replayed tokens fail without closing or changing another tab. */
enum ae_editor_workspace_result ae_editor_workspace_request_close(struct ae_editor_workspace *,uint64_t id,struct ae_editor_close_token *);
bool ae_editor_workspace_review(const struct ae_editor_workspace *,struct ae_editor_close_token *);
enum ae_editor_workspace_result ae_editor_workspace_resolve_close(struct ae_editor_workspace *,
    const struct ae_editor_close_token *,enum ae_editor_close_choice);

struct ae_editor_save_work;
/* One original save job per workspace. UI prepare captures copied text/history
   state and transfers the original file adapter to the job. Main documents stay
   editable/selectable; target close and conflicting filesystem operations are
   blocked until finish. A first save additionally borrows the fixed incumbent
   file adapters for duplicate checks, so their close is blocked too. No worker
   may access the workspace or live original documents. */
enum ae_editor_workspace_result ae_editor_workspace_prepare_save(struct ae_editor_workspace *,uint64_t id,struct ae_editor_save_work **);
enum ae_editor_workspace_result ae_editor_workspace_prepare_save_new(struct ae_editor_workspace *,const struct ae_editor_destination_token *,int directory,const char *leaf,struct ae_editor_save_work **);
/* Exactly one apply on the original worker. Immutable inputs are published by
   thread start; release/acquire done publishes result only. The trusted host
   MUST join that original worker before finish, freeing data or destroying the
   workspace. A done flag is not thread termination or successful file saving. */
bool ae_editor_save_work_apply(struct ae_editor_save_work *);
bool ae_editor_save_work_done(const struct ae_editor_save_work *);
uint64_t ae_editor_save_work_document(const struct ae_editor_save_work *);
/* No write cancellation/rollback. A failed start guarantees no worker exists;
   only then may abort return the original adapter without any file effect. */
bool ae_editor_workspace_abort_save(struct ae_editor_workspace *,struct ae_editor_save_work *);
/* Always returns the adapter/UNKNOWN obligation to its ORIGINAL retained tab,
   even after tab/input/review changes. Only actual OK acknowledges the captured
   history state; later edits are not declared saved. Never closes a document or
   resolves/rearms a later close review. Finish consumes work except on invalid/
   busy. Synchronous file APIs remain separately available. */
enum ae_editor_workspace_result ae_editor_workspace_finish_save(struct ae_editor_workspace *,struct ae_editor_save_work *);
#endif
