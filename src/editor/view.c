#include "view.h"
#include "text/grapheme.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
struct ae_editor_view {
    char line[AE_DOCUMENT_LINE_BYTES],*output;size_t points[AE_DOCUMENT_LINE_BYTES+1];
    size_t capacity,used,top;unsigned left,columns;
};
struct ae_editor_view *ae_editor_view_create(void) { return calloc(1,sizeof(struct ae_editor_view)); }
void ae_editor_view_destroy(struct ae_editor_view *v) { if (v) { free(v->output);free(v); } }
static bool emit(struct ae_editor_view *v,const char *text,size_t size) {
    if (size>v->capacity-v->used) return false;
    memcpy(v->output+v->used,text,size);v->used+=size;return true;
}
static bool literal(struct ae_editor_view *v,const char *text) { return emit(v,text,strlen(text)); }
/* Matches the existing terminal's bounded base-plus-three-marks cell profile.
   Unsupported/invisible clusters become visible ASCII; bytes never become CSI. */
static size_t cluster(const char *text,size_t size,char out[256],unsigned *width,unsigned maximum) {
    size_t at=0;unsigned cells=0,marks=0;bool fallback=size>=256;
    while (at<size) {
        uint32_t c;if (!ae_utf8_next(text,size,&at,&c)) return 0;
        int w=wcwidth((wchar_t)c);
        if (w<0 || (!w && (!cells || ++marks>3)) || c==0x200d || (c>=0xfe00 && c<=0xfe0f)) fallback=true;
        if (w>0) { cells+=(unsigned)w;marks=0; }
    }
    if (!fallback && cells && cells<256) {
        if (cells>maximum) { out[0]='?';*width=1;return 1; }
        memcpy(out,text,size);*width=cells;return size;
    }
    uint32_t first;at=0;if (!ae_utf8_next(text,size,&at,&first)) return 0;
    int n=snprintf(out,256,"[U+%04X%s]",(unsigned)first,at<size ? "+..." : "");
    if (n<=0 || n>=256) return 0;
    if ((unsigned)n>maximum) { out[0]='?';*width=1;return 1; }
    *width=(unsigned)n;return (size_t)n;
}
static bool row(struct ae_editor_view *v,unsigned y) {
    char command[32];int n=snprintf(command,sizeof(command),"\033[%u;1H\033[0m\033[2K",y+1);
    return n>0 && (size_t)n<sizeof(command) && emit(v,command,(size_t)n);
}
/* Labels may contain arbitrary pathname bytes. Controls/invalid bytes are
   rendered explicitly. Layout always leaves the last terminal cell unused. */
