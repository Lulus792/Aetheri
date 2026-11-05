/* Development desktop presentation. No normal session/app admission is implied. */
#define _GNU_SOURCE
#include "model.h"
#include "aetheri-development-desktop-v1-client.h"
#include "wlr-layer-shell-v1-client.h"
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <wayland-client.h>

struct app;
struct output { struct wl_output *object; char name[128]; };
struct control { unsigned kind; uint32_t window; SDL_FRect rect; };
struct view {
    struct app *app; unsigned index;
    SDL_Window *window; SDL_Renderer *renderer;
    struct wl_surface *surface; struct zwlr_layer_surface_v1 *layer;
    struct wl_callback *frame;
    char frame_state[640];
    int width, height;
    bool expanded, configured, dirty;
    unsigned settling;
    struct control controls[16]; size_t count;
    unsigned pressed; uint32_t pressed_window;
};
struct app {
    struct ae_desktop_model model;
    struct wl_display *display; struct wl_registry *registry;
    struct aetheri_development_desktop_v1 *control; struct zwlr_layer_shell_v1 *layers;
    struct output outputs[3]; char output_names[3][128];
    unsigned output_count, observed_count;
    struct view views[3];
    TTF_Font *font;
    SDL_Thread *worker;
    uint32_t event_type;
    int launcher, signals[2];
    bool running, failed, snapshot, retired, diagnostics;
    bool ui_ready, pending_toggle;
    bool closing;
    unsigned pending_output;
    unsigned frames;
};
static volatile sig_atomic_t signal_writer = -1;
static void signalled(int number) {
    (void)number; int saved = errno; unsigned char byte = 1;
    if (signal_writer >= 0) (void)write(signal_writer, &byte, 1);
    errno = saved;
}
static void wake(struct app *a, int code) {
    SDL_Event e = {.type = a->event_type}; e.user.code = code;
    /* A failed event enqueue cannot leave an unobservable pending command. */
    if (!SDL_PushEvent(&e)) _Exit(1);
}
static int worker(void *data) {
    struct app *a = data;
    for (;;) {
        struct pollfd fds[2] = {{a->launcher, POLLIN, 0}, {a->signals[0], POLLIN, 0}};
        int r; do { r = poll(fds, 2, -1); } while (r < 0 && errno == EINTR);
        if (r < 0) { wake(a, -1); return 1; }
        if (fds[1].revents) { wake(a, 'Q'); return 0; }
        if (fds[0].revents) {
            unsigned char bytes[8]; union { struct cmsghdr align; char bytes[CMSG_SPACE(sizeof(int) * 4)]; } ancillary;
            struct iovec io = {.iov_base = bytes, .iov_len = sizeof(bytes)};
            struct msghdr message = {.msg_iov = &io, .msg_iovlen = 1, .msg_control = ancillary.bytes, .msg_controllen = sizeof(ancillary.bytes)};
            ssize_t n; do { n = recvmsg(a->launcher, &message, MSG_CMSG_CLOEXEC); } while (n < 0 && errno == EINTR);
            bool extra = false;
            if (n >= 0) for (struct cmsghdr *h = CMSG_FIRSTHDR(&message); h; h = CMSG_NXTHDR(&message, h)) {
                extra = true;
                if (h->cmsg_level == SOL_SOCKET && h->cmsg_type == SCM_RIGHTS && h->cmsg_len >= CMSG_LEN(0)) {
                    size_t count = (h->cmsg_len-CMSG_LEN(0))/sizeof(int);
                    for (size_t i = 0; i < count; ++i) { int fd; memcpy(&fd, (unsigned char *)CMSG_DATA(h)+i*sizeof(fd), sizeof(fd)); close(fd); }
                }
            }
            if (n != 2 || extra || (message.msg_flags & (MSG_TRUNC | MSG_CTRUNC)) || bytes[1] != '1' ||
                    (bytes[0] != 'A' && bytes[0] != 'E' && bytes[0] != 'X')) { wake(a, -1); return 1; }
            wake(a, bytes[0]);
        }
    }
}
static void error(struct app *a, const char *what) {
    fprintf(stderr, "desktop: %s: %s\n", what, SDL_GetError()); a->failed = true; a->running = false;
}
static const char *tr(struct app *a, const char *en, const char *de) { return a->model.german ? de : en; }
static void dirty(struct app *a) { for (unsigned i = 0; i < a->observed_count; ++i) a->views[i].dirty = true; }
static bool inside(SDL_FRect r, float x, float y) { return x >= r.x && x < r.x+r.w && y >= r.y && y < r.y+r.h; }
static SDL_FRect card_rect(struct view *v) {
    float w=(float)v->width,h=(float)v->height,cw=fminf(w-32,680);
    float y=h<600 ? 16 : fminf(144,h*0.15f);
    return (SDL_FRect){(w-cw)/2,y,cw,h<600 ? h-32 : fminf(h-y-100,560)};
}
static void fill(struct view *v, SDL_FRect r, SDL_Color c) {
    if (!SDL_SetRenderDrawColor(v->renderer, c.r, c.g, c.b, c.a) || !SDL_RenderFillRect(v->renderer, &r)) error(v->app, "rectangle");
}
static void text(struct view *v, const char *s, float x, float y, float width, float size, SDL_Color c) {
    struct app *a = v->app;
    if (!*s) return;
    float density = SDL_GetWindowPixelDensity(v->window);
    if (!isfinite(density) || density < 0.5f || density > 4 || width < 1 || !TTF_SetFontSize(a->font, size*density)) { error(a, "font size"); return; }
    SDL_Surface *surface = TTF_RenderText_Blended_Wrapped(a->font, s, 0, c, (int)floorf(width*density));
    if (!surface) { error(a, "text rasterization"); return; }
    SDL_Texture *texture = SDL_CreateTextureFromSurface(v->renderer, surface);
    SDL_FRect r = {x, y, (float)surface->w/density, (float)surface->h/density}; SDL_DestroySurface(surface);
    if (!texture || !SDL_RenderTexture(v->renderer, texture, NULL, &r)) error(a, "text presentation");
    SDL_DestroyTexture(texture);
}
static void button(struct view *v, unsigned kind, uint32_t id, SDL_FRect r, const char *label, bool selected) {
    struct app *a = v->app;
    if (v->count >= SDL_arraysize(v->controls)) { error(a, "control bound"); return; }
    v->controls[v->count++] = (struct control){kind, id, r};
    SDL_Color panel = a->model.contrast ? (SDL_Color){0,0,0,255} : (SDL_Color){38,46,63,255};
    fill(v, r, selected ? (SDL_Color){44,72,108,255} : panel);
    if (selected) {
        SDL_FRect line = {r.x,r.y,r.w,2}; fill(v,line,(SDL_Color){139,189,255,255});
    }
    SDL_Rect clip = {(int)r.x+8,(int)r.y+2,(int)r.w-16,(int)r.h-4};
    if (!SDL_SetRenderClipRect(v->renderer,&clip)) error(a,"button clip");
    text(v,label,r.x+12,r.y+12,r.w-24,16,(SDL_Color){239,243,251,255});
    if (!SDL_SetRenderClipRect(v->renderer,NULL)) error(a,"clip reset");
}
static void query(struct view *v, SDL_FRect field) {
    struct app *a=v->app;float density=SDL_GetWindowPixelDensity(v->window);
    const char *value=*a->model.query ? a->model.query : field.w<350 ? tr(a,"Search…","Suchen…") :
        tr(a,"Type an app or window name…","App- oder Fensternamen eingeben…");
    char shown[AE_DESKTOP_QUERY+4];const char *tail=value;int width=0,height=0;
    if (!TTF_SetFontSize(a->font,17*density) || !TTF_GetStringSize(a->font,value,0,&width,&height)) { error(a,"query measurement");return; }
    float available=field.w-32;
    while ((float)width>available*density && *tail) {
        ++tail;while (((unsigned char)*tail&0xc0)==0x80) ++tail;
        snprintf(shown,sizeof(shown),"…%s",tail);value=shown;
        if (!TTF_GetStringSize(a->font,value,0,&width,&height)) { error(a,"query measurement");return; }
    }
    SDL_Rect clip={(int)field.x+2,(int)field.y+2,(int)field.w-4,(int)field.h-4};
    if (!SDL_SetRenderClipRect(v->renderer,&clip)) error(a,"query clip");
    text(v,value,field.x+14,field.y+15,available+1,17,*a->model.query ? (SDL_Color){239,243,251,255} : (SDL_Color){178,192,214,255});
    fill(v,(SDL_FRect){field.x+14+(*a->model.query ? (float)width/density : 0),field.y+15,1,20},(SDL_Color){139,189,255,255});
    if (!SDL_SetRenderClipRect(v->renderer,NULL)) error(a,"query clip reset");
}
static void frame_done(void *data,struct wl_callback *callback,uint32_t time) {
    (void)time;struct view *v=data;
    if (v->frame==callback) { v->frame=NULL;puts(v->frame_state); }
    wl_callback_destroy(callback);
}
static const struct wl_callback_listener frame_listener={.done=frame_done};
static void draw(struct view *v) {
    struct app *a = v->app;
    if (!v->configured || (v->expanded ? v->height < 320 : v->height != 84)) return;
    int pixels_w, pixels_h;
    if (!SDL_GetWindowSizeInPixels(v->window,&pixels_w,&pixels_h) ||
            !SDL_SetRenderScale(v->renderer,(float)pixels_w/(float)v->width,(float)pixels_h/(float)v->height) ||
            !SDL_SetRenderDrawColor(v->renderer,v->expanded ? 14 : 0,v->expanded ? 18 : 0,v->expanded ? 25 : 0,v->expanded ? 224 : 0) ||
            !SDL_RenderClear(v->renderer)) { error(a,"frame setup"); return; }
    v->count = 0;
    float w = (float)v->width, h = (float)v->height;
    SDL_Color secondary = {178,192,214,255};
    if (!v->expanded || h>=600) {
        float dock_width = fminf(w-32,640), dx = (w-dock_width)/2, dy = h-72;
        fill(v,(SDL_FRect){dx,dy,dock_width,60},(SDL_Color){25,31,43,255});
        if (dock_width<440) {
            float bw=(dock_width-40)/2;
            button(v,1,0,(SDL_FRect){dx+16,dy+8,bw,44},tr(a,"Search","Suchen"),v->expanded);
            button(v,2,0,(SDL_FRect){dx+24+bw,dy+8,bw,44},tr(a,"Settings","Einstellungen"),false);
        } else {
            text(v,"Aetheri",dx+16,dy+20,80,16,(SDL_Color){239,243,251,255});
            button(v,1,0,(SDL_FRect){dx+112,dy+8,142,44},tr(a,"Search","Suchen"),v->expanded);
            button(v,2,0,(SDL_FRect){dx+262,dy+8,150,44},tr(a,"Settings","Einstellungen"),false);
            if (dock_width >= 520) {
                char status[64]; snprintf(status,sizeof(status),"%zu %s",a->model.count,tr(a,a->model.count==1 ? "window" : "windows","Fenster"));
                text(v,status,dx+432,dy+22,dock_width-444,13,secondary);
            }
        }
    }
    if (v->expanded) {
        SDL_FRect card=card_rect(v);float cw=card.w,x=card.x,y=card.y,ch=card.h;
        fill(v,(SDL_FRect){x,y,cw,ch},a->model.contrast ? (SDL_Color){0,0,0,255} : (SDL_Color){25,31,43,255});
        text(v,tr(a,"Search Aetheri","Aetheri durchsuchen"),x+24,y+20,cw-48,21,(SDL_Color){239,243,251,255});
        fill(v,(SDL_FRect){x+24,y+62,cw-48,50},(SDL_Color){38,46,63,255});
        query(v,(SDL_FRect){x+24,y+62,cw-48,50});
        struct ae_desktop_result results[AE_DESKTOP_WINDOWS+1]; size_t n = ae_desktop_model_results(&a->model,results);
        size_t selected = 0; while (selected < n && results[selected].window != a->model.selected) ++selected;
        size_t visible = (size_t)fmaxf(0,floorf((ch-192)/52)); if (visible > 10) visible = 10;
        size_t first = selected < n && selected >= visible && visible ? selected-visible+1 : 0;
        if (!n) text(v,tr(a,"No matching apps or windows.","Keine passenden Apps oder Fenster."),x+28,y+138,cw-56,16,secondary);
        for (size_t i = first; i < n && i < first+visible; ++i) {
            const char *label = !results[i].window && a->model.pending ? tr(a,"Requesting Settings…","Einstellungen werden angefordert…") : results[i].label;
            button(v,results[i].window ? 3u : 2u,results[i].window,
                (SDL_FRect){x+24,y+128+(float)(i-first)*52,cw-48,46},label,i==selected);
        }
        const char *status = a->closing ? tr(a,"Closing the development desktop…","Entwicklungsdesktop wird geschlossen…") :
            a->model.launch_error ? tr(a,"Settings could not start. You can try again.","Einstellungen konnten nicht starten. Erneut versuchen.") :
            a->model.pending ? tr(a,"Waiting for the launch response.","Warten auf die Startantwort.") :
            tr(a,"Enter: open   ·   Tab / arrows: select   ·   Esc: close","Enter: öffnen   ·   Tab / Pfeile: wählen   ·   Esc: schließen");
        text(v,status,x+24,y+ch-48,cw-48,13,secondary);
    }
    if (a->diagnostics) {
        if (v->frame) wl_callback_destroy(v->frame);
        v->frame=wl_surface_frame(v->surface);
        if (!v->frame || wl_callback_add_listener(v->frame,&frame_listener,v)) { error(a,"diagnostic frame observation");return; }
        snprintf(v->frame_state,sizeof(v->frame_state),"DRAWN %u output=%u search=%d size=%dx%d de=%d contrast=%d pending=%d windows=%zu settled=%d query=%s",
            a->frames+1,v->index,v->expanded,v->width,v->height,a->model.german,a->model.contrast,a->model.pending,a->model.count,v->settling<=1,a->model.query);
    }
    if (!SDL_RenderPresent(v->renderer)) { error(a,"present"); return; }
    v->dirty = false; ++a->frames;
    /* Wayland EGL can submit the previous allocation once after a native
       resize. The viewport preserves logical geometry; one bounded follow-up
       repaint replaces that allocation with the newly sized backbuffer. */
    if (v->settling) {
        --v->settling;
        if (v->settling) { v->dirty=true;wake(a,0); }
        else if (a->diagnostics) printf("STABLE %u %dx%d\n",v->index,v->width,v->height);
    }
    if (a->diagnostics) {
        printf("VIEW %u output=%u search=%d size=%dx%d query=%s pending=%d windows=%zu de=%d contrast=%d\n",a->frames,v->index,v->expanded,v->width,v->height,a->model.query,a->model.pending,a->model.count,a->model.german,a->model.contrast);
        for (size_t i = 0; i < v->count; ++i) {
            struct control *c = &v->controls[i];
            printf("CONTROL %u %u %u %u %.1f %.1f\n",a->frames,v->index,c->kind,c->window,(double)(c->rect.x+c->rect.w/2),(double)(c->rect.y+c->rect.h/2));
        }
        printf("LAYOUT %u\n",a->frames);
    }
}
static void state(struct view *v, bool expanded) {
    v->expanded = expanded; v->pressed = 0;
    uint32_t anchors = ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM|ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT|ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
    if (expanded) anchors |= ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP;
    zwlr_layer_surface_v1_set_size(v->layer,0,expanded ? 0 : 84);
    zwlr_layer_surface_v1_set_anchor(v->layer,anchors); zwlr_layer_surface_v1_set_exclusive_zone(v->layer,84);
    zwlr_layer_surface_v1_set_exclusive_edge(v->layer,ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM);
    zwlr_layer_surface_v1_set_keyboard_interactivity(v->layer,expanded ?
        ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE : ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE);
    wl_surface_commit(v->surface); v->dirty = true;
    if (expanded) { if (!SDL_StartTextInput(v->window)) error(v->app,"search text input"); }
    else (void)SDL_StopTextInput(v->window);
}
static void search(struct app *a, unsigned index, bool open) {
    if (index >= a->observed_count) { error(a,"unknown search output"); return; }
    for (unsigned i = 0; i < a->observed_count; ++i) if (a->views[i].expanded && (!open || i != index)) state(&a->views[i],false);
    ae_desktop_model_search(&a->model,index,open);
    if (a->views[index].expanded != open) state(&a->views[index],open);
    a->views[index].dirty=true;
}
static void activate(struct app *a, unsigned kind, uint32_t id, unsigned output) {
    if (a->closing) return;
    if (kind == 1) { search(a,output,!(a->model.search && a->model.output==output)); return; }
    if (kind == 2) {
        if (!ae_desktop_model_request(&a->model)) return;
        if (send(a->launcher,"L1",2,MSG_NOSIGNAL|MSG_DONTWAIT) != 2) { error(a,"uncertain launch request"); return; }
        if (a->diagnostics) puts("LAUNCH_REQUEST");
    } else if (kind == 3) {
        search(a,output,false); aetheri_development_desktop_v1_activate(a->control,id);
    }
    dirty(a);
}
static void request_close(struct app *a) {
    if (a->closing) return;
    a->closing=true;
    if (send(a->launcher,"Q1",2,MSG_NOSIGNAL|MSG_DONTWAIT)!=2) error(a,"uncertain desktop close request");
    dirty(a);
}
static void configured(void *data, struct zwlr_layer_surface_v1 *layer, uint32_t serial, uint32_t width, uint32_t height) {
    struct view *v = data;
    if (width < 320 || width > 3840 || height < 84 || height > 2160) { error(v->app,"invalid desktop configure"); return; }
    zwlr_layer_surface_v1_ack_configure(layer,serial);
    v->width = (int)width; v->height = (int)height;
    if (!SDL_SetWindowSize(v->window,v->width,v->height)) error(v->app,"custom surface size");
    v->configured = true; v->dirty = true; v->settling=2; wake(v->app,0);
}
static void layer_closed(void *data, struct zwlr_layer_surface_v1 *layer) { (void)layer; struct view *v = data; error(v->app,"desktop surface closed"); }
static const struct zwlr_layer_surface_v1_listener layer_listener = {.configure=configured,.closed=layer_closed};
static void output_geometry(void *data, struct wl_output *o, int32_t x,int32_t y,int32_t pw,int32_t ph,int32_t sub,const char *make,const char *model,int32_t transform) {
    (void)data;(void)o;(void)x;(void)y;(void)pw;(void)ph;(void)sub;(void)make;(void)model;(void)transform;
}
static void output_mode(void *data,struct wl_output *o,uint32_t flags,int32_t w,int32_t h,int32_t refresh) {(void)data;(void)o;(void)flags;(void)w;(void)h;(void)refresh;}
static void output_done(void *data,struct wl_output *o) {(void)data;(void)o;}
static void output_scale(void *data,struct wl_output *o,int32_t scale) {(void)data;(void)o;(void)scale;}
static void output_name(void *data,struct wl_output *o,const char *name) {
    (void)o; struct output *out = data;
    if (strlen(name) < sizeof(out->name)) memcpy(out->name,name,strlen(name)+1);
}
static void output_description(void *data,struct wl_output *o,const char *description) {(void)data;(void)o;(void)description;}
static const struct wl_output_listener output_listener = {.geometry=output_geometry,.mode=output_mode,.done=output_done,.scale=output_scale,.name=output_name,.description=output_description};
static void desktop_output(void *data,struct aetheri_development_desktop_v1 *control,uint32_t index,const char *name) {
    (void)control; struct app *a = data;
    if (index != a->observed_count || index >= 3 || strlen(name) >= sizeof(a->output_names[0])) { error(a,"invalid output snapshot"); return; }
    memcpy(a->output_names[index],name,strlen(name)+1); ++a->observed_count;
}
static void desktop_window(void *data,struct aetheri_development_desktop_v1 *control,uint32_t id,const char *title,const char *app_id,uint32_t active) {
    (void)control; struct app *a = data;
    if (active > 1 || !ae_desktop_model_window(&a->model,id,title,app_id,active != 0)) error(a,"invalid window observation");
    dirty(a); wake(a,0);
}
static void desktop_closed(void *data,struct aetheri_development_desktop_v1 *control,uint32_t id) {
    (void)control; struct app *a=data; ae_desktop_model_closed(&a->model,id); dirty(a); wake(a,0);
}
static void desktop_toggle(void *data,struct aetheri_development_desktop_v1 *control,uint32_t output) {
    (void)control; struct app *a=data;
    if (!a->snapshot || output>=a->observed_count) { error(a,"invalid desktop toggle");return; }
    if (!a->ui_ready) {
        a->pending_toggle=!(a->pending_toggle && a->pending_output==output);a->pending_output=output;return;
    }
    search(a,output,!(a->model.search && a->model.output == output)); wake(a,0);
}
static void desktop_activated(void *data,struct aetheri_development_desktop_v1 *control,uint32_t id,uint32_t found) {
    (void)control; struct app *a=data;
    if (found > 1) error(a,"invalid activation response");
    if (a->diagnostics) printf("ACTIVATED %u %u\n",id,found);
}
static void desktop_retired(void *data,struct aetheri_development_desktop_v1 *control) {(void)control;struct app *a=data;a->retired=true;}
static void desktop_done(void *data,struct aetheri_development_desktop_v1 *control) {(void)control;struct app *a=data;a->snapshot=true;}
static const struct aetheri_development_desktop_v1_listener desktop_listener={.output=desktop_output,.window=desktop_window,.closed=desktop_closed,
    .toggle=desktop_toggle,.activated=desktop_activated,.retired=desktop_retired,.done=desktop_done};
