#include "editor/document.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"document %d: %s\n",__LINE__,#x);exit(1); } } while (0)
static int budget=-1;
void *__real_malloc(size_t size);
void *__real_calloc(size_t count,size_t size);
static bool fail(void) { if (budget<0) return false;if (!budget) return true;--budget;return false; }
void *__wrap_malloc(size_t size) { return fail() ? NULL : __real_malloc(size); }
void *__wrap_calloc(size_t count,size_t size) { return fail() ? NULL : __real_calloc(count,size); }
static struct ae_document *create(const char *text,size_t size) {
    enum ae_document_result result;struct ae_document *d=ae_document_create(text,size,&result);CHECK(d && result==AE_DOCUMENT_OK);return d;
}
static void equal(struct ae_document *d,const char *text,size_t size,size_t cursor) {
    struct ae_document_info info;CHECK(ae_document_info(d,&info) && info.size==size && info.cursor==cursor);
    struct ae_document_slice s;CHECK(ae_document_slice(d,0,size,&s) && s.first_size+s.second_size==size);
    CHECK(!s.first_size || !memcmp(s.first,text,s.first_size));CHECK(!s.second_size || !memcmp(s.second,text+s.first_size,s.second_size));
    size_t row=0,start=0;
    for (size_t at=0;at<=size;++at) if (at==size || text[at]=='\n') {
        size_t ending=at<size ? 1u : 0u,content=at-start;if (ending && content && text[at-1]=='\r') { --content;++ending; }
        struct ae_document_line line;CHECK(ae_document_line(d,row,&line));CHECK(line.start==start && line.size==content && line.ending_size==ending);
        size_t found;CHECK(ae_document_row(d,start,&found) && found==row);++row;start=at+1;
    }
    CHECK(info.lines==row);
}
static void basics(void) {
    struct ae_document *d=create(NULL,0);equal(d,"",0,0);CHECK(ae_document_step(d,-1) && ae_document_step(d,1));
    CHECK(ae_document_replace(d,0,0,"ä\r\nj́\nZ",9)==AE_DOCUMENT_OK);equal(d,"ä\r\nj́\nZ",9,9);
    CHECK(!ae_document_cursor(d,1) && !ae_document_cursor(d,3));CHECK(ae_document_cursor(d,2));
    CHECK(ae_document_step(d,1));struct ae_document_info info;CHECK(ae_document_info(d,&info) && info.cursor==4);
    CHECK(ae_document_step(d,-1));CHECK(ae_document_info(d,&info) && info.cursor==2);
    CHECK(ae_document_replace(d,2,4,"",0)==AE_DOCUMENT_OK);equal(d,"äj́\nZ",7,2);
    CHECK(ae_document_info(d,&info) && info.dirty);uint64_t saved=info.revision;CHECK(ae_document_mark_clean(d,saved));
    CHECK(ae_document_replace(d,0,0,"x",1)==AE_DOCUMENT_OK);CHECK(!ae_document_mark_clean(d,saved));
    char untouched[4]={'X','X','X','X'};CHECK(!ae_document_copy(d,0,5,untouched,4));CHECK(!memcmp(untouched,"XXXX",4));
    ae_document_destroy(d);
    const char *unicode="A👩‍💻🇩🇪Z";d=create(unicode,strlen(unicode));
    for (size_t i=0;i<4;++i) { static const size_t expected[]={1,12,20,21};CHECK(ae_document_step(d,1));CHECK(ae_document_info(d,&info) && info.cursor==expected[i]); }
    CHECK(!ae_document_cursor(d,5) && ae_document_cursor(d,1));CHECK(ae_document_replace(d,1,12,"",0)==AE_DOCUMENT_OK);
    equal(d,"A🇩🇪Z",10,1);ae_document_destroy(d);
    d=create("b",1);CHECK(ae_document_replace(d,0,0,"؀",2)==AE_DOCUMENT_OK);equal(d,"؀b",3,3);
    CHECK(!ae_document_cursor(d,2));CHECK(ae_document_step(d,-1));CHECK(ae_document_replace(d,0,3,"x",1)==AE_DOCUMENT_OK);equal(d,"x",1,1);ae_document_destroy(d);
}
static void failures(void) {
    enum ae_document_result result;
    for (int i=0;i<3;++i) { budget=i;struct ae_document *d=ae_document_create("abc",3,&result);budget=-1;CHECK(!d && result==AE_DOCUMENT_NOMEM); }
    const char *bad[]={"\0","\033","\r","\xc0\x80","\xed\xa0\x80"};const size_t sizes[]={1,1,1,2,3};
    for (size_t i=0;i<5;++i) { CHECK(!ae_document_create(bad[i],sizes[i],&result) && result==AE_DOCUMENT_INVALID); }
    struct ae_document *d=create("abc",3);char text[5000];memset(text,'x',sizeof(text));
    budget=0;CHECK(ae_document_replace(d,1,2,text,sizeof(text))==AE_DOCUMENT_NOMEM);budget=-1;equal(d,"abc",3,0);
    char lines[64];memset(lines,'\n',sizeof(lines));budget=0;CHECK(ae_document_replace(d,1,2,lines,sizeof(lines))==AE_DOCUMENT_NOMEM);budget=-1;equal(d,"abc",3,0);
    for (size_t i=0;i<sizeof(text);++i) text[i]=i%50==49 ? '\n' : 'x';
    budget=1;CHECK(ae_document_replace(d,1,2,text,sizeof(text))==AE_DOCUMENT_NOMEM);budget=-1;equal(d,"abc",3,0);
    CHECK(ae_document_replace(d,1,2,text,sizeof(text))==AE_DOCUMENT_OK);
    char expected[5002];expected[0]='a';memcpy(expected+1,text,sizeof(text));expected[5001]='c';equal(d,expected,sizeof(expected),5001);ae_document_destroy(d);
    char *large=malloc(AE_DOCUMENT_LINE_BYTES+2);CHECK(large);memset(large,'a',AE_DOCUMENT_LINE_BYTES);large[AE_DOCUMENT_LINE_BYTES]='\n';large[AE_DOCUMENT_LINE_BYTES+1]='b';
    d=create(large,AE_DOCUMENT_LINE_BYTES+2);CHECK(ae_document_cursor(d,AE_DOCUMENT_LINE_BYTES));
    CHECK(ae_document_replace(d,AE_DOCUMENT_LINE_BYTES,AE_DOCUMENT_LINE_BYTES+1,"",0)==AE_DOCUMENT_LIMIT);
    equal(d,large,AE_DOCUMENT_LINE_BYTES+2,AE_DOCUMENT_LINE_BYTES);ae_document_destroy(d);
    memset(large,'\n',AE_DOCUMENT_LINES-1);d=create(large,AE_DOCUMENT_LINES-1);
    CHECK(ae_document_replace(d,0,0,"\n",1)==AE_DOCUMENT_LIMIT);equal(d,large,AE_DOCUMENT_LINES-1,0);ae_document_destroy(d);free(large);
    large=malloc(AE_DOCUMENT_BYTES);CHECK(large);for (size_t i=0;i<AE_DOCUMENT_BYTES;++i) large[i]=i%4096==4095 ? '\n' : 'a';
    d=create(large,AE_DOCUMENT_BYTES);CHECK(ae_document_replace(d,0,0,"x",1)==AE_DOCUMENT_LIMIT);equal(d,large,AE_DOCUMENT_BYTES,0);ae_document_destroy(d);free(large);
}
static uint32_t random_state=0x32b91a7d;
static unsigned next(void) { random_state=random_state*1664525u+1013904223u;return random_state; }
static void reference_edits(void) {
    struct ae_document *d=create(NULL,0);char reference[8192],insert[16];size_t size=0,cursor=0;uint64_t revision=1;
    for (unsigned n=0;n<4000;++n) {
        size_t begin=next()%(size+1),end=begin+next()%(size-begin+1),count=next()%sizeof(insert);
        for (size_t i=0;i<count;++i) { unsigned r=next()%30;insert[i]=r==28 ? '\n' : r==29 ? '\t' : (char)('a'+r%26); }
        CHECK(size-(end-begin)+count<sizeof(reference));
        CHECK(ae_document_replace(d,begin,end,insert,count)==AE_DOCUMENT_OK);
        if (begin!=end || count) { memmove(reference+begin+count,reference+end,size-end);memcpy(reference+begin,insert,count);size=size-(end-begin)+count;cursor=begin+count;++revision; }
        equal(d,reference,size,cursor);struct ae_document_info info;CHECK(ae_document_info(d,&info) && info.revision==revision);
        if (!(n%97)) CHECK(ae_document_mark_clean(d,revision));
    }
    ae_document_destroy(d);
}
int main(void) {
    basics();failures();reference_edits();puts("document: exact LF/CRLF bytes, grapheme cursors, joined clusters, gap/index edits, allocation atomicity, limits and 4000 flat-reference operations passed");return 0;
}
