#include "workspace.h"
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include <fcntl.h>
#include <unistd.h>
struct tab {
    struct ae_document *document;struct ae_direct *input;struct ae_editor_file *file;
    uint64_t id;char leaf[256];bool saving;
};
struct ae_editor_workspace {
    struct tab tabs[AE_EDITOR_TABS];unsigned count;uint64_t active,serial;
    struct ae_editor_close_token review;
    struct ae_editor_save_work *work;
};
struct ae_editor_save_work {
    uint64_t document;bool first;int directory;char leaf[256];
    struct ae_editor_file *file,*incumbents[AE_EDITOR_TABS];unsigned count;
    struct ae_document_snapshot *snapshot;
    atomic_bool started,done;enum ae_editor_workspace_result result;
};
static enum ae_editor_workspace_result mapped(enum ae_editor_file_result result) {
    switch (result) {
    case AE_EDITOR_FILE_OK:return AE_EDITOR_WORKSPACE_OK;
    case AE_EDITOR_FILE_CONFLICT:return AE_EDITOR_WORKSPACE_CONFLICT;
    case AE_EDITOR_FILE_UNSUPPORTED:return AE_EDITOR_WORKSPACE_UNSUPPORTED;
    case AE_EDITOR_FILE_IO:return AE_EDITOR_WORKSPACE_IO;
    case AE_EDITOR_FILE_NOMEM:return AE_EDITOR_WORKSPACE_NOMEM;
    case AE_EDITOR_FILE_UNKNOWN:return AE_EDITOR_WORKSPACE_UNKNOWN;
    default:return AE_EDITOR_WORKSPACE_INVALID;
    }
}
static struct tab *find(const struct ae_editor_workspace *w,uint64_t id) {
    if (w && id) for (unsigned i=0;i<w->count;++i) if (w->tabs[i].id==id) return (struct tab *)&w->tabs[i];
    return NULL;
}
static bool cancellable(const struct tab *tab) {
    struct ae_direct_token token;return !tab || (ae_direct_token(tab->input,&token) && token.interaction<UINT64_MAX);
}
static bool cancel_pair(struct ae_editor_workspace *w,struct tab *target) {
    struct tab *active=find(w,w->active);
    if (!cancellable(active) || !cancellable(target)) return false;
    if (active) (void)ae_direct_cancel_input(active->input);
    if (target && target!=active) (void)ae_direct_cancel_input(target->input);
    return true;
}
static void free_tab(struct tab *tab) {
    ae_direct_destroy(tab->input);ae_editor_file_close(tab->file);ae_document_destroy(tab->document);*tab=(struct tab){0};
}
struct ae_editor_workspace *ae_editor_workspace_create(void) { return calloc(1,sizeof(struct ae_editor_workspace)); }
void ae_editor_workspace_destroy(struct ae_editor_workspace *w) {
    if (w && !w->work) { for (unsigned i=0;i<w->count;++i) free_tab(&w->tabs[i]);free(w); }
}
unsigned ae_editor_workspace_count(const struct ae_editor_workspace *w) { return w ? w->count : 0; }
uint64_t ae_editor_workspace_active(const struct ae_editor_workspace *w) { return w ? w->active : 0; }
bool ae_editor_workspace_get(const struct ae_editor_workspace *w,uint64_t id,struct ae_editor_tab *out) {
    struct tab *tab=find(w,id);if (!tab || !out) return false;
    *out=(struct ae_editor_tab){tab->id,tab->leaf[0] ? tab->leaf : NULL,ae_editor_file_recovery(tab->file),tab->document,
        w->active==id,ae_editor_file_uncertain(tab->file),tab->saving};return true;
}
bool ae_editor_workspace_at(const struct ae_editor_workspace *w,unsigned index,struct ae_editor_tab *out) {
    return w && index<w->count && ae_editor_workspace_get(w,w->tabs[index].id,out);
}
bool ae_editor_workspace_selection(const struct ae_editor_workspace *w,uint64_t id,struct ae_direct_selection *out) {
    struct tab *tab=find(w,id);return tab && ae_direct_selection(tab->input,out);
}
struct ae_direct *ae_editor_workspace_input(struct ae_editor_workspace *w) {
    struct tab *tab=w && !w->review.id ? find(w,w->active) : NULL;return tab ? tab->input : NULL;
}
static enum ae_editor_workspace_result add(struct ae_editor_workspace *w,struct ae_document *document,
    struct ae_editor_file *file,const char *leaf,uint64_t *id) {
    struct ae_direct *input=ae_direct_create(document);if (!input) return AE_EDITOR_WORKSPACE_NOMEM;
    if (!cancel_pair(w,NULL)) { ae_direct_destroy(input);return AE_EDITOR_WORKSPACE_LIMIT; }
    struct ae_direct_token token;(void)ae_direct_token(input,&token);
    struct tab *tab=&w->tabs[w->count++];*tab=(struct tab){.document=document,.input=input,.file=file,.id=token.identity};
    if (leaf) memcpy(tab->leaf,leaf,strlen(leaf)+1);
    w->active=tab->id;*id=tab->id;return AE_EDITOR_WORKSPACE_OK;
}
enum ae_editor_workspace_result ae_editor_workspace_new(struct ae_editor_workspace *w,uint64_t *id) {
    if (!w || !id) return AE_EDITOR_WORKSPACE_INVALID;
    if (w->review.id) return AE_EDITOR_WORKSPACE_BUSY;
    if (w->count==AE_EDITOR_TABS) return AE_EDITOR_WORKSPACE_LIMIT;
    enum ae_document_result result;struct ae_document *document=ae_document_create(NULL,0,&result);
    if (!document) return AE_EDITOR_WORKSPACE_NOMEM;
    enum ae_editor_workspace_result r=add(w,document,NULL,NULL,id);if (r!=AE_EDITOR_WORKSPACE_OK) ae_document_destroy(document);return r;
}
enum ae_editor_workspace_result ae_editor_workspace_select(struct ae_editor_workspace *w,uint64_t id) {
    struct tab *tab=find(w,id);if (!tab) return AE_EDITOR_WORKSPACE_INVALID;
    if (w->review.id) return w->active==id ? AE_EDITOR_WORKSPACE_OK : AE_EDITOR_WORKSPACE_BUSY;
    if (!cancel_pair(w,tab)) return AE_EDITOR_WORKSPACE_LIMIT;
    w->active=id;return AE_EDITOR_WORKSPACE_OK;
}
static enum ae_editor_workspace_result open_document(struct ae_editor_workspace *w,int directory,const char *leaf,int witness,uint64_t *id) {
    if (!w || !id) return AE_EDITOR_WORKSPACE_INVALID;
    if (w->work) return AE_EDITOR_WORKSPACE_BUSY;
    if (w->review.id) return AE_EDITOR_WORKSPACE_BUSY;
    for (unsigned i=0;i<w->count;++i) if (w->tabs[i].file) {
        bool match=false;enum ae_editor_file_result result=witness>=0 ? ae_editor_file_matches_selected(w->tabs[i].file,directory,leaf,witness,&match) :
            ae_editor_file_matches(w->tabs[i].file,directory,leaf,&match);
        if (result!=AE_EDITOR_FILE_OK) return mapped(result);
        if (match) { enum ae_editor_workspace_result r=ae_editor_workspace_select(w,w->tabs[i].id);if (r==AE_EDITOR_WORKSPACE_OK) *id=w->tabs[i].id;return r; }
    }
    if (w->count==AE_EDITOR_TABS) return AE_EDITOR_WORKSPACE_LIMIT;
    struct ae_editor_file *file=NULL;struct ae_document *document=NULL;
    enum ae_editor_file_result result=witness>=0 ? ae_editor_file_open_selected(directory,leaf,witness,&file,&document) :
        ae_editor_file_open(directory,leaf,&file,&document);if (result!=AE_EDITOR_FILE_OK) return mapped(result);
    enum ae_editor_workspace_result r=add(w,document,file,leaf,id);
    if (r!=AE_EDITOR_WORKSPACE_OK) { ae_editor_file_close(file);ae_document_destroy(document); }return r;
}
enum ae_editor_workspace_result ae_editor_workspace_open(struct ae_editor_workspace *w,int directory,const char *leaf,uint64_t *id) {
    return open_document(w,directory,leaf,-1,id);
}
enum ae_editor_workspace_result ae_editor_workspace_open_selected(struct ae_editor_workspace *w,int directory,const char *leaf,int witness,uint64_t *id) {
    return witness<0 ? AE_EDITOR_WORKSPACE_INVALID : open_document(w,directory,leaf,witness,id);
}
static enum ae_editor_workspace_result save_admission(struct ae_editor_workspace *w,struct tab *tab) {
    if (!tab) return AE_EDITOR_WORKSPACE_INVALID;
    if (w->work) return AE_EDITOR_WORKSPACE_BUSY;
    if (w->review.id && w->review.id!=tab->id) return AE_EDITOR_WORKSPACE_BUSY;
    if (w->review.id) {
        struct ae_document_info info;
        if (!ae_document_info(tab->document,&info) || info.revision!=w->review.revision) return AE_EDITOR_WORKSPACE_CONFLICT;
    }
    if (ae_editor_file_uncertain(tab->file)) return AE_EDITOR_WORKSPACE_UNKNOWN;
    return AE_EDITOR_WORKSPACE_OK;
}
enum ae_editor_workspace_result ae_editor_workspace_save(struct ae_editor_workspace *w,uint64_t id) {
    struct tab *tab=find(w,id);enum ae_editor_workspace_result r=save_admission(w,tab);if (r!=AE_EDITOR_WORKSPACE_OK) return r;
    if (!tab->file) return AE_EDITOR_WORKSPACE_NEEDS_PATH;
    if (!ae_direct_cancel_input(tab->input)) return AE_EDITOR_WORKSPACE_LIMIT;
    return mapped(ae_editor_file_save(tab->file,tab->document));
}
enum ae_editor_workspace_result ae_editor_workspace_destination(struct ae_editor_workspace *w,uint64_t id,struct ae_editor_destination_token *out) {
    struct tab *tab=find(w,id);enum ae_editor_workspace_result r=save_admission(w,tab);if (r!=AE_EDITOR_WORKSPACE_OK) return r;
    if (!out || tab->file) return AE_EDITOR_WORKSPACE_INVALID;
    if (w->active!=id) return AE_EDITOR_WORKSPACE_BUSY;
    struct ae_direct_token token;if (!ae_direct_token(tab->input,&token)) return AE_EDITOR_WORKSPACE_INVALID;
    *out=(struct ae_editor_destination_token){id,token.revision,token.interaction,w->review.serial};return AE_EDITOR_WORKSPACE_OK;
}
enum ae_editor_workspace_result ae_editor_workspace_save_new(struct ae_editor_workspace *w,const struct ae_editor_destination_token *token,int directory,const char *leaf) {
    if (!token) return AE_EDITOR_WORKSPACE_INVALID;
    struct tab *tab=find(w,token->id);enum ae_editor_workspace_result r=save_admission(w,tab);if (r!=AE_EDITOR_WORKSPACE_OK) return r;
    if (tab->file) return AE_EDITOR_WORKSPACE_CONFLICT;
    struct ae_editor_destination_token current;r=ae_editor_workspace_destination(w,token->id,&current);if (r!=AE_EDITOR_WORKSPACE_OK) return r;
    if (token->revision!=current.revision || token->interaction!=current.interaction || token->review!=current.review) return AE_EDITOR_WORKSPACE_CONFLICT;
    if (!cancellable(tab)) return AE_EDITOR_WORKSPACE_LIMIT;
    for (unsigned i=0;i<w->count;++i) if (w->tabs[i].file) {
        bool match=false;enum ae_editor_file_result selected=ae_editor_file_matches(w->tabs[i].file,directory,leaf,&match);
        if (selected!=AE_EDITOR_FILE_OK) return mapped(selected);
        if (match) return AE_EDITOR_WORKSPACE_CONFLICT;
    }
    struct ae_editor_file *file=NULL;struct ae_document *empty=NULL;
    enum ae_editor_file_result result=ae_editor_file_new(directory,leaf,&file,&empty);if (result!=AE_EDITOR_FILE_OK) return mapped(result);
    ae_document_destroy(empty);(void)ae_direct_cancel_input(tab->input);
    result=ae_editor_file_save(file,tab->document);
    if (result==AE_EDITOR_FILE_OK || result==AE_EDITOR_FILE_UNKNOWN) { tab->file=file;memcpy(tab->leaf,leaf,strlen(leaf)+1); }
    else ae_editor_file_close(file);
    return mapped(result);
}
static void retire(struct ae_editor_workspace *w,struct tab *tab) {
    unsigned index=(unsigned)(tab-w->tabs);uint64_t id=tab->id;free_tab(tab);--w->count;
    memmove(w->tabs+index,w->tabs+index+1,(w->count-index)*sizeof(*tab));w->tabs[w->count]=(struct tab){0};
    if (w->active==id) w->active=w->count ? w->tabs[index<w->count ? index : w->count-1].id : 0;
    w->review=(struct ae_editor_close_token){0};
}
enum ae_editor_workspace_result ae_editor_workspace_request_close(struct ae_editor_workspace *w,uint64_t id,struct ae_editor_close_token *out) {
    struct tab *tab=find(w,id);if (!tab || !out) return AE_EDITOR_WORKSPACE_INVALID;
    if (tab->saving || (w->work && w->work->first && tab->file)) return AE_EDITOR_WORKSPACE_BUSY;
    if (w->review.id) { if (w->review.id!=id) return AE_EDITOR_WORKSPACE_BUSY;*out=w->review;return AE_EDITOR_WORKSPACE_REVIEW; }
    struct ae_document_info info;if (!ae_document_info(tab->document,&info)) return AE_EDITOR_WORKSPACE_INVALID;
    bool review=info.dirty || ae_editor_file_uncertain(tab->file);
    if ((review && w->serial==UINT64_MAX) || !cancel_pair(w,tab)) return AE_EDITOR_WORKSPACE_LIMIT;
    if (!review) { retire(w,tab);*out=(struct ae_editor_close_token){0};return AE_EDITOR_WORKSPACE_OK; }
    w->active=id;w->review=(struct ae_editor_close_token){id,info.revision,++w->serial};*out=w->review;return AE_EDITOR_WORKSPACE_REVIEW;
}
bool ae_editor_workspace_review(const struct ae_editor_workspace *w,struct ae_editor_close_token *out) {
    if (!w || !w->review.id || !out) return false;
    *out=w->review;return true;
}
enum ae_editor_workspace_result ae_editor_workspace_resolve_close(struct ae_editor_workspace *w,
    const struct ae_editor_close_token *token,enum ae_editor_close_choice choice) {
    if (!w || !token || !w->review.id || token->id!=w->review.id || token->revision!=w->review.revision || token->serial!=w->review.serial)
        return AE_EDITOR_WORKSPACE_INVALID;
    struct tab *tab=find(w,token->id);struct ae_document_info info;
    if (!tab || !ae_document_info(tab->document,&info) || info.revision!=token->revision) return AE_EDITOR_WORKSPACE_CONFLICT;
    if (choice==AE_EDITOR_CLOSE_CANCEL) { w->review=(struct ae_editor_close_token){0};return AE_EDITOR_WORKSPACE_OK; }
    if (choice!=AE_EDITOR_CLOSE_DISCARD && choice!=AE_EDITOR_CLOSE_SAVE) return AE_EDITOR_WORKSPACE_INVALID;
    if (tab->saving || (w->work && w->work->first && tab->file)) return AE_EDITOR_WORKSPACE_BUSY;
    if (ae_editor_file_uncertain(tab->file)) return AE_EDITOR_WORKSPACE_UNKNOWN;
    if (choice==AE_EDITOR_CLOSE_SAVE && info.dirty) {
        enum ae_editor_workspace_result r=ae_editor_workspace_save(w,tab->id);if (r!=AE_EDITOR_WORKSPACE_OK) return r;
    }
    retire(w,tab);return AE_EDITOR_WORKSPACE_OK;
}
static void free_work(struct ae_editor_save_work *work) {
    if(work->directory>=0) (void)close(work->directory);
    ae_editor_file_close(work->file);ae_document_snapshot_destroy(work->snapshot);free(work);
}
static enum ae_editor_workspace_result prepare_work(struct ae_editor_workspace *w,struct tab *tab,
    bool first,int directory,const char *leaf,struct ae_editor_save_work **out) {
    if(!out || !cancellable(tab)) return AE_EDITOR_WORKSPACE_INVALID;
    struct ae_editor_save_work *work=calloc(1,sizeof(*work));if(!work) return AE_EDITOR_WORKSPACE_NOMEM;
    work->directory=-1;work->document=tab->id;work->first=first;
    atomic_init(&work->started,false);atomic_init(&work->done,false);
    if(first) {
        if(!leaf || !*leaf || strnlen(leaf,256)>255 || !strcmp(leaf,".") || !strcmp(leaf,"..") || strchr(leaf,'/')) {free_work(work);return AE_EDITOR_WORKSPACE_INVALID;}
        work->directory=fcntl(directory,F_DUPFD_CLOEXEC,4);
        if(work->directory<0) {free_work(work);return AE_EDITOR_WORKSPACE_IO;}
        memcpy(work->leaf,leaf,strlen(leaf)+1);
        for(unsigned i=0;i<w->count;++i) if(w->tabs[i].file) work->incumbents[work->count++]=w->tabs[i].file;
    }
    enum ae_document_result result=ae_document_snapshot_create(tab->document,&work->snapshot);
    if(result!=AE_DOCUMENT_OK) {free_work(work);return result==AE_DOCUMENT_NOMEM?AE_EDITOR_WORKSPACE_NOMEM:AE_EDITOR_WORKSPACE_INVALID;}
    (void)ae_direct_cancel_input(tab->input);
    if(!first) {work->file=tab->file;tab->file=NULL;}
    tab->saving=true;w->work=work;*out=work;return AE_EDITOR_WORKSPACE_OK;
}
enum ae_editor_workspace_result ae_editor_workspace_prepare_save(struct ae_editor_workspace *w,uint64_t id,struct ae_editor_save_work **out) {
    struct tab *tab=find(w,id);enum ae_editor_workspace_result result=save_admission(w,tab);
    if(result!=AE_EDITOR_WORKSPACE_OK) return result;
    if(!tab->file) return AE_EDITOR_WORKSPACE_NEEDS_PATH;
    return prepare_work(w,tab,false,-1,NULL,out);
}
enum ae_editor_workspace_result ae_editor_workspace_prepare_save_new(struct ae_editor_workspace *w,
    const struct ae_editor_destination_token *token,int directory,const char *leaf,struct ae_editor_save_work **out) {
    if(!token) return AE_EDITOR_WORKSPACE_INVALID;
    struct tab *tab=find(w,token->id);enum ae_editor_workspace_result result=save_admission(w,tab);
    if(result!=AE_EDITOR_WORKSPACE_OK) return result;
    if(tab->file) return AE_EDITOR_WORKSPACE_CONFLICT;
    struct ae_editor_destination_token current;result=ae_editor_workspace_destination(w,token->id,&current);
    if(result!=AE_EDITOR_WORKSPACE_OK) return result;
    if(token->revision!=current.revision || token->interaction!=current.interaction || token->review!=current.review) return AE_EDITOR_WORKSPACE_CONFLICT;
    return prepare_work(w,tab,true,directory,leaf,out);
}
bool ae_editor_save_work_apply(struct ae_editor_save_work *work) {
    if(!work) return false;
    bool expected=false;if(!atomic_compare_exchange_strong(&work->started,&expected,true)) return false;
    work->result=AE_EDITOR_WORKSPACE_OK;
    if(work->first) {
        for(unsigned i=0;i<work->count;++i) {
            bool match=false;enum ae_editor_file_result r=ae_editor_file_matches(work->incumbents[i],work->directory,work->leaf,&match);
            if(r!=AE_EDITOR_FILE_OK || match) {work->result=match?AE_EDITOR_WORKSPACE_CONFLICT:mapped(r);goto done;}
        }
        struct ae_document *empty=NULL;
        enum ae_editor_file_result r=ae_editor_file_new(work->directory,work->leaf,&work->file,&empty);
        if(r!=AE_EDITOR_FILE_OK) {work->result=mapped(r);goto done;}
        ae_document_destroy(empty);
    }
    work->result=mapped(ae_editor_file_save(work->file,ae_document_snapshot_body(work->snapshot)));
done:
    atomic_store_explicit(&work->done,true,memory_order_release);return true;
}
bool ae_editor_save_work_done(const struct ae_editor_save_work *work) {return work && atomic_load_explicit(&work->done,memory_order_acquire);}
uint64_t ae_editor_save_work_document(const struct ae_editor_save_work *work) {return work?work->document:0;}
bool ae_editor_workspace_abort_save(struct ae_editor_workspace *w,struct ae_editor_save_work *work) {
    if(!w || !work || w->work!=work) return false;
    struct tab *tab=find(w,work->document);if(!tab || !tab->saving) return false;
    bool expected=false;if(!atomic_compare_exchange_strong(&work->started,&expected,true)) return false;
    if(!work->first) {tab->file=work->file;work->file=NULL;}
    tab->saving=false;w->work=NULL;free_work(work);return true;
}
enum ae_editor_workspace_result ae_editor_workspace_finish_save(struct ae_editor_workspace *w,struct ae_editor_save_work *work) {
    if(!w || !work || w->work!=work) return AE_EDITOR_WORKSPACE_INVALID;
    if(!ae_editor_save_work_done(work)) return AE_EDITOR_WORKSPACE_BUSY;
    struct tab *tab=find(w,work->document);if(!tab || !tab->saving || tab->file) return AE_EDITOR_WORKSPACE_INVALID;
    enum ae_editor_workspace_result result=work->result;
    if(result==AE_EDITOR_WORKSPACE_OK && !ae_document_snapshot_acknowledge(tab->document,work->snapshot)) return AE_EDITOR_WORKSPACE_INVALID;
    if(!work->first || result==AE_EDITOR_WORKSPACE_OK || result==AE_EDITOR_WORKSPACE_UNKNOWN) {
        tab->file=work->file;work->file=NULL;
        if(work->first) memcpy(tab->leaf,work->leaf,sizeof(tab->leaf));
    }
    tab->saving=false;w->work=NULL;free_work(work);return result;
}
