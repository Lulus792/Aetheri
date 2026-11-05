#ifndef AETHERI_FILES_RENAME_H
#define AETHERI_FILES_RENAME_H
#include <stddef.h>
enum ae_files_rename_result {
    AE_FILES_RENAME_PREPARED,AE_FILES_RENAME_DONE,AE_FILES_RENAME_CONFLICT,
    AE_FILES_RENAME_INVALID,AE_FILES_RENAME_DENIED,AE_FILES_RENAME_NOMEM,
    AE_FILES_RENAME_IO,AE_FILES_RENAME_UNKNOWN,AE_FILES_RENAME_CHANGED
};
struct ae_files_rename;
/* Ordinary-user same-folder rename, not a grant or durable recovery journal.
   Source is an independently selected O_PATH regular-file/directory witness.
   Own duplicates of parent/witness and immutable literal old/new leaves. New
   names use the Files creation rules; old raw names may need escaped display.
   Parent must be owned/private/writable/searchable. Preparation has no effect. */
struct ae_files_rename *ae_files_rename_prepare(int directory,int witness,
    const char *old_name,size_t old_size,const char *new_name,size_t new_size,
    enum ae_files_rename_result *result);
/* Recheck original source and parent, then at most one RENAME_NOREPLACE attempt
   and parent flush. Every retry returns the original result without effects.
   Destination nonreplacement is atomic; SOURCE matching is NOT compare-and-swap.
   No hostile concurrent-writer exclusion, namespace/volume identity or restart
   reconciliation. After dispatch any error is UNKNOWN, never inferred absence.
   No fallback to replace/copy/unlink, rollback or candidate cleanup. */
enum ae_files_rename_result ae_files_rename_apply(struct ae_files_rename *operation);
enum ae_files_rename_result ae_files_rename_status(const struct ae_files_rename *operation);
const char *ae_files_rename_old(const struct ae_files_rename *operation);
const char *ae_files_rename_new(const struct ae_files_rename *operation);
void ae_files_rename_close(struct ae_files_rename *operation);
#endif
