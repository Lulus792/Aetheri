#ifndef AETHERI_COMPOSITOR_COMPOSE_H
#define AETHERI_COMPOSITOR_COMPOSE_H
#include <stdbool.h>
#include <stddef.h>
#include <xkbcommon/xkbcommon.h>
struct ae_compose;
enum ae_compose_kind { AE_COMPOSE_NONE,AE_COMPOSE_EDIT,AE_COMPOSE_COMMIT,AE_COMPOSE_CANCEL };
struct ae_compose_result { enum ae_compose_kind kind;const char *text;size_t size;bool consumed; };
/* One owner thread. Uses only build-embedded reviewed data; no environment or
   runtime table lookup. Returned NUL-terminated text is borrowed until the next
   feed/reset/destroy. Nothing here writes native protocol or keyboard events. */
struct ae_compose *ae_compose_create(void);
void ae_compose_destroy(struct ae_compose *compose);
void ae_compose_reset(struct ae_compose *compose);
bool ae_compose_active(const struct ae_compose *compose);
bool ae_compose_feed(struct ae_compose *compose,xkb_keysym_t symbol,struct ae_compose_result *out);
#endif
