#ifndef AETHERI_FILES_LAYOUT_H
#define AETHERI_FILES_LAYOUT_H
#include <stdbool.h>
#include <stddef.h>
struct ae_files_rect { float x,y,w,h; };
struct ae_files_layout {
    struct ae_files_rect chrome,toolbar,sidebar,heading,content,inspector,status;
    struct ae_files_rect back,list,grid,new_folder,search,details,more,close,fullscreen;
    bool sidebar_visible,inspector_visible,compact,compact_cards;
    size_t columns,lines,capacity;float cell_width,cell_height,gap,row_gap,padding;
};
/* Logical geometry only; no SDL handles, filesystem effects or font ownership.
   Adapts the accepted studio's sidebar/toolbar/content/optional-info hierarchy.
   Small or enlarged-text windows collapse optional panes, never the main task. */
bool ae_files_layout_make(int width,int height,float scale,bool grid,bool details,bool sidebar,
    bool naming,struct ae_files_layout *out);
/* One full visible cell, relative to the first visible row (grid aligned). */
bool ae_files_layout_cell(const struct ae_files_layout *layout,size_t slot,struct ae_files_rect *out);
/* First visible index for a complete grid row containing selection. Count can
   include one not-yet-created naming placeholder. No model selection is changed.
   Invalid/empty inputs return zero; arbitrary stale first values are clamped. */
size_t ae_files_layout_reveal(const struct ae_files_layout *layout,size_t count,size_t selection,size_t first);
#endif
