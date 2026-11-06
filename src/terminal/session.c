#include "session.h"
#include <errno.h>
#include <string.h>
#include <unistd.h>

bool ae_terminal_session_accepting(const struct ae_terminal_session *s) {
    return s && s->model && s->pty.master>=0 && s->pty.pin>=0 && s->pty.pid>0 &&
        !s->closing && !s->pty.ended && !s->pty.eof;
}
bool ae_terminal_session_enqueue(struct ae_terminal_session *s,const void *bytes,size_t size) {
    if (!s || !s->model || (!bytes && size) || s->input_count>sizeof(s->input)) { errno=EINVAL;return false; }
    if (!ae_terminal_session_accepting(s)) { errno=EBUSY;return false; }
    if (size>sizeof(s->input)-s->input_count) { errno=ENOBUFS;return false; }
    if (!size) return true;
    memcpy(s->input+s->input_count,bytes,size);s->input_count+=size;s->input_bytes+=(uint64_t)size;
    ae_terminal_select_clear(s->model);s->model->view=0;
    return true;
}
int ae_terminal_session_flush(struct ae_terminal_session *s) {
    if (!s || s->input_count>sizeof(s->input)) { errno=EINVAL;return -1; }
    while (s->input_count && !s->pty.eof) {
        ssize_t n=write(s->pty.master,s->input,s->input_count);
        if (n<0 && errno==EINTR) continue;
        if (n<0 && (errno==EAGAIN || errno==EWOULDBLOCK)) return 0;
        if (n<=0) { if (!n) errno=EIO;return -1; }
        memmove(s->input,s->input+n,s->input_count-(size_t)n);s->input_count-=(size_t)n;
    }
    return 0;
}
int ae_terminal_session_read(struct ae_terminal_session *s,bool *changed) {
    if (changed) *changed=false;
    if (!s || !s->model || !changed) { errno=EINVAL;return -1; }
    unsigned char bytes[4096];size_t budget=AE_TERM_READ_BUDGET;
    while (!s->pty.eof && budget) {
        ssize_t n=ae_terminal_pty_read(&s->pty,bytes,sizeof(bytes));
        if (n<0 && errno==EINTR) continue;
        if (n<0 && (errno==EAGAIN || errno==EWOULDBLOCK)) return 0;
        if (n<0) return -1;
        if (!n) break;
        ae_terminal_feed(s->model,bytes,(size_t)n);budget-=(size_t)n;*changed=true;
    }
    return 0;
}
int ae_terminal_session_resize(struct ae_terminal_session *s,unsigned cols,unsigned rows) {
    if (!s || !s->model || cols<2 || cols>AE_TERM_COLS || rows<2 || rows>AE_TERM_ROWS) { errno=EINVAL;return -1; }
    if (ae_terminal_pty_resize(&s->pty,cols,rows)) return -1;
    if (!ae_terminal_resize(s->model,cols,rows)) { errno=EINVAL;return -1; }
    return 0;
}
int ae_terminal_session_request_close(struct ae_terminal_session *s) {
    if (!s) { errno=EINVAL;return -1; }
    if (s->closing) return 0;
    s->closing=true;
    return ae_terminal_pty_hangup(&s->pty);
}
