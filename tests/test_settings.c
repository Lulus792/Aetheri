#include "settings/model.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "line %d: %s\n", __LINE__, #x); exit(1); } } while (0)
int main(void) {
    struct ae_settings m;
    ae_settings_init(&m);
    struct ae_settings_display d[] = {
        {.id=1,.name="First",.width=1920,.height=1080,.scale=1,.refresh=60,.mode_known=true},
        {.id=2,.name="Second",.width=3840,.height=2160,.scale=2,.refresh=0,.mode_known=true}
    };
    CHECK(ae_settings_snapshot(&m,d,2) && m.count==2 && m.refreshes==1);
    CHECK(ae_settings_select(&m,2) && m.selected==1);
    struct ae_settings_display swap[] = {d[1],d[0]};
    CHECK(ae_settings_snapshot(&m,swap,2) && m.selected==0);
    CHECK(!ae_settings_select(&m,3) && m.selected==0);
    d[0].id=2;
    CHECK(!ae_settings_snapshot(&m,d,2) && m.stale && m.count==2 && m.refreshes==2);
    CHECK(!ae_settings_select(&m,1));
    CHECK(ae_settings_snapshot(&m,m.displays,2) && !m.stale && m.selected==0);
    d[0].id=1; d[0].scale=NAN;
    CHECK(!ae_settings_snapshot(&m,d,2));
    d[0].scale=1; memset(d[0].name,'a',sizeof(d[0].name));
    CHECK(!ae_settings_snapshot(&m,d,2));
    CHECK(!ae_settings_snapshot(&m,NULL,1));
    CHECK(!ae_settings_snapshot(&m,d,AE_SETTINGS_DISPLAYS+1));
    CHECK(ae_settings_snapshot(&m,swap+1,1) && m.selected==0 && m.displays[0].id==1);
    CHECK(ae_settings_snapshot(&m,NULL,0) && m.count==0 && !m.stale);
    puts("settings snapshot/selection/stale/removal checks passed");
    return 0;
}
