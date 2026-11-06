#define _GNU_SOURCE
#include "files/session.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"Files session %d: %s errno=%d\n",__LINE__,#x,errno);exit(1); } } while (0)

struct worker { struct ae_files_session *pending;unsigned starts,joins;bool deny; };
static bool lost_mkdir;
int __real_mkdirat(int,const char *,mode_t);
int __wrap_mkdirat(int dir,const char *name,mode_t mode) {
    int result=__real_mkdirat(dir,name,mode);
    if (!result && lost_mkdir) { errno=EIO;return -1; }
    return result;
}
static bool start(void *context,struct ae_files_session *session) {
    struct worker *w=context;++w->starts;
    CHECK(!w->pending);
    if (w->deny) return false;
    w->pending=session;return true;
}
static void join(void *context) {
    struct worker *w=context;CHECK(w->pending && atomic_load_explicit(&w->pending->create_done,memory_order_acquire));
    ++w->joins;w->pending=NULL;
}
static void complete(struct worker *w) {
    CHECK(w->pending);struct ae_files_session *s=w->pending;
    ae_files_session_apply_worker(s);CHECK(ae_files_session_creating(s));
    ae_files_session_create_update(s);CHECK(!w->pending && !ae_files_session_creating(s));
}
static int setup(struct ae_files_session *s,struct worker *w) {
    char path[]="files-session-XXXXXX";CHECK(mkdtemp(path) && !chmod(path,0700));
    int root=open(path,O_RDONLY|O_DIRECTORY|O_CLOEXEC);CHECK(root>=3);
    ae_files_session_init(s,start,join,w);enum ae_files_result result;
    s->files=ae_files_create(root,&result);CHECK(s->files && result==AE_FILES_OK);
    CHECK(s->child.pid==-1 && s->child.pin==-1 && s->create_dir==-1 && s->rename_witness==-1 && s->clip_dir==-1 && s->move_target==-1);
    CHECK(s->running && s->dirty && s->scale==1 && s->details && s->sidebar);
    CHECK(ae_files_layout_make(1000,680,1,false,true,true,false,&s->layout));
    return root;
}
static bool exists(int root,const char *name) { struct stat st;return fstatat(root,name,&st,AT_SYMLINK_NOFOLLOW)==0; }
static void select_name(struct ae_files_session *s,const char *name) {
    s->rows=ae_files_count(s->files);
    for (size_t i=0;i<s->rows;++i) if (!strcmp(ae_files_entry(s->files,i)->name,name)) { CHECK(ae_files_select(s->files,i));return; }
    CHECK(false);
}
static void dispose(struct ae_files_session *s,int root) {
    CHECK(!ae_files_session_creating(s));ae_files_session_dialog_close(s);ae_files_session_clipboard_clear(s);
    ae_files_create_close(s->creation);ae_files_create_close(s->unknown);
    ae_files_rename_close(s->rename_op);ae_files_rename_close(s->rename_unknown);
    ae_files_destroy(s->files);CHECK(!close(root));
}
static void creation_and_close(void) {
    struct ae_files_session s;struct worker w={.deny=true};int root=setup(&s,&w);
    ae_files_session_dialog_open(&s,AE_FILES_CREATE_FILE);ae_files_session_name_input(&s,"first");
    ae_files_session_create_submit(&s);CHECK(w.starts==1 && !w.joins && !s.creation && !s.work_active && !exists(root,"first") && !s.creations);
    w.deny=false;ae_files_session_create_submit(&s);CHECK(w.starts==2 && s.creations==1 && s.work_active && !exists(root,"first"));
    ae_files_session_create_submit(&s);CHECK(w.starts==2);
    ae_files_session_request_close(&s);CHECK(s.closing && s.running && s.dialog && s.create_dir>=0);
    complete(&w);CHECK(w.joins==1 && exists(root,"first") && !s.running && !s.dialog && s.create_dir==-1);
    ae_files_session_create_update(&s);CHECK(w.joins==1 && w.starts==2);
    struct stat st;CHECK(!fstatat(root,"first",&st,AT_SYMLINK_NOFOLLOW) && st.st_size==0 && (st.st_mode&0777)==0600);
    dispose(&s,root);
}
static void rename_and_move(void) {
    struct ae_files_session s;struct worker w={0};int root=setup(&s,&w);
    int fd=openat(root,"first",O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC,0600);CHECK(fd>=0 && write(fd,"original",8)==8 && !close(fd));
    CHECK(!mkdirat(root,"target",0700) && ae_files_refresh(s.files)==AE_FILES_OK);select_name(&s,"first");
    ae_files_session_move(&s,INT_MIN);CHECK(ae_files_selected(s.files)==0);
    ae_files_session_move(&s,INT_MAX);CHECK(ae_files_selected(s.files)==ae_files_count(s.files)-1);
    CHECK(!ae_files_session_menu_enabled(&s,UINT_MAX));select_name(&s,"first");
    ae_files_session_rename_open(&s);CHECK(s.dialog && s.renaming && s.rename_witness>=0);
    ae_files_session_name_input(&s,"-renamed");ae_files_session_create_submit(&s);CHECK(w.starts==1 && !exists(root,"first-renamed"));
    complete(&w);CHECK(w.joins==1 && !s.dialog && !exists(root,"first") && exists(root,"first-renamed"));
    select_name(&s,"first-renamed");ae_files_session_cut_selected(&s);CHECK(s.clip_dir>=0 && s.clip_witness>=0);
    select_name(&s,"target");ae_files_session_activate(&s);CHECK(ae_files_depth(s.files)==1);
    ae_files_session_paste_open(&s);CHECK(s.dialog && s.moving && s.move_target>=0);
    ae_files_session_create_submit(&s);CHECK(w.starts==2 && s.moves==1);complete(&w);
    CHECK(w.joins==2 && !s.dialog && s.clip_dir==-1 && s.clip_witness==-1 && !exists(root,"first-renamed"));
    fd=openat(root,"target/first-renamed",O_RDONLY|O_CLOEXEC);char bytes[8];CHECK(fd>=0 && read(fd,bytes,8)==8 && !memcmp(bytes,"original",8) && !close(fd));
    dispose(&s,root);
}
static void unknown_and_input(void) {
    struct ae_files_session s;struct worker w={0};int root=setup(&s,&w);
    ae_files_session_dialog_open(&s,AE_FILES_CREATE_DIRECTORY);ae_files_session_name_input(&s,"retained");
    ae_files_session_name_input(&s,"/invalid");CHECK(s.name_bad && !strcmp(s.name,"retained"));
    ae_files_session_create_submit(&s);CHECK(!w.starts);
    s.name_bad=false;ae_files_session_create_submit(&s);CHECK(w.starts==1);
    lost_mkdir=true;complete(&w);lost_mkdir=false;
    CHECK(exists(root,"retained") && s.unknown && ae_files_session_unresolved(&s));
    CHECK(!ae_files_session_menu_enabled(&s,0) && !ae_files_session_menu_enabled(&s,1));
    ae_files_session_create_submit(&s);CHECK(w.starts==1 && w.joins==1);
    ae_files_session_dialog_close(&s);ae_files_session_dialog_open(&s,AE_FILES_CREATE_FILE);
    CHECK(s.dialog && s.unknown && !strcmp(s.name,"retained"));
    ae_files_session_create_submit(&s);CHECK(w.starts==1);
    ae_files_session_request_close(&s);CHECK(!s.running && exists(root,"retained"));
    dispose(&s,root);
}
static void document_fence(void) {
    struct ae_files_session s;struct worker w={0};int root=setup(&s,&w);
    /* Controlled state only; no observation, signaling or retirement of PID 1. */
    s.child.pid=1;s.child.ended=false;ae_files_session_request_close(&s);CHECK(s.closing && s.running);
    CHECK(!ae_files_session_menu_enabled(&s,0));s.child.pid=-1;
    ae_files_session_request_close(&s);CHECK(!s.running);dispose(&s,root);
}
int main(void) {
    CHECK(getuid()!=0);creation_and_close();rename_and_move();unknown_and_input();document_fence();
    puts("Files session: deferred worker, failed start, close fence, rename/move, unknown outcome and rejected intent");return 0;
}
