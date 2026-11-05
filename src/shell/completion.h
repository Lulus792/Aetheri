#ifndef AETHERI_SHELL_COMPLETION_H
#define AETHERI_SHELL_COMPLETION_H
#include "editor.h"
enum { AE_COMPLETION_COUNT=128, AE_COMPLETION_BYTES=65536 };
enum ae_completion_status { AE_COMPLETION_READY, AE_COMPLETION_NONE,
    AE_COMPLETION_UNSUPPORTED, AE_COMPLETION_LIMIT, AE_COMPLETION_IO, AE_COMPLETION_CANCELLED };
struct ae_completion_item { size_t offset;bool directory; };
struct ae_shell_completion {
    size_t begin,end,count,used,replacement_size;
    struct ae_completion_item items[AE_COMPLETION_COUNT];
    char values[AE_COMPLETION_BYTES],replacement[AE_LINE_BYTES+1];
};
/* Ordinary-user advisory directory/PATH lookup only: never opens candidate
   content, executes code or changes cwd/environment/editor. Immutable borrowed
   line and optional PATH (NULL means /usr/bin:/bin). No variable evaluation.
   Complete token at cursor (closing quotes may follow); literal suffixes are
   unsupported. Errors clear output. READY owns sorted/deduplicated values plus
   an optional fully quoted replacement. Check current editor/owner before apply.
   Bounded entries/directories/candidates/bytes and cooperative cancellation;
   individual filesystem syscalls can still block, especially on remote mounts.
   Line/PATH/output must not overlap or change during the call; callbacks may
   observe cancellation only, never mutate these inputs or recurse. */
enum ae_completion_status ae_shell_complete(const char *line,size_t size,size_t cursor,const char *path,
    bool (*cancelled)(void *),void *context,struct ae_shell_completion *out);
/* Encode a literal token in the shell's own quoting grammar, no expansion.
   Whole output unchanged on invalid/capacity failure. Required size excludes NUL.
   Input/output/required must not overlap. */
bool ae_shell_quote(const char *value,size_t size,char *out,size_t capacity,size_t *required);
#endif
