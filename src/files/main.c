/* Local development view; normal app/document grants remain separate. */
#define _GNU_SOURCE
#include "model.h"
#include "launch.h"
#include "text/grapheme.h"
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <poll.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
struct app {
    SDL_Window *window;SDL_Renderer *renderer;TTF_Font *font;SDL_Thread *worker;struct ae_files *files;struct ae_files_child child;
    const char *terminal,*editor,*terminal_font,*cache;char message[256];
    float scale,density,row_height,top,bottom;size_t first,rows;SDL_FRect back,hidden,search;
    bool running,failed,closing,dirty,german,contrast,show_hidden,query_focus,query_bad,diagnostics;unsigned frames,launches,pressed;size_t pressed_row;
    int wake[2];uint32_t event;atomic_bool active;
};
static volatile sig_atomic_t signal_writer=-1,stopping;
static void signalled(int number) { stopping=number;int saved=errno;unsigned char b=1;if (signal_writer>=0) (void)write(signal_writer,&b,1);errno=saved; }
static int wake_worker(void *data) {
    struct app *a=data;while (atomic_load(&a->active)) { struct pollfd p={a->wake[0],POLLIN,0};int n=poll(&p,1,-1);if (n<0 && errno==EINTR) continue;if (n<0) break;
        unsigned char bytes[64];while (read(a->wake[0],bytes,sizeof(bytes))>0) {}SDL_Event e={.type=a->event};(void)SDL_PushEvent(&e); }return 0;
}
static bool busy(const struct app *a) { return a->child.pid>0 && !a->child.ended; }
static const char *tr(const struct app *a,const char *en,const char *de) { return a->german ? de : en; }
static void message(struct app *a,const char *s) { snprintf(a->message,sizeof(a->message),"%s",s);a->dirty=true; }
static void fail(struct app *a,const char *s) { fprintf(stderr,"files: %s: %s\n",s,SDL_GetError());a->failed=true;a->closing=true;if (!busy(a)) a->running=false; }
static void result(struct app *a,enum ae_files_result r) {
    const char *s=r==AE_FILES_OK ? "" : r==AE_FILES_ROOT ? tr(a,"At the selected root","Am gewählten Ausgangsordner") : r==AE_FILES_CHANGED ? tr(a,"Folder or file changed; Ctrl+R refreshes","Ordner oder Datei geändert; Strg+R aktualisiert") : r==AE_FILES_LIMIT ? tr(a,"Folder/depth limit; view retained","Ordner-/Tiefenlimit; Ansicht bleibt erhalten") : r==AE_FILES_NOMEM ? tr(a,"Not enough memory; view retained","Nicht genug Speicher; Ansicht bleibt erhalten") : r==AE_FILES_UNSUPPORTED ? tr(a,"This entry cannot be opened here","Dieser Eintrag lässt sich hier nicht öffnen") : tr(a,"Cannot read this folder","Ordner kann nicht gelesen werden");
    message(a,s);if (r==AE_FILES_OK) { a->first=0;if (!*ae_files_query(a->files)) a->query_bad=false; }
}
static void reveal(struct app *a) { size_t selected=ae_files_selected(a->files);if (selected<a->first) a->first=selected;else if (a->rows && selected>=a->first+a->rows) a->first=selected-a->rows+1; }
static void move(struct app *a,int amount) {
    size_t count=ae_files_count(a->files),old=ae_files_selected(a->files);if (!count) return;
    size_t n=amount<0 ? (size_t)(-amount)>old ? 0 : old-(size_t)(-amount) : (size_t)amount>=count-old ? count-1 : old+(size_t)amount;
    (void)ae_files_select(a->files,n);reveal(a);a->dirty=true;
}
static void request_close(struct app *a) { a->closing=true;if (busy(a)) message(a,tr(a,"Close the document window first; Files is waiting","Zuerst das Dokumentfenster schließen; Dateien wartet"));else a->running=false; }
static void activate(struct app *a) {
    if (a->closing || a->query_bad) return;
    const struct ae_files_entry *e=ae_files_entry(a->files,ae_files_selected(a->files));if (!e) return;
    if (e->kind==AE_FILES_DIRECTORY) { result(a,ae_files_enter(a->files));return; }
    if (a->launches>=256) { message(a,tr(a,"Document-start limit reached; reopen Files","Dokumentstart-Limit erreicht; Dateien neu öffnen"));return; }
    if (busy(a) || a->child.pid>0) { message(a,tr(a,"One document window at a time","Nur ein Dokumentfenster gleichzeitig"));return; }
    int directory=-1,witness=-1;char leaf[AE_FILES_NAME+1];enum ae_files_result r=ae_files_open_selected(a->files,&directory,&witness,leaf);
    if (r!=AE_FILES_OK) { result(a,r);return; }
    ++a->launches;int spawned=ae_files_launch(&a->child,a->terminal,a->editor,a->terminal_font,a->cache,directory,witness,leaf,a->diagnostics);close(directory);close(witness);
    if (spawned) message(a,tr(a,"Could not start the document window","Dokumentfenster konnte nicht gestartet werden"));
    else { message(a,tr(a,"Document window requested","Dokumentfenster angefordert"));if (a->diagnostics) printf("OPEN pid=%d pin=%d\n",a->child.pid,a->child.pin>=0); }
}
static void child_update(struct app *a) {
    if (a->child.pid<=0) return;
    int r=ae_files_child_observe(&a->child);if (r<0) { a->failed=true;message(a,"Document observation unresolved; new starts and close remain blocked");return; }
    if (r) { int status=a->child.status;if (a->diagnostics) printf("DOCUMENT_END status=%d\n",status);(void)ae_files_child_close(&a->child);
        enum ae_files_result refreshed=ae_files_refresh(a->files);if (refreshed!=AE_FILES_OK) result(a,refreshed);else { a->first=0;message(a,status ? tr(a,"Document window ended with an error","Dokumentfenster mit Fehler beendet") : tr(a,"Document window closed","Dokumentfenster geschlossen")); }if (a->closing) a->running=false; }
}
static void fill(struct app *a,SDL_FRect rect,unsigned rgb) { if (!SDL_SetRenderDrawColor(a->renderer,(Uint8)(rgb>>16),(Uint8)(rgb>>8),(Uint8)rgb,255) || !SDL_RenderFillRect(a->renderer,&rect)) fail(a,"rectangle"); }
static void text(struct app *a,const char *s,float x,float y,float width,float points,unsigned rgb) {
    if (!*s || width<1) return;
    if (!TTF_SetFontSize(a->font,points*a->scale*a->density)) { fail(a,"font size");return; }
    size_t size=strlen(s),fit=0;if (size>AE_FILES_LABEL || !TTF_MeasureString(a->font,s,size,(int)floorf(width*a->density),NULL,&fit)) { fail(a,"label measurement");return; }
    char clipped[AE_FILES_LABEL+4];
    if (fit<size) {
        int mark=0;if (!TTF_GetStringSize(a->font,"…",3,&mark,NULL)) { fail(a,"ellipsis");return; }
        int available=(int)floorf(width*a->density)-mark;if (available<=0) return;
        if (!TTF_MeasureString(a->font,s,size,available,NULL,&fit)) { fail(a,"label fit");return; }
        size_t offsets[AE_FILES_LABEL+1],count;if (!ae_text_graphemes(s,size,offsets,AE_FILES_LABEL+1,&count)) { fail(a,"label boundaries");return; }
        size_t at=0;while (at+1<count && offsets[at+1]<=fit) ++at;fit=offsets[at];memcpy(clipped,s,fit);memcpy(clipped+fit,"…",4);s=clipped;size=fit+3;
    }
    SDL_Surface *surface=TTF_RenderText_Blended(a->font,s,size,(SDL_Color){(Uint8)(rgb>>16),(Uint8)(rgb>>8),(Uint8)rgb,255});if (!surface) { fail(a,"text");return; }
    SDL_Texture *texture=SDL_CreateTextureFromSurface(a->renderer,surface);SDL_FRect dst={x,y,(float)surface->w/a->density,(float)surface->h/a->density};SDL_DestroySurface(surface);
    SDL_Rect clip={(int)x,(int)y,(int)width,(int)ceilf(dst.h)};
    if (!texture || !SDL_SetRenderClipRect(a->renderer,&clip) || !SDL_RenderTexture(a->renderer,texture,NULL,&dst)) fail(a,"label");
    (void)SDL_SetRenderClipRect(a->renderer,NULL);SDL_DestroyTexture(texture);
}
static void draw(struct app *a) {
    int w,h,pw,ph;if (!SDL_GetWindowSize(a->window,&w,&h) || !SDL_GetWindowSizeInPixels(a->window,&pw,&ph)) { fail(a,"size");return; }
    a->density=SDL_GetWindowPixelDensity(a->window);if (!isfinite(a->density) || a->density<.5f || a->density>4 || !SDL_SetRenderScale(a->renderer,(float)pw/(float)w,(float)ph/(float)h)) { fail(a,"density");return; }
    unsigned bg=a->contrast ? 0u : 0x10141cu,panel=a->contrast ? 0u : 0x1b2230u,fg=a->contrast ? 0xffffffu : 0xe8edf5u;
    if (!SDL_SetRenderDrawColor(a->renderer,(Uint8)(bg>>16),(Uint8)(bg>>8),(Uint8)bg,255) || !SDL_RenderClear(a->renderer)) { fail(a,"clear");return; }
    float header=32*a->scale+24,control=26*a->scale+16;a->top=header+2*control+12;a->bottom=(float)h-(48*a->scale+16);a->row_height=34*a->scale+18;
    a->rows=a->bottom>a->top ? (size_t)((a->bottom-a->top)/a->row_height) : 0;reveal(a);
    text(a,tr(a,"Files","Dateien"),20,12,(float)w-40,24,fg);text(a,ae_files_folder(a->files),20,header,(float)w-40,14,0xa5b7d0);
    a->back=(SDL_FRect){20,header+control,100*a->scale,control-4};a->hidden=(SDL_FRect){28+a->back.w,header+control,170*a->scale,control-4};
    fill(a,a->back,panel);fill(a,a->hidden,panel);text(a,tr(a,"Back","Zurück"),28,a->back.y+5,a->back.w-16,14,fg);
    text(a,a->show_hidden ? tr(a,"Hide hidden","Versteckte aus") : tr(a,"Show hidden","Versteckte an"),a->hidden.x+8,a->hidden.y+5,a->hidden.w-16,14,fg);
    a->search=(SDL_FRect){a->hidden.x+a->hidden.w+8,header+control,(float)w-a->hidden.x-a->hidden.w-28,control-4};
    if (a->search.w>=80) { fill(a,a->search,a->query_focus ? 0x29466b : panel);text(a,*ae_files_query(a->files) ? ae_files_query(a->files) : tr(a,"Ctrl+L filter","Strg+L Filter"),a->search.x+8,a->search.y+5,a->search.w-16,14,fg); }
    else { a->search=(SDL_FRect){20,header+2*control,(float)w-40,control-4};a->top+=control;fill(a,a->search,a->query_focus ? 0x29466b : panel);text(a,*ae_files_query(a->files) ? ae_files_query(a->files) : tr(a,"Ctrl+L filter","Strg+L Filter"),28,a->search.y+5,a->search.w-16,14,fg);a->rows=a->bottom>a->top ? (size_t)((a->bottom-a->top)/a->row_height) : 0;reveal(a); }
    for (size_t i=0;i<a->rows && a->first+i<ae_files_count(a->files);++i) {
        size_t index=a->first+i;const struct ae_files_entry *e=ae_files_entry(a->files,index);float y=a->top+(float)i*a->row_height;
        if (index==ae_files_selected(a->files)) fill(a,(SDL_FRect){16,y,(float)w-32,a->row_height-4},0x29466b);
        text(a,e->label,28,y+3,(float)w-56,16,fg);char detail[96];const char *kind=e->kind==AE_FILES_DIRECTORY ? tr(a,"Folder","Ordner") : e->kind==AE_FILES_REGULAR ? tr(a,"File","Datei") : e->kind==AE_FILES_LINK ? tr(a,"Link (not followed)","Link (nicht gefolgt)") : tr(a,"Unavailable here","Hier nicht verfügbar");
        snprintf(detail,sizeof(detail),"%s · %llu bytes",kind,(unsigned long long)e->size);text(a,detail,28,y+20*a->scale+4,(float)w-56,12,0xa5b7d0);
    }
    if (!ae_files_count(a->files)) text(a,tr(a,"No matching entries","Keine passenden Einträge"),28,a->top,(float)w-56,16,fg);
    char footer[160];snprintf(footer,sizeof(footer),"%zu / %zu · %s",ae_files_count(a->files),ae_files_total(a->files),tr(a,"Enter open · Alt+Up back · Ctrl+R refresh","Enter öffnen · Alt+Hoch zurück · Strg+R neu"));
    text(a,a->closing && *a->message ? a->message : !a->rows ? tr(a,"Enlarge the window to browse","Fenster zum Navigieren vergrößern") : *a->message ? a->message : footer,20,a->bottom+4,(float)w-40,12,fg);text(a,tr(a,"F6 language · F7 contrast · F8 text · Ctrl+Q close","F6 Sprache · F7 Kontrast · F8 Text · Strg+Q schließen"),20,a->bottom+24*a->scale,(float)w-40,12,0xa5b7d0);
    if (!SDL_RenderPresent(a->renderer)) fail(a,"present");
    a->dirty=false;++a->frames;
    if (a->diagnostics) printf("VIEW frame=%u count=%zu total=%zu selected=%zu top=%zu rows=%zu depth=%u scale=%.2f german=%d contrast=%d filter=%d rejected=%d child=%d width=%d height=%d\n",a->frames,ae_files_count(a->files),ae_files_total(a->files),ae_files_selected(a->files),a->first,a->rows,ae_files_depth(a->files),(double)a->scale,a->german,a->contrast,a->query_focus,a->query_bad,a->child.pid,w,h);
}
static bool inside(SDL_FRect r,float x,float y) { return r.w>0 && x>=r.x && y>=r.y && x<r.x+r.w && y<r.y+r.h; }
static unsigned target(struct app *a,float x,float y,size_t *row) {
    *row=0;if (inside(a->back,x,y)) return 1;if (inside(a->hidden,x,y)) return 2;if (inside(a->search,x,y)) return 3;
    if (y>=a->top && y<a->bottom && a->rows && x>=16) { size_t n=(size_t)((y-a->top)/a->row_height);if (n<a->rows && a->first+n<ae_files_count(a->files)) { *row=a->first+n;return 4; } }return 0;
}
static SDL_HitTestResult hit_test(SDL_Window *window,const SDL_Point *p,void *data) {
    (void)data;int w,h;if (!SDL_GetWindowSize(window,&w,&h) || (SDL_GetWindowFlags(window)&(SDL_WINDOW_MAXIMIZED|SDL_WINDOW_FULLSCREEN))) return SDL_HITTEST_NORMAL;
    bool l=p->x<8,r=p->x>=w-8,t=p->y<8,b=p->y>=h-8;
    if (t&&l) return SDL_HITTEST_RESIZE_TOPLEFT;
    if (t&&r) return SDL_HITTEST_RESIZE_TOPRIGHT;
    if (b&&l) return SDL_HITTEST_RESIZE_BOTTOMLEFT;
    if (b&&r) return SDL_HITTEST_RESIZE_BOTTOMRIGHT;
    if (l) return SDL_HITTEST_RESIZE_LEFT;
    if (r) return SDL_HITTEST_RESIZE_RIGHT;
    if (t) return SDL_HITTEST_RESIZE_TOP;
    if (b) return SDL_HITTEST_RESIZE_BOTTOM;
    return p->y<44 ? SDL_HITTEST_DRAGGABLE : SDL_HITTEST_NORMAL;
}
static void event(struct app *a,const SDL_Event *e) {
    if (e->type==SDL_EVENT_QUIT || e->type==SDL_EVENT_WINDOW_CLOSE_REQUESTED) request_close(a);
    else if (e->type==SDL_EVENT_KEY_DOWN && !e->key.repeat) {
        a->pressed=0;
        SDL_Keycode k=e->key.key;SDL_Keymod mod=e->key.mod;
        if ((mod&SDL_KMOD_CTRL) && k==SDLK_Q) request_close(a);
        else if ((mod&SDL_KMOD_CTRL) && k==SDLK_R) result(a,ae_files_refresh(a->files));
        else if ((mod&SDL_KMOD_CTRL) && k==SDLK_H) { a->show_hidden=!a->show_hidden;ae_files_hidden(a->files,a->show_hidden);reveal(a);a->dirty=true; }
        else if ((mod&SDL_KMOD_CTRL) && (k==SDLK_L || (k==SDLK_U && a->query_focus))) { a->query_focus=true;a->query_bad=false;(void)ae_files_filter(a->files,"",0);a->dirty=true; }
        else if ((mod&SDL_KMOD_ALT) && k==SDLK_UP) result(a,ae_files_up(a->files));
        else if (k==SDLK_ESCAPE) { a->query_focus=false;a->query_bad=false;(void)ae_files_filter(a->files,"",0);a->first=0;a->dirty=true; }
        else if (k==SDLK_F6) { a->german=!a->german;a->dirty=true; }
        else if (k==SDLK_F7) { a->contrast=!a->contrast;a->dirty=true; }
        else if (k==SDLK_F8) { a->scale=a->scale==1 ? 1.25f : a->scale==1.25f ? 1.5f : a->scale==1.5f ? 2 : 1;a->dirty=true; }
        else if (k==SDLK_UP || k==SDLK_DOWN) move(a,k==SDLK_UP ? -1 : 1);
        else if (k==SDLK_PAGEUP || k==SDLK_PAGEDOWN) move(a,(int)(a->rows ? a->rows : 1)*(k==SDLK_PAGEUP ? -1 : 1));
        else if (k==SDLK_HOME || k==SDLK_END) { if (ae_files_count(a->files)) (void)ae_files_select(a->files,k==SDLK_HOME ? 0 : ae_files_count(a->files)-1);reveal(a);a->dirty=true; }
        else if (k==SDLK_RETURN) { a->query_focus=false;if (a->rows) activate(a); }
        else if (k==SDLK_BACKSPACE) { if (a->query_focus) { const char *q=ae_files_query(a->files);size_t n=strlen(q),points[AE_FILES_QUERY+1],count;if (!a->query_bad && n && ae_text_graphemes(q,n,points,AE_FILES_QUERY+1,&count)) (void)ae_files_filter(a->files,q,points[count-2]);a->dirty=true; }else result(a,ae_files_up(a->files)); }
    } else if (e->type==SDL_EVENT_TEXT_INPUT && a->query_focus) {
        a->pressed=0;if (a->query_bad) return;
        char q[AE_FILES_QUERY+1];size_t old=strlen(ae_files_query(a->files)),n=strlen(e->text.text);
        if (n<=AE_FILES_QUERY-old) { memcpy(q,ae_files_query(a->files),old);memcpy(q+old,e->text.text,n);if (ae_files_filter(a->files,q,old+n)) { a->first=0;a->dirty=true; }else { a->query_bad=true;message(a,"Filter rejected; Ctrl+L clears"); } }else { a->query_bad=true;message(a,"Filter rejected; Ctrl+L clears"); }
    } else if (e->type==SDL_EVENT_MOUSE_WHEEL) { a->pressed=0;move(a,e->wheel.y>0 ? -3 : e->wheel.y<0 ? 3 : 0); }
    else if (e->type==SDL_EVENT_MOUSE_BUTTON_DOWN && e->button.button==SDL_BUTTON_LEFT) a->pressed=target(a,e->button.x,e->button.y,&a->pressed_row);
    else if (e->type==SDL_EVENT_MOUSE_BUTTON_UP && e->button.button==SDL_BUTTON_LEFT) {
        size_t row;unsigned hit=target(a,e->button.x,e->button.y,&row),pressed=a->pressed;a->pressed=0;if (hit!=pressed) return;
        if (hit==1) result(a,ae_files_up(a->files));
        else if (hit==2) { a->show_hidden=!a->show_hidden;ae_files_hidden(a->files,a->show_hidden);reveal(a);a->dirty=true; }
        else if (hit==3) { a->query_focus=true;a->dirty=true; }
        else if (hit==4 && row==a->pressed_row && ae_files_select(a->files,row)) { a->query_focus=false;a->dirty=true;if (e->button.clicks==2) activate(a); }
    } else if (e->type==SDL_EVENT_WINDOW_FOCUS_LOST) a->pressed=0;
    else if (e->type==SDL_EVENT_WINDOW_EXPOSED || e->type==SDL_EVENT_WINDOW_RESIZED || e->type==SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) { a->pressed=0;a->dirty=true; }
}
int main(int argc,char **argv) {
    struct app a={.child={.pid=-1,.pin=-1},.wake={-1,-1},.scale=1,.running=true,.dirty=true};const char *root=NULL,*font=NULL;
    for (int i=1;i<argc;++i) {
        const char **slot=!strcmp(argv[i],"--root") ? &root : !strcmp(argv[i],"--font") ? &font : !strcmp(argv[i],"--cache") ? &a.cache : !strcmp(argv[i],"--terminal") ? &a.terminal : !strcmp(argv[i],"--editor") ? &a.editor : !strcmp(argv[i],"--terminal-font") ? &a.terminal_font : NULL;
        if (slot && !*slot && i+1<argc) *slot=argv[++i];else if (!strcmp(argv[i],"--diagnostics") && !a.diagnostics) a.diagnostics=true;else return 2;
    }
    struct stat st;if (!root || !font || !a.cache || !a.terminal || !a.editor || !a.terminal_font || root[0]!='/' || getuid()==0 || getuid()!=geteuid() || getgid()!=getegid() || lstat(a.cache,&st) || !S_ISDIR(st.st_mode) || st.st_uid!=getuid() || (st.st_mode&07777)!=0700) return 2;
    setvbuf(stdout,NULL,_IOLBF,0);int fd=open(root,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);enum ae_files_result r;a.files=fd<0 ? NULL : ae_files_create(fd,&r);if (fd>=0) close(fd);if (!a.files) return 1;
    struct sigaction child={.sa_handler=SIG_DFL};sigemptyset(&child.sa_mask);
    if (sigaction(SIGCHLD,&child,NULL) || setenv("MESA_SHADER_CACHE_DIR",a.cache,1) || setenv("MESA_SHADER_CACHE_MAX_SIZE","32M",1) || !SDL_SetAppMetadata("Aetheri Files","0.0.1","org.aetheri.Files") || !SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"wayland") || !SDL_Init(SDL_INIT_VIDEO) || !TTF_Init()) { a.failed=true;goto done; }
    int fm,fn,fp,hm,hn,hp;TTF_GetFreeTypeVersion(&fm,&fn,&fp);TTF_GetHarfBuzzVersion(&hm,&hn,&hp);
    if (SDL_GetVersion()!=SDL_VERSION || TTF_Version()!=SDL_TTF_VERSION || fm!=2 || fn!=15 || fp!=0 || hm!=15 || hn!=0 || hp!=0) { a.failed=true;fputs("files: unreviewed SDL/text version\n",stderr);goto done; }
    a.window=SDL_CreateWindow("Aetheri Files",1000,680,SDL_WINDOW_RESIZABLE|SDL_WINDOW_HIGH_PIXEL_DENSITY);a.font=TTF_OpenFont(font,16);if (a.window) a.renderer=SDL_CreateRenderer(a.window,"opengles2");
    if (!a.window || !a.font || !a.renderer || !SDL_SetWindowMinimumSize(a.window,640,420) || !SDL_SetWindowHitTest(a.window,hit_test,&a) || !SDL_StartTextInput(a.window) || pipe2(a.wake,O_CLOEXEC|O_NONBLOCK)) { a.failed=true;goto done; }
    a.event=SDL_RegisterEvents(1);atomic_init(&a.active,true);signal_writer=a.wake[1];struct sigaction action={.sa_handler=signalled};sigemptyset(&action.sa_mask);
    if (!a.event || sigaction(SIGINT,&action,NULL) || sigaction(SIGTERM,&action,NULL) || !(a.worker=SDL_CreateThread(wake_worker,"files-signal",&a))) { a.failed=true;goto done; }
    if (a.diagnostics) puts("READY files wayland opengles2");
    while (a.running) {
        child_update(&a);if (stopping) { stopping=0;request_close(&a); }if (!a.running) break;
        if (a.dirty && !a.failed) draw(&a);
        if (!a.running) break;
        SDL_Event e;bool got=busy(&a) ? SDL_WaitEventTimeout(&e,200) : SDL_WaitEvent(&e);if (got) event(&a,&e);else if (!busy(&a)) fail(&a,"event wait");
    }
done:
    if (a.worker) { atomic_store(&a.active,false);unsigned char b=1;(void)write(a.wake[1],&b,1);SDL_WaitThread(a.worker,NULL); }
    signal_writer=-1;for (unsigned i=0;i<2;++i) if (a.wake[i]>=0) close(a.wake[i]);
    if (a.child.pid>0) { if (!a.child.ended) { fputs("files: refusing to retire a live document child\n",stderr);return 125; }(void)ae_files_child_close(&a.child); }
    SDL_DestroyRenderer(a.renderer);SDL_DestroyWindow(a.window);TTF_CloseFont(a.font);TTF_Quit();SDL_Quit();ae_files_destroy(a.files);
    if (a.diagnostics) printf("CLOSED files frames=%u failed=%d\n",a.frames,a.failed);
    return a.failed ? 1 : 0;
}
