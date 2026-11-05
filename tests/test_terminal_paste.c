#include "terminal/paste.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"%d: %s\n",__LINE__,#x);exit(1); } } while (0)
static int width(uint32_t c) { return c==0x301 ? 0 : c==0x4e2d ? 2 : 1; }
static bool same(const struct ae_terminal_paste *a,const struct ae_terminal_paste *b) {
    return a->size==b->size && a->lines==b->lines && a->review==b->review && !memcmp(a->text,b->text,sizeof(a->text));
}
int main(void) {
    struct ae_terminal_paste p={0};
    CHECK(ae_terminal_paste_set(&p,"Gr\xc3\xbc\xc3\x9f" "e",7)==AE_TERM_PASTE_OK);
    CHECK(p.size==7 && !p.review && p.lines==1);
    struct ae_terminal_paste before=p;
    const char *invalid[]={"\xc0\x80","\xed\xa0\x80","\xf4\x90\x80\x80","\xc3"};
    for (unsigned i=0;i<sizeof(invalid)/sizeof(invalid[0]);++i) {
        CHECK(ae_terminal_paste_set(&p,invalid[i],strlen(invalid[i]))==AE_TERM_PASTE_UTF8);
        CHECK(same(&p,&before));
    }
    const char *controls[]={"\033[201~echo x", "a\001b", "\177", "\xc2\x85", "\xe2\x80\xae", "\xe2\x81\xa6"};
    for (unsigned i=0;i<sizeof(controls)/sizeof(controls[0]);++i) {
        CHECK(ae_terminal_paste_set(&p,controls[i],strlen(controls[i]))==AE_TERM_PASTE_CONTROL);
        CHECK(same(&p,&before));
    }
    CHECK(ae_terminal_paste_set(&p,"a\0b",3)==AE_TERM_PASTE_CONTROL);
    CHECK(ae_terminal_paste_set(&p,"a\r\nb\rc\nd\t",9)==AE_TERM_PASTE_OK);
    CHECK(p.review && p.lines==4 && !strcmp(p.text,"a\nb\nc\nd\t"));
    char line[AE_TERM_PREVIEW_BYTES];size_t at=0;
    at=ae_terminal_paste_preview(&p,at,16,width,line);CHECK(!strcmp(line,"a\\n"));
    at=ae_terminal_paste_preview(&p,at,16,width,line);CHECK(!strcmp(line,"b\\n"));
    at=ae_terminal_paste_preview(&p,at,16,width,line);CHECK(!strcmp(line,"c\\n"));
    at=ae_terminal_paste_preview(&p,at,16,width,line);CHECK(!strcmp(line,"d\\t") && at==p.size);
    CHECK(ae_terminal_paste_set(&p,"e\xcc\x81\\n",5)==AE_TERM_PASTE_OK);
    CHECK(ae_terminal_paste_preview(&p,0,24,width,line)==p.size && !strcmp(line,"e\\u{301}\\\\n"));
    CHECK(ae_terminal_paste_set(&p,"\xe2\x80\xa8",3)==AE_TERM_PASTE_OK && p.review);
    char large[AE_TERM_PASTE_BYTES+1];memset(large,'x',sizeof(large));
    CHECK(ae_terminal_paste_set(&p,large,sizeof(large))==AE_TERM_PASTE_TOO_LARGE);
    CHECK(ae_terminal_paste_set(&p,large,AE_TERM_PASTE_BYTES)==AE_TERM_PASTE_OK && p.size==AE_TERM_PASTE_BYTES);
    at=0;size_t displayed=0;
    while (at<p.size) {
        size_t next=ae_terminal_paste_preview(&p,at,16,width,line);
        CHECK(next>at && next<=p.size && strlen(line)<=16);displayed+=strlen(line);at=next;
    }
    CHECK(displayed==AE_TERM_PASTE_BYTES);
    ae_terminal_paste_clear(&p);CHECK(!p.size && !p.review && !p.lines);
    CHECK(ae_terminal_paste_set(&p,NULL,0)==AE_TERM_PASTE_EMPTY);
    puts("terminal paste: strict UTF-8/control validation, atomic rejection, CR normalization, literal snapshot and complete bounded preview passed");return 0;
}
