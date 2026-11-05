#include "compose.h"
#include <stdlib.h>
#include <string.h>
#include <xkbcommon/xkbcommon-compose.h>
#include <xkbcommon/xkbcommon-keysyms.h>
extern const unsigned char ae_compose_data[];
extern const size_t ae_compose_data_size;
enum { TEXT_BYTES=1024,SEQUENCE_KEYS=32 };
struct ae_compose {
    struct xkb_context *context;struct xkb_compose_table *table;struct xkb_compose_state *state;
    char draft[TEXT_BYTES+1],result[TEXT_BYTES+1];size_t size;unsigned keys;
};
/* The reviewed table is valid UTF-8. Still validate the complete returned value
   and reject control/bidi formatting before exposing a possible commit. */
static bool safe_text(const char *text,size_t size) {
    const unsigned char *p=(const unsigned char *)text;
    for (size_t i=0;i<size;) {
        uint32_t code=p[i++],minimum=0;unsigned tail=0;
        if (code>=0xc2 && code<=0xdf) { code&=31;tail=1;minimum=0x80; }
        else if (code>=0xe0 && code<=0xef) { code&=15;tail=2;minimum=0x800; }
        else if (code>=0xf0 && code<=0xf4) { code&=7;tail=3;minimum=0x10000; }
        else if (code>=0x80) return false;
        if (tail>size-i) return false;
        for (unsigned j=0;j<tail;++j) { if (p[i]<0x80 || p[i]>0xbf) return false;code=(code<<6)|(p[i++]&63u); }
        if (code<minimum || code>0x10ffff || (code>=0xd800 && code<=0xdfff) || code<32 ||
            (code>=0x7f && code<=0x9f) || code==0x2028 || code==0x2029 ||
            (code>=0x202a && code<=0x202e) || (code>=0x2066 && code<=0x2069)) return false;
    }
    return true;
}
void ae_compose_reset(struct ae_compose *c) {
    if (!c) return;
    xkb_compose_state_reset(c->state);c->size=c->keys=0;c->draft[0]=c->result[0]=0;
}
void ae_compose_destroy(struct ae_compose *c) {
    if (!c) return;
    xkb_compose_state_unref(c->state);xkb_compose_table_unref(c->table);xkb_context_unref(c->context);free(c);
}
struct ae_compose *ae_compose_create(void) {
    struct ae_compose *c=calloc(1,sizeof(*c));if (!c) return NULL;
    c->context=xkb_context_new(XKB_CONTEXT_NO_ENVIRONMENT_NAMES);
    if (c->context) c->table=xkb_compose_table_new_from_buffer(c->context,(const char *)ae_compose_data,ae_compose_data_size,
        "C.UTF-8",XKB_COMPOSE_FORMAT_TEXT_V1,XKB_COMPOSE_COMPILE_NO_FLAGS);
    if (c->table) c->state=xkb_compose_state_new(c->table,XKB_COMPOSE_STATE_NO_FLAGS);
    if (!c->state) { ae_compose_destroy(c);return NULL; }
    return c;
}
bool ae_compose_active(const struct ae_compose *c) { return c && xkb_compose_state_get_status(c->state)==XKB_COMPOSE_COMPOSING; }
static const char *dead_text(xkb_keysym_t sym) {
    switch (sym) {
    case XKB_KEY_Multi_key:return "·";
    case XKB_KEY_dead_grave:return "`";
    case XKB_KEY_dead_acute:return "´";
    case XKB_KEY_dead_circumflex:return "^";
    case XKB_KEY_dead_tilde:return "~";
    case XKB_KEY_dead_macron:return "¯";
    case XKB_KEY_dead_breve:return "˘";
    case XKB_KEY_dead_abovedot:return "˙";
    case XKB_KEY_dead_diaeresis:return "¨";
    case XKB_KEY_dead_abovering:return "˚";
    case XKB_KEY_dead_doubleacute:return "˝";
    case XKB_KEY_dead_caron:return "ˇ";
    case XKB_KEY_dead_cedilla:return "¸";
    case XKB_KEY_dead_ogonek:return "˛";
    default:return NULL;
    }
}
bool ae_compose_feed(struct ae_compose *c,xkb_keysym_t symbol,struct ae_compose_result *out) {
    if (!c || !out) return false;
    *out=(struct ae_compose_result){0};c->result[0]=0;
    if (symbol==XKB_KEY_Menu) symbol=XKB_KEY_Multi_key; /* Explicit opt-in provider. */
    bool active=ae_compose_active(c);
    if (xkb_compose_state_feed(c->state,symbol)==XKB_COMPOSE_FEED_IGNORED) return true;
    enum xkb_compose_status status=xkb_compose_state_get_status(c->state);
    if (status==XKB_COMPOSE_NOTHING) return true;
    out->consumed=active || status==XKB_COMPOSE_COMPOSING || status==XKB_COMPOSE_COMPOSED;
    if (status==XKB_COMPOSE_COMPOSING && ++c->keys<=SEQUENCE_KEYS) {
        char scalar[8];const char *text=dead_text(symbol);
        if (!text) { int n=xkb_keysym_to_utf8(symbol,scalar,sizeof(scalar));text=n>1 && safe_text(scalar,(size_t)n-1) ? scalar : "?"; }
        size_t n=strlen(text);
        if (n<=TEXT_BYTES-c->size) {
            memcpy(c->draft+c->size,text,n+1);c->size+=n;
            *out=(struct ae_compose_result){AE_COMPOSE_EDIT,c->draft,c->size,true};return true;
        }
    } else if (status==XKB_COMPOSE_COMPOSED) {
        int n=xkb_compose_state_get_utf8(c->state,c->result,sizeof(c->result));
        if (n>0 && n<=TEXT_BYTES && safe_text(c->result,(size_t)n)) {
            xkb_compose_state_reset(c->state);c->size=c->keys=0;c->draft[0]=0;
            *out=(struct ae_compose_result){AE_COMPOSE_COMMIT,c->result,(size_t)n,true};return true;
        }
    }
    ae_compose_reset(c);out->kind=AE_COMPOSE_CANCEL;out->text="";return true;
}
