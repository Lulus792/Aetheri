#include "terminal/model.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"%d: %s\n",__LINE__,#x);exit(1); } } while (0)
static int width(uint32_t c) { return c==0x301 ? 0 : c==0x4e2d ? 2 : 1; }
static void feed(struct ae_terminal *m,const char *s) { ae_terminal_feed(m,s,strlen(s)); }
static void copied(struct ae_terminal *m,const char *expected) {
    size_t n=ae_terminal_selection_size(m);CHECK(n==strlen(expected)+1);
    char *out=malloc(n+2);CHECK(out);memset(out,'!',n+2);
    CHECK(!ae_terminal_selection_copy(m,out,n-1));
    for (size_t i=0;i<n+2;++i) CHECK(out[i]=='!');
    CHECK(ae_terminal_selection_copy(m,out,n));CHECK(!strcmp(out,expected));
    CHECK(out[n]=='!' && out[n+1]=='!');free(out);
}
int main(void) {
    struct ae_terminal *m=malloc(sizeof(*m));CHECK(m);
    ae_terminal_init(m,8,3,width);CHECK(!ae_terminal_select_all(m));
    feed(m,"hello");CHECK(ae_terminal_select_all(m));copied(m,"hello");
    ae_terminal_init(m,8,3,width);feed(m,"abcdef\r\nsecond\r\nlast");
    CHECK(!ae_terminal_select_begin(m,3,0));CHECK(!ae_terminal_select_begin(m,0,9));
    CHECK(ae_terminal_select_begin(m,0,1));CHECK(ae_terminal_select_extend(m,1,3));
    copied(m,"bcdef\nsec");unsigned first,last;
    CHECK(ae_terminal_selected_range(m,0,&first,&last) && first==1 && last==8);
    CHECK(ae_terminal_selected_range(m,1,&first,&last) && first==0 && last==3);
    CHECK(!ae_terminal_selected_range(m,2,&first,&last));
    CHECK(ae_terminal_select_begin(m,1,3));CHECK(ae_terminal_select_extend(m,0,1));copied(m,"bcdef\nsec");
    feed(m,"\033[3;1HOTHER");copied(m,"bcdef\nsec"); /* Unselected row changes. */
    feed(m,"\033[2;1Hchanged");CHECK(ae_terminal_selection_size(m)==0 && !m->selection.count);
    ae_terminal_init(m,8,3,width);feed(m,"first\r\nsecond\r\nlast");
    CHECK(ae_terminal_select_begin(m,0,0));CHECK(ae_terminal_select_extend(m,0,5));
    uint64_t original=m->screen_rows[0].token;feed(m,"\r\nnew");copied(m,"first");
    CHECK(m->history_count==1 && m->history_rows[m->history_head].token==original);
    CHECK(!ae_terminal_selected_range(m,0,&first,&last));
    ae_terminal_scroll(m,1);CHECK(ae_terminal_selected_range(m,0,&first,&last));copied(m,"first");
    for (unsigned i=0;i<AE_TERM_HISTORY+1;++i) feed(m,"\r\nx");
    CHECK(!m->selection.count && !ae_terminal_selection_size(m)); /* Eviction cannot retarget. */
    ae_terminal_init(m,8,3,width);feed(m,"12345678AB  \r\nend");
    CHECK(ae_terminal_select_begin(m,0,0));CHECK(ae_terminal_select_extend(m,2,3));copied(m,"12345678AB\nend");
    ae_terminal_init(m,8,3,width);feed(m,"1234567 \xe4\xb8\xad");
    CHECK(ae_terminal_select_begin(m,0,0));CHECK(ae_terminal_select_extend(m,1,2));copied(m,"1234567 \xe4\xb8\xad");
    ae_terminal_init(m,8,3,width);feed(m,"1234567\xe4\xb8\xad");
    CHECK(ae_terminal_select_begin(m,0,0));CHECK(ae_terminal_select_extend(m,1,2));copied(m,"1234567\xe4\xb8\xad"); /* Wide wrap padding is not text. */
    ae_terminal_init(m,8,3,width);feed(m,"Ae\xcc\x81\xe4\xb8\xadZ");
    CHECK(ae_terminal_select_begin(m,0,1));CHECK(ae_terminal_select_extend(m,0,4));copied(m,"e\xcc\x81\xe4\xb8\xad");
    CHECK(ae_terminal_select_begin(m,0,3));CHECK(ae_terminal_select_extend(m,0,4));copied(m,"\xe4\xb8\xad");
    feed(m,"\033[1;3H");ae_terminal_select_clear(m);
    CHECK(ae_terminal_select_step(m,1,0));copied(m,"\xe4\xb8\xad");
    CHECK(ae_terminal_select_step(m,-1,0));CHECK(!ae_terminal_selection_size(m));
    CHECK(!ae_terminal_select_step(m,2,0));CHECK(!ae_terminal_select_step(m,1,1));
    CHECK(ae_terminal_select_all(m));CHECK(ae_terminal_resize(m,9,3));CHECK(!m->selection.count);
    CHECK(ae_terminal_select_all(m));feed(m,"\033[?1049halt");CHECK(!m->selection.count);
    CHECK(ae_terminal_select_all(m));feed(m,"\033[?1049l");CHECK(!m->selection.count);
    CHECK(ae_terminal_select_all(m));feed(m,"\033c");CHECK(!m->selection.count);
    ae_terminal_init(m,8,4,width);feed(m,"one\r\ntwo\r\nthree");
    CHECK(ae_terminal_select_begin(m,0,0));CHECK(ae_terminal_select_extend(m,2,5));
    feed(m,"\033[2;1HTWO");CHECK(!m->selection.count); /* Interior row changes, endpoints survive. */
    ae_terminal_init(m,8,3,width);feed(m,"one\r\ntwo\r\nthree\r\nfour");
    CHECK(ae_terminal_select_begin(m,0,0));CHECK(ae_terminal_select_step(m,0,-1));
    CHECK(m->view==1);copied(m,"one\n");
    ae_terminal_init(m,AE_TERM_COLS,AE_TERM_ROWS,width);
    char row[AE_TERM_COLS+3];memset(row,'x',AE_TERM_COLS);row[AE_TERM_COLS]='\r';row[AE_TERM_COLS+1]='\n';row[AE_TERM_COLS+2]=0;
    for (unsigned i=0;i<AE_TERM_LINES;++i) feed(m,row);
    CHECK(ae_terminal_select_all(m) && m->selection.count==AE_TERM_LINES-1);
    ae_terminal_scroll(m,AE_TERM_HISTORY);CHECK(ae_terminal_select_begin(m,0,0));
    ae_terminal_scroll(m,-AE_TERM_HISTORY);CHECK(ae_terminal_select_extend(m,AE_TERM_ROWS-1,AE_TERM_COLS));
    CHECK(m->selection.count==AE_TERM_LINES);
    size_t needed=ae_terminal_selection_size(m);CHECK(needed>(size_t)AE_TERM_HISTORY*AE_TERM_COLS);
    char *large=malloc(needed);CHECK(large && ae_terminal_selection_copy(m,large,needed));
    CHECK(strlen(large)+1==needed);free(large);
    ae_terminal_scroll(m,AE_TERM_HISTORY);CHECK(ae_terminal_selected_range(m,0,&first,&last));
    feed(m,"\033[3J");CHECK(!m->selection.count); /* Explicit history erase fences the old range. */
    ae_terminal_init(m,8,3,width);m->row_serial=UINT64_MAX;feed(m,"x");
    CHECK(!m->screen_rows[0].token && !ae_terminal_select_begin(m,0,0));
    CHECK(!ae_terminal_select_all(m)); /* Never reuse an exhausted identity. */
    free(m);puts("terminal selection: exact UTF-8, wide/combining cells, wraps, identity-preserving history, stale rejection, keyboard and atomic bounds passed");return 0;
}
