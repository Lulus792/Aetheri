#include "editor/modal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"document search %d: %s\n",__LINE__,#x);exit(1); } } while (0)
void *__real_malloc(size_t size);
static bool deny;
void *__wrap_malloc(size_t size) { return deny ? NULL : __real_malloc(size); }
static struct ae_document *document(const char *text,size_t size) {
    enum ae_document_result r;struct ae_document *d=ae_document_create(text,size,&r);CHECK(d && r==AE_DOCUMENT_OK);return d;
}
static void find(struct ae_document *d,const char *q,size_t origin,int direction,bool inclusive,bool found,size_t position) {
    bool actual=false;size_t at=SIZE_MAX;deny=true;
    CHECK(ae_document_find(d,q,strlen(q),origin,direction,inclusive,&actual,&at)==AE_DOCUMENT_OK);deny=false;
    CHECK(actual==found && at==(found ? position : 0));
}
static uint32_t random_value=0x182791u;
static uint32_t rng(void) { random_value^=random_value<<13;random_value^=random_value>>17;random_value^=random_value<<5;return random_value; }
int main(void) {
    struct ae_document *d=document("ababa\r\naba",10);find(d,"aba",0,1,true,true,0);find(d,"aba",0,1,false,true,2);
    find(d,"aba",10,-1,false,true,7);find(d,"aba",2,-1,false,true,0);find(d,"aba",0,-1,false,false,0);find(d,"ABA",0,1,true,false,0);
    CHECK(ae_document_cursor(d,2));CHECK(ae_document_type(d,"x",1)==AE_DOCUMENT_OK);find(d,"aba",3,1,false,true,8);
    struct ae_document_info before,after;struct ae_document_history_info hb,ha;
    CHECK(ae_document_info(d,&before) && ae_document_history_info(d,&hb));find(d,"aba",0,1,true,true,3);
    CHECK(ae_document_info(d,&after) && ae_document_history_info(d,&ha));
    CHECK(before.size==after.size && before.cursor==after.cursor && before.revision==after.revision && before.dirty==after.dirty);
    CHECK(hb.undo==ha.undo && hb.redo==ha.redo && hb.bytes==ha.bytes);
    bool found=true;size_t pos=99;
    CHECK(ae_document_find(d,"\n",1,0,1,true,&found,&pos)==AE_DOCUMENT_INVALID && !found && !pos);
    CHECK(ae_document_find(d,"",0,0,1,true,&found,&pos)==AE_DOCUMENT_INVALID);
    CHECK(ae_document_find(d,"\xff",1,0,1,true,&found,&pos)==AE_DOCUMENT_INVALID);
    CHECK(ae_document_find(d,"x",1,0,0,true,&found,&pos)==AE_DOCUMENT_INVALID);
    CHECK(ae_document_find(d,"x",1,100,1,true,&found,&pos)==AE_DOCUMENT_INVALID);
    char oversized[257];memset(oversized,'a',sizeof(oversized));CHECK(ae_document_find(d,oversized,sizeof(oversized),0,1,true,&found,&pos)==AE_DOCUMENT_LIMIT);
    struct ae_modal *m=ae_modal_create(d);CHECK(m && ae_modal_seek(m,3));CHECK(ae_modal_action(m,AE_MODAL_ENTER_INSERT)==AE_DOCUMENT_OK && !ae_modal_seek(m,0));
    ae_modal_destroy(m);ae_document_destroy(d);
    static const char unicode[]="j\xcc\x81 j \xf0\x9f\x87\xa9\xf0\x9f\x87\xaa x";
    d=document(unicode,sizeof(unicode)-1);find(d,"j",0,1,true,true,4);find(d,"j\xcc\x81",0,1,true,true,0);find(d,"\xcc\x81",0,1,true,false,0);
    find(d,"\xf0\x9f\x87\xa9",0,1,true,false,0);find(d,"\xf0\x9f\x87\xa9\xf0\x9f\x87\xaa",0,1,true,true,6);ae_document_destroy(d);
    /* Independent flat-byte reference, overlapping and absent literals, all
       directions and inclusive/exclusive origins over random text/queries. */
    for (unsigned trial=0;trial<800;++trial) {
        char text[513],q[20];size_t size=rng()%512u,n=1+rng()%19u;
        for (size_t i=0;i<size;++i) text[i]=(rng()%8u)==0 ? '\n' : (char)('a'+rng()%3u);
        for (size_t i=0;i<n;++i) q[i]=(char)('a'+rng()%4u);
        q[n]=0;d=document(text,size);
        for (unsigned k=0;k<16;++k) {
            size_t origin=rng()%(size+1),expected=0;int dir=k&1u ? 1 : -1;bool inclusive=(k&2u)!=0,exists=false;
            for (size_t i=0;i+n<=size;++i) {
                bool eligible=dir>0 ? i>origin || (inclusive && i==origin) : i<origin || (inclusive && i==origin);
                if (eligible && !memcmp(text+i,q,n)) { expected=i;exists=true;if (dir>0) break; }
            }
            find(d,q,origin,dir,inclusive,exists,expected);
        }
        ae_document_destroy(d);
    }
    char *large=malloc(AE_DOCUMENT_BYTES);CHECK(large);memset(large,'a',AE_DOCUMENT_BYTES);
    for (size_t i=32767;i<AE_DOCUMENT_BYTES;i+=32768) large[i]='\n';
    d=document(large,AE_DOCUMENT_BYTES);char query[257];memset(query,'a',256);query[255]='b';query[256]=0;
    find(d,query,0,1,true,false,0);query[255]='a';find(d,query,AE_DOCUMENT_BYTES,-1,false,true,AE_DOCUMENT_BYTES-257);
    ae_document_destroy(d);free(large);
    puts("document search: independent reference, overlapping/gap/Unicode boundaries, maximum text and unchanged history passed");return 0;
}
