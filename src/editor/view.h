#ifndef AETHERI_EDITOR_VIEW_H
#define AETHERI_EDITOR_VIEW_H
#include "modal.h"
enum { AE_EDITOR_VIEW_COLUMNS=512, AE_EDITOR_VIEW_ROWS=200 };
struct ae_editor_view;
struct ae_editor_view_state {
    const char *title,*message,*command;
    const char *dialog;bool uncertain,help;unsigned help_offset;
    char command_prefix; /* zero means ':', otherwise '/' or '?' for search */
    size_t highlight_size; /* admitted whole-grapheme match at the document cursor */
};
struct ae_editor_frame { const char *bytes;size_t size;unsigned x,y;bool compact;unsigned help_max; };
/* Owns reusable row/segmentation/frame scratch and viewport only. Borrowed frame
   lasts until next render/destruction. Terminal-cell profile, not shaped/bidi or
   accessible document geometry. No I/O, input parsing or document mutation. */
struct ae_editor_view *ae_editor_view_create(void);
void ae_editor_view_destroy(struct ae_editor_view *view);
bool ae_editor_view_render(struct ae_editor_view *view,const struct ae_modal *modal,
    const struct ae_editor_view_state *state,unsigned columns,unsigned rows,struct ae_editor_frame *frame);
#endif
