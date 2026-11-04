#include "model.h"
#include <limits.h>
#include <math.h>
#include <string.h>

void ae_settings_init(struct ae_settings *model) {
    *model = (struct ae_settings){.theme = AE_SETTINGS_DARK, .text_percent = 100};
}
bool ae_settings_snapshot(struct ae_settings *model, const struct ae_settings_display *displays, size_t count) {
    if (!model) return false;
    if (count > AE_SETTINGS_DISPLAYS || (count && !displays) || model->refreshes == UINT_MAX) goto fail;
    for (size_t i = 0; i < count; ++i) {
        const struct ae_settings_display *d = &displays[i];
        if (!d->id || !memchr(d->name, 0, sizeof(d->name)) || !d->name[0] ||
                !isfinite(d->scale) || d->scale <= 0 || !isfinite(d->refresh) || d->refresh < 0 ||
                (d->mode_known && (d->width <= 0 || d->height <= 0))) goto fail;
        for (size_t j = 0; j < i; ++j) if (displays[j].id == d->id) goto fail;
    }
    uint32_t selected = model->selected < model->count ? model->displays[model->selected].id : 0;
    /* The input may refer to our own current records. Copy before clearing. */
    struct ae_settings_display copy[AE_SETTINGS_DISPLAYS] = {0};
    if (count) memcpy(copy, displays, count * sizeof(*copy));
    memcpy(model->displays, copy, sizeof(copy));
    model->count = count; model->selected = 0;
    for (size_t i = 0; i < count; ++i) if (copy[i].id == selected) model->selected = i;
    model->stale = false; ++model->refreshes;
    return true;
fail:
    model->stale = true;
    return false;
}
bool ae_settings_select(struct ae_settings *model, uint32_t id) {
    if (!model || model->stale) return false;
    for (size_t i = 0; i < model->count; ++i) if (model->displays[i].id == id) {
        model->selected = i; return true;
    }
    return false;
}
