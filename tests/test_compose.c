#include "compositor/compose.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xkbcommon/xkbcommon-keysyms.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"compose %d: %s\n",__LINE__,#x);exit(1); } } while (0)
int main(void) {
    struct ae_compose *c=ae_compose_create();CHECK(c);struct ae_compose_result r;
    CHECK(ae_compose_feed(c,XKB_KEY_a,&r) && r.kind==AE_COMPOSE_NONE && !r.consumed);
    CHECK(ae_compose_feed(c,XKB_KEY_dead_acute,&r) && r.kind==AE_COMPOSE_EDIT && !strcmp(r.text,"´") && ae_compose_active(c));
    CHECK(ae_compose_feed(c,XKB_KEY_Shift_L,&r) && !r.consumed && ae_compose_active(c));
    CHECK(ae_compose_feed(c,XKB_KEY_e,&r) && r.kind==AE_COMPOSE_COMMIT && !strcmp(r.text,"é") && !ae_compose_active(c));
    CHECK(ae_compose_feed(c,XKB_KEY_Menu,&r) && r.kind==AE_COMPOSE_EDIT && !strcmp(r.text,"·"));
    CHECK(ae_compose_feed(c,XKB_KEY_quotedbl,&r) && r.kind==AE_COMPOSE_EDIT);
    CHECK(ae_compose_feed(c,XKB_KEY_a,&r) && r.kind==AE_COMPOSE_COMMIT && !strcmp(r.text,"ä"));
    CHECK(ae_compose_feed(c,XKB_KEY_Multi_key,&r) && r.kind==AE_COMPOSE_EDIT);
    CHECK(ae_compose_feed(c,XKB_KEY_apostrophe,&r) && r.kind==AE_COMPOSE_EDIT);
    CHECK(ae_compose_feed(c,XKB_KEY_j,&r) && r.kind==AE_COMPOSE_COMMIT && r.size==3 && !strcmp(r.text,"j́"));
    CHECK(ae_compose_feed(c,XKB_KEY_dead_acute,&r));ae_compose_reset(c);CHECK(!ae_compose_active(c));
    CHECK(ae_compose_feed(c,XKB_KEY_e,&r) && r.kind==AE_COMPOSE_NONE && !r.consumed);
    CHECK(ae_compose_feed(c,XKB_KEY_Multi_key,&r) && r.consumed);
    CHECK(ae_compose_feed(c,XKB_KEY_Escape,&r) && r.kind==AE_COMPOSE_CANCEL && r.consumed);
    for (unsigned i=0;i<1000;++i) { CHECK(ae_compose_feed(c,XKB_KEY_dead_circumflex,&r));if (r.kind!=AE_COMPOSE_EDIT) ae_compose_reset(c); }
    ae_compose_destroy(c);puts("compose: embedded standard table, ordinary keys, dead/Compose Unicode, ignored modifiers, explicit reset and bounded cancellation passed");return 0;
}
