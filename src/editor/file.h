#ifndef AETHERI_EDITOR_FILE_H
#define AETHERI_EDITOR_FILE_H
#include "document.h"

enum ae_editor_file_result { AE_EDITOR_FILE_OK, AE_EDITOR_FILE_INVALID,
    AE_EDITOR_FILE_UNSUPPORTED, AE_EDITOR_FILE_CONFLICT, AE_EDITOR_FILE_IO,
    AE_EDITOR_FILE_NOMEM, AE_EDITOR_FILE_UNKNOWN };
struct ae_editor_file;
/* Development-only, single synchronous owner. Caller supplies an independently
   chosen directory FD and one leaf, not a grant/path to resolve in an authority.
   Owns independent directory/file descriptors and original bytes/metadata.
   Returns a separately caller-owned document, which outlives no file operation.
   Local Linux filesystems only; no hostile same-UID writer, app admission,
   brokered save, volume identity or crash-recovery claim. */
enum ae_editor_file_result ae_editor_file_open(int directory,const char *leaf,
    struct ae_editor_file **file,struct ae_document **document);
/* Explicit development handoff only: witness is an independently retained
   O_PATH/regular file FD. The named opened object must match it before reading.
   Neither FD is consumed; no normal document grant or durable identity implied. */
enum ae_editor_file_result ae_editor_file_open_selected(int directory,const char *leaf,int witness,
    struct ae_editor_file **file,struct ae_document **document);
/* Creates no file until save. An existing name is a conflict, never adopted. */
enum ae_editor_file_result ae_editor_file_new(int directory,const char *leaf,
    struct ae_editor_file **file,struct ae_document **document);
/* Preserves original UTF-8/endings, ordinary mode/GID and readable xattrs.
   Rejects set-ID, hard links, foreign owner, capability/oversize metadata and
   unavailable metadata rather than silently dropping them. Pre-rename failures
   leave the target untouched. UNKNOWN fences all later saves on this object;
   retain the document and any named recovery file reported below. Content clean
   state is unchanged on failure; UNKNOWN is a separate close/save fence even
   when the submitted document was already clean. No
   rollback/retry after a possibly applied rename or failed post-rename flush.
   Existing-name checks are not an atomic conditional rename against other apps. */
enum ae_editor_file_result ae_editor_file_save(struct ae_editor_file *file,struct ae_document *document);
bool ae_editor_file_uncertain(const struct ae_editor_file *file);
/* Borrowed literal leaf of an owned/potential recovery artifact, or NULL.
   Do not concatenate it into shell commands or delete a name merely on this basis.
   Close frees memory/FDs; it never deletes an uncertain artifact. */
const char *ae_editor_file_recovery(const struct ae_editor_file *file);
void ae_editor_file_close(struct ae_editor_file *file);
#endif
