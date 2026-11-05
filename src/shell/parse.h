#ifndef AETHERI_SHELL_PARSE_H
#define AETHERI_SHELL_PARSE_H
#include <stddef.h>
enum { AE_SHELL_BYTES=16384, AE_SHELL_COMMANDS=16, AE_SHELL_ARGS=64, AE_SHELL_WORDS=256 };
enum ae_shell_error { AE_SHELL_OK, AE_SHELL_SYNTAX, AE_SHELL_LIMIT, AE_SHELL_UNSET };
enum ae_shell_redirect { AE_SHELL_NO_REDIRECT, AE_SHELL_READ, AE_SHELL_WRITE, AE_SHELL_APPEND };
struct ae_shell_file { enum ae_shell_redirect mode; char *path; };
struct ae_shell_command {
    size_t argc;
    char *argv[AE_SHELL_ARGS+1];
    struct ae_shell_file files[3];
};
struct ae_shell_program {
    size_t count, used;
    struct ae_shell_command commands[AE_SHELL_COMMANDS];
    char bytes[AE_SHELL_BYTES];
};
/* Lookup returns a borrowed NUL-terminated value, copied immediately; NULL is
   unset. Successful strings are owned by out, never by the source/lookup.
   Do not copy/move a successful object without rebasing its internal pointers.
   Source/lookup storage must not overlap out. Failure empties out. Error offsets
   are byte offsets, not Unicode columns. */
typedef const char *(*ae_shell_lookup)(void *context, const char *name);
enum ae_shell_error ae_shell_parse(const char *source,size_t length,ae_shell_lookup lookup,
    void *context,struct ae_shell_program *out,size_t *offset);
int ae_shell_name(const char *name);
#endif
