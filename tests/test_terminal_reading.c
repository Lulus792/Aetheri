#define _XOPEN_SOURCE 700
#include "terminal/nexaui_view.h"
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#define CHECK(x) do {if(!(x)) {fprintf(stderr,"reading %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static int width(uint32_t c) {return wcwidth((wchar_t)c);}
static unsigned char *load(const char *path,size_t *size) {
    FILE *f=fopen(path,"rb");CHECK(f && !fseek(f,0,SEEK_END));long n=ftell(f);CHECK(n>0 && n<16*1024*1024 && !fseek(f,0,SEEK_SET));
    *size=(size_t)n;unsigned char *data=malloc(*size);CHECK(data && fread(data,1,*size,f)==*size && !fclose(f));return data;
}
static void paint(struct ae_terminal_view *v,const struct ae_terminal *m,bool named,bool enabled,bool visible) {
    CHECK(ae_terminal_view_begin(v,400,160,1,16,10,22,17));
    if(visible) {
        if(named) {
            struct ae_terminal_pane_view p={.id=17,.name="Original pane",.model=m,.bounds={20,30,300,88},.enabled=enabled,.focusable=enabled};
            CHECK(ae_terminal_view_pane(v,&p));
        } else CHECK(ae_terminal_view_grid(v,m,(nx_rect){20,30,300,88},false,true,enabled));
    }
    const unsigned char *pixels=NULL;size_t stride=0;CHECK(ae_terminal_view_finish(v,400,160,&pixels,&stride));
    bool focused=false;size_t count=0;CHECK(nx_semantics_count(ae_terminal_view_context(v),&count)==NX_OK);
    for(size_t i=0;i<count;++i) {
        nx_semantic_view row;CHECK(nx_semantics_get(ae_terminal_view_context(v),(uint32_t)i,&row)==NX_OK);
        if(row.role==NX_ROLE_TEXT && (row.state&NX_SEM_FOCUSED)) {
            CHECK(!focused && row.id==nx_focus_id(ae_terminal_view_context(v)));focused=true;
            unsigned y=(unsigned)row.bounds.y;const unsigned char *p=pixels+(size_t)y*stride+20*4;
            CHECK(p[0]==0x7c && p[1]==0xa8 && p[2]==0xe8);
        }
    }
    CHECK(focused==ae_terminal_view_reading(v));
}
static void focus(struct ae_terminal_view *v,bool named,unsigned row) {
    CHECK(named?ae_terminal_view_focus_pane_row(v,17,row):ae_terminal_view_focus_row(v,row));
}
static void move(struct ae_terminal_view *v,struct ae_terminal *m,bool named,enum ae_terminal_read_move step) {
    struct ae_terminal *copy=malloc(sizeof(*copy));CHECK(copy);memcpy(copy,m,sizeof(*copy));int scroll=999;
    CHECK(ae_terminal_view_read_move(v,m,named?17u:0u,step,&scroll));CHECK(!memcmp(copy,m,sizeof(*copy)));free(copy);
    CHECK(scroll>=-AE_TERM_HISTORY && scroll<=AE_TERM_HISTORY);if(scroll) ae_terminal_scroll(m,scroll);
}
static void expected(struct ae_terminal_view *v,const char *prefix) {
    uint64_t id=nx_focus_id(ae_terminal_view_context(v));uint32_t index;nx_semantic_view row;
    CHECK(id && nx_semantics_find(ae_terminal_view_context(v),id,&index)==NX_OK && nx_semantics_get(ae_terminal_view_context(v),index,&row)==NX_OK);
    CHECK(row.role==NX_ROLE_TEXT && row.value.length>=strlen(prefix) && !memcmp(row.value.data,prefix,strlen(prefix)));
}
static uint64_t row_id(struct ae_terminal_view *v,unsigned wanted) {
    size_t count=0;CHECK(nx_semantics_count(ae_terminal_view_context(v),&count)==NX_OK);
    for(size_t i=0;i<count;++i) {
        nx_semantic_view row;CHECK(nx_semantics_get(ae_terminal_view_context(v),(uint32_t)i,&row)==NX_OK);
        if(row.role==NX_ROLE_TEXT) {if(!wanted) return row.id;--wanted;}
    }
    CHECK(0);return 0;
}
int main(int argc,char **argv) {
    CHECK(argc==3 && setlocale(LC_CTYPE,"C.UTF-8"));size_t rn,bn;
    unsigned char *regular=load(argv[1],&rn),*bold=load(argv[2],&bn);struct ae_terminal *m=malloc(sizeof(*m));CHECK(m);
    for(unsigned mode=0;mode<2;++mode) {
        bool named=mode!=0;struct ae_terminal_view *v=NULL;CHECK(ae_terminal_view_create(regular,rn,bold,bn,&v));
        ae_terminal_init(m,30,4,width);
        for(unsigned i=0;i<10;++i) {char line[32];int n=snprintf(line,sizeof(line),"read-%u\r\n",i);CHECK(n>0);ae_terminal_feed(m,line,(size_t)n);}
        CHECK(m->history_count==7);paint(v,m,named,true,true);focus(v,named,1);
        move(v,m,named,AE_TERM_READ_UP);move(v,m,named,AE_TERM_READ_UP);
        CHECK(m->view==1);paint(v,m,named,true,true);expected(v,"read-6");
        move(v,m,named,AE_TERM_READ_PAGE_UP);paint(v,m,named,true,true);expected(v,"read-3");
        move(v,m,named,AE_TERM_READ_OLDEST);paint(v,m,named,true,true);expected(v,"read-0");CHECK(m->view==7);
        move(v,m,named,AE_TERM_READ_UP);paint(v,m,named,true,true);expected(v,"read-0");
        move(v,m,named,AE_TERM_READ_LAST);paint(v,m,named,true,true);expected(v,"read-3");
        move(v,m,named,AE_TERM_READ_DOWN);paint(v,m,named,true,true);expected(v,"read-4");CHECK(m->view==6);
        move(v,m,named,AE_TERM_READ_FIRST);paint(v,m,named,true,true);expected(v,"read-1");
        move(v,m,named,AE_TERM_READ_PAGE_DOWN);paint(v,m,named,true,true);expected(v,"read-4");
        move(v,m,named,AE_TERM_READ_LATEST);paint(v,m,named,true,true);expected(v," ");CHECK(!m->view);
        move(v,m,named,AE_TERM_READ_DOWN);paint(v,m,named,true,true);expected(v," ");
        /* A target edited after the key is not replaced by its new contents. */
        focus(v,named,1);move(v,m,named,AE_TERM_READ_UP);
        ae_terminal_feed(m,"\033[1;1HX",7);paint(v,m,named,true,true);CHECK(!ae_terminal_view_reading(v));
        /* Another focus owner wins over a deferred keyboard request. */
        focus(v,named,1);move(v,m,named,AE_TERM_READ_DOWN);
        CHECK(nx_focus_set(ae_terminal_view_context(v),row_id(v,0))==NX_OK);
        int cancelled=9;
        CHECK(!ae_terminal_view_read_move(v,m,named?17u:0u,AE_TERM_READ_DOWN,&cancelled) && !cancelled);
        paint(v,m,named,true,true);expected(v,"Xead-7");
        /* Even an unchanged target cannot outlive its edited source row. */
        focus(v,named,1);move(v,m,named,AE_TERM_READ_UP);
        ae_terminal_feed(m,"\033[2;1HY",7);paint(v,m,named,true,true);CHECK(!ae_terminal_view_reading(v));
        focus(v,named,1);move(v,m,named,AE_TERM_READ_DOWN);ae_terminal_scroll(m,1);
        paint(v,m,named,true,true);CHECK(!ae_terminal_view_reading(v));
        ae_terminal_scroll(m,-1);paint(v,m,named,true,true);
        focus(v,named,1);move(v,m,named,AE_TERM_READ_DOWN);paint(v,m,named,false,true);CHECK(!ae_terminal_view_reading(v));
        paint(v,m,named,true,true);focus(v,named,1);move(v,m,named,AE_TERM_READ_DOWN);
        CHECK(ae_terminal_resize(m,29,4));paint(v,m,named,true,true);CHECK(!ae_terminal_view_reading(v));
        focus(v,named,1);move(v,m,named,AE_TERM_READ_DOWN);
        ae_terminal_feed(m,"\033[?1049h",8);paint(v,m,named,true,true);CHECK(!ae_terminal_view_reading(v));
        focus(v,named,0);move(v,m,named,AE_TERM_READ_LATEST);paint(v,m,named,true,true);
        CHECK(nx_focus_id(ae_terminal_view_context(v))==row_id(v,m->rows-1) && !m->view);
        focus(v,named,1);move(v,m,named,AE_TERM_READ_DOWN);ae_terminal_view_clear_reading(v);
        paint(v,m,named,true,true);CHECK(!ae_terminal_view_reading(v));
        if(named) {
            focus(v,true,1);move(v,m,true,AE_TERM_READ_DOWN);paint(v,m,true,true,false);
            paint(v,m,true,true,true);CHECK(!ae_terminal_view_reading(v));
        }
        ae_terminal_feed(m,"\033[?1049l",8);paint(v,m,named,true,true);
        uint64_t moving=row_id(v,1);focus(v,named,2);move(v,m,named,AE_TERM_READ_UP);
        ae_terminal_feed(m,"\033[4;1H\r\n",8);paint(v,m,named,true,true);
        CHECK(nx_focus_id(ae_terminal_view_context(v))==moving);
        /* New output increments the numeric history offset while preserving
           the same visible contents. Reading must follow those original rows. */
        move(v,m,named,AE_TERM_READ_OLDEST);paint(v,m,named,true,true);
        uint64_t anchored=row_id(v,1);focus(v,named,0);move(v,m,named,AE_TERM_READ_DOWN);
        ae_terminal_feed(m,"\033[4;1H\r\n",8);paint(v,m,named,true,true);
        CHECK(nx_focus_id(ae_terminal_view_context(v))==anchored);
        focus(v,named,1);move(v,m,named,AE_TERM_READ_UP);
        for(unsigned i=0;i<AE_TERM_LINES+1;++i) ae_terminal_feed(m,"replacement\r\n",13);
        paint(v,m,named,true,true);CHECK(!ae_terminal_view_reading(v));
        int scroll=9;CHECK(!ae_terminal_view_read_move(v,m,named?17u:0u,AE_TERM_READ_UP,&scroll) && !scroll);
        ae_terminal_view_destroy(v);
    }
    free(m);free(regular);free(bold);puts("reading: original content row/page/history navigation, batching, pixels/semantic agreement and stale/disabled/resize/screen/hidden cancellation passed");return 0;
}
