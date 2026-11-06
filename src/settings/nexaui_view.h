#ifndef AETHERI_SETTINGS_NEXAUI_VIEW_H
#define AETHERI_SETTINGS_NEXAUI_VIEW_H
#include "settings/model.h"
#include <nexaui/nexaui.h>

struct ae_settings_view;
struct ae_settings_chrome {
    nx_rect titlebar, title, close, minimize, zoom;
    float resize_margin;
};
enum ae_settings_hit_kind { AE_SETTINGS_HIT_NONE, AE_SETTINGS_HIT_TITLE, AE_SETTINGS_HIT_RESIZE };
struct ae_settings_hit {
    enum ae_settings_hit_kind kind;
    nx_window_resize_edge edge;
    nx_cursor_shape cursor;
};
/* Shared logical geometry for paint and native pointer requests. No side effects. */
bool ae_settings_chrome_layout(const struct ae_settings *model, float width, float height,
    struct ae_settings_chrome *out);
bool ae_settings_chrome_hit(const struct ae_settings *model, float width, float height,
    const nx_window_status *status, nx_point point, struct ae_settings_hit *out);
enum ae_settings_window_action {
    AE_SETTINGS_WINDOW_NONE, AE_SETTINGS_WINDOW_CLOSE,
    AE_SETTINGS_WINDOW_MINIMIZE, AE_SETTINGS_WINDOW_ZOOM
};
/* Model and immutable font bytes belong to the caller and outlive the view. */
bool ae_settings_view_create(struct ae_settings *model, const void *font,
    size_t length, struct ae_settings_view **out);
void ae_settings_view_destroy(struct ae_settings_view *view);
nx_context *ae_settings_view_context(struct ae_settings_view *view);
bool ae_settings_view_frame(struct ae_settings_view *view, float width, float height,
    const nx_window_status *status, bool *changed);
/* Consumed once by the original host; a request is not observed window state. */
enum ae_settings_window_action ae_settings_view_take_action(struct ae_settings_view *view);
bool ae_settings_view_take_refresh(struct ae_settings_view *view);
#endif
