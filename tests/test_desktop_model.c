#include "desktop/model.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"%d: %s\n",__LINE__,#x);exit(1); } } while (0)
int main(void) {
    struct ae_desktop_model m;struct ae_desktop_result out[AE_DESKTOP_RESULTS];ae_desktop_model_init(&m);
    CHECK(!ae_desktop_model_text(&m,"x"));ae_desktop_model_search(&m,2,true);
    CHECK(ae_desktop_model_results(&m,out)==2 && !out[0].window && out[0].app==AE_DESKTOP_SETTINGS && out[1].app==AE_DESKTOP_TERMINAL);
    CHECK(ae_desktop_model_window(&m,17,"Grüße","example.editor",false));
    CHECK(ae_desktop_model_window(&m,18,"Second window","example.other",true));
    ae_desktop_model_select(&m,1);CHECK(!m.selected && m.selected_app==AE_DESKTOP_TERMINAL);
    ae_desktop_model_select(&m,1);CHECK(m.selected==17 && m.selected_app==AE_DESKTOP_APP_NONE);
    CHECK(ae_desktop_model_window(&m,17,"Updated Grüße","example.editor",true) && m.selected==17 && m.count==2);
    CHECK(ae_desktop_model_text(&m,"grü"));CHECK(ae_desktop_model_results(&m,out)==1 && out[0].window==17);
    ae_desktop_model_backspace(&m);CHECK(!strcmp(m.query,"gr"));
    CHECK(!ae_desktop_model_text(&m,"\xc0\x80") && !strcmp(m.query,"gr"));
    CHECK(!ae_desktop_model_text(&m,"\xf4\x90\x80\x80"));
    CHECK(!ae_desktop_model_window(&m,19,"bad\nname","x",false));
    ae_desktop_model_closed(&m,17);CHECK(m.count==1 && ae_desktop_model_results(&m,out)==0);
    ae_desktop_model_search(&m,0,true);CHECK(m.query[0]==0 && m.selected==0);
    CHECK(!ae_desktop_model_request(&m,AE_DESKTOP_APP_NONE) && !ae_desktop_model_request(&m,AE_DESKTOP_APP_END));
    CHECK(ae_desktop_model_request(&m,AE_DESKTOP_SETTINGS));CHECK(!ae_desktop_model_request(&m,AE_DESKTOP_SETTINGS));
    CHECK(m.pending[AE_DESKTOP_SETTINGS] && !ae_desktop_model_reply(&m,AE_DESKTOP_SETTINGS,'?') && m.pending[AE_DESKTOP_SETTINGS]);
    CHECK(!ae_desktop_model_reply(&m,AE_DESKTOP_TERMINAL,'A') && m.pending[AE_DESKTOP_SETTINGS]);
    CHECK(ae_desktop_model_request(&m,AE_DESKTOP_TERMINAL));
    CHECK(ae_desktop_model_reply(&m,AE_DESKTOP_SETTINGS,'A') && m.process[AE_DESKTOP_SETTINGS] && !m.pending[AE_DESKTOP_SETTINGS] && m.pending[AE_DESKTOP_TERMINAL]);
    CHECK(!ae_desktop_model_reply(&m,AE_DESKTOP_SETTINGS,'A'));CHECK(ae_desktop_model_request(&m,AE_DESKTOP_SETTINGS));
    CHECK(ae_desktop_model_reply(&m,AE_DESKTOP_SETTINGS,'X') && m.pending[AE_DESKTOP_SETTINGS] && !m.process[AE_DESKTOP_SETTINGS] && m.pending[AE_DESKTOP_TERMINAL]);
    CHECK(ae_desktop_model_reply(&m,AE_DESKTOP_SETTINGS,'E') && !m.pending[AE_DESKTOP_SETTINGS] && m.launch_error[AE_DESKTOP_SETTINGS]);
    CHECK(ae_desktop_model_request(&m,AE_DESKTOP_SETTINGS) && !m.launch_error[AE_DESKTOP_SETTINGS]);
    CHECK(ae_desktop_model_reply(&m,AE_DESKTOP_TERMINAL,'A') && m.process[AE_DESKTOP_TERMINAL]);
    CHECK(ae_desktop_model_reply(&m,AE_DESKTOP_SETTINGS,'X') && m.process[AE_DESKTOP_TERMINAL]);
    ae_desktop_model_init(&m);
    for (uint32_t i=1;i<=AE_DESKTOP_WINDOWS;++i) CHECK(ae_desktop_model_window(&m,i,"Window","id",false));
    CHECK(!ae_desktop_model_window(&m,65,"Overflow","id",false));CHECK(m.count==64);
    CHECK(ae_desktop_model_results(&m,out)==AE_DESKTOP_RESULTS);
    ae_desktop_model_closed(&m,32);CHECK(m.count==63 && ae_desktop_model_window(&m,65,"New","id",false));
    ae_desktop_model_search(&m,0,true);char full[256];memset(full,'a',255);full[255]=0;
    CHECK(ae_desktop_model_text(&m,full));CHECK(!ae_desktop_model_text(&m,"b") && strlen(m.query)==255);
    puts("desktop model: selection, UTF-8 bounds, stable IDs, bounded windows and no uncertain-request replay passed");return 0;
}
