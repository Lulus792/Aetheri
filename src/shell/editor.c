#include "editor.h"
#include "text/grapheme.h"
#include <stdlib.h>
#include <string.h>
struct edit_state { size_t size,cursor;char text[]; };
struct ae_line_editor {
    char text[AE_LINE_BYTES+1],saved[AE_LINE_BYTES+1],kill[AE_LINE_BYTES+1];
    size_t size,cursor,boundaries[AE_LINE_BYTES+1],count,saved_size,saved_cursor,kill_size;
    char *history[AE_LINE_HISTORY];size_t lengths[AE_LINE_HISTORY],used,at,bytes;
    struct edit_state *undo[AE_LINE_UNDO],*redo[AE_LINE_UNDO];
    size_t undone,redone,edit_bytes;
    bool browsing,grouped;
};
static void wipe(void *memory,size_t size) { volatile unsigned char *p=memory;while (size--) *p++=0; }
static size_t state_bytes(const struct edit_state *s) { return sizeof(*s)+s->size+1; }
static void drop(struct ae_line_editor *e,struct edit_state *s) {
    size_t bytes=state_bytes(s);e->edit_bytes-=bytes;wipe(s,bytes);free(s);
}
static void clear_edits(struct ae_line_editor *e) {
    while (e->undone) drop(e,e->undo[--e->undone]);
    while (e->redone) drop(e,e->redo[--e->redone]);
    e->grouped=false;
}
static struct edit_state *capture(const struct ae_line_editor *e) {
    struct edit_state *s=malloc(sizeof(*s)+e->size+1);if (!s) return NULL;
    s->size=e->size;s->cursor=e->cursor;memcpy(s->text,e->text,e->size+1);return s;
}
static void room(struct ae_line_editor *e,size_t bytes) {
    while (e->undone+e->redone>=AE_LINE_UNDO || e->edit_bytes>AE_LINE_UNDO_BYTES-bytes) {
        struct edit_state **stack=e->undone ? e->undo : e->redo;
        size_t *count=e->undone ? &e->undone : &e->redone;
        drop(e,stack[0]);--*count;memmove(stack,stack+1,*count*sizeof(*stack));
    }
}
bool ae_line_valid_utf8(const char *text,size_t size) {
    if (!text && size) return false;
    size_t at=0;
    while (at<size) {
        uint32_t c;
        if (!ae_utf8_next(text,size,&at,&c) || c<32 || (c>=0x7f && c<=0x9f) ||
            c==0x2028 || c==0x2029 || (c>=0x202a && c<=0x202e) || (c>=0x2066 && c<=0x2069)) return false;
    }
    return true;
}
static bool replace(struct ae_line_editor *e,size_t begin,size_t end,const char *text,size_t size,bool record,bool grouping) {
    if (begin>end || end>e->size || size>AE_LINE_BYTES-(e->size-(end-begin)) || !ae_line_valid_utf8(text,size)) return false;
    char candidate[AE_LINE_BYTES+1];size_t length=e->size-(end-begin)+size,desired=begin+size;
    memcpy(candidate,e->text,begin);
    if (size) memcpy(candidate+begin,text,size);
    memcpy(candidate+desired,e->text+end,e->size-end);candidate[length]=0;
    size_t count,boundaries[AE_LINE_BYTES+1];
    if (!ae_text_graphemes(candidate,length,boundaries,AE_LINE_BYTES+1,&count)) return false;
    bool changed=length!=e->size || memcmp(candidate,e->text,length)!=0;
    struct edit_state *saved=NULL;
    if (record && changed && !(grouping && e->grouped)) { saved=capture(e);if (!saved) return false; }
    if (record && changed) {
        while (e->redone) drop(e,e->redo[--e->redone]);
        if (saved) { size_t bytes=state_bytes(saved);room(e,bytes);e->undo[e->undone++]=saved;e->edit_bytes+=bytes; }
    }
    memcpy(e->text,candidate,length+1);memcpy(e->boundaries,boundaries,count*sizeof(*boundaries));e->size=length;e->count=count;
    size_t index=0;while (e->boundaries[index]<desired) ++index;
    e->cursor=e->boundaries[index];if (changed) e->grouped=record && grouping;return true;
}
struct ae_line_editor *ae_line_create(void) {
    struct ae_line_editor *e=calloc(1,sizeof(*e));if (e) e->count=1;return e;
}
void ae_line_destroy(struct ae_line_editor *e) {
    if (!e) return;
    clear_edits(e);
    for (size_t i=0;i<e->used;++i) { wipe(e->history[i],e->lengths[i]);free(e->history[i]); }
    wipe(e,sizeof(*e));free(e);
}
void ae_line_clear(struct ae_line_editor *e) {
    if (!e) return;
    clear_edits(e);
    wipe(e->text,sizeof(e->text));wipe(e->saved,sizeof(e->saved));
    e->size=e->cursor=e->saved_size=e->saved_cursor=0;e->count=1;e->boundaries[0]=0;e->browsing=false;e->at=e->used;
}
const char *ae_line_text(const struct ae_line_editor *e,size_t *size,size_t *cursor) {
    if (!e || !size || !cursor) return NULL;
    *size=e->size;*cursor=e->cursor;return e->text;
}
const size_t *ae_line_boundaries(const struct ae_line_editor *e,size_t *count) {
    if (!e || !count) return NULL;
    *count=e->count;return e->boundaries;
}
bool ae_line_insert(struct ae_line_editor *e,const char *text,size_t size) {
    return e && replace(e,e->cursor,e->cursor,text,size,true,true);
}
bool ae_line_replace(struct ae_line_editor *e,size_t begin,size_t end,const char *text,size_t size) {
    if (!e) return false;
    bool a=false,b=false;
    for (size_t i=0;i<e->count;++i) { a|=e->boundaries[i]==begin;b|=e->boundaries[i]==end; }
    if (!a || !b || !replace(e,begin,end,text,size,true,false)) return false;
    e->grouped=false;return true;
}
bool ae_line_undo(struct ae_line_editor *e,int direction) {
    if (!e || (direction!=-1 && direction!=1)) return false;
    struct edit_state **from=direction<0 ? e->undo : e->redo;
    struct edit_state **to=direction<0 ? e->redo : e->undo;
    size_t *used=direction<0 ? &e->undone : &e->redone,*target=direction<0 ? &e->redone : &e->undone;
    if (!*used) { e->grouped=false;return true; }
    struct edit_state *saved=capture(e);if (!saved) return false;
    struct edit_state *old=from[--*used];
    (void)replace(e,0,e->size,old->text,old->size,false,false);e->cursor=old->cursor;
    drop(e,old);size_t bytes=state_bytes(saved);room(e,bytes);to[(*target)++]=saved;e->edit_bytes+=bytes;e->grouped=false;return true;
}
static size_t current(const struct ae_line_editor *e) {
    size_t lo=0,hi=e->count;
    while (lo<hi) { size_t mid=lo+(hi-lo)/2;if (e->boundaries[mid]<e->cursor) lo=mid+1;else hi=mid; }
    return lo;
}
bool ae_line_edit(struct ae_line_editor *e,enum ae_line_action action) {
    if (!e) return false;
    size_t index=current(e),begin=e->cursor,end=begin;
    switch (action) {
    case AE_LINE_LEFT:e->grouped=false;if (index) e->cursor=e->boundaries[index-1];return true;
    case AE_LINE_RIGHT:e->grouped=false;if (index+1<e->count) e->cursor=e->boundaries[index+1];return true;
    case AE_LINE_HOME:e->grouped=false;e->cursor=0;return true;
    case AE_LINE_END:e->grouped=false;e->cursor=e->size;return true;
    case AE_LINE_CLEAR:ae_line_clear(e);return true;
    case AE_LINE_YANK:return replace(e,e->cursor,e->cursor,e->kill,e->kill_size,true,false);
    case AE_LINE_BACKSPACE:if (index) begin=e->boundaries[index-1];break;
    case AE_LINE_DELETE:if (index+1<e->count) end=e->boundaries[index+1];break;
    case AE_LINE_KILL_START:begin=0;break;
    case AE_LINE_KILL_END:end=e->size;break;
    case AE_LINE_WORD_BACK:
        while (index && e->text[e->boundaries[index-1]]==' ') --index;
        while (index && e->text[e->boundaries[index-1]]!=' ') --index;
        begin=e->boundaries[index];break;
    default:return false;
    }
    if (begin==end) { e->grouped=false;return true; }
    bool cut=action==AE_LINE_KILL_START || action==AE_LINE_KILL_END || action==AE_LINE_WORD_BACK;
    char removed[AE_LINE_BYTES+1];size_t removed_size=end-begin;
    if (cut) memcpy(removed,e->text+begin,removed_size);
    if (!replace(e,begin,end,NULL,0,true,false)) return false;
    if (cut) { e->kill_size=removed_size;memcpy(e->kill,removed,removed_size);e->kill[removed_size]=0; }
    return true;
}
bool ae_line_history(struct ae_line_editor *e,int direction) {
    if (!e || (direction!=-1 && direction!=1)) return false;
    if (!e->browsing) {
        if (direction>0 || !e->used) return true;
        memcpy(e->saved,e->text,e->size+1);e->saved_size=e->size;e->saved_cursor=e->cursor;e->at=e->used;e->browsing=true;
    }
    size_t at=e->at;
    if (direction<0) { if (at) --at; }
    else if (at<e->used) ++at;
    const char *text=at==e->used ? e->saved : e->history[at];
    size_t size=at==e->used ? e->saved_size : e->lengths[at];
    if (!replace(e,0,e->size,text,size,false,false)) return false;
    clear_edits(e);
    e->at=at;if (at==e->used) { e->cursor=e->saved_cursor;e->browsing=false; }
    return true;
}
bool ae_line_remember(struct ae_line_editor *e) {
    if (!e) return false;
    if (!e->size || e->text[0]==' ' || (e->used && e->lengths[e->used-1]==e->size && !memcmp(e->history[e->used-1],e->text,e->size))) return true;
    char *copy=malloc(e->size+1);if (!copy) return false;memcpy(copy,e->text,e->size+1);
    while (e->used==AE_LINE_HISTORY || e->bytes>AE_LINE_HISTORY_BYTES-(e->size+1)) {
        e->bytes-=e->lengths[0]+1;wipe(e->history[0],e->lengths[0]);free(e->history[0]);--e->used;
        memmove(e->history,e->history+1,e->used*sizeof(e->history[0]));
        memmove(e->lengths,e->lengths+1,e->used*sizeof(e->lengths[0]));
    }
    e->history[e->used]=copy;e->lengths[e->used++]=e->size;e->bytes+=e->size+1;e->at=e->used;e->browsing=false;return true;
}
