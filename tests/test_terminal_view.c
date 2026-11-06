#define _XOPEN_SOURCE 700
#include "terminal/nexaui_view.h"
#include <locale.h>
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
static void frame(struct ae_terminal_view *v,struct ae_terminal *m,float scale,bool contrast) {
    CHECK(ae_terminal_view_begin(v,400,160,scale,16,10,22,17));
    CHECK(!ae_terminal_view_begin(v,400,160,scale,16,10,22,17));
    CHECK(ae_terminal_view_fill(v,(nx_rect){0,0,400,160},0x10141c));
    CHECK(ae_terminal_view_grid(v,m,(nx_rect){20,30,300,88},contrast,true));
    nx_rect clip={20,4,120,20};CHECK(ae_terminal_view_clip(v,&clip));
    CHECK(ae_terminal_view_label(v,"Terminal · Grüße",20,4,0xffffff));CHECK(ae_terminal_view_clip(v,NULL));
    const unsigned char *pixels=NULL;size_t stride=0;unsigned w=(unsigned)(400*scale),h=(unsigned)(160*scale);
    CHECK(ae_terminal_view_finish(v,w,h,&pixels,&stride) && pixels && stride==(size_t)w*4);
    uint32_t index=0;nx_semantic_view row;
    CHECK(nx_semantics_find(ae_terminal_view_context(v),1001,&index)==NX_OK && nx_semantics_get(ae_terminal_view_context(v),index,&row)==NX_OK);
    CHECK(row.role==NX_ROLE_TEXT && (row.state&NX_SEM_READONLY) && !row.actions && row.value.length==33);
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
    ae_terminal_init(m,AE_TERM_COLS,AE_TERM_ROWS,width);
    const char cluster[]="\360\235\220\200\314\201\314\202\314\203";
    for (unsigned i=0;i<AE_TERM_COLS*AE_TERM_ROWS;++i) ae_terminal_feed(m,cluster,sizeof(cluster)-1);
    sequence=m->sequence;
    CHECK(ae_terminal_view_begin(v,2450,2260,1,16,10,22,17));
    CHECK(ae_terminal_view_grid(v,m,(nx_rect){20,30,2400,2200},false,false));
    const unsigned char *dense_pixels=NULL;size_t dense_stride=0;
    CHECK(ae_terminal_view_finish(v,2450,2260,&dense_pixels,&dense_stride) && dense_pixels && dense_stride==9800);
    size_t total=0;
    for (unsigned i=0;i<AE_TERM_ROWS;++i) {
        uint32_t index=0;nx_semantic_view row;
        CHECK(nx_semantics_find(ae_terminal_view_context(v),1000u+i,&index)==NX_OK && nx_semantics_get(ae_terminal_view_context(v),index,&row)==NX_OK);
        CHECK(row.value.length==AE_TERM_COLS*(sizeof(cluster)-1));total+=row.value.length;
    }
    CHECK(total>65536 && m->sequence==sequence);
    CHECK(!ae_terminal_view_begin(v,400,160,1,16,0,22,17));
    CHECK(ae_terminal_view_begin(v,400,160,1,16,10,22,17));
    const unsigned char *pixels=NULL;size_t stride=0;CHECK(!ae_terminal_view_finish(v,8192,8192,&pixels,&stride) && !pixels);
    ae_terminal_view_destroy(v);free(m);free(regular);free(bold);
    puts("terminal NexaUI view: native font raster, bold/combining/wide cells, selection/contrast, clipping, density and owner immutability passed");return 0;
}
