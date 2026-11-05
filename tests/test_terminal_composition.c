#include "terminal/composition.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"composition %d: %s\n",__LINE__,#x);exit(1); } } while (0)
static int width(uint32_t c) { return c==0x301 ? 0 : c==0x4e2d ? 2 : 1; }
static bool same(const struct ae_terminal_composition *a,const struct ae_terminal_composition *b) {
    return a->count==b->count && a->cursor==b->cursor && a->anchor==b->anchor && a->size==b->size && a->caret==b->caret &&
        !memcmp(a->text,b->text,sizeof(a->text)) && !memcmp(a->offsets,b->offsets,sizeof(a->offsets));
}
int main(void) {
    struct ae_terminal_composition c={0};struct ae_terminal_composition_view v;
    CHECK(ae_terminal_composition_set(&c,"Gr\xc3\xbc\xc3\x9f" "e",7,2,2));
    CHECK(c.count==5 && c.size==7 && c.offsets[2]==2 && c.offsets[4]==6 && c.caret);
    CHECK(ae_terminal_composition_view(&c,16,width,&v) && !v.hidden_before && !v.hidden_after);
    CHECK(v.cursor==2 && v.selection_begin==2 && v.selection_end==4 && v.end==7);
    CHECK(ae_terminal_composition_set(&c,"abcde",5,4,-2));
    CHECK(ae_terminal_composition_view(&c,16,width,&v) && v.selection_begin==2 && v.selection_end==4 && v.cursor==4);
    struct ae_terminal_composition old=c;
    const char *bad[]={"\xc0\x80","a\033b","a\nb","a\tb","a\xe2\x80\xae"};
    for (unsigned i=0;i<sizeof(bad)/sizeof(bad[0]);++i) { CHECK(!ae_terminal_composition_set(&c,bad[i],strlen(bad[i]),0,0));CHECK(same(&c,&old)); }
    CHECK(!ae_terminal_composition_set(&c,"a\0b",3,0,0) && same(&c,&old));
    CHECK(!ae_terminal_composition_set(&c,"ab",2,3,0) && same(&c,&old));
    CHECK(!ae_terminal_composition_set(&c,"ab",2,1,2) && same(&c,&old));
    CHECK(!ae_terminal_composition_set(&c,"ab",2,0,-2) && same(&c,&old));
    CHECK(ae_terminal_composition_set(&c,"ab",2,-1,-1) && !c.caret);
    char text[AE_TERM_COMPOSITION_BYTES+1];memset(text,'x',sizeof(text));
    CHECK(!ae_terminal_composition_set(&c,text,sizeof(text),0,0));
    CHECK(ae_terminal_composition_set(&c,text,AE_TERM_COMPOSITION_BYTES,AE_TERM_COMPOSITION_BYTES,0));
    CHECK(ae_terminal_composition_view(&c,16,width,&v) && v.hidden_before && !v.hidden_after && v.end==AE_TERM_COMPOSITION_BYTES && v.cursor==15);
    CHECK(ae_terminal_composition_set(&c,"e\xcc\x81\xe4\xb8\xad",6,3,0));
    CHECK(ae_terminal_composition_view(&c,3,width,&v) && v.cursor==2 && v.begin==3 && v.end==6);
    CHECK(ae_terminal_composition_set(&c,NULL,0,0,0) && !c.size && !c.count);
    CHECK(!ae_terminal_composition_view(&c,0,width,&v));
    puts("composition: complete bounded UTF-8, scalar/byte/cell ranges, reverse/unknown selection, atomic failure and horizontal viewport passed");return 0;
}
