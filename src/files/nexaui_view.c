#define _POSIX_C_SOURCE 200809L
#include "nexaui_view.h"
#include "text/grapheme.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static nx_result checked(nx_result result,const char *call,int line) {
    if(result!=NX_OK) fprintf(stderr,"Files view: %s at %d returned %d\n",call,line,(int)result);
    return result;
}
#define nx_semantics_add(...) checked(nx_semantics_add(__VA_ARGS__),"semantics",__LINE__)
#define nx_widget_register(...) checked(nx_widget_register(__VA_ARGS__),"widget",__LINE__)
#define nx_selection_group(...) checked(nx_selection_group(__VA_ARGS__),"selection",__LINE__)
#define nx_draw_text_layout(...) checked(nx_draw_text_layout(__VA_ARGS__),"text",__LINE__)
#define nx_end_frame(...) checked(nx_end_frame(__VA_ARGS__),"end frame",__LINE__)

#define LABEL_COUNT 768u
#define ID_BLOCK UINT64_C(40000)
struct label { char source[AE_FILES_LABEL+1];float size,width;bool tail;nx_text_layout *layout; };
struct colors { nx_color canvas,side,card,text,muted,line,selected,hover,focus; };
struct ae_files_view {
    struct ae_files_session *s;struct ae_files *model;nx_context *ui;nx_font *font;nx_font_collection *fonts;
    struct label labels[LABEL_COUNT];size_t label_count;
    struct colors p;uint64_t next,base,row_base,revision,serial,selected,modal_parent;unsigned state;
    float width,height;bool usable,zoom,consume_release,row_focus;uint64_t focus_wanted,last_click,double_click;
    double last_time;nx_point last_point;nx_rect menu,info,name,cancel,confirm;
    uint64_t drawn[AE_FV_CONTROLS];nx_rect bounds[AE_FV_CONTROLS];
    char commit[AE_FILES_NAME+1];size_t commit_size;bool commit_active,commit_bad;uint64_t commit_scope;
    char draft[AE_FILES_NAME+1];size_t draft_size;
};
static nx_utf8_span span(const char *p) { return (nx_utf8_span){p,strlen(p)}; }
static nx_rect rect(struct ae_files_rect r) { return (nx_rect){r.x,r.y,r.w,r.h}; }
static nx_color rgb(unsigned c) { return (nx_color){(float)((c>>16)&255u)/255,(float)((c>>8)&255u)/255,(float)(c&255u)/255,1}; }
static const char *tr(const struct ae_files_view *v,const char *en,const char *de) { return ae_files_session_tr(v->s,en,de); }
static bool inside(nx_rect r,nx_point p) { return r.width>0 && r.height>0 && p.x>=r.x && p.y>=r.y && p.x<r.x+r.width && p.y<r.y+r.height; }
static unsigned scope_state(const struct ae_files_session *s,bool usable) {
    return (s->dialog?1u:0u)|(s->menu?2u:0u)|(s->info_overlay?4u:0u)|(s->work_active?8u:0u)|
        (ae_files_session_unresolved(s)?16u:0u)|(s->rename_stale?32u:0u)|(s->closing?64u:0u)|(usable?128u:0u);
}
static bool sync_scope(struct ae_files_view *v) {
    struct ae_files_session *s=v->s;
    if(s->files!=v->model) return false;
    uint64_t revision=ae_files_revision(s->files),selected=ae_files_selected(s->files);unsigned state=scope_state(s,v->usable);
    bool draft_changed=s->dialog && (v->draft_size!=s->name_size || memcmp(v->draft,s->name,s->name_size));
    if(revision!=v->revision || selected!=v->selected || s->dialog_serial!=v->serial || state!=v->state || draft_changed) {
        if(v->next>UINT64_MAX-2*ID_BLOCK) return false;
        uint64_t focused=nx_focus_id(v->ui);
        for(unsigned i=0;i<AE_FV_CONTROLS;++i) if(!v->focus_wanted && v->drawn[i]==focused && focused &&
            (i!=AE_FV_SEARCH || s->query_focus) && (i!=AE_FV_NAME || s->dialog)) v->focus_wanted=i+1;
        if(revision!=v->revision || ((state^v->state)&128u)) {
            v->row_base=v->next;v->next+=ID_BLOCK;v->last_click=v->double_click=0;
        }
        if(s->dialog_serial!=v->serial && s->dialog) v->focus_wanted=(uint64_t)(ae_files_session_unresolved(s)?AE_FV_CANCEL:AE_FV_NAME)+1;
        if((state&16u) && !(v->state&16u) && s->dialog) v->focus_wanted=(uint64_t)AE_FV_CANCEL+1;
        v->base=v->next;v->next+=ID_BLOCK;v->revision=revision;v->selected=selected;v->serial=s->dialog_serial;v->state=state;
        v->draft_size=s->dialog?s->name_size:0;memcpy(v->draft,s->name,v->draft_size);v->draft[v->draft_size]=0;
        memset(v->drawn,0,sizeof(v->drawn));v->commit_active=false;
    }
    return true;
}
static uint64_t control(const struct ae_files_view *v,enum ae_files_control c) { return c==AE_FV_CLOSE?10:c==AE_FV_ZOOM?11:v->base+(uint64_t)c; }
static uint64_t entry(const struct ae_files_view *v,size_t visible) {
    const struct ae_files_entry *e=ae_files_entry(v->model,visible);
    if(!e) return 0;
    for(size_t i=0;i<ae_files_total(v->model);++i) if(ae_files_snapshot_entry(v->model,i)==e) return v->row_base+1024+(uint64_t)i*8;
    return 0;
}
/* Sidebar ordinals change when hidden locations disappear. Bind root children
   to their raw snapshot entry, and ancestors to their held depth instead. */
