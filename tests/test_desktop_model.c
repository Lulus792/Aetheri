#include "desktop/model.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"%d: %s\n",__LINE__,#x);exit(1); } } while (0)
int main(void) {
    struct ae_desktop_model m;struct ae_desktop_result out[AE_DESKTOP_WINDOWS+1];ae_desktop_model_init(&m);
    CHECK(!ae_desktop_model_text(&m,"x"));ae_desktop_model_search(&m,2,true);
    CHECK(ae_desktop_model_results(&m,out)==1 && !out[0].window);
    CHECK(ae_desktop_model_window(&m,17,"Grüße","example.editor",false));
    CHECK(ae_desktop_model_window(&m,18,"Second window","example.other",true));
    ae_desktop_model_select(&m,1);CHECK(m.selected==17);
    CHECK(ae_desktop_model_window(&m,17,"Updated Grüße","example.editor",true) && m.selected==17 && m.count==2);
    CHECK(ae_desktop_model_text(&m,"grü"));CHECK(ae_desktop_model_results(&m,out)==1 && out[0].window==17);
    ae_desktop_model_backspace(&m);CHECK(!strcmp(m.query,"gr"));
    CHECK(!ae_desktop_model_text(&m,"\xc0\x80") && !strcmp(m.query,"gr"));
    CHECK(!ae_desktop_model_text(&m,"\xf4\x90\x80\x80"));
    CHECK(!ae_desktop_model_window(&m,19,"bad\nname","x",false));
    ae_desktop_model_closed(&m,17);CHECK(m.count==1 && ae_desktop_model_results(&m,out)==0);
    ae_desktop_model_search(&m,0,true);CHECK(m.query[0]==0 && m.selected==0);
    CHECK(ae_desktop_model_request(&m));CHECK(!ae_desktop_model_request(&m));
    CHECK(m.pending && !ae_desktop_model_reply(&m,'?') && m.pending);
    CHECK(ae_desktop_model_reply(&m,'A') && m.process && !m.pending);
    CHECK(!ae_desktop_model_reply(&m,'A'));CHECK(ae_desktop_model_request(&m));
    CHECK(ae_desktop_model_reply(&m,'X') && m.pending && !m.process);
    CHECK(ae_desktop_model_reply(&m,'E') && !m.pending && m.launch_error);
    CHECK(ae_desktop_model_request(&m) && !m.launch_error);
    ae_desktop_model_init(&m);
    for (uint32_t i=1;i<=AE_DESKTOP_WINDOWS;++i) CHECK(ae_desktop_model_window(&m,i,"Window","id",false));
    CHECK(!ae_desktop_model_window(&m,65,"Overflow","id",false));CHECK(m.count==64);
    ae_desktop_model_closed(&m,32);CHECK(m.count==63 && ae_desktop_model_window(&m,65,"New","id",false));
    ae_desktop_model_search(&m,0,true);char full[256];memset(full,'a',255);full[255]=0;
    CHECK(ae_desktop_model_text(&m,full));CHECK(!ae_desktop_model_text(&m,"b") && strlen(m.query)==255);
    puts("desktop model: selection, UTF-8 bounds, stable IDs, bounded windows and no uncertain-request replay passed");return 0;
}
