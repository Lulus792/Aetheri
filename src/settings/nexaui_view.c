#include "settings/nexaui_view.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Local, semantic tokens for the accepted Settings study's opaque fallback.
   No compositor blur or backdrop permission is inferred from a painted panel. */
struct palette { nx_color canvas, sidebar, card, text, secondary, line, accent, selected, hover, focus; };
struct label { char bytes[512]; float width, size; nx_text_layout *layout; };
enum node_kind { TEXT, CARD, BUTTON, RADIO, NAV };
struct node {
    enum node_kind kind;
    uint64_t id, parent;
    nx_rect rect;
    size_t label;
    nx_icon icon;
    const char *name;
    bool selected, disabled, secondary, icon_only;
};
#define LABELS 128u
#define NODES 128u
#define GLYPH_CACHE (1024u * 1024u)
struct ae_settings_view {
    struct ae_settings *model;
    nx_context *context;
    nx_font *font;
    nx_font_collection *fonts;
    struct label labels[LABELS];
    struct node nodes[NODES];
    size_t label_count, node_count;
    struct palette colors;
    float scroll_x, scroll_y, last_width, last_height;
    uint64_t last_focus;
    unsigned last_text;
    bool last_german;
    enum ae_settings_page last_page;
    enum ae_settings_window_action action;
    bool refresh;
};
static nx_utf8_span span(const char *s) { return (nx_utf8_span){s, strlen(s)}; }
static nx_color rgb(unsigned value) {
    return (nx_color){(float)((value>>16)&255u)/255, (float)((value>>8)&255u)/255,
        (float)(value&255u)/255, 1};
}
static const char *tr(const struct ae_settings_view *v, const char *en, const char *de) {
    return v->model->german?de:en;
}
static bool palette(struct ae_settings_view *v) {
    enum ae_settings_theme kind=v->model->theme;
    struct palette p;
    if(kind==AE_SETTINGS_LIGHT) p=(struct palette){rgb(0xf4f5f8),rgb(0xe7e9ef),rgb(0xffffff),
        rgb(0x20242c),rgb(0x535d6d),rgb(0x8d96a5),rgb(0x394c70),rgb(0xd6deeb),rgb(0xe7ecf4),rgb(0x294a89)};
    else if(kind==AE_SETTINGS_CONTRAST) p=(struct palette){rgb(0x000000),rgb(0x000000),rgb(0x000000),
        rgb(0xffffff),rgb(0xffffff),rgb(0xffffff),rgb(0xffe240),rgb(0x222222),rgb(0x333333),rgb(0xffe240)};
    else p=(struct palette){rgb(0x20232c),rgb(0x1c1f28),rgb(0x282c36),
        rgb(0xe8eaf0),rgb(0xb9c1ce),rgb(0x596271),rgb(0xc3cad6),rgb(0x3b4353),rgb(0x333946),rgb(0xc6d8ef)};
    v->colors=p;
    nx_theme t;
    if(nx_theme_preset(kind==AE_SETTINGS_LIGHT?NX_THEME_LIGHT:
        kind==AE_SETTINGS_CONTRAST?NX_THEME_HIGH_CONTRAST:NX_THEME_DARK,&t)!=NX_OK) return false;
    const nx_color_token tokens[]={NX_COLOR_BACKGROUND,NX_COLOR_SURFACE,NX_COLOR_SURFACE_RAISED,
        NX_COLOR_TEXT,NX_COLOR_TEXT_MUTED,NX_COLOR_BORDER,NX_COLOR_ACCENT,NX_COLOR_ACCENT_TEXT,
        NX_COLOR_HOVER,NX_COLOR_PRESSED,NX_COLOR_FOCUS,NX_COLOR_SELECTION};
    const nx_color colors[]={p.canvas,p.canvas,p.card,p.text,p.secondary,p.line,p.accent,p.canvas,
        p.hover,p.selected,p.focus,p.selected};
    for(size_t i=0;i<sizeof(tokens)/sizeof(tokens[0]);++i)
        if(nx_theme_set_color(&t,tokens[i],colors[i])!=NX_OK) return false;
    return nx_context_set_theme(v->context,&t)==NX_OK;
}
bool ae_settings_view_create(struct ae_settings *model, const void *font, size_t length,
    struct ae_settings_view **out) {
    if(!out) return false;
    *out=NULL;
    if(!model || !font || !length) return false;
    struct ae_settings_view *v=calloc(1,sizeof(*v));
    if(!v) return false;
    v->model=model;
    if(nx_context_create(NULL,&v->context)!=NX_OK || nx_font_open(font,length,NULL,&v->font)!=NX_OK ||
       nx_font_collection_create(NULL,&v->fonts)!=NX_OK || nx_font_collection_add(v->fonts,v->font)!=NX_OK) {
        ae_settings_view_destroy(v);return false;
    }
    *out=v;return true;
}
void ae_settings_view_destroy(struct ae_settings_view *v) {
    if(!v) return;
    for(size_t i=0;i<LABELS;++i) nx_text_layout_destroy(v->labels[i].layout);
    nx_context_destroy(v->context);nx_font_collection_destroy(v->fonts);nx_font_close(v->font);free(v);
}
nx_context *ae_settings_view_context(struct ae_settings_view *v) {return v?v->context:NULL;}
enum ae_settings_window_action ae_settings_view_take_action(struct ae_settings_view *v) {
    if(!v) return AE_SETTINGS_WINDOW_NONE;
    enum ae_settings_window_action action=v->action;v->action=AE_SETTINGS_WINDOW_NONE;return action;
}
bool ae_settings_view_take_refresh(struct ae_settings_view *v) {
    if(!v) return false;
    bool refresh=v->refresh;v->refresh=false;return refresh;
}
static bool label(struct ae_settings_view *v, const char *text, float size, float width, size_t *index) {
    if(v->label_count==LABELS || !text || strlen(text)>=sizeof(v->labels[0].bytes) || width<=0) return false;
    *index=v->label_count++;
    struct label *l=&v->labels[*index];
    if(l->layout && l->size==size && l->width==width && !strcmp(l->bytes,text)) return true;
    nx_text_layout *candidate=NULL;
    if(nx_text_layout_create(v->fonts,text,strlen(text),NX_BIDI_LTR,size,width,&candidate)!=NX_OK) return false;
    if(nx_text_layout_width(candidate)>(double)width+0.01) {
        /* Emergency wrapping only for runs wider than the ordinary word wrapper.
           Keep semantic bytes intact; never split a grapheme for painting. */
        char wrapped[1024];size_t used=0, line=0, offset=0, length=strlen(text);
        nx_text_layout_destroy(candidate);candidate=NULL;
        while(offset<length) {
            size_t next;
            if(nx_grapheme_next(text,length,offset,&next)!=NX_OK || next<=offset ||
               used+(next-offset)+2>=sizeof(wrapped)) return false;
            size_t start=used;memcpy(wrapped+used,text+offset,next-offset);used+=next-offset;
            nx_text_layout *measure=NULL;
            if(nx_text_layout_create(v->fonts,wrapped+line,used-line,NX_BIDI_LTR,size,width,&measure)!=NX_OK) return false;
            bool over=nx_text_layout_width(measure)>(double)width+0.01;
            nx_text_layout_destroy(measure);
            if(over && start>line) {
                memmove(wrapped+start+1,wrapped+start,used-start);wrapped[start]='\n';++used;line=start+1;
            }
            if(text[offset]=='\n') line=used;
            offset=next;
        }
        if(nx_text_layout_create(v->fonts,wrapped,used,NX_BIDI_LTR,size,width,&candidate)!=NX_OK) return false;
    }
    nx_text_layout_destroy(l->layout);l->layout=candidate;
    memcpy(l->bytes,text,strlen(text)+1);l->width=width;l->size=size;
    return true;
}
static bool node(struct ae_settings_view *v, struct node n) {
    if(v->node_count==NODES) return false;
    v->nodes[v->node_count++]=n;return true;
}
static bool text(struct ae_settings_view *v, uint64_t id, uint64_t parent,
    const char *value, float size, float x, float *y, float width, bool secondary) {
    size_t i;
    if(!label(v,value,size,width,&i)) return false;
    float h=(float)nx_text_layout_height(v->labels[i].layout);
    if(!node(v,(struct node){.kind=TEXT,.id=id,.parent=parent,.rect={x,*y,width,h},.label=i,.secondary=secondary})) return false;
    *y+=h;return true;
}
static bool choice(struct ae_settings_view *v, enum node_kind kind, uint64_t id, uint64_t parent,
    const char *value, float size, nx_rect *rect, bool selected, bool disabled, nx_icon icon) {
    size_t i;
    float padding=kind==RADIO || icon!=NX_ICON_NONE?56:24;
    if(!label(v,value,size,fmaxf(1,rect->width-padding),&i)) return false;
    float h=(float)nx_text_layout_height(v->labels[i].layout)+24;
    if(rect->height<h) rect->height=h;
    return node(v,(struct node){.kind=kind,.id=id,.parent=parent,.rect=*rect,.label=i,
        .selected=selected,.disabled=disabled,.icon=icon});
}
static bool group(struct ae_settings_view *v, uint64_t id, float x, float y, float w, size_t *index) {
    *index=v->node_count;
    return node(v,(struct node){.kind=CARD,.id=id,.parent=5,.rect={x,y,w,0}});
}
static bool content(struct ae_settings_view *v, float width, float *extent) {
    struct ae_settings *m=v->model;
    float s=(float)m->text_percent/100, x=16, y=20, w=width-32;
    const char *titles[]={tr(v,"Displays","Bildschirme"),tr(v,"Appearance","Darstellung"),tr(v,"Language","Sprache")};
    const char *subtitles[]={tr(v,"Displays observed in this session","Beobachtete Bildschirme dieser Sitzung"),
        tr(v,"Colors and text for this window","Farben und Text für dieses Fenster"),
        tr(v,"Language for this window","Sprache für dieses Fenster")};
    if(!text(v,20,5,titles[m->page],25*s,x,&y,w,false)) return false;
    y+=8;
    if(!text(v,21,5,subtitles[m->page],13*s,x,&y,w,true)) return false;
    y+=24;
    size_t card;
    if(m->page==AE_SETTINGS_DISPLAY) {
        const char *status=m->stale?tr(v,"Refresh failed. Previous observations are shown.","Aktualisierung fehlgeschlagen. Der vorherige Stand bleibt sichtbar."):
            !m->refreshes?tr(v,"Display information is not available yet.","Anzeigeinformationen sind noch nicht verfügbar."):
            !m->count?tr(v,"No displays observed.","Keine Bildschirme beobachtet."):
            tr(v,"Select a display to see its information.","Wähle einen Bildschirm für seine Informationen.");
        if(!text(v,22,5,status,12*s,x,&y,w,true)) return false;
        y+=14;
        if(!group(v,30,x,y,w,&card)) return false;
        y+=8;
        for(size_t i=0;i<m->count;++i) {
            const struct ae_settings_display *d=&m->displays[i];char name[256];
            if(snprintf(name,sizeof(name),"%zu. %s",i+1,d->name)<0) return false;
            nx_rect r={x+8,y,w-16,60};
            if(!choice(v,RADIO,UINT64_C(0x100000000)+d->id,30,name,14*s,&r,i==m->selected,m->stale,NX_ICON_NONE)) return false;
            y+=r.height+8;
        }
        v->nodes[card].rect.height=y-v->nodes[card].rect.y;
        if(!m->count) {v->node_count=card;y-=8;}
        y+=20;
        if(m->count) {
            const struct ae_settings_display *d=&m->displays[m->selected];char mode[128], scale[128], rate[128];
            if(d->mode_known) (void)snprintf(mode,sizeof(mode),"%s  %d × %d",tr(v,"Resolution","Auflösung"),d->width,d->height);
            else (void)snprintf(mode,sizeof(mode),"%s",tr(v,"Resolution unavailable","Auflösung nicht verfügbar"));
            (void)snprintf(scale,sizeof(scale),"%s  %.0f %%",tr(v,"Content scale","Inhaltsskalierung"),(double)d->scale*100);
            if(d->refresh>0) (void)snprintf(rate,sizeof(rate),"%s  %.2f Hz",tr(v,"Refresh rate","Bildwiederholrate"),(double)d->refresh);
            else (void)snprintf(rate,sizeof(rate),"%s",tr(v,"Refresh rate unavailable","Bildwiederholrate nicht verfügbar"));
            if(!group(v,31,x,y,w,&card)) return false;
            y+=16;
            if(!text(v,32,31,mode,13*s,x+16,&y,w-32,false)) return false;
            y+=14;
            if(!text(v,33,31,scale,13*s,x+16,&y,w-32,false)) return false;
            y+=14;
            if(!text(v,34,31,rate,13*s,x+16,&y,w-32,false)) return false;
            y+=16;v->nodes[card].rect.height=y-v->nodes[card].rect.y;
        }
        y+=16;
        nx_rect refresh={x,y,w,40};
        if(!choice(v,BUTTON,40,5,tr(v,"Refresh","Aktualisieren"),13*s,&refresh,false,false,NX_ICON_NONE)) return false;
        y+=refresh.height+16;
        if(!text(v,41,5,tr(v,"Display changes are not available yet.","Bildschirmeinstellungen können noch nicht geändert werden."),12*s,x,&y,w,true)) return false;
    } else if(m->page==AE_SETTINGS_APPEARANCE) {
        if(!text(v,22,5,tr(v,"APPEARANCE","ERSCHEINUNGSBILD"),11*s,x,&y,w,true)) return false;
        y+=10;
        if(!group(v,30,x,y,w,&card)) return false;
        y+=8;
        const char *names[]={tr(v,"Dark","Dunkel"),tr(v,"Light","Hell"),tr(v,"High contrast","Hoher Kontrast")};
        for(unsigned i=0;i<3;++i) {
            nx_rect r={x+8,y,w-16,44};
            if(!choice(v,RADIO,100+i,30,names[i],14*s,&r,(unsigned)m->theme==i,false,NX_ICON_NONE)) return false;
            y+=r.height+4;
        }
        y+=4;v->nodes[card].rect.height=y-v->nodes[card].rect.y;
        y+=24;
        if(!text(v,23,5,tr(v,"TEXT SIZE","TEXTGRÖSSE"),11*s,x,&y,w,true)) return false;
        y+=10;
        if(!group(v,31,x,y,w,&card)) return false;
        y+=8;
        const char *sizes[]={"100 %","150 %","200 %"};const unsigned values[]={100,150,200};
        for(unsigned i=0;i<3;++i) {
            nx_rect r={x+8,y,w-16,44};
            if(!choice(v,RADIO,110+i,31,sizes[i],14*s,&r,m->text_percent==values[i],false,NX_ICON_NONE)) return false;
            y+=r.height+4;
        }
        y+=4;v->nodes[card].rect.height=y-v->nodes[card].rect.y;
    } else {
        if(!text(v,22,5,tr(v,"DISPLAY LANGUAGE","ANZEIGESPRACHE"),11*s,x,&y,w,true)) return false;
        y+=10;
        if(!group(v,30,x,y,w,&card)) return false;
        y+=8;
        const char *names[]={tr(v,"English","Englisch"),tr(v,"German","Deutsch")};
        for(unsigned i=0;i<2;++i) {
            nx_rect r={x+8,y,w-16,44};
            if(!choice(v,RADIO,120+i,30,names[i],14*s,&r,m->german==(i==1),false,NX_ICON_NONE)) return false;
            y+=r.height+4;
        }
        y+=4;v->nodes[card].rect.height=y-v->nodes[card].rect.y;
    }
    y+=24;
    if(!text(v,25,5,tr(v,"Appearance and language choices last until this window closes.",
        "Design und Sprache gelten für dieses Fenster, bis es geschlossen wird."),12*s,x,&y,w,true)) return false;
    *extent=y+24;return true;
}
static bool paint(struct ae_settings_view *v, struct node n, float dx, float dy, uint64_t *pressed) {
    nx_rect r=n.rect;r.x+=dx;r.y+=dy;
    struct palette p=v->colors;
    if(n.kind==CARD) {
        nx_semantic_desc sem={.id=n.id,.parent_id=n.parent,.role=NX_ROLE_GROUP,.bounds=r};
        return nx_semantics_add(v->context,&sem,NULL)==NX_OK &&
            nx_fill_round_rect(v->context,r,10,p.line)==NX_OK &&
            nx_fill_round_rect(v->context,(nx_rect){r.x+1,r.y+1,r.width-2,r.height-2},9,p.card)==NX_OK;
    }
    struct label *l=&v->labels[n.label];
    if(n.kind==TEXT) return nx_label(v->context,n.id,n.parent,r,l->layout,(nx_point){r.x,r.y},
        n.secondary?p.secondary:p.text,GLYPH_CACHE)==NX_OK;
    nx_widget_desc widget={.semantic={.id=n.id,.parent_id=n.parent,.bounds=r,.name=span(n.name?n.name:l->bytes),
        .role=n.kind==RADIO?NX_ROLE_RADIO:NX_ROLE_BUTTON,
        .state=(n.disabled?NX_SEM_DISABLED:0u)|(n.selected?(n.kind==RADIO?NX_SEM_CHECKED:NX_SEM_SELECTED):0u),
        .actions=NX_ACTION_PRESS|NX_ACTION_FOCUS},.flags=NX_WIDGET_INTERACTIVE|NX_WIDGET_FOCUSABLE};
    nx_interaction_state interaction;
    if(nx_widget_register(v->context,&widget,&interaction)!=NX_OK) return false;
    if(interaction.clicked && !n.disabled) *pressed=n.id;
    nx_color fill=n.selected?p.selected:interaction.held?p.selected:interaction.hovered?p.hover:n.kind==NAV?p.sidebar:p.card;
    if(nx_fill_round_rect(v->context,r,7,fill)!=NX_OK) return false;
    if(n.kind==BUTTON && nx_stroke_rect(v->context,r,1,p.line)!=NX_OK) return false;
    if(interaction.focused && nx_stroke_rect(v->context,r,2,p.focus)!=NX_OK) return false;
    float tx=r.x+12, ty=r.y+(r.height-(float)nx_text_layout_height(l->layout))/2;
    if(n.kind==RADIO) {
        nx_rect mark={r.x+12,r.y+r.height/2-7,14,14};
        if(nx_fill_round_rect(v->context,mark,7,p.accent)!=NX_OK ||
           nx_fill_round_rect(v->context,(nx_rect){mark.x+1,mark.y+1,12,12},6,fill)!=NX_OK) return false;
        if(n.selected && nx_fill_round_rect(v->context,(nx_rect){mark.x+4,mark.y+4,6,6},3,p.accent)!=NX_OK) return false;
        tx+=28;
    } else if(n.icon!=NX_ICON_NONE) {
        float icon_x=n.icon_only?r.x+r.width/2-8:r.x+12;
        if(nx_draw_icon(v->context,n.icon,(nx_rect){icon_x,r.y+r.height/2-8,16,16},1.5f,p.accent)!=NX_OK) return false;
        tx+=26;
    }
    if(n.icon_only) return true;
    return nx_draw_text_layout(v->context,l->layout,(nx_point){tx,ty},n.disabled?p.secondary:p.text,GLYPH_CACHE)==NX_OK;
}
bool ae_settings_view_frame(struct ae_settings_view *v, float width, float height,
    const nx_window_status *status, bool *changed) {
    if(!v || !changed || !status || !isfinite(width) || !isfinite(height) || width<320 || height<240) return false;
    *changed=false;
    struct ae_settings *m=v->model;
    if(m->page<AE_SETTINGS_DISPLAY || m->page>AE_SETTINGS_LANGUAGE || m->count>AE_SETTINGS_DISPLAYS ||
       (m->count && m->selected>=m->count) || m->theme<AE_SETTINGS_DARK || m->theme>AE_SETTINGS_CONTRAST ||
       (m->text_percent!=100 && m->text_percent!=150 && m->text_percent!=200)) return false;
    for(size_t i=0;i<m->count;++i)
        if(!m->displays[i].id || !memchr(m->displays[i].name,0,sizeof(m->displays[i].name))) return false;
    v->label_count=v->node_count=0;
    float s=(float)m->text_percent/100;
    bool narrow=width<640*s;
    float header=48*s, side=narrow?0:210*s;
    if(header>height-100) header=height-100;
    if(!palette(v) || nx_begin_frame(v->context,width,height,0)!=NX_OK) return false;
    nx_semantic_desc root={.id=1,.role=NX_ROLE_WINDOW,.bounds={0,0,width,height},.name=span(tr(v,"Settings","Einstellungen"))};
    nx_semantic_desc nav={.id=2,.parent_id=1,.role=NX_ROLE_GROUP,.bounds={0,header,narrow?width:side,height-header},
        .name=span(tr(v,"Settings sections","Einstellungsbereiche"))};
    if(nx_semantics_add(v->context,&root,NULL)!=NX_OK || nx_semantics_add(v->context,&nav,NULL)!=NX_OK ||
       nx_fill_rect(v->context,(nx_rect){0,0,width,height},v->colors.canvas)!=NX_OK ||
       nx_fill_rect(v->context,(nx_rect){0,header,side,height-header},v->colors.sidebar)!=NX_OK) return false;
    uint64_t pressed=0;
    float title_y=12*s;
    if(!narrow && !text(v,3,1,tr(v,"Settings","Einstellungen"),14*s,18,&title_y,width-170*s,false)) return false;
    nx_rect close={width-44*s,8*s,34*s,32*s};
    if(!choice(v,BUTTON,10,1,"×",14*s,&close,false,false,NX_ICON_NONE)) return false;
    v->nodes[v->node_count-1].name=tr(v,"Close","Schließen");
    /* Window operations remain supported requests; the host observes the result. */
    nx_rect zoom={width-82*s,8*s,34*s,32*s};
    if(!choice(v,BUTTON,11,1,"+",14*s,&zoom,false,
        !(status->capabilities&((status->state&NX_WINDOW_STATE_MAXIMIZED)?NX_WINDOW_CAP_RESTORE:NX_WINDOW_CAP_MAXIMIZE)),NX_ICON_NONE)) return false;
    v->nodes[v->node_count-1].name=(status->state&NX_WINDOW_STATE_MAXIMIZED)?tr(v,"Restore","Wiederherstellen"):tr(v,"Maximize","Maximieren");
    nx_rect minimize={width-120*s,8*s,34*s,32*s};
    if(!choice(v,BUTTON,12,1,"−",14*s,&minimize,false,!(status->capabilities&NX_WINDOW_CAP_MINIMIZE),NX_ICON_NONE)) return false;
    v->nodes[v->node_count-1].name=tr(v,"Minimize","Minimieren");
    float nav_y=header+18, nav_x=12, nav_width=narrow?(width-32)/3:side-24;
    const char *names[]={tr(v,"Displays","Bildschirme"),tr(v,"Appearance","Darstellung"),tr(v,"Language","Sprache")};
    const nx_icon icons[]={NX_ICON_GRID,NX_ICON_SUN,NX_ICON_SETTINGS};
    float nav_bottom=header;
    for(unsigned i=0;i<3;++i) {
        nx_rect r={nav_x,nav_y,nav_width,narrow?32:40*s};
        if(!choice(v,NAV,50+i,2,narrow?"":names[i],13*s,&r,(unsigned)m->page==i,false,icons[i])) return false;
        v->nodes[v->node_count-1].name=names[i];v->nodes[v->node_count-1].icon_only=narrow;
        if(r.y+r.height>nav_bottom) nav_bottom=r.y+r.height;
        if(narrow) nav_x+=nav_width+4;else nav_y+=r.height+8;
    }
    if(narrow) header=nav_bottom+12;
    size_t fixed_count=v->node_count;
    float cw=width-side, extent=0;
    if(!content(v,cw,&extent)) return false;
    nx_scroll_area_desc scroll={.id=5,.parent_id=1,.name=span(names[m->page]),
        .viewport={side,header,cw,height-header},.content_size={cw,extent},.axes=NX_SCROLL_Y,.wheel_step=48};
    uint64_t focus=nx_focus_id(v->context);
    bool reveal=focus!=v->last_focus || width!=v->last_width || height!=v->last_height ||
        m->text_percent!=v->last_text || m->german!=v->last_german || m->page!=v->last_page;
    if(m->page!=v->last_page) v->scroll_y=0;
    if(reveal) for(size_t i=fixed_count;i<v->node_count;++i) if(v->nodes[i].id==focus) {
        nx_rect r=v->nodes[i].rect;
        if(r.y<v->scroll_y) v->scroll_y=r.y;
        else if(r.y+r.height>v->scroll_y+scroll.viewport.height) v->scroll_y=r.y+r.height-scroll.viewport.height;
    }
    /* Read-only help remains reachable without moving focus or activating a
       choice. A later Tab returns to focus-following navigation. */
    if(nx_key_pressed(v->context,NX_KEY_PAGE_DOWN)) v->scroll_y+=scroll.viewport.height*0.8f;
    if(nx_key_pressed(v->context,NX_KEY_PAGE_UP)) v->scroll_y-=scroll.viewport.height*0.8f;
    if(nx_key_pressed(v->context,NX_KEY_HOME)) v->scroll_y=0;
    if(nx_key_pressed(v->context,NX_KEY_END)) v->scroll_y=extent;
    v->last_focus=focus;v->last_width=width;v->last_height=height;v->last_text=m->text_percent;
    v->last_german=m->german;v->last_page=m->page;
    /* Fixed regions precede the scroll tree, preserving deterministic Tab order. */
    for(size_t i=0;i<fixed_count;++i) if(!paint(v,v->nodes[i],0,0,&pressed)) return false;
    nx_scroll_info info;
    if(nx_scroll_area_begin(v->context,&scroll,&v->scroll_x,&v->scroll_y,&info)!=NX_OK) return false;
    for(size_t i=fixed_count;i<v->node_count;++i)
        if(!paint(v,v->nodes[i],side,header-v->scroll_y,&pressed)) return false;
    if(nx_scroll_area_end(v->context)!=NX_OK || nx_end_frame(v->context)!=NX_OK) return false;
    if(pressed==10) v->action=AE_SETTINGS_WINDOW_CLOSE;
    else if(pressed==11) v->action=AE_SETTINGS_WINDOW_ZOOM;
    else if(pressed==12) v->action=AE_SETTINGS_WINDOW_MINIMIZE;
    else if(pressed>=50 && pressed<=52) {m->page=(enum ae_settings_page)(pressed-50);*changed=true;}
    else if(pressed>=100 && pressed<=102) {m->theme=(enum ae_settings_theme)(pressed-100);*changed=true;}
    else if(pressed>=110 && pressed<=112) {const unsigned values[]={100,150,200};m->text_percent=values[pressed-110];*changed=true;}
    else if(pressed==120 || pressed==121) {m->german=pressed==121;*changed=true;}
    else if(pressed==40) v->refresh=true;
    else if(pressed>UINT64_C(0x100000000) && pressed<=UINT64_C(0x1ffffffff)) {
        if(!ae_settings_select(m,(uint32_t)(pressed-UINT64_C(0x100000000)))) return false;
        *changed=true;
    }
    return true;
}
