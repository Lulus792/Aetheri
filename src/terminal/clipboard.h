#ifndef AETHERI_TERMINAL_CLIPBOARD_H
#define AETHERI_TERMINAL_CLIPBOARD_H
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
struct wl_display;
struct wl_surface;
struct ae_clipboard;
enum ae_clipboard_result {
    AE_CLIPBOARD_NONE, AE_CLIPBOARD_PENDING, AE_CLIPBOARD_DATA,
    AE_CLIPBOARD_NO_TEXT, AE_CLIPBOARD_LIMIT, AE_CLIPBOARD_TIMEOUT,
    AE_CLIPBOARD_CANCELLED, AE_CLIPBOARD_IO_ERROR
};
enum { AE_CLIPBOARD_TEXT_LIMIT=8*1024*1024, AE_CLIPBOARD_MEMORY_LIMIT=16*1024*1024 };
struct ae_clipboard_observation { size_t bytes;unsigned sources,sends,offers;bool receive,sync_pending,available,focused; };
/* Owner-local resource counters, not app admission or delivery receipts. */
int ae_clipboard_observe(const struct ae_clipboard *clipboard,struct ae_clipboard_observation *out);
/* One owner thread, same thread/queue as the borrowed window surface. The caller
   dispatches that queue; no second connection or reader is created. A single
   seat is required. Wake only schedules owner work: no API reentry from wake.
   Borrowed display/surface must outlive close; *out starts NULL. errno-style. */
int ae_clipboard_open(struct ae_clipboard **out,struct wl_display *display,
    struct wl_surface *surface,void (*wake)(void *),void *userdata);
bool ae_clipboard_focused(const struct ae_clipboard *clipboard);
uint64_t ae_clipboard_focus_epoch(const struct ae_clipboard *clipboard);
/* On success takes the malloc-owned NUL-terminated snapshot and clears *text.
   This queues a native publication, not a receipt of delivery or authority. */
int ae_clipboard_publish(struct ae_clipboard *clipboard,char **text,size_t length);
/* At most one receive and one unacknowledged FD-bearing request. Total deadline
   is five seconds, not renewed by bytes. Limit is caller-selected, <=8 MiB. */
int ae_clipboard_receive(struct ae_clipboard *clipboard,size_t limit);
int ae_clipboard_cancel(struct ae_clipboard *clipboard);
/* DATA transfers malloc ownership; all terminal results are consumed once.
   PENDING/NONE leave outputs unchanged. *text/length start NULL/0. */
enum ae_clipboard_result ae_clipboard_result(struct ae_clipboard *clipboard,char **text,size_t *length);
/* Pump on the owner thread. A readiness worker may poll only this stable epoll
   descriptor; it must never read it or access the object. Deadline uses absolute
   CLOCK_MONOTONIC milliseconds (0 means no timer). Copy deadline to the worker
   with synchronization; stop/join that worker before close. */
int ae_clipboard_watch_fd(const struct ae_clipboard *clipboard);
uint64_t ae_clipboard_deadline(const struct ae_clipboard *clipboard);
int ae_clipboard_pump(struct ae_clipboard *clipboard);
int ae_clipboard_close(struct ae_clipboard **clipboard);
#endif
