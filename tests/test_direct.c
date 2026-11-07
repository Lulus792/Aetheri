#include "editor/direct.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"direct editor %d: %s\n",__LINE__,#x);exit(1); } } while (0)
static int budget=-1;
void *__real_malloc(size_t);
void *__real_calloc(size_t,size_t);
void *__wrap_malloc(size_t n) { if (!budget) return NULL;if (budget>0) --budget;return __real_malloc(n); }
void *__wrap_calloc(size_t n,size_t s) { if (!budget) return NULL;if (budget>0) --budget;return __real_calloc(n,s); }
static struct ae_document *document(const char *text) {
    enum ae_document_result result;struct ae_document *doc=ae_document_create(text,strlen(text),&result);
    CHECK(doc && result==AE_DOCUMENT_OK);return doc;
}
static struct ae_document_info info(const struct ae_direct *edit) {
    struct ae_document_info value;CHECK(ae_document_info(ae_direct_document(edit),&value));return value;
}
static void text(const struct ae_direct *edit,const char *expected,size_t cursor) {
    struct ae_document_info value=info(edit);size_t size=strlen(expected);char *copy=malloc(size ? size : 1);
    CHECK(copy && value.size==size && value.cursor==cursor);
    CHECK(ae_document_copy(ae_direct_document(edit),0,size,copy,size) && !memcmp(copy,expected,size));free(copy);
}
static struct ae_direct_token token(const struct ae_direct *edit) {
    struct ae_direct_token result;CHECK(ae_direct_token(edit,&result));return result;
}
static void same_token(const struct ae_direct *edit,struct ae_direct_token old) {
    struct ae_direct_token now=token(edit);
    CHECK(now.identity==old.identity && now.interaction==old.interaction && now.revision==old.revision &&
        now.cursor==old.cursor && now.anchor==old.anchor && now.selecting==old.selecting);
}
static void selection(const struct ae_direct *edit,size_t begin,size_t end) {
    struct ae_direct_selection value;CHECK(ae_direct_selection(edit,&value));
    CHECK(value.begin==begin && value.end==end && value.active==(begin!=end));
}
static void destroy(struct ae_direct *edit,struct ae_document *doc) { ae_direct_destroy(edit);ae_document_destroy(doc); }
static void direct_entry_and_history(void) {
    struct ae_document *doc=document("");struct ae_direct *edit=ae_direct_create(doc);CHECK(edit);
    CHECK(ae_direct_text(edit,"h",1)==AE_DOCUMENT_OK && ae_direct_text(edit,"i",1)==AE_DOCUMENT_OK);text(edit,"hi",2);
    struct ae_document_history_info history;CHECK(ae_document_history_info(doc,&history) && history.undo==1);
    bool changed=false;CHECK(ae_direct_history(edit,-1,&changed)==AE_DOCUMENT_OK && changed);text(edit,"",0);CHECK(!info(edit).dirty);
    CHECK(ae_direct_history(edit,1,&changed)==AE_DOCUMENT_OK && changed);text(edit,"hi",2);
    CHECK(ae_direct_cancel_input(edit) && ae_document_mark_clean(doc,info(edit).revision));
    CHECK(ae_direct_text(edit,"!",1)==AE_DOCUMENT_OK);CHECK(info(edit).dirty);text(edit,"hi!",3);
    CHECK(ae_direct_history(edit,-1,&changed)==AE_DOCUMENT_OK && changed && !info(edit).dirty);text(edit,"hi",2);
    destroy(edit,doc);
}
static void unicode_and_endings(void) {
    const char *original="Ae\xcc\x81\xf0\x9f\x91\xa9\xe2\x80\x8d\xf0\x9f\x94\xac\r\n\xce\xb2";
    size_t emoji_end=strlen("Ae\xcc\x81\xf0\x9f\x91\xa9\xe2\x80\x8d\xf0\x9f\x94\xac");
    struct ae_document *doc=document(original);struct ae_direct *edit=ae_direct_create(doc);CHECK(edit);
    CHECK(ae_direct_move(edit,AE_DIRECT_RIGHT,false));text(edit,original,1);
    CHECK(ae_direct_move(edit,AE_DIRECT_RIGHT,false));text(edit,original,4);
    struct ae_direct_token before=token(edit);CHECK(!ae_direct_seek(edit,2,false));same_token(edit,before);
    CHECK(ae_direct_move(edit,AE_DIRECT_RIGHT,false));text(edit,original,emoji_end);
    CHECK(ae_direct_move(edit,AE_DIRECT_RIGHT,false));text(edit,original,emoji_end+2);
    CHECK(ae_direct_delete(edit,true)==AE_DOCUMENT_OK);text(edit,"Ae\xcc\x81\xf0\x9f\x91\xa9\xe2\x80\x8d\xf0\x9f\x94\xac\xce\xb2",emoji_end);
    bool changed=false;CHECK(ae_direct_history(edit,-1,&changed)==AE_DOCUMENT_OK && changed);text(edit,original,emoji_end+2);
    CHECK(ae_direct_move(edit,AE_DIRECT_LEFT,false) && ae_direct_delete(edit,false)==AE_DOCUMENT_OK);
    CHECK(ae_direct_history(edit,-1,&changed)==AE_DOCUMENT_OK && changed);
    CHECK(ae_direct_seek(edit,emoji_end,false) && ae_direct_delete(edit,true)==AE_DOCUMENT_OK);text(edit,"Ae\xcc\x81\r\n\xce\xb2",4);
    CHECK(ae_direct_history(edit,-1,&changed)==AE_DOCUMENT_OK && changed);
    CHECK(ae_direct_seek(edit,4,false) && ae_direct_delete(edit,true)==AE_DOCUMENT_OK);text(edit,"A\xf0\x9f\x91\xa9\xe2\x80\x8d\xf0\x9f\x94\xac\r\n\xce\xb2",1);
    destroy(edit,doc);
}
static void selection_and_replacement(void) {
    struct ae_document *doc=document("abcdef");struct ae_direct *edit=ae_direct_create(doc);CHECK(edit);
    CHECK(ae_direct_seek(edit,1,false) && ae_direct_seek(edit,4,true));selection(edit,1,4);
    CHECK(ae_direct_move(edit,AE_DIRECT_LEFT,false));selection(edit,1,1);text(edit,"abcdef",1);
    CHECK(ae_direct_seek(edit,4,false) && ae_direct_seek(edit,1,true));selection(edit,1,4);
    CHECK(ae_direct_move(edit,AE_DIRECT_RIGHT,false));text(edit,"abcdef",4);selection(edit,4,4);
    CHECK(ae_direct_seek(edit,1,true) && ae_direct_text(edit,"X",1)==AE_DOCUMENT_OK);text(edit,"aXef",2);selection(edit,2,2);
    bool changed=false;CHECK(ae_direct_history(edit,-1,&changed)==AE_DOCUMENT_OK && changed);text(edit,"abcdef",1);
    CHECK(ae_direct_select_all(edit));selection(edit,0,6);CHECK(ae_direct_delete(edit,false)==AE_DOCUMENT_OK);text(edit,"",0);
    CHECK(ae_direct_select_all(edit));selection(edit,0,0);CHECK(ae_direct_delete(edit,true)==AE_DOCUMENT_OK);text(edit,"",0);
    destroy(edit,doc);
}
static void vertical_goal_and_newline(void) {
    const char *original="a\xcc\x81" "bc\r\nq\r\n\r\n\xce\xb1\xce\xb2\xce\xb3\xce\xb4";
    struct ae_document *doc=document(original);struct ae_direct *edit=ae_direct_create(doc);CHECK(edit);
    CHECK(ae_direct_move(edit,AE_DIRECT_END,false));CHECK(info(edit).cursor==5);
    CHECK(ae_direct_move(edit,AE_DIRECT_DOWN,false));CHECK(info(edit).cursor==8);
    CHECK(ae_direct_move(edit,AE_DIRECT_DOWN,false));CHECK(info(edit).cursor==10);
    CHECK(ae_direct_move(edit,AE_DIRECT_DOWN,false));CHECK(info(edit).cursor==18);
    CHECK(ae_direct_move(edit,AE_DIRECT_UP,true));selection(edit,10,18);
    CHECK(ae_direct_move(edit,AE_DIRECT_UP,true));selection(edit,8,18);
    CHECK(ae_direct_clear_selection(edit) && ae_direct_move(edit,AE_DIRECT_FIRST,false));
    CHECK(ae_direct_newline(edit)==AE_DOCUMENT_OK);text(edit,"\r\na\xcc\x81" "bc\r\nq\r\n\r\n\xce\xb1\xce\xb2\xce\xb3\xce\xb4",2);
    CHECK(ae_direct_move(edit,AE_DIRECT_LAST,false) && ae_direct_newline(edit)==AE_DOCUMENT_OK);
    size_t size=info(edit).size;char ending[2];CHECK(ae_document_copy(doc,size-2,2,ending,2) && !memcmp(ending,"\r\n",2));
    destroy(edit,doc);
}
static void correlated_paste(void) {
    struct ae_document *doc=document("abc"),*other_doc=document("abc");
    struct ae_direct *edit=ae_direct_create(doc),*other=ae_direct_create(other_doc);CHECK(edit && other);
    struct ae_direct_token before=token(edit);CHECK(ae_direct_paste(other,&before,"x",1)==AE_DOCUMENT_INVALID);
    CHECK(ae_direct_move(edit,AE_DIRECT_LEFT,false));CHECK(ae_direct_paste(edit,&before,"x",1)==AE_DOCUMENT_INVALID);
    CHECK(ae_direct_seek(edit,1,false) && ae_direct_seek(edit,2,true));before=token(edit);
    CHECK(ae_direct_cancel_input(edit));selection(edit,1,2);CHECK(ae_direct_paste(edit,&before,"x",1)==AE_DOCUMENT_INVALID);
    before=token(edit);CHECK(ae_direct_paste(edit,&before,"\033",1)==AE_DOCUMENT_INVALID);same_token(edit,before);selection(edit,1,2);
    CHECK(ae_direct_paste(edit,&before,"XYZ",3)==AE_DOCUMENT_OK);text(edit,"aXYZc",4);
    CHECK(ae_direct_paste(edit,&before,"XYZ",3)==AE_DOCUMENT_INVALID);
    CHECK(ae_direct_text(edit,"!",1)==AE_DOCUMENT_OK);bool changed=false;
    CHECK(ae_direct_history(edit,-1,&changed)==AE_DOCUMENT_OK && changed);text(edit,"aXYZc",4);
    CHECK(ae_direct_history(edit,-1,&changed)==AE_DOCUMENT_OK && changed);text(edit,"abc",2);
    destroy(other,other_doc);destroy(edit,doc);
}
static void failures_preserve_editing(void) {
    struct ae_document *doc=document("original");budget=0;CHECK(!ae_direct_create(doc));budget=-1;
    struct ae_direct *edit=ae_direct_create(doc);CHECK(edit && ae_direct_select_all(edit));
    struct ae_direct_token before=token(edit);struct ae_document_history_info history;CHECK(ae_document_history_info(doc,&history));
    budget=0;CHECK(ae_direct_text(edit,"replacement",11)==AE_DOCUMENT_NOMEM);budget=-1;
    same_token(edit,before);text(edit,"original",8);selection(edit,0,8);
    struct ae_document_history_info after;CHECK(ae_document_history_info(doc,&after));
    CHECK(after.undo==history.undo && after.redo==history.redo && after.bytes==history.bytes);
    const char *bad[]={"\0","\033","\r","\xc0\x80","\xed\xa0\x80"};const size_t sizes[]={1,1,1,2,3};
    for (size_t i=0;i<5;++i) { CHECK(ae_direct_text(edit,bad[i],sizes[i])==AE_DOCUMENT_INVALID);same_token(edit,before); }
    CHECK(ae_direct_text(edit,"",0)==AE_DOCUMENT_INVALID && ae_direct_text(edit,NULL,1)==AE_DOCUMENT_INVALID);
    CHECK(!ae_direct_move(edit,(enum ae_direct_move)999,false));same_token(edit,before);
    bool changed=true;CHECK(ae_direct_history(edit,0,&changed)==AE_DOCUMENT_INVALID && changed);same_token(edit,before);
    budget=0;CHECK(ae_direct_delete(edit,false)==AE_DOCUMENT_NOMEM);budget=-1;same_token(edit,before);text(edit,"original",8);
    destroy(edit,doc);
}
static void owner_identity_is_not_an_address(void) {
    struct ae_document *doc=document("same");struct ae_direct *edit=ae_direct_create(doc);CHECK(edit);
    struct ae_direct_token original=token(edit);ae_direct_destroy(edit);
    for (unsigned i=0;i<1024;++i) {
        edit=ae_direct_create(doc);CHECK(edit && token(edit).identity!=original.identity);
        CHECK(ae_direct_paste(edit,&original,"x",1)==AE_DOCUMENT_INVALID);ae_direct_destroy(edit);
    }
    ae_document_destroy(doc);CHECK(!ae_direct_create(NULL));
    CHECK(!ae_direct_seek(NULL,0,false) && ae_direct_text(NULL,"x",1)==AE_DOCUMENT_INVALID);
}
int main(void) {
    direct_entry_and_history();unicode_and_endings();selection_and_replacement();vertical_goal_and_newline();
    correlated_paste();failures_preserve_editing();owner_identity_is_not_an_address();
    puts("direct graphical-editor input: 7 workflows passed");return 0;
}
