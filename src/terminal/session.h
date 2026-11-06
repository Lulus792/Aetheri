/* Internal terminal state shared by native presentation adapters. */
#ifndef AETHERI_TERMINAL_SESSION_H
#define AETHERI_TERMINAL_SESSION_H
#include "model.h"
#include "pty.h"

enum { AE_TERM_INPUT_BYTES=4096, AE_TERM_READ_BUDGET=16384 };
struct ae_terminal_session {
    struct ae_terminal_pty pty;
    struct ae_terminal *model;
    unsigned char input[AE_TERM_INPUT_BYTES];
    size_t input_count;uint64_t input_bytes;
    bool closing;
};
#define AE_TERMINAL_SESSION_INIT { .pty={ .master=-1,.pin=-1,.pid=-1 } }

/* One owner thread. Initialize once with AE_TERMINAL_SESSION_INIT, then supply
   an initialized, independently owned model and open the original PTY before
   starting workers. Never copy live state or replace its model/PTY. The host
   retains allocation, trusted source admission, pidfd wait and final close.
   A presentation adapter borrows this state; destroying a view settles nothing.
   These methods do not validate clipboard/IME provenance or authorize app runs. */
bool ae_terminal_session_accepting(const struct ae_terminal_session *);
/* Whole submission or no change. Zero bytes is a no-op. EBUSY means intake is
   fenced/ended; ENOBUFS means capacity, EINVAL means invalid caller arguments.
   Success stages bytes, clears selection/scrollback and counts admission, not
   delivery/execution. No syscall, wakeup, text validation or implicit retry. */
bool ae_terminal_session_enqueue(struct ae_terminal_session *,const void *,size_t);
/* Nonblocking original-master write. Retain the exact unsent suffix on partial
   progress/EAGAIN/error. The host must handle failure and readiness; return zero
   alone does not mean the queue is empty. Closing fences new intake but preserves
   already admitted bytes and the original child/stream obligations. */
int ae_terminal_session_flush(struct ae_terminal_session *);
/* At most AE_TERM_READ_BUDGET bytes per turn; feed only actual original-master
   output. changed remains true after partial progress followed by an error.
   Does not reap the child, infer drain, or release either held descriptor. */
int ae_terminal_session_read(struct ae_terminal_session *,bool *changed);
int ae_terminal_session_resize(struct ae_terminal_session *,unsigned cols,unsigned rows);
/* Fence intake before the one-shot pidfd hangup. Never closes/reaps/discards the
   queue. The editor's user save/drop/back request remains a separate host action. */
int ae_terminal_session_request_close(struct ae_terminal_session *);
#endif
