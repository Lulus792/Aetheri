/* Development graphical terminal; normal app admission and full VT remain work. */
#define _GNU_SOURCE
#include "model.h"
#include "pty.h"
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <errno.h>
#include <fcntl.h>
#include <locale.h>
#include <math.h>
#include <poll.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <wchar.h>
#include <wayland-client.h>
enum { GLYPHS=512, TEXTURE_BUDGET=8*1024*1024, INPUT_BYTES=4096 };
struct glyph { uint32_t code,marks[3];uint8_t count,style;SDL_Texture *texture;float w,h;size_t bytes;uint64_t age; };
struct app {
    struct ae_terminal_pty pty;struct ae_terminal *model;
    SDL_Window *window;SDL_Renderer *renderer;TTF_Font *font;
    struct glyph glyphs[GLYPHS];size_t texture_bytes;uint64_t age;
    SDL_Thread *worker;SDL_Semaphore *ack;
    atomic_bool active,readable,watch_child,writeable;
    int wake[2];uint32_t event;
    unsigned char input[INPUT_BYTES];size_t input_count;
    float density,cell_w,cell_h,left,grid_y,bar_height,footer_height;
    unsigned font_points,applied_points;
    bool running,failed,closing,dirty,diagnostics,german,contrast,close_pressed,selecting;
    unsigned frame,drawn_frame;uint64_t input_bytes;struct wl_callback *frame_callback;
    const char *message;char ended_message[128];
};
static volatile sig_atomic_t wake_writer=-1,stop_signal;
static void signalled(int number) { int saved=errno;stop_signal=number;unsigned char byte=1;if (wake_writer>=0) (void)write(wake_writer,&byte,1);errno=saved; }
static void notify(struct app *a) { unsigned char byte=1;if (a->wake[1]>=0) (void)write(a->wake[1],&byte,1); }
static int worker(void *data) {
    struct app *a=data;
    while (atomic_load(&a->active)) {
        short io=(short)((atomic_load(&a->readable) ? POLLIN : 0)|(atomic_load(&a->writeable) ? POLLOUT : 0));
        struct pollfd fds[3]={{io ? a->pty.master : -1,io,0},{atomic_load(&a->watch_child) ? a->pty.pin : -1,POLLIN,0},{a->wake[0],POLLIN,0}};
        int n;do { n=poll(fds,3,-1); } while (n<0 && errno==EINTR);
        if (fds[2].revents) { unsigned char bytes[64];while (read(a->wake[0],bytes,sizeof(bytes))>0) {} }
        if (!atomic_load(&a->active)) break;
        SDL_Event event={.type=a->event};event.user.code=n<0 ? -1 : 0;
        bool posted=false;
        while (atomic_load(&a->active)) {
            if (SDL_PushEvent(&event)) { posted=true;break; }
            /* Queue pressure must not abandon the original shell. Deliver a
               failure once capacity returns; normal idle never uses a timer. */
            event.user.code=-1;SDL_Delay(10);
        }
        if (!posted) break;
        SDL_WaitSemaphore(a->ack);
    }
    return 0;
}
static const char *tr(struct app *a,const char *en,const char *de) { return a->german ? de : en; }
static void close_request(struct app *a) {
    if (a->closing) return;
    a->closing=true;a->dirty=true;a->selecting=false;
    if (a->pty.ended && a->pty.eof) { a->running=false;return; }
    if (ae_terminal_pty_hangup(&a->pty) && errno!=ESRCH) { a->failed=true;a->message="Shell hangup could not be acknowledged"; }
}
static void error(struct app *a,const char *message) { fprintf(stderr,"terminal: %s: %s\n",message,SDL_GetError());a->failed=true;a->message=message;close_request(a); }
static int width(uint32_t code) { return code<=0x10ffff ? wcwidth((wchar_t)code) : -1; }
static void glyphs_clear(struct app *a) {
    for (unsigned i=0;i<GLYPHS;++i) { SDL_DestroyTexture(a->glyphs[i].texture);a->glyphs[i]=(struct glyph){0}; }
    a->texture_bytes=0;
}
static size_t utf8(char *out,uint32_t c) {
    if (c<0x80) { out[0]=(char)c;return 1; }
    if (c<0x800) { out[0]=(char)(0xc0|(c>>6));out[1]=(char)(0x80|(c&63));return 2; }
    if (c<0x10000) { out[0]=(char)(0xe0|(c>>12));out[1]=(char)(0x80|((c>>6)&63));out[2]=(char)(0x80|(c&63));return 3; }
    out[0]=(char)(0xf0|(c>>18));out[1]=(char)(0x80|((c>>12)&63));out[2]=(char)(0x80|((c>>6)&63));out[3]=(char)(0x80|(c&63));return 4;
}
static struct glyph *glyph(struct app *a,const struct ae_term_cell *cell) {
    uint8_t style=cell->style&AE_TERM_BOLD;
    for (unsigned i=0;i<GLYPHS;++i) {
        struct glyph *g=&a->glyphs[i];
        if (g->texture && g->code==cell->code && g->style==style && g->count==cell->count && !memcmp(g->marks,cell->marks,sizeof(g->marks))) { g->age=++a->age;return g; }
    }
    char text[17];size_t n=utf8(text,cell->code);
    for (unsigned i=0;i<cell->count;++i) n+=utf8(text+n,cell->marks[i]);
    text[n]=0;
    TTF_SetFontStyle(a->font,style ? TTF_STYLE_BOLD : TTF_STYLE_NORMAL);
    SDL_Surface *surface=TTF_RenderText_Blended(a->font,text,n,(SDL_Color){255,255,255,255});
    if (!surface) { error(a,"glyph rasterization");return NULL; }
    size_t bytes=(size_t)surface->w*(size_t)surface->h*4;
    if (surface->w<1 || surface->h<1 || bytes>TEXTURE_BUDGET) { SDL_DestroySurface(surface);error(a,"glyph resource bound");return NULL; }
    unsigned slot=GLYPHS;
    for (unsigned i=0;i<GLYPHS;++i) if (!a->glyphs[i].texture) { slot=i;break; }
    while (slot==GLYPHS || a->texture_bytes+bytes>TEXTURE_BUDGET) {
        unsigned oldest=GLYPHS;
        for (unsigned i=0;i<GLYPHS;++i) if (a->glyphs[i].texture && (oldest==GLYPHS || a->glyphs[i].age<a->glyphs[oldest].age)) oldest=i;
        if (oldest==GLYPHS) { SDL_DestroySurface(surface);error(a,"glyph cache accounting");return NULL; }
        struct glyph *old=&a->glyphs[oldest];a->texture_bytes-=old->bytes;SDL_DestroyTexture(old->texture);*old=(struct glyph){0};
        if (slot==GLYPHS) slot=oldest;
    }
    struct glyph *g=&a->glyphs[slot];g->texture=SDL_CreateTextureFromSurface(a->renderer,surface);
    *g=(struct glyph){.texture=g->texture,.code=cell->code,.style=style,.count=cell->count,.w=(float)surface->w/a->density,.h=(float)surface->h/a->density,.bytes=bytes,.age=++a->age};
    memcpy(g->marks,cell->marks,sizeof(g->marks));SDL_DestroySurface(surface);
    if (!g->texture) { error(a,"glyph texture");return NULL; }
    a->texture_bytes+=bytes;return g;
}
static void fill(struct app *a,SDL_FRect r,uint32_t color) {
    if (!SDL_SetRenderDrawColor(a->renderer,(uint8_t)(color>>16),(uint8_t)(color>>8),(uint8_t)color,255) || !SDL_RenderFillRect(a->renderer,&r)) error(a,"rectangle");
}
static void label(struct app *a,const char *text,float x,float y,uint32_t color) {
    TTF_SetFontStyle(a->font,TTF_STYLE_NORMAL);
    SDL_Surface *s=TTF_RenderText_Blended(a->font,text,0,(SDL_Color){(uint8_t)(color>>16),(uint8_t)(color>>8),(uint8_t)color,255});
    if (!s) { error(a,"label rasterization");return; }
    SDL_Texture *t=SDL_CreateTextureFromSurface(a->renderer,s);SDL_FRect r={x,y,(float)s->w/a->density,(float)s->h/a->density};SDL_DestroySurface(s);
    if (!t || !SDL_RenderTexture(a->renderer,t,NULL,&r)) error(a,"label presentation");
    SDL_DestroyTexture(t);
}
static void frame_done(void *data,struct wl_callback *callback,uint32_t when) {
    (void)when;struct app *a=data;if (a->frame_callback==callback) { a->frame_callback=NULL;printf("DRAWN %u\n",a->drawn_frame); }wl_callback_destroy(callback);
}
static const struct wl_callback_listener frame_listener={.done=frame_done};
static void layout(struct app *a) {
    int w,h;if (!SDL_GetWindowSize(a->window,&w,&h)) { error(a,"window size");return; }
    float density=SDL_GetWindowPixelDensity(a->window);
    if (!isfinite(density) || density<.5f || density>4) { error(a,"pixel density");return; }
    if (a->density!=density || a->font_points!=a->applied_points) {
        glyphs_clear(a);a->density=density;
        TTF_SetFontStyle(a->font,TTF_STYLE_NORMAL);
        if (!TTF_SetFontSize(a->font,(float)a->font_points*density)) { error(a,"font scale");return; }
        if (!SDL_SetWindowMinimumSize(a->window,(int)(30*a->font_points),(int)(20*a->font_points))) { error(a,"text-size minimum window");return; }
        int advance;if (!TTF_GetGlyphMetrics(a->font,'M',NULL,NULL,NULL,NULL,&advance) || advance<1) { error(a,"cell metrics");return; }
        a->cell_w=(float)advance/density;a->cell_h=(float)TTF_GetFontHeight(a->font)/density+2;a->applied_points=a->font_points;
        a->bar_height=fmaxf(40,a->cell_h+18);a->footer_height=fmaxf(28,a->cell_h+8);a->grid_y=a->bar_height+12;
    }
    int cols=(int)floorf(((float)w-32)/a->cell_w),rows=(int)floorf(((float)h-a->grid_y-a->footer_height-16)/a->cell_h);
    if (cols<2) cols=2;
    if (cols>AE_TERM_COLS) cols=AE_TERM_COLS;
    if (rows<2) rows=2;
    if (rows>AE_TERM_ROWS) rows=AE_TERM_ROWS;
    if (a->model->cols!=(unsigned)cols || a->model->rows!=(unsigned)rows) {
        a->selecting=false;
        if (ae_terminal_pty_resize(&a->pty,(unsigned)cols,(unsigned)rows) || !ae_terminal_resize(a->model,(unsigned)cols,(unsigned)rows)) error(a,"PTY/grid resize");
    }
    a->left=((float)w-(float)cols*a->cell_w)/2;
}
static void draw(struct app *a) {
    layout(a);int w,h,pw,ph;
    if (!SDL_GetWindowSize(a->window,&w,&h) || !SDL_GetWindowSizeInPixels(a->window,&pw,&ph) ||
            !SDL_SetRenderScale(a->renderer,(float)pw/(float)w,(float)ph/(float)h) ||
            !SDL_SetRenderDrawColor(a->renderer,16,20,28,255) || !SDL_RenderClear(a->renderer)) { error(a,"frame setup");return; }
    fill(a,(SDL_FRect){0,0,(float)w,a->bar_height},a->contrast ? 0 : 0x191f2b);
    label(a,tr(a,"Terminal · Aetheri shell","Terminal · Aetheri-Shell"),16,10,0xeff3fb);
    label(a,"×",(float)w-32,10,0xeff3fb);
    for (unsigned y=0;y<a->model->rows;++y) {
        const struct ae_term_cell *r=ae_terminal_row(a->model,y);
        unsigned first=0,last=0;bool selected=ae_terminal_selected_range(a->model,y,&first,&last);
        for (unsigned x=0;x<a->model->cols;++x) {
            uint32_t bg=(r[x].style&AE_TERM_REVERSE) ? r[x].fg : r[x].bg;
            if (a->contrast && bg==0x10141c) bg=0;
            if (selected && x>=first && x<last) bg=0x304e78;
            fill(a,(SDL_FRect){a->left+(float)x*a->cell_w,a->grid_y+(float)y*a->cell_h,a->cell_w,a->cell_h},bg);
        }
        for (unsigned x=0;x<a->model->cols;++x) {
            const struct ae_term_cell *c=&r[x];uint32_t fg=c->fg,bg=c->bg;
            if (c->style&AE_TERM_REVERSE) { uint32_t temp=fg;fg=bg;bg=temp; }
            if (a->contrast && bg==0x10141c) { bg=0;fg=fg==0xdce4ef ? 0xffffff : fg; }
            if (selected && x>=first && x<last) fg=0xffffff;
            SDL_FRect box={a->left+(float)x*a->cell_w,a->grid_y+(float)y*a->cell_h,a->cell_w,a->cell_h};
            if (c->width && c->code!=' ' && c->code) {
                struct glyph *g=glyph(a,c);SDL_FRect dest={box.x,box.y,g ? g->w : 0,g ? g->h : 0};
                SDL_Rect clip={(int)box.x,(int)box.y,(int)ceilf(a->cell_w*(float)c->width),(int)ceilf(a->cell_h)};
                if (g && (!SDL_SetRenderClipRect(a->renderer,&clip) || !SDL_SetTextureColorMod(g->texture,(uint8_t)(fg>>16),(uint8_t)(fg>>8),(uint8_t)fg) || !SDL_RenderTexture(a->renderer,g->texture,NULL,&dest))) error(a,"cell presentation");
                (void)SDL_SetRenderClipRect(a->renderer,NULL);
            }
            if (c->style&AE_TERM_UNDERLINE) fill(a,(SDL_FRect){box.x,box.y+box.h-2,box.w,1},fg);
        }
    }
    if (a->model->cursor && !a->model->view && !a->pty.ended) {
        SDL_FRect cursor={a->left+(float)a->model->x*a->cell_w,a->grid_y+(float)a->model->y*a->cell_h,a->cell_w,2};cursor.y+=a->cell_h-2;fill(a,cursor,0xa2c7ff);
    }
    const char *status=a->message ? a->message : a->closing ? tr(a,"Closing · waiting for shell/jobs","Schließen · Warten auf Shell/Jobs") :
        a->pty.ended ? tr(a,"Shell ended · waiting for terminal writers","Shell beendet · Warten auf Terminalausgabe") :
        a->model->selection.count ? tr(a,"Selection · Esc clears","Auswahl · Esc löscht") :
        tr(a,"Ctrl+C/Z · Shift+PgUp/PgDn · Ctrl+Shift+Q","Strg+C/Z · Shift+Bild↑/↓ · Strg+Shift+Q");
    label(a,status,16,(float)h-a->footer_height,0xb2c0d6);
    if (a->diagnostics) {
        if (a->frame_callback) wl_callback_destroy(a->frame_callback);
        struct wl_surface *surface=SDL_GetPointerProperty(SDL_GetWindowProperties(a->window),SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER,NULL);
        a->frame_callback=surface ? wl_surface_frame(surface) : NULL;a->drawn_frame=a->frame+1;
        if (!a->frame_callback || wl_callback_add_listener(a->frame_callback,&frame_listener,a)) { error(a,"frame observation");return; }
    }
    if (!SDL_RenderPresent(a->renderer)) { error(a,"present");return; }
    a->dirty=false;++a->frame;
    if (a->diagnostics) printf("VIEW %u cols=%u rows=%u x=%u y=%u history=%u view=%u sequence=%llu closing=%d ended=%d eof=%d german=%d contrast=%d points=%u width=%d height=%d selection=%zu selecting=%d grid_left=%.3f grid_top=%.3f cell_width=%.3f cell_height=%.3f\n",a->frame,a->model->cols,a->model->rows,a->model->x,a->model->y,a->model->history_count,a->model->view,(unsigned long long)a->model->sequence,a->closing,a->pty.ended,a->pty.eof,a->german,a->contrast,a->font_points,w,h,ae_terminal_selection_size(a->model),a->selecting,(double)a->left,(double)a->grid_y,(double)a->cell_w,(double)a->cell_h);
}
static void flush_input(struct app *a) {
    while (a->input_count && !a->pty.eof) {
        ssize_t n=write(a->pty.master,a->input,a->input_count);
        if (n<0 && errno==EINTR) continue;
        if (n<0 && (errno==EAGAIN || errno==EWOULDBLOCK)) break;
        if (n<=0) { error(a,"PTY input");break; }
        memmove(a->input,a->input+n,a->input_count-(size_t)n);a->input_count-=(size_t)n;
    }
    atomic_store(&a->writeable,a->input_count!=0 && !a->pty.eof);
}
static void input(struct app *a,const void *bytes,size_t size) {
    if (a->closing || a->pty.ended || a->pty.eof) return;
    if (size>sizeof(a->input)-a->input_count) { a->message="Input queue full; new input was not sent";a->dirty=true;return; }
    if (a->model->selection.count) { ae_terminal_select_clear(a->model);a->selecting=false;a->dirty=true; }
    memcpy(a->input+a->input_count,bytes,size);a->input_count+=size;
    if (a->model->view) a->dirty=true;
    a->model->view=0;a->message=NULL;flush_input(a);
    a->input_bytes+=(uint64_t)size;
    if (a->diagnostics) printf("INPUT %llu\n",(unsigned long long)a->input_bytes);
    if (a->input_count) notify(a);
}
static void io_event(struct app *a) {
    if (stop_signal) close_request(a);
    unsigned char bytes[4096];size_t budget=16384;
    while (!a->pty.eof && budget) {
        ssize_t n=ae_terminal_pty_read(&a->pty,bytes,sizeof(bytes));
        if (n<0 && errno==EINTR) continue;
        if (n<0 && (errno==EAGAIN || errno==EWOULDBLOCK)) break;
        if (n<0) { error(a,"PTY output");break; }
        if (!n) break;
        ae_terminal_feed(a->model,bytes,(size_t)n);if (!a->model->selection.count) a->selecting=false;budget-=(size_t)n;a->dirty=true;
    }
    bool ended=a->pty.ended;
    if (!a->pty.ended && ae_terminal_pty_observe(&a->pty)<0) error(a,"original shell wait");
    if (a->pty.ended!=ended) a->dirty=true;
    atomic_store(&a->readable,!a->pty.eof);atomic_store(&a->watch_child,!a->pty.ended);
    flush_input(a);
    if (a->pty.ended && a->pty.eof) {
        if (a->closing || a->pty.status==0) a->running=false;
        else { snprintf(a->ended_message,sizeof(a->ended_message),"%s %d · %s",tr(a,"Shell ended:","Shell beendet:"),a->pty.status,tr(a,"Close to leave","Schließen zum Beenden"));a->message=a->ended_message;a->dirty=true; }
    }
}
static SDL_HitTestResult hit_test(SDL_Window *window,const SDL_Point *p,void *data) {
    const struct app *a=data;int w,h;if (!SDL_GetWindowSize(window,&w,&h) || (SDL_GetWindowFlags(window)&(SDL_WINDOW_MAXIMIZED|SDL_WINDOW_FULLSCREEN))) return SDL_HITTEST_NORMAL;
    bool l=p->x<8,r=p->x>=w-8,t=p->y<8,b=p->y>=h-8;
    if (t&&l) return SDL_HITTEST_RESIZE_TOPLEFT;
    if (t&&r) return SDL_HITTEST_RESIZE_TOPRIGHT;
    if (b&&l) return SDL_HITTEST_RESIZE_BOTTOMLEFT;
    if (b&&r) return SDL_HITTEST_RESIZE_BOTTOMRIGHT;
    if (l) return SDL_HITTEST_RESIZE_LEFT;
    if (r) return SDL_HITTEST_RESIZE_RIGHT;
    if (t) return SDL_HITTEST_RESIZE_TOP;
    if (b) return SDL_HITTEST_RESIZE_BOTTOM;
    return (float)p->y<a->bar_height && p->x<w-48 ? SDL_HITTEST_DRAGGABLE : SDL_HITTEST_NORMAL;
}
static bool selection_point(struct app *a,float x,float y,bool clamp,unsigned *row,unsigned *column) {
    if (!isfinite(x) || !isfinite(y) || a->cell_w<=0 || a->cell_h<=0) return false;
    float col=(x-a->left)/a->cell_w,line=(y-a->grid_y)/a->cell_h;
    if (!clamp && (col<0 || col>=(float)a->model->cols || line<0 || line>=(float)a->model->rows)) return false;
    col=fmaxf(0,fminf((float)a->model->cols,floorf(col+.5f)));
    line=fmaxf(0,fminf((float)a->model->rows-1,floorf(line)));
    *row=(unsigned)line;*column=(unsigned)col;return true;
}
static void event(struct app *a,const SDL_Event *e) {
    if (a->diagnostics && (e->type==SDL_EVENT_WINDOW_FOCUS_GAINED || e->type==SDL_EVENT_WINDOW_FOCUS_LOST)) printf("FOCUS %d\n",e->type==SDL_EVENT_WINDOW_FOCUS_GAINED);
    if (e->type==a->event) { if (e->user.code<0) error(a,"PTY event bridge");io_event(a);SDL_SignalSemaphore(a->ack);return; }
    if (e->type==SDL_EVENT_QUIT || e->type==SDL_EVENT_WINDOW_CLOSE_REQUESTED) { close_request(a);return; }
    if (e->type==SDL_EVENT_WINDOW_EXPOSED || e->type==SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED || e->type==SDL_EVENT_WINDOW_RESIZED || e->type==SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED) a->dirty=true;
    /* Committed text owns its input provenance. The current global modifier
       state can already belong to a later queued key, or to an IME shortcut. */
    else if (e->type==SDL_EVENT_TEXT_INPUT) input(a,e->text.text,strlen(e->text.text));
    else if (e->type==SDL_EVENT_MOUSE_WHEEL && e->wheel.y!=0) { ae_terminal_scroll(a->model,e->wheel.y>0 ? 3 : -3);a->dirty=true; }
    else if (e->type==SDL_EVENT_WINDOW_FOCUS_LOST) { a->close_pressed=false;a->selecting=false;a->dirty=true; }
    else if (e->type==SDL_EVENT_MOUSE_MOTION && a->selecting) {
        unsigned row,column;
        if (selection_point(a,e->motion.x,e->motion.y,true,&row,&column) && !ae_terminal_select_extend(a->model,row,column)) a->selecting=false;
        a->dirty=true;
    }
    else if ((e->type==SDL_EVENT_MOUSE_BUTTON_DOWN || e->type==SDL_EVENT_MOUSE_BUTTON_UP) && e->button.button==SDL_BUTTON_LEFT) {
        int w,h;SDL_GetWindowSize(a->window,&w,&h);bool hit=e->button.y<a->bar_height && e->button.x>(float)w-48;
        unsigned row,column;
        if (e->type==SDL_EVENT_MOUSE_BUTTON_DOWN) {
            a->close_pressed=hit;a->selecting=false;
            if (!hit && !a->closing && selection_point(a,e->button.x,e->button.y,false,&row,&column))
                a->selecting=ae_terminal_select_begin(a->model,row,column);
        } else {
            if (hit && a->close_pressed) close_request(a);
            if (a->selecting && selection_point(a,e->button.x,e->button.y,true,&row,&column)) (void)ae_terminal_select_extend(a->model,row,column);
            a->close_pressed=false;a->selecting=false;
        }
        a->dirty=true;
    }
    else if (e->type==SDL_EVENT_KEY_DOWN) {
        SDL_Keycode key=e->key.key;SDL_Keymod mod=e->key.mod;
        if ((mod&SDL_KMOD_CTRL) && (mod&SDL_KMOD_SHIFT) && key==SDLK_Q) close_request(a);
        else if ((mod&SDL_KMOD_CTRL) && (mod&SDL_KMOD_SHIFT) && !(mod&(SDL_KMOD_ALT|SDL_KMOD_GUI)) && (key==SDLK_C || key==SDLK_V)) {
            /* Reserve clipboard chords: they must never become terminal
               interrupt or literal-next while native transfer is unavailable. */
            a->message=key==SDLK_C ? tr(a,"Copy unavailable","Kopieren nicht verfügbar") : tr(a,"Paste unavailable","Einfügen nicht verfügbar");a->dirty=true;
        }
        else if ((mod&SDL_KMOD_CTRL) && (mod&SDL_KMOD_SHIFT) && !(mod&(SDL_KMOD_ALT|SDL_KMOD_GUI)) && key==SDLK_A) {
            (void)ae_terminal_select_all(a->model);a->selecting=false;a->dirty=true;
        }
        else if ((mod&SDL_KMOD_CTRL) && (mod&SDL_KMOD_SHIFT) && !(mod&(SDL_KMOD_ALT|SDL_KMOD_GUI)) &&
                 (key==SDLK_UP || key==SDLK_DOWN || key==SDLK_LEFT || key==SDLK_RIGHT)) {
            (void)ae_terminal_select_step(a->model,key==SDLK_LEFT ? -1 : key==SDLK_RIGHT ? 1 : 0,key==SDLK_UP ? -1 : key==SDLK_DOWN ? 1 : 0);
            a->selecting=false;a->dirty=true;
        }
        else if (key==SDLK_ESCAPE && a->model->selection.count) { if (!a->failed) a->message=NULL;ae_terminal_select_clear(a->model);a->selecting=false;a->dirty=true; }
        else if ((mod&SDL_KMOD_SHIFT) && (key==SDLK_PAGEUP || key==SDLK_PAGEDOWN)) { ae_terminal_scroll(a->model,key==SDLK_PAGEUP ? (int)a->model->rows/2 : -(int)a->model->rows/2);a->dirty=true; }
        else if (key==SDLK_F6) { a->german=!a->german;a->dirty=true; }
        else if (key==SDLK_F7) { a->contrast=!a->contrast;a->dirty=true; }
        else if (key==SDLK_F8) { a->font_points=a->font_points==16 ? 20 : a->font_points==20 ? 24 : a->font_points==24 ? 32 : 16;a->dirty=true; }
        else if ((mod&SDL_KMOD_CTRL) && key>=SDLK_A && key<=SDLK_Z) { unsigned char c=(unsigned char)(key-SDLK_A+1);input(a,&c,1); }
        else if (key==SDLK_RETURN || key==SDLK_KP_ENTER) input(a,"\r",1);
        else if (key==SDLK_BACKSPACE) input(a,"\177",1);
        else if (key==SDLK_TAB) input(a,"\t",1);
        else if (key==SDLK_ESCAPE) input(a,"\033",1);
        else if (key==SDLK_UP || key==SDLK_DOWN || key==SDLK_RIGHT || key==SDLK_LEFT) { char seq[]={27,a->model->application_cursor ? 'O' : '[',key==SDLK_UP ? 'A' : key==SDLK_DOWN ? 'B' : key==SDLK_RIGHT ? 'C' : 'D'};input(a,seq,sizeof(seq)); }
    }
}
int main(int argc,char **argv) {
    struct app a={.pty={.master=-1,.pin=-1,.pid=-1},.wake={-1,-1},.running=true,.dirty=true,.font_points=16,.bar_height=40};
    const char *font=NULL,*cache=NULL,*shell=NULL;
    for (int i=1;i<argc;++i) {
        if (!strcmp(argv[i],"--font") && !font && i+1<argc) font=argv[++i];
        else if (!strcmp(argv[i],"--cache") && !cache && i+1<argc) cache=argv[++i];
        else if (!strcmp(argv[i],"--shell") && !shell && i+1<argc) shell=argv[++i];
        else if (!strcmp(argv[i],"--diagnostics") && !a.diagnostics) a.diagnostics=true;
        else return 2;
    }
    struct stat st;if (!font || !cache || !shell || shell[0]!='/' || cache[0]!='/' || getuid()==0 || getuid()!=geteuid() || getgid()!=getegid() || lstat(cache,&st) || !S_ISDIR(st.st_mode) || st.st_uid!=getuid() || (st.st_mode&07777)!=0700) return 2;
    setvbuf(stdout,NULL,_IOLBF,0);
    struct sigaction child={.sa_handler=SIG_DFL};sigemptyset(&child.sa_mask);
    if (sigaction(SIGCHLD,&child,NULL) || !setlocale(LC_CTYPE,"C.UTF-8") || setenv("TERM","dumb",1) || unsetenv("COLORTERM") || setenv("MESA_SHADER_CACHE_DIR",cache,1) || setenv("MESA_SHADER_CACHE_MAX_SIZE","32M",1)) return 1;
    a.model=malloc(sizeof(*a.model));if (!a.model) return 1;ae_terminal_init(a.model,80,24,width);
    int source=open(shell,O_RDONLY|O_NOFOLLOW|O_CLOEXEC);
    if (source<0 || ae_terminal_pty_open(&a.pty,source,80,24)) { if (source>=0) close(source);free(a.model);return 1; }close(source);
    if (!SDL_SetAppMetadata("Aetheri Terminal","0.0.1","org.aetheri.Terminal") || !SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"wayland") || !SDL_Init(SDL_INIT_VIDEO) || !TTF_Init()) { a.failed=true;goto cleanup; }
    int fm,fn,fp,hm,hn,hp;TTF_GetFreeTypeVersion(&fm,&fn,&fp);TTF_GetHarfBuzzVersion(&hm,&hn,&hp);
    if (SDL_GetVersion()!=SDL_VERSION || TTF_Version()!=SDL_TTF_VERSION || fm!=2 || fn!=15 || fp!=0 || hm!=15 || hn!=0 || hp!=0) { error(&a,"unreviewed SDL/text version");goto cleanup; }
    a.font=TTF_OpenFont(font,16);a.window=SDL_CreateWindow("Aetheri Terminal",1000,700,SDL_WINDOW_RESIZABLE|SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (a.window) a.renderer=SDL_CreateRenderer(a.window,"opengles2");
    if (!a.font || !TTF_FontIsFixedWidth(a.font) || !a.window || !a.renderer || !SDL_SetWindowMinimumSize(a.window,480,320) || !SDL_SetWindowHitTest(a.window,hit_test,&a) || !SDL_StartTextInput(a.window)) { error(&a,"font/window/renderer/input setup");goto cleanup; }
    a.event=SDL_RegisterEvents(1);a.ack=SDL_CreateSemaphore(0);
    if (!a.event || !a.ack || pipe2(a.wake,O_NONBLOCK|O_CLOEXEC)) { error(&a,"event bridge setup");goto cleanup; }
    struct sigaction action={.sa_handler=signalled};sigemptyset(&action.sa_mask);wake_writer=a.wake[1];
    if (sigaction(SIGTERM,&action,NULL) || sigaction(SIGINT,&action,NULL)) { error(&a,"signal setup");goto cleanup; }
    atomic_init(&a.active,true);atomic_init(&a.readable,true);atomic_init(&a.watch_child,true);atomic_init(&a.writeable,false);
    a.worker=SDL_CreateThread(worker,"terminal-ready",&a);if (!a.worker) { error(&a,"PTY event worker");goto cleanup; }
    if (a.diagnostics) printf("READY wayland opengles2 shell=%d\n",a.pty.pid);
    while (a.running) {
        if (a.dirty && !a.failed) draw(&a);
        if (!a.running) break;
        SDL_Event e;if (!SDL_WaitEvent(&e)) { error(&a,"event wait");break; }event(&a,&e);
    }
cleanup:
    if (a.worker) { atomic_store(&a.active,false);notify(&a);SDL_SignalSemaphore(a.ack);SDL_WaitThread(a.worker,NULL); }
    wake_writer=-1;
    if (!a.pty.ended || !a.pty.eof) {
        (void)ae_terminal_pty_hangup(&a.pty);
        while (!a.pty.ended || !a.pty.eof) {
            struct pollfd fds[2]={{a.pty.eof ? -1 : a.pty.master,POLLIN,0},{a.pty.ended ? -1 : a.pty.pin,POLLIN,0}};
            int n;do { n=poll(fds,2,-1); } while (n<0 && errno==EINTR);if (n<0) { a.failed=true;break; }
            unsigned char bytes[4096];if (fds[0].revents) { ssize_t got;do { got=ae_terminal_pty_read(&a.pty,bytes,sizeof(bytes)); } while (got>0); }
            if (fds[1].revents && ae_terminal_pty_observe(&a.pty)<0) { a.failed=true;break; }
        }
    }
    if (ae_terminal_pty_close(&a.pty)) a.failed=true;
    if (a.frame_callback) wl_callback_destroy(a.frame_callback);
    glyphs_clear(&a);SDL_DestroySemaphore(a.ack);for (unsigned i=0;i<2;++i) if (a.wake[i]>=0) close(a.wake[i]);
    SDL_DestroyRenderer(a.renderer);SDL_DestroyWindow(a.window);TTF_CloseFont(a.font);TTF_Quit();SDL_Quit();
    printf("CLOSED frames=%u shell_status=%d failed=%d\n",a.frame,a.pty.status,a.failed);free(a.model);return a.failed ? 1 : 0;
}
