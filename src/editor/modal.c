#include "modal.h"
#include "text/grapheme.h"
#include <stdlib.h>
#include <string.h>
struct ae_modal { struct ae_document *document;enum ae_modal_mode mode;size_t goal,anchor;bool vertical;
    char *copied;size_t copied_size;bool copied_lines;char separator[2];size_t separator_size; };
static bool visual(const struct ae_modal *m) { return m->mode==AE_MODAL_VISUAL || m->mode==AE_MODAL_LINES; }
static void wipe_free(char *text,size_t size) { if (text) { volatile char *p=text;while (size--) *p++=0;free(text); } }
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
void ae_modal_destroy(struct ae_modal *m) { if (m) { wipe_free(m->copied,m->copied_size);free(m); } }
const struct ae_document *ae_modal_document(const struct ae_modal *m) { return m ? m->document : NULL; }
enum ae_modal_mode ae_modal_mode(const struct ae_modal *m) { return m ? m->mode : AE_MODAL_NORMAL; }
bool ae_modal_seek(struct ae_modal *m,size_t position) {
    if (!m || m->mode!=AE_MODAL_NORMAL || !ae_document_cursor(m->document,position)) return false;
    m->vertical=false;return true;
}
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
bool ae_modal_select(struct ae_modal *m,bool linewise) {
    struct ae_document_info info;if (!m || m->mode==AE_MODAL_INSERT || !ae_document_info(m->document,&info)) return false;
    enum ae_modal_mode next=linewise ? AE_MODAL_LINES : AE_MODAL_VISUAL;
    if (m->mode==next) return ae_modal_action(m,AE_MODAL_ESCAPE)==AE_DOCUMENT_OK;
    if (!visual(m)) m->anchor=info.cursor;
    ae_document_history_break(m->document);m->mode=next;m->vertical=false;return true;
}
bool ae_modal_selection(const struct ae_modal *m,struct ae_modal_selection *s) {
    if (!m || !s) return false;
    *s=(struct ae_modal_selection){0};if (!visual(m)) return true;
    struct ae_document_info info;struct ae_document_line line;size_t row,anchor_row;
    if (!state(m,&info,&row,&line) || !ae_document_row(m->document,m->anchor,&anchor_row)) return false;
    s->active=true;s->linewise=m->mode==AE_MODAL_LINES;s->first_row=row<anchor_row ? row : anchor_row;s->last_row=row>anchor_row ? row : anchor_row;
    if (s->linewise) {
        if (!ae_document_line(m->document,s->first_row,&line)) return false;
        s->begin=line.start;if (!ae_document_line(m->document,s->last_row,&line)) return false;
        s->end=line.start+line.size+line.ending_size;return true;
    }
    s->begin=m->anchor<info.cursor ? m->anchor : info.cursor;size_t last=m->anchor>info.cursor ? m->anchor : info.cursor;
    if (!ae_document_row(m->document,last,&row) || !ae_document_line(m->document,row,&line)) return false;
    size_t offsets[AE_DOCUMENT_LINE_BYTES+1],count;if (!points(m,&line,offsets,&count)) return false;
    size_t at=0;while (at<count && line.start+offsets[at]<last) ++at;
    if (at==count || line.start+offsets[at]!=last) return false;
    s->end=at+1<count ? line.start+offsets[at+1] : line.start+line.size+line.ending_size;return true;
}
bool ae_modal_register_info(const struct ae_modal *m,struct ae_modal_register_info *info) {
    if (!m || !info) return false;
    *info=(struct ae_modal_register_info){m->copied_size,m->copied_lines};return true;
}
static enum ae_document_result paste(struct ae_modal *m,bool after) {
    if (m->mode!=AE_MODAL_NORMAL) return AE_DOCUMENT_INVALID;
    if (!m->copied_size) return AE_DOCUMENT_OK;
    struct ae_document_info info;struct ae_document_line line;size_t row;
    if (!state(m,&info,&row,&line)) return AE_DOCUMENT_INVALID;
    size_t begin=info.cursor,prefix=0,suffix=0;
    if (m->copied_lines) {
        begin=line.start;
        if (after) { begin+=line.size+line.ending_size;if (!line.ending_size && info.size) prefix=m->separator_size; }
        if (!prefix && info.size && m->copied[m->copied_size-1]!='\n') suffix=m->separator_size;
    } else if (after && info.cursor<line.start+line.size) {
        size_t offsets[AE_DOCUMENT_LINE_BYTES+1],count;if (!points(m,&line,offsets,&count)) return AE_DOCUMENT_INVALID;
        size_t at=0;while (at<count && line.start+offsets[at]<info.cursor) ++at;
        if (at+1>=count) return AE_DOCUMENT_INVALID;
        begin=line.start+offsets[at+1];
    }
    size_t extra=prefix+suffix;
    if (m->copied_size>AE_DOCUMENT_BYTES-extra || m->copied_size+extra>AE_DOCUMENT_BYTES-info.size) return AE_DOCUMENT_LIMIT;
    size_t size=m->copied_size+extra;char *joined=NULL;const char *text=m->copied;
    if (extra) {
        joined=malloc(size);if (!joined) return AE_DOCUMENT_NOMEM;
        memcpy(joined,m->separator,prefix);memcpy(joined+prefix,m->copied,m->copied_size);memcpy(joined+prefix+m->copied_size,m->separator,suffix);text=joined;
    }
    enum ae_document_result r=ae_document_replace(m->document,begin,begin,text,size);wipe_free(joined,size);
    if (r!=AE_DOCUMENT_OK) return r;
    m->vertical=false;return normalize(m) ? AE_DOCUMENT_OK : AE_DOCUMENT_INVALID;
}
enum ae_document_result ae_modal_transfer(struct ae_modal *m,enum ae_modal_transfer transfer) {
    if (!m) return AE_DOCUMENT_INVALID;
    if (transfer==AE_MODAL_PASTE_BEFORE || transfer==AE_MODAL_PASTE_AFTER) return paste(m,transfer==AE_MODAL_PASTE_AFTER);
    if (transfer!=AE_MODAL_YANK && transfer!=AE_MODAL_CUT && transfer!=AE_MODAL_CHANGE) return AE_DOCUMENT_INVALID;
    struct ae_modal_selection s;struct ae_document_info info;struct ae_document_line first,last;
    if (!ae_modal_selection(m,&s) || !s.active || !ae_document_info(m->document,&info) ||
        !ae_document_line(m->document,s.first_row,&first) || !ae_document_line(m->document,s.last_row,&last)) return AE_DOCUMENT_INVALID;
    size_t size=s.end-s.begin,separator_size;const char *separator=ending(m,s.first_row,&first,&separator_size);
    char *copy=size ? malloc(size) : NULL;if (size && !copy) return AE_DOCUMENT_NOMEM;
    if (size && !ae_document_copy(m->document,s.begin,size,copy,size)) { wipe_free(copy,size);return AE_DOCUMENT_INVALID; }
    size_t begin=s.begin;const char *replacement="";size_t replacement_size=0;
    if (s.linewise && transfer==AE_MODAL_CUT && !last.ending_size && s.first_row) {
        struct ae_document_line previous;if (!ae_document_line(m->document,s.first_row-1,&previous)) { wipe_free(copy,size);return AE_DOCUMENT_INVALID; }
        begin=previous.start+previous.size;
    }
    if (s.linewise && transfer==AE_MODAL_CHANGE && last.ending_size) { replacement=last.ending_size==2 ? "\r\n" : "\n";replacement_size=last.ending_size; }
    bool unchanged=begin==s.begin && size==replacement_size && (!size || !memcmp(copy,replacement,size));
    if (transfer!=AE_MODAL_YANK && !unchanged) {
        enum ae_document_result r=s.linewise && transfer==AE_MODAL_CHANGE ?
            ae_document_replace_line_start(m->document,begin,s.end,replacement,replacement_size) : ae_document_replace(m->document,begin,s.end,replacement,replacement_size);
        if (r!=AE_DOCUMENT_OK) { wipe_free(copy,size);return r; }
    } else if (transfer==AE_MODAL_CHANGE && !ae_document_cursor(m->document,s.begin)) { wipe_free(copy,size);return AE_DOCUMENT_INVALID; }
    if (size) {
        wipe_free(m->copied,m->copied_size);m->copied=copy;m->copied_size=size;m->copied_lines=s.linewise;
        memcpy(m->separator,separator,separator_size);m->separator_size=separator_size;
    }
    m->mode=transfer==AE_MODAL_CHANGE ? AE_MODAL_INSERT : AE_MODAL_NORMAL;m->vertical=false;
    return m->mode==AE_MODAL_NORMAL && !normalize(m) ? AE_DOCUMENT_INVALID : AE_DOCUMENT_OK;
}
static enum ae_document_result vertical(struct ae_modal *m,int direction,struct ae_document_info *info,size_t row,struct ae_document_line *line) {
    size_t offsets[AE_DOCUMENT_LINE_BYTES+1],count;
    if (!points(m,line,offsets,&count)) return AE_DOCUMENT_INVALID;
    size_t goal=m->goal;
    if (!m->vertical) { goal=0;while (goal<count && line->start+offsets[goal]<info->cursor) ++goal;if (goal==count) return AE_DOCUMENT_INVALID; }
    size_t target=row;if (direction<0 && row) --target;else if (direction>0 && row+1<info->lines) ++target;
    if (!ae_document_line(m->document,target,line) || !points(m,line,offsets,&count)) return AE_DOCUMENT_INVALID;
    size_t maximum=count-1;if (m->mode!=AE_MODAL_INSERT && maximum) --maximum;
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
    if (visual(m) && (action==AE_MODAL_DELETE || action==AE_MODAL_BACKSPACE)) return ae_modal_transfer(m,AE_MODAL_CUT);
    if (action==AE_MODAL_LEFT || action==AE_MODAL_RIGHT || (action==AE_MODAL_BACKSPACE && m->mode==AE_MODAL_NORMAL)) {
        int direction=action==AE_MODAL_RIGHT ? 1 : -1;
        if (m->mode==AE_MODAL_INSERT || (direction<0 ? info.cursor>line.start : info.cursor<line.start+line.size)) {
            if (!ae_document_step(m->document,direction)) return AE_DOCUMENT_INVALID;
        }
        if (m->mode!=AE_MODAL_INSERT && !normalize(m)) return AE_DOCUMENT_INVALID;
        m->vertical=false;return AE_DOCUMENT_OK;
    }
    if (action==AE_MODAL_HOME || action==AE_MODAL_END) {
        if (!ae_document_cursor(m->document,action==AE_MODAL_HOME ? line.start : line.start+line.size)) return AE_DOCUMENT_INVALID;
        if (m->mode!=AE_MODAL_INSERT && !normalize(m)) return AE_DOCUMENT_INVALID;
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
