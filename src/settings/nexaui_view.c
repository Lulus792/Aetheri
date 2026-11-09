#include "settings/nexaui_view.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Local, semantic tokens for the accepted Settings study's opaque fallback.
   No compositor blur or backdrop permission is inferred from a painted panel. */
struct palette { nx_color canvas, sidebar, card, text, secondary, line, accent, selected, hover, focus; };
struct label { char bytes[512]; float width, size; nx_text_layout *layout; };
enum node_kind { TEXT, CARD, BUTTON, RADIO, NAV, CHROME, VALUE, TOGGLE };
struct node {
    enum node_kind kind;
    uint64_t id, parent;
    nx_rect rect;
    size_t label;
    nx_icon icon;
    const char *name;
    bool selected, disabled, secondary, icon_only;
    uint64_t target_id;
    double value;
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
    uint64_t sound_drag_id,sound_target;
    double sound_draft,sound_volume;
    enum ae_audio_command_kind sound_kind;
    bool sound_intent,sound_mute,sound_redraw;
};
static nx_utf8_span span(const char *s) { return (nx_utf8_span){s, strlen(s)}; }
static nx_color rgb(unsigned value) {
    return (nx_color){(float)((value>>16)&255u)/255, (float)((value>>8)&255u)/255,
        (float)(value&255u)/255, 1};
}
static const char *tr(const struct ae_settings_view *v, const char *en, const char *de) {
    return v->model->german?de:en;
}
bool ae_settings_chrome_layout(const struct ae_settings *m, float width, float height,
    struct ae_settings_chrome *out) {
    if(!out) return false;
    *out=(struct ae_settings_chrome){0};
    if(!m || !isfinite(width) || !isfinite(height) || width<64 || height<64 ||
       (m->text_percent!=100 && m->text_percent!=150 && m->text_percent!=200)) return false;
    float s=(float)m->text_percent/100, header=fmaxf(32,12+13*s), top=(header-26)/2;
    *out=(struct ae_settings_chrome){.titlebar={0,0,width,header},.title={102,0,width-114,header},
        .close={8,top,26,26},.minimize={36,top,26,26},.zoom={64,top,26,26},.resize_margin=6};
    if(width<320 || height<240) {out->minimize=(nx_rect){0};out->zoom=(nx_rect){0};out->title=(nx_rect){0};}
    return true;
}
static bool contains(nx_rect r,nx_point p) {
    return p.x>=r.x && p.x<r.x+r.width && p.y>=r.y && p.y<r.y+r.height;
}
bool ae_settings_chrome_hit(const struct ae_settings *m, float width, float height,
    const nx_window_status *status, nx_point p, struct ae_settings_hit *out) {
    if(!out) return false;
    *out=(struct ae_settings_hit){.cursor=NX_CURSOR_DEFAULT};
    struct ae_settings_chrome c;
    if(!status || !isfinite(p.x) || !isfinite(p.y) ||
       !ae_settings_chrome_layout(m,width,height,&c)) return false;
    if(p.x<0 || p.y<0 || p.x>=width || p.y>=height ||
       contains(c.close,p) || contains(c.minimize,p) || contains(c.zoom,p)) return true;
    bool normal=(status->state&(NX_WINDOW_STATE_MAXIMIZED|NX_WINDOW_STATE_FULLSCREEN))==0;
    if(normal && (status->capabilities&NX_WINDOW_CAP_RESIZE)) {
        bool left=p.x<c.resize_margin,right=p.x>=width-c.resize_margin;
        bool top=p.y<c.resize_margin,bottom=p.y>=height-c.resize_margin;
        if(left || right || top || bottom) {
            out->kind=AE_SETTINGS_HIT_RESIZE;
            if(left && top) out->edge=NX_WINDOW_RESIZE_TOP_LEFT;
            else if(right && top) out->edge=NX_WINDOW_RESIZE_TOP_RIGHT;
            else if(left && bottom) out->edge=NX_WINDOW_RESIZE_BOTTOM_LEFT;
            else if(right && bottom) out->edge=NX_WINDOW_RESIZE_BOTTOM_RIGHT;
            else out->edge=left?NX_WINDOW_RESIZE_LEFT:right?NX_WINDOW_RESIZE_RIGHT:top?NX_WINDOW_RESIZE_TOP:NX_WINDOW_RESIZE_BOTTOM;
            out->cursor=(left||right)&&(top||bottom)?((left&&top)||(right&&bottom)?NX_CURSOR_RESIZE_DIAGONAL_NW_SE:NX_CURSOR_RESIZE_DIAGONAL_NE_SW):
                (left||right)?NX_CURSOR_RESIZE_HORIZONTAL:NX_CURSOR_RESIZE_VERTICAL;
            return true;
        }
    }
    if(!(status->state&NX_WINDOW_STATE_FULLSCREEN) && contains(c.titlebar,p) &&
       (status->capabilities&(NX_WINDOW_CAP_MOVE|NX_WINDOW_CAP_MAXIMIZE|NX_WINDOW_CAP_RESTORE))) out->kind=AE_SETTINGS_HIT_TITLE;
    return true;
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
    float h=(*value || icon==NX_ICON_NONE)?(float)nx_text_layout_height(v->labels[i].layout)+24:0;
    if(rect->height<h) rect->height=h;
    return node(v,(struct node){.kind=kind,.id=id,.parent=parent,.rect=*rect,.label=i,
        .selected=selected,.disabled=disabled,.icon=icon});
}
static bool group(struct ae_settings_view *v, uint64_t id, float x, float y, float w, size_t *index) {
    *index=v->node_count;
    return node(v,(struct node){.kind=CARD,.id=id,.parent=5,.rect={x,y,w,0}});
}
static bool sound_content(struct ae_settings_view *v,float x,float *y,float w,float s) {
    struct ae_settings_sound *audio=&v->model->sound;
    const char *status=audio->fenced?tr(v,"Audio connection lost. Changes are disabled.","Audioverbindung verloren. Änderungen sind gesperrt."):
        audio->stale?tr(v,"Audio information could not be refreshed.","Audioinformationen konnten nicht aktualisiert werden."):
        !audio->attached?tr(v,"Audio is unavailable in this session.","Audio ist in dieser Sitzung nicht verfügbar."):
        audio->observation.state==AE_AUDIO_CONNECTING?tr(v,"Connecting to audio…","Verbindung mit Audio wird hergestellt…"):
        !audio->observation.count?tr(v,"No audio outputs are available.","Keine Audioausgänge verfügbar."):
        audio->feedback==AE_SETTINGS_SOUND_UNKNOWN?tr(v,"The outcome of the last change is unknown.","Das Ergebnis der letzten Änderung ist unbekannt."):
        audio->feedback==AE_SETTINGS_SOUND_QUEUED || audio->feedback==AE_SETTINGS_SOUND_SUBMITTED || audio->observation.command_state==AE_AUDIO_COMMAND_PENDING?
            tr(v,"Applying change…","Änderung wird angewendet…"):
        audio->feedback==AE_SETTINGS_SOUND_REJECTED?tr(v,"The change was not submitted. Check the current state and try again.","Die Änderung wurde nicht übermittelt. Prüfe den aktuellen Stand und versuche es erneut."):
        audio->feedback==AE_SETTINGS_SOUND_OBSERVED?tr(v,"Change confirmed by the audio service.","Änderung vom Audiodienst bestätigt."):
        tr(v,"Choose where sound plays.","Wähle, wo Ton wiedergegeben wird.");
    if(!text(v,22,5,status,13*s,x,y,w,true)) return false;
    *y+=20;
    if(!audio->attached || audio->fenced || audio->observation.state!=AE_AUDIO_READY) return true;
    size_t card,selected=AE_AUDIO_OUTPUTS_MAX;
    bool enabled=ae_settings_sound_enabled(v->model);
    if(audio->observation.count) {
        if(!group(v,30,x,*y,w,&card)) return false;
        *y+=8;
        for(size_t i=0;i<audio->observation.count;++i) {
            const struct ae_audio_output *o=&audio->observation.outputs[i];
            nx_rect r={x+8,*y,w-16,52*s};
            const char *description=o->description[0]?o->description:o->name;
            if(!choice(v,RADIO,audio->ids[i],30,description,14*s,&r,o->is_default,!enabled,NX_ICON_NONE)) return false;
            if(o->is_default) selected=i;
            *y+=r.height+8;
        }
        v->nodes[card].rect.height=*y-v->nodes[card].rect.y;
    }
    *y+=20;
    if(selected==AE_AUDIO_OUTPUTS_MAX) return text(v,23,5,tr(v,"Choose an output to adjust its volume.","Wähle einen Ausgang, um seine Lautstärke einzustellen."),13*s,x,y,w,true);
    const struct ae_audio_output *o=&audio->observation.outputs[selected];
    if(!o->volume_available) return text(v,23,5,tr(v,"Volume control is unavailable for this output.","Für diesen Ausgang ist keine Lautstärkeregelung verfügbar."),13*s,x,y,w,true);
    if(!group(v,31,x,*y,w,&card)) return false;
    *y+=16;
    char volume[128];
    double shown=v->sound_drag_id==audio->ids[selected]+1?v->sound_draft:o->volume;
    if(shown>10) (void)snprintf(volume,sizeof(volume),"%s",tr(v,"Volume above 1000 %","Lautstärke über 1000 %"));
    else (void)snprintf(volume,sizeof(volume),"%s   %.0f %%",tr(v,"Volume","Lautstärke"),shown*100);
    if(!text(v,23,31,volume,14*s,x+16,y,w-32,false)) return false;
    *y+=12;
    if(!node(v,(struct node){.kind=VALUE,.id=audio->ids[selected]+1,.target_id=audio->ids[selected],.parent=31,
            .rect={x+16,*y,w-32,40*s},.name=tr(v,"Volume","Lautstärke"),.value=fmin(o->volume,1),.disabled=!enabled || o->muted})) return false;
    *y+=40*s+12;
    size_t label_id;
    if(!label(v,tr(v,"Mute output","Ausgabe stummschalten"),13*s,w-48-36*s,&label_id) ||
            !node(v,(struct node){.kind=TOGGLE,.id=audio->ids[selected]+2,.target_id=audio->ids[selected],.parent=31,
            .rect={x+16,*y,w-32,fmaxf(44*s,(float)nx_text_layout_height(v->labels[label_id].layout)+24)},
            .label=label_id,.name=tr(v,"Mute output","Ausgabe stummschalten"),.selected=o->muted,.disabled=!enabled})) return false;
    *y+=v->nodes[v->node_count-1].rect.height+16;
    v->nodes[card].rect.height=*y-v->nodes[card].rect.y;
    return true;
}
static bool content(struct ae_settings_view *v, float width, float *extent) {
    struct ae_settings *m=v->model;
    float s=(float)m->text_percent/100, x=16, y=20, w=width-32;
    const char *titles[]={tr(v,"Displays","Bildschirme"),tr(v,"Appearance","Darstellung"),tr(v,"Language","Sprache"),tr(v,"Sound","Ton")};
    const char *subtitles[]={tr(v,"Displays observed in this session","Beobachtete Bildschirme dieser Sitzung"),
        tr(v,"Colors and text for this window","Farben und Text für dieses Fenster"),
        tr(v,"Language for this window","Sprache für dieses Fenster"),tr(v,"Output and volume","Ausgabe und Lautstärke")};
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
    } else if(m->page==AE_SETTINGS_SOUND) {
        if(!sound_content(v,x,&y,w,s)) return false;
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
    const char *footer=m->page==AE_SETTINGS_SOUND?
        tr(v,"Microphone and app mixer controls are not available yet.","Mikrofon- und App-Mixer-Einstellungen sind noch nicht verfügbar."):
        tr(v,"Appearance and language choices last until this window closes.","Design und Sprache gelten für dieses Fenster, bis es geschlossen wird.");
    if(!text(v,25,5,footer,12*s,x,&y,w,true)) return false;
    *extent=y+24;return true;
}
static bool paint(struct ae_settings_view *v, struct node n, float dx, float dy, uint64_t *pressed) {
    nx_rect r=n.rect;r.x+=dx;r.y+=dy;
    struct palette p=v->colors;
    if(n.kind==VALUE || n.kind==TOGGLE) {
        nx_control_desc desc={.id=n.id,.parent_id=n.parent,.bounds=r,.name=span(n.name),.disabled=n.disabled};
        nx_interaction_state state;
        if(n.kind==TOGGLE) {
            struct label *l=&v->labels[n.label];
            nx_widget_desc widget={.semantic={.id=n.id,.parent_id=n.parent,.bounds=r,.name=span(n.name),.role=NX_ROLE_SWITCH,
                .state=(n.selected?NX_SEM_CHECKED:0u)|(n.disabled?NX_SEM_DISABLED:0u),.actions=NX_ACTION_PRESS|NX_ACTION_FOCUS},
                .flags=NX_WIDGET_INTERACTIVE|NX_WIDGET_FOCUSABLE};
            if(nx_widget_register(v->context,&widget,&state)!=NX_OK) return false;
            float scale=(float)v->model->text_percent/100;
            nx_rect track={r.x+r.width-36*scale,r.y+r.height/2-10*scale,36*scale,20*scale};
            nx_rect thumb={track.x+(n.selected?18:2)*scale,track.y+2*scale,16*scale,16*scale};
            if(nx_fill_round_rect(v->context,track,10*scale,n.disabled?p.line:n.selected?p.accent:p.line)!=NX_OK ||
                    nx_fill_round_rect(v->context,thumb,8*scale,n.selected?p.canvas:p.text)!=NX_OK ||
                    nx_draw_text_layout(v->context,l->layout,(nx_point){r.x,r.y+(r.height-(float)nx_text_layout_height(l->layout))/2},n.disabled?p.secondary:p.text,GLYPH_CACHE)!=NX_OK ||
                    (state.focused && nx_stroke_rect(v->context,r,2,p.focus)!=NX_OK)) return false;
            if(state.clicked && !n.disabled && !v->sound_intent) {v->sound_intent=true;v->sound_target=n.target_id;v->sound_kind=AE_AUDIO_MUTE;v->sound_mute=!n.selected;}
        } else {
            if(v->sound_drag_id!=n.id || n.disabled) {v->sound_drag_id=0;v->sound_draft=n.value;}
            double value=v->sound_drag_id?v->sound_draft:n.value,before=value;
            float scale=(float)v->model->text_percent/100;
            desc.bounds.x+=8*scale;desc.bounds.width-=16*scale;
            nx_value_control_desc slider={.control=desc,.minimum=0,.maximum=100,.step=1,.units_per_pixel=1};
            value*=100;
            if(nx_slider(v->context,&slider,&value,&state)!=NX_OK) return false;
            const nx_event *events=NULL;size_t event_count=0;bool cancelled=false;
            if(nx_frame_events(v->context,&events,&event_count)!=NX_OK) return false;
            for(size_t i=0;i<event_count;++i)
                if(events[i].type==NX_EVENT_POINTER_CANCEL || (events[i].type==NX_EVENT_FOCUS && !events[i].focused)) cancelled=true;
            if(cancelled) {value=n.value*100;state.held=0;v->sound_redraw=true;}
            /* Keep the toolkit's native input, value semantics and full-height
             * target; skin its default filled bar with our approved thin track. */
            nx_rect track={desc.bounds.x,r.y+r.height/2-2*scale,desc.bounds.width,4*scale};
            nx_rect thumb={track.x+(float)(value/100)*track.width-8*scale,r.y+r.height/2-8*scale,16*scale,16*scale};
            if(nx_fill_rect(v->context,r,p.card)!=NX_OK || nx_fill_round_rect(v->context,track,2*scale,p.line)!=NX_OK ||
                    nx_fill_round_rect(v->context,(nx_rect){track.x,track.y,(float)(value/100)*track.width,track.height},2*scale,n.disabled?p.line:p.accent)!=NX_OK ||
                    nx_fill_round_rect(v->context,thumb,8*scale,n.disabled?p.secondary:p.text)!=NX_OK ||
                    (state.focused && nx_stroke_rect(v->context,desc.bounds,2,p.focus)!=NX_OK)) return false;
            value/=100;
            if(!cancelled && !n.disabled && !v->sound_intent && (state.pointer_released || (!state.held && value!=before))) {
                v->sound_intent=true;v->sound_target=n.target_id;v->sound_kind=AE_AUDIO_VOLUME;v->sound_volume=value;
            }
            if(state.held && value!=before) v->sound_redraw=true;
            v->sound_drag_id=!n.disabled && state.held?n.id:0;v->sound_draft=value;
        }
        return true;
    }
    if(n.kind==CARD) {
        nx_semantic_desc sem={.id=n.id,.parent_id=n.parent,.role=NX_ROLE_GROUP,.bounds=r};
        return nx_semantics_add(v->context,&sem,NULL)==NX_OK &&
            nx_fill_round_rect(v->context,r,10,p.line)==NX_OK &&
            nx_fill_round_rect(v->context,(nx_rect){r.x+1,r.y+1,r.width-2,r.height-2},9,p.card)==NX_OK;
    }
    struct label *l=n.kind==CHROME?NULL:&v->labels[n.label];
    if(n.kind==TEXT) return nx_label(v->context,n.id,n.parent,r,l->layout,(nx_point){r.x,r.y},
        n.secondary?p.secondary:p.text,GLYPH_CACHE)==NX_OK;
    nx_widget_desc widget={.semantic={.id=n.id,.parent_id=n.parent,.bounds=r,.name=span(n.name?n.name:l->bytes),
        .role=n.kind==RADIO?NX_ROLE_RADIO:NX_ROLE_BUTTON,
        .state=(n.disabled?NX_SEM_DISABLED:0u)|(n.selected?(n.kind==RADIO?NX_SEM_CHECKED:n.kind==NAV?NX_SEM_SELECTED:0u):0u),
        .actions=NX_ACTION_PRESS|NX_ACTION_FOCUS},.flags=NX_WIDGET_INTERACTIVE|NX_WIDGET_FOCUSABLE};
    nx_interaction_state interaction;
    if(nx_widget_register(v->context,&widget,&interaction)!=NX_OK) return false;
    if(interaction.clicked && !n.disabled) *pressed=n.id;
    if(n.kind==CHROME) {
        if((interaction.hovered || interaction.held) && nx_fill_round_rect(v->context,r,6,interaction.held?p.selected:p.hover)!=NX_OK) return false;
        if(interaction.focused && nx_stroke_rect(v->context,r,2,p.focus)!=NX_OK) return false;
        nx_color dot=n.disabled?rgb(0x747b87):n.id==10?rgb(0xb97e7d):n.id==12?rgb(0xbdab7f):rgb(0x829e8b);
        if(v->model->theme==AE_SETTINGS_CONTRAST && !n.disabled) dot=p.text;
        nx_color ink=rgb(0x20242c);
        if(v->model->theme==AE_SETTINGS_CONTRAST) ink=p.canvas;
        float cx=r.x+r.width/2,cy=r.y+r.height/2;
        if(nx_fill_round_rect(v->context,(nx_rect){cx-6,cy-6,12,12},6,dot)!=NX_OK) return false;
        if(n.id==10) return nx_draw_line(v->context,(nx_point){cx-2,cy-2},(nx_point){cx+2,cy+2},1,ink)==NX_OK &&
            nx_draw_line(v->context,(nx_point){cx-2,cy+2},(nx_point){cx+2,cy-2},1,ink)==NX_OK;
        if(n.id==12) return nx_draw_line(v->context,(nx_point){cx-3,cy},(nx_point){cx+3,cy},1,ink)==NX_OK;
        if(n.selected && nx_stroke_rect(v->context,(nx_rect){cx-1,cy-3,4,4},1,ink)!=NX_OK) return false;
        return nx_stroke_rect(v->context,(nx_rect){cx-3,cy-1,4,4},1,ink)==NX_OK;
    }
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
        if(n.kind==NAV && n.id==53) {
            float cy=r.y+r.height/2;
            const nx_point points[]={{icon_x+1,cy-3},{icon_x+5,cy-3},{icon_x+10,cy-7},{icon_x+10,cy+7},{icon_x+5,cy+3},{icon_x+1,cy+3},{icon_x+1,cy-3}};
            for(size_t i=1;i<sizeof(points)/sizeof(points[0]);++i)
                if(nx_draw_line(v->context,points[i-1],points[i],1.5f,p.accent)!=NX_OK) return false;
            if(nx_draw_line(v->context,(nx_point){icon_x+13,cy-4},(nx_point){icon_x+15,cy},1.5f,p.accent)!=NX_OK ||
                    nx_draw_line(v->context,(nx_point){icon_x+15,cy},(nx_point){icon_x+13,cy+4},1.5f,p.accent)!=NX_OK) return false;
        } else if(nx_draw_icon(v->context,n.icon,(nx_rect){icon_x,r.y+r.height/2-8,16,16},1.5f,p.accent)!=NX_OK) return false;
        tx+=26;
    }
    if(n.icon_only) return true;
    return nx_draw_text_layout(v->context,l->layout,(nx_point){tx,ty},n.disabled?p.secondary:p.text,GLYPH_CACHE)==NX_OK;
}
static bool small_frame(struct ae_settings_view *v,float width,float height) {
    struct ae_settings_chrome c;
    if(!ae_settings_chrome_layout(v->model,width,height,&c) || !palette(v) ||
       nx_begin_frame(v->context,width,height,0)!=NX_OK) return false;
    nx_semantic_desc root={.id=1,.role=NX_ROLE_WINDOW,.bounds={0,0,width,height},.name=span(tr(v,"Settings","Einstellungen"))};
    if(nx_semantics_add(v->context,&root,NULL)!=NX_OK ||
       nx_fill_rect(v->context,root.bounds,v->colors.canvas)!=NX_OK) return false;
    uint64_t pressed=0;
    if(!paint(v,(struct node){.kind=CHROME,.id=10,.parent=1,.rect=c.close,.name=tr(v,"Close","Schließen")},0,0,&pressed)) return false;
    float y=c.titlebar.height+8;
    if(!text(v,20,1,tr(v,"Enlarge the window to continue.","Vergrößere das Fenster, um fortzufahren."),
        11*(float)v->model->text_percent/100,12,&y,width-24,true) ||
       nx_push_clip(v->context,(nx_rect){0,c.titlebar.height,width,height-c.titlebar.height})!=NX_OK ||
       !paint(v,v->nodes[0],0,0,&pressed) || nx_pop_clip(v->context)!=NX_OK || nx_end_frame(v->context)!=NX_OK) return false;
    if(pressed==10) v->action=AE_SETTINGS_WINDOW_CLOSE;
    return true;
}
bool ae_settings_view_frame(struct ae_settings_view *v, float width, float height,
    const nx_window_status *status, bool *changed) {
    if(!v || !changed || !status || !isfinite(width) || !isfinite(height) || width<64 || height<64) return false;
    *changed=false;
    struct ae_settings *m=v->model;
    if(m->page<AE_SETTINGS_DISPLAY || m->page>AE_SETTINGS_SOUND || m->count>AE_SETTINGS_DISPLAYS || m->sound.observation.count>AE_AUDIO_OUTPUTS_MAX ||
       (m->count && m->selected>=m->count) || m->theme<AE_SETTINGS_DARK || m->theme>AE_SETTINGS_CONTRAST ||
       (m->text_percent!=100 && m->text_percent!=150 && m->text_percent!=200)) return false;
    for(size_t i=0;i<m->count;++i)
        if(!m->displays[i].id || !memchr(m->displays[i].name,0,sizeof(m->displays[i].name))) return false;
    v->label_count=v->node_count=0;
    v->sound_intent=false;v->sound_redraw=false;
    if(width<320 || height<240) return small_frame(v,width,height);
    float s=(float)m->text_percent/100;
    bool narrow=width<640*s;
    struct ae_settings_chrome chrome;
    if(!ae_settings_chrome_layout(m,width,height,&chrome)) return false;
    float header=chrome.titlebar.height, side=narrow?0:210*s;
    if(!palette(v) || nx_begin_frame(v->context,width,height,0)!=NX_OK) return false;
    nx_semantic_desc root={.id=1,.role=NX_ROLE_WINDOW,.bounds={0,0,width,height},.name=span(tr(v,"Settings","Einstellungen"))};
    nx_semantic_desc nav={.id=2,.parent_id=1,.role=NX_ROLE_GROUP,.bounds={0,header,narrow?width:side,height-header},
        .name=span(tr(v,"Settings sections","Einstellungsbereiche"))};
    if(nx_semantics_add(v->context,&root,NULL)!=NX_OK || nx_semantics_add(v->context,&nav,NULL)!=NX_OK ||
       nx_fill_rect(v->context,(nx_rect){0,0,width,height},v->colors.canvas)!=NX_OK ||
       nx_fill_rect(v->context,chrome.titlebar,v->colors.sidebar)!=NX_OK ||
       nx_fill_rect(v->context,(nx_rect){0,header,side,height-header},v->colors.sidebar)!=NX_OK) return false;
    uint64_t pressed=0;
    size_t title_label;
    if(!label(v,tr(v,"Settings","Einstellungen"),11*s,chrome.title.width,&title_label)) return false;
    nx_rect title=chrome.title;title.y=(header-(float)nx_text_layout_height(v->labels[title_label].layout))/2;
    title.height=(float)nx_text_layout_height(v->labels[title_label].layout);
    if(!node(v,(struct node){.kind=TEXT,.id=3,.parent=1,.rect=title,.label=title_label}) ||
       !node(v,(struct node){.kind=CHROME,.id=10,.parent=1,.rect=chrome.close,.name=tr(v,"Close","Schließen")}) ||
       !node(v,(struct node){.kind=CHROME,.id=12,.parent=1,.rect=chrome.minimize,.name=tr(v,"Minimize","Minimieren"),
                           .disabled=!(status->capabilities&NX_WINDOW_CAP_MINIMIZE)})) return false;
    /* Window operations remain supported requests; the host observes the result. */
    bool restore=(status->state&(NX_WINDOW_STATE_MAXIMIZED|NX_WINDOW_STATE_FULLSCREEN))!=0;
    if(!node(v,(struct node){.kind=CHROME,.id=11,.parent=1,.rect=chrome.zoom,
        .name=restore?tr(v,"Restore","Wiederherstellen"):tr(v,"Maximize","Maximieren"),.selected=restore,
        .disabled=!(status->capabilities&(restore?NX_WINDOW_CAP_RESTORE:NX_WINDOW_CAP_MAXIMIZE))})) return false;
    float nav_y=header+18, nav_x=12, nav_width=narrow?(width-36)/4:side-24;
    const char *names[]={tr(v,"Displays","Bildschirme"),tr(v,"Appearance","Darstellung"),tr(v,"Language","Sprache"),tr(v,"Sound","Ton")};
    const nx_icon icons[]={NX_ICON_GRID,NX_ICON_SUN,NX_ICON_SETTINGS,NX_ICON_SETTINGS};
    float nav_bottom=header;
    for(unsigned i=0;i<4;++i) {
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
    else if(pressed>=50 && pressed<=53) {m->page=(enum ae_settings_page)(pressed-50);v->sound_drag_id=0;*changed=true;}
    else if(pressed>=100 && pressed<=102) {m->theme=(enum ae_settings_theme)(pressed-100);*changed=true;}
    else if(pressed>=110 && pressed<=112) {const unsigned values[]={100,150,200};m->text_percent=values[pressed-110];*changed=true;}
    else if(pressed==120 || pressed==121) {m->german=pressed==121;*changed=true;}
    else if(pressed==40) v->refresh=true;
    else if(pressed>UINT64_C(0x100000000) && pressed<=UINT64_C(0x1ffffffff)) {
        if(!ae_settings_select(m,(uint32_t)(pressed-UINT64_C(0x100000000)))) return false;
        *changed=true;
    }
    else if(pressed>=UINT64_C(0x200000000) && m->page==AE_SETTINGS_SOUND) {
        if(ae_settings_sound_request(m,pressed,AE_AUDIO_DEFAULT,0,false)) *changed=true;
    }
    if(v->sound_intent && ae_settings_sound_request(m,v->sound_target,v->sound_kind,v->sound_volume,v->sound_mute)) *changed=true;
    if(v->sound_redraw) *changed=true;
    return true;
}
