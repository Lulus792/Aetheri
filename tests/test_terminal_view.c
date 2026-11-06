#define _XOPEN_SOURCE 700
#include "terminal/nexaui_view.h"
#include <locale.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#define CHECK(x) do { if (!(x)) {fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x);exit(1);} } while (0)
static unsigned char *load(const char *name,size_t *length) {
    FILE *f=fopen(name,"rb");CHECK(f && !fseek(f,0,SEEK_END));long n=ftell(f);CHECK(n>0 && n<16*1024*1024 && !fseek(f,0,SEEK_SET));
    *length=(size_t)n;unsigned char *bytes=malloc(*length);CHECK(bytes && fread(bytes,1,*length,f)==*length && !fclose(f));return bytes;
}
static int width(uint32_t c) {return wcwidth((wchar_t)c);}
static bool color(const unsigned char *p,unsigned rgb) {return p[0]==(rgb>>16) && p[1]==((rgb>>8)&255u) && p[2]==(rgb&255u) && p[3]==255;}
static void chrome_cases(struct ae_terminal_view *v,const void *regular,size_t rn,const void *bold,size_t bn) {
    struct ae_terminal_view *reference=NULL;CHECK(ae_terminal_view_create(regular,rn,bold,bn,&reference));
    struct ae_terminal_chrome c;
    CHECK(!ae_terminal_chrome_layout(NAN,320,1,&c));
    CHECK(!ae_terminal_chrome_layout(480,320,0,&c));
    for (unsigned i=0;i<8;++i) {
        float text=(i&1u)?2:1,density=(i&2u)?2:1;
        CHECK(ae_terminal_chrome_layout(480,320,text,&c));
        CHECK(c.bar.height<40 && c.title.x>c.buttons[2].x+c.buttons[2].width && c.title.x+c.title.width<=480);
        CHECK(ae_terminal_chrome_target(&c,19,c.bar.height/2)==AE_TERM_CHROME_CLOSE);
        CHECK(ae_terminal_chrome_target(&c,41,c.bar.height/2)==AE_TERM_CHROME_MINIMIZE);
        CHECK(ae_terminal_chrome_target(&c,63,c.bar.height/2)==AE_TERM_CHROME_ZOOM);
        CHECK(!ae_terminal_chrome_target(&c,200,c.bar.height/2) && !ae_terminal_chrome_target(&c,NAN,0));
        CHECK(ae_terminal_view_begin(v,480,320,density,16*text,10*text,22*text,17*text));
        struct ae_terminal_chrome_state state={.german=(i&4u)!=0,.contrast=(i&4u)!=0,.editor=(i&1u)!=0,.hovered=AE_TERM_CHROME_CLOSE};
        unsigned request=99;CHECK(ae_terminal_view_chrome(v,&state,&request) && !request);
        CHECK(ae_terminal_view_label(v,"Terminal",92,65,0xe8eaf0));
        const unsigned char *pixels=NULL;size_t stride=0;
        CHECK(ae_terminal_view_finish(v,(unsigned)(480*density),(unsigned)(320*density),&pixels,&stride));
        CHECK(pixels && stride==(size_t)(480*density)*4);
        CHECK(ae_terminal_view_begin(reference,480,320,density,16*text,10*text,22*text,17*text));
        CHECK(ae_terminal_view_label(reference,"Terminal",92,65,0xe8eaf0));
        const unsigned char *expected=NULL;size_t expected_stride=0;
        CHECK(ae_terminal_view_finish(reference,(unsigned)(480*density),(unsigned)(320*density),&expected,&expected_stride));
        CHECK(expected_stride==stride);
        /* Other labels at a different size must not change this label's pixels. */
        for(unsigned y=(unsigned)(65*density);y<(unsigned)(140*density);++y)
            CHECK(!memcmp(pixels+(size_t)y*stride,expected+(size_t)y*stride,stride));
        uint32_t index;nx_semantic_view node;
        CHECK(nx_semantics_find(ae_terminal_view_context(v),4,&index)==NX_OK && nx_semantics_get(ae_terminal_view_context(v),index,&node)==NX_OK);
        CHECK(node.role==NX_ROLE_BUTTON && (node.state&NX_SEM_DISABLED) && !node.actions);
        CHECK(nx_semantics_request_action(ae_terminal_view_context(v),4,NX_ACTION_PRESS)!=NX_OK);
    }
    CHECK(nx_semantics_request_action(ae_terminal_view_context(v),5,NX_ACTION_PRESS)==NX_OK);
    CHECK(ae_terminal_view_begin(v,480,320,1,16,10,22,17));
    struct ae_terminal_chrome_state state={.zoom_disabled=true};unsigned request;
    CHECK(ae_terminal_view_chrome(v,&state,&request) && !request);
    const unsigned char *pixels=NULL;size_t stride=0;CHECK(ae_terminal_view_finish(v,480,320,&pixels,&stride));
    CHECK(nx_semantics_request_action(ae_terminal_view_context(v),5,NX_ACTION_PRESS)!=NX_OK);
    CHECK(nx_semantics_request_action(ae_terminal_view_context(v),3,NX_ACTION_PRESS)==NX_OK);
    CHECK(ae_terminal_view_begin(v,480,320,1,16,10,22,17));
    CHECK(ae_terminal_view_chrome(v,&state,&request) && request==AE_TERM_CHROME_CLOSE);
    CHECK(ae_terminal_view_finish(v,480,320,&pixels,&stride));
    ae_terminal_view_destroy(reference);
}
static nx_semantic_view row_at(struct ae_terminal_view *v,unsigned row) {
    nx_semantic_view sem;CHECK(nx_semantics_get(ae_terminal_view_context(v),2u+row,&sem)==NX_OK);
    CHECK(sem.role==NX_ROLE_TEXT && sem.parent_id==2);return sem;
}
static void focus_frame(struct ae_terminal_view *v,struct ae_terminal *m,bool enabled,bool outline) {
    struct ae_terminal *before=malloc(sizeof(*before));CHECK(before);memcpy(before,m,sizeof(*before));
    CHECK(ae_terminal_view_begin(v,400,160,1,16,10,22,17));
    CHECK(ae_terminal_view_grid(v,m,(nx_rect){20,30,300,88},false,true,enabled));
    const unsigned char *pixels=NULL;size_t stride=0;CHECK(ae_terminal_view_finish(v,400,160,&pixels,&stride));
    CHECK(!memcmp(m,before,sizeof(*m)));free(before);
    unsigned blue=0;
    for (unsigned y=29;y<120;++y) for (unsigned x=19;x<322;++x)
        if (color(pixels+(size_t)y*stride+(size_t)x*4,0x7ca8e8)) ++blue;
    CHECK(outline?blue>200:blue==0);
}
static void focus_cases(struct ae_terminal_view *v,struct ae_terminal *m) {
    const char text[]="first\r\noriginal\r\nthird";ae_terminal_feed(m,text,sizeof(text)-1);
    focus_frame(v,m,true,false);uint64_t original=row_at(v,1).id;
    CHECK(!ae_terminal_view_reading(v) && !ae_terminal_view_focus_row(v,4));
    CHECK(ae_terminal_view_focus_row(v,1));focus_frame(v,m,true,true);
    CHECK(ae_terminal_view_reading(v) && nx_focus_id(ae_terminal_view_context(v))==original && (row_at(v,1).state&NX_SEM_FOCUSED));
    const char move[]="\r\nnew\r\n";ae_terminal_feed(m,move,sizeof(move)-1);
    focus_frame(v,m,true,true);CHECK(row_at(v,0).id==original && (row_at(v,0).state&NX_SEM_FOCUSED));
    CHECK(nx_semantics_request_action(ae_terminal_view_context(v),original,NX_ACTION_FOCUS)==NX_OK);
    const char edit[]="\033[1;1HX";ae_terminal_feed(m,edit,sizeof(edit)-1);
    focus_frame(v,m,true,false);uint32_t index;
    CHECK(row_at(v,0).id!=original && !ae_terminal_view_reading(v) && !nx_focus_id(ae_terminal_view_context(v)));
    CHECK(nx_semantics_find(ae_terminal_view_context(v),original,&index)!=NX_OK);
    CHECK(ae_terminal_view_focus_row(v,0));focus_frame(v,m,true,true);
    uint64_t before_resize=row_at(v,0).id;
    CHECK(ae_terminal_resize(m,29,4));focus_frame(v,m,true,false);
    uint64_t resized=row_at(v,0).id;
    CHECK(resized!=before_resize && !ae_terminal_view_reading(v));
    CHECK(nx_semantics_request_action(ae_terminal_view_context(v),before_resize,NX_ACTION_FOCUS)!=NX_OK);
    CHECK(ae_terminal_view_focus_row(v,0));focus_frame(v,m,true,true);
    CHECK(ae_terminal_resize(m,30,4));focus_frame(v,m,true,false);
    CHECK(row_at(v,0).id!=before_resize && row_at(v,0).id!=resized && !ae_terminal_view_reading(v));
    CHECK(nx_semantics_request_action(ae_terminal_view_context(v),resized,NX_ACTION_FOCUS)!=NX_OK);
    CHECK(ae_terminal_view_focus_row(v,0));focus_frame(v,m,true,true);
    focus_frame(v,m,false,false);CHECK(!ae_terminal_view_reading(v) && !(row_at(v,0).state&NX_SEM_FOCUSED));
    CHECK(!ae_terminal_view_focus_row(v,0) && nx_semantics_request_action(ae_terminal_view_context(v),row_at(v,0).id,NX_ACTION_FOCUS)!=NX_OK);
    m->screen_rows[0].token=UINT64_MAX;
    CHECK(ae_terminal_view_begin(v,400,160,1,16,10,22,17));
    CHECK(!ae_terminal_view_grid(v,m,(nx_rect){20,30,300,88},false,true,true));
}
static void frame(struct ae_terminal_view *v,struct ae_terminal *m,float scale,bool contrast) {
    CHECK(ae_terminal_view_begin(v,400,160,scale,16,10,22,17));
    CHECK(!ae_terminal_view_begin(v,400,160,scale,16,10,22,17));
    CHECK(ae_terminal_view_fill(v,(nx_rect){0,0,400,160},0x10141c));
    CHECK(ae_terminal_view_grid(v,m,(nx_rect){20,30,300,88},contrast,true,true));
    nx_rect clip={20,4,120,20};CHECK(ae_terminal_view_clip(v,&clip));
    CHECK(ae_terminal_view_label(v,"Terminal · Grüße",20,4,0xffffff));CHECK(ae_terminal_view_clip(v,NULL));
    const unsigned char *pixels=NULL;size_t stride=0;unsigned w=(unsigned)(400*scale),h=(unsigned)(160*scale);
    CHECK(ae_terminal_view_finish(v,w,h,&pixels,&stride) && pixels && stride==(size_t)w*4);
    nx_semantic_view row;
    CHECK(nx_semantics_get(ae_terminal_view_context(v),3,&row)==NX_OK);
    CHECK(row.role==NX_ROLE_TEXT && row.parent_id==2 && (row.state&NX_SEM_READONLY) && row.actions==NX_ACTION_FOCUS && row.value.length==33);
    CHECK(!memcmp(row.value.data,"e\314\201 \344\270\255",7));
    CHECK(!ae_terminal_view_finish(v,w,h,&pixels,&stride));
    CHECK(color(pixels+(size_t)(159*scale)*stride+(size_t)(399*scale)*4,0x10141c));
    unsigned bright=0;
    for (unsigned y=(unsigned)(30*scale);y<(unsigned)(52*scale);++y)
        for (unsigned x=(unsigned)(20*scale);x<(unsigned)(90*scale);++x) {const unsigned char *p=pixels+(size_t)y*stride+(size_t)x*4;if (p[0]>100 && p[1]>100 && p[2]>100) ++bright;}
    CHECK(bright>30); /* Actual rasterized glyphs, not a submitted empty canvas. */
    unsigned expected=m->selection.count?0x304e78:contrast?0:0x10141c;
    CHECK(color(pixels+(size_t)(30*scale)*stride+(size_t)(20*scale)*4,expected));
}
int main(int argc,char **argv) {
    CHECK(argc==3 && setlocale(LC_CTYPE,"C.UTF-8"));size_t rn,bn;
    unsigned char *regular=load(argv[1],&rn),*bold=load(argv[2],&bn);struct ae_terminal_view *v=NULL;
    CHECK(!ae_terminal_view_create(regular,1,bold,bn,&v) && !v);
    CHECK(ae_terminal_view_create(regular,rn,bold,bn,&v));
    struct ae_terminal *m=malloc(sizeof(*m));CHECK(m);ae_terminal_init(m,30,4,width);
    const char text[]="regular \033[1mbold\033[0m\r\ne\314\201 \344\270\255\r\n\033[4munderlined\033[0m";ae_terminal_feed(m,text,sizeof(text)-1);
    uint64_t sequence=m->sequence;unsigned x=m->x,y=m->y;
    frame(v,m,1,false);frame(v,m,2,false);frame(v,m,1,true);
    CHECK(m->sequence==sequence && m->x==x && m->y==y);
    CHECK(ae_terminal_select_begin(m,0,0) && ae_terminal_select_extend(m,0,7));
    frame(v,m,1,false);frame(v,m,2,true);CHECK(m->selection.count);
    ae_terminal_view_destroy(v);v=NULL;CHECK(ae_terminal_view_create(regular,rn,bold,bn,&v));
    ae_terminal_init(m,AE_TERM_COLS,AE_TERM_ROWS,width);
    const char cluster[]="\360\235\220\200\314\201\314\202\314\203";
    for (unsigned i=0;i<AE_TERM_COLS*AE_TERM_ROWS;++i) ae_terminal_feed(m,cluster,sizeof(cluster)-1);
    sequence=m->sequence;
    CHECK(ae_terminal_view_begin(v,2450,2260,1,16,10,22,17));
    CHECK(ae_terminal_view_grid(v,m,(nx_rect){20,30,2400,2200},false,false,true));
    const unsigned char *dense_pixels=NULL;size_t dense_stride=0;
    CHECK(ae_terminal_view_finish(v,2450,2260,&dense_pixels,&dense_stride) && dense_pixels && dense_stride==9800);
    size_t total=0;
    for (unsigned i=0;i<AE_TERM_ROWS;++i) {
        nx_semantic_view row;
        CHECK(nx_semantics_get(ae_terminal_view_context(v),2u+i,&row)==NX_OK);
        CHECK(row.value.length==AE_TERM_COLS*(sizeof(cluster)-1));total+=row.value.length;
    }
    CHECK(total>65536 && m->sequence==sequence);
    CHECK(!ae_terminal_view_begin(v,400,160,1,16,0,22,17));
    CHECK(ae_terminal_view_begin(v,400,160,1,16,10,22,17));
    const unsigned char *pixels=NULL;size_t stride=0;CHECK(!ae_terminal_view_finish(v,8192,8192,&pixels,&stride) && !pixels);
    ae_terminal_view_destroy(v);v=NULL;CHECK(ae_terminal_view_create(regular,rn,bold,bn,&v));
    chrome_cases(v,regular,rn,bold,bn);ae_terminal_init(m,30,4,width);focus_cases(v,m);
    ae_terminal_view_destroy(v);free(m);free(regular);free(bold);
    puts("terminal NexaUI view: native font raster, bold/combining/wide cells, selection/contrast, clipping, density and owner immutability passed");return 0;
}
