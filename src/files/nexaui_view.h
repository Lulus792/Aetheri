#ifndef AETHERI_FILES_NEXAUI_VIEW_H
#define AETHERI_FILES_NEXAUI_VIEW_H
#include "session.h"
#include <nexaui/nexaui.h>

struct ae_files_view;
enum ae_files_control {
    AE_FV_UP,AE_FV_LIST,AE_FV_GRID,AE_FV_NEW_FILE,AE_FV_NEW_FOLDER,AE_FV_SEARCH,
    AE_FV_DETAILS,AE_FV_MORE,AE_FV_HIDDEN,AE_FV_OPEN,AE_FV_CUT,AE_FV_PASTE,
    AE_FV_RENAME,AE_FV_CANCEL,AE_FV_CONFIRM,AE_FV_NAME,AE_FV_INFO_CLOSE,
    AE_FV_CLOSE,AE_FV_ZOOM,AE_FV_CONTROLS
};
/* Session/model and immutable font bytes outlive the view, including pending
   work and native publications. View IDs are local observations, never grants. */
bool ae_files_view_create(struct ae_files_session *,const void *,size_t,struct ae_files_view **);
void ae_files_view_destroy(struct ae_files_view *);
nx_context *ae_files_view_context(struct ae_files_view *);
/* One native event then one frame preserves pointer/command ordering. Native
   accessibility actions route through the same context; at most one UI intent
   is applied per frame. Call the controller's completion updates before frame. */
bool ae_files_view_event(struct ae_files_view *,const nx_event *);
bool ae_files_view_frame(struct ae_files_view *,float,float,const nx_window_status *,bool *changed);
/* IDs from the most recently drawn scope. Zero when absent/stale. */
uint64_t ae_files_view_control_id(const struct ae_files_view *,enum ae_files_control);
uint64_t ae_files_view_entry_id(const struct ae_files_view *,size_t visible);
/* Consumed once; 1 means work-area maximize/restore, never fullscreen. */
bool ae_files_view_take_zoom(struct ae_files_view *);
bool ae_files_view_native_hit(const struct ae_files_view *,nx_point,const nx_window_status *,
    bool *title,nx_window_resize_edge *edge,nx_cursor_shape *cursor);
#endif
