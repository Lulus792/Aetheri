#include "nexaui_view.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

enum { SHAPES=512,LABELS=64,LABEL_BYTES=4096,PIXEL_BUDGET=64*1024*1024 };
struct shape {
    uint32_t code,marks[3];uint8_t count,bold,glyphs;bool used;
    nx_cell_glyph placed[NX_CELL_GRID_MAX_GLYPHS];uint64_t age;
};
struct label { char text[LABEL_BYTES+1];nx_text_layout *layout;uint64_t age; };
struct ae_terminal_view {
    nx_context *ui;nx_font *regular,*bold;nx_font_collection *fonts[2];
    nx_cell_view *cells;struct shape shapes[SHAPES];struct label labels[LABELS];
    unsigned char *pixels;size_t capacity;float width,height,scale,points,cw,ch,baseline;
    uint64_t age;bool frame,clip;
};
static nx_color rgb(uint32_t c) { return (nx_color){(float)((c>>16)&255u)/255,(float)((c>>8)&255u)/255,(float)(c&255u)/255,1}; }
static nx_utf8_span span(const char *s) {return (nx_utf8_span){s,strlen(s)};}
static size_t utf8(char *out,uint32_t c) {
    if (!c || c>0x10ffffu || (c>=0xd800u && c<=0xdfffu)) return 0;
    size_t n=c<0x80?1u:c<0x800?2u:c<0x10000?3u:4u;
    if (n==1) out[0]=(char)c;
    else { for (size_t i=n-1;i;i--) {out[i]=(char)(0x80u|(c&63u));c>>=6;}out[0]=(char)((n==2?0xc0u:n==3?0xe0u:0xf0u)|c); }
    return n;
}
static void clear_cache(struct ae_terminal_view *v) {
    memset(v->shapes,0,sizeof(v->shapes));
    for (unsigned i=0;i<LABELS;++i) {nx_text_layout_destroy(v->labels[i].layout);v->labels[i]=(struct label){0};}
}
bool ae_terminal_view_create(const void *regular,size_t regular_size,const void *bold,size_t bold_size,struct ae_terminal_view **out) {
    if (!out || *out || !regular || !bold) return false;
    struct ae_terminal_view *v=calloc(1,sizeof(*v));if (!v) return false;
    v->cells=calloc((size_t)AE_TERM_COLS*AE_TERM_ROWS,sizeof(*v->cells));
    if (!v->cells || nx_font_open(regular,regular_size,NULL,&v->regular)!=NX_OK ||
        nx_font_open(bold,bold_size,NULL,&v->bold)!=NX_OK || nx_context_create(NULL,&v->ui)!=NX_OK) goto failed;
    for (unsigned i=0;i<2;++i) if (nx_font_collection_create(NULL,&v->fonts[i])!=NX_OK ||
        nx_font_collection_add(v->fonts[i],i?v->bold:v->regular)!=NX_OK) goto failed;
    *out=v;return true;
failed: ae_terminal_view_destroy(v);return false;
}
void ae_terminal_view_destroy(struct ae_terminal_view *v) {
    if (!v) return;
    clear_cache(v);nx_context_destroy(v->ui);
    for (unsigned i=0;i<2;++i) nx_font_collection_destroy(v->fonts[i]);
    nx_font_close(v->bold);nx_font_close(v->regular);free(v->pixels);free(v->cells);free(v);
}
nx_context *ae_terminal_view_context(struct ae_terminal_view *v) {return v?v->ui:NULL;}
bool ae_terminal_view_begin(struct ae_terminal_view *v,float w,float h,float scale,float points,float cw,float ch,float baseline) {
    if (!v || v->frame || !isfinite(w) || !isfinite(h) || w<1 || h<1 ||
        !isfinite(scale) || scale<.5f || scale>4 || !isfinite(points) || points<8 || points>64 ||
        !isfinite(cw) || !isfinite(ch) || cw<=0 || ch<=0 || !isfinite(baseline) || baseline<0 || baseline>ch) return false;
    if (v->points!=points) clear_cache(v);
    v->width=w;v->height=h;v->scale=scale;v->points=points;v->cw=cw;v->ch=ch;v->baseline=baseline;
    if (nx_context_set_text_raster_scale(v->ui,scale)!=NX_OK || nx_begin_frame(v->ui,w,h,0)!=NX_OK) return false;
    v->frame=true;v->clip=false;
    nx_semantic_desc root={.id=1,.role=NX_ROLE_WINDOW,.bounds={0,0,w,h},.name=span("Aetheri Terminal")};
    return nx_semantics_add(v->ui,&root,NULL)==NX_OK;
}
bool ae_terminal_view_fill(struct ae_terminal_view *v,nx_rect r,uint32_t color) {
    return v && v->frame && nx_fill_rect(v->ui,r,rgb(color))==NX_OK;
}
bool ae_terminal_view_clip(struct ae_terminal_view *v,const nx_rect *r) {
    if (!v || !v->frame) return false;
    if (v->clip) {if (nx_pop_clip(v->ui)!=NX_OK) return false;v->clip=false;}
    if (r) {if (nx_push_clip(v->ui,*r)!=NX_OK) return false;v->clip=true;}
    return true;
}
bool ae_terminal_view_label(struct ae_terminal_view *v,const char *text,float x,float y,uint32_t color) {
    if (!v || !v->frame || !text || strlen(text)>LABEL_BYTES) return false;
    struct label *found=NULL,*oldest=&v->labels[0];
    for (unsigned i=0;i<LABELS;++i) {
        struct label *l=&v->labels[i];
        if (l->layout && !strcmp(l->text,text)) {found=l;break;}
        if (!l->layout || l->age<oldest->age) oldest=l;
    }
    if (!found) {
        nx_text_layout *layout=NULL;
        if (nx_text_layout_create(v->fonts[0],text,strlen(text),NX_BIDI_LTR,v->points,16384,&layout)!=NX_OK) return false;
        /* Frame commands borrow the layout's fonts/masks, not the layout. */
        nx_text_layout_destroy(oldest->layout);oldest->layout=layout;memcpy(oldest->text,text,strlen(text)+1);found=oldest;
    }
    found->age=++v->age;
    return nx_draw_text_layout(v->ui,found->layout,(nx_point){x,y},rgb(color),1024u*1024u)==NX_OK;
}
static bool glyphs(struct ae_terminal_view *v,const struct ae_term_cell *c,nx_cell_view *out) {
    unsigned bold=(c->style&AE_TERM_BOLD)?1u:0u;struct shape *found=NULL,*oldest=&v->shapes[0];
    for (unsigned i=0;i<SHAPES;++i) {
        struct shape *s=&v->shapes[i];
        if (s->used && s->code==c->code && s->count==c->count && s->bold==bold && !memcmp(s->marks,c->marks,sizeof(s->marks))) {found=s;break;}
        if (!s->used || s->age<oldest->age) oldest=s;
    }
    if (!found) {
        char text[16];size_t n=utf8(text,c->code);if (!n) return false;
        for (unsigned i=0;i<c->count;++i) {size_t added=utf8(text+n,c->marks[i]);if (!added) return false;n+=added;}
        nx_shaped_glyph shaped[4];nx_shape_run runs[4];size_t count=0,run_count=0;uint8_t paragraph=0;
        if (nx_shape_text(v->fonts[bold],text,n,NX_BIDI_LTR,v->points,shaped,4,runs,4,&count,&run_count,&paragraph)!=NX_OK || count>4) return false;
        struct shape next={.code=c->code,.count=c->count,.bold=(uint8_t)bold,.glyphs=(uint8_t)count,.used=true};
        memcpy(next.marks,c->marks,sizeof(next.marks));double pen=0;
        for (size_t i=0;i<count;++i) {
            next.placed[i]=(nx_cell_glyph){shaped[i].font,shaped[i].glyph_index,(float)(pen+shaped[i].offset_x),(float)-shaped[i].offset_y};
            pen+=shaped[i].advance_x;
        }
        *oldest=next;found=oldest;
    }
    found->age=++v->age;out->glyph_count=found->glyphs;memcpy(out->glyphs,found->placed,sizeof(out->glyphs));return true;
}
static bool row_text(struct ae_terminal_view *v,const struct ae_terminal *m,unsigned y,nx_rect bounds,bool live_caret) {
    /* One complete visible row per snapshot avoids the SDK's 65536-byte total
       per text object without truncating a large Unicode grid. History remains
       in the original model, not in a second accessibility document. */
    char text[AE_TERM_COLS*16];size_t used=0,offsets[AE_TERM_COLS+1];
    const struct ae_term_cell *row=ae_terminal_row(m,y);
    for (unsigned x=0;x<m->cols;++x) {
        offsets[x]=used;if (!row[x].width) continue;
        size_t n=utf8(text+used,row[x].code);if (!n) return false;used+=n;
        for (unsigned i=0;i<row[x].count;++i) {n=utf8(text+used,row[x].marks[i]);if (!n) return false;used+=n;}
    }
    offsets[m->cols]=used;
    unsigned offset=m->alt?0:m->view;
    uint64_t token=y<offset?m->history_rows[(m->history_head+m->history_count-offset+y)%AE_TERM_HISTORY].token:m->screen_rows[y-offset].token;
    if (!token) return false;
    nx_semantic_desc sem={.id=1000u+y,.parent_id=2,.role=NX_ROLE_TEXT,
        .bounds={bounds.x,bounds.y+(float)y*v->ch,bounds.width,v->ch},.name=span("Terminal output row"),
        .value={text,used},.state=NX_SEM_READONLY};
    /* A per-row projection does not invent a caret at a clipped selection edge.
       Only the live input row with no selection has its actual cursor here. */
    if (live_caret && !m->view && !m->selection.count && m->cursor && y==m->y && m->x<m->cols)
        return nx_semantics_add_readonly_text_snapshot(v->ui,&sem,token,offsets[m->x],offsets[m->x],NULL)==NX_OK;
    return nx_semantics_add_readonly_text_snapshot_without_caret(v->ui,&sem,token,NULL)==NX_OK;
}
bool ae_terminal_view_grid(struct ae_terminal_view *v,const struct ae_terminal *m,nx_rect bounds,bool contrast,bool live_caret) {
    if (!v || !v->frame || !m || m->cols<2 || m->cols>AE_TERM_COLS || m->rows<2 || m->rows>AE_TERM_ROWS ||
        m->history_count>AE_TERM_HISTORY || m->history_head>=AE_TERM_HISTORY || (!m->alt && m->view>m->history_count)) return false;
    nx_semantic_desc group={.id=2,.parent_id=1,.role=NX_ROLE_GROUP,.bounds=bounds,.name=span("Terminal output")};
    if (nx_semantics_add(v->ui,&group,NULL)!=NX_OK) return false;
    for (unsigned y=0;y<m->rows;++y) {
        const struct ae_term_cell *row=ae_terminal_row(m,y);if (!row) return false;
        unsigned first=0,last=0;bool selected=ae_terminal_selected_range(m,y,&first,&last);
        for (unsigned x=0;x<m->cols;++x) {
            const struct ae_term_cell *c=&row[x];if (c->width>2 || c->count>3 || c->fg>0xffffffu || c->bg>0xffffffu ||
                (c->style&~(AE_TERM_BOLD|AE_TERM_UNDERLINE|AE_TERM_REVERSE))) return false;
            nx_cell_view *target=&v->cells[(size_t)y*m->cols+x];*target=(nx_cell_view){.span=c->width};
            uint32_t fg=c->fg,bg=c->bg;if (c->style&AE_TERM_REVERSE) {uint32_t tmp=fg;fg=bg;bg=tmp;}
            if (contrast && bg==0x10141c) {bg=0;if (fg==0xdce4ef) fg=0xffffff;}
            if (selected && x>=first && x<last) {fg=0xffffff;bg=0x304e78;}
            target->foreground=rgb(fg);target->background=rgb(bg);if (c->style&AE_TERM_UNDERLINE) target->flags|=NX_CELL_UNDERLINE;
            if (c->width && c->code!=' ' && !glyphs(v,c,target)) return false;
        }
        if (!row_text(v,m,y,bounds,live_caret)) return false;
    }
    nx_cell_grid_desc grid={.bounds=bounds,.cell_width=v->cw,.cell_height=v->ch,.baseline=v->baseline,.pixels_per_em=v->points,
        .cells=v->cells,.rows=m->rows,.columns=m->cols,.row_stride=m->cols,.cell_capacity=(size_t)m->rows*m->cols,
        .glyph_cache_bytes=1024u*1024u,.mask_budget_bytes=NX_CELL_GRID_MAX_MASK_BYTES};
    return nx_draw_cell_grid(v->ui,&grid)==NX_OK;
}
bool ae_terminal_view_finish(struct ae_terminal_view *v,unsigned w,unsigned h,const unsigned char **pixels,size_t *stride) {
    if (!v || !v->frame || !pixels || !stride || !w || !h || w>8192 || h>8192 ||
        (size_t)w*h>PIXEL_BUDGET/4 || fabs((double)w/v->width-(double)h/v->height)>.01) return false;
    if (!ae_terminal_view_clip(v,NULL) || nx_end_frame(v->ui)!=NX_OK) return false;
    v->frame=false;
    size_t bytes=(size_t)w*h*4;
    if (bytes>v->capacity) {unsigned char *p=realloc(v->pixels,bytes);if (!p) return false;v->pixels=p;v->capacity=bytes;}
    const nx_command *commands=NULL;size_t count=0;
    if (nx_frame_commands(v->ui,&commands,&count)!=NX_OK ||
        nx_headless_render_scaled(commands,count,v->pixels,w,h,(size_t)w*4,rgb(0x10141c),(double)w/v->width)!=NX_OK) return false;
    *pixels=v->pixels;*stride=(size_t)w*4;return true;
}
