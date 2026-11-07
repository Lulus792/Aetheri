#include "nexaui_view.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

enum { SHAPES=512,LABELS=64,LABEL_BYTES=4096,PIXEL_BUDGET=64*1024*1024 };
struct shape {
    uint32_t code,marks[3];uint8_t count,bold,glyphs;bool used;
    nx_cell_glyph placed[NX_CELL_GRID_MAX_GLYPHS];uint64_t age;
};
struct label { char text[LABEL_BYTES+1];nx_text_layout *layout;uint64_t age;float points; };
struct projection {
    uint64_t key,group,tokens[AE_TERM_ROWS],ids[AE_TERM_ROWS],selection_serial;
    const struct ae_terminal *model;unsigned rows,cols;bool seen,alt,selected;
};
struct reading_request {
    const struct ae_terminal *model;uint64_t pane,source_id,source_token,target,top,selection_serial;
    unsigned rows,cols,view;bool alt,selected;
};
struct ae_terminal_view {
    nx_context *ui;nx_font *regular,*bold;nx_font_collection *fonts[2];
    nx_cell_view *cells;struct shape shapes[SHAPES];struct label labels[LABELS];
    unsigned char *pixels;size_t capacity;float width,height,scale,points,cw,ch,baseline;
    uint64_t age;bool frame,clip;
    uint64_t row_ids[AE_TERM_ROWS],row_tokens[AE_TERM_ROWS],id_offset,highest_id,selection_serial;unsigned row_count,model_rows,model_cols;
    const struct ae_terminal *model;bool model_alt,model_selected;struct reading_request reading;
    struct projection panes[16];uint64_t named_serial;unsigned named_calls;bool named_mode,legacy_mode;
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
bool ae_terminal_view_focus_row(struct ae_terminal_view *v,unsigned row) {
    if(!v || v->frame || row>=v->row_count) return false;
    v->reading=(struct reading_request){0};return nx_focus_set(v->ui,v->row_ids[row])==NX_OK;
}
void ae_terminal_view_clear_reading(struct ae_terminal_view *v) {
    if(!v) return;
    v->reading=(struct reading_request){0};(void)nx_focus_set(v->ui,0);
}
bool ae_terminal_view_reading(const struct ae_terminal_view *v) {
    if (!v) return false;
    uint64_t id=nx_focus_id(v->ui);
    for (unsigned i=0;id && i<v->row_count;++i) if (v->row_ids[i]==id) return true;
    for(unsigned p=0;id && p<16;++p) if(v->panes[p].key && v->panes[p].seen)
        for(unsigned i=0;i<v->panes[p].rows;++i) if(v->panes[p].ids[i]==id) return true;
    return false;
}
bool ae_terminal_view_focus_pane_row(struct ae_terminal_view *v,uint64_t pane,unsigned row) {
    if(!v || v->frame || !v->named_mode) return false;
    for(unsigned p=0;p<16;++p) if(v->panes[p].key==pane && v->panes[p].seen && row<v->panes[p].rows) {
        v->reading=(struct reading_request){0};return nx_focus_set(v->ui,v->panes[p].ids[row])==NX_OK;
    }
    return false;
}
static bool readable_model(const struct ae_terminal *m) {
    return m && m->rows>=2 && m->rows<=AE_TERM_ROWS && m->cols>=2 && m->cols<=AE_TERM_COLS &&
        m->history_count<=AE_TERM_HISTORY && m->history_head<AE_TERM_HISTORY && m->view<=m->history_count;
}
static uint64_t line_token(const struct ae_terminal *m,unsigned line) {
    unsigned history=m->alt?0:m->history_count;
    return line<history?m->history_rows[(m->history_head+line)%AE_TERM_HISTORY].token:m->screen_rows[line-history].token;
}
static unsigned token_line(const struct ae_terminal *m,uint64_t token) {
    unsigned count=(m->alt?0:m->history_count)+m->rows;
    if(token) for(unsigned i=0;i<count;++i) if(line_token(m,i)==token) return i;
    return count;
}
static bool reading_context(const struct ae_terminal_view *v,const struct ae_terminal *m,uint64_t pane,struct reading_request *out) {
    if(!v || v->frame || !readable_model(m)) return false;
    uint64_t focus=nx_focus_id(v->ui);if(!focus) return false;
    struct reading_request request=v->reading;
    if(request.model && request.source_id!=focus) return false;
    if(!request.model) {
        const uint64_t *ids=NULL,*tokens=NULL;unsigned rows=0,cols=0;bool alt=false,selected=false;uint64_t serial=0;
        if(!pane && v->legacy_mode && v->model==m) {
            ids=v->row_ids;tokens=v->row_tokens;rows=v->row_count;cols=v->model_cols;alt=v->model_alt;selected=v->model_selected;serial=v->selection_serial;
        } else if(pane && v->named_mode) for(unsigned i=0;i<16;++i) {
            const struct projection *p=&v->panes[i];
            if(p->key==pane && p->seen && p->model==m) {ids=p->ids;tokens=p->tokens;rows=p->rows;cols=p->cols;alt=p->alt;selected=p->selected;serial=p->selection_serial;break;}
        }
        for(unsigned i=0;i<rows;++i) if(ids[i]==focus) {
            request=(struct reading_request){.model=m,.pane=pane,.source_id=focus,.source_token=tokens[i],.target=tokens[i],
                .rows=rows,.cols=cols,.view=m->view,.top=line_token(m,(m->alt?0:m->history_count-m->view)),.alt=alt,.selected=selected,.selection_serial=serial};break;
        }
    }
    if(!request.model) return false;
    unsigned history=m->alt?0:m->history_count,count=history+m->rows,start=history-(m->alt?0:m->view);
    unsigned at=token_line(m,request.target);
    if(request.model!=m || request.pane!=pane || request.source_id!=focus || request.rows!=m->rows || request.cols!=m->cols ||
        request.alt!=m->alt || (request.view!=m->view && request.top!=line_token(m,start)) ||
        token_line(m,request.source_token)==count || at<start || at>=start+m->rows) {
        return false;
    }
    request.selected=request.selected || m->selection.count!=0 || request.selection_serial!=m->selection_serial || m->selection_serial==UINT64_MAX;
    *out=request;return true;
}
bool ae_terminal_view_read_target(const struct ae_terminal_view *v,const struct ae_terminal *m,uint64_t pane,
    uint64_t *token,bool *had_selection) {
    if(!token) return false;
    *token=0;if(had_selection) *had_selection=false;
    struct reading_request request;
    if(!reading_context(v,m,pane,&request)) return false;
    *token=request.target;if(had_selection) *had_selection=request.selected;return true;
}
bool ae_terminal_view_read_move(struct ae_terminal_view *v,const struct ae_terminal *m,uint64_t pane,
    enum ae_terminal_read_move move,int *scroll) {
    if(!scroll) return false;
    *scroll=0;
    if(!v || v->frame || !readable_model(m) || move<AE_TERM_READ_UP || move>AE_TERM_READ_LATEST) return false;
    struct reading_request request;
    if(!reading_context(v,m,pane,&request)) {
        if(v->reading.model && v->reading.source_id!=nx_focus_id(v->ui)) v->reading=(struct reading_request){0};
        else if(ae_terminal_view_reading(v)) ae_terminal_view_clear_reading(v);
        return false;
    }
    unsigned history=m->alt?0:m->history_count,count=history+m->rows,start=history-(m->alt?0:m->view),at=token_line(m,request.target);
    unsigned next=at,top=start,page=m->rows-1;
    switch(move) {
    case AE_TERM_READ_UP: if(next) --next;break;
    case AE_TERM_READ_DOWN: if(next+1<count) ++next;break;
    case AE_TERM_READ_FIRST: next=start;break;
    case AE_TERM_READ_LAST: next=start+m->rows-1;break;
    case AE_TERM_READ_PAGE_UP: next=page>next?0:next-page;top=page>top?0:top-page;break;
    case AE_TERM_READ_PAGE_DOWN: next=page>count-1-next?count-1:next+page;top=page>history-top?history:top+page;break;
    case AE_TERM_READ_OLDEST: next=0;top=0;break;
    case AE_TERM_READ_LATEST: next=count-1;top=history;break;
    }
    if(next<top) top=next;
    else if(next>=top+m->rows) top=next-m->rows+1;
    request.target=line_token(m,next);request.top=line_token(m,top);request.view=m->alt?m->view:history-top;v->reading=request;
    *scroll=(int)start-(int)top;return true;
}
static void apply_reading(struct ae_terminal_view *v,const struct ae_terminal *m,const struct projection *pane,bool enabled) {
    struct reading_request request=v->reading;
    if(!request.model || request.pane!=(pane?pane->key:0)) return;
    v->reading=(struct reading_request){0};
    if(nx_focus_id(v->ui)!=request.source_id) return;
    if(enabled && request.model==m && request.rows==m->rows && request.cols==m->cols && request.alt==m->alt &&
        (request.view==m->view || request.top==line_token(m,m->alt?0:m->history_count-m->view)) &&
        token_line(m,request.source_token)<(m->alt?0:m->history_count)+m->rows) {
        for(unsigned y=0;y<m->rows;++y) if((pane?pane->tokens[y]:v->row_tokens[y])==request.target) {
            if(nx_focus_set(v->ui,pane?pane->ids[y]:v->row_ids[y])==NX_OK) return;
            break;
        }
    }
    (void)nx_focus_set(v->ui,0);
}
bool ae_terminal_view_begin(struct ae_terminal_view *v,float w,float h,float scale,float points,float cw,float ch,float baseline) {
    if (!v || v->frame || !isfinite(w) || !isfinite(h) || w<1 || h<1 ||
        !isfinite(scale) || scale<.5f || scale>4 || !isfinite(points) || points<8 || points>64 ||
        !isfinite(cw) || !isfinite(ch) || cw<=0 || ch<=0 || !isfinite(baseline) || baseline<0 || baseline>ch) return false;
    if (v->points!=points) clear_cache(v);
    v->width=w;v->height=h;v->scale=scale;v->points=points;v->cw=cw;v->ch=ch;v->baseline=baseline;
    if (nx_context_set_text_raster_scale(v->ui,scale)!=NX_OK || nx_begin_frame(v->ui,w,h,0)!=NX_OK) return false;
    v->frame=true;v->clip=false;
    v->named_calls=0;for(unsigned i=0;i<16;++i) v->panes[i].seen=false;
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
static bool label_size(struct ae_terminal_view *v,const char *text,float x,float y,float points,bool middle,uint32_t color) {
    if (!v || !v->frame || !text || strlen(text)>LABEL_BYTES) return false;
    struct label *found=NULL,*oldest=&v->labels[0];
    for (unsigned i=0;i<LABELS;++i) {
        struct label *l=&v->labels[i];
        if (l->layout && l->points==points && !strcmp(l->text,text)) {found=l;break;}
        if (!l->layout || l->age<oldest->age) oldest=l;
    }
    if (!found) {
        nx_text_layout *layout=NULL;
        if (nx_text_layout_create(v->fonts[0],text,strlen(text),NX_BIDI_LTR,points,16384,&layout)!=NX_OK) return false;
        /* Frame commands borrow the layout's fonts/masks, not the layout. */
        nx_text_layout_destroy(oldest->layout);oldest->layout=layout;oldest->points=points;memcpy(oldest->text,text,strlen(text)+1);found=oldest;
    }
    found->age=++v->age;
    if (middle) y-=(float)nx_text_layout_height(found->layout)/2;
    return nx_draw_text_layout(v->ui,found->layout,(nx_point){x,y},rgb(color),1024u*1024u)==NX_OK;
}
bool ae_terminal_view_label(struct ae_terminal_view *v,const char *text,float x,float y,uint32_t color) {
    return v && label_size(v,text,x,y,v->points,false,color);
}
bool ae_terminal_view_group(struct ae_terminal_view *v,uint64_t id,nx_rect bounds,const char *name,bool tabs) {
    if(!v || !v->frame || !name) return false;
    nx_semantic_desc group={.id=id,.parent_id=1,.role=tabs?NX_ROLE_TAB_LIST:NX_ROLE_GROUP,.bounds=bounds,.name=span(name)};
    return nx_semantics_add(v->ui,&group,NULL)==NX_OK;
}
bool ae_terminal_view_tabs(struct ae_terminal_view *v,uint64_t id,nx_rect bounds,const char *name,bool disabled,uint64_t *selected) {
    if(!v || !v->frame || !name || !selected || !*selected) return false;
    nx_semantic_desc group={.id=id,.parent_id=1,.role=NX_ROLE_TAB_LIST,.bounds=bounds,.name=span(name),
        .state=disabled?NX_SEM_DISABLED:0u};
    return nx_selection_group(v->ui,&group,1,selected)==NX_OK;
}
bool ae_terminal_view_control(struct ae_terminal_view *v,const struct ae_terminal_view_control *s,bool *activated) {
    if(!activated) return false;
    *activated=false;if(!v || !v->frame || !s || !s->id || !s->name || !s->label) return false;
    nx_widget_desc w={.semantic={.id=s->id,.parent_id=s->parent,.role=s->tab?NX_ROLE_TAB:NX_ROLE_BUTTON,.bounds=s->bounds,.name=span(s->name),
        .state=(s->disabled?NX_SEM_DISABLED:0u)|(s->selected?NX_SEM_SELECTED:0u),
        /* SELECT also declares membership. Preserve it while disabled; the
           group's/current widget state rejects requests without losing the
           one required selected member during menus and orderly close. */
        .actions=s->tab?NX_ACTION_SELECT|NX_ACTION_FOCUS:s->disabled?0u:NX_ACTION_PRESS},
        .flags=NX_WIDGET_INTERACTIVE|(s->tab?NX_WIDGET_FOCUSABLE:0u)};
    nx_interaction_state state;if(nx_widget_register(v->ui,&w,&state)!=NX_OK) return false;
    uint32_t background=s->contrast?0:s->selected?0x2d3545:s->pressed?0x363f52:s->hovered?0x242c3b:0x191e28;
    if(nx_fill_round_rect(v->ui,s->bounds,4,rgb(background))!=NX_OK) return false;
    if(s->selected && nx_fill_rect(v->ui,(nx_rect){s->bounds.x+4,s->bounds.y+s->bounds.height-2,s->bounds.width-8,2},rgb(s->contrast?0xffffff:0x8faaff))!=NX_OK) return false;
    if(nx_push_clip(v->ui,s->bounds)!=NX_OK) return false;
    bool okay=label_size(v,s->label,s->bounds.x+8,s->bounds.y+s->bounds.height/2,11*v->points/16,true,
        s->disabled?0x7d8592:s->contrast?0xffffff:0xe5e9f1);
    if(nx_pop_clip(v->ui)!=NX_OK) return false;
    *activated=state.clicked && !s->disabled;return okay;
}
bool ae_terminal_chrome_layout(float width,float height,float scale,struct ae_terminal_chrome *out) {
    if (!out) return false;
    *out=(struct ae_terminal_chrome){0};
    if (!isfinite(width) || !isfinite(height) || !isfinite(scale) || width<200 || height<64 || scale<.5f || scale>4) return false;
    float header=fmaxf(32,12+13*scale),top=(header-26)/2;
    *out=(struct ae_terminal_chrome){.bar={0,0,width,header},.title={92,0,width-104,header},
        .buttons={{8,top,22,26},{30,top,22,26},{52,top,22,26}}};
    return true;
}
unsigned ae_terminal_chrome_target(const struct ae_terminal_chrome *c,float x,float y) {
    if (!c || !isfinite(x) || !isfinite(y)) return AE_TERM_CHROME_NONE;
    for (unsigned i=0;i<3;++i) {nx_rect r=c->buttons[i];if (x>=r.x && y>=r.y && x<r.x+r.width && y<r.y+r.height) return i+1;}
    return AE_TERM_CHROME_NONE;
}
bool ae_terminal_view_chrome(struct ae_terminal_view *v,const struct ae_terminal_chrome_state *s,unsigned *request) {
    if (!request) return false;
    *request=AE_TERM_CHROME_NONE;struct ae_terminal_chrome c;
    if (!v || !v->frame || !s || !ae_terminal_chrome_layout(v->width,v->height,v->points/16,&c)) return false;
    if (nx_fill_rect(v->ui,c.bar,rgb(s->contrast?0:0x1c1f28))!=NX_OK ||
        nx_fill_rect(v->ui,(nx_rect){0,c.bar.height-1,v->width,1},rgb(s->contrast?0xffffff:0x343b48))!=NX_OK) return false;
    const char *names[3]={s->german?"Fenster schließen":"Close window",
        s->german?"Minimieren nicht verfügbar":"Minimize unavailable",
        s->maximized?(s->german?"Fenster wiederherstellen":"Restore window"):(s->german?"Fenster vergrößern":"Zoom window")};
    for (unsigned i=0;i<3;++i) {
        unsigned action=i+1;bool disabled=action==AE_TERM_CHROME_MINIMIZE || (action==AE_TERM_CHROME_ZOOM && s->zoom_disabled);
        nx_widget_desc w={.semantic={.id=i+3u,.parent_id=1,.role=NX_ROLE_BUTTON,.bounds=c.buttons[i],.name=span(names[i]),
            .state=disabled?NX_SEM_DISABLED:0u,.actions=disabled?0u:NX_ACTION_PRESS},.flags=NX_WIDGET_INTERACTIVE};
        nx_interaction_state interaction;if (nx_widget_register(v->ui,&w,&interaction)!=NX_OK) return false;
        if (interaction.clicked && !disabled && !*request) *request=action;
        bool hover=s->hovered==action && !disabled,held=s->pressed==action && hover;
        nx_rect r=c.buttons[i];float x=r.x+r.width/2,y=r.y+r.height/2;
        if ((hover || held) && nx_fill_round_rect(v->ui,r,5,rgb(held?0x3b4353:0x333946))!=NX_OK) return false;
        uint32_t dot=disabled?0x747b87:s->contrast?0xffffff:action==AE_TERM_CHROME_CLOSE?0xb97e7d:0x829e8b;
        if (nx_fill_round_rect(v->ui,(nx_rect){x-6,y-6,12,12},6,rgb(dot))!=NX_OK) return false;
        if (!hover && !held && !s->contrast) continue;
        nx_color ink=rgb(0x20242c);
        if (action==AE_TERM_CHROME_CLOSE) {
            if (nx_draw_line(v->ui,(nx_point){x-2,y-2},(nx_point){x+2,y+2},1,ink)!=NX_OK ||
                nx_draw_line(v->ui,(nx_point){x-2,y+2},(nx_point){x+2,y-2},1,ink)!=NX_OK) return false;
        } else if (action==AE_TERM_CHROME_ZOOM && nx_stroke_rect(v->ui,(nx_rect){x-2,y-2,4,4},1,ink)!=NX_OK) return false;
    }
    const char *title=s->editor?(s->german?"Terminaleditor":"Terminal editor"):"Terminal";
    if (nx_push_clip(v->ui,c.title)!=NX_OK) return false;
    bool okay=label_size(v,title,c.title.x,c.bar.height/2,11*v->points/16,true,s->contrast?0xffffff:0xe8eaf0);
    return nx_pop_clip(v->ui)==NX_OK && okay;
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
static uint64_t next_named(struct ae_terminal_view *v) {
    if(v->named_serial<1000) v->named_serial=1000;
    return v->named_serial<(UINT64_C(1)<<60)-1?++v->named_serial:0;
}
static bool row_text(struct ae_terminal_view *v,const struct ae_terminal *m,unsigned y,nx_rect bounds,bool live_caret,bool focusable,bool enabled,
    struct projection *pane,const uint64_t *old_tokens,const uint64_t *old_ids) {
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
    if (!token || (!pane && token>UINT64_MAX-v->id_offset)) return false;
    /* Position is not identity: movement retains the original row, while any
       edit/replacement retires its native node and any pending focus target. */
    uint64_t id=0;
    if(pane) {
        for(unsigned i=0;i<AE_TERM_ROWS;++i) if(old_tokens[i]==token) {id=old_ids[i];break;}
        if(!id) id=next_named(v);
        if(!id) return false;
        pane->tokens[y]=token;pane->ids[y]=id;
    } else {id=token+v->id_offset;if (id>v->highest_id) v->highest_id=id;v->row_ids[y]=id;v->row_tokens[y]=token;v->row_count=y+1;}
    nx_semantic_desc sem={.id=id,.parent_id=pane?pane->group:2,.role=NX_ROLE_TEXT,
        .bounds={bounds.x,bounds.y+(float)y*v->ch,bounds.width,v->ch},.name=span("Terminal output row"),
        .value={text,used},.state=NX_SEM_READONLY|(enabled?0u:NX_SEM_DISABLED),.actions=focusable?NX_ACTION_FOCUS:0u};
    nx_interaction_state interaction;
    if (nx_interact_region(v->ui,id,sem.bounds,focusable?NX_REGION_FOCUSABLE:NX_REGION_DISABLED,&interaction)!=NX_OK) return false;
    /* A per-row projection does not invent a caret at a clipped selection edge.
       Only the live input row with no selection has its actual cursor here. */
    if (live_caret && !m->view && !m->selection.count && m->cursor && y==m->y && m->x<m->cols)
        return nx_semantics_add_readonly_text_snapshot(v->ui,&sem,token,offsets[m->x],offsets[m->x],NULL)==NX_OK;
    return nx_semantics_add_readonly_text_snapshot_without_caret(v->ui,&sem,token,NULL)==NX_OK;
}
static bool grid(struct ae_terminal_view *v,const struct ae_terminal *m,nx_rect bounds,bool contrast,bool live_caret,bool read_focus,bool enabled,struct projection *pane,const char *name,const char *description) {
    if (!v || !v->frame || !m || m->cols<2 || m->cols>AE_TERM_COLS || m->rows<2 || m->rows>AE_TERM_ROWS ||
        m->history_count>AE_TERM_HISTORY || m->history_head>=AE_TERM_HISTORY || (!m->alt && m->view>m->history_count)) return false;
    /* Resize changes the projected row, even when the original cell model keeps
       its content token. Never reuse an earlier projection's semantic IDs when
       dimensions change back. Ordinary movement keeps the current epoch. */
    if (!pane && v->model_rows && (v->model_rows!=m->rows || v->model_cols!=m->cols || v->model_alt!=m->alt)) {
        if (v->highest_id==UINT64_MAX) return false;
        v->id_offset=v->highest_id+1;
    }
    uint64_t old_tokens[AE_TERM_ROWS]={0},old_ids[AE_TERM_ROWS]={0};
    if(pane) {
        if(pane->rows==m->rows && pane->cols==m->cols && pane->alt==m->alt) {memcpy(old_tokens,pane->tokens,sizeof(old_tokens));memcpy(old_ids,pane->ids,sizeof(old_ids));}
        memset(pane->tokens,0,sizeof(pane->tokens));memset(pane->ids,0,sizeof(pane->ids));pane->rows=m->rows;pane->cols=m->cols;pane->alt=m->alt;pane->selected=m->selection.count!=0;pane->selection_serial=m->selection_serial;
    } else {
        if (!v->id_offset) v->id_offset=UINT64_C(1000);
        if(v->model && v->model!=m) return false;
        v->model=m;v->model_rows=m->rows;v->model_cols=m->cols;v->model_alt=m->alt;v->model_selected=m->selection.count!=0;v->selection_serial=m->selection_serial;v->row_count=0;
    }
    nx_semantic_desc group={.id=pane?pane->group:2,.parent_id=1,.role=NX_ROLE_GROUP,.bounds=bounds,.name=span(name),
        .description=span(description?description:"")};
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
        if (!row_text(v,m,y,bounds,live_caret,read_focus,enabled,pane,old_tokens,old_ids)) return false;
    }
    apply_reading(v,m,pane,read_focus);
    nx_cell_grid_desc grid={.bounds=bounds,.cell_width=v->cw,.cell_height=v->ch,.baseline=v->baseline,.pixels_per_em=v->points,
        .cells=v->cells,.rows=m->rows,.columns=m->cols,.row_stride=m->cols,.cell_capacity=(size_t)m->rows*m->cols,
        .glyph_cache_bytes=1024u*1024u,.mask_budget_bytes=NX_CELL_GRID_MAX_MASK_BYTES};
    if (nx_draw_cell_grid(v->ui,&grid)!=NX_OK) return false;
    if (read_focus) for (unsigned y=0;y<m->rows;++y) if (nx_focus_id(v->ui)==(pane?pane->ids[y]:v->row_ids[y]))
        return nx_stroke_rect(v->ui,(nx_rect){bounds.x,bounds.y+(float)y*v->ch,bounds.width,v->ch},1,rgb(0x7ca8e8))==NX_OK;
    return true;
}
bool ae_terminal_view_grid(struct ae_terminal_view *v,const struct ae_terminal *m,nx_rect bounds,bool contrast,bool live_caret,bool read_focus) {
    if(!v || v->named_mode) return false;
    v->legacy_mode=true;return grid(v,m,bounds,contrast,live_caret,read_focus,read_focus,NULL,"Terminal output",NULL);
}
bool ae_terminal_view_pane(struct ae_terminal_view *v,const struct ae_terminal_pane_view *s) {
    if(!v || !v->frame || !s || !s->id || !s->model || !s->name || strlen(s->name)>LABEL_BYTES ||
        (s->description && strlen(s->description)>LABEL_BYTES) || v->legacy_mode || v->named_calls>=8) return false;
    struct projection *p=NULL,*empty=NULL;
    for(unsigned i=0;i<16;++i) {if(v->panes[i].key==s->id) p=&v->panes[i];else if(!v->panes[i].key && !empty) empty=&v->panes[i];}
    if(p && (p->seen || p->model!=s->model)) return false;
    if(!p) {if(!empty) return false;uint64_t group=next_named(v);if(!group) return false;p=empty;*p=(struct projection){.key=s->id,.group=group,.model=s->model};}
    v->named_mode=true;++v->named_calls;p->seen=true;
    return grid(v,s->model,s->bounds,s->contrast,s->caret,s->enabled && s->focusable,s->enabled,p,s->name,s->description);
}
bool ae_terminal_view_finish(struct ae_terminal_view *v,unsigned w,unsigned h,const unsigned char **pixels,size_t *stride) {
    if (!v || !v->frame || !pixels || !stride || !w || !h || w>8192 || h>8192 ||
        (size_t)w*h>PIXEL_BUDGET/4 || fabs((double)w/v->width-(double)h/v->height)>.01) return false;
    if(v->reading.model) ae_terminal_view_clear_reading(v);
    if (!ae_terminal_view_clip(v,NULL) || nx_end_frame(v->ui)!=NX_OK) return false;
    v->frame=false;
    for(unsigned i=0;i<16;++i) if(!v->panes[i].seen) v->panes[i]=(struct projection){0};
    size_t bytes=(size_t)w*h*4;
    if (bytes>v->capacity) {unsigned char *p=realloc(v->pixels,bytes);if (!p) return false;v->pixels=p;v->capacity=bytes;}
    const nx_command *commands=NULL;size_t count=0;
    if (nx_frame_commands(v->ui,&commands,&count)!=NX_OK ||
        nx_headless_render_scaled(commands,count,v->pixels,w,h,(size_t)w*4,rgb(0x10141c),(double)w/v->width)!=NX_OK) return false;
    *pixels=v->pixels;*stride=(size_t)w*4;return true;
}
