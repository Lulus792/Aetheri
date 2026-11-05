#include "editor/modal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"modal %d: %s\n",__LINE__,#x);exit(1); } } while (0)
static bool fail_malloc;
void *__real_malloc(size_t size);
void *__wrap_malloc(size_t size) { return fail_malloc ? NULL : __real_malloc(size); }
static struct ae_modal *create(const char *text,struct ae_document **document) {
    enum ae_document_result result;*document=ae_document_create(text,strlen(text),&result);CHECK(*document && result==AE_DOCUMENT_OK);
    struct ae_modal *m=ae_modal_create(*document);CHECK(m);return m;
}
static void equal(struct ae_modal *m,const char *text,size_t cursor,enum ae_modal_mode mode) {
    struct ae_document_info info;const struct ae_document *d=ae_modal_document(m);CHECK(ae_document_info(d,&info));
    CHECK(info.size==strlen(text) && info.cursor==cursor && ae_modal_mode(m)==mode);
    struct ae_document_slice s;CHECK(ae_document_slice(d,0,info.size,&s));CHECK(!s.first_size || !memcmp(s.first,text,s.first_size));
    CHECK(!s.second_size || !memcmp(s.second,text+s.first_size,s.second_size));
}
static void action(struct ae_modal *m,enum ae_modal_action a) { CHECK(ae_modal_action(m,a)==AE_DOCUMENT_OK); }
int main(void) {
    struct ae_document *d;struct ae_modal *m=create("ac",&d);equal(m,"ac",0,AE_MODAL_NORMAL);
    CHECK(ae_modal_text(m,"x",1)==AE_DOCUMENT_INVALID);action(m,AE_MODAL_APPEND);CHECK(ae_modal_text(m,"b",1)==AE_DOCUMENT_OK);
    equal(m,"abc",2,AE_MODAL_INSERT);action(m,AE_MODAL_ESCAPE);equal(m,"abc",1,AE_MODAL_NORMAL);
    action(m,AE_MODAL_DELETE);equal(m,"ac",1,AE_MODAL_NORMAL);action(m,AE_MODAL_END);equal(m,"ac",1,AE_MODAL_NORMAL);
    action(m,AE_MODAL_LINE_APPEND);CHECK(ae_modal_text(m,"j́",3)==AE_DOCUMENT_OK);equal(m,"acj́",5,AE_MODAL_INSERT);
    action(m,AE_MODAL_ESCAPE);equal(m,"acj́",2,AE_MODAL_NORMAL);action(m,AE_MODAL_DELETE);equal(m,"ac",1,AE_MODAL_NORMAL);
    action(m,AE_MODAL_LINE_INSERT);action(m,AE_MODAL_NEWLINE);equal(m,"\nac",1,AE_MODAL_INSERT);
    action(m,AE_MODAL_BACKSPACE);equal(m,"ac",0,AE_MODAL_INSERT);action(m,AE_MODAL_ESCAPE);
    action(m,AE_MODAL_OPEN_AFTER);equal(m,"ac\n",3,AE_MODAL_INSERT);CHECK(ae_modal_text(m,"last",4)==AE_DOCUMENT_OK);
    action(m,AE_MODAL_ESCAPE);equal(m,"ac\nlast",6,AE_MODAL_NORMAL);action(m,AE_MODAL_DELETE_LINE);equal(m,"ac",1,AE_MODAL_NORMAL);
    ae_modal_destroy(m);ae_document_destroy(d);
    m=create("abcde\nx\nabcde",&d);action(m,AE_MODAL_END);equal(m,"abcde\nx\nabcde",4,AE_MODAL_NORMAL);
    action(m,AE_MODAL_DOWN);equal(m,"abcde\nx\nabcde",6,AE_MODAL_NORMAL);action(m,AE_MODAL_DOWN);equal(m,"abcde\nx\nabcde",12,AE_MODAL_NORMAL);
    action(m,AE_MODAL_UP);action(m,AE_MODAL_UP);equal(m,"abcde\nx\nabcde",4,AE_MODAL_NORMAL);
    action(m,AE_MODAL_HOME);action(m,AE_MODAL_OPEN_BEFORE);equal(m,"\nabcde\nx\nabcde",0,AE_MODAL_INSERT);
    ae_modal_destroy(m);ae_document_destroy(d);
    m=create("a\r\nb",&d);action(m,AE_MODAL_OPEN_AFTER);equal(m,"a\r\n\r\nb",3,AE_MODAL_INSERT);
    CHECK(ae_modal_text(m,"z",1)==AE_DOCUMENT_OK);action(m,AE_MODAL_NEWLINE);equal(m,"a\r\nz\r\n\r\nb",6,AE_MODAL_INSERT);
    action(m,AE_MODAL_DELETE);equal(m,"a\r\nz\r\nb",6,AE_MODAL_INSERT);
    ae_modal_destroy(m);ae_document_destroy(d);
    m=create("؀\nb",&d);action(m,AE_MODAL_DOWN);action(m,AE_MODAL_ENTER_INSERT);action(m,AE_MODAL_BACKSPACE);
    equal(m,"؀b",3,AE_MODAL_INSERT);action(m,AE_MODAL_ESCAPE);equal(m,"؀b",0,AE_MODAL_NORMAL);
    ae_modal_destroy(m);ae_document_destroy(d);
    char lines[64];memset(lines,'\n',63);lines[63]=0;m=create(lines,&d);fail_malloc=true;
    CHECK(ae_modal_action(m,AE_MODAL_OPEN_BEFORE)==AE_DOCUMENT_NOMEM);fail_malloc=false;equal(m,lines,0,AE_MODAL_NORMAL);
    struct ae_document_info info;CHECK(ae_document_info(d,&info) && !info.dirty && info.revision==1);
    action(m,AE_MODAL_OPEN_BEFORE);CHECK(ae_modal_mode(m)==AE_MODAL_INSERT);
    ae_modal_destroy(m);ae_document_destroy(d);
    CHECK(!ae_modal_create(NULL) && !ae_modal_document(NULL));CHECK(ae_modal_action(NULL,AE_MODAL_ESCAPE)==AE_DOCUMENT_INVALID);
    puts("modal: normal/insert state, whole-grapheme edits, vertical goals, line actions, mixed endings, joined clusters and failed-open atomicity passed");return 0;
}
