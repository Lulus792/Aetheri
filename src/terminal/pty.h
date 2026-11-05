#ifndef AETHERI_TERMINAL_PTY_H
#define AETHERI_TERMINAL_PTY_H
#include <stdbool.h>
#include <sys/types.h>
struct ae_terminal_pty { int master,pin;pid_t pid;bool ended,eof,hangup;int status; };
/* Fresh single-threaded trusted setup only, before SDL/Mesa/worker creation.
   A held regular ELF source and fixed argv/env are caller obligations. No app
   admission, namespace isolation or descendant drain is implied. */
int ae_terminal_pty_open(struct ae_terminal_pty *pty,int source,unsigned cols,unsigned rows);
int ae_terminal_pty_resize(struct ae_terminal_pty *pty,unsigned cols,unsigned rows);
ssize_t ae_terminal_pty_read(struct ae_terminal_pty *pty,void *buffer,size_t length);
/* Stream EOF and original-child exit are separate observations. EOF is not a
   descendant/holder fence; reads require positive capacity. */
int ae_terminal_pty_observe(struct ae_terminal_pty *pty);
int ae_terminal_pty_hangup(struct ae_terminal_pty *pty);
/* Refuses to drop unsettled child/stream obligations. */
int ae_terminal_pty_close(struct ae_terminal_pty *pty);
#endif
