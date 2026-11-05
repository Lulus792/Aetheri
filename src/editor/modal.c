#include "modal.h"
#include "text/grapheme.h"
#include <stdlib.h>
struct ae_modal { struct ae_document *document;enum ae_modal_mode mode;size_t goal;bool vertical; };
static bool state(const struct ae_modal *m,struct ae_document_info *info,size_t *row,struct ae_document_line *line) {
    return m && ae_document_info(m->document,info) && ae_document_row(m->document,info->cursor,row) && ae_document_line(m->document,*row,line);
}
static bool points(const struct ae_modal *m,const struct ae_document_line *line,size_t *offsets,size_t *count) {
    char text[AE_DOCUMENT_LINE_BYTES];return ae_document_copy(m->document,line->start,line->size,text,sizeof(text)) &&
        ae_text_graphemes(text,line->size,offsets,AE_DOCUMENT_LINE_BYTES+1,count);
}
static bool normalize(struct ae_modal *m) {
    struct ae_document_info info;struct ae_document_line line;size_t row;
    if (!state(m,&info,&row,&line)) return false;
    return !line.size || info.cursor<line.start+line.size || ae_document_step(m->document,-1);
}
struct ae_modal *ae_modal_create(struct ae_document *document) {
    if (!document) return NULL;
    struct ae_modal *m=calloc(1,sizeof(*m));if (!m) return NULL;
    m->document=document;if (!normalize(m)) { free(m);return NULL; }return m;
}
void ae_modal_destroy(struct ae_modal *m) { free(m); }
const struct ae_document *ae_modal_document(const struct ae_modal *m) { return m ? m->document : NULL; }
enum ae_modal_mode ae_modal_mode(const struct ae_modal *m) { return m ? m->mode : AE_MODAL_NORMAL; }
enum ae_document_result ae_modal_text(struct ae_modal *m,const char *text,size_t size) {
    if (!m || m->mode!=AE_MODAL_INSERT) return AE_DOCUMENT_INVALID;
    struct ae_document_info info;if (!ae_document_info(m->document,&info)) return AE_DOCUMENT_INVALID;
    enum ae_document_result result=ae_document_type(m->document,text,size);
    if (result==AE_DOCUMENT_OK) m->vertical=false;
    return result;
}
enum ae_document_result ae_modal_history(struct ae_modal *m,int direction,bool *changed) {
    if (!m) return AE_DOCUMENT_INVALID;
    enum ae_document_result r=ae_document_history(m->document,direction,changed);
    if (r==AE_DOCUMENT_OK && *changed) { m->mode=AE_MODAL_NORMAL;m->vertical=false;if (!normalize(m)) return AE_DOCUMENT_INVALID; }
    return r;
}
static const char *ending(const struct ae_modal *m,size_t row,const struct ae_document_line *line,size_t *size) {
    struct ae_document_line previous;size_t bytes=line->ending_size;
    if (!bytes && row && ae_document_line(m->document,row-1,&previous)) bytes=previous.ending_size;
    *size=bytes==2 ? 2u : 1u;return bytes==2 ? "\r\n" : "\n";
}
static enum ae_document_result vertical(struct ae_modal *m,int direction,struct ae_document_info *info,size_t row,struct ae_document_line *line) {
    size_t offsets[AE_DOCUMENT_LINE_BYTES+1],count;
    if (!points(m,line,offsets,&count)) return AE_DOCUMENT_INVALID;
    size_t goal=m->goal;
    if (!m->vertical) { goal=0;while (goal<count && line->start+offsets[goal]<info->cursor) ++goal;if (goal==count) return AE_DOCUMENT_INVALID; }
    size_t target=row;if (direction<0 && row) --target;else if (direction>0 && row+1<info->lines) ++target;
    if (!ae_document_line(m->document,target,line) || !points(m,line,offsets,&count)) return AE_DOCUMENT_INVALID;
    size_t maximum=count-1;if (m->mode==AE_MODAL_NORMAL && maximum) --maximum;
    size_t column=goal<maximum ? goal : maximum;
    if (!ae_document_cursor(m->document,line->start+offsets[column])) return AE_DOCUMENT_INVALID;
    m->vertical=true;m->goal=goal;return AE_DOCUMENT_OK;
}
enum ae_document_result ae_modal_action(struct ae_modal *m,enum ae_modal_action action) {
    struct ae_document_info info;struct ae_document_line line;size_t row;
    if (!state(m,&info,&row,&line)) return AE_DOCUMENT_INVALID;
    ae_document_history_break(m->document);
    if (action==AE_MODAL_UP || action==AE_MODAL_DOWN) return vertical(m,action==AE_MODAL_UP ? -1 : 1,&info,row,&line);
    if (action==AE_MODAL_ESCAPE) {
        if (m->mode==AE_MODAL_INSERT && info.cursor>line.start && !ae_document_step(m->document,-1)) return AE_DOCUMENT_INVALID;
        m->mode=AE_MODAL_NORMAL;m->vertical=false;return normalize(m) ? AE_DOCUMENT_OK : AE_DOCUMENT_INVALID;
    }
    if (action==AE_MODAL_LEFT || action==AE_MODAL_RIGHT || (action==AE_MODAL_BACKSPACE && m->mode==AE_MODAL_NORMAL)) {
        int direction=action==AE_MODAL_RIGHT ? 1 : -1;
        if (m->mode==AE_MODAL_INSERT || (direction<0 ? info.cursor>line.start : info.cursor<line.start+line.size)) {
            if (!ae_document_step(m->document,direction)) return AE_DOCUMENT_INVALID;
        }
        if (m->mode==AE_MODAL_NORMAL && !normalize(m)) return AE_DOCUMENT_INVALID;
        m->vertical=false;return AE_DOCUMENT_OK;
    }
    if (action==AE_MODAL_HOME || action==AE_MODAL_END) {
        if (!ae_document_cursor(m->document,action==AE_MODAL_HOME ? line.start : line.start+line.size)) return AE_DOCUMENT_INVALID;
        if (m->mode==AE_MODAL_NORMAL && !normalize(m)) return AE_DOCUMENT_INVALID;
        m->vertical=false;return AE_DOCUMENT_OK;
    }
    if (action==AE_MODAL_ENTER_INSERT || action==AE_MODAL_APPEND || action==AE_MODAL_LINE_INSERT || action==AE_MODAL_LINE_APPEND) {
        if (m->mode!=AE_MODAL_NORMAL) return AE_DOCUMENT_INVALID;
        if (action==AE_MODAL_APPEND && info.cursor<line.start+line.size && !ae_document_step(m->document,1)) return AE_DOCUMENT_INVALID;
        if ((action==AE_MODAL_LINE_INSERT || action==AE_MODAL_LINE_APPEND) &&
            !ae_document_cursor(m->document,action==AE_MODAL_LINE_INSERT ? line.start : line.start+line.size)) return AE_DOCUMENT_INVALID;
        m->mode=AE_MODAL_INSERT;m->vertical=false;return AE_DOCUMENT_OK;
    }
    size_t begin=info.cursor,end=begin,size=0,cursor=0;const char *text="";bool insert=false,reposition=false;
    if (action==AE_MODAL_DELETE || action==AE_MODAL_BACKSPACE) {
        size_t offsets[AE_DOCUMENT_LINE_BYTES+1],count;if (!points(m,&line,offsets,&count)) return AE_DOCUMENT_INVALID;
        size_t at=0;while (at<count && line.start+offsets[at]<info.cursor) ++at;
        if (at==count) return AE_DOCUMENT_INVALID;
        if (action==AE_MODAL_DELETE) {
            if (at+1<count) end=line.start+offsets[at+1];
            else if (m->mode==AE_MODAL_INSERT && row+1<info.lines) { struct ae_document_line next;if (!ae_document_line(m->document,row+1,&next)) return AE_DOCUMENT_INVALID;end=next.start; }
        } else if (at) begin=line.start+offsets[at-1];
        else if (row) { struct ae_document_line previous;if (!ae_document_line(m->document,row-1,&previous)) return AE_DOCUMENT_INVALID;begin=previous.start+previous.size; }
    } else if (action==AE_MODAL_NEWLINE && m->mode==AE_MODAL_INSERT) {
        text=ending(m,row,&line,&size);
    } else if ((action==AE_MODAL_OPEN_BEFORE || action==AE_MODAL_OPEN_AFTER) && m->mode==AE_MODAL_NORMAL) {
        text=ending(m,row,&line,&size);insert=reposition=true;
        if (action==AE_MODAL_OPEN_BEFORE) begin=end=cursor=line.start;
        else if (row+1<info.lines) { struct ae_document_line next;if (!ae_document_line(m->document,row+1,&next)) return AE_DOCUMENT_INVALID;begin=end=cursor=next.start; }
        else { begin=end=line.start+line.size;cursor=begin+size; }
    } else if (action==AE_MODAL_DELETE_LINE && m->mode==AE_MODAL_NORMAL) {
        begin=line.start;end=line.start+line.size+line.ending_size;
        if (!line.ending_size && row) { struct ae_document_line previous;if (!ae_document_line(m->document,row-1,&previous)) return AE_DOCUMENT_INVALID;begin=previous.start+previous.size; }
    } else return AE_DOCUMENT_INVALID;
    enum ae_document_result result=ae_document_replace(m->document,begin,end,text,size);
    if (result!=AE_DOCUMENT_OK) return result;
    if (reposition && !ae_document_cursor(m->document,cursor)) return AE_DOCUMENT_INVALID;
    if (insert) m->mode=AE_MODAL_INSERT;
    if (m->mode==AE_MODAL_NORMAL && !normalize(m)) return AE_DOCUMENT_INVALID;
    m->vertical=false;return AE_DOCUMENT_OK;
}
