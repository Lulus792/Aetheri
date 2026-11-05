#include "files/layout.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"files layout %d: %s\n",__LINE__,#x);exit(1); } } while (0)
static size_t cases;
static bool contained(struct ae_files_rect parent,struct ae_files_rect child) {
    const float tolerance=.01f;
    return isfinite(child.x) && isfinite(child.y) && isfinite(child.w) && isfinite(child.h) && child.w>0 && child.h>0 &&
        child.x+tolerance>=parent.x && child.y+tolerance>=parent.y &&
        child.x+child.w<=parent.x+parent.w+tolerance && child.y+child.h<=parent.y+parent.h+tolerance;
}
static void check(int w,int h,float s,bool grid,bool details,bool sidebar,bool naming) {
    struct ae_files_layout l;CHECK(ae_files_layout_make(w,h,s,grid,details,sidebar,naming,&l));++cases;
    struct ae_files_rect window={0,0,(float)w,(float)h};
    CHECK(contained(window,l.chrome) && contained(window,l.toolbar) && contained(window,l.heading) && contained(window,l.content) && contained(window,l.status));
    CHECK(l.columns>=1 && l.lines>=1 && l.capacity==l.lines*l.columns);
    CHECK(grid || l.columns==1);
    CHECK(l.content.w>=120*s && l.content.h>=l.cell_height);
    CHECK(l.content.y+l.content.h<=l.status.y+.01f);
    const struct ae_files_rect controls[]={l.back,l.list,l.grid,l.new_folder,l.new_file,l.search,l.details};
    for (size_t i=0;i<sizeof(controls)/sizeof(*controls);++i) {
        CHECK(contained(l.toolbar,controls[i]));CHECK(controls[i].w>=28*s && controls[i].h>=32*s);
        if (i) CHECK(controls[i-1].x+controls[i-1].w<=controls[i].x+.01f);
    }
    CHECK(contained(l.chrome,l.more) && contained(l.chrome,l.close) && contained(l.chrome,l.fullscreen));
    if (l.sidebar_visible) { CHECK(sidebar && contained(window,l.sidebar));CHECK(l.sidebar.x+l.sidebar.w<l.content.x); }
    if (l.inspector_visible) { CHECK(details && contained(window,l.inspector));CHECK(l.content.x+l.content.w<l.inspector.x); }
    for (size_t i=0;i<l.capacity;++i) {
        struct ae_files_rect cell;CHECK(ae_files_layout_cell(&l,i,&cell) && contained(l.content,cell));
        if (i%l.columns) { struct ae_files_rect before;CHECK(ae_files_layout_cell(&l,i-1,&before));CHECK(before.x+before.w<cell.x); }
        if (i>=l.columns) { struct ae_files_rect above;CHECK(ae_files_layout_cell(&l,i-l.columns,&above));CHECK(above.y+above.h<cell.y); }
    }
    struct ae_files_rect sentinel={1,2,3,4},saved=sentinel;
    CHECK(!ae_files_layout_cell(&l,l.capacity,&sentinel) && !memcmp(&sentinel,&saved,sizeof(saved)));
    const size_t counts[]={1,2,17,4096,4097};
    for (size_t i=0;i<sizeof(counts)/sizeof(*counts);++i) {
        size_t count=counts[i],selection=count-1;
        size_t first=ae_files_layout_reveal(&l,count,selection,SIZE_MAX);
        CHECK(first%l.columns==0 && first<=selection && selection-first<l.capacity);
        CHECK(ae_files_layout_reveal(&l,count,selection,first)==first);
        CHECK(ae_files_layout_reveal(&l,count,0,first)==0);
        CHECK(ae_files_layout_reveal(&l,count,count,first)==0);
    }
}
int main(void) {
    const float scales[]={1,1.25f,1.5f,2};
    for (int w=640;w<=1600;w+=17) for (int h=420;h<=1000;h+=19)
        for (size_t i=0;i<sizeof(scales)/sizeof(*scales);++i) for (unsigned mode=0;mode<16;++mode)
            check(w,h,scales[i],(mode&1)!=0,(mode&2)!=0,(mode&4)!=0,(mode&8)!=0);
    const int large[][2]={{1920,1080},{2560,1440},{3840,2160},{16384,8192}};
    for (size_t i=0;i<sizeof(large)/sizeof(*large);++i) for (size_t j=0;j<sizeof(scales)/sizeof(*scales);++j)
        for (unsigned mode=0;mode<16;++mode) check(large[i][0],large[i][1],scales[j],(mode&1)!=0,(mode&2)!=0,(mode&4)!=0,(mode&8)!=0);
    struct ae_files_layout l={0},saved=l;
    const float invalid[]={NAN,INFINITY,-INFINITY,0,.99f,2.01f};
    for (size_t i=0;i<sizeof(invalid)/sizeof(*invalid);++i) CHECK(!ae_files_layout_make(1000,680,invalid[i],false,true,true,false,&l) && !memcmp(&l,&saved,sizeof(l)));
    CHECK(!ae_files_layout_make(639,420,1,false,true,true,false,&l));
    CHECK(!ae_files_layout_make(640,419,1,false,true,true,false,&l));
    CHECK(!ae_files_layout_make(16385,680,1,false,true,true,false,&l));
    CHECK(!ae_files_layout_make(1000,8193,1,false,true,true,false,&l));
    CHECK(!ae_files_layout_make(1000,680,1,false,true,true,false,NULL));
    CHECK(!ae_files_layout_cell(NULL,0,&(struct ae_files_rect){0}));
    CHECK(!ae_files_layout_cell(&l,0,NULL));
    CHECK(ae_files_layout_reveal(NULL,10,9,0)==0 && ae_files_layout_reveal(&l,0,0,0)==0);
    CHECK(ae_files_layout_make(640,420,2,true,true,true,true,&l));
    CHECK(l.capacity>=1 && !l.sidebar_visible && !l.inspector_visible && l.compact_cards);
    printf("files layout: %zu size/scale/pane/naming combinations; full cells, nonoverlapping controls, grid-aligned reveal and minimum 200-percent geometry passed\n",cases);
    return 0;
}
