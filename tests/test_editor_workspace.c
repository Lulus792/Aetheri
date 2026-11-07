#include "editor/workspace.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"editor workspace %d: %s (errno %d)\n",__LINE__,#x,errno);exit(1); } } while (0)
#define OK AE_EDITOR_WORKSPACE_OK
#define REVIEW AE_EDITOR_WORKSPACE_REVIEW
static int budget=-1,sync_fault,syncs;static bool no_space;
void *__real_malloc(size_t);
void *__real_calloc(size_t,size_t);
int __real_fsync(int);
ssize_t __real_write(int,const void *,size_t);
void *__wrap_malloc(size_t n) { if (!budget) return NULL;if (budget>0) --budget;return __real_malloc(n); }
void *__wrap_calloc(size_t n,size_t s) { if (!budget) return NULL;if (budget>0) --budget;return __real_calloc(n,s); }
int __wrap_fsync(int fd) { int result=__real_fsync(fd);if (++syncs==sync_fault) { errno=EIO;return -1; }return result; }
ssize_t __wrap_write(int fd,const void *p,size_t n) { if (no_space) { no_space=false;errno=ENOSPC;return -1; }return __real_write(fd,p,n); }
static void put(int dir,const char *leaf,const char *text) {
    int fd=openat(dir,leaf,O_WRONLY|O_CREAT|O_TRUNC|O_CLOEXEC,0600);size_t size=strlen(text);
    CHECK(fd>=0 && __real_write(fd,text,size)==(ssize_t)size && close(fd)==0);
}
static void disk(int dir,const char *leaf,const char *expected) {
    int fd=openat(dir,leaf,O_RDONLY|O_CLOEXEC);char buffer[256];CHECK(fd>=0);
    ssize_t n=read(fd,buffer,sizeof(buffer));CHECK(n>=0 && (size_t)n==strlen(expected) && !memcmp(buffer,expected,(size_t)n));CHECK(close(fd)==0);
}
static struct ae_editor_tab tab(struct ae_editor_workspace *w,uint64_t id) {
    struct ae_editor_tab value;CHECK(ae_editor_workspace_get(w,id,&value));return value;
}
static struct ae_document_info info(struct ae_editor_workspace *w,uint64_t id) {
    struct ae_document_info value;CHECK(ae_document_info(tab(w,id).document,&value));return value;
}
static void content(struct ae_editor_workspace *w,uint64_t id,const char *expected) {
    const struct ae_document *doc=tab(w,id).document;struct ae_document_info value;char buffer[256];CHECK(ae_document_info(doc,&value));
    CHECK(value.size==strlen(expected) && ae_document_copy(doc,0,value.size,buffer,sizeof(buffer)) && !memcmp(buffer,expected,value.size));
}
static void typing(struct ae_editor_workspace *w,const char *text) { CHECK(ae_direct_text(ae_editor_workspace_input(w),text,strlen(text))==AE_DOCUMENT_OK); }
static enum ae_editor_workspace_result save_new(struct ae_editor_workspace *w,uint64_t id,int directory,const char *leaf) {
    struct ae_editor_destination_token token;enum ae_editor_workspace_result r=ae_editor_workspace_destination(w,id,&token);
    return r==OK ? ae_editor_workspace_save_new(w,&token,directory,leaf) : r;
}
static void empty_and_capacity(void) {
    struct ae_editor_workspace *w=ae_editor_workspace_create();CHECK(w && !ae_editor_workspace_count(w) && !ae_editor_workspace_input(w));
    uint64_t ids[AE_EDITOR_TABS],out=999;
    for (unsigned i=0;i<AE_EDITOR_TABS;++i) CHECK(ae_editor_workspace_new(w,&ids[i])==OK);
    CHECK(ae_editor_workspace_new(w,&out)==AE_EDITOR_WORKSPACE_LIMIT && out==999 && ae_editor_workspace_count(w)==AE_EDITOR_TABS);
    struct ae_editor_close_token closing;
    CHECK(ae_editor_workspace_request_close(w,ids[3],&closing)==OK && !closing.id);
    CHECK(ae_editor_workspace_new(w,&out)==OK && out!=ids[3]);
    struct ae_editor_tab current;CHECK(ae_editor_workspace_at(w,AE_EDITOR_TABS-1,&current) && current.id==out);
    while (ae_editor_workspace_count(w)) { CHECK(ae_editor_workspace_at(w,0,&current));CHECK(ae_editor_workspace_request_close(w,current.id,&closing)==OK); }
    CHECK(!ae_editor_workspace_active(w) && !ae_editor_workspace_input(w));
    CHECK(ae_editor_workspace_new(w,&out)==OK && out!=ids[0]);ae_editor_workspace_destroy(w);
}
static void independent_tabs_and_input(int dir,int other) {
    put(dir,"notes","old");put(other,"notes","other");
    struct ae_editor_workspace *w=ae_editor_workspace_create();uint64_t first,second,reused=0;CHECK(w);
    CHECK(ae_editor_workspace_open(w,dir,"notes",&first)==OK);typing(w,"new ");content(w,first,"new old");
    struct ae_direct_token input;CHECK(ae_direct_token(ae_editor_workspace_input(w),&input));
    CHECK(ae_editor_workspace_open(w,other,"notes",&second)==OK && first!=second && ae_editor_workspace_count(w)==2);
    CHECK(ae_direct_paste(ae_editor_workspace_input(w),&input,"wrong",5)==AE_DOCUMENT_INVALID);
    int alias=dup(dir);CHECK(alias>=0 && ae_editor_workspace_open(w,alias,"notes",&reused)==OK && close(alias)==0);
    CHECK(reused==first && ae_editor_workspace_count(w)==2 && ae_editor_workspace_active(w)==first);content(w,first,"new old");
    int selected=openat(dir,"notes",O_PATH|O_CLOEXEC),wrong=openat(other,"notes",O_PATH|O_CLOEXEC);CHECK(selected>=0 && wrong>=0);
    reused=999;CHECK(ae_editor_workspace_open_selected(w,dir,"notes",wrong,&reused)==AE_EDITOR_WORKSPACE_CONFLICT && reused==999);
    CHECK(ae_editor_workspace_open_selected(w,dir,"notes",selected,&reused)==OK && reused==first);
    CHECK(close(selected)==0 && close(wrong)==0);
    CHECK(ae_direct_paste(ae_editor_workspace_input(w),&input,"wrong",5)==AE_DOCUMENT_INVALID);
    CHECK(ae_editor_workspace_select(w,second)==OK);put(dir,"notes","external");reused=123;
    CHECK(ae_editor_workspace_open(w,dir,"notes",&reused)==AE_EDITOR_WORKSPACE_CONFLICT && reused==123);
    CHECK(ae_editor_workspace_active(w)==second && ae_editor_workspace_count(w)==2);content(w,first,"new old");disk(dir,"notes","external");
    ae_editor_workspace_destroy(w);
}
static void save_and_create_only(int dir) {
    struct ae_editor_workspace *w=ae_editor_workspace_create();uint64_t id;CHECK(w && ae_editor_workspace_new(w,&id)==OK);typing(w,"hello\r\n");
    CHECK(ae_editor_workspace_save(w,id)==AE_EDITOR_WORKSPACE_NEEDS_PATH);
    put(dir,"occupied","keep");CHECK(save_new(w,id,dir,"occupied")==AE_EDITOR_WORKSPACE_CONFLICT);
    CHECK(!tab(w,id).leaf && info(w,id).dirty);disk(dir,"occupied","keep");
    no_space=true;CHECK(save_new(w,id,dir,"failed-new")==AE_EDITOR_WORKSPACE_IO);
    CHECK(!tab(w,id).leaf && info(w,id).dirty && faccessat(dir,"failed-new",F_OK,0)!=0 && errno==ENOENT);
    CHECK(save_new(w,id,dir,"created")==OK && !info(w,id).dirty);disk(dir,"created","hello\r\n");
    CHECK(!strcmp(tab(w,id).leaf,"created"));typing(w,"world");CHECK(ae_editor_workspace_save(w,id)==OK);disk(dir,"created","hello\r\nworld");
    uint64_t same=0;CHECK(ae_editor_workspace_open(w,dir,"created",&same)==OK && same==id && ae_editor_workspace_count(w)==1);
    struct ae_editor_close_token closing;CHECK(ae_editor_workspace_request_close(w,id,&closing)==OK && !ae_editor_workspace_count(w));disk(dir,"created","hello\r\nworld");
    ae_editor_workspace_destroy(w);
}
static void close_review_and_save_path(int dir) {
    struct ae_editor_workspace *w=ae_editor_workspace_create();uint64_t first,second,unchanged=567;CHECK(w);
    CHECK(ae_editor_workspace_new(w,&first)==OK);typing(w,"first");CHECK(ae_editor_workspace_new(w,&second)==OK);typing(w,"second");
    struct ae_editor_close_token closing,again,bad;
    CHECK(ae_editor_workspace_request_close(w,first,&closing)==REVIEW && ae_editor_workspace_active(w)==first && !ae_editor_workspace_input(w));
    CHECK(ae_editor_workspace_request_close(w,first,&again)==REVIEW && closing.serial==again.serial);
    CHECK(ae_editor_workspace_new(w,&unchanged)==AE_EDITOR_WORKSPACE_BUSY && unchanged==567);
    CHECK(ae_editor_workspace_select(w,second)==AE_EDITOR_WORKSPACE_BUSY);
    CHECK(ae_editor_workspace_request_close(w,second,&again)==AE_EDITOR_WORKSPACE_BUSY);
    CHECK(save_new(w,second,dir,"wrong-tab")==AE_EDITOR_WORKSPACE_BUSY);
    bad=closing;++bad.serial;CHECK(ae_editor_workspace_resolve_close(w,&bad,AE_EDITOR_CLOSE_DISCARD)==AE_EDITOR_WORKSPACE_INVALID);
    bad=closing;bad.id=second;CHECK(ae_editor_workspace_resolve_close(w,&bad,AE_EDITOR_CLOSE_DISCARD)==AE_EDITOR_WORKSPACE_INVALID);
    bad=closing;++bad.revision;CHECK(ae_editor_workspace_resolve_close(w,&bad,AE_EDITOR_CLOSE_DISCARD)==AE_EDITOR_WORKSPACE_INVALID);
    CHECK(ae_editor_workspace_resolve_close(w,&closing,AE_EDITOR_CLOSE_CANCEL)==OK && ae_editor_workspace_input(w));content(w,first,"first");
    CHECK(ae_editor_workspace_request_close(w,first,&again)==REVIEW && again.serial!=closing.serial);
    CHECK(ae_editor_workspace_resolve_close(w,&closing,AE_EDITOR_CLOSE_DISCARD)==AE_EDITOR_WORKSPACE_INVALID);
    CHECK(ae_editor_workspace_resolve_close(w,&again,AE_EDITOR_CLOSE_SAVE)==AE_EDITOR_WORKSPACE_NEEDS_PATH);
    CHECK(save_new(w,first,dir,"review-save")==OK);
    syncs=0;CHECK(ae_editor_workspace_resolve_close(w,&again,AE_EDITOR_CLOSE_SAVE)==OK && syncs==0);
    CHECK(ae_editor_workspace_count(w)==1 && ae_editor_workspace_active(w)==second);content(w,second,"second");disk(dir,"review-save","first");
    CHECK(ae_editor_workspace_resolve_close(w,&again,AE_EDITOR_CLOSE_DISCARD)==AE_EDITOR_WORKSPACE_INVALID);
    CHECK(ae_editor_workspace_request_close(w,second,&closing)==REVIEW && ae_editor_workspace_resolve_close(w,&closing,AE_EDITOR_CLOSE_DISCARD)==OK);
    CHECK(!ae_editor_workspace_count(w) && !ae_editor_workspace_review(w,&closing));ae_editor_workspace_destroy(w);
}
static void discard_and_conflicting_save(int dir) {
    put(dir,"discard","original");struct ae_editor_workspace *w=ae_editor_workspace_create();uint64_t id;CHECK(w && ae_editor_workspace_open(w,dir,"discard",&id)==OK);
    typing(w,"changed ");struct ae_editor_close_token closing;CHECK(ae_editor_workspace_request_close(w,id,&closing)==REVIEW);
    put(dir,"discard","external");CHECK(ae_editor_workspace_resolve_close(w,&closing,AE_EDITOR_CLOSE_SAVE)==AE_EDITOR_WORKSPACE_CONFLICT);
    content(w,id,"changed original");disk(dir,"discard","external");CHECK(ae_editor_workspace_review(w,&closing));
    CHECK(ae_editor_workspace_resolve_close(w,&closing,AE_EDITOR_CLOSE_DISCARD)==OK);disk(dir,"discard","external");ae_editor_workspace_destroy(w);
}
static void uncertain_even_when_clean(int dir) {
    struct ae_editor_workspace *w=ae_editor_workspace_create();uint64_t id;CHECK(w && ae_editor_workspace_new(w,&id)==OK && !info(w,id).dirty);
    syncs=0;sync_fault=3;CHECK(save_new(w,id,dir,"unknown")==AE_EDITOR_WORKSPACE_UNKNOWN);sync_fault=0;
    CHECK(tab(w,id).uncertain && !info(w,id).dirty && !strcmp(tab(w,id).leaf,"unknown"));disk(dir,"unknown","");
    syncs=0;CHECK(ae_editor_workspace_save(w,id)==AE_EDITOR_WORKSPACE_UNKNOWN && syncs==0);
    struct ae_editor_close_token closing;CHECK(ae_editor_workspace_request_close(w,id,&closing)==REVIEW);
    CHECK(ae_editor_workspace_resolve_close(w,&closing,AE_EDITOR_CLOSE_DISCARD)==AE_EDITOR_WORKSPACE_UNKNOWN);
    CHECK(ae_editor_workspace_resolve_close(w,&closing,AE_EDITOR_CLOSE_SAVE)==AE_EDITOR_WORKSPACE_UNKNOWN && syncs==0);
    CHECK(ae_editor_workspace_resolve_close(w,&closing,AE_EDITOR_CLOSE_CANCEL)==OK);
    uint64_t other;CHECK(ae_editor_workspace_new(w,&other)==OK);typing(w,"retained elsewhere");
    CHECK(save_new(w,other,dir,"unknown")==AE_EDITOR_WORKSPACE_UNKNOWN);CHECK(!tab(w,other).leaf);
    CHECK(ae_editor_workspace_count(w)==2 && tab(w,id).uncertain);disk(dir,"unknown","");
    /* Test-process retirement releases local FDs only; never a successful
       application closing/recovery. The actual named result is retained. */
    ae_editor_workspace_destroy(w);disk(dir,"unknown","");
}
static void allocation_failure(int dir) {
    budget=0;CHECK(!ae_editor_workspace_create());budget=-1;
    struct ae_editor_workspace *w=ae_editor_workspace_create();uint64_t id=88;CHECK(w);
    budget=0;CHECK(ae_editor_workspace_new(w,&id)==AE_EDITOR_WORKSPACE_NOMEM);budget=-1;
    CHECK(id==88 && !ae_editor_workspace_count(w));
    put(dir,"oom","original");budget=0;CHECK(ae_editor_workspace_open(w,dir,"oom",&id)==AE_EDITOR_WORKSPACE_NOMEM);budget=-1;
    CHECK(id==88 && !ae_editor_workspace_count(w));disk(dir,"oom","original");
    CHECK(ae_editor_workspace_new(w,&id)==OK);typing(w,"draft");budget=0;
    CHECK(save_new(w,id,dir,"oom-save")==AE_EDITOR_WORKSPACE_NOMEM);budget=-1;
    CHECK(!tab(w,id).leaf && info(w,id).dirty && faccessat(dir,"oom-save",F_OK,0)!=0);
    ae_editor_workspace_destroy(w);
}
static void stale_destination_picker(int dir) {
    struct ae_editor_workspace *w=ae_editor_workspace_create();uint64_t id,other;struct ae_editor_destination_token old,current;
    CHECK(w && ae_editor_workspace_new(w,&id)==OK && ae_editor_workspace_destination(w,id,&old)==OK);typing(w,"changed");
    CHECK(ae_editor_workspace_save_new(w,&old,dir,"stale-destination")==AE_EDITOR_WORKSPACE_CONFLICT);
    CHECK(ae_editor_workspace_destination(w,id,&old)==OK && ae_editor_workspace_new(w,&other)==OK && ae_editor_workspace_select(w,id)==OK);
    CHECK(ae_editor_workspace_save_new(w,&old,dir,"stale-destination")==AE_EDITOR_WORKSPACE_CONFLICT);
    struct ae_editor_close_token closing,new_close;
    CHECK(ae_editor_workspace_request_close(w,id,&closing)==REVIEW && ae_editor_workspace_destination(w,id,&old)==OK);
    CHECK(ae_editor_workspace_resolve_close(w,&closing,AE_EDITOR_CLOSE_CANCEL)==OK && ae_editor_workspace_request_close(w,id,&new_close)==REVIEW);
    CHECK(ae_editor_workspace_save_new(w,&old,dir,"stale-destination")==AE_EDITOR_WORKSPACE_CONFLICT);
    CHECK(faccessat(dir,"stale-destination",F_OK,0)!=0 && errno==ENOENT);
    CHECK(ae_editor_workspace_destination(w,id,&current)==OK);old=current;old.review=closing.serial;
    CHECK(ae_editor_workspace_save_new(w,&old,dir,"stale-destination")==AE_EDITOR_WORKSPACE_CONFLICT);
    CHECK(ae_editor_workspace_save_new(w,&current,dir,"picker-save")==OK);
    CHECK(ae_editor_workspace_save_new(w,&current,dir,"replayed-destination")==AE_EDITOR_WORKSPACE_CONFLICT);
    CHECK(faccessat(dir,"replayed-destination",F_OK,0)!=0 && errno==ENOENT);disk(dir,"picker-save","changed");
    CHECK(ae_editor_workspace_resolve_close(w,&new_close,AE_EDITOR_CLOSE_SAVE)==OK && ae_editor_workspace_count(w)==1);
    ae_editor_workspace_destroy(w);
}
int main(void) {
    char first[]="workspace-a-XXXXXX",second[]="workspace-b-XXXXXX";CHECK(geteuid()!=0 && mkdtemp(first) && mkdtemp(second));
    int dir=open(first,O_RDONLY|O_DIRECTORY|O_CLOEXEC),other=open(second,O_RDONLY|O_DIRECTORY|O_CLOEXEC);CHECK(dir>=0 && other>=0);
    empty_and_capacity();independent_tabs_and_input(dir,other);save_and_create_only(dir);close_review_and_save_path(dir);
    discard_and_conflicting_save(dir);uncertain_even_when_clean(dir);allocation_failure(dir);stale_destination_picker(dir);
    CHECK(close(dir)==0 && close(other)==0);puts("graphical editor workspace: 8 native file/tab/closing workflows passed; fixture files retained");return 0;
}
