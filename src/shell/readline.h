#ifndef AETHERI_SHELL_READLINE_H
#define AETHERI_SHELL_READLINE_H
#include "editor.h"
#include "jobs.h"
/* Explicit own-terminal editor profile. Returns a complete line including LF,
   0 for deliberately requested empty EOF, -2 for Ctrl+C, -1 for loss/failure.
   Never reads bytes after the accepted LF. Restores original modes before any
   execution. No UI toolkit, history file, daemon or implicit terminal fallback. */
ssize_t ae_shell_readline(struct ae_shell_terminal *terminal,struct ae_line_editor *editor,char *out,size_t capacity);
#endif
