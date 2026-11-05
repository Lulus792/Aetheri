#ifndef AETHERI_DESKTOP_MODEL_H
#define AETHERI_DESKTOP_MODEL_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
enum { AE_DESKTOP_WINDOWS = 64, AE_DESKTOP_QUERY = 256 };
struct ae_desktop_window {
    uint32_t id;
    char title[257], app_id[257];
    bool active;
};
struct ae_desktop_result { uint32_t window; const char *label; };
struct ae_desktop_model {
    struct ae_desktop_window windows[AE_DESKTOP_WINDOWS];
    size_t count;
    char query[AE_DESKTOP_QUERY];
    uint32_t selected;
    unsigned output;
    bool search, german, contrast, pending, process, launch_error;
};
void ae_desktop_model_init(struct ae_desktop_model *model);
bool ae_desktop_model_window(struct ae_desktop_model *model, uint32_t id, const char *title, const char *app_id, bool active);
void ae_desktop_model_closed(struct ae_desktop_model *model, uint32_t id);
/* Labels are borrowed from this model or literals; consume before mutation. */
size_t ae_desktop_model_results(const struct ae_desktop_model *model, struct ae_desktop_result results[AE_DESKTOP_WINDOWS + 1]);
bool ae_desktop_model_text(struct ae_desktop_model *model, const char *text);
void ae_desktop_model_backspace(struct ae_desktop_model *model);
void ae_desktop_model_select(struct ae_desktop_model *model, int delta);
void ae_desktop_model_search(struct ae_desktop_model *model, unsigned output, bool open);
bool ae_desktop_model_request(struct ae_desktop_model *model);
bool ae_desktop_model_reply(struct ae_desktop_model *model, char reply);
#endif
