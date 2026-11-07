/* Optional NexaUI presentation; the original terminal host owns all effects. */
#ifndef AETHERI_TERMINAL_NEXAUI_VIEW_H
#define AETHERI_TERMINAL_NEXAUI_VIEW_H
#include "model.h"
#include <nexaui/cell_grid.h>
struct ae_terminal_view;
struct ae_terminal_pane_view {
    uint64_t id;const char *name,*description;const struct ae_terminal *model;nx_rect bounds;
    bool contrast,caret,focusable,enabled;
};
struct ae_terminal_view_control {
    uint64_t id,parent;nx_rect bounds;const char *label,*name;
    bool selected,disabled,pressed,hovered,tab,contrast;
};
bool ae_terminal_view_control(struct ae_terminal_view *,const struct ae_terminal_view_control *,bool *activated);
bool ae_terminal_view_group(struct ae_terminal_view *,uint64_t id,nx_rect,const char *name,bool tabs);
/* Register a required single-selection group and consume native requests into
   a temporary widget ID. The host validates/applies it before drawing members;
   no session or model pointer is retained by the view. */
bool ae_terminal_view_tabs(struct ae_terminal_view *,uint64_t id,nx_rect,const char *name,bool disabled,uint64_t *selected);
enum ae_terminal_chrome_action { AE_TERM_CHROME_NONE,AE_TERM_CHROME_CLOSE,AE_TERM_CHROME_MINIMIZE,AE_TERM_CHROME_ZOOM };
struct ae_terminal_chrome { nx_rect bar,title,buttons[3]; };
struct ae_terminal_chrome_state { bool german,contrast,editor,maximized,zoom_disabled;unsigned hovered,pressed; };
bool ae_terminal_chrome_layout(float width,float height,float text_scale,struct ae_terminal_chrome *);
unsigned ae_terminal_chrome_target(const struct ae_terminal_chrome *,float x,float y);
/* Minimize is explicitly unavailable until the development compositor supports
   its complete hide/restore lifecycle. Requests never imply observed state. */
bool ae_terminal_view_chrome(struct ae_terminal_view *,const struct ae_terminal_chrome_state *,unsigned *request);
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
/* One original model lifetime per view. Reinitializing/replacing a model requires
   a fresh view; its row tokens are process-local content witnesses, not authority. */
bool ae_terminal_view_grid(struct ae_terminal_view *,const struct ae_terminal *,nx_rect,bool contrast,bool live_caret,bool read_focus);
/* Between completed frames, on the original owner thread. Reading focus never
   edits the model, sends PTY input or claims native window activation. */
bool ae_terminal_view_focus_row(struct ae_terminal_view *,unsigned row);
/* Named multi-pane mode has disjoint, monotone semantic row identities. An ID
   binds one original model while projected; hidden/retired views get fresh
   identities on return. Do not mix with legacy grid calls in one view. */
bool ae_terminal_view_pane(struct ae_terminal_view *,const struct ae_terminal_pane_view *);
bool ae_terminal_view_focus_pane_row(struct ae_terminal_view *,uint64_t pane,unsigned row);
bool ae_terminal_view_reading(const struct ae_terminal_view *);
/* Complete-frame RGBA8, borrowed until the next finish/destroy. At most 64 MiB;
   the SDL host uploads it once and remains the sole native window dispatcher. */
bool ae_terminal_view_finish(struct ae_terminal_view *,unsigned width,unsigned height,
    const unsigned char **pixels,size_t *stride);
#endif
