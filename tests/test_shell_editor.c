#include "shell/editor.h"
#include "text/grapheme.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"editor %d: %s\n",__LINE__,#x);exit(1); } } while (0)
static bool fail_malloc;
void *__real_malloc(size_t size);
void *__wrap_malloc(size_t size) { return fail_malloc ? NULL : __real_malloc(size); }
static void expect(struct ae_line_editor *e,const char *text,size_t cursor) {
    size_t n,c;const char *p=ae_line_text(e,&n,&c);CHECK(p && n==strlen(text) && !memcmp(p,text,n+1) && c==cursor);
    size_t boundaries[AE_LINE_BYTES+1],count;CHECK(ae_text_graphemes(p,n,boundaries,AE_LINE_BYTES+1,&count));
    bool found=false;for (size_t i=0;i<count;++i) found|=boundaries[i]==c;CHECK(found);
}
int main(void) {
    struct ae_line_editor *e=ae_line_create();CHECK(e);
    CHECK(ae_line_insert(e,"Aäj́👩‍💻🇩🇪Z",strlen("Aäj́👩‍💻🇩🇪Z")));
    CHECK(ae_line_edit(e,AE_LINE_LEFT));expect(e,"Aäj́👩‍💻🇩🇪Z",25);
    CHECK(ae_line_edit(e,AE_LINE_BACKSPACE));expect(e,"Aäj́👩‍💻Z",17);
    CHECK(ae_line_edit(e,AE_LINE_BACKSPACE));expect(e,"Aäj́Z",6);
    CHECK(ae_line_edit(e,AE_LINE_BACKSPACE));expect(e,"AäZ",3);
    CHECK(ae_line_edit(e,AE_LINE_LEFT));CHECK(ae_line_edit(e,AE_LINE_DELETE));expect(e,"AZ",1);
    CHECK(ae_line_insert(e,"j́",3));expect(e,"Aj́Z",4);
    CHECK(ae_line_edit(e,AE_LINE_HOME));CHECK(ae_line_insert(e,"x",1));expect(e,"xAj́Z",1);
    CHECK(ae_line_edit(e,AE_LINE_END));CHECK(ae_line_edit(e,AE_LINE_WORD_BACK));expect(e,"",0);
    CHECK(ae_line_edit(e,AE_LINE_YANK));expect(e,"xAj́Z",6);
    size_t n,c;const char *text=ae_line_text(e,&n,&c);CHECK(ae_line_insert(e,text,n));expect(e,"xAj́ZxAj́Z",12);
    CHECK(!ae_line_insert(e,"\xc0\x80",2));expect(e,"xAj́ZxAj́Z",12);
    CHECK(!ae_line_insert(e,"a\nb",3));CHECK(!ae_line_insert(e,"\033[2J",4));CHECK(!ae_line_insert(e,"\xe2\x80\xae",3));
    CHECK(ae_line_remember(e));ae_line_clear(e);CHECK(ae_line_insert(e,"draft",5));CHECK(ae_line_edit(e,AE_LINE_LEFT));
    CHECK(ae_line_history(e,-1));expect(e,"xAj́ZxAj́Z",12);CHECK(ae_line_edit(e,AE_LINE_BACKSPACE));
    CHECK(ae_line_history(e,1));expect(e,"draft",4);CHECK(ae_line_history(e,-1));expect(e,"xAj́ZxAj́Z",12);
    ae_line_clear(e);CHECK(ae_line_insert(e," private",8));CHECK(ae_line_remember(e));ae_line_clear(e);
    CHECK(ae_line_history(e,-1));expect(e,"xAj́ZxAj́Z",12);
    ae_line_clear(e);CHECK(ae_line_insert(e,"new",3));fail_malloc=true;CHECK(!ae_line_remember(e));fail_malloc=false;
    expect(e,"new",3);CHECK(ae_line_history(e,-1));expect(e,"xAj́ZxAj́Z",12);
    for (unsigned i=0;i<70;++i) { char line[32];int size=snprintf(line,sizeof(line),"command%u",i);CHECK(size>0);ae_line_clear(e);CHECK(ae_line_insert(e,line,(size_t)size));CHECK(ae_line_remember(e)); }
    ae_line_clear(e);
    for (unsigned i=0;i<100;++i) { CHECK(ae_line_history(e,-1)); }
    expect(e,"command6",8);
    for (unsigned i=0;i<100;++i) { CHECK(ae_line_history(e,1)); }
    expect(e,"",0);
    char large[AE_LINE_BYTES+1];memset(large,'a',AE_LINE_BYTES);large[AE_LINE_BYTES]=0;
    CHECK(ae_line_insert(e,large,AE_LINE_BYTES));CHECK(!ae_line_insert(e,"b",1));expect(e,large,AE_LINE_BYTES);
    for (unsigned i=0;i<20;++i) { ae_line_clear(e);large[0]=(char)('a'+i);CHECK(ae_line_insert(e,large,AE_LINE_BYTES));CHECK(ae_line_remember(e)); }
    ae_line_clear(e);
    for (unsigned i=0;i<100;++i) { CHECK(ae_line_history(e,-1)); }
    large[0]='m';expect(e,large,AE_LINE_BYTES);
    ae_line_destroy(e);puts("editor: grapheme edits, aliases, atomic rejection, history draft/ownership/OOM and byte/count bounds passed");return 0;
}
