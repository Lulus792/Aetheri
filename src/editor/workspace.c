#include "workspace.h"
#include <stdlib.h>
#include <string.h>
struct tab {
    struct ae_document *document;struct ae_direct *input;struct ae_editor_file *file;
    uint64_t id;char leaf[256];
};
struct ae_editor_workspace {
    struct tab tabs[AE_EDITOR_TABS];unsigned count;uint64_t active,serial;
    struct ae_editor_close_token review;
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
    if (w) { for (unsigned i=0;i<w->count;++i) free_tab(&w->tabs[i]);free(w); }
}
unsigned ae_editor_workspace_count(const struct ae_editor_workspace *w) { return w ? w->count : 0; }
uint64_t ae_editor_workspace_active(const struct ae_editor_workspace *w) { return w ? w->active : 0; }
bool ae_editor_workspace_get(const struct ae_editor_workspace *w,uint64_t id,struct ae_editor_tab *out) {
    struct tab *tab=find(w,id);if (!tab || !out) return false;
    *out=(struct ae_editor_tab){tab->id,tab->file ? tab->leaf : NULL,ae_editor_file_recovery(tab->file),tab->document,
        w->active==id,ae_editor_file_uncertain(tab->file)};return true;
}
bool ae_editor_workspace_at(const struct ae_editor_workspace *w,unsigned index,struct ae_editor_tab *out) {
    return w && index<w->count && ae_editor_workspace_get(w,w->tabs[index].id,out);
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
enum ae_editor_workspace_result ae_editor_workspace_open(struct ae_editor_workspace *w,int directory,const char *leaf,uint64_t *id) {
    if (!w || !id) return AE_EDITOR_WORKSPACE_INVALID;
    if (w->review.id) return AE_EDITOR_WORKSPACE_BUSY;
    for (unsigned i=0;i<w->count;++i) if (w->tabs[i].file) {
        bool match=false;enum ae_editor_file_result result=ae_editor_file_matches(w->tabs[i].file,directory,leaf,&match);
        if (result!=AE_EDITOR_FILE_OK) return mapped(result);
        if (match) { enum ae_editor_workspace_result r=ae_editor_workspace_select(w,w->tabs[i].id);if (r==AE_EDITOR_WORKSPACE_OK) *id=w->tabs[i].id;return r; }
    }
    if (w->count==AE_EDITOR_TABS) return AE_EDITOR_WORKSPACE_LIMIT;
    struct ae_editor_file *file=NULL;struct ae_document *document=NULL;
    enum ae_editor_file_result result=ae_editor_file_open(directory,leaf,&file,&document);if (result!=AE_EDITOR_FILE_OK) return mapped(result);
    enum ae_editor_workspace_result r=add(w,document,file,leaf,id);
    if (r!=AE_EDITOR_WORKSPACE_OK) { ae_editor_file_close(file);ae_document_destroy(document); }return r;
}
static enum ae_editor_workspace_result save_admission(struct ae_editor_workspace *w,struct tab *tab) {
    if (!tab) return AE_EDITOR_WORKSPACE_INVALID;
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
    if (ae_editor_file_uncertain(tab->file)) return AE_EDITOR_WORKSPACE_UNKNOWN;
    if (choice==AE_EDITOR_CLOSE_SAVE && info.dirty) {
        enum ae_editor_workspace_result r=ae_editor_workspace_save(w,tab->id);if (r!=AE_EDITOR_WORKSPACE_OK) return r;
    }
    retire(w,tab);return AE_EDITOR_WORKSPACE_OK;
}