static uint64_t place(const struct ae_files_view *v,size_t index) {
    const struct ae_files_session *s=v->s;
    return s->place_snapshot[index]==SIZE_MAX?v->row_base+512+(uint64_t)s->place_depth[index]*4:
        v->row_base+1024+(uint64_t)s->place_snapshot[index]*8+4;
}
uint64_t ae_files_view_control_id(const struct ae_files_view *v,enum ae_files_control c) {
    return v && c>=0 && c<AE_FV_CONTROLS && v->model==v->s->files && v->revision==ae_files_revision(v->model) &&
        v->serial==v->s->dialog_serial && v->selected==ae_files_selected(v->model) &&
        (!v->s->dialog || (v->draft_size==v->s->name_size && !memcmp(v->draft,v->s->name,v->draft_size))) ? v->drawn[c] : 0;
}
uint64_t ae_files_view_entry_id(const struct ae_files_view *v,size_t visible) {
    if(!v || !v->usable || v->revision!=ae_files_revision(v->model) || v->selected!=ae_files_selected(v->model) ||
       visible<v->s->first || visible-v->s->first>=v->s->rows) return 0;
    return entry(v,visible);
}
bool ae_files_view_create(struct ae_files_session *s,const void *font,size_t n,struct ae_files_view **out) {
    if(!out) return false;
    *out=NULL;if(!s || !s->files || !font || !n) return false;
    struct ae_files_view *v=calloc(1,sizeof(*v));if(!v) return false;
    v->s=s;v->model=s->files;v->next=1024;
    if(nx_context_create(NULL,&v->ui)!=NX_OK || nx_font_open(font,n,NULL,&v->font)!=NX_OK ||
       nx_font_collection_create(NULL,&v->fonts)!=NX_OK || nx_font_collection_add(v->fonts,v->font)!=NX_OK) {
        ae_files_view_destroy(v);return false;
    }
    *out=v;return true;
}
void ae_files_view_destroy(struct ae_files_view *v) {
    if(!v) return;
    for(size_t i=0;i<LABEL_COUNT;++i) nx_text_layout_destroy(v->labels[i].layout);
    nx_context_destroy(v->ui);nx_font_collection_destroy(v->fonts);nx_font_close(v->font);free(v);
}
nx_context *ae_files_view_context(struct ae_files_view *v) { return v?v->ui:NULL; }
bool ae_files_view_take_zoom(struct ae_files_view *v) { bool value=v && v->zoom;if(v) v->zoom=false;return value; }
static bool palette(struct ae_files_view *v) {
    v->p=v->s->contrast?(struct colors){rgb(0),rgb(0),rgb(0),rgb(0xffffff),rgb(0xffffff),rgb(0xffffff),rgb(0x222222),rgb(0x333333),rgb(0xffe240)}:
        (struct colors){rgb(0x252833),rgb(0x20232c),rgb(0x2b2f3a),rgb(0xe8eaf0),rgb(0xb9c1ce),rgb(0x596271),rgb(0x3b4353),rgb(0x333946),rgb(0xc6d8ef)};
    nx_theme theme;
    if(nx_theme_preset(v->s->contrast?NX_THEME_HIGH_CONTRAST:NX_THEME_DARK,&theme)!=NX_OK) return false;
    const nx_color_token tokens[]={NX_COLOR_BACKGROUND,NX_COLOR_SURFACE,NX_COLOR_TEXT,NX_COLOR_TEXT_MUTED,NX_COLOR_BORDER,NX_COLOR_FOCUS};
    nx_color values[]={v->p.canvas,v->p.card,v->p.text,v->p.muted,v->p.line,v->p.focus};
    for(size_t i=0;i<sizeof(tokens)/sizeof(tokens[0]);++i) if(nx_theme_set_color(&theme,tokens[i],values[i])!=NX_OK) return false;
    return nx_context_set_theme(v->ui,&theme)==NX_OK;
}
/* Cache actual layouts; elision never changes the full semantic name or raw
   filesystem name. Both ends are whole graphemes, including editable tails. */
