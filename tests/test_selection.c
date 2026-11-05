#include "editor/modal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"selection %d: %s\n",__LINE__,#x);exit(1); } } while (0)
void *__real_malloc(size_t size);
static int allocations=-1;
void *__wrap_malloc(size_t size) { if (!allocations) return NULL;if (allocations>0) --allocations;return __real_malloc(size); }
static struct ae_document *d;static struct ae_modal *m;
static void create(const char *text,size_t size) { enum ae_document_result r;d=ae_document_create(text,size,&r);CHECK(d && r==AE_DOCUMENT_OK);m=ae_modal_create(d);CHECK(m); }
static void close_model(void) { ae_modal_destroy(m);ae_document_destroy(d); }
static void equal(const char *text,size_t size) {
    struct ae_document_info i;CHECK(ae_document_info(d,&i) && i.size==size);char *s=size ? malloc(size) : NULL;CHECK(!size || s);
    CHECK(ae_document_copy(d,0,size,s,size));if (size && memcmp(s,text,size)) { int n=(int)(size<128 ? size : 128);fprintf(stderr,"expected [%.*s], actual [%.*s]\n",n,text,n,s);CHECK(false); }free(s);
}
static void action(enum ae_modal_action a) { CHECK(ae_modal_action(m,a)==AE_DOCUMENT_OK); }
static void transfer(enum ae_modal_transfer t) { CHECK(ae_modal_transfer(m,t)==AE_DOCUMENT_OK); }
static void range(size_t begin,size_t end,bool lines) { struct ae_modal_selection s;CHECK(ae_modal_selection(m,&s) && s.active && s.begin==begin && s.end==end && s.linewise==lines); }
static void history(int direction) { bool changed;CHECK(ae_modal_history(m,direction,&changed)==AE_DOCUMENT_OK && changed); }
static struct ae_document_info info(void) { struct ae_document_info i;CHECK(ae_document_info(d,&i));return i; }
int main(void) {
    create("abcdef",6);CHECK(ae_document_replace_line_start(d,1,1,"",0)==AE_DOCUMENT_INVALID && info().revision==1);
    CHECK(ae_document_replace_line_start(d,1,2,"x",1)==AE_DOCUMENT_INVALID && info().revision==1);
    CHECK(ae_document_replace_line_start(d,0,0,"",0)==AE_DOCUMENT_OK && info().revision==1);
    CHECK(ae_modal_seek(m,3) && ae_modal_select(m,false));action(AE_MODAL_LEFT);action(AE_MODAL_LEFT);range(1,4,false);
    uint64_t revision=info().revision;transfer(AE_MODAL_YANK);CHECK(info().revision==revision && !info().dirty);
    CHECK(ae_modal_seek(m,0));transfer(AE_MODAL_PASTE_BEFORE);equal("bcdabcdef",9);history(-1);equal("abcdef",6);CHECK(!info().dirty);history(1);equal("bcdabcdef",9);close_model();

    static const char combined[]="j\xcc\x81\xf0\x9f\x87\xa9\xf0\x9f\x87\xaa!";
    create(combined,sizeof(combined)-1);CHECK(ae_modal_select(m,false));action(AE_MODAL_RIGHT);range(0,11,false);transfer(AE_MODAL_CUT);equal("!",1);
    transfer(AE_MODAL_PASTE_BEFORE);equal(combined,sizeof(combined)-1);history(-1);equal("!",1);history(-1);equal(combined,sizeof(combined)-1);close_model();

    create("abc",3);CHECK(ae_modal_select(m,false));action(AE_MODAL_RIGHT);action(AE_MODAL_DELETE);equal("c",1);
    transfer(AE_MODAL_PASTE_BEFORE);equal("abc",3);close_model();

    create("one\r\ntwo",8);action(AE_MODAL_DOWN);CHECK(ae_modal_select(m,true));range(5,8,true);transfer(AE_MODAL_CUT);equal("one",3);
    transfer(AE_MODAL_PASTE_AFTER);equal("one\r\ntwo",8);history(-1);equal("one",3);history(-1);equal("one\r\ntwo",8);close_model();
    create("one\r\ntwo\nlast",13);CHECK(ae_modal_select(m,true));action(AE_MODAL_DOWN);range(0,9,true);transfer(AE_MODAL_CHANGE);equal("\nlast",5);CHECK(ae_modal_mode(m)==AE_MODAL_INSERT);
    CHECK(info().cursor==0);CHECK(ae_modal_text(m,"new",3)==AE_DOCUMENT_OK);action(AE_MODAL_ESCAPE);equal("new\nlast",8);history(-1);equal("\nlast",5);history(-1);equal("one\r\ntwo\nlast",13);
    history(1);equal("\nlast",5);CHECK(info().cursor==0);close_model();

    create("a\n\nb",4);action(AE_MODAL_DOWN);CHECK(ae_modal_select(m,false));range(2,3,false);transfer(AE_MODAL_CUT);equal("a\nb",3);history(-1);equal("a\n\nb",4);close_model();
    create("\n",1);CHECK(ae_modal_select(m,true));revision=info().revision;transfer(AE_MODAL_CHANGE);CHECK(ae_modal_mode(m)==AE_MODAL_INSERT && info().revision==revision && !info().dirty);close_model();
    create("",0);CHECK(ae_modal_select(m,false));range(0,0,false);transfer(AE_MODAL_YANK);CHECK(!info().dirty);transfer(AE_MODAL_PASTE_AFTER);equal("",0);close_model();
    create("a\n",2);action(AE_MODAL_DOWN);CHECK(ae_modal_select(m,true));range(2,2,true);transfer(AE_MODAL_CUT);equal("a",1);history(-1);equal("a\n",2);close_model();

    /* A failed cut must preserve a different prior register and redo branch. */
    for (int fault=0;fault<2;++fault) {
        create("abc",3);CHECK(ae_modal_select(m,false));action(AE_MODAL_RIGHT);transfer(AE_MODAL_CUT);history(-1);
        CHECK(ae_modal_seek(m,1) && ae_modal_select(m,false));action(AE_MODAL_RIGHT);struct ae_document_info before=info();
        allocations=fault;CHECK(ae_modal_transfer(m,AE_MODAL_CUT)==AE_DOCUMENT_NOMEM);allocations=-1;
        CHECK(info().revision==before.revision && info().cursor==before.cursor);range(1,3,false);equal("abc",3);
        struct ae_document_history_info h;CHECK(ae_document_history_info(d,&h) && h.redo==1);
        action(AE_MODAL_ESCAPE);transfer(AE_MODAL_PASTE_BEFORE);equal("ababc",5);close_model();
    }
    char long_line[5000];memset(long_line,'x',sizeof(long_line));
    for (int fault=0;fault<3;++fault) {
        create(long_line,sizeof(long_line));CHECK(ae_modal_select(m,true));transfer(AE_MODAL_YANK);struct ae_document_info before=info();
        allocations=fault;CHECK(ae_modal_transfer(m,AE_MODAL_PASTE_AFTER)==AE_DOCUMENT_NOMEM);allocations=-1;
        CHECK(info().revision==before.revision && !info().dirty);equal(long_line,sizeof(long_line));
        transfer(AE_MODAL_PASTE_AFTER);CHECK(info().size==10001);history(-1);equal(long_line,sizeof(long_line));close_model();
    }
    char rows[63];memset(rows,'\n',sizeof(rows));
    for (int fault=0;fault<2;++fault) {
        create(rows,sizeof(rows));CHECK(ae_modal_select(m,true));transfer(AE_MODAL_YANK);allocations=fault;
        CHECK(ae_modal_transfer(m,AE_MODAL_PASTE_BEFORE)==AE_DOCUMENT_NOMEM);allocations=-1;equal(rows,sizeof(rows));CHECK(!info().dirty);close_model();
    }
    /* Independent flat-string range/concatenation reference, forward/reverse
       inclusive endpoint selection, copied paste and cut as one undo entry. */
    uint32_t rng=0x716274u;
    for (unsigned trial=0;trial<1000;++trial) {
        rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;size_t n=1+rng%90u,a=(rng>>8)%n,b=(rng>>16)%n;
        char text[90],expected[180];for (size_t i=0;i<n;++i) text[i]=(char)('a'+i%26);
        create(text,n);CHECK(ae_modal_seek(m,a) && ae_modal_select(m,false));
        for (size_t i=a;i!=b;) { action(i<b ? AE_MODAL_RIGHT : AE_MODAL_LEFT);i=i<b ? i+1 : i-1; }
        size_t lo=a<b ? a : b,hi=(a>b ? a : b)+1,size=hi-lo;range(lo,hi,false);
        if (trial&1u) {
            transfer(AE_MODAL_CUT);memcpy(expected,text,lo);memcpy(expected+lo,text+hi,n-hi);equal(expected,n-size);history(-1);equal(text,n);CHECK(!info().dirty);
        } else {
            transfer(AE_MODAL_YANK);CHECK(!info().dirty && ae_modal_seek(m,0));transfer(AE_MODAL_PASTE_BEFORE);
            memcpy(expected,text+lo,size);memcpy(expected+size,text,n);equal(expected,n+size);history(-1);equal(text,n);
        }
        close_model();
    }
    char *large=malloc(AE_DOCUMENT_BYTES);CHECK(large);memset(large,'a',AE_DOCUMENT_BYTES);
    for (size_t i=65535;i<AE_DOCUMENT_BYTES;i+=65536) large[i]='\n';
    create(large,AE_DOCUMENT_BYTES);CHECK(ae_modal_select(m,true));for (unsigned i=0;i<128;++i) action(AE_MODAL_DOWN);range(0,AE_DOCUMENT_BYTES,true);
    transfer(AE_MODAL_CUT);equal("",0);struct ae_modal_register_info reg;CHECK(ae_modal_register_info(m,&reg) && reg.size==AE_DOCUMENT_BYTES && reg.linewise);
    transfer(AE_MODAL_PASTE_AFTER);equal(large,AE_DOCUMENT_BYTES);revision=info().revision;
    CHECK(ae_modal_transfer(m,AE_MODAL_PASTE_AFTER)==AE_DOCUMENT_LIMIT && info().revision==revision);equal(large,AE_DOCUMENT_BYTES);
    history(-1);equal("",0);history(-1);equal(large,AE_DOCUMENT_BYTES);CHECK(!info().dirty);close_model();free(large);
    puts("selection: independent inclusive ranges, Unicode/CRLF/EOF, register ownership, allocation atomicity, undo and actual 8MiB transfer passed");return 0;
}
