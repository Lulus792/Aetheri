/* Optional NexaUI presentation; the original terminal host owns all effects. */
#ifndef AETHERI_TERMINAL_NEXAUI_VIEW_H
#define AETHERI_TERMINAL_NEXAUI_VIEW_H
#include "model.h"
#include <nexaui/cell_grid.h>
struct ae_terminal_view;
/* Font bytes are copied by the host and borrowed until view destruction. No
   window, PTY, clipboard connection, timer or second terminal model is created. */
bool ae_terminal_view_create(const void *regular,size_t,const void *bold,size_t,struct ae_terminal_view **);
void ae_terminal_view_destroy(struct ae_terminal_view *);
nx_context *ae_terminal_view_context(struct ae_terminal_view *);
bool ae_terminal_view_begin(struct ae_terminal_view *,float width,float height,float scale,float points,
    float cell_width,float cell_height,float baseline);
bool ae_terminal_view_fill(struct ae_terminal_view *,nx_rect,uint32_t);
bool ae_terminal_view_clip(struct ae_terminal_view *,const nx_rect *);
bool ae_terminal_view_label(struct ae_terminal_view *,const char *,float x,float y,uint32_t);
bool ae_terminal_view_grid(struct ae_terminal_view *,const struct ae_terminal *,nx_rect,bool contrast,bool live_caret);
/* Complete-frame RGBA8, borrowed until the next finish/destroy. At most 64 MiB;
   the SDL host uploads it once and remains the sole native window dispatcher. */
bool ae_terminal_view_finish(struct ae_terminal_view *,unsigned width,unsigned height,
    const unsigned char **pixels,size_t *stride);
#endif
