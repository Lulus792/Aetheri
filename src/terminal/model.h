#ifndef AETHERI_TERMINAL_MODEL_H
#define AETHERI_TERMINAL_MODEL_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
enum { AE_TERM_COLS=240, AE_TERM_ROWS=100, AE_TERM_HISTORY=1024 };
enum { AE_TERM_BOLD=1, AE_TERM_UNDERLINE=2, AE_TERM_REVERSE=4 };
struct ae_term_cell { uint32_t code,fg,bg,marks[3];uint8_t count,width,style; };
typedef int (*ae_term_width)(uint32_t code);
struct ae_terminal {
    struct ae_term_cell screen[AE_TERM_ROWS][AE_TERM_COLS], alternate[AE_TERM_ROWS][AE_TERM_COLS];
    struct ae_term_cell history[AE_TERM_HISTORY][AE_TERM_COLS];
    unsigned cols,rows,x,y,saved_x,saved_y,top,bottom,history_head,history_count,view;
    unsigned normal_x,normal_y;uint32_t normal_fg,normal_bg;uint8_t normal_style;bool normal_wrap;
    uint32_t fg,bg,unicode;unsigned need,minimum;
    unsigned parameters[16],parameter_count,control_bytes;
    uint8_t style,parser;
    bool private_mode,bad_control,wrap,cursor,alt,application_cursor;
    uint64_t sequence,unsupported;
    ae_term_width width;
};
/* One owner thread. Allocate on the heap; history and both screens are bounded.
   width returns 0, 1 or 2 cells (negative is replaced). No shaping/bidi/reflow
   or terminal-command side effects are supplied by this model. */
void ae_terminal_init(struct ae_terminal *model,unsigned cols,unsigned rows,ae_term_width width);
bool ae_terminal_resize(struct ae_terminal *model,unsigned cols,unsigned rows);
void ae_terminal_feed(struct ae_terminal *model,const void *bytes,size_t count);
const struct ae_term_cell *ae_terminal_row(const struct ae_terminal *model,unsigned row);
void ae_terminal_scroll(struct ae_terminal *model,int lines);
#endif
