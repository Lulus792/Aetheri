#include "document.h"
#include "text/grapheme.h"
#include <stdlib.h>
#include <string.h>
struct edit {
    size_t begin,removed,inserted,capacity,before_cursor,after_cursor;
    uint64_t before_state,after_state;bool typed;char data[];
};
struct ae_document {
    char *bytes;size_t capacity,gap_begin,gap_end,size,cursor;
    size_t *starts,line_capacity,lines;uint64_t revision,state,clean;
    struct edit *history[AE_DOCUMENT_HISTORY];size_t used,at,history_bytes;bool merging;
};
struct ae_document_snapshot {struct ae_document *owner,*body;uint64_t revision,state,body_revision;bool acknowledged;};
struct statistics { size_t breaks,first,last; };
static void wipe(void *memory,size_t size) { volatile unsigned char *p=memory;while (size--) *p++=0; }
static size_t edit_bytes(const struct edit *e) { return sizeof(*e)+e->capacity; }
static void edit_free(struct edit *e) { if (e) { wipe(e,edit_bytes(e));free(e); } }
static void drop_oldest(struct ae_document *d) {
    d->history_bytes-=edit_bytes(d->history[0]);edit_free(d->history[0]);--d->used;if (d->at) --d->at;
    memmove(d->history,d->history+1,d->used*sizeof(*d->history));
}
void ae_document_history_break(struct ae_document *d) { if (d) d->merging=false; }
bool ae_document_history_info(const struct ae_document *d,struct ae_document_history_info *info) {
    if (!d || !info) return false;
    *info=(struct ae_document_history_info){d->at,d->used-d->at,d->history_bytes};return true;
}
static enum ae_document_result statistics(const char *text,size_t size,struct statistics *s) {
    *s=(struct statistics){0};if (size>AE_DOCUMENT_BYTES) return AE_DOCUMENT_LIMIT;
    if (!text && size) return AE_DOCUMENT_INVALID;
    size_t at=0,start=0;
    while (at<size) {
        size_t before=at;uint32_t c;
        if (!ae_utf8_next(text,size,&at,&c) || (c<32 && c!='\t' && c!='\n' && c!='\r') || (c>=0x7f && c<=0x9f)) return AE_DOCUMENT_INVALID;
        if (c=='\r' && (at==size || text[at]!='\n')) return AE_DOCUMENT_INVALID;
        if (c=='\n') {
            size_t length=before-start-(before>start && text[before-1]=='\r' ? 1u : 0u);
            if (length>AE_DOCUMENT_LINE_BYTES || ++s->breaks>=AE_DOCUMENT_LINES) return AE_DOCUMENT_LIMIT;
            if (s->breaks==1) s->first=length;
            start=at;
        }
    }
    s->last=size-start;if (!s->breaks) s->first=s->last;
    return s->last>AE_DOCUMENT_LINE_BYTES ? AE_DOCUMENT_LIMIT : AE_DOCUMENT_OK;
}
static size_t capacity(size_t old,size_t need,size_t minimum,size_t maximum) {
    size_t n=old ? old : minimum;while (n<need) n=n>maximum/2 ? maximum : n*2;return n;
}
struct ae_document *ae_document_create(const char *text,size_t size,enum ae_document_result *result) {
    if (!result) return NULL;
    struct statistics s;*result=statistics(text,size,&s);if (*result!=AE_DOCUMENT_OK) return NULL;
    struct ae_document *d=calloc(1,sizeof(*d));if (!d) { *result=AE_DOCUMENT_NOMEM;return NULL; }
    d->lines=s.breaks+1;d->line_capacity=capacity(0,d->lines,64,AE_DOCUMENT_LINES);
    d->starts=malloc(d->line_capacity*sizeof(*d->starts));
    if (size) { d->capacity=capacity(0,size,4096,AE_DOCUMENT_BYTES);d->bytes=malloc(d->capacity); }
    if (!d->starts || (size && !d->bytes)) { *result=AE_DOCUMENT_NOMEM;ae_document_destroy(d);return NULL; }
    if (size) memcpy(d->bytes,text,size);
    d->size=d->gap_begin=size;d->gap_end=d->capacity;d->revision=d->state=d->clean=1;
    d->starts[0]=0;size_t row=1;for (size_t i=0;i<size;++i) if (text[i]=='\n') d->starts[row++]=i+1;
    return d;
}
void ae_document_destroy(struct ae_document *d) {
    if (!d) return;
    if (d->bytes) { wipe(d->bytes,d->capacity);free(d->bytes); }
    for (size_t i=0;i<d->used;++i) edit_free(d->history[i]);
    free(d->starts);wipe(d,sizeof(*d));free(d);
}
bool ae_document_info(const struct ae_document *d,struct ae_document_info *info) {
    if (!d || !info) return false;
    *info=(struct ae_document_info){d->size,d->lines,d->cursor,d->revision,d->state!=d->clean};return true;
}
enum ae_document_result ae_document_snapshot_create(struct ae_document *d,struct ae_document_snapshot **out) {
    if(!d || !out) return AE_DOCUMENT_INVALID;
    struct ae_document_snapshot *s=calloc(1,sizeof(*s));if(!s) return AE_DOCUMENT_NOMEM;
    char *bytes=d->size?malloc(d->size):NULL;
    if(d->size && !bytes) {free(s);return AE_DOCUMENT_NOMEM;}
    if(!ae_document_copy(d,0,d->size,bytes,d->size)) {free(bytes);free(s);return AE_DOCUMENT_INVALID;}
    enum ae_document_result result;s->body=ae_document_create(bytes,d->size,&result);
    if(bytes) {wipe(bytes,d->size);free(bytes);}
    if(!s->body) {free(s);return result;}
    s->owner=d;s->revision=d->revision;s->state=d->state;s->body_revision=s->body->revision;d->merging=false;*out=s;return AE_DOCUMENT_OK;
}
struct ae_document *ae_document_snapshot_body(struct ae_document_snapshot *s) {return s?s->body:NULL;}
bool ae_document_snapshot_acknowledge(struct ae_document *d,struct ae_document_snapshot *s) {
    if(!d || !s || s->owner!=d || d->revision<s->revision) return false;
    if(s->acknowledged) return true;
    if(s->body->revision!=s->body_revision) return false;
    d->clean=s->state;
    if(d->state==s->state) d->merging=false;
    s->acknowledged=true;return true;
}
void ae_document_snapshot_destroy(struct ae_document_snapshot *s) {
    if(s) {ae_document_destroy(s->body);wipe(s,sizeof(*s));free(s);}
}
bool ae_document_slice(const struct ae_document *d,size_t start,size_t size,struct ae_document_slice *slice) {
    if (!slice) return false;
    *slice=(struct ae_document_slice){0};if (!d || start>d->size || size>d->size-start) return false;
    if (!size) return true;
    if (start<d->gap_begin) {
        size_t first=d->gap_begin-start;if (first>size) first=size;
        slice->first=d->bytes+start;slice->first_size=first;
        if (first<size) { slice->second=d->bytes+d->gap_end;slice->second_size=size-first; }
    } else { slice->first=d->bytes+start+(d->gap_end-d->gap_begin);slice->first_size=size; }
    return true;
}
bool ae_document_copy(const struct ae_document *d,size_t start,size_t size,char *out,size_t available) {
    struct ae_document_slice s;if (size>available || (!out && size) || !ae_document_slice(d,start,size,&s)) return false;
    if (s.first_size) memcpy(out,s.first,s.first_size);
    if (s.second_size) memcpy(out+s.first_size,s.second,s.second_size);
    return true;
}
static char byte(const struct ae_document *d,size_t position) { return d->bytes[position<d->gap_begin ? position : position+d->gap_end-d->gap_begin]; }
bool ae_document_line(const struct ae_document *d,size_t row,struct ae_document_line *line) {
    if (!d || !line || row>=d->lines) return false;
    size_t start=d->starts[row],end=row+1<d->lines ? d->starts[row+1] : d->size,ending=0;
    if (row+1<d->lines) { ending=1;if (end-start>1 && byte(d,end-2)=='\r') ++ending; }
    *line=(struct ae_document_line){start,end-start-ending,ending};return true;
}
bool ae_document_row(const struct ae_document *d,size_t position,size_t *row) {
    if (!d || !row || position>d->size) return false;
    size_t lo=0,hi=d->lines;
    while (lo+1<hi) { size_t mid=lo+(hi-lo)/2;if (d->starts[mid]<=position) lo=mid;else hi=mid; }
    *row=lo;return true;
}
static bool points(const struct ae_document *d,size_t row,size_t *offsets,size_t *count,struct ae_document_line *line) {
    char text[AE_DOCUMENT_LINE_BYTES];
    return ae_document_line(d,row,line) && ae_document_copy(d,line->start,line->size,text,sizeof(text)) &&
        ae_text_graphemes(text,line->size,offsets,AE_DOCUMENT_LINE_BYTES+1,count);
}
static bool boundary(const struct ae_document *d,size_t position,size_t *row) {
    if (!ae_document_row(d,position,row)) return false;
    size_t offsets[AE_DOCUMENT_LINE_BYTES+1],count;struct ae_document_line line;
    if (!points(d,*row,offsets,&count,&line) || position>line.start+line.size) return false;
    for (size_t i=0;i<count;++i) if (line.start+offsets[i]==position) return true;
    return false;
}
bool ae_document_cursor(struct ae_document *d,size_t position) {
    size_t row;if (!boundary(d,position,&row)) return false;
    d->cursor=position;d->merging=false;return true;
}
bool ae_document_step(struct ae_document *d,int direction) {
    if (!d || (direction!=-1 && direction!=1)) return false;
    size_t row,offsets[AE_DOCUMENT_LINE_BYTES+1],count;struct ae_document_line line;
    if (!ae_document_row(d,d->cursor,&row) || !points(d,row,offsets,&count,&line)) return false;
    size_t at=0;while (at<count && line.start+offsets[at]<d->cursor) ++at;
    if (at==count || line.start+offsets[at]!=d->cursor) return false;
    d->merging=false;
    if (direction>0) {
        if (at+1<count) d->cursor=line.start+offsets[at+1];else if (row+1<d->lines) d->cursor=d->starts[row+1];
    } else if (at) d->cursor=line.start+offsets[at-1];
    else if (row) { if (!ae_document_line(d,row-1,&line)) return false;d->cursor=line.start+line.size; }
    return true;
}
static void move_gap(struct ae_document *d,size_t position) {
    if (position<d->gap_begin) {
        size_t n=d->gap_begin-position;memmove(d->bytes+d->gap_end-n,d->bytes+position,n);d->gap_begin-=n;d->gap_end-=n;
    } else if (position>d->gap_begin) {
        size_t n=position-d->gap_begin;memmove(d->bytes+d->gap_begin,d->bytes+d->gap_end,n);d->gap_begin+=n;d->gap_end+=n;
    }
}
static enum ae_document_result replace(struct ae_document *d,size_t begin,size_t end,const char *text,size_t size,bool record,bool typing,bool stay_at_line_start) {
    if (!d || begin>end || end>d->size) return AE_DOCUMENT_INVALID;
    if (size>AE_DOCUMENT_BYTES-(d->size-(end-begin))) return AE_DOCUMENT_LIMIT;
    struct statistics s;enum ae_document_result status=statistics(text,size,&s);if (status!=AE_DOCUMENT_OK) return status;
    size_t first,last;
    if (record ? (!boundary(d,begin,&first) || !boundary(d,end,&last)) : (!ae_document_row(d,begin,&first) || !ae_document_row(d,end,&last))) return AE_DOCUMENT_INVALID;
    if (stay_at_line_start && begin!=d->starts[first]) return AE_DOCUMENT_INVALID;
    if (begin==end && !size) return AE_DOCUMENT_OK;
    if (d->revision==UINT64_MAX) return AE_DOCUMENT_EXHAUSTED;
    size_t lines=d->lines-(last-first)+s.breaks;if (lines>AE_DOCUMENT_LINES) return AE_DOCUMENT_LIMIT;
    struct ae_document_line a,b;
    if (!ae_document_line(d,first,&a) || !ae_document_line(d,last,&b)) return AE_DOCUMENT_INVALID;
    if (begin>a.start+a.size || end>b.start+b.size) return AE_DOCUMENT_INVALID;
    size_t prefix=begin-a.start,tail=b.start+b.size-end;
    if ((!s.breaks && prefix+size+tail>AE_DOCUMENT_LINE_BYTES) ||
        (s.breaks && (prefix+s.first>AE_DOCUMENT_LINE_BYTES || s.last+tail>AE_DOCUMENT_LINE_BYTES))) return AE_DOCUMENT_LIMIT;
    char joined[AE_DOCUMENT_LINE_BYTES];size_t used=0,desired;
    if (!s.breaks) { (void)ae_document_copy(d,a.start,prefix,joined,sizeof(joined));used=prefix; }
    size_t insert=s.breaks ? s.last : size;if (insert) memcpy(joined+used,text+size-insert,insert);used+=insert;desired=used;
    (void)ae_document_copy(d,end,tail,joined+used,sizeof(joined)-used);used+=tail;
    size_t offsets[AE_DOCUMENT_LINE_BYTES+1],count;
    if (!ae_text_graphemes(joined,used,offsets,AE_DOCUMENT_LINE_BYTES+1,&count)) return AE_DOCUMENT_INVALID;
    size_t snap=0;while (offsets[snap]<desired) ++snap;
    size_t cursor=begin+size+offsets[snap]-desired,new_size=d->size-(end-begin)+size;
    if (stay_at_line_start) cursor=begin;
    size_t new_capacity=d->capacity,new_lines=d->line_capacity;char *bytes=NULL;size_t *starts=NULL;
    if (new_size>d->capacity) { new_capacity=capacity(d->capacity,new_size,4096,AE_DOCUMENT_BYTES);bytes=malloc(new_capacity);if (!bytes) return AE_DOCUMENT_NOMEM; }
    if (lines>d->line_capacity) {
        new_lines=capacity(d->line_capacity,lines,64,AE_DOCUMENT_LINES);starts=malloc(new_lines*sizeof(*starts));
        if (!starts) { free(bytes);return AE_DOCUMENT_NOMEM; }
    }
    /* All text/index/history allocation precedes mutation. Inverse deltas may
       start inside a newly joined grapheme; only private original history can
       select this raw byte path, with its exact logical current-state binding. */
    struct edit *entry=NULL,*previous=NULL;size_t old_inserted=0;bool merge=false;
    if (record) {
        typing=typing && begin==end && !s.breaks;
        if (typing && d->merging && d->at==d->used && d->at) {
            previous=d->history[d->at-1];merge=previous->typed && !previous->removed && previous->begin+previous->inserted==begin && previous->after_state==d->state;
        }
        size_t removed=merge ? previous->removed : end-begin,inserted=size+(merge ? previous->inserted : 0u),need=removed+inserted;
        if (merge && need<=previous->capacity) entry=previous;
        else {
            size_t allocation=capacity(merge ? previous->capacity : 0,need,64,2u*AE_DOCUMENT_BYTES);
            entry=malloc(sizeof(*entry)+allocation);
            if (!entry) { free(bytes);free(starts);return AE_DOCUMENT_NOMEM; }
            *entry=(struct edit){.begin=merge ? previous->begin : begin,.removed=removed,.capacity=allocation,
                .before_cursor=merge ? previous->before_cursor : d->cursor,.before_state=merge ? previous->before_state : d->state,.typed=typing};
            if (merge) memcpy(entry->data,previous->data,removed+previous->inserted);
            else if (removed) (void)ae_document_copy(d,begin,removed,entry->data,removed);
        }
        old_inserted=merge ? previous->inserted : 0;
        if (entry!=previous && size) memcpy(entry->data+removed+old_inserted,text,size);
    }
    size_t after=d->lines-last-1,at=first+1+s.breaks;size_t *index=starts ? starts : d->starts;
    if (starts) { memcpy(index,d->starts,(first+1)*sizeof(*index));memcpy(index+at,d->starts+last+1,after*sizeof(*index)); }
    else memmove(index+at,index+last+1,after*sizeof(*index));
    for (size_t i=at;i<at+after;++i) index[i]=size>=end-begin ? index[i]+(size-(end-begin)) : index[i]-((end-begin)-size);
    size_t row=first+1;for (size_t i=0;i<size;++i) if (text[i]=='\n') index[row++]=begin+i+1;
    if (bytes) {
        (void)ae_document_copy(d,0,begin,bytes,new_capacity);if (size) memcpy(bytes+begin,text,size);
        size_t remaining=d->size-end;(void)ae_document_copy(d,end,remaining,bytes+new_capacity-remaining,remaining);
        if (d->bytes) { wipe(d->bytes,d->capacity);free(d->bytes); }
        d->bytes=bytes;d->capacity=new_capacity;d->gap_begin=begin+size;d->gap_end=new_capacity-remaining;
    } else {
        move_gap(d,begin);d->gap_end+=end-begin;if (size) memcpy(d->bytes+d->gap_begin,text,size);d->gap_begin+=size;
    }
    if (starts) { free(d->starts);d->starts=starts;d->line_capacity=new_lines; }
    d->size=new_size;d->lines=lines;d->cursor=cursor;++d->revision;
    if (record) {
        while (d->used>d->at) { struct edit *e=d->history[--d->used];d->history_bytes-=edit_bytes(e);edit_free(e); }
        if (merge) {
            if (entry==previous && size) memcpy(entry->data+entry->removed+old_inserted,text,size);
            if (entry!=previous) { d->history_bytes-=edit_bytes(previous);edit_free(previous);d->history[d->at-1]=entry;d->history_bytes+=edit_bytes(entry); }
        } else {
            if (d->used==AE_DOCUMENT_HISTORY) drop_oldest(d);
            d->history[d->used++]=entry;d->at=d->used;d->history_bytes+=edit_bytes(entry);
        }
        entry->inserted=old_inserted+size;entry->after_cursor=d->cursor;entry->after_state=d->state=d->revision;d->merging=typing;
        while (d->history_bytes>AE_DOCUMENT_HISTORY_BYTES && d->used>1) drop_oldest(d);
    }
    return AE_DOCUMENT_OK;
}
enum ae_document_result ae_document_replace(struct ae_document *d,size_t begin,size_t end,const char *text,size_t size) { return replace(d,begin,end,text,size,true,false,false); }
enum ae_document_result ae_document_replace_line_start(struct ae_document *d,size_t begin,size_t end,const char *text,size_t size) { return replace(d,begin,end,text,size,true,false,true); }
enum ae_document_result ae_document_type(struct ae_document *d,const char *text,size_t size) { return d ? replace(d,d->cursor,d->cursor,text,size,true,true,false) : AE_DOCUMENT_INVALID; }
enum ae_document_result ae_document_history(struct ae_document *d,int direction,bool *changed) {
    if (!changed) return AE_DOCUMENT_INVALID;
    *changed=false;if (!d || (direction!=-1 && direction!=1)) return AE_DOCUMENT_INVALID;
    if (direction<0 ? !d->at : d->at==d->used) { d->merging=false;return AE_DOCUMENT_OK; }
    struct edit *entry=d->history[direction<0 ? d->at-1 : d->at];bool undo=direction<0;
    if (d->state!=(undo ? entry->after_state : entry->before_state)) return AE_DOCUMENT_INVALID;
    size_t remove=undo ? entry->inserted : entry->removed,insert=undo ? entry->removed : entry->inserted;
    enum ae_document_result r=replace(d,entry->begin,entry->begin+remove,entry->data+(undo ? 0 : entry->removed),insert,false,false,false);
    if (r!=AE_DOCUMENT_OK) return r;
    d->cursor=undo ? entry->before_cursor : entry->after_cursor;d->state=undo ? entry->before_state : entry->after_state;
    d->at=undo ? d->at-1 : d->at+1;d->merging=false;*changed=true;return AE_DOCUMENT_OK;
}
bool ae_document_mark_clean(struct ae_document *d,uint64_t revision) {
    if (!d || d->revision!=revision) return false;
    d->clean=d->state;d->merging=false;return true;
}
enum ae_document_result ae_document_find(const struct ae_document *d,const char *query,size_t size,size_t origin,int direction,bool inclusive,bool *found,size_t *position) {
    if (!found || !position) return AE_DOCUMENT_INVALID;
    *found=false;*position=0;
    if (!d || !query || !size || origin>d->size || (direction!=-1 && direction!=1)) return AE_DOCUMENT_INVALID;
    if (size>AE_DOCUMENT_QUERY_BYTES) return AE_DOCUMENT_LIMIT;
    struct statistics s;enum ae_document_result r=statistics(query,size,&s);
    if (r!=AE_DOCUMENT_OK || s.breaks) return AE_DOCUMENT_INVALID;
    size_t prefix[AE_DOCUMENT_QUERY_BYTES]={0};
    for (size_t i=1,j=0;i<size;++i) {
        while (j && query[i]!=query[j]) j=prefix[j-1];
        if (query[i]==query[j]) ++j;
        prefix[i]=j;
    }
    /* Scan each relevant line once. KMP preserves overlapping candidates; two
       monotonic boundary cursors avoid resegmenting a line for every match. */
    for (size_t row=0;row<d->lines;++row) {
        struct ae_document_line line;if (!ae_document_line(d,row,&line)) return AE_DOCUMENT_INVALID;
        if (direction>0 && line.start+line.size<origin) continue;
        if (direction<0 && line.start>origin) break;
        if (line.size<size) continue;
        char text[AE_DOCUMENT_LINE_BYTES];size_t offsets[AE_DOCUMENT_LINE_BYTES+1],count;
        if (!ae_document_copy(d,line.start,line.size,text,sizeof(text)) || !ae_text_graphemes(text,line.size,offsets,AE_DOCUMENT_LINE_BYTES+1,&count)) return AE_DOCUMENT_INVALID;
        size_t left=0,right=0;
        for (size_t i=0,j=0;i<line.size;++i) {
            while (j && text[i]!=query[j]) j=prefix[j-1];
            if (text[i]==query[j]) ++j;
            if (j!=size) continue;
            size_t begin=i+1-size,end=i+1,absolute=line.start+begin;j=prefix[j-1];
            while (left<count && offsets[left]<begin) ++left;
            while (right<count && offsets[right]<end) ++right;
            if (left==count || right==count || offsets[left]!=begin || offsets[right]!=end) continue;
            bool eligible=direction>0 ? absolute>origin || (inclusive && absolute==origin) : absolute<origin || (inclusive && absolute==origin);
            if (!eligible) continue;
            *found=true;*position=absolute;if (direction>0) return AE_DOCUMENT_OK;
        }
    }
    return AE_DOCUMENT_OK;
}
