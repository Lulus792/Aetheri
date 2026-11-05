#include "editor.h"
#include "text/grapheme.h"
#include <stdlib.h>
#include <string.h>
struct ae_line_editor {
    char text[AE_LINE_BYTES+1],saved[AE_LINE_BYTES+1],kill[AE_LINE_BYTES+1];
    size_t size,cursor,boundaries[AE_LINE_BYTES+1],count,saved_size,saved_cursor,kill_size;
    char *history[AE_LINE_HISTORY];size_t lengths[AE_LINE_HISTORY],used,at,bytes;
    bool browsing;
};
static void wipe(void *memory,size_t size) { volatile unsigned char *p=memory;while (size--) *p++=0; }
static bool valid(const char *text,size_t size) {
    if (!text && size) return false;
    size_t at=0;
    while (at<size) {
        uint32_t c;
        if (!ae_utf8_next(text,size,&at,&c) || c<32 || (c>=0x7f && c<=0x9f) ||
            c==0x2028 || c==0x2029 || (c>=0x202a && c<=0x202e) || (c>=0x2066 && c<=0x2069)) return false;
    }
    return true;
}
static bool replace(struct ae_line_editor *e,size_t begin,size_t end,const char *text,size_t size) {
    if (begin>end || end>e->size || size>AE_LINE_BYTES-(e->size-(end-begin)) || !valid(text,size)) return false;
    char candidate[AE_LINE_BYTES+1];size_t length=e->size-(end-begin)+size,desired=begin+size;
    memcpy(candidate,e->text,begin);
    if (size) memcpy(candidate+begin,text,size);
    memcpy(candidate+desired,e->text+end,e->size-end);candidate[length]=0;
    size_t count;
    if (!ae_text_graphemes(candidate,length,e->boundaries,AE_LINE_BYTES+1,&count)) return false;
    memcpy(e->text,candidate,length+1);e->size=length;e->count=count;
    size_t index=0;while (e->boundaries[index]<desired) ++index;
    e->cursor=e->boundaries[index];return true;
}
struct ae_line_editor *ae_line_create(void) {
    struct ae_line_editor *e=calloc(1,sizeof(*e));if (e) e->count=1;return e;
}
void ae_line_destroy(struct ae_line_editor *e) {
    if (!e) return;
    for (size_t i=0;i<e->used;++i) { wipe(e->history[i],e->lengths[i]);free(e->history[i]); }
    wipe(e,sizeof(*e));free(e);
}
void ae_line_clear(struct ae_line_editor *e) {
    if (!e) return;
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
    return e && replace(e,e->cursor,e->cursor,text,size);
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
    case AE_LINE_LEFT:if (index) e->cursor=e->boundaries[index-1];return true;
    case AE_LINE_RIGHT:if (index+1<e->count) e->cursor=e->boundaries[index+1];return true;
    case AE_LINE_HOME:e->cursor=0;return true;
    case AE_LINE_END:e->cursor=e->size;return true;
    case AE_LINE_CLEAR:ae_line_clear(e);return true;
    case AE_LINE_YANK:return ae_line_insert(e,e->kill,e->kill_size);
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
    if (begin==end) return true;
    if (action==AE_LINE_KILL_START || action==AE_LINE_KILL_END || action==AE_LINE_WORD_BACK) {
        e->kill_size=end-begin;memcpy(e->kill,e->text+begin,e->kill_size);e->kill[e->kill_size]=0;
    }
    return replace(e,begin,end,NULL,0);
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
    if (!replace(e,0,e->size,text,size)) return false;
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
