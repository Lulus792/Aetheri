#ifndef AETHERI_SETTINGS_MODEL_H
#define AETHERI_SETTINGS_MODEL_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Transient display observations, never saved device identities or authority. */
#define AE_SETTINGS_DISPLAYS 16
struct ae_settings_display {
    uint32_t id;
    char name[128];
    int width, height;
    float scale, refresh;
    bool mode_known;
};
enum ae_settings_page { AE_SETTINGS_DISPLAY, AE_SETTINGS_APPEARANCE, AE_SETTINGS_LANGUAGE };
enum ae_settings_theme { AE_SETTINGS_DARK, AE_SETTINGS_LIGHT, AE_SETTINGS_CONTRAST };
struct ae_settings {
    struct ae_settings_display displays[AE_SETTINGS_DISPLAYS];
    size_t count, selected;
    unsigned refreshes;
    bool stale, german;
    enum ae_settings_page page;
    enum ae_settings_theme theme;
    unsigned text_percent;
};
void ae_settings_init(struct ae_settings *model);
/* Failed snapshots preserve the previous observation and mark it stale. */
bool ae_settings_snapshot(struct ae_settings *model, const struct ae_settings_display *displays, size_t count);
bool ae_settings_select(struct ae_settings *model, uint32_t id);
#endif
