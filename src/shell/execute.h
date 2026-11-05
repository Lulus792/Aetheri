#ifndef AETHERI_SHELL_EXECUTE_H
#define AETHERI_SHELL_EXECUTE_H
#include "parse.h"
#include <stdbool.h>
struct ae_shell_terminal;
struct ae_shell_session { int status; bool exiting; char status_text[4]; struct ae_shell_terminal *terminal; };
const char *ae_shell_environment(void *context,const char *name);
/* A successful parsed program only. Single-threaded ordinary Linux user only;
   caller owns admission, SIGCHLD waitability and stdio.
   Mutates process cwd/environment. Reaps direct children, not descendant drain. */
int ae_shell_execute(struct ae_shell_session *session,struct ae_shell_program *program);
#endif
