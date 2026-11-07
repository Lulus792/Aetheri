#include "direct.h"
#include "text/grapheme.h"
#include <stdatomic.h>
#include <stdlib.h>

struct ae_direct {
    struct ae_document *document;
    uint64_t identity,interaction;
    size_t anchor,goal;
    bool selecting,vertical;
};
static _Atomic uint64_t next_identity=1;

static bool available(const struct ae_direct *edit) { return edit && edit->interaction<UINT64_MAX; }
static bool state(const struct ae_direct *edit,struct ae_document_info *info,size_t *row,struct ae_document_line *line) {
    return edit && ae_document_info(edit->document,info) && ae_document_row(edit->document,info->cursor,row) &&
        ae_document_line(edit->document,*row,line);
}
static bool points(const struct ae_direct *edit,const struct ae_document_line *line,size_t *offsets,size_t *count) {
    char text[AE_DOCUMENT_LINE_BYTES];
    return ae_document_copy(edit->document,line->start,line->size,text,sizeof(text)) &&
        ae_text_graphemes(text,line->size,offsets,AE_DOCUMENT_LINE_BYTES+1,count);
}
static void accepted(struct ae_direct *edit) { ++edit->interaction; }
static void collapsed(struct ae_direct *edit) {
    struct ae_document_info info;
    (void)ae_document_info(edit->document,&info);
    edit->anchor=info.cursor;edit->selecting=false;edit->vertical=false;accepted(edit);
}
struct ae_direct *ae_direct_create(struct ae_document *document) {
    struct ae_document_info info;if (!ae_document_info(document,&info)) return NULL;
    struct ae_direct *edit=calloc(1,sizeof(*edit));if (!edit) return NULL;
    uint64_t identity=atomic_load(&next_identity);
    do { if (identity==UINT64_MAX) { free(edit);return NULL; } }
    while (!atomic_compare_exchange_weak(&next_identity,&identity,identity+1));
    edit->document=document;edit->identity=identity;edit->interaction=1;edit->anchor=info.cursor;return edit;
}
void ae_direct_destroy(struct ae_direct *edit) { free(edit); }
const struct ae_document *ae_direct_document(const struct ae_direct *edit) { return edit ? edit->document : NULL; }
bool ae_direct_selection(const struct ae_direct *edit,struct ae_direct_selection *selection) {
    struct ae_document_info info;if (!edit || !selection || !ae_document_info(edit->document,&info)) return false;
    *selection=(struct ae_direct_selection){info.cursor,info.cursor,false};
    if (edit->selecting) {
        selection->begin=edit->anchor<info.cursor ? edit->anchor : info.cursor;
        selection->end=edit->anchor>info.cursor ? edit->anchor : info.cursor;
        selection->active=selection->begin!=selection->end;
    }
    return true;
}
bool ae_direct_token(const struct ae_direct *edit,struct ae_direct_token *token) {
    struct ae_document_info info;if (!edit || !token || !ae_document_info(edit->document,&info)) return false;
    *token=(struct ae_direct_token){edit->identity,edit->interaction,info.revision,info.cursor,edit->anchor,edit->selecting};return true;
}
static bool place(struct ae_direct *edit,size_t position,bool extend) {
    struct ae_document_info info;
    if (!available(edit) || !ae_document_info(edit->document,&info) || !ae_document_cursor(edit->document,position)) return false;
    if (extend) { if (!edit->selecting) edit->anchor=info.cursor;edit->selecting=edit->anchor!=position; }
    else { edit->anchor=position;edit->selecting=false; }
    accepted(edit);return true;
}
bool ae_direct_seek(struct ae_direct *edit,size_t position,bool extend) {
    if (!place(edit,position,extend)) return false;
    edit->vertical=false;return true;
}
bool ae_direct_select_all(struct ae_direct *edit) {
    struct ae_document_info info;
    if (!available(edit) || !ae_document_info(edit->document,&info) || !ae_document_cursor(edit->document,info.size)) return false;
    edit->anchor=0;edit->selecting=info.size!=0;edit->vertical=false;accepted(edit);return true;
}
bool ae_direct_clear_selection(struct ae_direct *edit) {
    if (!available(edit)) return false;
    ae_document_history_break(edit->document);collapsed(edit);return true;
}
bool ae_direct_cancel_input(struct ae_direct *edit) {
    if (!available(edit)) return false;
    ae_document_history_break(edit->document);accepted(edit);return true;
}
static bool neighbour(const struct ae_direct *edit,bool backwards,size_t *position) {
    struct ae_document_info info;struct ae_document_line line;size_t row,offsets[AE_DOCUMENT_LINE_BYTES+1],count;
    if (!state(edit,&info,&row,&line) || !points(edit,&line,offsets,&count)) return false;
    size_t at=0;while (at<count && line.start+offsets[at]<info.cursor) ++at;
    if (at==count || line.start+offsets[at]!=info.cursor) return false;
    *position=info.cursor;
    if (backwards) {
        if (at) *position=line.start+offsets[at-1];
        else if (row) { if (!ae_document_line(edit->document,row-1,&line)) return false;*position=line.start+line.size; }
    } else if (at+1<count) *position=line.start+offsets[at+1];
    else if (row+1<info.lines) { if (!ae_document_line(edit->document,row+1,&line)) return false;*position=line.start; }
    return true;
}
bool ae_direct_move(struct ae_direct *edit,enum ae_direct_move move,bool extend) {
    struct ae_document_info info;struct ae_document_line line;struct ae_direct_selection selection;size_t row;
    if (!available(edit) || !state(edit,&info,&row,&line) || !ae_direct_selection(edit,&selection)) return false;
    size_t position=info.cursor,goal=edit->goal;bool vertical=false;
    switch (move) {
    case AE_DIRECT_LEFT:case AE_DIRECT_RIGHT:
        if (!extend && selection.active) position=move==AE_DIRECT_LEFT ? selection.begin : selection.end;
        else if (!neighbour(edit,move==AE_DIRECT_LEFT,&position)) return false;
        break;
    case AE_DIRECT_UP:case AE_DIRECT_DOWN: {
        size_t offsets[AE_DOCUMENT_LINE_BYTES+1],count;
        if (!edit->vertical) {
            if (!points(edit,&line,offsets,&count)) return false;
            goal=0;while (goal<count && line.start+offsets[goal]<info.cursor) ++goal;
            if (goal==count || line.start+offsets[goal]!=info.cursor) return false;
        }
        if (move==AE_DIRECT_UP) { if (row) --row; }else if (row+1<info.lines) ++row;
        if (!ae_document_line(edit->document,row,&line) || !points(edit,&line,offsets,&count)) return false;
        position=line.start+offsets[goal<count ? goal : count-1];vertical=true;break;
    }
    case AE_DIRECT_HOME:position=line.start;break;
    case AE_DIRECT_END:position=line.start+line.size;break;
    case AE_DIRECT_FIRST:position=0;break;
    case AE_DIRECT_LAST:position=info.size;break;
    default:return false;
    }
    if (!place(edit,position,extend)) return false;
    edit->vertical=vertical;edit->goal=goal;return true;
}
enum ae_document_result ae_direct_text(struct ae_direct *edit,const char *text,size_t size) {
    struct ae_direct_selection selection;
    if (!available(edit) || !text || !size || !ae_direct_selection(edit,&selection)) return AE_DOCUMENT_INVALID;
    enum ae_document_result result=selection.active ? ae_document_replace(edit->document,selection.begin,selection.end,text,size) :
        ae_document_type(edit->document,text,size);
    if (result==AE_DOCUMENT_OK) collapsed(edit);
    return result;
}
enum ae_document_result ae_direct_delete(struct ae_direct *edit,bool backwards) {
    struct ae_document_info info;struct ae_direct_selection selection;
    if (!available(edit) || !ae_document_info(edit->document,&info) || !ae_direct_selection(edit,&selection)) return AE_DOCUMENT_INVALID;
    size_t begin=selection.begin,end=selection.end,other;
    if (!selection.active) {
        if (!neighbour(edit,backwards,&other)) return AE_DOCUMENT_INVALID;
        begin=other<info.cursor ? other : info.cursor;end=other>info.cursor ? other : info.cursor;
    }
    enum ae_document_result result=begin==end ? AE_DOCUMENT_OK : ae_document_replace(edit->document,begin,end,"",0);
    if (result==AE_DOCUMENT_OK) { ae_document_history_break(edit->document);collapsed(edit); }
    return result;
}
enum ae_document_result ae_direct_newline(struct ae_direct *edit) {
    struct ae_document_info info;struct ae_document_line line,previous;size_t row;
    if (!state(edit,&info,&row,&line)) return AE_DOCUMENT_INVALID;
    size_t size=line.ending_size;
    if (!size && row && ae_document_line(edit->document,row-1,&previous)) size=previous.ending_size;
    return ae_direct_text(edit,size==2 ? "\r\n" : "\n",size==2 ? 2u : 1u);
}
enum ae_document_result ae_direct_history(struct ae_direct *edit,int direction,bool *changed) {
    if (!available(edit) || !changed) return AE_DOCUMENT_INVALID;
    bool applied=false;enum ae_document_result result=ae_document_history(edit->document,direction,&applied);
    if (result==AE_DOCUMENT_OK) { collapsed(edit);*changed=applied; }
    return result;
}
enum ae_document_result ae_direct_paste(struct ae_direct *edit,const struct ae_direct_token *token,const char *text,size_t size) {
    struct ae_direct_token current;
    if (!token || !ae_direct_token(edit,&current) || token->identity!=current.identity || token->interaction!=current.interaction ||
        token->revision!=current.revision || token->cursor!=current.cursor || token->anchor!=current.anchor || token->selecting!=current.selecting ||
        !text || !size || !available(edit)) return AE_DOCUMENT_INVALID;
    struct ae_direct_selection selection;if (!ae_direct_selection(edit,&selection)) return AE_DOCUMENT_INVALID;
    enum ae_document_result result=ae_document_replace(edit->document,selection.begin,selection.end,text,size);
    if (result==AE_DOCUMENT_OK) { ae_document_history_break(edit->document);collapsed(edit); }
    return result;
}
