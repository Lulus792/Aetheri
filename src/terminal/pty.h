#ifndef AETHERI_TERMINAL_PTY_H
#define AETHERI_TERMINAL_PTY_H
#include <stdbool.h>
#include <sys/types.h>
struct ae_terminal_pty { int master,pin;pid_t pid;bool ended,eof,hangup,pidfd_wait;int status; };
/* Internal shared baseline for a newly allocated private slave, before any
   child can use it. Changes terminal modes; caller retains the original FD.
   Not for reconfiguring an existing session or an externally supplied tty. */
int ae_terminal_pty_configure_slave(int slave);
/* Fresh single-threaded trusted setup only, before SDL/Mesa/worker creation.
   A held regular ELF source and fixed argv/env are caller obligations. No app
   admission, namespace isolation or descendant drain is implied. */
int ae_terminal_pty_open(struct ae_terminal_pty *pty,int source,unsigned cols,unsigned rows);
/* Separate fixed shell profile, adding --welcome to the original -i --edit.
   Decoration is actual shell output; no input injection or substitute shell. */
int ae_terminal_pty_open_welcome(struct ae_terminal_pty *pty,int source,unsigned cols,unsigned rows);
/* Separate development editor profile: passes literal leaf and copies directory/
   O_PATH witness to child FDs 3/4, retaining no other inherited input descriptors.
   Caller retains ownership. No shell parsing, normal file grant or app admission. */
int ae_terminal_pty_open_editor(struct ae_terminal_pty *pty,int source,int directory,int witness,const char *leaf,unsigned cols,unsigned rows);
int ae_terminal_pty_resize(struct ae_terminal_pty *pty,unsigned cols,unsigned rows);
ssize_t ae_terminal_pty_read(struct ae_terminal_pty *pty,void *buffer,size_t length);
/* Stream EOF and original-child exit are separate observations. EOF is not a
   descendant/holder fence; reads require positive capacity. */
int ae_terminal_pty_observe(struct ae_terminal_pty *pty);
int ae_terminal_pty_hangup(struct ae_terminal_pty *pty);
/* Refuses to drop unsettled child/stream obligations. */
int ae_terminal_pty_close(struct ae_terminal_pty *pty);
#endif
