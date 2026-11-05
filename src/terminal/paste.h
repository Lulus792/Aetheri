#ifndef AETHERI_TERMINAL_PASTE_H
#define AETHERI_TERMINAL_PASTE_H
#include "model.h"
enum { AE_TERM_PASTE_BYTES=4096, AE_TERM_PREVIEW_BYTES=256 };
enum ae_term_paste_result { AE_TERM_PASTE_OK, AE_TERM_PASTE_EMPTY, AE_TERM_PASTE_TOO_LARGE, AE_TERM_PASTE_UTF8, AE_TERM_PASTE_CONTROL };
struct ae_terminal_paste { char text[AE_TERM_PASTE_BYTES+1];size_t size;unsigned lines;bool review; };
/* Validate the complete snapshot before changing the destination. CRLF/CR become
   LF; other bytes remain literal. C0 except TAB/LF, DEL, C1 and bidi formatting
   controls are rejected. LF/TAB and Unicode line separators require review.
   No PTY, clipboard, shell parse or execution effect is performed. */
enum ae_term_paste_result ae_terminal_paste_set(struct ae_terminal_paste *paste,const void *bytes,size_t size);
void ae_terminal_paste_clear(struct ae_terminal_paste *paste);
/* Bounded, UTF-8-safe preview rows. Backslashes, TAB, LF and zero-width/nonprinting
   scalars are shown with visible escapes; the actual normalized snapshot stays
   unchanged. Columns:16..120. Returns next source offset; always advances for
   valid remaining text. No font/toolkit allocation. */
size_t ae_terminal_paste_preview(const struct ae_terminal_paste *paste,size_t offset,unsigned columns,
    ae_term_width width,char line[AE_TERM_PREVIEW_BYTES]);
#endif
