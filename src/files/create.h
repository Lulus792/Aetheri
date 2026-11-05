#ifndef AETHERI_FILES_CREATE_H
#define AETHERI_FILES_CREATE_H
#include <stdbool.h>
#include <stddef.h>
enum ae_files_create_kind { AE_FILES_CREATE_FILE, AE_FILES_CREATE_DIRECTORY };
enum ae_files_create_result {
    AE_FILES_CREATE_PREPARED, AE_FILES_CREATE_DONE, AE_FILES_CREATE_CONFLICT,
    AE_FILES_CREATE_INVALID, AE_FILES_CREATE_DENIED, AE_FILES_CREATE_NOMEM,
    AE_FILES_CREATE_IO, AE_FILES_CREATE_UNKNOWN
};
struct ae_files_create;
/* Local ordinary-user development operation, not an authority/allocation journal.
   One literal UTF-8 leaf; no slash, dot/dotdot, controls or hidden format characters.
   Caller supplies an independently selected directory FD. Owns a duplicate and
   immutable name/kind, so later UI navigation/path renames cannot retarget it.
   Requires an ordinary non-setid user and owned, owner-writable/searchable parent
   without group/world write or special mode bits. No effect during prepare. */
bool ae_files_create_name(const char *name,size_t size);
struct ae_files_create *ae_files_create_prepare(int directory,enum ae_files_create_kind kind,
    const char *name,size_t size,enum ae_files_create_result *result);
/* At most one attempt per object. Every repeat returns its original result with
   no mkdir/save/flush replay. Unknown keeps possible entries/stages; never undo,
   adopt a replacement or infer a durable absence. No hostile concurrent-writer,
   volume/clone authentication, file grant or restart recovery claim. */
enum ae_files_create_result ae_files_create_apply(struct ae_files_create *operation);
enum ae_files_create_result ae_files_create_status(const struct ae_files_create *operation);
const char *ae_files_create_leaf(const struct ae_files_create *operation);
/* Possible original staging name, not proof that it still exists or a backup. */
const char *ae_files_create_recovery(const struct ae_files_create *operation);
/* Releases memory/FDs only. Never deletes a possible created entry or stage. */
void ae_files_create_close(struct ae_files_create *operation);
#endif