static bool label(struct ae_files_view *v,const char *source,float size,float width,bool tail,nx_text_layout **out) {
    if(v->label_count==LABEL_COUNT || strlen(source)>AE_FILES_LABEL || width<=0) return false;
    struct label *l=&v->labels[v->label_count++];
    if(l->layout && l->size==size && l->width==width && l->tail==tail && !strcmp(l->source,source)) { *out=l->layout;return true; }
    nx_text_layout *candidate=NULL;size_t n=strlen(source);
    if(nx_text_layout_create(v->fonts,source,n,NX_BIDI_LTR,size,16384,&candidate)!=NX_OK) return false;
    if(nx_text_layout_width(candidate)>width) {
        size_t offsets[AE_FILES_LABEL+1],count;char display[AE_FILES_LABEL+4];
        if(!ae_text_graphemes(source,n,offsets,AE_FILES_LABEL+1,&count)) { nx_text_layout_destroy(candidate);return false; }
        size_t lo=0,hi=count-1,best=0;
        while(lo<=hi) {
            size_t mid=lo+(hi-lo)/2,bytes=tail?n-offsets[count-1-mid]:offsets[mid];
            if(tail) { memcpy(display,"…",3);memcpy(display+3,source+n-bytes,bytes); }
            else { memcpy(display,source,bytes);memcpy(display+bytes,"…",3); }
            nx_text_layout *probe=NULL;
            if(nx_text_layout_create(v->fonts,display,bytes+3,NX_BIDI_LTR,size,16384,&probe)!=NX_OK) { nx_text_layout_destroy(candidate);return false; }
            bool fits=nx_text_layout_width(probe)<=width;nx_text_layout_destroy(probe);
            if(fits) { best=mid;lo=mid+1; }else { if(!mid) break;hi=mid-1; }
        }
        size_t bytes=tail?n-offsets[count-1-best]:offsets[best];
        if(tail) { memcpy(display,"…",3);memcpy(display+3,source+n-bytes,bytes); }
        else { memcpy(display,source,bytes);memcpy(display+bytes,"…",3); }
        nx_text_layout_destroy(candidate);candidate=NULL;
        if(nx_text_layout_create(v->fonts,display,bytes+3,NX_BIDI_LTR,size,16384,&candidate)!=NX_OK) return false;
    }
    nx_text_layout_destroy(l->layout);l->layout=candidate;l->size=size;l->width=width;l->tail=tail;memcpy(l->source,source,n+1);*out=candidate;return true;
}
static bool text(struct ae_files_view *v,uint64_t id,uint64_t parent,const char *s,nx_rect r,float size,nx_color color,bool tail) {
    if(r.width<=0 || r.height<=0) return true;
    nx_text_layout *l=NULL;
    nx_semantic_desc sem={.id=id,.parent_id=parent,.role=NX_ROLE_TEXT,.bounds=r,.name=span(s)};
    if(nx_semantics_add(v->ui,&sem,NULL)!=NX_OK || !label(v,s,size,r.width,tail,&l) || nx_push_clip(v->ui,r)!=NX_OK) return false;
    bool okay=nx_draw_text_layout(v->ui,l,(nx_point){r.x,r.y+(r.height-(float)nx_text_layout_height(l))/2},color,1024u*1024u)==NX_OK;
    return nx_pop_clip(v->ui)==NX_OK && okay;
}
struct intent { uint64_t id;bool native;unsigned count; };
static bool widget(struct ae_files_view *v,uint64_t id,uint64_t parent,nx_semantic_role role,const char *name,const char *value,
    nx_rect r,bool disabled,bool selected,bool invalid,struct intent *intent) {
    nx_widget_desc d={.semantic={.id=id,.parent_id=parent,.role=role,.bounds=r,.name=span(name),.value=span(value?value:""),
        .description=span(invalid?tr(v,"Input rejected; clear it before continuing","Eingabe abgewiesen; vor dem Fortfahren löschen"):""),
        .state=(disabled?NX_SEM_DISABLED:0u)|(selected?NX_SEM_SELECTED:0u)|(invalid?NX_SEM_INVALID:0u),.actions=NX_ACTION_FOCUS|(role==NX_ROLE_TEXT_FIELD?0u:NX_ACTION_PRESS)|(role==NX_ROLE_LIST_ITEM?NX_ACTION_SELECT:0u)},
        .flags=NX_WIDGET_INTERACTIVE|NX_WIDGET_FOCUSABLE};
    nx_interaction_state i;
    if(nx_widget_register(v->ui,&d,&i)!=NX_OK) return false;
    if(!disabled && i.clicked) { intent->id=id;intent->native=!i.pointer_released;intent->count+=i.clicked; }
    if(nx_fill_round_rect(v->ui,r,6,selected?v->p.selected:i.held?v->p.selected:i.hovered?v->p.hover:v->p.card)!=NX_OK) return false;
    return !i.focused || nx_stroke_rect(v->ui,r,2,v->p.focus)==NX_OK;
}
static bool button(struct ae_files_view *v,enum ae_files_control c,const char *name,nx_rect r,bool disabled,bool selected,struct intent *intent) {
    uint64_t id=control(v,c);v->drawn[c]=id;v->bounds[c]=r;
    if(!widget(v,id,v->modal_parent?v->modal_parent:1,v->modal_parent && v->s->menu?NX_ROLE_MENU_ITEM:NX_ROLE_BUTTON,name,NULL,r,disabled,selected,false,intent)) return false;
    nx_color ink=disabled?v->p.muted:v->p.text;float x=r.x+r.width/2,y=r.y+r.height/2;
    if(c==AE_FV_CLOSE) return nx_draw_line(v->ui,(nx_point){x-4,y-4},(nx_point){x+4,y+4},1,ink)==NX_OK &&
        nx_draw_line(v->ui,(nx_point){x+4,y-4},(nx_point){x-4,y+4},1,ink)==NX_OK;
    if(c==AE_FV_ZOOM) return nx_stroke_rect(v->ui,(nx_rect){x-5,y-4,10,8},1,ink)==NX_OK;
    if(c==AE_FV_LIST || c==AE_FV_MORE) {
        for(unsigned i=0;i<3;++i) if(nx_draw_line(v->ui,(nx_point){x-5,y-4+(float)i*4},(nx_point){x+5,y-4+(float)i*4},1,ink)!=NX_OK) return false;
        return true;
    }
    if(c==AE_FV_GRID) {
        for(unsigned i=0;i<4;++i) if(nx_stroke_rect(v->ui,(nx_rect){x-6+(float)(i%2)*7,y-6+(float)(i/2)*7,5,5},1,ink)!=NX_OK) return false;
        return true;
    }
    if(c==AE_FV_NEW_FILE || c==AE_FV_NEW_FOLDER) return nx_stroke_rect(v->ui,(nx_rect){x-7,y-5,11,10},1,ink)==NX_OK &&
        nx_draw_line(v->ui,(nx_point){x+2,y+4},(nx_point){x+8,y+4},1,ink)==NX_OK &&
        nx_draw_line(v->ui,(nx_point){x+5,y+1},(nx_point){x+5,y+7},1,ink)==NX_OK;
    if(c==AE_FV_UP) return nx_draw_line(v->ui,(nx_point){x+3,y-5},(nx_point){x-3,y},1,ink)==NX_OK &&
        nx_draw_line(v->ui,(nx_point){x-3,y},(nx_point){x+3,y+5},1,ink)==NX_OK;
    return text(v,id+256,id,c==AE_FV_DETAILS?"i":name,(nx_rect){r.x+6,r.y,r.width-12,r.height},11*v->s->scale,ink,false);
}
static bool field(struct ae_files_view *v,enum ae_files_control c,const char *name,const char *value,nx_rect r,bool disabled,bool invalid,struct intent *intent) {
    uint64_t id=control(v,c);v->drawn[c]=id;v->bounds[c]=r;
    if(!widget(v,id,v->modal_parent?v->modal_parent:1,NX_ROLE_TEXT_FIELD,name,value,r,disabled,false,invalid,intent)) return false;
    if(invalid && nx_stroke_rect(v->ui,r,2,rgb(0xffbdad))!=NX_OK) return false;
    return text(v,id+256,id,*value?value:name,(nx_rect){r.x+8,r.y,r.width-16,r.height},14*v->s->scale,*value?v->p.text:v->p.muted,true);
}
static void focus_control(struct ae_files_view *v,enum ae_files_control c) { v->focus_wanted=(uint64_t)c+1; }
static bool blocked(const struct ae_files_session *s) { return s->dialog || s->menu || s->info_overlay || s->closing; }
static void details(struct ae_files_view *v) {
    struct ae_files_session *s=v->s;
    if(s->details && (s->layout.inspector_visible || s->info_overlay)) { s->details=false;s->info_overlay=false; }
    else { struct ae_files_layout layout;s->details=true;
        s->info_overlay=!ae_files_layout_make((int)v->width,(int)v->height,s->scale,s->grid,true,s->sidebar,false,&layout) || !layout.inspector_visible; }
    if(s->info_overlay) focus_control(v,AE_FV_INFO_CLOSE);
    s->dirty=true;
}
static void action(struct ae_files_view *v,enum ae_files_control c) {
    struct ae_files_session *s=v->s;
    switch(c) {
    case AE_FV_CLOSE:ae_files_session_request_close(s);break;
    case AE_FV_ZOOM:v->zoom=true;break;
    case AE_FV_UP:ae_files_session_result(s,ae_files_up(s->files));break;
    case AE_FV_LIST:s->grid=false;break;case AE_FV_GRID:s->grid=true;break;
    case AE_FV_NEW_FILE:case AE_FV_NEW_FOLDER:ae_files_session_dialog_open(s,c==AE_FV_NEW_FILE?AE_FILES_CREATE_FILE:AE_FILES_CREATE_DIRECTORY);focus_control(v,AE_FV_NAME);break;
    case AE_FV_SEARCH:s->query_focus=true;focus_control(v,AE_FV_SEARCH);break;
    case AE_FV_DETAILS:details(v);break;
    case AE_FV_MORE:ae_files_session_menu_open(s,v->width-320*s->scale,32*s->scale,true);focus_control(v,(enum ae_files_control)((unsigned)AE_FV_CUT+s->menu_focus));break;
    case AE_FV_HIDDEN:s->show_hidden=!s->show_hidden;ae_files_hidden(s->files,s->show_hidden);ae_files_session_reveal(s);break;
    case AE_FV_OPEN:ae_files_session_activate(s);break;
    case AE_FV_CUT:ae_files_session_cut_selected(s);s->menu=false;break;
    case AE_FV_PASTE:ae_files_session_paste_open(s);s->menu=false;break;
    case AE_FV_RENAME:ae_files_session_rename_open(s);s->menu=false;focus_control(v,AE_FV_NAME);break;
    case AE_FV_CANCEL:ae_files_session_dialog_close(s);v->row_focus=true;break;
    case AE_FV_CONFIRM:ae_files_session_create_submit(s);break;
    case AE_FV_NAME:s->name_focus=0;focus_control(v,AE_FV_NAME);break;
    case AE_FV_INFO_CLOSE:s->info_overlay=false;s->details=false;v->row_focus=true;break;
    default:break;
    }
    s->dirty=true;
}
static bool information(struct ae_files_view *v,nx_rect r) {
    struct ae_files_session *s=v->s;float z=s->scale,y=r.y+12;uint64_t parent=v->modal_parent?v->modal_parent:1;
    const struct ae_files_entry *e=ae_files_entry(s->files,ae_files_selected(s->files));
    if(nx_fill_round_rect(v->ui,r,8,v->p.side)!=NX_OK ||
       !text(v,v->base+80,parent,tr(v,"Information","Informationen"),(nx_rect){r.x+12,y,r.width-24,28*z},13*z,v->p.text,false)) return false;
    y+=36*z;
    float bottom=r.y+r.height-(v->modal_parent?46*z:12),row=fminf(34*z,(bottom-y)/5);
    if(!e) return text(v,v->base+81,parent,tr(v,"No entry selected","Kein Eintrag ausgewählt"),(nx_rect){r.x+12,y,r.width-24,fminf(32*z,bottom-y)},12*z,v->p.muted,false);
    char bytes[64];(void)snprintf(bytes,sizeof(bytes),"%llu %s",(unsigned long long)e->size,tr(v,"bytes","Bytes"));
    const char *values[]={e->label,ae_files_session_kind_label(s,e),bytes,tr(v,"Location","Ort"),ae_files_session_folder_label(s)};
    for(size_t i=0;i<5;++i) { if(!text(v,v->base+81+i,parent,values[i],(nx_rect){r.x+12,y,r.width-24,row},i?12*z:14*z,i?v->p.muted:v->p.text,false)) return false;y+=row; }
    return true;
}
static bool naming(struct ae_files_view *v,nx_rect r,bool review,struct intent *intent) {
    struct ae_files_session *s=v->s;float z=s->scale;
    bool waiting=ae_files_session_creating(s),unknown=ae_files_session_unresolved(s),disabled=waiting || unknown || s->rename_stale || s->closing;
    const char *heading=unknown?tr(v,"Inspect file operation","Dateiaktion prüfen"):s->moving?tr(v,"Move","Verschieben"):s->renaming?tr(v,"Rename","Umbenennen"):
        s->create_kind==AE_FILES_CREATE_DIRECTORY?tr(v,"New folder","Neuer Ordner"):tr(v,"New file","Neue Datei");
    v->modal_parent=v->base+90;
    nx_semantic_desc dialog={.id=v->modal_parent,.parent_id=1,.role=NX_ROLE_DIALOG,.bounds=r,.name=span(heading)};
    if(nx_semantics_add(v->ui,&dialog,NULL)!=NX_OK) return false;
    if(nx_fill_round_rect(v->ui,r,8,v->p.card)!=NX_OK) return false;
    if(unknown) {
        /* Inspection describes the retained attempt; it offers neither editing
           nor a confirmation that could imply retry or rollback. */
        const char *labels[]={s->moving?tr(v,"From folder","Aus Ordner"):s->renaming?tr(v,"Previous name","Vorheriger Name"):tr(v,"Operation","Aktion"),
            s->moving?tr(v,"To folder","In Ordner"):tr(v,"Folder","Ordner"),s->renaming?tr(v,"New name","Neuer Name"):tr(v,"Name","Name")};
        const char *values[]={s->moving?s->from_folder:s->renaming?s->original_label:s->create_kind==AE_FILES_CREATE_DIRECTORY?tr(v,"New folder","Neuer Ordner"):tr(v,"New file","Neue Datei"),
            s->folder,s->name};
        float top=r.y+12+32*z,row=(r.height-70*z-8-(top-r.y))/3,column=fminf(150*z,(r.width-24)*.35f);
        if(!text(v,v->base+70,v->modal_parent,heading,(nx_rect){r.x+12,r.y+8,r.width-24,32*z},18*z,v->p.text,false)) return false;
        for(unsigned i=0;i<3;++i) {
            nx_rect line={r.x+12,top+(float)i*row,r.width-24,row};
            if(!text(v,v->base+71+(uint64_t)i*2,v->modal_parent,labels[i],(nx_rect){line.x,line.y,column,line.height},12*z,v->p.muted,false) ||
               !text(v,v->base+72+(uint64_t)i*2,v->modal_parent,values[i],(nx_rect){line.x+column+8,line.y,line.width-column-8,line.height},12*z,v->p.text,false)) return false;
        }
        v->cancel=(nx_rect){r.x+12,r.y+r.height-40*z,r.width-24,32*z};s->name_focus=1;
        bool okay=text(v,v->base+77,v->modal_parent,s->create_message,(nx_rect){r.x+12,r.y+r.height-70*z,r.width-24,26*z},11*z,v->p.text,false) &&
            button(v,AE_FV_CANCEL,tr(v,"Close inspection","Prüfung schließen"),v->cancel,false,false,intent);
        v->modal_parent=0;return okay;
    }
    if(review) {
        v->name=(nx_rect){r.x+12,r.y+r.height-104*z,r.width-24,32*z};
        float top=r.y+12+32*z,row=(v->name.y-8-top)/3;
        if(!text(v,v->base+70,v->modal_parent,heading,(nx_rect){r.x+12,r.y+8,r.width-24,32*z},18*z,v->p.text,false) ||
           !text(v,v->base+71,v->modal_parent,s->original_label,(nx_rect){r.x+12,top,r.width-24,row},12*z,v->p.muted,false) ||
           !text(v,v->base+72,v->modal_parent,s->moving?s->from_folder:s->folder,(nx_rect){r.x+12,top+row,r.width-24,row},12*z,v->p.muted,false) ||
           !text(v,v->base+73,v->modal_parent,s->folder,(nx_rect){r.x+12,top+2*row,r.width-24,row},12*z,v->p.muted,false)) return false;
        v->cancel=(nx_rect){r.x+12,r.y+r.height-40*z,(r.width-32)/2,32*z};
        v->confirm=(nx_rect){r.x+20+(r.width-32)/2,r.y+r.height-40*z,(r.width-32)/2,32*z};
        if(!text(v,v->base+74,v->modal_parent,s->create_message,(nx_rect){r.x+12,r.y+r.height-70*z,r.width-24,26*z},11*z,v->p.text,false)) return false;
    } else if(s->grid) {
        v->name=(nx_rect){r.x+6,r.y+6,r.width-12,32*z};
        v->cancel=(nx_rect){r.x+6,r.y+r.height-32*z-6,(r.width-18)/2,32*z};
        v->confirm=(nx_rect){r.x+12+(r.width-18)/2,r.y+r.height-32*z-6,(r.width-18)/2,32*z};
    } else {
        v->cancel=(nx_rect){r.x+r.width-72*z,r.y+6,32*z,r.height-12};
        v->confirm=(nx_rect){r.x+r.width-36*z,r.y+6,32*z,r.height-12};
        v->name=(nx_rect){r.x+6,r.y+6,r.width-84*z,r.height-12};
    }
    if(!field(v,AE_FV_NAME,tr(v,"Name","Name"),s->name,v->name,disabled,s->name_bad,intent) ||
       !button(v,AE_FV_CANCEL,tr(v,"Cancel","Abbrechen"),v->cancel,waiting,false,intent) ||
       !button(v,AE_FV_CONFIRM,review?tr(v,"Confirm","Bestätigen"):tr(v,"OK","OK"),v->confirm,
            disabled || s->composing || s->name_bad || !ae_files_create_name(s->name,s->name_size),false,intent)) return false;
    uint64_t f=nx_focus_id(v->ui);
    if(f==control(v,AE_FV_NAME)) s->name_focus=0;else if(f==control(v,AE_FV_CANCEL)) s->name_focus=1;else if(f==control(v,AE_FV_CONFIRM)) s->name_focus=2;
    v->modal_parent=0;
    return true;
}
bool ae_files_view_frame(struct ae_files_view *v,float width,float height,const nx_window_status *status,bool *changed) {
    if(!v || !status || !changed || !isfinite(width) || !isfinite(height) || width<64 || height<64 || width>3840 || height>2160) return false;
    *changed=false;struct ae_files_session *s=v->s;
    if(!isfinite(s->scale) || s->scale<1 || s->scale>2) return false;
    v->width=width;v->height=height;v->usable=width>=640 && height>=420;v->label_count=0;v->modal_parent=0;
    if(!sync_scope(v) || !palette(v) || nx_begin_frame(v->ui,width,height,0)!=NX_OK) return false;
    memset(v->drawn,0,sizeof(v->drawn));
    nx_semantic_desc root={.id=1,.role=NX_ROLE_WINDOW,.bounds={0,0,width,height},.name=span(tr(v,"Files","Dateien"))};
    struct intent intent={0};
    if(nx_semantics_add(v->ui,&root,NULL)!=NX_OK || nx_fill_rect(v->ui,root.bounds,v->p.canvas)!=NX_OK) return false;
    float z=s->scale;
    if(!v->usable) {
        if(!button(v,AE_FV_CLOSE,"×",(nx_rect){8,2,26,26},false,false,&intent) ||
           !text(v,v->base+64,1,tr(v,"Enlarge the window to continue.","Vergrößere das Fenster, um fortzufahren."),
                (nx_rect){12,38,width-24,height-42},12*z,v->p.text,false) || nx_end_frame(v->ui)!=NX_OK) return false;
        if(intent.id==10) { action(v,AE_FV_CLOSE);*changed=true; }return true;
    }
    if(!ae_files_layout_make((int)width,(int)height,z,s->grid,s->details,s->sidebar,ae_files_session_inline_edit(s),&s->layout)) return false;
    struct ae_files_layout *l=&s->layout;bool background=blocked(s);uint64_t group=v->row_base+32;
    uint64_t selected=entry(v,ae_files_selected(s->files)),requested=selected;
    if(!background && nx_selection_apply(v->ui,group,1,&requested)!=NX_OK) return false;
    if(!background && requested!=selected) {
        for(size_t i=0;i<ae_files_count(s->files);++i) if(entry(v,i)==requested) { (void)ae_files_select(s->files,i);s->query_focus=false;*changed=true;break; }
        if(!sync_scope(v)) return false;
    }
    selected=entry(v,ae_files_selected(s->files));ae_files_session_reveal(s);s->rows=l->capacity;
    if(nx_fill_rect(v->ui,rect(l->chrome),v->p.side)!=NX_OK || nx_fill_rect(v->ui,rect(l->sidebar),v->p.side)!=NX_OK) return false;
    bool restore=(status->state&(NX_WINDOW_STATE_MAXIMIZED|NX_WINDOW_STATE_FULLSCREEN))!=0;
    if(!button(v,AE_FV_CLOSE,tr(v,"Close","Schließen"),(nx_rect){8,2,40,26},background,false,&intent) ||
       !button(v,AE_FV_ZOOM,restore?tr(v,"Restore","Zurück"):tr(v,"Zoom","Zoom"),(nx_rect){54,2,50,26},background,
            false,&intent) || !text(v,v->base+64,1,tr(v,"Files","Dateien"),(nx_rect){112,0,width-170,l->chrome.h},11*z,v->p.text,false)) return false;
    const enum ae_files_control tools[]={AE_FV_UP,AE_FV_LIST,AE_FV_GRID,AE_FV_NEW_FOLDER,AE_FV_NEW_FILE,AE_FV_DETAILS,AE_FV_MORE};
    const char *names[]={tr(v,"Up","Zurück"),tr(v,"List","Liste"),tr(v,"Grid","Raster"),tr(v,"New folder","Neuer Ordner"),tr(v,"New file","Neue Datei"),tr(v,"Information","Informationen"),tr(v,"Actions","Aktionen")};
    nx_rect boxes[]={rect(l->back),rect(l->list),rect(l->grid),rect(l->new_folder),rect(l->new_file),rect(l->details),rect(l->more)};
    for(size_t i=0;i<sizeof(tools)/sizeof(tools[0]);++i)
        if(!button(v,tools[i],names[i],boxes[i],background,(tools[i]==AE_FV_GRID?s->grid:tools[i]==AE_FV_LIST?!s->grid:false),&intent)) return false;
    if(!field(v,AE_FV_SEARCH,tr(v,"Search this folder","Ordner durchsuchen"),ae_files_query(s->files),rect(l->search),background,s->query_bad,&intent)) return false;
    if(nx_focus_id(v->ui)==control(v,AE_FV_SEARCH)) s->query_focus=true;
    if(!text(v,v->base+65,1,ae_files_session_folder_label(s),rect(l->heading),18*z,v->p.text,false)) return false;
    s->place_count=0;
    if(l->sidebar_visible) {
        float y=l->sidebar.y+10*z;
        for(unsigned d=0;d<=ae_files_depth(s->files) && s->place_count<=AE_FILES_DEPTH;++d) {
            nx_rect r={10*z,y,l->sidebar.w-20*z,32*z};if(r.y+r.height>l->status.y) break;
            size_t index=s->place_count++;s->place_depth[index]=d;s->place_snapshot[index]=SIZE_MAX;
            uint64_t id=place(v,index);
            const char *name=d?ae_files_ancestor_label(s->files,d):s->root_label;
            if(!widget(v,id,1,NX_ROLE_BUTTON,name,NULL,r,background,d==ae_files_depth(s->files),false,&intent) ||
               !text(v,id+1,id,name,(nx_rect){r.x+8,r.y,r.width-16,r.height},12*z,v->p.text,false)) return false;
            y+=38*z;
        }
        if(!ae_files_depth(s->files)) for(size_t i=0;i<ae_files_total(s->files) && s->place_count<=AE_FILES_DEPTH;++i) {
            const struct ae_files_entry *e=ae_files_snapshot_entry(s->files,i);
            if(e->kind!=AE_FILES_DIRECTORY || (!s->show_hidden && e->name[0]=='.')) continue;
            nx_rect r={10*z,y,l->sidebar.w-20*z,32*z};if(r.y+r.height>l->status.y) break;
            size_t index=s->place_count++;s->place_depth[index]=1;s->place_snapshot[index]=i;
            uint64_t id=place(v,index);
            if(!widget(v,id,1,NX_ROLE_BUTTON,e->label,NULL,r,background,false,false,&intent) ||
               !text(v,id+1,id,e->label,(nx_rect){r.x+8,r.y,r.width-16,r.height},12*z,v->p.text,false)) return false;
            y+=38*z;
        }
    }
    nx_semantic_desc list={.id=group,.parent_id=1,.role=NX_ROLE_LIST,.bounds=rect(l->content),.name=span(tr(v,"Entries","Einträge")),.state=background?NX_SEM_DISABLED:0u};
    size_t count=ae_files_count(s->files);bool inline_name=ae_files_session_inline_edit(s);
    /* Naming can scroll the original selection offscreen at large text sizes.
       The blocked list exposes no selected member until its rows return. */
    if(inline_name) selected=0;
    if(nx_selection_group(v->ui,&list,selected!=0,&selected)!=NX_OK) return false;
    size_t naming_index=s->renaming?ae_files_selected(s->files):count;
    if(inline_name) s->first=ae_files_layout_reveal(l,count+(s->renaming?0u:1u),naming_index,s->first);
    for(size_t slot=0;slot<l->capacity && s->first+slot<count+(inline_name && !s->renaming?1u:0u);++slot) {
        size_t index=s->first+slot;struct ae_files_rect cell;if(!ae_files_layout_cell(l,slot,&cell)) return false;nx_rect r=rect(cell);
        if(inline_name && index==naming_index) { if(!naming(v,r,false,&intent)) return false;continue; }
        const struct ae_files_entry *e=ae_files_entry(s->files,index);if(!e) return false;
        uint64_t id=entry(v,index);
        if(!widget(v,id,group,NX_ROLE_LIST_ITEM,e->label,ae_files_session_kind_label(s,e),r,background,id==selected,false,&intent)) return false;
        float x=r.x+10,top=r.y,iw=s->grid?32*z:20*z;
        if(nx_stroke_rect(v->ui,(nx_rect){x,top+8,iw,e->kind==AE_FILES_DIRECTORY?iw*.7f:iw},1,v->p.muted)!=NX_OK) return false;
        if(e->kind==AE_FILES_DIRECTORY && nx_draw_line(v->ui,(nx_point){x,top+5},(nx_point){x+iw*.4f,top+5},2,v->p.muted)!=NX_OK) return false;
        nx_rect name=s->grid?(nx_rect){x,top+iw+12,r.width-20,30*z}:(nx_rect){x+iw+10,top,r.width-iw-30,r.height};
        if(!text(v,id+1,id,e->label,name,13*z,v->p.text,false)) return false;
        if(s->grid && r.height>100*z && !text(v,id+2,id,ae_files_session_kind_label(s,e),(nx_rect){x,name.y+30*z,r.width-20,24*z},11*z,v->p.muted,false)) return false;
    }
    if(!count && !inline_name && !text(v,v->base+66,1,tr(v,"No entries","Keine Einträge"),rect(l->content),14*z,v->p.muted,false)) return false;
    if(l->inspector_visible && !s->info_overlay && !information(v,rect(l->inspector))) return false;
    const char *message=*s->create_message && s->dialog?s->create_message:*s->message?s->message:tr(v,"Enter open · Ctrl+N new file · F10 actions","Enter öffnen · Strg+N neue Datei · F10 Aktionen");
    if(!text(v,v->base+67,1,message,(nx_rect){12,l->status.y,width-130*z,l->status.h},10*z,v->p.muted,false) ||
       !button(v,AE_FV_HIDDEN,s->show_hidden?tr(v,"Hidden: on","Versteckt: an"):tr(v,"Hidden: off","Versteckt: aus"),
            (nx_rect){width-120*z,l->status.y,116*z,l->status.h},background,false,&intent)) return false;
    if(s->dialog && !inline_name) {
        nx_rect r={16,16,width-32,height-32};if(r.width>720) { r.x+=(r.width-720)/2;r.width=720; }
        if(r.height>320*z) {r.y+=(r.height-320*z)/2;r.height=320*z;}
        if(!naming(v,r,true,&intent)) return false;
    }
    if(s->menu) {
        float w=fminf(300*z,width-32),h=3*40*z+16;
        v->menu=(nx_rect){fmaxf(16,fminf(s->menu_x,width-w-16)),fmaxf(16,fminf(s->menu_y,height-h-16)),w,h};
        if(nx_fill_round_rect(v->ui,v->menu,8,v->p.side)!=NX_OK) return false;
        v->modal_parent=v->base+92;nx_semantic_desc menu={.id=v->modal_parent,.parent_id=1,.role=NX_ROLE_MENU,.bounds=v->menu,.name=span(tr(v,"Actions","Aktionen"))};
        if(nx_semantics_add(v->ui,&menu,NULL)!=NX_OK) return false;
        const enum ae_files_control choices[]={AE_FV_CUT,AE_FV_PASTE,AE_FV_RENAME};
        const char *labels[]={tr(v,"Cut","Ausschneiden"),tr(v,"Paste…","Einfügen…"),tr(v,"Rename…","Umbenennen…")};
        for(unsigned i=0;i<3;++i) if(!button(v,choices[i],labels[i],(nx_rect){v->menu.x+8,v->menu.y+8+(float)i*40*z,w-16,36*z},!ae_files_session_menu_enabled(s,i),false,&intent)) return false;
        v->modal_parent=0;
    }
    if(s->info_overlay) {
        float w=fminf(360*z,width-32);v->info=(nx_rect){(width-w)/2,8,w,height-16};
        v->modal_parent=v->base+91;nx_semantic_desc info={.id=v->modal_parent,.parent_id=1,.role=NX_ROLE_DIALOG,.bounds=v->info,.name=span(tr(v,"Information","Informationen"))};
        if(nx_semantics_add(v->ui,&info,NULL)!=NX_OK || !information(v,v->info) || !button(v,AE_FV_INFO_CLOSE,tr(v,"Close information","Informationen schließen"),
            (nx_rect){v->info.x+8,v->info.y+v->info.height-36*z,v->info.width-16,30*z},false,false,&intent)) return false;
        v->modal_parent=0;
    }
    if(nx_end_frame(v->ui)!=NX_OK) return false;
    if(v->focus_wanted) {
        uint64_t id=control(v,(enum ae_files_control)(v->focus_wanted-1));v->focus_wanted=0;
        if(nx_focus_set(v->ui,id)==NX_OK) *changed=true;
    }
    if(v->row_focus) { v->row_focus=false;if(nx_focus_set(v->ui,entry(v,ae_files_selected(s->files)))==NX_OK) *changed=true; }
    s->query_focus=!blocked(s) && nx_focus_id(v->ui)==control(v,AE_FV_SEARCH);
    if(intent.count==1) {
        bool acted=false;
        for(unsigned i=0;i<AE_FV_CONTROLS;++i) if(v->drawn[i] && v->drawn[i]==intent.id) { action(v,(enum ae_files_control)i);acted=true;break; }
        if(!acted && !background) {
            for(size_t i=0;i<count;++i) if(entry(v,i)==intent.id) {
                (void)ae_files_select(s->files,i);s->query_focus=false;ae_files_session_reveal(s);
                if(intent.native || v->double_click==intent.id) ae_files_session_activate(s);
                acted=true;break;
            }
            for(size_t i=0;!acted && i<s->place_count;++i) if(place(v,i)==intent.id) { ae_files_session_navigate_place(s,i);acted=true; }
        }
        if(acted) { *changed=true;s->dirty=true; }
    }
    v->double_click=0;return true;
}
static void reject_text(struct ae_files_view *v) {
    if(v->s->dialog) v->s->name_bad=true;else v->s->query_bad=true;
    ae_files_session_message(v->s,tr(v,"Input rejected; clear it before continuing","Eingabe abgewiesen; vor dem Fortfahren löschen"));
}
static void append(struct ae_files_view *v,const char *bytes,size_t length) {
    struct ae_files_session *s=v->s;
    if(s->dialog) { if(!length || length>AE_FILES_NAME) { reject_text(v);return; }char value[AE_FILES_NAME+1];memcpy(value,bytes,length);value[length]=0;ae_files_session_name_input(s,value); }
    else if(s->query_focus && !s->query_bad && !blocked(s)) {
        char value[AE_FILES_QUERY+1];size_t old=strlen(ae_files_query(s->files));
        if(length>AE_FILES_QUERY-old) { reject_text(v);return; }
        memcpy(value,ae_files_query(s->files),old);memcpy(value+old,bytes,length);
        if(!ae_files_filter(s->files,value,old+length)) reject_text(v);else { s->first=0;s->dirty=true; }
    }
}
static size_t scalar(uint32_t c,char out[4]) {
    if(!c || c>0x10ffff || (c>=0xd800 && c<=0xdfff)) return 0;
    if(c<0x80) { out[0]=(char)c;return 1; }
    if(c<0x800) { out[0]=(char)(0xc0u|(c>>6));out[1]=(char)(0x80u|(c&63u));return 2; }
    if(c<0x10000) { out[0]=(char)(0xe0u|(c>>12));out[1]=(char)(0x80u|((c>>6)&63u));out[2]=(char)(0x80u|(c&63u));return 3; }
    out[0]=(char)(0xf0u|(c>>18));out[1]=(char)(0x80u|((c>>12)&63u));out[2]=(char)(0x80u|((c>>6)&63u));out[3]=(char)(0x80u|(c&63u));return 4;
}
bool ae_files_view_event(struct ae_files_view *v,const nx_event *e) {
    if(!v || !e || !sync_scope(v)) return false;
    struct ae_files_session *s=v->s;s->dirty=true;
    if(e->type==NX_EVENT_FOCUS && !e->focused) {
        s->menu=false;v->consume_release=false;v->last_click=0;
        if(s->composing || v->commit_active) reject_text(v);
        s->composing=false;v->commit_active=false;
    }
    if(e->type==NX_EVENT_POINTER_CANCEL || e->type==NX_EVENT_RESIZE) { v->consume_release=false;v->last_click=0;s->menu=false; }
    if(v->commit_active && (e->type==NX_EVENT_KEY_DOWN || e->type==NX_EVENT_POINTER_DOWN)) { reject_text(v);v->commit_active=false; }
    if(e->type==NX_EVENT_TEXT) {
        /* Tagged native IME edits require a host-owned target/revision. This
           first append-only adapter does not admit them or a retained prefix. */
        if(e->ime_target || e->ime_revision) { reject_text(v);v->commit_active=false;return true; }
        char bytes[4];size_t n=scalar(e->scalar,bytes);uint32_t flags=e->ime_before;
        if(flags&NX_IME_COMMIT_START) { v->commit_active=true;v->commit_size=0;v->commit_bad=false;v->commit_scope=v->base; }
        if(flags&NX_IME_COMMIT_FILTERED) v->commit_bad=true;
        if(v->commit_active) {
            if(!n || n>sizeof(v->commit)-1-v->commit_size || v->commit_scope!=v->base) v->commit_bad=true;
            else { memcpy(v->commit+v->commit_size,bytes,n);v->commit_size+=n; }
            if(flags&NX_IME_COMMIT_END) { if(v->commit_bad) reject_text(v);else append(v,v->commit,v->commit_size);v->commit_active=false; }
        } else if(flags || !n) reject_text(v);else append(v,bytes,n);
        s->composing=false;return true;
    }
    if(e->type==NX_EVENT_IME_PREEDIT_CHANGED) { s->composing=true;return true; }
    if(e->type==NX_EVENT_IME_DELETE_RANGE || e->type==NX_EVENT_IME_DELETE_SURROUNDING || e->type==NX_EVENT_TEXT_COMMAND) { reject_text(v);return true; }
    if(e->type==NX_EVENT_KEY_DOWN && !e->repeat) {
        uint32_t k=e->key,m=e->modifiers;bool ctrl=(m&NX_MOD_CONTROL)!=0,alt=(m&NX_MOD_ALT)!=0;
        if(ctrl && k==NX_KEY_Q) { ae_files_session_request_close(s);return true; }
        if(!v->usable) return true;
        if(k==NX_KEY_F6) s->german=!s->german;
        else if(k==NX_KEY_F7) s->contrast=!s->contrast;
        else if(k==NX_KEY_F8) s->scale=s->scale==1?1.25f:s->scale==1.25f?1.5f:s->scale==1.5f?2:1;
        else if(k==NX_KEY_F11) ae_files_session_message(s,tr(v,"Fullscreen is unavailable in this view","Vollbild ist in dieser Ansicht nicht verfügbar"));
        else if(s->dialog) {
            if(ae_files_session_creating(s)) return true;
            if(k==NX_KEY_ESCAPE) { ae_files_session_dialog_close(s);v->row_focus=true; }
            else if(k==NX_KEY_ENTER) { if(s->name_focus==1) ae_files_session_dialog_close(s);else ae_files_session_create_submit(s); }
            else if(ctrl && k==NX_KEY_U && !ae_files_session_unresolved(s) && !s->rename_stale && s->name_focus==0) { s->name_bad=false;s->composing=false;s->name_size=0;s->name[0]=0;s->create_message[0]=0; }
            else if(k==NX_KEY_BACKSPACE && !s->name_bad && s->name_focus==0 && !ae_files_session_unresolved(s) && !s->rename_stale && s->name_size) {
                size_t offsets[AE_FILES_NAME+1],count;if(ae_text_graphemes(s->name,s->name_size,offsets,AE_FILES_NAME+1,&count)) { s->name_size=offsets[count-2];s->name[s->name_size]=0; }
            } else return nx_context_queue_event(v->ui,e)==NX_OK;
        } else if(s->info_overlay) { if(k==NX_KEY_ESCAPE || k==NX_KEY_F9) { s->info_overlay=false;s->details=false; } }
        else if(s->menu) {
            if(k==NX_KEY_ESCAPE || k==NX_KEY_F10) s->menu=false;
            else return nx_context_queue_event(v->ui,e)==NX_OK;
        } else if(k==NX_KEY_F9) details(v);
        else if(k==NX_KEY_F4) s->sidebar=!s->sidebar;
        else if(ctrl && (k==NX_KEY_1 || k==NX_KEY_2)) s->grid=k==NX_KEY_2;
        else if(k==NX_KEY_F10) action(v,AE_FV_MORE);
        else if(ctrl && k==NX_KEY_X && !s->query_focus) action(v,AE_FV_CUT);
        else if(ctrl && k==NX_KEY_V && !s->query_focus) action(v,AE_FV_PASTE);
        else if(k==NX_KEY_F2) action(v,AE_FV_RENAME);
        else if(ctrl && k==NX_KEY_N) action(v,(m&NX_MOD_SHIFT)?AE_FV_NEW_FOLDER:AE_FV_NEW_FILE);
        else if(ctrl && k==NX_KEY_R) ae_files_session_result(s,ae_files_refresh(s->files));
        else if(ctrl && k==NX_KEY_H) action(v,AE_FV_HIDDEN);
        else if(ctrl && (k==NX_KEY_L || (k==NX_KEY_U && s->query_focus))) { (void)ae_files_filter(s->files,"",0);s->query_bad=false;s->query_focus=true;focus_control(v,AE_FV_SEARCH); }
        else if(alt && k==NX_KEY_UP) action(v,AE_FV_UP);
        else if(alt && k==NX_KEY_HOME) ae_files_session_result(s,ae_files_ancestor(s->files,0));
        else if(k==NX_KEY_ESCAPE) { ae_files_session_clipboard_clear(s);s->query_focus=false;s->query_bad=false;(void)ae_files_filter(s->files,"",0);s->first=0;(void)nx_focus_set(v->ui,0);v->row_focus=true; }
        else if(k==NX_KEY_UP || k==NX_KEY_DOWN || k==NX_KEY_PAGE_UP || k==NX_KEY_PAGE_DOWN || k==NX_KEY_HOME || k==NX_KEY_END || (s->grid && (k==NX_KEY_LEFT || k==NX_KEY_RIGHT))) {
            if(k==NX_KEY_HOME || k==NX_KEY_END) { if(ae_files_count(s->files)) (void)ae_files_select(s->files,k==NX_KEY_HOME?0:ae_files_count(s->files)-1); }
            else { int step=k==NX_KEY_PAGE_UP || k==NX_KEY_PAGE_DOWN?(int)s->rows:k==NX_KEY_UP || k==NX_KEY_DOWN?(int)s->layout.columns:1;
                ae_files_session_move(s,(k==NX_KEY_UP || k==NX_KEY_PAGE_UP || k==NX_KEY_LEFT)?-step:step); }
            s->query_focus=false;ae_files_session_reveal(s);v->row_focus=true;
        } else if(k==NX_KEY_ENTER) {
            uint64_t f=nx_focus_id(v->ui);bool toolbar=false;
            for(unsigned i=0;i<AE_FV_CONTROLS;++i) if(i!=AE_FV_SEARCH && v->drawn[i] && v->drawn[i]==f) toolbar=true;
            if(toolbar) return nx_context_queue_event(v->ui,e)==NX_OK;
            s->query_focus=false;(void)nx_focus_set(v->ui,0);ae_files_session_activate(s);
        }
        else if(k==NX_KEY_BACKSPACE) {
            if(s->query_focus) { const char *q=ae_files_query(s->files);size_t n=strlen(q),offsets[AE_FILES_QUERY+1],count;
                if(!s->query_bad && n && ae_text_graphemes(q,n,offsets,AE_FILES_QUERY+1,&count)) (void)ae_files_filter(s->files,q,offsets[count-2]);
            }else action(v,AE_FV_UP);
        } else return nx_context_queue_event(v->ui,e)==NX_OK;
        return true;
    }
    if(e->type==NX_EVENT_WHEEL && !blocked(s)) { ae_files_session_move(s,e->delta_y>0?-3:e->delta_y<0?3:0);v->row_focus=true;return true; }
    nx_point point={e->x,e->y};
    if(e->type==NX_EVENT_POINTER_DOWN && e->button==2 && !blocked(s)) { action(v,AE_FV_MORE);return true; }
    if(e->type==NX_EVENT_POINTER_DOWN && e->button==1) {
        bool swallow=false;
        if(s->menu && !inside(v->menu,point)) { s->menu=false;swallow=true; }
        else if(s->info_overlay && !inside(v->info,point)) { s->info_overlay=false;s->details=false;swallow=true; }
        else if(ae_files_session_inline_edit(s) && v->drawn[AE_FV_NAME] && !inside(v->name,point) && !inside(v->cancel,point) && !inside(v->confirm,point)) { ae_files_session_create_submit(s);swallow=true; }
        if(swallow) { v->consume_release=true;nx_event cancel={.type=NX_EVENT_POINTER_CANCEL};return nx_context_queue_event(v->ui,&cancel)==NX_OK; }
    }
    if(v->consume_release && (e->type==NX_EVENT_POINTER_MOVE || e->type==NX_EVENT_POINTER_UP)) { if(e->type==NX_EVENT_POINTER_UP) v->consume_release=false;return true; }
    if(e->type==NX_EVENT_POINTER_UP && e->button==1 && !blocked(s)) {
        struct timespec t;if(clock_gettime(CLOCK_MONOTONIC,&t)) return false;double now=(double)t.tv_sec+(double)t.tv_nsec/1e9;
        for(size_t slot=0;slot<s->rows && s->first+slot<ae_files_count(s->files);++slot) {
            struct ae_files_rect r;if(!ae_files_layout_cell(&s->layout,slot,&r)) break;
            if(inside(rect(r),point)) { uint64_t id=entry(v,s->first+slot);float dx=point.x-v->last_point.x,dy=point.y-v->last_point.y;
                if(v->last_click==id && now-v->last_time<=.45 && dx*dx+dy*dy<=36) v->double_click=id;
                v->last_click=id;v->last_time=now;v->last_point=point;break;
            }
        }
    }
    return nx_context_queue_event(v->ui,e)==NX_OK;
}
bool ae_files_view_native_hit(const struct ae_files_view *v,nx_point p,const nx_window_status *status,bool *title,nx_window_resize_edge *edge,nx_cursor_shape *cursor) {
    if(!v || !status || !title || !edge || !cursor || !isfinite(p.x) || !isfinite(p.y)) return false;
    *title=false;*edge=0;*cursor=NX_CURSOR_DEFAULT;
    if(blocked(v->s) || p.x<0 || p.y<0 || p.x>=v->width || p.y>=v->height) return true;
    for(unsigned i=0;i<AE_FV_CONTROLS;++i) if(v->drawn[i] && inside(v->bounds[i],p)) return true;
    bool normal=(status->state&(NX_WINDOW_STATE_MAXIMIZED|NX_WINDOW_STATE_FULLSCREEN))==0;
    bool left=p.x<6,right=p.x>=v->width-6,top=p.y<6,bottom=p.y>=v->height-6;
    if(normal && (status->capabilities&NX_WINDOW_CAP_RESIZE) && (left || right || top || bottom)) {
        *edge=left && top?NX_WINDOW_RESIZE_TOP_LEFT:right && top?NX_WINDOW_RESIZE_TOP_RIGHT:left && bottom?NX_WINDOW_RESIZE_BOTTOM_LEFT:right && bottom?NX_WINDOW_RESIZE_BOTTOM_RIGHT:
            left?NX_WINDOW_RESIZE_LEFT:right?NX_WINDOW_RESIZE_RIGHT:top?NX_WINDOW_RESIZE_TOP:NX_WINDOW_RESIZE_BOTTOM;
        *cursor=(left||right)&&(top||bottom)?((left&&top)||(right&&bottom)?NX_CURSOR_RESIZE_DIAGONAL_NW_SE:NX_CURSOR_RESIZE_DIAGONAL_NE_SW):(left||right)?NX_CURSOR_RESIZE_HORIZONTAL:NX_CURSOR_RESIZE_VERTICAL;
    } else if(normal && (status->capabilities&NX_WINDOW_CAP_MOVE) && p.y<30*v->s->scale) *title=true;
    return true;
}
