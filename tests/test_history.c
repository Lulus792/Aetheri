#include "editor/document.h"
#include "editor/modal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"history %d: %s\n",__LINE__,#x);exit(1); } } while (0)
static int budget=-1;
void *__real_malloc(size_t size);
void *__wrap_malloc(size_t size) { if (!budget) return NULL;if (budget>0) --budget;return __real_malloc(size); }
static struct ae_document *create(const char *text) { enum ae_document_result r;struct ae_document *d=ae_document_create(text,strlen(text),&r);CHECK(d && r==AE_DOCUMENT_OK);return d; }
static struct ae_document_info info(struct ae_document *d) { struct ae_document_info i;CHECK(ae_document_info(d,&i));return i; }
static struct ae_document_history_info history_info(struct ae_document *d) { struct ae_document_history_info i;CHECK(ae_document_history_info(d,&i));return i; }
static void equal(struct ae_document *d,const char *text,size_t cursor,bool dirty) {
    struct ae_document_info i=info(d);CHECK(i.size==strlen(text) && i.cursor==cursor && i.dirty==dirty);
    struct ae_document_slice s;CHECK(ae_document_slice(d,0,i.size,&s));
    CHECK(!s.first_size || !memcmp(s.first,text,s.first_size));CHECK(!s.second_size || !memcmp(s.second,text+s.first_size,s.second_size));
}
static void traverse(struct ae_document *d,int direction,bool expected) { bool changed=!expected;CHECK(ae_document_history(d,direction,&changed)==AE_DOCUMENT_OK && changed==expected); }
static void basic(void) {
    struct ae_document *d=create("");traverse(d,-1,false);CHECK(ae_document_type(d,"a",1)==AE_DOCUMENT_OK && ae_document_type(d,"j́",3)==AE_DOCUMENT_OK);
    equal(d,"aj́",4,true);CHECK(history_info(d).undo==1);uint64_t old=info(d).revision;
    budget=0;traverse(d,-1,true);equal(d,"",0,false);traverse(d,1,true);equal(d,"aj́",4,true);budget=-1;
    CHECK(info(d).revision==old+2 && !ae_document_mark_clean(d,old));CHECK(ae_document_mark_clean(d,info(d).revision));
    CHECK(ae_document_type(d,"x",1)==AE_DOCUMENT_OK && history_info(d).undo==2);traverse(d,-1,true);equal(d,"aj́",4,false);
    traverse(d,1,true);equal(d,"aj́x",5,true);traverse(d,-1,true);
    struct ae_document_history_info before=history_info(d);budget=0;
    CHECK(ae_document_type(d,"q",1)==AE_DOCUMENT_NOMEM);budget=-1;CHECK(history_info(d).undo==before.undo && history_info(d).redo==before.redo);
    traverse(d,1,true);equal(d,"aj́x",5,true);traverse(d,-1,true);
    CHECK(ae_document_type(d,"b",1)==AE_DOCUMENT_OK);CHECK(history_info(d).redo==0);traverse(d,1,false);equal(d,"aj́b",5,true);ae_document_destroy(d);
    d=create("");CHECK(ae_document_type(d,"x",1)==AE_DOCUMENT_OK);char chars[64];memset(chars,'y',sizeof(chars));
    budget=0;CHECK(ae_document_type(d,chars,sizeof(chars))==AE_DOCUMENT_NOMEM);budget=-1;equal(d,"x",1,true);CHECK(history_info(d).undo==1);
    budget=0;CHECK(ae_document_type(d,"z",1)==AE_DOCUMENT_OK);budget=-1;equal(d,"xz",2,true);traverse(d,-1,true);equal(d,"",0,false);ae_document_destroy(d);
    CHECK(!ae_document_history_info(NULL,&before));bool changed=true;CHECK(ae_document_history(NULL,-1,&changed)==AE_DOCUMENT_INVALID && !changed);
}
static void joined(void) {
    struct ae_document *d=create("a");CHECK(ae_document_cursor(d,1));CHECK(ae_document_type(d,"́",2)==AE_DOCUMENT_OK);equal(d,"á",3,true);
    CHECK(ae_document_replace(d,1,3,"",0)==AE_DOCUMENT_INVALID);traverse(d,-1,true);equal(d,"a",1,false);traverse(d,1,true);equal(d,"á",3,true);ae_document_destroy(d);
    d=create("b");CHECK(ae_document_type(d,"؀",2)==AE_DOCUMENT_OK);equal(d,"؀b",3,true);CHECK(ae_document_type(d,"x",1)==AE_DOCUMENT_OK && history_info(d).undo==2);
    traverse(d,-1,true);equal(d,"؀b",3,true);traverse(d,-1,true);equal(d,"b",0,false);traverse(d,1,true);equal(d,"؀b",3,true);ae_document_destroy(d);
    d=create("🇩");CHECK(ae_document_cursor(d,4));CHECK(ae_document_type(d,"🇪",4)==AE_DOCUMENT_OK);traverse(d,-1,true);equal(d,"🇩",4,false);traverse(d,1,true);equal(d,"🇩🇪",8,true);ae_document_destroy(d);
    d=create("؀\r\nb");CHECK(ae_document_cursor(d,2));CHECK(ae_document_replace(d,2,4,"",0)==AE_DOCUMENT_OK);equal(d,"؀b",3,true);
    traverse(d,-1,true);equal(d,"؀\r\nb",2,false);traverse(d,1,true);equal(d,"؀b",3,true);ae_document_destroy(d);
}
static void boundaries(void) {
    struct ae_document *d=create("");
    for (unsigned n=0;n<AE_DOCUMENT_HISTORY+2u;++n) { CHECK(ae_document_replace(d,0,0,"x",1)==AE_DOCUMENT_OK);if (n==1) CHECK(ae_document_mark_clean(d,info(d).revision)); }
    CHECK(history_info(d).undo==AE_DOCUMENT_HISTORY && history_info(d).bytes<=AE_DOCUMENT_HISTORY_BYTES);
    for (unsigned n=0;n<AE_DOCUMENT_HISTORY;++n) traverse(d,-1,true);
    equal(d,"xx",1,false);traverse(d,-1,false);
    for (unsigned n=0;n<AE_DOCUMENT_HISTORY;++n) traverse(d,1,true);
    CHECK(info(d).size==AE_DOCUMENT_HISTORY+2u);ae_document_destroy(d);
    char *a=malloc(AE_DOCUMENT_BYTES),*b=malloc(AE_DOCUMENT_BYTES);CHECK(a && b);
    for (size_t n=0;n<AE_DOCUMENT_BYTES;++n) { a[n]=n%4096==4095 ? '\n' : 'a';b[n]=n%4096==4095 ? '\n' : 'b'; }
    enum ae_document_result r;d=ae_document_create(a,AE_DOCUMENT_BYTES,&r);CHECK(d && r==AE_DOCUMENT_OK);
    CHECK(ae_document_replace(d,0,AE_DOCUMENT_BYTES,b,AE_DOCUMENT_BYTES)==AE_DOCUMENT_OK);
    CHECK(ae_document_replace(d,0,AE_DOCUMENT_BYTES,a,AE_DOCUMENT_BYTES)==AE_DOCUMENT_OK);
    CHECK(history_info(d).undo==1 && history_info(d).bytes<=AE_DOCUMENT_HISTORY_BYTES);
    budget=0;traverse(d,-1,true);traverse(d,1,true);budget=-1;
    CHECK(info(d).size==AE_DOCUMENT_BYTES && info(d).dirty);CHECK(ae_document_copy(d,0,AE_DOCUMENT_BYTES,b,AE_DOCUMENT_BYTES) && !memcmp(a,b,AE_DOCUMENT_BYTES));
    ae_document_destroy(d);free(a);free(b);
}
struct reference { char before[2048],after[2048];size_t before_size,after_size,before_cursor,after_cursor;uint64_t before_state,after_state; };
static uint32_t random_state=0xb73ea611;
static unsigned random_next(void) { random_state^=random_state<<13;random_state^=random_state>>17;random_state^=random_state<<5;return random_state; }
static void independent(void) {
    struct reference *entries=calloc(128,sizeof(*entries));CHECK(entries);struct ae_document *d=create("");
    char text[2048]={0},insert[8];size_t size=0,cursor=0,at=0,used=0;uint64_t revision=1,state=1,clean=1;
    unsigned counts[5]={0};
    for (unsigned n=0;n<5000;++n) {
        unsigned op=random_next()%8;if (at==128 && op>=4) op=0;
        if (op<=1) {
            ++counts[op];
            bool expected=op==0 ? at>0 : at<used;traverse(d,op==0 ? -1 : 1,expected);
            if (expected) { struct reference *r=&entries[op==0 ? at-1 : at];++revision;state=op==0 ? r->before_state : r->after_state;cursor=op==0 ? r->before_cursor : r->after_cursor;size=op==0 ? r->before_size : r->after_size;memcpy(text,op==0 ? r->before : r->after,size);at=op==0 ? at-1 : at+1; }
        } else if (op==2) { ++counts[2];CHECK(ae_document_mark_clean(d,revision));clean=state; }
        else if (op==3) { ++counts[3];cursor=random_next()%(size+1);CHECK(ae_document_cursor(d,cursor)); }
        else {
            ++counts[4];
            size_t begin=random_next()%(size+1),end=begin+random_next()%(size-begin+1),count=1+random_next()%sizeof(insert);CHECK(size-(end-begin)+count<sizeof(text));
            for (size_t i=0;i<count;++i) { unsigned c=random_next()%28;insert[i]=c==27 ? '\n' : c==26 ? '\t' : (char)('a'+c); }
            struct reference *r=&entries[at];r->before_size=size;r->before_cursor=cursor;r->before_state=state;memcpy(r->before,text,size);
            CHECK(ae_document_replace(d,begin,end,insert,count)==AE_DOCUMENT_OK);memmove(text+begin+count,text+end,size-end);memcpy(text+begin,insert,count);
            size=size-(end-begin)+count;cursor=begin+count;state=++revision;r->after_state=state;r->after_size=size;r->after_cursor=cursor;memcpy(r->after,text,size);used=++at;
        }
        text[size]=0;equal(d,text,cursor,state!=clean);CHECK(info(d).revision==revision);struct ae_document_history_info h=history_info(d);CHECK(h.undo==at && h.redo==used-at);
    }
    for (size_t i=0;i<5;++i) CHECK(counts[i]>100);
    ae_document_destroy(d);free(entries);
}
static void modal(void) {
    struct ae_document *d=create("old");struct ae_modal *m=ae_modal_create(d);CHECK(m);
    CHECK(ae_modal_action(m,AE_MODAL_LINE_APPEND)==AE_DOCUMENT_OK && ae_modal_text(m,"ab",2)==AE_DOCUMENT_OK && ae_modal_text(m,"c",1)==AE_DOCUMENT_OK);
    bool changed;CHECK(ae_modal_history(m,-1,&changed)==AE_DOCUMENT_OK && changed && ae_modal_mode(m)==AE_MODAL_NORMAL);equal(d,"old",2,false);
    CHECK(ae_modal_history(m,1,&changed)==AE_DOCUMENT_OK && changed);equal(d,"oldabc",5,true);
    ae_modal_destroy(m);ae_document_destroy(d);
}
int main(void) { basic();joined();boundaries();independent();modal();puts("history: deltas, grouping, saved-state identity, Unicode joins, allocation atomicity, budgets and 5000 independent reference transitions passed");return 0; }
