#include "model.h"
#include <string.h>

static bool utf8(const char *s, size_t limit) {
    size_t used = 0;
    const unsigned char *p = (const unsigned char *)s;
    if (!s) return false;
    while (*p && used < limit) {
        unsigned c = *p;
        size_t n = c < 0x80 ? 1u : c >= 0xc2 && c <= 0xdf ? 2u : c >= 0xe0 && c <= 0xef ? 3u : c >= 0xf0 && c <= 0xf4 ? 4u : 0u;
        if (!n || used + n > limit || c < 0x20 || c == 0x7f) return false;
        for (size_t i = 1; i < n; ++i) if (p[i] < 0x80 || p[i] > 0xbf) return false;
        if (n >= 3 && ((c == 0xe0 && p[1] < 0xa0) || (c == 0xed && p[1] >= 0xa0))) return false;
        if (n == 4 && ((c == 0xf0 && p[1] < 0x90) || (c == 0xf4 && p[1] >= 0x90))) return false;
        used += n; p += n;
    }
    return *p == 0;
}
static unsigned lower(unsigned char c) { return c >= 'A' && c <= 'Z' ? (unsigned)c + ('a' - 'A') : c; }
static bool matches(const char *label, const char *query) {
    if (!*query) return true;
    for (; *label; ++label) {
        const unsigned char *a = (const unsigned char *)label, *b = (const unsigned char *)query;
        while (*a && *b && lower(*a) == lower(*b)) { ++a; ++b; }
        if (!*b) return true;
    }
    return false;
}
static bool valid_app(enum ae_desktop_app app) { return app>AE_DESKTOP_APP_NONE && app<AE_DESKTOP_APP_END; }
const char *ae_desktop_app_label(enum ae_desktop_app app, bool german) {
    return app==AE_DESKTOP_SETTINGS ? german ? "Einstellungen" : "Settings" : app==AE_DESKTOP_TERMINAL ? "Terminal" : app==AE_DESKTOP_FILES ? german ? "Dateien" : "Files" : "";
}
const char *ae_desktop_app_id(enum ae_desktop_app app) {
    return app==AE_DESKTOP_SETTINGS ? "org.aetheri.Settings" : app==AE_DESKTOP_TERMINAL ? "org.aetheri.Terminal" : app==AE_DESKTOP_FILES ? "org.aetheri.Files" : "";
}
char ae_desktop_app_selector(enum ae_desktop_app app) { return app==AE_DESKTOP_SETTINGS ? 'S' : app==AE_DESKTOP_TERMINAL ? 'T' : app==AE_DESKTOP_FILES ? 'F' : 0; }
enum ae_desktop_app ae_desktop_app_from_selector(char s) { return s=='S' ? AE_DESKTOP_SETTINGS : s=='T' ? AE_DESKTOP_TERMINAL : s=='F' ? AE_DESKTOP_FILES : AE_DESKTOP_APP_NONE; }
bool ae_desktop_model_pending(const struct ae_desktop_model *m) { for (enum ae_desktop_app app=AE_DESKTOP_SETTINGS;app<AE_DESKTOP_APP_END;++app) if (m->pending[app]) return true;return false; }
size_t ae_desktop_model_results(const struct ae_desktop_model *m, struct ae_desktop_result out[AE_DESKTOP_RESULTS]) {
    size_t n = 0;
    for (enum ae_desktop_app app=AE_DESKTOP_SETTINGS;app<AE_DESKTOP_APP_END;++app)
        if (matches(ae_desktop_app_label(app,false),m->query) || matches(ae_desktop_app_label(app,true),m->query))
            out[n++] = (struct ae_desktop_result){.app=app,.label=ae_desktop_app_label(app,m->german)};
    for (size_t i = 0; i < m->count; ++i) {
        const struct ae_desktop_window *w = &m->windows[i];
        if (matches(w->title, m->query) || matches(w->app_id, m->query))
            out[n++] = (struct ae_desktop_result){.window=w->id,.label=*w->title ? w->title : m->german ? "Unbenanntes Fenster" : "Untitled window"};
    }
    return n;
}
static void selection(struct ae_desktop_model *m) {
    struct ae_desktop_result out[AE_DESKTOP_RESULTS];
    size_t n = ae_desktop_model_results(m, out);
    for (size_t i = 0; i < n; ++i) if (out[i].window == m->selected && out[i].app==m->selected_app) return;
    m->selected = n ? out[0].window : 0;
    m->selected_app=n ? out[0].app : AE_DESKTOP_APP_NONE;
}
void ae_desktop_model_init(struct ae_desktop_model *m) { memset(m, 0, sizeof(*m));m->selected_app=AE_DESKTOP_SETTINGS; }
bool ae_desktop_model_window(struct ae_desktop_model *m, uint32_t id, const char *title, const char *app_id, bool active) {
    if (!id || !utf8(title, 256) || !utf8(app_id, 256)) return false;
    size_t i = 0; while (i < m->count && m->windows[i].id != id) ++i;
    if (i == AE_DESKTOP_WINDOWS) return false;
    if (i == m->count) ++m->count;
    m->windows[i].id = id; m->windows[i].active = active;
    memcpy(m->windows[i].title, title, strlen(title) + 1);
    memcpy(m->windows[i].app_id, app_id, strlen(app_id) + 1);
    selection(m); return true;
}
void ae_desktop_model_closed(struct ae_desktop_model *m, uint32_t id) {
    for (size_t i = 0; i < m->count; ++i) if (m->windows[i].id == id) {
        memmove(&m->windows[i], &m->windows[i+1], (m->count-i-1) * sizeof(m->windows[0])); --m->count; break;
    }
    selection(m);
}
bool ae_desktop_model_text(struct ae_desktop_model *m, const char *text) {
    if (!m->search || !utf8(text, AE_DESKTOP_QUERY-1)) return false;
    size_t used = strlen(m->query), added = strlen(text);
    if (used + added >= sizeof(m->query)) return false;
    memcpy(m->query+used, text, added+1); selection(m); return true;
}
void ae_desktop_model_backspace(struct ae_desktop_model *m) {
    size_t n = strlen(m->query);
    if (!n) return;
    --n; while (n && ((unsigned char)m->query[n] & 0xc0) == 0x80) --n;
    m->query[n] = 0; selection(m);
}
void ae_desktop_model_select(struct ae_desktop_model *m, int delta) {
    struct ae_desktop_result out[AE_DESKTOP_RESULTS]; size_t n = ae_desktop_model_results(m, out), i = 0;
    if (!n) return;
    while (i < n && (out[i].window != m->selected || out[i].app!=m->selected_app)) ++i;
    if (i == n) i = 0;
    else if (delta > 0) i = (i+1) % n;
    else i = i ? i-1 : n-1;
    m->selected = out[i].window;
    m->selected_app=out[i].app;
}
void ae_desktop_model_search(struct ae_desktop_model *m, unsigned output, bool open) {
    m->search = open; m->output = output;
    if (open) { m->query[0] = 0; m->selected = 0;m->selected_app=AE_DESKTOP_SETTINGS; }
}
bool ae_desktop_model_request(struct ae_desktop_model *m,enum ae_desktop_app app) {
    if (!valid_app(app) || m->pending[app]) return false;
    m->pending[app] = true; m->launch_error[app] = false; return true;
}
bool ae_desktop_model_reply(struct ae_desktop_model *m,enum ae_desktop_app app,char reply) {
    if (!valid_app(app)) return false;
    if (reply == 'X') { m->process[app] = false; return true; }
    if ((reply != 'A' && reply != 'E') || !m->pending[app]) return false;
    m->pending[app] = false; m->process[app] = reply == 'A'; m->launch_error[app] = reply == 'E'; return true;
}
