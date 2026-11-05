#ifndef AETHERI_TERMINAL_COMPOSITION_H
#define AETHERI_TERMINAL_COMPOSITION_H
#include "model.h"
enum { AE_TERM_COMPOSITION_BYTES=1024 };
struct ae_terminal_composition {
    char text[AE_TERM_COMPOSITION_BYTES+1];
    uint16_t offsets[AE_TERM_COMPOSITION_BYTES+1];
    unsigned count,cursor,anchor;size_t size;bool caret;
};
struct ae_terminal_composition_view {
    size_t begin,end;unsigned cells,cursor,selection_begin,selection_end;
    bool caret,hidden_before,hidden_after;
};
/* Entire UTF-8 draft, one line, no terminal/bidi controls. Position units are
   Unicode scalars, not bytes/cells/graphemes. -1 means unspecified under SDL's
   contract; other negative lengths describe reverse ranges. Failure is atomic.
   Empty text clears; this model never writes PTY bytes or app history. */
bool ae_terminal_composition_set(struct ae_terminal_composition *composition,
    const void *text,size_t bytes,int start,int length);
void ae_terminal_composition_clear(struct ae_terminal_composition *composition);
/* A bounded horizontal viewport following the draft cursor. Uses the terminal's
   existing width policy, not a general bidi/grapheme shaping implementation. */
bool ae_terminal_composition_view(const struct ae_terminal_composition *composition,
    unsigned columns,ae_term_width width,struct ae_terminal_composition_view *out);
#endif
