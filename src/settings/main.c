/* Development view: no installed app admission or system-setting mutations. */
#define _POSIX_C_SOURCE 200809L
#include "model.h"
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

struct control { unsigned id; SDL_FRect rect; bool content; };
struct palette { SDL_Color bg, panel, text, secondary, line, accent, selected; };
struct app {
    SDL_Window *window;
    SDL_Renderer *renderer;
    TTF_Font *font;
    struct ae_settings model;
    struct control controls[64];
    size_t count;
    unsigned focus, pressed, hover;
    float density, text_scale, scroll, max_scroll;
    SDL_FRect viewport;
    struct palette colors;
    bool running, dirty, diagnostics, failed, reveal_focus;
    unsigned long frames;
};
static void error(struct app *a, const char *what);
static float measure(struct app *a, const char *s, float width, float size) {
    int w=0,h=0;
    if (width<1 || !TTF_SetFontSize(a->font,size*a->text_scale*a->density) ||
            !TTF_GetStringSizeWrapped(a->font,s,0,(int)floorf(width*a->density),&w,&h)) {
        error(a,"text measurement"); return 0;
    }
    return (float)h/a->density;
}
static const char *tr(const struct app *a, const char *en, const char *de) { return a->model.german ? de : en; }
static void error(struct app *a, const char *what) {
    fprintf(stderr, "settings: %s: %s\n", what, SDL_GetError());
    a->failed = true; a->running = false;
}
static void color(struct app *a, SDL_Color c) {
    if (!SDL_SetRenderDrawColor(a->renderer,c.r,c.g,c.b,c.a)) error(a,"draw color");
}
static void fill(struct app *a, SDL_FRect r, SDL_Color c) {
    color(a,c);
    if (!SDL_RenderFillRect(a->renderer,&r)) error(a,"rectangle");
}
static float text(struct app *a, const char *s, float x, float y, float width, float size, SDL_Color c) {
    if (width < 1 || width*a->density > 32768 || !TTF_SetFontSize(a->font,size*a->text_scale*a->density)) {
        error(a,"text size"); return 0;
    }
    SDL_Surface *surface = TTF_RenderText_Blended_Wrapped(a->font,s,0,c,(int)floorf(width*a->density));
    if (!surface) { error(a,"text rasterization"); return 0; }
    float h = (float)surface->h / a->density;
    SDL_Texture *texture = SDL_CreateTextureFromSurface(a->renderer,surface);
    SDL_FRect dest = {x,y,(float)surface->w/a->density,h};
    SDL_DestroySurface(surface);
    if (!texture) { error(a,"text texture"); return 0; }
    if (!SDL_RenderTexture(a->renderer,texture,NULL,&dest)) error(a,"text presentation");
    SDL_DestroyTexture(texture);
    return h;
}
static bool inside(SDL_FRect r, float x, float y) { return x>=r.x && x<r.x+r.w && y>=r.y && y<r.y+r.h; }
static void button(struct app *a, unsigned id, const char *label, SDL_FRect r, bool selected, bool content) {
    if (a->count >= SDL_arraysize(a->controls)) { error(a,"control bound"); return; }
    a->controls[a->count++] = (struct control){id,r,content};
    fill(a,r,(selected || a->pressed==id) ? a->colors.selected : a->colors.panel);
    if (a->focus==id || a->hover==id || a->pressed==id) {
        color(a,a->focus==id || a->pressed==id ? a->colors.accent : a->colors.secondary);
        SDL_FRect focus={r.x+1,r.y+1,r.w-2,r.h-2};
        if (!SDL_RenderRect(a->renderer,&focus)) error(a,"focus ring");
    }
    text(a,label,r.x+12,r.y+10,r.w-24,16,a->colors.text);
}
static void refresh(struct app *a) {
    uint32_t focused=0;
    if (a->focus>=100 && a->focus<100+a->model.count) focused=a->model.displays[a->focus-100].id;
    a->pressed=0; a->hover=0;
    int count=0;
    SDL_DisplayID *ids = SDL_GetDisplays(&count);
    struct ae_settings_display records[AE_SETTINGS_DISPLAYS] = {0};
    bool ok=ids && count>=0 && count<=AE_SETTINGS_DISPLAYS;
    for (int i=0; ok && i<count; ++i) {
        struct ae_settings_display *d=&records[i];
        const char *name=SDL_GetDisplayName(ids[i]);
        if (!name || strlen(name)>=sizeof(d->name)) { ok=false; break; }
        memcpy(d->name,name,strlen(name)+1); d->id=ids[i];
        d->scale=SDL_GetDisplayContentScale(ids[i]);
        const SDL_DisplayMode *mode=SDL_GetCurrentDisplayMode(ids[i]);
        if (mode) { d->mode_known=true; d->width=mode->w; d->height=mode->h; d->refresh=mode->refresh_rate; }
    }
    SDL_free(ids);
    if (ok) ok=ae_settings_snapshot(&a->model,records,(size_t)count);
    if (!ok) a->model.stale=true;
    if (ok && focused) {
        a->focus=1;
        for (size_t i=0; i<a->model.count; ++i) if (a->model.displays[i].id==focused) a->focus=100+(unsigned)i;
        a->reveal_focus=true;
    }
    a->dirty=true;
    if (a->diagnostics) {
        printf("OBSERVATION %u %zu %d\n",a->model.refreshes,a->model.count,a->model.stale);
        for (size_t i=0; i<a->model.count; ++i) {
            struct ae_settings_display *d=&a->model.displays[i];
            printf("DISPLAY %u %d %d %.3f %.3f %d\n",d->id,d->width,d->height,(double)d->scale,(double)d->refresh,d->mode_known);
        }
    }
}
static void activate(struct app *a, unsigned id) {
    if (id>=1 && id<=3) {
        a->model.page=(enum ae_settings_page)(id-1); a->scroll=0; a->pressed=0; a->hover=0;
    } else if (id==4) refresh(a);
    else if (id==5) a->running=false;
    else if (id>=10 && id<=12) a->model.theme=(enum ae_settings_theme)(id-10);
    else if (id>=20 && id<=22) {
        const unsigned scales[]={100,150,200}; a->model.text_percent=scales[id-20]; a->reveal_focus=true;
    } else if (id==30 || id==31) {
        a->model.german=id==31;
        SDL_SetWindowTitle(a->window,tr(a,"Aetheri Settings","Aetheri Einstellungen"));
        a->reveal_focus=true;
    } else if (id>=100 && id<100+a->model.count) ae_settings_select(&a->model,a->model.displays[id-100].id);
    a->dirty=true;
}
static void tab(struct app *a, bool reverse) {
    if (!a->count) return;
    size_t index=0;
    for (size_t i=0; i<a->count; ++i) if (a->controls[i].id==a->focus) { index=i; break; }
    index=reverse ? (index+a->count-1)%a->count : (index+1)%a->count;
    a->focus=a->controls[index].id; a->pressed=0; a->dirty=true; a->reveal_focus=true;
}
static void palette(struct app *a) {
    if (a->model.theme==AE_SETTINGS_LIGHT) a->colors=(struct palette){
        {242,244,248,255},{255,255,255,255},{22,30,44,255},{66,78,94,255},
        {188,197,210,255},{18,83,180,255},{218,233,255,255}};
    else if (a->model.theme==AE_SETTINGS_CONTRAST) a->colors=(struct palette){
        {0,0,0,255},{20,20,20,255},{255,255,255,255},{230,230,230,255},
        {255,255,255,255},{255,226,64,255},{48,48,48,255}};
    else a->colors=(struct palette){
        {19,23,31,255},{29,35,46,255},{239,244,252,255},{178,192,212,255},
        {70,83,104,255},{140,186,255,255},{39,60,88,255}};
}
static void draw(struct app *a) {
    int width,height;
    if (!SDL_GetWindowSize(a->window,&width,&height)) { error(a,"window size"); return; }
    a->density=SDL_GetWindowPixelDensity(a->window);
    if (!isfinite(a->density) || a->density<0.25f || a->density>8) { error(a,"pixel density"); return; }
    a->text_scale=(float)a->model.text_percent/100;
    palette(a); a->count=0;
    if (!SDL_SetRenderScale(a->renderer,a->density,a->density)) { error(a,"render scale"); return; }
    color(a,a->colors.bg);
    if (!SDL_RenderClear(a->renderer)) { error(a,"clear"); return; }
    float content_w=fminf((float)width-48,1100);
    float x=((float)width-content_w)/2, y=24;
    float close_w=110*a->text_scale, h=24+20*a->text_scale;
    float title_h=text(a,tr(a,"Settings","Einstellungen"),x,y,content_w-close_w-20,28,a->colors.text);
    button(a,5,tr(a,"Close","Schließen"),(SDL_FRect){x+content_w-close_w,y,close_w,h},false,false);
    y+=fmaxf(title_h,h)+16;
    float tab_w=(content_w-16)/3;
    const char *titles[]={tr(a,"Displays","Bildschirme"),tr(a,"Appearance","Design"),tr(a,"Language","Sprache")};
    for (unsigned i=0; i<3; ++i) button(a,i+1,titles[i],(SDL_FRect){x+(tab_w+8)*(float)i,y,tab_w,h},
        (unsigned)a->model.page==i,false);
    y+=h+20;
    a->viewport=(SDL_FRect){x,y,content_w,fmaxf(40,(float)height-y-56)};
    SDL_Rect clip={(int)x,(int)y,(int)content_w,(int)a->viewport.h};
    if (!SDL_SetRenderClipRect(a->renderer,&clip)) { error(a,"content clip"); return; }
    float top=y; y-=a->scroll;
    if (a->model.page==AE_SETTINGS_DISPLAY) {
        y+=text(a,tr(a,"Connected displays","Verbundene Bildschirme"),x,y,content_w,22,a->colors.text)+12;
        y+=text(a,tr(a,"Display information from this session. Display changes are not available yet.",
            "Anzeigeinformationen dieser Sitzung. Änderungen sind noch nicht verfügbar."),x,y,content_w,16,a->colors.secondary)+20;
        if (a->model.stale) y+=text(a,tr(a,"Refresh failed. The previous observation is shown below.",
            "Aktualisierung fehlgeschlagen. Unten steht der vorherige Stand."),x,y,content_w,16,a->colors.accent)+16;
        if (!a->model.count) y+=text(a,tr(a,"No display information is available.","Keine Anzeigeinformationen verfügbar."),x,y,content_w,16,a->colors.secondary)+16;
        for (size_t i=0; i<a->model.count; ++i) {
            struct ae_settings_display *d=&a->model.displays[i];
            char label[256];
            if (d->mode_known) snprintf(label,sizeof(label),"%zu. %s   ·   %d × %d",i+1,d->name,d->width,d->height);
            else snprintf(label,sizeof(label),"%zu. %s   ·   %s",i+1,d->name,tr(a,"Mode unavailable","Modus nicht verfügbar"));
            float row_h=fmaxf(h,24+measure(a,label,content_w-24,16));
            button(a,100+(unsigned)i,label,(SDL_FRect){x,y,content_w,row_h},a->model.selected==i,true);
            y+=row_h+8;
        }
        if (a->model.count) {
            struct ae_settings_display *d=&a->model.displays[a->model.selected];
            char details[256];
            if (d->refresh>0) snprintf(details,sizeof(details),"%s: %.2f Hz   ·   %s: %.0f%%",
                tr(a,"Refresh rate","Bildwiederholrate"),(double)d->refresh,tr(a,"Content scale","Inhaltsskalierung"),(double)d->scale*100);
            else snprintf(details,sizeof(details),"%s   ·   %s: %.0f%%",
                tr(a,"Refresh rate unavailable","Bildwiederholrate nicht verfügbar"),tr(a,"Content scale","Inhaltsskalierung"),(double)d->scale*100);
            y+=12+text(a,details,x,y+12,content_w,16,a->colors.secondary)+20;
        }
        button(a,4,tr(a,"Refresh","Aktualisieren"),(SDL_FRect){x,y,fminf(content_w,220*a->text_scale),h},false,true); y+=h;
    } else if (a->model.page==AE_SETTINGS_APPEARANCE) {
        y+=text(a,tr(a,"Appearance for this window","Design für dieses Fenster"),x,y,content_w,22,a->colors.text)+12;
        y+=text(a,tr(a,"These choices affect this window and last until it closes.",
            "Diese Auswahl gilt für dieses Fenster, bis es geschlossen wird."),x,y,content_w,16,a->colors.secondary)+24;
        const char *themes[]={tr(a,"Dark","Dunkel"),tr(a,"Light","Hell"),tr(a,"High contrast","Hoher Kontrast")};
        for (unsigned i=0; i<3; ++i) {
            button(a,10+i,themes[i],(SDL_FRect){x,y,content_w,h},(unsigned)a->model.theme==i,true); y+=h+8;
        }
        y+=16+text(a,tr(a,"Text size","Textgröße"),x,y+16,content_w,18,a->colors.text)+16;
        const char *scales[]={"100%","150%","200%"}; const unsigned values[]={100,150,200};
        for (unsigned i=0; i<3; ++i) {
            button(a,20+i,scales[i],(SDL_FRect){x,y,content_w,h},a->model.text_percent==values[i],true); y+=h+8;
        }
    } else {
        y+=text(a,tr(a,"Language for this window","Sprache für dieses Fenster"),x,y,content_w,22,a->colors.text)+12;
        y+=text(a,tr(a,"System language and keyboard layout stay unchanged.",
            "Systemsprache und Tastaturbelegung bleiben unverändert."),x,y,content_w,16,a->colors.secondary)+24;
        button(a,30,"English",(SDL_FRect){x,y,content_w,h},!a->model.german,true); y+=h+8;
        button(a,31,"Deutsch",(SDL_FRect){x,y,content_w,h},a->model.german,true); y+=h;
    }
    a->max_scroll=fmaxf(0,y+a->scroll-top-a->viewport.h+16);
    bool redraw=false;
    if (a->scroll>a->max_scroll) { a->scroll=a->max_scroll; redraw=true; }
    if (a->reveal_focus) {
        a->reveal_focus=false;
        for (size_t i=0; i<a->count; ++i) if (a->controls[i].id==a->focus && a->controls[i].content) {
            SDL_FRect r=a->controls[i].rect;
            float delta=r.y<top ? r.y-top : fmaxf(0,r.y+r.h-top-a->viewport.h);
            float next=fminf(a->max_scroll,fmaxf(0,a->scroll+delta));
            if (next!=a->scroll) { a->scroll=next; redraw=true; }
        }
    }
    if (!SDL_SetRenderClipRect(a->renderer,NULL)) { error(a,"clip reset"); return; }
    text(a,tr(a,"Tab: focus   ·   Enter   ·   F5   ·   Ctrl+Q",
        "Tab: Fokus   ·   Enter   ·   F5   ·   Strg+Q"),x,(float)height-36,content_w,12,a->colors.secondary);
    if (redraw && !a->failed) { draw(a); return; }
    if (!SDL_RenderPresent(a->renderer)) { error(a,"present"); return; }
    ++a->frames; a->dirty=false;
    if (a->diagnostics) {
        printf("VIEW %lu page=%u selected=%zu theme=%u text=%u de=%d focus=%u scroll=%.1f max=%.1f size=%dx%d\n",
            a->frames,(unsigned)a->model.page,a->model.selected,(unsigned)a->model.theme,a->model.text_percent,
            a->model.german,a->focus,(double)a->scroll,(double)a->max_scroll,width,height);
        for (size_t i=0; i<a->count; ++i) {
            struct control *c=&a->controls[i];
            float low=c->content ? fmaxf(c->rect.y,a->viewport.y) : c->rect.y;
            float high=c->content ? fminf(c->rect.y+c->rect.h,a->viewport.y+a->viewport.h) : c->rect.y+c->rect.h;
            printf("CONTROL %lu %u %.2f %.2f %.2f %.2f\n",a->frames,c->id,
                (double)(c->rect.x+c->rect.w/2),(double)((low+high)/2),(double)low,(double)high);
        }
        printf("LAYOUT %lu\n",a->frames);
    }
}
static SDL_HitTestResult hit_test(SDL_Window *window, const SDL_Point *point, void *data) {
    struct app *a=data;
    int w,h;
    if (!SDL_GetWindowSize(window,&w,&h) || (SDL_GetWindowFlags(window)&(SDL_WINDOW_MAXIMIZED|SDL_WINDOW_FULLSCREEN))) return SDL_HITTEST_NORMAL;
    bool left=point->x<8, right=point->x>=w-8, top=point->y<8, bottom=point->y>=h-8;
    if (top && left) return SDL_HITTEST_RESIZE_TOPLEFT;
    if (top && right) return SDL_HITTEST_RESIZE_TOPRIGHT;
    if (bottom && left) return SDL_HITTEST_RESIZE_BOTTOMLEFT;
    if (bottom && right) return SDL_HITTEST_RESIZE_BOTTOMRIGHT;
    if (left) return SDL_HITTEST_RESIZE_LEFT;
    if (right) return SDL_HITTEST_RESIZE_RIGHT;
    if (top) return SDL_HITTEST_RESIZE_TOP;
    if (bottom) return SDL_HITTEST_RESIZE_BOTTOM;
    /* The close control is excluded from the provisional draggable header. */
    for (size_t i=0; i<a->count; ++i) if (a->controls[i].id==5 &&
            inside(a->controls[i].rect,(float)point->x,(float)point->y)) return SDL_HITTEST_NORMAL;
    return point->y<60 ? SDL_HITTEST_DRAGGABLE : SDL_HITTEST_NORMAL;
}
static void event(struct app *a, const SDL_Event *e) {
    if (e->type==SDL_EVENT_QUIT || e->type==SDL_EVENT_WINDOW_CLOSE_REQUESTED) a->running=false;
    else if (e->type>=SDL_EVENT_DISPLAY_FIRST && e->type<=SDL_EVENT_DISPLAY_LAST) refresh(a);
    else if (e->type==SDL_EVENT_WINDOW_FOCUS_LOST || e->type==SDL_EVENT_WINDOW_MOUSE_LEAVE) {
        a->pressed=0; a->hover=0; a->dirty=true;
    }
    else if (e->type==SDL_EVENT_WINDOW_EXPOSED || e->type==SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED ||
             e->type==SDL_EVENT_WINDOW_RESIZED || e->type==SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED) {
        a->dirty=true; a->reveal_focus=true; a->pressed=0; a->hover=0;
    } else if (e->type==SDL_EVENT_KEY_DOWN && !e->key.repeat) {
        if (e->key.key==SDLK_TAB) tab(a,(e->key.mod&SDL_KMOD_SHIFT)!=0);
        else if (e->key.key==SDLK_RETURN || e->key.key==SDLK_SPACE) activate(a,a->focus);
        else if (e->key.key==SDLK_F5) refresh(a);
        else if (e->key.key==SDLK_Q && (e->key.mod&SDL_KMOD_CTRL)) a->running=false;
        else if (e->key.key==SDLK_PAGEDOWN || e->key.key==SDLK_PAGEUP) {
            float step=e->key.key==SDLK_PAGEDOWN ? a->viewport.h : -a->viewport.h;
            a->scroll=fmaxf(0,fminf(a->max_scroll,a->scroll+step)); a->dirty=true;
        }
    } else if (e->type==SDL_EVENT_MOUSE_WHEEL) {
        float direction=e->wheel.direction==SDL_MOUSEWHEEL_FLIPPED ? -1 : 1;
        a->scroll=fmaxf(0,fminf(a->max_scroll,a->scroll-e->wheel.y*direction*48)); a->dirty=true; a->pressed=0; a->hover=0;
    } else if (e->type==SDL_EVENT_MOUSE_MOTION) {
        unsigned hit=0;
        for (size_t i=0; i<a->count; ++i) {
            struct control *c=&a->controls[i];
            if (inside(c->rect,e->motion.x,e->motion.y) && (!c->content || inside(a->viewport,e->motion.x,e->motion.y))) hit=c->id;
        }
        if (hit!=a->hover) { a->hover=hit; a->dirty=true; }
    } else if ((e->type==SDL_EVENT_MOUSE_BUTTON_DOWN || e->type==SDL_EVENT_MOUSE_BUTTON_UP) && e->button.button==SDL_BUTTON_LEFT) {
        unsigned hit=0;
        for (size_t i=0; i<a->count; ++i) {
            struct control *c=&a->controls[i];
            if (inside(c->rect,e->button.x,e->button.y) && (!c->content || inside(a->viewport,e->button.x,e->button.y))) hit=c->id;
        }
        if (e->type==SDL_EVENT_MOUSE_BUTTON_DOWN) { a->pressed=hit; if (hit) a->focus=hit; }
        else { if (hit && hit==a->pressed) activate(a,hit); a->pressed=0; }
        a->dirty=true;
    }
}
int main(int argc, char **argv) {
    struct app a={.running=true,.dirty=true,.focus=1};
    const char *font=NULL,*cache=NULL;
    for (int i=1; i<argc; ++i) {
        if (!strcmp(argv[i],"--font") && i+1<argc && !font) font=argv[++i];
        else if (!strcmp(argv[i],"--cache") && i+1<argc && !cache) cache=argv[++i];
        else if (!strcmp(argv[i],"--diagnostics") && !a.diagnostics) a.diagnostics=true;
        else { fprintf(stderr,"usage: aetheri-settings --font FONT --cache DIRECTORY [--diagnostics]\n"); return 2; }
    }
    if (!font || !cache || cache[0]!='/' || getuid()==0 || geteuid()!=getuid() || getegid()!=getgid()) return 2;
    struct stat cache_stat;
    if (lstat(cache,&cache_stat) || !S_ISDIR(cache_stat.st_mode) || cache_stat.st_uid!=getuid() ||
            (cache_stat.st_mode&07777)!=0700) {
        fprintf(stderr,"settings: select an existing owned private cache directory\n"); return 2;
    }
    /* A development cache path, not StorageRef/volume admission or a disk quota. */
    if (setenv("MESA_SHADER_CACHE_DIR",cache,1) || setenv("MESA_SHADER_CACHE_MAX_SIZE","32M",1)) return 1;
    setvbuf(stdout,NULL,_IOLBF,0);
    ae_settings_init(&a.model);
    if (!SDL_SetAppMetadata("Aetheri Settings","0.0.1","org.aetheri.Settings") ||
            !SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"wayland") || !SDL_Init(SDL_INIT_VIDEO) || !TTF_Init()) {
        fprintf(stderr,"settings: initialization: %s\n",SDL_GetError()); SDL_Quit(); return 1;
    }
    int ft_major,ft_minor,ft_patch,hb_major,hb_minor,hb_patch;
    TTF_GetFreeTypeVersion(&ft_major,&ft_minor,&ft_patch);
    TTF_GetHarfBuzzVersion(&hb_major,&hb_minor,&hb_patch);
    if (SDL_GetVersion()!=SDL_VERSION || TTF_Version()!=SDL_TTF_VERSION ||
            ft_major!=2 || ft_minor!=15 || ft_patch!=0 || hb_major!=15 || hb_minor!=0 || hb_patch!=0)
        error(&a,"unreviewed loaded SDL/text version");
    a.font=TTF_OpenFont(font,16);
    a.window=SDL_CreateWindow("Aetheri Settings",1100,760,SDL_WINDOW_RESIZABLE|SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (a.window) {
        if (!SDL_SetWindowMinimumSize(a.window,760,480)) error(&a,"minimum size");
        if (!SDL_SetWindowHitTest(a.window,hit_test,&a)) error(&a,"native move/resize");
        a.renderer=SDL_CreateRenderer(a.window,"opengles2");
    }
    if (!a.font || !a.window || !a.renderer) error(&a,"window/font/renderer");
    if (!a.failed) {
        refresh(&a);
        if (a.diagnostics) {
            const unsigned char *(*get_string)(unsigned)=(const unsigned char *(*)(unsigned))SDL_GL_GetProcAddress("glGetString");
            const unsigned char *renderer=get_string ? get_string(0x1F01) : NULL;
            printf("READY %s %s SDL=%d TTF=%d\n",SDL_GetCurrentVideoDriver(),SDL_GetRendererName(a.renderer),SDL_GetVersion(),TTF_Version());
            printf("GL_RENDERER %s\n",renderer ? (const char *)renderer : "unavailable");
        }
        while (a.running) {
            if (a.dirty) draw(&a);
            if (!a.running) break;
            SDL_Event e;
            if (!SDL_WaitEvent(&e)) { error(&a,"event wait"); break; }
            /* Rebuild changed hit/focus geometry before taking another event. */
            event(&a,&e);
        }
    }
    if (a.diagnostics) printf("CLOSED frames=%lu\n",a.frames);
    SDL_DestroyRenderer(a.renderer); SDL_DestroyWindow(a.window); TTF_CloseFont(a.font);
    TTF_Quit(); SDL_Quit();
    return a.failed ? 1 : 0;
}
