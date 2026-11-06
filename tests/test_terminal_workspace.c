#include "terminal/workspace.h"
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do {if(!(x)) {fprintf(stderr,"workspace %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static unsigned node(const struct ae_terminal_workspace *w,unsigned slot) {
    for(unsigned i=0;i<AE_TERM_NODES;++i) if(w->nodes[i].kind==AE_PART_LEAF && w->nodes[i].slot==slot) return i;
    return AE_TERM_NODES;
}
static void geometry(const struct ae_terminal_workspace *w) {
    struct ae_terminal_area body={10,20,2000,2000};struct ae_terminal_workspace_layout result;
    CHECK(ae_terminal_workspace_layout(w,body,40,30,4,&result));
    unsigned slots[AE_TERM_PANES],count=ae_terminal_workspace_order(w,slots);CHECK(count>=1 && count<=8);
    unsigned mask=0;
    for(unsigned i=0;i<count;++i) {
        unsigned slot=slots[i];CHECK(slot<AE_TERM_PANES && !(mask&(1u<<slot)));mask|=1u<<slot;
        struct ae_terminal_area r=result.panes[slot];CHECK(r.w>=39.99f && r.h>=29.99f);
        CHECK(r.x>=body.x && r.y>=body.y && r.x+r.w<=body.x+body.w+.001f && r.y+r.h<=body.y+body.h+.001f);
        for(unsigned j=0;j<i;++j) {
            struct ae_terminal_area s=result.panes[slots[j]];
            CHECK(r.x+r.w<=s.x || s.x+s.w<=r.x || r.y+r.h<=s.y || s.y+s.h<=r.y);
        }
    }
    CHECK(result.visible==mask);
}
int main(void) {
    struct ae_terminal_workspace w;ae_terminal_workspace_init(&w);CHECK(ae_terminal_workspace_valid(&w));
    CHECK(ae_terminal_workspace_focused(&w)==AE_TERM_PANES && !ae_terminal_workspace_split(&w,0,true));
    CHECK(ae_terminal_workspace_tab(&w,0));uint64_t first=w.nodes[node(&w,0)].id;
    CHECK(ae_terminal_workspace_split(&w,1,true) && w.nodes[node(&w,0)].id==first);
    unsigned root=w.tabs[0].root;CHECK(ae_terminal_workspace_split(&w,2,false));
    CHECK(ae_terminal_workspace_focused(&w)==2 && ae_terminal_workspace_ratio(&w,root,.1f));
    struct ae_terminal_workspace before;memcpy(&before,&w,sizeof(w));
    struct ae_terminal_workspace_layout layout;
    CHECK(ae_terminal_workspace_layout(&w,(struct ae_terminal_area){0,0,640,400},200,80,4,&layout));
    CHECK(fabsf(layout.panes[0].w-200)<.01f && !memcmp(&before,&w,sizeof(w)));
    unsigned order[8];CHECK(ae_terminal_workspace_order(&w,order)==3 && order[0]==0 && order[1]==1 && order[2]==2);
    CHECK(!ae_terminal_workspace_tab(&w,2) && !ae_terminal_workspace_split(&w,1,false) && !ae_terminal_workspace_focus(&w,7));
    CHECK(!ae_terminal_workspace_ratio(&w,root,NAN) && !ae_terminal_workspace_ratio(&w,root,.99f) && !memcmp(&before,&w,sizeof(w)));
    memset(&layout,0x55,sizeof(layout));struct ae_terminal_workspace_layout untouched;memcpy(&untouched,&layout,sizeof(layout));
    CHECK(!ae_terminal_workspace_layout(&w,(struct ae_terminal_area){0,0,403,400},200,80,4,&layout) && !memcmp(&layout,&untouched,sizeof(layout)));
    CHECK(!ae_terminal_workspace_layout(&w,(struct ae_terminal_area){FLT_MAX,0,FLT_MAX,400},200,80,4,&layout));
    CHECK(ae_terminal_workspace_tab(&w,3) && ae_terminal_workspace_focused(&w)==3);
    CHECK(!ae_terminal_workspace_focus(&w,0) && ae_terminal_workspace_select(&w,0) && ae_terminal_workspace_focused(&w)==2);
    CHECK(ae_terminal_workspace_remove(&w,1));geometry(&w);CHECK(w.nodes[node(&w,0)].id==first);
    uint64_t previous=w.serial;CHECK(ae_terminal_workspace_split(&w,1,false) && w.nodes[node(&w,1)].id>previous);
    for(unsigned slot=4;slot<8;++slot) {CHECK(ae_terminal_workspace_split(&w,slot,(slot&1u)!=0));geometry(&w);}
    CHECK(!ae_terminal_workspace_tab(&w,8) && !ae_terminal_workspace_split(&w,8,true));
    const unsigned removal[]={0,7,2,5,3,1,6,4};
    for(unsigned i=0;i<8;++i) {uint64_t serial=w.serial;CHECK(ae_terminal_workspace_remove(&w,removal[i]) && ae_terminal_workspace_valid(&w) && w.serial==serial);if(w.count) geometry(&w);}
    CHECK(!w.count && !ae_terminal_workspace_remove(&w,0));previous=w.serial;
    for(unsigned i=0;i<8;++i) CHECK(ae_terminal_workspace_tab(&w,i));
    CHECK(w.count==8 && w.nodes[node(&w,0)].id>previous && !ae_terminal_workspace_tab(&w,0));
    for(unsigned i=0;i<8;++i) {CHECK(ae_terminal_workspace_select(&w,i) && ae_terminal_workspace_focused(&w)==i);geometry(&w);}
    struct ae_terminal_workspace corrupt=w;corrupt.nodes[corrupt.tabs[0].root].slot=1;CHECK(!ae_terminal_workspace_valid(&corrupt));
    corrupt=w;corrupt.nodes[corrupt.tabs[1].root].id=corrupt.tabs[0].id;CHECK(!ae_terminal_workspace_valid(&corrupt));
    corrupt=w;corrupt.nodes[corrupt.tabs[0].root].kind=AE_PART_COLUMNS;corrupt.nodes[corrupt.tabs[0].root].first=corrupt.tabs[0].root;CHECK(!ae_terminal_workspace_valid(&corrupt));
    w.serial=UINT64_MAX;memcpy(&before,&w,sizeof(w));CHECK(!ae_terminal_workspace_split(&w,0,true) && !memcmp(&before,&w,sizeof(w)));
    for(unsigned repeat=0;repeat<200;++repeat) {
        ae_terminal_workspace_init(&w);CHECK(ae_terminal_workspace_tab(&w,0));
        for(unsigned slot=1;slot<8;++slot) CHECK(ae_terminal_workspace_split(&w,slot,(slot+repeat)%2!=0));
        geometry(&w);
        for(unsigned i=0;i<8;++i) CHECK(ae_terminal_workspace_remove(&w,(i+repeat)%8) && ae_terminal_workspace_valid(&w));
    }
    puts("workspace: bounded tab/split identities, focus, arbitrary removal, remembered minimum-constrained ratios, nonoverlap, malformed graphs and atomic limits passed");return 0;
}
