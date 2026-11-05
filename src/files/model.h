#ifndef AETHERI_FILES_MODEL_H
#define AETHERI_FILES_MODEL_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
enum { AE_FILES_ENTRIES=4096, AE_FILES_DEPTH=64, AE_FILES_NAME=255, AE_FILES_LABEL=1024, AE_FILES_QUERY=255 };
enum ae_files_result { AE_FILES_OK, AE_FILES_INVALID, AE_FILES_IO, AE_FILES_NOMEM, AE_FILES_LIMIT, AE_FILES_CHANGED, AE_FILES_UNSUPPORTED, AE_FILES_ROOT, AE_FILES_EXHAUSTED };
enum ae_files_kind { AE_FILES_DIRECTORY, AE_FILES_REGULAR, AE_FILES_LINK, AE_FILES_OTHER, AE_FILES_UNAVAILABLE };
struct ae_files_entry { char name[AE_FILES_NAME+1],label[AE_FILES_LABEL+1];enum ae_files_kind kind;dev_t device;ino_t inode;uint64_t size; };
struct ae_files;
/* Ordinary-user development navigation, not a sandbox, StorageRef, file grant or
   volume/clone identity. Root is independently selected by the caller. Held
   directory stack, no symlink traversal, no parent escape through '..'. Mounts
   and renamed held directories retain ordinary kernel/process rights. No writes,
   watching daemon, recursion or content reads. Snapshots are bounded observations;
   actions recheck selected inode/type. Read-only views expire on refresh/destroy. */
struct ae_files *ae_files_create(int root,enum ae_files_result *result);
void ae_files_destroy(struct ae_files *files);
enum ae_files_result ae_files_refresh(struct ae_files *files);
enum ae_files_result ae_files_enter(struct ae_files *files);
enum ae_files_result ae_files_up(struct ae_files *files);
bool ae_files_filter(struct ae_files *files,const char *query,size_t size);
void ae_files_hidden(struct ae_files *files,bool show);
bool ae_files_select(struct ae_files *files,size_t visible_index);
size_t ae_files_count(const struct ae_files *files);
size_t ae_files_total(const struct ae_files *files);
size_t ae_files_selected(const struct ae_files *files);
unsigned ae_files_depth(const struct ae_files *files);
const char *ae_files_query(const struct ae_files *files);
const char *ae_files_folder(const struct ae_files *files);
/* Own CLOEXEC duplicate of the current held directory, >=4. Caller closes it.
   Local navigation observation only, not a writable grant or allocation proof. */
int ae_files_directory(const struct ae_files *files);
const struct ae_files_entry *ae_files_entry(const struct ae_files *files,size_t visible_index);
/* Own CLOEXEC directory + O_PATH regular-file witness, >=4. Caller closes both.
   Their identity is transient and FD-bound, not durable authorization. Leaf is
   literal copied bytes, never command text. Failed admission leaves outputs -1. */
enum ae_files_result ae_files_open_selected(const struct ae_files *files,int *directory,int *witness,char leaf[AE_FILES_NAME+1]);
/* Same transient selected-object checks, but also accepts a directory. Does not
   authorize mutation or turn its FD into a grant. Owned outputs as above. */
enum ae_files_result ae_files_hold_selected(const struct ae_files *files,int *directory,int *witness,char leaf[AE_FILES_NAME+1]);
#endif