static void global(void *data,struct wl_registry *registry,uint32_t name,const char *interface,uint32_t version) {
    struct app *a=data;
    if (!strcmp(interface,"aetheri_development_desktop_v1")) {
        a->control=wl_registry_bind(registry,name,&aetheri_development_desktop_v1_interface,1);
        if (!a->control || aetheri_development_desktop_v1_add_listener(a->control,&desktop_listener,a)) error(a,"desktop control binding");
    } else if (!strcmp(interface,"zwlr_layer_shell_v1")) {
        if (version < 5) { error(a,"layer protocol version"); return; }
        a->layers=wl_registry_bind(registry,name,&zwlr_layer_shell_v1_interface,5);
    } else if (!strcmp(interface,"wl_output")) {
        if (version < 4 || a->output_count >= 3) { error(a,"output protocol profile"); return; }
        struct output *out=&a->outputs[a->output_count++]; out->object=wl_registry_bind(registry,name,&wl_output_interface,4);
        if (!out->object || wl_output_add_listener(out->object,&output_listener,out)) error(a,"output binding");
    }
}
static void global_remove(void *data,struct wl_registry *registry,uint32_t name) {(void)registry;(void)name;struct app *a=data;error(a,"desktop registry changed");}
static const struct wl_registry_listener registry_listener={.global=global,.global_remove=global_remove};
static bool create_window(struct app *a,unsigned index) {
    struct view *v=&a->views[index];v->app=a;v->index=index;
    SDL_PropertiesID p=SDL_CreateProperties();
    bool ok=p && SDL_SetBooleanProperty(p,SDL_PROP_WINDOW_CREATE_WAYLAND_SURFACE_ROLE_CUSTOM_BOOLEAN,true) &&
        SDL_SetBooleanProperty(p,SDL_PROP_WINDOW_CREATE_OPENGL_BOOLEAN,true) &&
        SDL_SetBooleanProperty(p,SDL_PROP_WINDOW_CREATE_HIGH_PIXEL_DENSITY_BOOLEAN,true) &&
        SDL_SetBooleanProperty(p,SDL_PROP_WINDOW_CREATE_TRANSPARENT_BOOLEAN,true) &&
        SDL_SetNumberProperty(p,SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER,640) && SDL_SetNumberProperty(p,SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER,84) &&
        SDL_SetStringProperty(p,SDL_PROP_WINDOW_CREATE_TITLE_STRING,"Aetheri Desktop");
    if (ok) v->window=SDL_CreateWindowWithProperties(p);
    SDL_DestroyProperties(p);
    if (v->window) v->surface=SDL_GetPointerProperty(SDL_GetWindowProperties(v->window),SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER,NULL);
    return v->window && v->surface;
}
static void event(struct app *a,const SDL_Event *e) {
    if (e->type==SDL_EVENT_QUIT) { request_close(a); return; }
    if (e->type==a->event_type) {
        if (e->user.code=='Q') a->running=false;
        else if (e->user.code==-1) error(a,"launcher connection lost or invalid");
        else if (e->user.code) {
            if (!ae_desktop_model_reply(&a->model,(char)e->user.code)) { error(a,"uncorrelated launcher reply"); return; }
            if (a->diagnostics) printf("LAUNCH_REPLY %c\n",(char)e->user.code);
            if (e->user.code=='A') {
                search(a,a->model.output,false);
                /* Descriptive grouping only. The launch owner always selects
                   its fixed program independently of client metadata. */
                for (size_t i=0;i<a->model.count;++i) if (!strcmp(a->model.windows[i].app_id,"org.aetheri.Settings")) {
                    aetheri_development_desktop_v1_activate(a->control,a->model.windows[i].id);break;
                }
            }
            dirty(a);
        }
        return;
    }
    SDL_WindowID id=0;
    if (e->type>=SDL_EVENT_WINDOW_FIRST && e->type<=SDL_EVENT_WINDOW_LAST) id=e->window.windowID;
    else if (e->type==SDL_EVENT_KEY_DOWN) id=e->key.windowID;
    else if (e->type==SDL_EVENT_TEXT_INPUT) id=e->text.windowID;
    else if (e->type==SDL_EVENT_MOUSE_BUTTON_DOWN || e->type==SDL_EVENT_MOUSE_BUTTON_UP) id=e->button.windowID;
    struct view *v=NULL;
    for (unsigned i=0;i<a->observed_count;++i) if (SDL_GetWindowID(a->views[i].window)==id) v=&a->views[i];
    if (!v) return;
    if (e->type==SDL_EVENT_WINDOW_CLOSE_REQUESTED) request_close(a);
    else if (e->type==SDL_EVENT_WINDOW_FOCUS_GAINED && v->expanded) {
        if (!SDL_StartTextInput(v->window)) error(a,"search text input");
        if (a->diagnostics) printf("FOCUS %u\n",v->index);
    } else if (e->type==SDL_EVENT_WINDOW_EXPOSED || e->type==SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED || e->type==SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED) v->dirty=true;
    else if (e->type==SDL_EVENT_WINDOW_FOCUS_LOST) v->pressed=0;
    else if (e->type==SDL_EVENT_KEY_DOWN) {
        if ((e->key.mod&SDL_KMOD_CTRL) && e->key.key==SDLK_Q) request_close(a);
        else if (e->key.key==SDLK_F6) { a->model.german=!a->model.german;dirty(a); }
        else if (e->key.key==SDLK_F7) { a->model.contrast=!a->model.contrast;dirty(a); }
        else if (v->expanded) {
            if (e->key.key==SDLK_ESCAPE) search(a,v->index,false);
            else if (e->key.key==SDLK_BACKSPACE) ae_desktop_model_backspace(&a->model);
            else if (e->key.key==SDLK_TAB || e->key.key==SDLK_DOWN || e->key.key==SDLK_UP)
                ae_desktop_model_select(&a->model,e->key.key==SDLK_UP || (e->key.mod&SDL_KMOD_SHIFT) ? -1 : 1);
            else if (e->key.key==SDLK_RETURN && !e->key.repeat) {
                struct ae_desktop_result results[AE_DESKTOP_WINDOWS+1];size_t n=ae_desktop_model_results(&a->model,results);
                for (size_t i=0;i<n;++i) if (results[i].window==a->model.selected) activate(a,results[i].window ? 3u : 2u,results[i].window,v->index);
            }
            v->dirty=true;
        }
    } else if (e->type==SDL_EVENT_TEXT_INPUT && v->expanded) { (void)ae_desktop_model_text(&a->model,e->text.text);v->dirty=true; }
    else if (e->type==SDL_EVENT_MOUSE_BUTTON_DOWN && e->button.button==SDL_BUTTON_LEFT) {
        v->pressed=0;
        for (size_t i=0;i<v->count;++i) if (inside(v->controls[i].rect,e->button.x,e->button.y)) {
            v->pressed=v->controls[i].kind;v->pressed_window=v->controls[i].window;break;
        }
    } else if (e->type==SDL_EVENT_MOUSE_BUTTON_UP && e->button.button==SDL_BUTTON_LEFT) {
        bool hit=false;
        for (size_t i=0;i<v->count;++i) {
            struct control c=v->controls[i];
            if (c.kind==v->pressed && c.window==v->pressed_window && inside(c.rect,e->button.x,e->button.y)) {
                activate(a,c.kind,c.window,v->index);hit=true;break;
            }
        }
        if (!hit && !v->pressed && v->expanded) {
            SDL_FRect card=card_rect(v);
            if (!inside(card,e->button.x,e->button.y)) search(a,v->index,false);
        }
        v->pressed=0;
    }
}
int main(int argc,char **argv) {
    struct app a={.launcher=-1,.signals={-1,-1},.running=true};const char *font=NULL,*cache=NULL;
    for (int i=1;i<argc;++i) {
        if (!strcmp(argv[i],"--font") && !font && i+1<argc) font=argv[++i];
        else if (!strcmp(argv[i],"--cache") && !cache && i+1<argc) cache=argv[++i];
        else if (!strcmp(argv[i],"--launcher-fd") && a.launcher<0 && i+1<argc) {
            char *end=NULL;const char *value=argv[++i];errno=0;long fd=strtol(value,&end,10);
            if (!*value || strspn(value,"0123456789")!=strlen(value) || errno || !end || *end || fd<3 || fd>INT32_MAX) return 2;
            a.launcher=(int)fd;
        } else if (!strcmp(argv[i],"--diagnostics")) a.diagnostics=true;
        else return 2;
    }
    struct stat st;int type=0;socklen_t size=sizeof(type);struct sockaddr_storage peer;socklen_t peer_size=sizeof(peer);
    if (!font || !cache || cache[0]!='/' || a.launcher<3 || getuid()==0 || getuid()!=geteuid() || getgid()!=getegid() ||
            lstat(cache,&st) || !S_ISDIR(st.st_mode) || st.st_uid!=getuid() || (st.st_mode&07777)!=0700 ||
            getsockopt(a.launcher,SOL_SOCKET,SO_TYPE,&type,&size) || type!=SOCK_SEQPACKET ||
            getpeername(a.launcher,(struct sockaddr *)&peer,&peer_size) || peer.ss_family!=AF_UNIX || fcntl(a.launcher,F_SETFD,FD_CLOEXEC)) return 2;
    if (setenv("MESA_SHADER_CACHE_DIR",cache,1) || setenv("MESA_SHADER_CACHE_MAX_SIZE","32M",1)) return 1;
    setvbuf(stdout,NULL,_IOLBF,0);ae_desktop_model_init(&a.model);
    if (!SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"wayland") || !SDL_Init(SDL_INIT_VIDEO) || !TTF_Init()) { fprintf(stderr,"desktop initialization: %s\n",SDL_GetError());SDL_Quit();return 1; }
    a.event_type=SDL_RegisterEvents(1);
    int fm,fn,fp,hm,hn,hp;TTF_GetFreeTypeVersion(&fm,&fn,&fp);TTF_GetHarfBuzzVersion(&hm,&hn,&hp);
    if (!a.event_type || SDL_GetVersion()!=SDL_VERSION || TTF_Version()!=SDL_TTF_VERSION || fm!=2 || fn!=15 || fp!=0 || hm!=15 || hn!=0 || hp!=0) error(&a,"unreviewed SDL/text version");
    a.font=TTF_OpenFont(font,16);
    if (!a.font || !create_window(&a,0)) error(&a,"window/font/renderer");
    if (!a.failed) {
        a.display=SDL_GetPointerProperty(SDL_GetWindowProperties(a.views[0].window),SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER,NULL);
        a.registry=a.display ? wl_display_get_registry(a.display) : NULL;
        if (!a.registry || wl_registry_add_listener(a.registry,&registry_listener,&a) || wl_display_roundtrip(a.display)<0 || wl_display_roundtrip(a.display)<0 ||
                !a.control || !a.layers || !a.snapshot || !a.observed_count || a.observed_count!=a.output_count) error(&a,"private desktop discovery");
    }
    for (unsigned i=0;!a.failed && i<a.observed_count;++i) {
        if (i && !create_window(&a,i)) { error(&a,"output window");break; }
        struct wl_output *o=NULL;
        for (unsigned j=0;j<a.output_count;++j) if (!strcmp(a.output_names[i],a.outputs[j].name)) o=a.outputs[j].object;
        struct view *v=&a.views[i];
        if (!o) { error(&a,"output name binding");break; }
        v->layer=zwlr_layer_shell_v1_get_layer_surface(a.layers,v->surface,o,ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY,"aetheri.desktop");
        if (!v->layer || zwlr_layer_surface_v1_add_listener(v->layer,&layer_listener,v)) { error(&a,"desktop role");break; }
        state(v,false);
    }
    struct sigaction previous_int={0},previous_term={0};bool handlers=false;
    if (!a.failed && (pipe2(a.signals,O_CLOEXEC|O_NONBLOCK) || wl_display_roundtrip(a.display)<0)) error(&a,"initial configure/signal pipe");
    for (unsigned i=0;!a.failed && i<a.observed_count;++i) {
        struct view *v=&a.views[i];
        if (!v->configured) { error(&a,"missing initial configure");break; }
        v->renderer=SDL_CreateRenderer(v->window,"opengles2");
        if (!v->renderer || !SDL_SetRenderDrawBlendMode(v->renderer,SDL_BLENDMODE_BLEND)) error(&a,"configured renderer");
    }
    if (!a.failed) {
        struct sigaction action={.sa_handler=signalled};sigemptyset(&action.sa_mask);signal_writer=a.signals[1];
        if (sigaction(SIGINT,&action,&previous_int) || sigaction(SIGTERM,&action,&previous_term)) error(&a,"signal handler");
        else handlers=true;
        a.worker=SDL_CreateThread(worker,"desktop-launch",&a);
        if (!a.worker) error(&a,"launch event worker");
    }
    if (!a.failed) {
        a.ui_ready=true;
        if (a.pending_toggle) search(&a,a.pending_output,true);
        printf("READY wayland opengles2 outputs=%u\n",a.observed_count);
        while (a.running) {
            for (unsigned i=0;i<a.observed_count && a.running;++i) if (a.views[i].dirty) draw(&a.views[i]);
            if (!a.running) break;
            SDL_Event e;if (!SDL_WaitEvent(&e)) { error(&a,"event wait");break; } event(&a,&e);
        }
    }
    if (a.worker) { (void)shutdown(a.launcher,SHUT_RDWR);SDL_WaitThread(a.worker,NULL); }
    signal_writer=-1;
    if (handlers) { (void)sigaction(SIGINT,&previous_int,NULL);(void)sigaction(SIGTERM,&previous_term,NULL); }
    for (unsigned i=0;i<2;++i) if (a.signals[i]>=0) close(a.signals[i]);
    close(a.launcher);
    if (a.control && !a.failed) {
        aetheri_development_desktop_v1_retire(a.control);
        if (wl_display_roundtrip(a.display)<0 || !a.retired) error(&a,"desktop retirement acknowledgement");
    }
    for (unsigned i=0;i<3;++i) {
        if (a.views[i].frame) { wl_callback_destroy(a.views[i].frame);a.views[i].frame=NULL; }
        if (a.views[i].layer) zwlr_layer_surface_v1_destroy(a.views[i].layer);
        SDL_DestroyRenderer(a.views[i].renderer);SDL_DestroyWindow(a.views[i].window);
        if (a.outputs[i].object) wl_output_release(a.outputs[i].object);
    }
    if (a.control) aetheri_development_desktop_v1_destroy(a.control);
    if (a.layers) zwlr_layer_shell_v1_destroy(a.layers);
    if (a.registry) wl_registry_destroy(a.registry);
    TTF_CloseFont(a.font);TTF_Quit();SDL_Quit();printf("CLOSED frames=%u failed=%d\n",a.frames,a.failed);
    return a.failed ? 1 : 0;
}
