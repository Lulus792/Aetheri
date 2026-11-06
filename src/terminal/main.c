/* Development graphical terminal; normal app admission and full VT remain work. */
#define _GNU_SOURCE
#include "model.h"
#include "session.h"
#include "clipboard.h"
#include "paste.h"
#include "composition.h"
#ifdef AETHERI_TERMINAL_NEXAUI
#include "nexaui_view.h"
#include <nexaui/linux_accessibility_host.h>
#endif
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <errno.h>
#include <fcntl.h>
#include <locale.h>
#include <limits.h>
#include <time.h>
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
enum { GLYPHS=512, TEXTURE_BUDGET=8*1024*1024, INPUT_BYTES=AE_TERM_INPUT_BYTES };
struct glyph { uint32_t code,marks[3];uint8_t count,style;SDL_Texture *texture;float w,h;size_t bytes;uint64_t age; };
struct app {
    struct ae_terminal_session session;
#ifdef AETHERI_TERMINAL_NEXAUI
    struct ae_terminal_view *view;SDL_Texture *view_texture;
    unsigned char *view_font,*view_bold;unsigned view_width,view_height;
    nx_linux_accessibility_host *accessibility;
    SDL_Mutex *accessibility_lock;int accessibility_fd;short accessibility_events;
    uint64_t accessibility_deadline;bool accessibility_available,accessibility_focused;
    SDL_WindowFlags accessibility_visibility;
    unsigned chrome_hover,chrome_pressed;bool chrome_escape;
#endif
    SDL_Window *window;SDL_Renderer *renderer;TTF_Font *font;
    struct glyph glyphs[GLYPHS];size_t texture_bytes;uint64_t age;
    SDL_Thread *worker;SDL_Semaphore *ack;
    atomic_bool active,readable,watch_child,writeable;
    atomic_uint_fast64_t clipboard_deadline;
    struct ae_clipboard *clipboard;int clipboard_fd;
    struct ae_clipboard_observation clipboard_observed;
    struct ae_terminal_paste paste;bool paste_receiving,paste_alt;
    struct ae_terminal_composition composition;
    SDL_Rect input_area;uint64_t text_after;pid_t composition_group;bool composition_alt;
    uint64_t paste_epoch,paste_shown;pid_t paste_group;
    unsigned paste_scroll,paste_total,paste_visible,paste_button;
    int paste_pressed;SDL_FRect paste_buttons[2];
    int wake[2];uint32_t event;
    float density,cell_w,cell_h,left,grid_y,bar_height,footer_height;
    unsigned font_points,applied_points;
    bool running,failed,dirty,diagnostics,german,contrast,close_pressed,selecting,editor;
    unsigned frame,drawn_frame;struct wl_callback *frame_callback;
    const char *message;char ended_message[128];
};
static void paste_cancel(struct app *a,const char *message);
static void clipboard_update(struct app *a);
static void composition_cancel(struct app *a,bool reset);
static bool composition_context(struct app *a);
static void user_close(struct app *a);
#ifdef AETHERI_TERMINAL_NEXAUI
static void chrome_action(struct app *a,unsigned action);
#endif
static volatile sig_atomic_t wake_writer=-1,stop_signal;
static void signalled(int number) { int saved=errno;stop_signal=number;unsigned char byte=1;if (wake_writer>=0) (void)write(wake_writer,&byte,1);errno=saved; }
static void notify(struct app *a) { unsigned char byte=1;if (a->wake[1]>=0) (void)write(a->wake[1],&byte,1); }
static int worker(void *data) {
    struct app *a=data;
    while (atomic_load(&a->active)) {
        short io=(short)((atomic_load(&a->readable) ? POLLIN : 0)|(atomic_load(&a->writeable) ? POLLOUT : 0));
        struct pollfd fds[5]={{io ? a->session.pty.master : -1,io,0},{atomic_load(&a->watch_child) ? a->session.pty.pin : -1,POLLIN,0},{a->wake[0],POLLIN,0},{a->clipboard_fd,POLLIN,0},{-1,0,0}};
        int timeout=-1;uint64_t deadline=atomic_load(&a->clipboard_deadline);
#ifdef AETHERI_TERMINAL_NEXAUI
        /* Only readiness crosses threads. Duplicate under the lock so a bridge
           reconnect cannot close/reuse the object while poll still borrows it. */
        SDL_LockMutex(a->accessibility_lock);
        bool watch_failed=false;
        if (a->accessibility_fd>=0) {fds[4].fd=fcntl(a->accessibility_fd,F_DUPFD_CLOEXEC,0);watch_failed=fds[4].fd<0;}
        fds[4].events=a->accessibility_events;
        if (a->accessibility_deadline && (!deadline || a->accessibility_deadline<deadline)) deadline=a->accessibility_deadline;
        SDL_UnlockMutex(a->accessibility_lock);
#endif
        if (deadline) {
            struct timespec t;
            if (clock_gettime(CLOCK_MONOTONIC,&t)) timeout=0;
            else { uint64_t now=(uint64_t)t.tv_sec*1000+(uint64_t)t.tv_nsec/1000000;timeout=deadline<=now ? 0 : deadline-now>(uint64_t)INT_MAX ? INT_MAX : (int)(deadline-now); }
        }
        int n;
#ifdef AETHERI_TERMINAL_NEXAUI
        if (watch_failed) n=-1;else
#endif
        do { n=poll(fds,5,timeout); } while (n<0 && errno==EINTR);
        if (fds[4].fd>=0) close(fds[4].fd);
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
    if (a->session.closing) return;
    int result=ae_terminal_session_request_close(&a->session),saved=errno;a->dirty=true;a->selecting=false;paste_cancel(a,NULL);composition_cancel(a,true);
    if (a->session.pty.ended && a->session.pty.eof) { a->running=false;return; }
    if (result && saved!=ESRCH) { a->failed=true;a->message="Shell hangup could not be acknowledged"; }
}
static void error(struct app *a,const char *message) { fprintf(stderr,"terminal: %s: %s\n",message,SDL_GetError());a->failed=true;a->message=message;close_request(a); }
#ifdef AETHERI_TERMINAL_NEXAUI
static bool accessibility_result(nx_result result) {return result==NX_OK || result==NX_ERROR_UNSUPPORTED;}
static void accessibility_publish(struct app *a) {
    int w,h;if (!SDL_GetWindowSize(a->window,&w,&h)) {error(a,"accessibility geometry");return;}
    SDL_WindowFlags flags=SDL_GetWindowFlags(a->window);
    nx_linux_accessibility_geometry g={.width=w,.height=h,
        .visible=!(flags&(SDL_WINDOW_HIDDEN|SDL_WINDOW_MINIMIZED)),.focused=SDL_GetKeyboardFocus()==a->window};
    nx_result result=nx_linux_accessibility_host_publish(a->accessibility,ae_terminal_view_context(a->view),&g);
    if (!accessibility_result(result)) {error(a,"accessibility publication");return;}
    bool available=result==NX_OK;
    a->accessibility_visibility=flags&(SDL_WINDOW_HIDDEN|SDL_WINDOW_MINIMIZED);
    a->accessibility_focused=g.focused!=0;
    if (a->diagnostics && available!=a->accessibility_available) printf("ACCESSIBILITY available=%d\n",available);
    a->accessibility_available=available;
}
static void accessibility_prepare(struct app *a) {
    if (!a->accessibility_available ||
        a->accessibility_visibility!=(SDL_GetWindowFlags(a->window)&(SDL_WINDOW_HIDDEN|SDL_WINDOW_MINIMIZED)) ||
        a->accessibility_focused!=(SDL_GetKeyboardFocus()==a->window)) accessibility_publish(a);
    if (a->failed) return;
    size_t routed=0;int focus=0;
    nx_result result=nx_linux_accessibility_host_route(a->accessibility,ae_terminal_view_context(a->view),&routed,&focus);
    if (!accessibility_result(result)) {error(a,"accessibility routing");return;}
    /* The pinned bridge admits row focus only in an already active window.
       Routing logical reading focus is not a cross-window activation grant. */
    if (focus && a->diagnostics) printf("ACCESSIBILITY read_focus native=%d\n",SDL_GetKeyboardFocus()==a->window);
    if (routed) a->dirty=true;
    /* Borrow readiness only after every operation that can change it. A routed
       reply can change write interest; no SDK call may invalidate this snapshot
       before its duplicate is captured below. Newly pumped requests wake us. */
    int fd=-1,writable=0;uint32_t timeout=UINT32_MAX;
    result=nx_linux_accessibility_host_prepare(a->accessibility,&fd,&writable,&timeout);
    if (!accessibility_result(result)) {error(a,"accessibility dispatch");return;}
    if (result==NX_ERROR_UNSUPPORTED) timeout=1000;
    struct timespec time;
    if (clock_gettime(CLOCK_MONOTONIC,&time)) {error(a,"accessibility clock");return;}
    uint64_t now=(uint64_t)time.tv_sec*1000u+(uint64_t)time.tv_nsec/1000000u;
    uint64_t deadline=timeout==UINT32_MAX?0:now+timeout;
    short events=(short)(POLLIN|(writable?POLLOUT:0));bool wake=false;
    SDL_LockMutex(a->accessibility_lock);
    bool same=fd<0 && a->accessibility_fd<0;
    if (fd>=0 && a->accessibility_fd>=0) {
        struct stat original,held;
        same=!fstat(fd,&original) && !fstat(a->accessibility_fd,&held) && original.st_dev==held.st_dev && original.st_ino==held.st_ino;
    }
    int next=same?a->accessibility_fd:fd<0?-1:fcntl(fd,F_DUPFD_CLOEXEC,0);
    bool failed=fd>=0 && next<0;
    if (!failed) {
        wake=!same || a->accessibility_events!=events ||
            (deadline && (!a->accessibility_deadline || deadline<a->accessibility_deadline));
        if (!same && a->accessibility_fd>=0) close(a->accessibility_fd);
        a->accessibility_fd=next;a->accessibility_events=events;a->accessibility_deadline=deadline;
    }
    SDL_UnlockMutex(a->accessibility_lock);
    if (failed) error(a,"accessibility readiness handle");else if (wake) notify(a);
}
#endif
static int width(uint32_t code) { return code<=0x10ffff ? wcwidth((wchar_t)code) : -1; }
static bool text_input_wanted(struct app *a) {
    return !a->session.closing && !a->session.pty.ended && !a->session.pty.eof && SDL_GetKeyboardFocus()==a->window && !(a->paste.size && a->paste.review);
}
static void composition_cancel(struct app *a,bool reset) {
    bool active=a->composition.size!=0;ae_terminal_composition_clear(&a->composition);
    if (active) a->dirty=true;
    if (reset && a->window) {
        /* ClearComposition is a successful no-op in the pinned Wayland backend.
           Disable/re-enable its actual text-input state instead. Already queued
           editing/text is fenced by owner time, never adopted as a new draft. */
        a->text_after=SDL_GetTicksNS();
        if (!SDL_StopTextInput(a->window) || (text_input_wanted(a) && !SDL_StartTextInput(a->window))) error(a,"composition reset");
    }
}
static void text_input_sync(struct app *a) {
    bool wanted=text_input_wanted(a);
    if (wanted==SDL_TextInputActive(a->window)) return;
    if (!wanted) { composition_cancel(a,false);a->text_after=SDL_GetTicksNS(); }
    if (!(wanted ? SDL_StartTextInput(a->window) : SDL_StopTextInput(a->window))) error(a,"text input activation");
}
static bool composition_context(struct app *a) {
    return !a->session.closing && !a->session.pty.eof && !a->session.pty.ended && SDL_GetKeyboardFocus()==a->window &&
        a->session.model->alt==a->composition_alt && tcgetpgrp(a->session.pty.master)==a->composition_group && ae_terminal_pty_observe(&a->session.pty)==0;
}
static void glyphs_clear(struct app *a) {
    for (unsigned i=0;i<GLYPHS;++i) { SDL_DestroyTexture(a->glyphs[i].texture);a->glyphs[i]=(struct glyph){0}; }
    a->texture_bytes=0;
}
#ifndef AETHERI_TERMINAL_NEXAUI
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
#endif
static bool set_clip(struct app *a,const SDL_Rect *r) {
#ifdef AETHERI_TERMINAL_NEXAUI
    nx_rect clip={0};if (r) clip=(nx_rect){(float)r->x,(float)r->y,(float)r->w,(float)r->h};
    return ae_terminal_view_clip(a->view,r?&clip:NULL);
#else
    return SDL_SetRenderClipRect(a->renderer,r);
#endif
}
static void fill(struct app *a,SDL_FRect r,uint32_t color) {
#ifdef AETHERI_TERMINAL_NEXAUI
    if (!ae_terminal_view_fill(a->view,(nx_rect){r.x,r.y,r.w,r.h},color)) error(a,"NexaUI rectangle");
#else
    if (!SDL_SetRenderDrawColor(a->renderer,(uint8_t)(color>>16),(uint8_t)(color>>8),(uint8_t)color,255) || !SDL_RenderFillRect(a->renderer,&r)) error(a,"rectangle");
#endif
}
static void label(struct app *a,const char *text,float x,float y,uint32_t color) {
#ifdef AETHERI_TERMINAL_NEXAUI
    if (!ae_terminal_view_label(a->view,text,x,y,color)) error(a,"NexaUI text");
#else
    TTF_SetFontStyle(a->font,TTF_STYLE_NORMAL);
    SDL_Surface *s=TTF_RenderText_Blended(a->font,text,0,(SDL_Color){(uint8_t)(color>>16),(uint8_t)(color>>8),(uint8_t)color,255});
    if (!s) { error(a,"label rasterization");return; }
    SDL_Texture *t=SDL_CreateTextureFromSurface(a->renderer,s);SDL_FRect r={x,y,(float)s->w/a->density,(float)s->h/a->density};SDL_DestroySurface(s);
    if (!t || !SDL_RenderTexture(a->renderer,t,NULL,&r)) error(a,"label presentation");
    SDL_DestroyTexture(t);
#endif
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
        a->bar_height=fmaxf(40,a->cell_h+18);a->footer_height=fmaxf(28,a->cell_h+8);
    }
#ifdef AETHERI_TERMINAL_NEXAUI
    struct ae_terminal_chrome chrome;
    if (!ae_terminal_chrome_layout((float)w,(float)h,(float)a->font_points/16,&chrome)) {error(a,"chrome geometry");return;}
    a->bar_height=chrome.bar.height;
#endif
    a->grid_y=a->bar_height+12;
    int cols=(int)floorf(((float)w-32)/a->cell_w),rows=(int)floorf(((float)h-a->grid_y-a->footer_height-16)/a->cell_h);
    if (cols<2) cols=2;
    if (cols>AE_TERM_COLS) cols=AE_TERM_COLS;
    if (rows<2) rows=2;
    if (rows>AE_TERM_ROWS) rows=AE_TERM_ROWS;
    if (a->session.model->cols!=(unsigned)cols || a->session.model->rows!=(unsigned)rows) {
        a->selecting=false;
        if (ae_terminal_session_resize(&a->session,(unsigned)cols,(unsigned)rows)) error(a,"PTY/grid resize");
    }
    a->left=((float)w-(float)cols*a->cell_w)/2;
}
static SDL_FRect paste_card(int w,int h) {
    float cw=fminf((float)w-48,800),ch=fminf((float)h-96,460);
    return (SDL_FRect){((float)w-cw)/2,((float)h-ch)/2,cw,ch};
}
static void draw_composition(struct app *a) {
    float x=a->left+(float)a->session.model->x*a->cell_w,y=a->grid_y+(float)a->session.model->y*a->cell_h;
    unsigned cursor=0;
    if (a->composition.size) {
        unsigned columns=a->session.model->cols-a->session.model->x;
        if (columns<16) { columns=a->session.model->cols;x=a->left; }
        struct ae_terminal_composition_view view;
        if (!ae_terminal_composition_view(&a->composition,columns,width,&view)) { error(a,"composition viewport");return; }
        cursor=view.cursor;
        unsigned occupied=view.cells<columns ? view.cells+1 : columns;
        SDL_Rect clip={(int)x,(int)y,(int)ceilf((float)columns*a->cell_w),(int)ceilf(a->cell_h)};
        if (!set_clip(a,&clip)) { error(a,"composition clip");return; }
        fill(a,(SDL_FRect){x,y,(float)occupied*a->cell_w,a->cell_h},0x263043);
        if (view.selection_end>view.selection_begin)
            fill(a,(SDL_FRect){x+(float)view.selection_begin*a->cell_w,y,(float)(view.selection_end-view.selection_begin)*a->cell_w,a->cell_h},0x304e78);
        char visible[AE_TERM_COMPOSITION_BYTES+1];size_t size=view.end-view.begin;
        memcpy(visible,a->composition.text+view.begin,size);visible[size]=0;
        if (size) label(a,visible,x,y,0xffffff);
        fill(a,(SDL_FRect){x,y+a->cell_h-2,(float)occupied*a->cell_w,2},0xa2c7ff);
        if (view.caret) fill(a,(SDL_FRect){x+(float)cursor*a->cell_w,y,2,a->cell_h},0xffffff);
        if (view.hidden_before) fill(a,(SDL_FRect){x,y,2,a->cell_h},0xa2c7ff);
        if (view.hidden_after) fill(a,(SDL_FRect){x+(float)columns*a->cell_w-2,y,2,a->cell_h},0xa2c7ff);
        (void)set_clip(a,NULL);
    }
    SDL_Rect area={(int)floorf(x+(float)cursor*a->cell_w),(int)floorf(y),(int)ceilf(a->cell_w),(int)ceilf(a->cell_h)};
    if (area.x!=a->input_area.x || area.y!=a->input_area.y || area.w!=a->input_area.w || area.h!=a->input_area.h) {
        if (!SDL_SetTextInputArea(a->window,&area,0)) { error(a,"text input area");return; }
        a->input_area=area;
        if (a->diagnostics) printf("TEXT area=%d,%d,%d,%d\n",area.x,area.y,area.w,area.h);
    }
}
static void draw_paste(struct app *a,int w,int h) {
    if (!a->paste.size || !a->paste.review) return;
    SDL_FRect card=paste_card(w,h);fill(a,card,a->contrast ? 0 : 0x1c2432);
    fill(a,(SDL_FRect){card.x,card.y,card.w,2},0xa2c7ff);
    label(a,tr(a,"Review paste","Einfügen prüfen"),card.x+24,card.y+12,0xffffff);
    char summary[96];snprintf(summary,sizeof(summary),a->german ? "%u Zeilen · %zu Bytes" : "%u lines · %zu bytes",a->paste.lines,a->paste.size);
    label(a,summary,card.x+24,card.y+16+a->cell_h,0xb2c0d6);
    unsigned columns=(unsigned)fmaxf(16,fminf(120,floorf((card.w-48)/a->cell_w)));
    float top=card.y+2*a->cell_h+28,button_h=a->cell_h+18;
    float buttons_y=card.y+card.h-button_h-16,help_y=buttons_y-a->cell_h-12;
    a->paste_visible=(unsigned)fmaxf(1,floorf((help_y-top-12)/a->cell_h));
    char line[AE_TERM_PREVIEW_BYTES];size_t offset=0;a->paste_total=0;
    while (offset<a->paste.size) {
        size_t next=ae_terminal_paste_preview(&a->paste,offset,columns,width,line);
        if (next<=offset) { error(a,"paste preview bounds");return; }
        offset=next;++a->paste_total;
    }
    unsigned maximum=a->paste_total>a->paste_visible ? a->paste_total-a->paste_visible : 0;
    if (a->paste_scroll>maximum) a->paste_scroll=maximum;
    offset=0;
    for (unsigned i=0;i<a->paste_scroll;++i) offset=ae_terminal_paste_preview(&a->paste,offset,columns,width,line);
    SDL_Rect clip={(int)card.x+24,(int)top,(int)card.w-48,(int)(help_y-top-8)};
    if (!set_clip(a,&clip)) { error(a,"paste preview clip");return; }
    for (unsigned i=0;i<a->paste_visible && offset<a->paste.size;++i) {
        offset=ae_terminal_paste_preview(&a->paste,offset,columns,width,line);
        label(a,line,card.x+24,top+(float)i*a->cell_h,0xeff3fb);
    }
    (void)set_clip(a,NULL);
    label(a,tr(a,"Ctrl+Enter: paste · Esc: cancel","Strg+Enter: einfügen · Esc: abbrechen"),card.x+24,help_y,0xb2c0d6);
    for (unsigned i=0;i<2;++i) {
        a->paste_buttons[i]=(SDL_FRect){card.x+24+(float)i*(card.w/2-12),buttons_y,(card.w-72)/2,button_h};
        fill(a,a->paste_buttons[i],a->paste_button==i ? 0x304e78 : 0x263043);
        label(a,i ? tr(a,"Paste","Einfügen") : tr(a,"Cancel","Abbrechen"),a->paste_buttons[i].x+12,buttons_y+8,0xffffff);
    }
}
static void draw(struct app *a) {
    layout(a);int w,h,pw,ph;
    if (!SDL_GetWindowSize(a->window,&w,&h) || !SDL_GetWindowSizeInPixels(a->window,&pw,&ph) ||
            !SDL_SetRenderScale(a->renderer,(float)pw/(float)w,(float)ph/(float)h) ||
            !SDL_SetRenderDrawColor(a->renderer,16,20,28,255) || !SDL_RenderClear(a->renderer)) { error(a,"frame setup");return; }
#ifdef AETHERI_TERMINAL_NEXAUI
    if (!ae_terminal_view_begin(a->view,(float)w,(float)h,(float)pw/(float)w,(float)a->font_points,
        a->cell_w,a->cell_h,(float)TTF_GetFontAscent(a->font)/a->density)) { error(a,"NexaUI frame");return; }
#endif
#ifdef AETHERI_TERMINAL_NEXAUI
    SDL_WindowFlags flags=SDL_GetWindowFlags(a->window);
    struct ae_terminal_chrome_state chrome={.german=a->german,.contrast=a->contrast,.editor=a->editor,
        .maximized=(flags&SDL_WINDOW_MAXIMIZED)!=0,.zoom_disabled=a->session.closing || a->composition.size ||
        (a->paste.size && a->paste.review) || (flags&SDL_WINDOW_FULLSCREEN),.hovered=a->chrome_hover,.pressed=a->chrome_pressed};
    unsigned chrome_request=0;
    if (!ae_terminal_view_chrome(a->view,&chrome,&chrome_request)) {error(a,"NexaUI chrome");return;}
    if (!ae_terminal_view_grid(a->view,a->session.model,(nx_rect){a->left,a->grid_y,(float)a->session.model->cols*a->cell_w,(float)a->session.model->rows*a->cell_h},a->contrast,!a->session.pty.ended,
        !a->session.closing && !a->composition.size && !(a->paste.size && a->paste.review))) { error(a,"NexaUI grid");return; }
#else
    fill(a,(SDL_FRect){0,0,(float)w,a->bar_height},a->contrast ? 0 : 0x191f2b);
    label(a,a->editor ? tr(a,"Terminal · Aetheri editor","Terminal · Aetheri-Editor") : tr(a,"Terminal · Aetheri shell","Terminal · Aetheri-Shell"),16,10,0xeff3fb);
    label(a,"×",(float)w-32,10,0xeff3fb);
    for (unsigned y=0;y<a->session.model->rows;++y) {
        const struct ae_term_cell *r=ae_terminal_row(a->session.model,y);
        unsigned first=0,last=0;bool selected=ae_terminal_selected_range(a->session.model,y,&first,&last);
        for (unsigned x=0;x<a->session.model->cols;++x) {
            uint32_t bg=(r[x].style&AE_TERM_REVERSE) ? r[x].fg : r[x].bg;
            if (a->contrast && bg==0x10141c) bg=0;
            if (selected && x>=first && x<last) bg=0x304e78;
            fill(a,(SDL_FRect){a->left+(float)x*a->cell_w,a->grid_y+(float)y*a->cell_h,a->cell_w,a->cell_h},bg);
        }
        for (unsigned x=0;x<a->session.model->cols;++x) {
            const struct ae_term_cell *c=&r[x];uint32_t fg=c->fg,bg=c->bg;
            if (c->style&AE_TERM_REVERSE) { uint32_t temp=fg;fg=bg;bg=temp; }
            if (a->contrast && bg==0x10141c) { bg=0;fg=fg==0xdce4ef ? 0xffffff : fg; }
            if (selected && x>=first && x<last) fg=0xffffff;
            SDL_FRect box={a->left+(float)x*a->cell_w,a->grid_y+(float)y*a->cell_h,a->cell_w,a->cell_h};
            if (c->width && c->code!=' ' && c->code) {
                struct glyph *g=glyph(a,c);SDL_FRect dest={box.x,box.y,g ? g->w : 0,g ? g->h : 0};
                SDL_Rect clip={(int)box.x,(int)box.y,(int)ceilf(a->cell_w*(float)c->width),(int)ceilf(a->cell_h)};
                if (g && (!set_clip(a,&clip) || !SDL_SetTextureColorMod(g->texture,(uint8_t)(fg>>16),(uint8_t)(fg>>8),(uint8_t)fg) || !SDL_RenderTexture(a->renderer,g->texture,NULL,&dest))) error(a,"cell presentation");
                (void)set_clip(a,NULL);
            }
            if (c->style&AE_TERM_UNDERLINE) fill(a,(SDL_FRect){box.x,box.y+box.h-2,box.w,1},fg);
        }
    }
#endif
    if (a->session.model->cursor && !a->session.model->view && !a->session.pty.ended) {
        SDL_FRect cursor={a->left+(float)a->session.model->x*a->cell_w,a->grid_y+(float)a->session.model->y*a->cell_h,a->cell_w,2};cursor.y+=a->cell_h-2;fill(a,cursor,0xa2c7ff);
    }
    draw_composition(a);
    const char *status=a->message ? a->message : a->session.closing ? (a->editor ? tr(a,"Closing · waiting for editor","Schließen · Warten auf Editor") : tr(a,"Closing · waiting for shell/jobs","Schließen · Warten auf Shell/Jobs")) :
        a->session.pty.ended ? (a->editor ? tr(a,"Editor ended · waiting for terminal writers","Editor beendet · Warten auf Terminalausgabe") : tr(a,"Shell ended · waiting for terminal writers","Shell beendet · Warten auf Terminalausgabe")) :
        a->composition.size ? tr(a,"Composition · Esc cancels","Texteingabe · Esc bricht ab") :
        a->session.model->selection.count ? tr(a,"Selection · Esc clears","Auswahl · Esc löscht") :
        a->editor ? tr(a,"Ctrl+S save · Ctrl+Q document close · Ctrl+Shift+Q window close","Strg+S speichern · Strg+Q Dokument · Strg+Shift+Q Fenster") : tr(a,"Ctrl+C/Z · Shift+PgUp/PgDn · Ctrl+Shift+Q","Strg+C/Z · Shift+Bild↑/↓ · Strg+Shift+Q");
#ifdef AETHERI_TERMINAL_NEXAUI
    if (!a->message && !a->session.pty.ended && ae_terminal_view_reading(a->view) && !a->session.closing && !a->composition.size && !(a->paste.size && a->paste.review))
        status=tr(a,"Reading output · Ctrl+Shift+R clears focus","Lesefokus · Strg+Umschalt+R löst ihn");
#endif
    label(a,status,16,(float)h-a->footer_height,0xb2c0d6);
    draw_paste(a,w,h);
    if (a->diagnostics) {
        if (a->frame_callback) wl_callback_destroy(a->frame_callback);
        struct wl_surface *surface=SDL_GetPointerProperty(SDL_GetWindowProperties(a->window),SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER,NULL);
        a->frame_callback=surface ? wl_surface_frame(surface) : NULL;a->drawn_frame=a->frame+1;
        if (!a->frame_callback || wl_callback_add_listener(a->frame_callback,&frame_listener,a)) { error(a,"frame observation");return; }
    }
#ifdef AETHERI_TERMINAL_NEXAUI
    const unsigned char *pixels=NULL;size_t stride=0;
    if (!ae_terminal_view_finish(a->view,(unsigned)pw,(unsigned)ph,&pixels,&stride) || stride>INT_MAX) {error(a,"NexaUI raster");return;}
    if (a->view_width!=(unsigned)pw || a->view_height!=(unsigned)ph) {
        SDL_Texture *texture=SDL_CreateTexture(a->renderer,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_STREAMING,pw,ph);
        if (!texture || !SDL_SetTextureScaleMode(texture,SDL_SCALEMODE_NEAREST)) {SDL_DestroyTexture(texture);error(a,"NexaUI canvas");return;}
        SDL_DestroyTexture(a->view_texture);a->view_texture=texture;a->view_width=(unsigned)pw;a->view_height=(unsigned)ph;
    }
    if (!SDL_UpdateTexture(a->view_texture,NULL,pixels,(int)stride) || !SDL_SetRenderScale(a->renderer,1,1) ||
        !SDL_RenderTexture(a->renderer,a->view_texture,NULL,NULL)) {error(a,"NexaUI canvas upload");return;}
#endif
    if (!SDL_RenderPresent(a->renderer)) { error(a,"present");return; }
#ifdef AETHERI_TERMINAL_NEXAUI
    accessibility_publish(a);
    if (a->diagnostics) {printf("NEXAUI_FRAME %u width=%d height=%d stride=%zu\n",a->frame+1,pw,ph,stride);printf("READ_FOCUS id=%llu\n",(unsigned long long)nx_focus_id(ae_terminal_view_context(a->view)));printf("CHROME maximized=%d header=%.1f\n",chrome.maximized,a->bar_height);}
#endif
    a->dirty=false;++a->frame;
    if (a->paste.size && a->paste.review && !a->paste_shown) a->paste_shown=SDL_GetTicksNS();
    if (a->diagnostics) printf("VIEW %u cols=%u rows=%u x=%u y=%u history=%u view=%u sequence=%llu closing=%d ended=%d eof=%d german=%d contrast=%d points=%u width=%d height=%d selection=%zu selecting=%d paste=%zu receiving=%d review=%d preview_scroll=%u preview_total=%u composition=%zu grid_left=%.3f grid_top=%.3f cell_width=%.3f cell_height=%.3f\n",a->frame,a->session.model->cols,a->session.model->rows,a->session.model->x,a->session.model->y,a->session.model->history_count,a->session.model->view,(unsigned long long)a->session.model->sequence,a->session.closing,a->session.pty.ended,a->session.pty.eof,a->german,a->contrast,a->font_points,w,h,ae_terminal_selection_size(a->session.model),a->selecting,a->paste.size,a->paste_receiving,a->paste.size && a->paste.review,a->paste_scroll,a->paste_total,a->composition.size,(double)a->left,(double)a->grid_y,(double)a->cell_w,(double)a->cell_h);
#ifdef AETHERI_TERMINAL_NEXAUI
    if (chrome_request) chrome_action(a,chrome_request);
#endif
}
static void flush_input(struct app *a) {
    if (ae_terminal_session_flush(&a->session)) error(a,"PTY input");
    atomic_store(&a->writeable,a->session.input_count!=0 && !a->session.pty.eof);
}
static bool enqueue(struct app *a,const void *bytes,size_t size) {
    bool selected=a->session.model->selection.count!=0,scrolled=a->session.model->view!=0;
    if (!ae_terminal_session_enqueue(&a->session,bytes,size)) {
        if (errno==ENOBUFS) { a->message="Input queue full; new input was not sent";a->dirty=true; }
        return false;
    }
    if (selected) { a->selecting=false;a->dirty=true; }
    if (scrolled) a->dirty=true;
    a->message=NULL;flush_input(a);
    if (a->diagnostics) printf("INPUT %llu\n",(unsigned long long)a->session.input_bytes);
    if (a->session.input_count) notify(a);
    return true;
}
static void paste_cancel(struct app *a,const char *message) {
    if (a->clipboard) (void)ae_clipboard_cancel(a->clipboard);
    a->paste_receiving=false;ae_terminal_paste_clear(&a->paste);a->paste_epoch=a->paste_shown=0;
    a->paste_scroll=a->paste_total=a->paste_button=0;a->paste_pressed=-1;
    if (message) a->message=message;
    a->dirty=true;
}
static void input(struct app *a,const void *bytes,size_t size) {
    if (a->composition.size) composition_cancel(a,true);
    if (size && (a->paste_receiving || a->paste.size)) paste_cancel(a,NULL);
    (void)enqueue(a,bytes,size);
}
static void user_close(struct app *a) {
    if (a->editor && !a->session.closing && !a->session.pty.ended && !a->session.pty.eof && tcgetpgrp(a->session.pty.master)==a->session.pty.pid) input(a,"\021",1);
    else close_request(a);
}
static bool paste_context(struct app *a) {
    if (!a->clipboard || a->session.closing || a->session.pty.eof || a->session.pty.ended || !a->paste_epoch ||
        ae_clipboard_focus_epoch(a->clipboard)!=a->paste_epoch || a->session.model->alt!=a->paste_alt) return false;
    if (ae_terminal_pty_observe(&a->session.pty)!=0) return false;
    return a->paste_group>0 && tcgetpgrp(a->session.pty.master)==a->paste_group;
}
static void paste_apply(struct app *a) {
    if (!paste_context(a)) { paste_cancel(a,tr(a,"Paste cancelled","Einfügen abgebrochen"));return; }
    if (a->paste.size && enqueue(a,a->paste.text,a->paste.size)) {
        ae_terminal_paste_clear(&a->paste);a->paste_epoch=a->paste_shown=0;a->paste_pressed=-1;a->dirty=true;
        if (a->diagnostics) puts("CLIPBOARD paste queued");
    }
}
static void clipboard_copy(struct app *a) {
    if (a->session.closing) return;
    paste_cancel(a,NULL);composition_cancel(a,true);size_t needed=ae_terminal_selection_size(a->session.model);
    if (needed<2) { a->message=tr(a,"No text selected","Kein Text ausgewählt");return; }
    char *text=malloc(needed);
    if (!text || !ae_terminal_selection_copy(a->session.model,text,needed)) { free(text);a->message=tr(a,"Copy failed","Kopieren fehlgeschlagen");return; }
    int result=a->clipboard ? ae_clipboard_publish(a->clipboard,&text,needed-1) : EOPNOTSUPP;free(text);
    a->message=result ? tr(a,"Copy unavailable","Kopieren nicht verfügbar") : tr(a,"Selection copied","Auswahl kopiert");
    if (a->diagnostics) printf("CLIPBOARD copy result=%d bytes=%zu\n",result,needed-1);
}
static void clipboard_paste(struct app *a) {
    if (a->session.closing || a->session.pty.ended || a->session.pty.eof) return;
    paste_cancel(a,NULL);composition_cancel(a,true);a->paste_epoch=ae_clipboard_focus_epoch(a->clipboard);a->paste_group=tcgetpgrp(a->session.pty.master);a->paste_alt=a->session.model->alt;
    int result=a->clipboard && a->paste_epoch && a->paste_group>0 ? ae_clipboard_receive(a->clipboard,AE_TERM_PASTE_BYTES) : EACCES;
    a->paste_receiving=result==0;a->message=result ? tr(a,"Paste unavailable","Einfügen nicht verfügbar") : tr(a,"Reading clipboard…","Zwischenablage wird gelesen…");
    if (a->diagnostics) printf("CLIPBOARD receive result=%d\n",result);
}
static void clipboard_wake(void *data) { notify(data); }
static void clipboard_update(struct app *a) {
    if (!a->clipboard) return;
    if (ae_clipboard_pump(a->clipboard)) { error(a,"clipboard connection");return; }
    if ((a->paste_receiving || a->paste.size) && !paste_context(a)) paste_cancel(a,tr(a,"Paste cancelled","Einfügen abgebrochen"));
    char *bytes=NULL;size_t size=0;enum ae_clipboard_result result=ae_clipboard_result(a->clipboard,&bytes,&size);
    if (result==AE_CLIPBOARD_DATA) {
        if (a->paste_receiving && paste_context(a)) {
            enum ae_term_paste_result checked=ae_terminal_paste_set(&a->paste,bytes,size);
            if (a->diagnostics) printf("CLIPBOARD validated=%d bytes=%zu review=%d\n",checked,size,a->paste.review);
            a->paste_receiving=false;a->paste_scroll=a->paste_total=a->paste_button=0;a->paste_shown=0;a->paste_pressed=-1;
            if (checked==AE_TERM_PASTE_OK) {
                a->message=NULL;
                if (!a->paste.review) paste_apply(a);
            } else a->message=checked==AE_TERM_PASTE_EMPTY ? tr(a,"Clipboard is empty","Zwischenablage ist leer") : tr(a,"Clipboard text was rejected","Text der Zwischenablage wurde abgelehnt");
        }
        free(bytes);a->dirty=true;
    } else if (result!=AE_CLIPBOARD_NONE && result!=AE_CLIPBOARD_PENDING) {
        if (a->diagnostics) printf("CLIPBOARD receive_end=%d\n",result);
        bool requested=a->paste_receiving;a->paste_receiving=false;
        if (requested) {
            a->message=result==AE_CLIPBOARD_LIMIT ? tr(a,"Paste exceeds 4096 bytes","Einfügen überschreitet 4096 Bytes") :
                result==AE_CLIPBOARD_TIMEOUT ? tr(a,"Clipboard transfer timed out","Übertragung der Zwischenablage abgelaufen") :
                result==AE_CLIPBOARD_NO_TEXT ? tr(a,"No clipboard text available","Kein Text in der Zwischenablage") :
                tr(a,"Paste cancelled","Einfügen abgebrochen");a->dirty=true;
        }
    }
    uint64_t deadline=ae_clipboard_deadline(a->clipboard);
    if (atomic_exchange(&a->clipboard_deadline,deadline)!=deadline) notify(a);
    struct ae_clipboard_observation state={0};
    if (!ae_clipboard_observe(a->clipboard,&state)) {
        struct ae_clipboard_observation old=a->clipboard_observed;
        if (a->diagnostics && (state.bytes!=old.bytes || state.sources!=old.sources || state.sends!=old.sends ||
            state.receive!=old.receive || state.sync_pending!=old.sync_pending || state.available!=old.available || state.focused!=old.focused))
            printf("CLIPBOARD state sends=%u sources=%u bytes=%zu receiving=%d barrier=%d available=%d focused=%d\n",
                state.sends,state.sources,state.bytes,state.receive,state.sync_pending,state.available,state.focused);
        a->clipboard_observed=state;
    }
}
static void io_event(struct app *a) {
    if (stop_signal) close_request(a);
    bool changed=false;
    if (ae_terminal_session_read(&a->session,&changed)) error(a,"PTY output");
    if (changed) { if (!a->session.model->selection.count) a->selecting=false;a->dirty=true; }
    bool ended=a->session.pty.ended;
    if (!a->session.pty.ended && ae_terminal_pty_observe(&a->session.pty)<0) error(a,"original shell wait");
    if (a->session.pty.ended!=ended) a->dirty=true;
    if (a->composition.size && !composition_context(a)) composition_cancel(a,true);
    atomic_store(&a->readable,!a->session.pty.eof);atomic_store(&a->watch_child,!a->session.pty.ended);
    flush_input(a);
    if (a->session.pty.ended && a->session.pty.eof) {
        if (a->session.closing || a->session.pty.status==0) a->running=false;
        else { snprintf(a->ended_message,sizeof(a->ended_message),"%s %d · %s",a->editor ? tr(a,"Editor ended:","Editor beendet:") : tr(a,"Shell ended:","Shell beendet:"),a->session.pty.status,tr(a,"Close to leave","Schließen zum Beenden"));a->message=a->ended_message;a->dirty=true; }
    }
}
#ifdef AETHERI_TERMINAL_NEXAUI
static bool chrome_zoom_allowed(struct app *a) {
    return !a->session.closing && !a->composition.size && !(a->paste.size && a->paste.review) &&
        !(SDL_GetWindowFlags(a->window)&SDL_WINDOW_FULLSCREEN);
}
static void chrome_action(struct app *a,unsigned action) {
    if (action==AE_TERM_CHROME_CLOSE) user_close(a);
    else if (action==AE_TERM_CHROME_ZOOM && chrome_zoom_allowed(a)) {
        bool maximized=(SDL_GetWindowFlags(a->window)&SDL_WINDOW_MAXIMIZED)!=0;
        bool okay=maximized?SDL_RestoreWindow(a->window):SDL_MaximizeWindow(a->window);
        if (a->diagnostics) printf("CHROME request=zoom observed=%d accepted=%d\n",maximized,okay);
        if (!okay) {a->message=tr(a,"Window request was refused","Fensteranfrage wurde abgewiesen");a->dirty=true;}
        /* Only later native events/flags acknowledge the requested state. */
    }
}
static unsigned chrome_target(struct app *a,float x,float y) {
    int w,h;struct ae_terminal_chrome c;
    return SDL_GetWindowSize(a->window,&w,&h) && ae_terminal_chrome_layout((float)w,(float)h,(float)a->font_points/16,&c)?
        ae_terminal_chrome_target(&c,x,y):AE_TERM_CHROME_NONE;
}
static bool chrome_event(struct app *a,const SDL_Event *e) {
    if (e->type==SDL_EVENT_KEY_UP && e->key.key==SDLK_ESCAPE && a->chrome_escape) {a->chrome_escape=false;return true;}
    if (e->type==SDL_EVENT_KEY_DOWN && e->key.key==SDLK_ESCAPE && (a->chrome_pressed || a->chrome_escape)) {
        a->chrome_pressed=0;a->chrome_escape=true;a->dirty=true;return true;
    }
    if (e->type==SDL_EVENT_WINDOW_FOCUS_LOST || e->type==SDL_EVENT_WINDOW_MOUSE_LEAVE ||
        e->type==SDL_EVENT_WINDOW_RESIZED || e->type==SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED ||
        e->type==SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED || e->type==SDL_EVENT_WINDOW_MAXIMIZED || e->type==SDL_EVENT_WINDOW_RESTORED) {
        a->chrome_hover=a->chrome_pressed=0;
        if (e->type==SDL_EVENT_WINDOW_FOCUS_LOST) a->chrome_escape=false;
        a->dirty=true;
    }
    if (e->type==SDL_EVENT_MOUSE_MOTION) {
        unsigned hover=chrome_target(a,e->motion.x,e->motion.y);
        if (hover!=a->chrome_hover) {a->chrome_hover=hover;a->dirty=true;}
    }
    if (e->type==SDL_EVENT_MOUSE_BUTTON_DOWN && e->button.button==SDL_BUTTON_LEFT) {
        unsigned hit=chrome_target(a,e->button.x,e->button.y);
        if (hit) {a->chrome_pressed=a->chrome_escape?0:hit;a->chrome_hover=hit;a->selecting=false;a->dirty=true;return true;}
    }
    if (e->type==SDL_EVENT_MOUSE_BUTTON_UP && e->button.button==SDL_BUTTON_LEFT && a->chrome_pressed) {
        unsigned original=a->chrome_pressed;a->chrome_pressed=0;a->dirty=true;
        if (original==chrome_target(a,e->button.x,e->button.y)) chrome_action(a,original);
        return true;
    }
    return false;
}
#endif
static SDL_HitTestResult hit_test(SDL_Window *window,const SDL_Point *p,void *data) {
    const struct app *a=data;int w,h;if (!SDL_GetWindowSize(window,&w,&h) || (SDL_GetWindowFlags(window)&(SDL_WINDOW_MAXIMIZED|SDL_WINDOW_FULLSCREEN))) return SDL_HITTEST_NORMAL;
#ifdef AETHERI_TERMINAL_NEXAUI
    struct ae_terminal_chrome c;
    if (!ae_terminal_chrome_layout((float)w,(float)h,(float)a->font_points/16,&c) || ae_terminal_chrome_target(&c,(float)p->x,(float)p->y)) return SDL_HITTEST_NORMAL;
#endif
    bool l=p->x<8,r=p->x>=w-8,t=p->y<8,b=p->y>=h-8;
    if (t&&l) return SDL_HITTEST_RESIZE_TOPLEFT;
    if (t&&r) return SDL_HITTEST_RESIZE_TOPRIGHT;
    if (b&&l) return SDL_HITTEST_RESIZE_BOTTOMLEFT;
    if (b&&r) return SDL_HITTEST_RESIZE_BOTTOMRIGHT;
    if (l) return SDL_HITTEST_RESIZE_LEFT;
    if (r) return SDL_HITTEST_RESIZE_RIGHT;
    if (t) return SDL_HITTEST_RESIZE_TOP;
    if (b) return SDL_HITTEST_RESIZE_BOTTOM;
    return (float)p->y<a->bar_height
#ifndef AETHERI_TERMINAL_NEXAUI
        && p->x<w-48
#endif
        ? SDL_HITTEST_DRAGGABLE : SDL_HITTEST_NORMAL;
}
static bool selection_point(struct app *a,float x,float y,bool clamp,unsigned *row,unsigned *column) {
    if (!isfinite(x) || !isfinite(y) || a->cell_w<=0 || a->cell_h<=0) return false;
    float col=(x-a->left)/a->cell_w,line=(y-a->grid_y)/a->cell_h;
    if (!clamp && (col<0 || col>=(float)a->session.model->cols || line<0 || line>=(float)a->session.model->rows)) return false;
    col=fmaxf(0,fminf((float)a->session.model->cols,floorf(col+.5f)));
    line=fmaxf(0,fminf((float)a->session.model->rows-1,floorf(line)));
    *row=(unsigned)line;*column=(unsigned)col;return true;
}
static bool contains(SDL_FRect r,float x,float y) { return x>=r.x && x<r.x+r.w && y>=r.y && y<r.y+r.h; }
static bool paste_event(struct app *a,const SDL_Event *e) {
    if (!a->paste.size || !a->paste.review) return false;
    if (e->type==SDL_EVENT_WINDOW_FOCUS_LOST) { paste_cancel(a,tr(a,"Paste cancelled","Einfügen abgebrochen"));return false; }
    if (e->type==SDL_EVENT_WINDOW_RESIZED || e->type==SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED || e->type==SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED) { a->paste_pressed=-1;return false; }
    if (e->type==SDL_EVENT_KEY_DOWN) {
        SDL_Keycode key=e->key.key;SDL_Keymod mod=e->key.mod;
        if (key==SDLK_ESCAPE) paste_cancel(a,tr(a,"Paste cancelled","Einfügen abgebrochen"));
        else if ((mod&SDL_KMOD_CTRL) && (mod&SDL_KMOD_SHIFT) && key==SDLK_Q) user_close(a);
        else if (key==SDLK_F6 || key==SDLK_F7 || key==SDLK_F8) return false;
        else if (key==SDLK_TAB) { a->paste_button=1-a->paste_button;a->dirty=true; }
        else if ((key==SDLK_RETURN || key==SDLK_KP_ENTER) && !e->key.repeat && a->paste_shown && e->key.timestamp>=a->paste_shown) {
            if ((mod&SDL_KMOD_CTRL) || a->paste_button) paste_apply(a);
            else paste_cancel(a,tr(a,"Paste cancelled","Einfügen abgebrochen"));
        } else if (key==SDLK_PAGEUP || key==SDLK_PAGEDOWN || key==SDLK_UP || key==SDLK_DOWN || key==SDLK_HOME || key==SDLK_END) {
            unsigned step=(key==SDLK_PAGEUP || key==SDLK_PAGEDOWN) && a->paste_visible>1 ? a->paste_visible-1 : 1;
            if (key==SDLK_HOME) a->paste_scroll=0;
            else if (key==SDLK_END) a->paste_scroll=UINT_MAX;
            else if (key==SDLK_UP || key==SDLK_PAGEUP) a->paste_scroll=step>a->paste_scroll ? 0 : a->paste_scroll-step;
            else a->paste_scroll=step>UINT_MAX-a->paste_scroll ? UINT_MAX : a->paste_scroll+step;
            a->dirty=true;
        }
        return true;
    }
    if (e->type==SDL_EVENT_MOUSE_WHEEL) {
        if (e->wheel.y>0) a->paste_scroll=a->paste_scroll>3 ? a->paste_scroll-3 : 0;
        else if (e->wheel.y<0) a->paste_scroll=a->paste_scroll>UINT_MAX-3 ? UINT_MAX : a->paste_scroll+3;
        a->dirty=true;return true;
    }
    if (e->type==SDL_EVENT_MOUSE_BUTTON_DOWN && e->button.button==SDL_BUTTON_LEFT) {
        int w,h;(void)SDL_GetWindowSize(a->window,&w,&h);
        if (e->button.y<a->bar_height && e->button.x>(float)w-48) return false;
        a->paste_pressed=-1;
        if (a->paste_shown && e->button.timestamp>=a->paste_shown) for (unsigned i=0;i<2;++i) if (contains(a->paste_buttons[i],e->button.x,e->button.y)) { a->paste_pressed=(int)i;a->paste_button=i; }
        a->dirty=true;return true;
    }
    if (e->type==SDL_EVENT_MOUSE_BUTTON_UP && e->button.button==SDL_BUTTON_LEFT) {
        if (a->close_pressed) return false;
        int selected=a->paste_pressed;a->paste_pressed=-1;
        if (selected>=0 && contains(a->paste_buttons[selected],e->button.x,e->button.y)) {
            if (selected) paste_apply(a);else paste_cancel(a,tr(a,"Paste cancelled","Einfügen abgebrochen"));
        }
        return true;
    }
    return e->type==SDL_EVENT_TEXT_INPUT || e->type==SDL_EVENT_TEXT_EDITING || e->type==SDL_EVENT_KEY_UP || e->type==SDL_EVENT_MOUSE_MOTION;
}
static bool text_event_allowed(struct app *a,uint32_t window,uint64_t timestamp) {
    return window==SDL_GetWindowID(a->window) && timestamp>=a->text_after && SDL_GetKeyboardFocus()==a->window &&
        !a->session.closing && !a->session.pty.ended && !a->session.pty.eof;
}
static void composition_edit(struct app *a,const SDL_TextEditingEvent *e) {
    if (!text_event_allowed(a,e->windowID,e->timestamp)) return;
    if (a->composition.size && !composition_context(a)) { composition_cancel(a,true);return; }
    if (!a->composition.size) { a->composition_group=tcgetpgrp(a->session.pty.master);a->composition_alt=a->session.model->alt; }
    if (a->composition_group<=0 || !composition_context(a)) return;
    if (a->paste_receiving) paste_cancel(a,NULL);
    size_t size=e->text ? strnlen(e->text,AE_TERM_COMPOSITION_BYTES+1) : 0;
    bool accepted=ae_terminal_composition_set(&a->composition,e->text,size,e->start,e->length);
    if (!accepted) { composition_cancel(a,true);a->message=tr(a,"Composition was rejected","Texteingabe wurde abgelehnt"); }
    else { a->session.model->view=0;a->message=NULL; }
    a->dirty=true;
    if (a->diagnostics) printf("TEXT edit accepted=%d bytes=%zu start=%d length=%d\n",accepted,size,e->start,e->length);
}
static void composition_commit(struct app *a,const SDL_TextInputEvent *e) {
    if (!text_event_allowed(a,e->windowID,e->timestamp)) { if (a->diagnostics) puts("TEXT commit stale");return; }
    if (a->composition.size && !composition_context(a)) { composition_cancel(a,true);return; }
    size_t size=e->text ? strnlen(e->text,INPUT_BYTES+1) : 0;struct ae_terminal_paste text={0};
    enum ae_term_paste_result checked=ae_terminal_paste_set(&text,e->text,size);
    composition_cancel(a,false);bool accepted=false;
    if (checked==AE_TERM_PASTE_OK && !text.review) {
        if (a->paste_receiving || a->paste.size) paste_cancel(a,NULL);
        accepted=enqueue(a,text.text,text.size);
    }
    else { composition_cancel(a,true);a->message=tr(a,"Text input was rejected","Texteingabe wurde abgelehnt");a->dirty=true; }
    if (a->diagnostics) printf("TEXT commit accepted=%d bytes=%zu\n",accepted,size);
}
static void event(struct app *a,const SDL_Event *e) {
    if (a->diagnostics && (e->type==SDL_EVENT_WINDOW_FOCUS_GAINED || e->type==SDL_EVENT_WINDOW_FOCUS_LOST)) printf("FOCUS %d\n",e->type==SDL_EVENT_WINDOW_FOCUS_GAINED);
    if (e->type==a->event) { if (e->user.code<0) error(a,"PTY event bridge");io_event(a);SDL_SignalSemaphore(a->ack);return; }
    if (e->type==SDL_EVENT_QUIT || e->type==SDL_EVENT_WINDOW_CLOSE_REQUESTED) { user_close(a);return; }
#ifdef AETHERI_TERMINAL_NEXAUI
    if (chrome_event(a,e)) return;
#endif
    if (paste_event(a,e)) return;
    if (e->type==SDL_EVENT_WINDOW_EXPOSED || e->type==SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED || e->type==SDL_EVENT_WINDOW_RESIZED || e->type==SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED) a->dirty=true;
    /* Committed text owns its input provenance. The current global modifier
       state can already belong to a later queued key, or to an IME shortcut. */
    else if (e->type==SDL_EVENT_TEXT_INPUT) composition_commit(a,&e->text);
    else if (e->type==SDL_EVENT_TEXT_EDITING) composition_edit(a,&e->edit);
    else if (e->type==SDL_EVENT_MOUSE_WHEEL && e->wheel.y!=0) { ae_terminal_scroll(a->session.model,e->wheel.y>0 ? 3 : -3);a->dirty=true; }
    else if (e->type==SDL_EVENT_WINDOW_FOCUS_LOST) { paste_cancel(a,NULL);composition_cancel(a,true);a->close_pressed=false;a->selecting=false;a->dirty=true; }
    else if (e->type==SDL_EVENT_WINDOW_FOCUS_GAINED) { if (e->window.timestamp>a->text_after) a->text_after=e->window.timestamp; }
    else if (e->type==SDL_EVENT_MOUSE_MOTION && a->selecting) {
        unsigned row,column;
        if (selection_point(a,e->motion.x,e->motion.y,true,&row,&column) && !ae_terminal_select_extend(a->session.model,row,column)) a->selecting=false;
        a->dirty=true;
    }
    else if ((e->type==SDL_EVENT_MOUSE_BUTTON_DOWN || e->type==SDL_EVENT_MOUSE_BUTTON_UP) && e->button.button==SDL_BUTTON_LEFT) {
        bool hit=false;
#ifndef AETHERI_TERMINAL_NEXAUI
        int w,h;SDL_GetWindowSize(a->window,&w,&h);hit=e->button.y<a->bar_height && e->button.x>(float)w-48;
#endif
        unsigned row,column;
        if (e->type==SDL_EVENT_MOUSE_BUTTON_DOWN) {
            if (a->composition.size) composition_cancel(a,true);
            if (a->paste_receiving) paste_cancel(a,NULL);
            a->close_pressed=hit;a->selecting=false;
            if (!hit && !a->session.closing && selection_point(a,e->button.x,e->button.y,false,&row,&column))
                a->selecting=ae_terminal_select_begin(a->session.model,row,column);
        } else {
            if (hit && a->close_pressed) user_close(a);
            if (a->selecting && selection_point(a,e->button.x,e->button.y,true,&row,&column)) (void)ae_terminal_select_extend(a->session.model,row,column);
            a->close_pressed=false;a->selecting=false;
        }
        a->dirty=true;
    }
    else if (e->type==SDL_EVENT_KEY_DOWN) {
        SDL_Keycode key=e->key.key;SDL_Keymod mod=e->key.mod;
        if ((mod&SDL_KMOD_CTRL) && (mod&SDL_KMOD_SHIFT) && key==SDLK_Q) user_close(a);
        else if (a->composition.size && key!=SDLK_F6 && key!=SDLK_F7 && key!=SDLK_F8 &&
            !((mod&SDL_KMOD_CTRL) && (mod&SDL_KMOD_SHIFT) && (key==SDLK_C || key==SDLK_V))) {
            if (key==SDLK_ESCAPE) composition_cancel(a,true);
            /* IME owns editing/confirmation keys while its draft is visible.
               Never send a speculative Enter/arrow/backspace to the PTY. */
        }
#ifdef AETHERI_TERMINAL_NEXAUI
        else if ((mod&SDL_KMOD_CTRL) && (mod&SDL_KMOD_SHIFT) && !(mod&(SDL_KMOD_ALT|SDL_KMOD_GUI)) && key==SDLK_M) {
            if (!e->key.repeat) chrome_action(a,AE_TERM_CHROME_ZOOM);
        }
        else if ((mod&SDL_KMOD_CTRL) && (mod&SDL_KMOD_SHIFT) && !(mod&(SDL_KMOD_ALT|SDL_KMOD_GUI)) && key==SDLK_R) {
            if (a->session.closing) { /* Closing admits no new reading target. */ }
            else if (ae_terminal_view_reading(a->view)) {
                if (nx_focus_set(ae_terminal_view_context(a->view),0)!=NX_OK) error(a,"clear reading focus");
            } else if (!ae_terminal_view_focus_row(a->view,a->session.model->view?0:a->session.model->y)) error(a,"reading focus");
            a->dirty=true;
        }
#endif
        else if ((mod&SDL_KMOD_CTRL) && (mod&SDL_KMOD_SHIFT) && !(mod&(SDL_KMOD_ALT|SDL_KMOD_GUI)) && (key==SDLK_C || key==SDLK_V)) {
            if (!e->key.repeat) { if (key==SDLK_C) clipboard_copy(a);else clipboard_paste(a); }
        }
        else if ((mod&SDL_KMOD_CTRL) && (mod&SDL_KMOD_SHIFT) && !(mod&(SDL_KMOD_ALT|SDL_KMOD_GUI)) && key==SDLK_A) {
            if (a->paste_receiving) paste_cancel(a,NULL);
            (void)ae_terminal_select_all(a->session.model);a->selecting=false;a->dirty=true;
        }
        else if ((mod&SDL_KMOD_CTRL) && (mod&SDL_KMOD_SHIFT) && !(mod&(SDL_KMOD_ALT|SDL_KMOD_GUI)) &&
                 (key==SDLK_UP || key==SDLK_DOWN || key==SDLK_LEFT || key==SDLK_RIGHT)) {
            if (a->paste_receiving) paste_cancel(a,NULL);
            (void)ae_terminal_select_step(a->session.model,key==SDLK_LEFT ? -1 : key==SDLK_RIGHT ? 1 : 0,key==SDLK_UP ? -1 : key==SDLK_DOWN ? 1 : 0);
            a->selecting=false;a->dirty=true;
        }
        else if (key==SDLK_ESCAPE && a->session.model->selection.count) { if (!a->failed) a->message=NULL;ae_terminal_select_clear(a->session.model);a->selecting=false;a->dirty=true; }
        else if ((mod&SDL_KMOD_SHIFT) && (key==SDLK_PAGEUP || key==SDLK_PAGEDOWN)) { ae_terminal_scroll(a->session.model,key==SDLK_PAGEUP ? (int)a->session.model->rows/2 : -(int)a->session.model->rows/2);a->dirty=true; }
        else if (key==SDLK_F6) { a->german=!a->german;a->dirty=true; }
        else if (key==SDLK_F7) { a->contrast=!a->contrast;a->dirty=true; }
        else if (key==SDLK_F8) { a->font_points=a->font_points==16 ? 20 : a->font_points==20 ? 24 : a->font_points==24 ? 32 : 16;a->dirty=true; }
        else if ((mod&SDL_KMOD_CTRL) && (mod&SDL_KMOD_SHIFT) && !(mod&(SDL_KMOD_ALT|SDL_KMOD_GUI)) && (key==SDLK_Z || key==SDLK_Y)) {
            unsigned char c=key==SDLK_Z ? 31 : 30;input(a,&c,1);
        }
        else if ((mod&SDL_KMOD_CTRL) && key>=SDLK_A && key<=SDLK_Z) { unsigned char c=(unsigned char)(key-SDLK_A+1);input(a,&c,1); }
        else if (key==SDLK_RETURN || key==SDLK_KP_ENTER) input(a,"\r",1);
        else if (key==SDLK_BACKSPACE) input(a,"\177",1);
        else if (key==SDLK_TAB) input(a,"\t",1);
        else if (key==SDLK_ESCAPE) input(a,"\033",1);
        else if (key==SDLK_HOME || key==SDLK_END) { char seq[]={27,'[',key==SDLK_HOME ? 'H' : 'F'};input(a,seq,sizeof(seq)); }
        else if (key==SDLK_DELETE) input(a,"\033[3~",4);
        else if (key==SDLK_UP || key==SDLK_DOWN || key==SDLK_RIGHT || key==SDLK_LEFT) { char seq[]={27,a->session.model->application_cursor ? 'O' : '[',key==SDLK_UP ? 'A' : key==SDLK_DOWN ? 'B' : key==SDLK_RIGHT ? 'C' : 'D'};input(a,seq,sizeof(seq)); }
    }
}
#ifdef AETHERI_TERMINAL_NEXAUI
static unsigned char *load_view_font(const char *path,size_t *size) {
    int fd=open(path,O_RDONLY|O_NOFOLLOW|O_CLOEXEC);struct stat st;
    if (fd<0) return NULL;
    if (fstat(fd,&st) || !S_ISREG(st.st_mode) || st.st_size<1 || st.st_size>16*1024*1024) {close(fd);return NULL;}
    *size=(size_t)st.st_size;unsigned char *bytes=malloc(*size);size_t used=0;
    if (!bytes) {close(fd);return NULL;}
    while (used<*size) {ssize_t n=read(fd,bytes+used,*size-used);if (n<0 && errno==EINTR) continue;if (n<=0) {free(bytes);close(fd);return NULL;}used+=(size_t)n;}
    close(fd);return bytes;
}
#endif
int main(int argc,char **argv) {
    struct app a={.session=AE_TERMINAL_SESSION_INIT,.wake={-1,-1},.clipboard_fd=-1,.paste_pressed=-1,.running=true,.dirty=true,.font_points=16,.bar_height=40};
#ifdef AETHERI_TERMINAL_NEXAUI
    a.accessibility_fd=-1;
#endif
    const char *font=NULL,*cache=NULL,*shell=NULL,*editor=NULL,*leaf=NULL;
#ifdef AETHERI_TERMINAL_NEXAUI
    const char *bold_font="/usr/share/fonts/liberation-mono-fonts/LiberationMono-Bold.ttf";bool bold_selected=false;
#endif
    for (int i=1;i<argc;++i) {
        if (!strcmp(argv[i],"--font") && !font && i+1<argc) font=argv[++i];
#ifdef AETHERI_TERMINAL_NEXAUI
        else if (!strcmp(argv[i],"--bold-font") && !bold_selected && i+1<argc) {bold_font=argv[++i];bold_selected=true;}
#endif
        else if (!strcmp(argv[i],"--cache") && !cache && i+1<argc) cache=argv[++i];
        else if (!strcmp(argv[i],"--shell") && !shell && i+1<argc) shell=argv[++i];
        else if (!strcmp(argv[i],"--editor") && !editor && i+1<argc) editor=argv[++i];
        else if (!strcmp(argv[i],"--open-leaf") && !leaf && i+1<argc) leaf=argv[++i];
        else if (!strcmp(argv[i],"--diagnostics") && !a.diagnostics) a.diagnostics=true;
        else return 2;
    }
    a.editor=editor!=NULL;const char *program=a.editor ? editor : shell;
    struct stat st;if (!font || !cache || !program || program[0]!='/' || (a.editor ? shell!=NULL || leaf==NULL : leaf!=NULL) || cache[0]!='/' || getuid()==0 || getuid()!=geteuid() || getgid()!=getegid() || lstat(cache,&st) || !S_ISDIR(st.st_mode) || st.st_uid!=getuid() || (st.st_mode&07777)!=0700) return 2;
    if (a.editor && (fstat(3,&st) || !S_ISDIR(st.st_mode) || fstat(4,&st) || !S_ISREG(st.st_mode))) return 2;
    setvbuf(stdout,NULL,_IOLBF,0);
    struct sigaction child={.sa_handler=SIG_DFL};sigemptyset(&child.sa_mask);
    if (sigaction(SIGCHLD,&child,NULL) || !setlocale(LC_CTYPE,"C.UTF-8") || setenv("TERM","dumb",1) || unsetenv("COLORTERM") || setenv("MESA_SHADER_CACHE_DIR",cache,1) || setenv("MESA_SHADER_CACHE_MAX_SIZE","32M",1)) return 1;
    a.session.model=malloc(sizeof(*a.session.model));if (!a.session.model) return 1;ae_terminal_init(a.session.model,80,24,width);
    int source=open(program,O_RDONLY|O_NOFOLLOW|O_CLOEXEC);
    int opened=source<0 ? -1 : a.editor ? ae_terminal_pty_open_editor(&a.session.pty,source,3,4,leaf,80,24) : ae_terminal_pty_open(&a.session.pty,source,80,24);
    if (source>=0) close(source);
    if (a.editor) { (void)close(3);(void)close(4); }
    if (opened) { free(a.session.model);return 1; }
    if (!SDL_SetAppMetadata(a.editor ? "Aetheri Editor" : "Aetheri Terminal","0.0.1",a.editor ? "org.aetheri.Editor" : "org.aetheri.Terminal") || !SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"wayland") || !SDL_Init(SDL_INIT_VIDEO) || !TTF_Init()) { a.failed=true;goto cleanup; }
    int fm,fn,fp,hm,hn,hp;TTF_GetFreeTypeVersion(&fm,&fn,&fp);TTF_GetHarfBuzzVersion(&hm,&hn,&hp);
    if (SDL_GetVersion()!=SDL_VERSION || TTF_Version()!=SDL_TTF_VERSION || fm!=2 || fn!=15 || fp!=0 || hm!=15 || hn!=0 || hp!=0) { error(&a,"unreviewed SDL/text version");goto cleanup; }
    a.font=TTF_OpenFont(font,16);a.window=SDL_CreateWindow(a.editor ? "Aetheri Editor" : "Aetheri Terminal",1000,700,SDL_WINDOW_RESIZABLE|SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (a.window) a.renderer=SDL_CreateRenderer(a.window,"opengles2");
    if (!a.font || !TTF_FontIsFixedWidth(a.font) || !a.window || !a.renderer || !SDL_SetWindowMinimumSize(a.window,480,320) || !SDL_SetWindowHitTest(a.window,hit_test,&a) || !SDL_StartTextInput(a.window)) { error(&a,"font/window/renderer/input setup");goto cleanup; }
#ifdef AETHERI_TERMINAL_NEXAUI
    size_t regular_size=0,bold_size=0;
    a.view_font=load_view_font(font,&regular_size);a.view_bold=load_view_font(bold_font,&bold_size);
    if (!a.view_font || !a.view_bold || !ae_terminal_view_create(a.view_font,regular_size,a.view_bold,bold_size,&a.view)) {error(&a,"NexaUI fonts/view");goto cleanup;}
    a.accessibility_lock=SDL_CreateMutex();
    if (!a.accessibility_lock || nx_linux_accessibility_host_create(&a.accessibility)!=NX_OK) {error(&a,"accessibility host setup");goto cleanup;}
    if (a.diagnostics) puts("PRESENTATION nexaui 0.1.18 sdl-host rgba8");
#endif
    struct wl_display *display=SDL_GetPointerProperty(SDL_GetWindowProperties(a.window),SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER,NULL);
    struct wl_surface *surface=SDL_GetPointerProperty(SDL_GetWindowProperties(a.window),SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER,NULL);
    if (!ae_clipboard_open(&a.clipboard,display,surface,clipboard_wake,&a)) a.clipboard_fd=ae_clipboard_watch_fd(a.clipboard);
    a.event=SDL_RegisterEvents(1);a.ack=SDL_CreateSemaphore(0);
    if (!a.event || !a.ack || pipe2(a.wake,O_NONBLOCK|O_CLOEXEC)) { error(&a,"event bridge setup");goto cleanup; }
    struct sigaction action={.sa_handler=signalled};sigemptyset(&action.sa_mask);wake_writer=a.wake[1];
    if (sigaction(SIGTERM,&action,NULL) || sigaction(SIGINT,&action,NULL)) { error(&a,"signal setup");goto cleanup; }
    atomic_init(&a.clipboard_deadline,0);atomic_init(&a.active,true);atomic_init(&a.readable,true);atomic_init(&a.watch_child,true);atomic_init(&a.writeable,false);
    a.worker=SDL_CreateThread(worker,"terminal-ready",&a);if (!a.worker) { error(&a,"PTY event worker");goto cleanup; }
    if (a.diagnostics) printf("READY wayland opengles2 %s=%d\n",a.editor ? "editor" : "shell",a.session.pty.pid);
    while (a.running) {
        if (a.dirty && !a.failed) draw(&a);
#ifdef AETHERI_TERMINAL_NEXAUI
        if (!a.failed) accessibility_prepare(&a);
        if (a.dirty && !a.failed) continue;
#endif
        if (!a.running) break;
        SDL_Event e;if (!SDL_WaitEvent(&e)) { error(&a,"event wait");break; }event(&a,&e);clipboard_update(&a);text_input_sync(&a);
    }
cleanup:
    if (a.worker) { atomic_store(&a.active,false);notify(&a);SDL_SignalSemaphore(a.ack);SDL_WaitThread(a.worker,NULL); }
#ifdef AETHERI_TERMINAL_NEXAUI
    if (nx_linux_accessibility_host_destroy(&a.accessibility)!=NX_OK) a.failed=true;
    if (a.accessibility_fd>=0) close(a.accessibility_fd);
    SDL_DestroyMutex(a.accessibility_lock);
#endif
    if (ae_clipboard_close(&a.clipboard)) a.failed=true;
    ae_terminal_paste_clear(&a.paste);wake_writer=-1;
    if (!a.session.pty.ended || !a.session.pty.eof) {
        (void)ae_terminal_pty_hangup(&a.session.pty);
        while (!a.session.pty.ended || !a.session.pty.eof) {
            struct pollfd fds[2]={{a.session.pty.eof ? -1 : a.session.pty.master,POLLIN,0},{a.session.pty.ended ? -1 : a.session.pty.pin,POLLIN,0}};
            int n;do { n=poll(fds,2,-1); } while (n<0 && errno==EINTR);if (n<0) { a.failed=true;break; }
            unsigned char bytes[4096];if (fds[0].revents) { ssize_t got;do { got=ae_terminal_pty_read(&a.session.pty,bytes,sizeof(bytes)); } while (got>0); }
            if (fds[1].revents && ae_terminal_pty_observe(&a.session.pty)<0) { a.failed=true;break; }
        }
    }
    if (ae_terminal_pty_close(&a.session.pty)) a.failed=true;
    if (a.frame_callback) wl_callback_destroy(a.frame_callback);
#ifdef AETHERI_TERMINAL_NEXAUI
    SDL_DestroyTexture(a.view_texture);ae_terminal_view_destroy(a.view);free(a.view_font);free(a.view_bold);
#endif
    glyphs_clear(&a);SDL_DestroySemaphore(a.ack);for (unsigned i=0;i<2;++i) if (a.wake[i]>=0) close(a.wake[i]);
    SDL_DestroyRenderer(a.renderer);SDL_DestroyWindow(a.window);TTF_CloseFont(a.font);TTF_Quit();SDL_Quit();
    printf("CLOSED frames=%u shell_status=%d failed=%d\n",a.frame,a.session.pty.status,a.failed);free(a.session.model);return a.failed ? 1 : 0;
}
