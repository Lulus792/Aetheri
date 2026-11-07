#include "editor/workspace.h"
#include <errno.h>
#include <fcntl.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>
#include <unistd.h>
#define CHECK(x) do {if(!(x)) {fprintf(stderr,"save work %d: %s errno=%d\n",__LINE__,#x,errno);exit(1);}} while(0)
#define OK AE_EDITOR_WORKSPACE_OK
static int entered[2],release_pipe[2],budget=-1;
static atomic_int pause_sync,syncs,fault;
void *__real_malloc(size_t);
void *__real_calloc(size_t,size_t);
int __real_fsync(int);
void *__wrap_malloc(size_t n) {if(!budget) return NULL;if(budget>0) --budget;return __real_malloc(n);}
void *__wrap_calloc(size_t n,size_t s) {if(!budget) return NULL;if(budget>0) --budget;return __real_calloc(n,s);}
int __wrap_fsync(int fd) {
    if(atomic_exchange(&pause_sync,0)) {char byte=1;CHECK(write(entered[1],&byte,1)==1 && read(release_pipe[0],&byte,1)==1);}
    int result=__real_fsync(fd);
    if(atomic_fetch_add(&syncs,1)+1==atomic_load(&fault)) {errno=EIO;return -1;}
    return result;
}
static void text(const struct ae_document *d,const char *expected) {
    struct ae_document_info i;char bytes[256];CHECK(ae_document_info(d,&i) && i.size==strlen(expected));
    CHECK(ae_document_copy(d,0,i.size,bytes,sizeof(bytes)) && !memcmp(bytes,expected,i.size));
}
static bool dirty(const struct ae_document *d) {struct ae_document_info i;CHECK(ae_document_info(d,&i));return i.dirty;}
static struct ae_editor_tab tab(struct ae_editor_workspace *w,uint64_t id) {struct ae_editor_tab t;CHECK(ae_editor_workspace_get(w,id,&t));return t;}
static void type(struct ae_editor_workspace *w,const char *s) {CHECK(ae_direct_text(ae_editor_workspace_input(w),s,strlen(s))==AE_DOCUMENT_OK);}
static void disk(int dir,const char *leaf,const char *expected) {
    int fd=openat(dir,leaf,O_RDONLY|O_CLOEXEC);char bytes[256];CHECK(fd>=0);ssize_t size=read(fd,bytes,sizeof(bytes));
    CHECK(size>=0 && (size_t)size==strlen(expected) && !memcmp(bytes,expected,(size_t)size) && close(fd)==0);
}
static void put(int dir,const char *leaf,const char *s) {int fd=openat(dir,leaf,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC,0600);CHECK(fd>=0 && write(fd,s,strlen(s))==(ssize_t)strlen(s) && close(fd)==0);}
static int worker(void *p) {return ae_editor_save_work_apply(p)?0:1;}
static void run(struct ae_editor_save_work *work) {thrd_t t;int code;CHECK(thrd_create(&t,worker,work)==thrd_success && thrd_join(t,&code)==thrd_success && !code && ae_editor_save_work_done(work));}
static void pause_start(struct ae_editor_save_work *work,thrd_t *thread) {
    CHECK(pipe2(entered,O_CLOEXEC)==0 && pipe2(release_pipe,O_CLOEXEC)==0);atomic_store(&pause_sync,1);
    CHECK(thrd_create(thread,worker,work)==thrd_success);char byte;CHECK(read(entered[0],&byte,1)==1);
}
static void resume_join(thrd_t thread) {
    char byte=1;int code;CHECK(write(release_pipe[1],&byte,1)==1 && thrd_join(thread,&code)==thrd_success && !code);
    CHECK(!close(entered[0]) && !close(entered[1]) && !close(release_pipe[0]) && !close(release_pipe[1]));
}
static void snapshot_history(void) {
    enum ae_document_result r;struct ae_document *d=ae_document_create("",0,&r),*other=ae_document_create("",0,&r);CHECK(d && other);
    CHECK(ae_document_type(d,"a",1)==AE_DOCUMENT_OK);struct ae_document_snapshot *first=NULL;
    CHECK(ae_document_snapshot_create(d,&first)==AE_DOCUMENT_OK);text(ae_document_snapshot_body(first),"a");
    CHECK(ae_document_type(d,"b",1)==AE_DOCUMENT_OK && !ae_document_snapshot_acknowledge(other,first));
    CHECK(ae_document_snapshot_acknowledge(d,first) && dirty(d));
    /* Background completion must not split newer adjacent typing according to
       filesystem timing. The captured boundary already protects saved state. */
    CHECK(ae_document_type(d,"c",1)==AE_DOCUMENT_OK);bool changed;
    CHECK(ae_document_history(d,-1,&changed)==AE_DOCUMENT_OK && changed && !dirty(d));text(d,"a");
    CHECK(ae_document_history(d,1,&changed)==AE_DOCUMENT_OK && changed && dirty(d));text(d,"abc");
    struct ae_document_snapshot *second=NULL;CHECK(ae_document_snapshot_create(d,&second)==AE_DOCUMENT_OK && ae_document_snapshot_acknowledge(d,second) && !dirty(d));
    CHECK(ae_document_snapshot_acknowledge(d,first) && !dirty(d));
    ae_document_snapshot_destroy(first);ae_document_snapshot_destroy(second);ae_document_destroy(other);ae_document_destroy(d);
    d=ae_document_create("",0,&r);CHECK(d && ae_document_type(d,"original",8)==AE_DOCUMENT_OK && ae_document_snapshot_create(d,&first)==AE_DOCUMENT_OK);
    CHECK(ae_document_type(ae_document_snapshot_body(first),"changed",7)==AE_DOCUMENT_OK);
    CHECK(!ae_document_snapshot_acknowledge(d,first) && dirty(d));text(d,"original");
    ae_document_snapshot_destroy(first);ae_document_destroy(d);
    d=ae_document_create("",0,&r);CHECK(d && ae_document_type(d,"a",1)==AE_DOCUMENT_OK);
    struct ae_document_snapshot *sentinel=(void *)(uintptr_t)1;budget=0;CHECK(ae_document_snapshot_create(d,&sentinel)==AE_DOCUMENT_NOMEM && sentinel==(void *)(uintptr_t)1);budget=-1;
    CHECK(ae_document_type(d,"b",1)==AE_DOCUMENT_OK && ae_document_history(d,-1,&changed)==AE_DOCUMENT_OK && changed);text(d,"");ae_document_destroy(d);
}
static void named_save(int directory) {
    put(directory,"named","base");struct ae_editor_workspace *w=ae_editor_workspace_create(),*wrong=ae_editor_workspace_create();uint64_t id,other,earlier;
    CHECK(w && wrong && ae_editor_workspace_new(w,&earlier)==OK && ae_editor_workspace_open(w,directory,"named",&id)==OK);type(w,"first");
    struct ae_editor_save_work *work=NULL;CHECK(ae_editor_workspace_prepare_save(w,id,&work)==OK && ae_editor_save_work_document(work)==id);
    CHECK(tab(w,id).saving && !strcmp(tab(w,id).leaf,"named") && !ae_editor_save_work_done(work));
    CHECK(ae_editor_workspace_finish_save(w,work)==AE_EDITOR_WORKSPACE_BUSY && ae_editor_workspace_finish_save(wrong,work)==AE_EDITOR_WORKSPACE_INVALID);
    thrd_t thread;pause_start(work,&thread);CHECK(!ae_editor_workspace_abort_save(w,work));
    struct ae_editor_close_token close={0};CHECK(ae_editor_workspace_request_close(w,id,&close)==AE_EDITOR_WORKSPACE_BUSY);
    CHECK(ae_editor_workspace_save(w,id)==AE_EDITOR_WORKSPACE_BUSY);disk(directory,"named","base");
    /* Shift the original tab in the table while its adapter is on the worker. */
    type(w,"later");CHECK(ae_editor_workspace_request_close(w,earlier,&close)==OK);
    CHECK(ae_editor_workspace_new(w,&other)==OK);type(w,"other");
    CHECK(ae_editor_workspace_request_close(w,other,&close)==AE_EDITOR_WORKSPACE_REVIEW);
    CHECK(ae_editor_workspace_resolve_close(w,&close,AE_EDITOR_CLOSE_DISCARD)==OK);
    resume_join(thread);CHECK(!ae_editor_save_work_apply(work));
    CHECK(ae_editor_workspace_finish_save(w,work)==OK && !tab(w,id).saving);disk(directory,"named","firstbase");text(tab(w,id).document,"firstlaterbase");CHECK(dirty(tab(w,id).document));
    bool changed;CHECK(ae_direct_history(ae_editor_workspace_input(w),-1,&changed)==AE_DOCUMENT_OK && changed && !dirty(tab(w,id).document));
    text(tab(w,id).document,"firstbase");CHECK(ae_editor_workspace_request_close(w,id,&close)==OK);ae_editor_workspace_destroy(w);ae_editor_workspace_destroy(wrong);
}
static void first_save_and_failures(int directory) {
    struct ae_editor_workspace *w=ae_editor_workspace_create();uint64_t id,incumbent;CHECK(w && ae_editor_workspace_open(w,directory,"named",&incumbent)==OK);
    CHECK(ae_editor_workspace_new(w,&id)==OK);type(w,"new");struct ae_editor_destination_token token;CHECK(ae_editor_workspace_destination(w,id,&token)==OK);
    struct ae_editor_save_work *work=NULL;CHECK(ae_editor_workspace_prepare_save_new(w,&token,directory,"first",&work)==OK);
    struct ae_editor_close_token close={0};CHECK(ae_editor_workspace_request_close(w,incumbent,&close)==AE_EDITOR_WORKSPACE_BUSY);
    thrd_t thread;pause_start(work,&thread);type(w,"tail");resume_join(thread);
    CHECK(ae_editor_workspace_finish_save(w,work)==OK);disk(directory,"first","new");text(tab(w,id).document,"newtail");CHECK(dirty(tab(w,id).document) && !strcmp(tab(w,id).leaf,"first"));
    CHECK(ae_editor_workspace_prepare_save(w,id,&work)==OK && ae_editor_workspace_abort_save(w,work));CHECK(!tab(w,id).saving);disk(directory,"first","new");
    /* Stale destination and first-create conflict never attach another file. */
    uint64_t next;CHECK(ae_editor_workspace_new(w,&next)==OK);type(w,"fresh");CHECK(ae_editor_workspace_prepare_save_new(w,&token,directory,"late",&work)==AE_EDITOR_WORKSPACE_CONFLICT);
    CHECK(ae_editor_workspace_destination(w,next,&token)==OK && ae_editor_workspace_prepare_save_new(w,&token,directory,"named",&work)==OK);
    run(work);CHECK(ae_editor_workspace_finish_save(w,work)==AE_EDITOR_WORKSPACE_CONFLICT && !tab(w,next).leaf);disk(directory,"named","firstbase");
    for(int i=0;i<7;++i) {
        CHECK(ae_editor_workspace_destination(w,next,&token)==OK);budget=i;work=(void *)(uintptr_t)1;
        enum ae_editor_workspace_result r=ae_editor_workspace_prepare_save_new(w,&token,directory,"allocation",&work);budget=-1;
        if(r==OK) CHECK(ae_editor_workspace_abort_save(w,work));else CHECK(r==AE_EDITOR_WORKSPACE_NOMEM && work==(void *)(uintptr_t)1);
        CHECK(!tab(w,next).saving);text(tab(w,next).document,"fresh");
    }
    ae_editor_workspace_destroy(w);
}
static void review_cancel_and_unknown(int directory) {
    put(directory,"review","base");struct ae_editor_workspace *w=ae_editor_workspace_create();uint64_t id;CHECK(w && ae_editor_workspace_open(w,directory,"review",&id)==OK);type(w,"old");
    struct ae_editor_close_token close;CHECK(ae_editor_workspace_request_close(w,id,&close)==AE_EDITOR_WORKSPACE_REVIEW);
    struct ae_editor_save_work *work=NULL;CHECK(ae_editor_workspace_prepare_save(w,id,&work)==OK);
    thrd_t thread;pause_start(work,&thread);CHECK(ae_editor_workspace_resolve_close(w,&close,AE_EDITOR_CLOSE_CANCEL)==OK);type(w,"new");resume_join(thread);
    CHECK(ae_editor_workspace_finish_save(w,work)==OK && ae_editor_workspace_count(w)==1 && dirty(tab(w,id).document));disk(directory,"review","oldbase");
    CHECK(ae_editor_workspace_resolve_close(w,&close,AE_EDITOR_CLOSE_SAVE)==AE_EDITOR_WORKSPACE_INVALID);
    CHECK(ae_editor_workspace_prepare_save(w,id,&work)==OK);atomic_store(&syncs,0);atomic_store(&fault,3);run(work);atomic_store(&fault,0);
    CHECK(ae_editor_workspace_finish_save(w,work)==AE_EDITOR_WORKSPACE_UNKNOWN && tab(w,id).uncertain && !tab(w,id).saving);
    CHECK(ae_editor_workspace_prepare_save(w,id,&work)==AE_EDITOR_WORKSPACE_UNKNOWN);
    CHECK(ae_editor_workspace_request_close(w,id,&close)==AE_EDITOR_WORKSPACE_REVIEW && ae_editor_workspace_resolve_close(w,&close,AE_EDITOR_CLOSE_DISCARD)==AE_EDITOR_WORKSPACE_UNKNOWN);
    CHECK(ae_editor_workspace_resolve_close(w,&close,AE_EDITOR_CLOSE_CANCEL)==OK);ae_editor_workspace_destroy(w);
    w=ae_editor_workspace_create();CHECK(w && ae_editor_workspace_open(w,directory,"review",&id)==OK && !dirty(tab(w,id).document));
    CHECK(ae_editor_workspace_prepare_save(w,id,&work)==OK);atomic_store(&syncs,0);atomic_store(&fault,3);run(work);atomic_store(&fault,0);
    CHECK(ae_editor_workspace_finish_save(w,work)==AE_EDITOR_WORKSPACE_UNKNOWN && !dirty(tab(w,id).document) && tab(w,id).uncertain);
    CHECK(ae_editor_workspace_request_close(w,id,&close)==AE_EDITOR_WORKSPACE_REVIEW && ae_editor_workspace_resolve_close(w,&close,AE_EDITOR_CLOSE_DISCARD)==AE_EDITOR_WORKSPACE_UNKNOWN);
    CHECK(ae_editor_workspace_resolve_close(w,&close,AE_EDITOR_CLOSE_CANCEL)==OK);ae_editor_workspace_destroy(w);
}
int main(void) {
    char path[]="save-work-XXXXXX";CHECK(mkdtemp(path));int directory=open(path,O_RDONLY|O_DIRECTORY|O_CLOEXEC);CHECK(directory>=0);
    snapshot_history();named_save(directory);first_save_and_failures(directory);review_cancel_and_unknown(directory);CHECK(!close(directory));puts("editor save work: 4 groups passed");return 0;
}