static bool label(struct ae_editor_view *v,const char *text,unsigned limit,unsigned *painted) {
    size_t size=strlen(text),at=0;unsigned cells=0;
    while (at<size && cells<limit) {
        size_t before=at;uint32_t code;char bytes[32];size_t n;unsigned width;
        if (ae_utf8_next(text,size,&at,&code) && code>=32 && code!=127 && (code<0x80 || wcwidth((wchar_t)code)>0)) {
            width=code<0x80 ? 1u : (unsigned)wcwidth((wchar_t)code);n=at-before;
            if (width>limit-cells) break;
            if (!emit(v,text+before,n)) return false;
        } else {
            at=before+1;int k=snprintf(bytes,sizeof(bytes),"\\x%02X",(unsigned)(unsigned char)text[before]);
            if (k!=4 || limit-cells<4) break;
            n=4;width=4;if (!emit(v,bytes,n)) return false;
        }
        cells+=width;
    }
    if (painted) *painted=cells;
    return true;
}
static bool line_points(struct ae_editor_view *v,const struct ae_document *d,size_t index,struct ae_document_line *line,size_t *count) {
    return ae_document_line(d,index,line) && ae_document_copy(d,line->start,line->size,v->line,sizeof(v->line)) &&
        ae_text_graphemes(v->line,line->size,v->points,AE_DOCUMENT_LINE_BYTES+1,count);
}
static size_t display(struct ae_editor_view *v,size_t i,unsigned logical,char bytes[256],unsigned *width,unsigned maximum) {
    size_t start=v->points[i],size=v->points[i+1]-start;
    if (size==1 && v->line[start]=='\t') { *width=4u-logical%4u;memset(bytes,' ',*width);return *width; }
    return cluster(v->line+start,size,bytes,width,maximum);
}
bool ae_editor_view_render(struct ae_editor_view *v,const struct ae_modal *modal,const struct ae_editor_view_state *s,
        unsigned columns,unsigned rows,struct ae_editor_frame *frame) {
    if (!v || !modal || !s || !frame || columns<2 || columns>AE_EDITOR_VIEW_COLUMNS || rows<2 || rows>AE_EDITOR_VIEW_ROWS) return false;
    *frame=(struct ae_editor_frame){0};size_t need=(size_t)columns*rows*32u+(size_t)rows*128u+2048u;
    if (need>v->capacity) { char *p=malloc(need);if (!p) return false;free(v->output);v->output=p;v->capacity=need; }
    v->columns=columns;v->used=0;
    const struct ae_document *d=ae_modal_document(modal);struct ae_document_info info;size_t current;
    if (!ae_document_info(d,&info) || !ae_document_row(d,info.cursor,&current)) return false;
    bool compact=columns<30 || rows<8;unsigned x=0,y=0,help_max=0;
    if (!literal(v,"\033[?25l")) return false;
    for (unsigned i=0;i<rows;++i) {
        if (!row(v,i)) return false;
        if (compact) {
            if (!i && !label(v,"Enlarge to 30x8",columns-1,NULL)) return false;
            if (i==1 && !label(v,"Editing paused",columns-1,NULL)) return false;
            continue;
        }
        if (!i) {
            const char *title=s->title ? s->title : "[untitled]",*leaf=strrchr(title,'/');leaf=leaf ? leaf+1 : title;
            unsigned painted;
            if (!literal(v,"\033[7m") || !label(v," Aetheri Editor | ",columns-1,&x) || !label(v,leaf,columns-1-x,&painted)) return false;
            x+=painted;
            if (leaf!=title && columns-1-x>3) {
                if (!label(v," | ",columns-1-x,&painted)) return false;
                x+=painted;if (!label(v,title,columns-1-x,NULL)) return false;
            }
            if (!literal(v,"\033[0m")) return false;
        }
    }
    if (!compact) {
        struct ae_modal_selection selection;if (!ae_modal_selection(modal,&selection)) return false;
        unsigned body=rows-3,available=columns-10;
        if (current<v->top) v->top=current;else if (current>=v->top+body) v->top=current-body+1;
        struct ae_document_line line;size_t count;
        if (!line_points(v,d,current,&line,&count)) return false;
        unsigned caret=0;size_t col=0;
        for (size_t i=0;i+1<count && line.start+v->points[i]<info.cursor;++i) {
            char bytes[256];unsigned width;if (!display(v,i,caret,bytes,&width,available)) return false;caret+=width;++col;
        }
        if (caret<v->left) v->left=caret;
        if (caret-v->left>=available) v->left=caret-available/2u;
        unsigned edge=0;size_t skip=0;
        while (skip+1<count && line.start+v->points[skip]<info.cursor) {
            char bytes[256];unsigned width;if (!display(v,skip,edge,bytes,&width,available)) return false;
            if (edge+width>v->left && caret-edge<available) break;
            edge+=width;++skip;
        }
        v->left=edge;
        x=8;y=(unsigned)(current-v->top)+1;
        for (unsigned screen=0;screen<body;++screen) {
            size_t index=v->top+screen;if (!row(v,screen+1)) return false;
            if (index>=info.lines) { if (!literal(v,"     ~")) return false;continue; }
            if (!line_points(v,d,index,&line,&count)) return false;
            char prefix[32];int n=snprintf(prefix,sizeof(prefix),"%5zu  ",index+1);
            if (n<=0 || (size_t)n>=sizeof(prefix) || !emit(v,prefix,(size_t)n)) return false;
            unsigned logical=0,painted=0;size_t start=0;
            while (start+1<count) {
                char bytes[256];unsigned width;if (!display(v,start,logical,bytes,&width,available)) return false;
                if (logical+width>v->left) break;
                logical+=width;++start;
            }
            if (!literal(v,start ? "<" : " ")) return false;
            size_t i=start;
            for (;i+1<count;++i) {
                char bytes[256];unsigned width;size_t bytes_size=display(v,i,logical,bytes,&width,available);
                if (!bytes_size) return false;
                if (index==current && line.start+v->points[i]==info.cursor) x=8+painted;
                if (width>available-painted) break;
                size_t position=line.start+v->points[i];
                bool highlight=(selection.active && position>=selection.begin && position<selection.end) ||
                    (s->highlight_size && s->highlight_size<=info.size-info.cursor && position>=info.cursor && position-info.cursor<s->highlight_size);
                if (highlight && !literal(v,"\033[7m")) return false;
                if (!emit(v,bytes,bytes_size) || (highlight && !literal(v,"\033[0m"))) return false;
                logical+=width;painted+=width;
            }
            if (index==current && info.cursor==line.start+line.size) x=8+painted;
            bool selected_ending=selection.active && ((line.ending_size && selection.begin<=line.start+line.size && selection.end>line.start+line.size) ||
                ((selection.linewise || !line.size) && index>=selection.first_row && index<=selection.last_row));
            if (i+1>=count && selected_ending && painted<available && !literal(v,"\033[7m \033[0m")) return false;
            if (i+1<count && !literal(v,">")) return false;
        }
        enum ae_modal_mode mode=ae_modal_mode(modal);const char *mode_name=mode==AE_MODAL_INSERT ? "INSERT" : mode==AE_MODAL_VISUAL ? "VISUAL" : mode==AE_MODAL_LINES ? "LINES" : "NORMAL";
        char status[256];int n=snprintf(status,sizeof(status)," %s%s | %zu:%zu | %s",s->uncertain ? "SAVE UNKNOWN | " : "",mode_name,
            current+1,col+1,info.dirty ? "modified" : "clean");
        if (n<=0 || (size_t)n>=sizeof(status) || !row(v,rows-2) || !literal(v,"\033[7m") ||
            !label(v,s->dialog && s->command ? s->dialog : status,columns-1,NULL) || !literal(v,"\033[0m")) return false;
        if (!row(v,rows-1)) return false;
        if (s->command) {
            const char *tail=s->command;size_t size=strlen(tail),at=0;
            while (size-at>columns-4u) { uint32_t code;size_t old=at;if (!ae_utf8_next(tail,size,&at,&code)) at=old+1; }
            char prefix[2]={s->command_prefix ? s->command_prefix : ':',0};
            unsigned painted;if (!label(v,at ? "<" : prefix,columns-1,NULL) || !label(v,tail+at,columns-2,&painted)) return false;x=1+painted;y=rows-1;
        }
        else if (!label(v,s->dialog ? s->dialog : s->message && *s->message ? s->message : selection.active ? "y Copy d Cut c Edit Esc Back" : "Ctrl+S Save  Ctrl+Q Close  :help",columns-1,NULL)) return false;
        if (s->dialog && !s->command) { x=0;y=rows-1; }
        if (s->help) {
            static const char *help[]={"Aetheri keyboard help","Normal: h j k l / arrows; 0 $ / Home End","i/a/I/A insert; o/O open line; x delete; dd line","Insert: UTF-8 text; Enter newline; Backspace/Delete","u Undo; Ctrl+R Redo; Ctrl+Shift+Z/Y aliases","v characters; V lines; y copy; d cut; c change","p/P paste from editor register; Esc cancels","/ forward; ? backward literal search; n/N repeat","Search: Enter choose; Esc back; Ctrl+U clear","Esc Normal/cancel; Ctrl+S save; Ctrl+Q close","Commands: :w :wq :q :q! :saveas NEW-PATH",":export NEW-PATH keeps original state; never overwrites","Save UNKNOWN: no retry; export or explicit discard","Ctrl+Z suspends; shell fg resumes; resize retains text","Block selection/panes and accessible API pending"};
            struct { const char *text;size_t size; } wrapped[128];unsigned total=0;
            for (size_t i=0;i<sizeof(help)/sizeof(*help);++i) {
                const char *p=help[i];size_t remaining=strlen(p);
                while (remaining) {
                    size_t chunk=remaining<columns-1u ? remaining : columns-1u;
                    if (chunk<remaining) { size_t space=chunk;while (space && p[space]!=' ') --space;if (space) chunk=space; }
                    if (total==sizeof(wrapped)/sizeof(*wrapped)) return false;
                    wrapped[total].text=p;wrapped[total++].size=chunk;p+=chunk;remaining-=chunk;
                    while (remaining && *p==' ') { ++p;--remaining; }
                }
            }
            help_max=total ? total-1u : 0u;unsigned offset=s->help_offset<total ? s->help_offset : help_max;
            for (unsigned i=0;i<body;++i) {
                if (!row(v,i+1)) return false;
                if (i+offset<total && !emit(v,wrapped[i+offset].text,wrapped[i+offset].size)) return false;
            }
            if (!row(v,rows-1) || !label(v,"Help j/k scroll; Esc Back",columns-1,NULL)) return false;
            x=0;y=1;
        }
    }
    char cursor[64];int n=snprintf(cursor,sizeof(cursor),"\033[%u;%uH\033[?25h",y+1,x+1);
    if (n<=0 || (size_t)n>=sizeof(cursor) || !emit(v,cursor,(size_t)n)) return false;
    *frame=(struct ae_editor_frame){v->output,v->used,x,y,compact,help_max};return true;
}
