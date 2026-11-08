/* Local development view; normal app/document grants remain separate. */
#define _GNU_SOURCE
#include "session.h"
#include "launch.h"
#include "create.h"
#include "rename.h"
#include "layout.h"
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
    struct ae_files_session session;
    SDL_Window *window;SDL_Renderer *renderer;TTF_Font *font;SDL_Thread *worker,*creator;
    float density;SDL_FRect back,hidden,search,new_file,new_folder,name_field,cancel,submit;
    int wake[2],notifications;uint32_t event;atomic_bool active,external_changed,notification_failed;
    SDL_FRect more,menu_rect,menu_rows[3],list_button,grid_button,details_button,close_button,fullscreen_button,info_close;
    SDL_FRect places[AE_FILES_DEPTH+1],info_rect;
};
static volatile sig_atomic_t signal_writer=-1,stopping;
static void signalled(int number) { stopping=number;int saved=errno;unsigned char b=1;if (signal_writer>=0) (void)write(signal_writer,&b,1);errno=saved; }
static int wake_worker(void *data) {
    struct app *a=data;while (atomic_load(&a->active)) {
        struct pollfd p[2]={{a->wake[0],POLLIN,0},{atomic_load(&a->notification_failed) ? -1 : a->notifications,POLLIN,0}};
        int n=poll(p,2,-1);if (n<0 && errno==EINTR) continue;if (n<0) break;
        if (p[0].revents) { unsigned char bytes[64];while (read(a->wake[0],bytes,sizeof(bytes))>0) {} }
        if (p[1].revents) {
            int changed=ae_files_notifications_read(a->notifications);
            if (changed<0 || (p[1].revents&(POLLERR|POLLHUP|POLLNVAL))) atomic_store(&a->notification_failed,true);
            else if (changed) atomic_store(&a->external_changed,true);
        }
        SDL_Event e={.type=a->event};(void)SDL_PushEvent(&e);
    }return 0;
}

static void fail(struct app *a,const char *s) { fprintf(stderr,"files: %s: %s\n",s,SDL_GetError());a->session.failed=true;a->session.closing=true;if (!ae_files_session_busy(&a->session) && !ae_files_session_creating(&a->session)) a->session.running=false; }

static int create_worker(void *data) {
    struct app *a=data;ae_files_session_apply_worker(&a->session);
    SDL_Event e={.type=a->event};(void)SDL_PushEvent(&e);return 0;
}
static bool start_worker(void *data,struct ae_files_session *session) {
    struct app *a=data;
    if (session!=&a->session || a->creator) return false;
    a->creator=SDL_CreateThread(create_worker,session->renaming ? "files-rename" : "files-create",a);return a->creator!=NULL;
}
static void join_worker(void *data) {
    struct app *a=data;SDL_WaitThread(a->creator,NULL);a->creator=NULL;
}

static void fill(struct app *a,SDL_FRect rect,unsigned rgb) { if (!SDL_SetRenderDrawColor(a->renderer,(Uint8)(rgb>>16),(Uint8)(rgb>>8),(Uint8)rgb,255) || !SDL_RenderFillRect(a->renderer,&rect)) fail(a,"rectangle"); }
static void text(struct app *a,const char *s,float x,float y,float width,float points,unsigned rgb) {
    if (!*s || width<1) return;
    if (!TTF_SetFontSize(a->font,points*a->session.scale*a->density)) { fail(a,"font size");return; }
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
static void name_text(struct app *a,float x,float y,float width,unsigned fg) {
    /* Keep the editable tail visible without splitting a UTF-8 grapheme. */
    size_t offsets[AE_FILES_NAME+1],count=0,start=0;int pixels=0,mark=0;
    if (!TTF_SetFontSize(a->font,16*a->session.scale*a->density) || !TTF_GetStringSize(a->font,a->session.name,a->session.name_size,&pixels,NULL) ||
        !TTF_GetStringSize(a->font,"…",3,&mark,NULL) || !ae_text_graphemes(a->session.name,a->session.name_size,offsets,AE_FILES_NAME+1,&count)) { fail(a,"name measurement");return; }
    char tail[AE_FILES_NAME+4];
    while ((float)(pixels+(start ? mark : 0))>width*a->density && start+1<count) {
        ++start;if (!TTF_GetStringSize(a->font,a->session.name+offsets[start],a->session.name_size-offsets[start],&pixels,NULL)) { fail(a,"name tail");return; }
    }
    if (start) { memcpy(tail,"…",3);memcpy(tail+3,a->session.name+offsets[start],a->session.name_size-offsets[start]+1); }
    text(a,start ? tail : a->session.name,x,y,width,16,fg);
}

static void draw_menu(struct app *a,int w,int h,unsigned fg) {
    float width=fminf(300*a->session.scale,(float)w-32),rh=34*a->session.scale+16,height=3*rh+8;
    float x=fmaxf(16,fminf(a->session.menu_x,(float)w-width-16)),y=fmaxf(16,fminf(a->session.menu_y,(float)h-height-16));
    a->menu_rect=(SDL_FRect){x,y,width,height};fill(a,a->menu_rect,a->session.contrast ? 0xffffff : 0x6e91c2);
    for (unsigned i=0;i<3;++i) {
        SDL_FRect r={x+2,y+4+(float)i*rh,width-4,rh};a->menu_rows[i]=r;bool enabled=ae_files_session_menu_enabled(&a->session,i);
        fill(a,r,enabled && i==a->session.menu_focus ? 0x29466b : a->session.contrast ? 0 : 0x1b2230);
        const char *label=i==0 ? ae_files_session_tr(&a->session,"Cut","Ausschneiden") : i==1 ? ae_files_session_tr(&a->session,"Paste…","Einfügen…") : ae_files_session_tr(&a->session,"Rename…","Umbenennen…");
        text(a,label,r.x+12,r.y+8,r.w-100*a->session.scale-24,16,enabled ? fg : 0xa5b7d0);
        text(a,i==0 ? "Ctrl+X" : i==1 ? "Ctrl+V" : "F2",r.x+r.w-100*a->session.scale,r.y+8,100*a->session.scale-8,14,enabled ? fg : 0xa5b7d0);
    }
}
static void draw_dialog(struct app *a,int w,int h,unsigned fg) {
    float width=fminf((float)w-32,720),height=154*a->session.scale+80,x=((float)w-width)/2,y=((float)h-height)/2;
    fill(a,(SDL_FRect){x-2,y-2,width+4,height+4},a->session.contrast ? 0xffffff : 0x6e91c2);
    fill(a,(SDL_FRect){x,y,width,height},a->session.contrast ? 0 : 0x1b2230);
    text(a,ae_files_session_unresolved(&a->session) ? ae_files_session_tr(&a->session,"Inspect file operation","Dateiaktion prüfen") : a->session.moving ? ae_files_session_tr(&a->session,"Move","Verschieben") : a->session.renaming ? ae_files_session_tr(&a->session,"Rename","Umbenennen") : a->session.create_kind==AE_FILES_CREATE_DIRECTORY ? ae_files_session_tr(&a->session,"New folder","Neuer Ordner") : ae_files_session_tr(&a->session,"New file","Neue Datei"),x+16,y+12,width-32,20,fg);
    text(a,a->session.renaming ? a->session.original_label : a->session.folder,x+16,y+18+26*a->session.scale,width-32,14,0xa5b7d0);
    if (a->session.renaming) {
        float fy=y+18+40*a->session.scale;
        if (a->session.moving) {
            float half=(width-32)/2;
            text(a,ae_files_session_tr(&a->session,"From:","Von:"),x+16,fy,45*a->session.scale,10,0xa5b7d0);text(a,a->session.from_folder,x+16+45*a->session.scale,fy,half-45*a->session.scale-8,10,0xa5b7d0);
            text(a,ae_files_session_tr(&a->session,"To:","Nach:"),x+16+half,fy,45*a->session.scale,10,0xa5b7d0);text(a,a->session.folder,x+16+half+45*a->session.scale,fy,half-45*a->session.scale,10,0xa5b7d0);
        } else { text(a,ae_files_session_tr(&a->session,"Folder:","Ordner:"),x+16,fy,50*a->session.scale,10,0xa5b7d0);text(a,a->session.folder,x+16+50*a->session.scale,fy,width-32-50*a->session.scale,10,0xa5b7d0); }
    }
    a->name_field=(SDL_FRect){x+16,y+28+46*a->session.scale,width-32,24*a->session.scale+16};
    fill(a,a->name_field,a->session.name_bad ? 0x613538 : a->session.name_focus==0 && !ae_files_session_unresolved(&a->session) && !a->session.rename_stale ? 0x29466b : 0x10141c);
    if (*a->session.name) name_text(a,a->name_field.x+8,a->name_field.y+6,a->name_field.w-16,fg);
    else text(a,a->session.renaming ? ae_files_session_tr(&a->session,"New name","Neuer Name") : ae_files_session_tr(&a->session,"Name","Name"),a->name_field.x+8,a->name_field.y+6,a->name_field.w-16,16,0xa5b7d0);
    float status=a->name_field.y+a->name_field.h+8;
    text(a,*a->session.create_message ? a->session.create_message : ae_files_session_tr(&a->session,"Existing entries are never replaced","Vorhandene Einträge werden nie ersetzt"),x+16,status,width-32,12,a->session.name_bad || ae_files_session_unresolved(&a->session) || a->session.rename_stale ? 0xffbdad : fg);
    text(a,a->session.name_bad ? ae_files_session_tr(&a->session,"Ctrl+U clears the rejected name","Strg+U löscht den abgewiesenen Namen") : ae_files_session_unresolved(&a->session) ? ae_files_session_tr(&a->session,"Further creation/rename blocked in this window","Weiteres Anlegen/Umbenennen hier gesperrt") : a->session.rename_stale ? ae_files_session_tr(&a->session,"Cancel, then refresh the folder","Abbrechen, dann den Ordner aktualisieren") : a->session.moving ? ae_files_session_tr(&a->session,"Enter move · Tab focus · Escape cancel","Enter verschieben · Tab Fokus · Escape zurück") : a->session.renaming ? ae_files_session_tr(&a->session,"Ctrl+U name · Enter rename · Escape cancel","Strg+U Name · Enter OK · Escape zurück") : ae_files_session_tr(&a->session,"Enter create · Tab change focus · Escape cancel","Enter anlegen · Tab Fokus · Escape abbrechen"),x+16,status+16*a->session.scale+4,width-32,12,0xa5b7d0);
    const char *recovery=ae_files_create_recovery(a->session.unknown);
    if (recovery) text(a,recovery,x+16,status+32*a->session.scale+8,width-32,12,0xffbdad);
    float button=(width-40)/2,bh=22*a->session.scale+16,by=y+height-bh-16;
    a->cancel=(SDL_FRect){x+16,by,button,bh};a->submit=(SDL_FRect){x+24+button,by,button,bh};
    fill(a,a->cancel,a->session.name_focus==1 ? 0x29466b : 0x10141c);text(a,ae_files_session_unresolved(&a->session) ? ae_files_session_tr(&a->session,"Back","Zurück") : ae_files_session_tr(&a->session,"Cancel","Abbrechen"),a->cancel.x+10,by+6,button-20,14,fg);
    bool enabled=!ae_files_session_creating(&a->session) && !ae_files_session_unresolved(&a->session) && !a->session.rename_stale && !a->session.name_bad && ae_files_create_name(a->session.name,a->session.name_size) && !a->session.closing && (!a->session.renaming || a->session.moving || strcmp(a->session.name,a->session.original));
    fill(a,a->submit,enabled ? a->session.name_focus==2 ? 0x416b9e : 0x29466b : 0x10141c);text(a,a->session.moving ? ae_files_session_tr(&a->session,"Move","Verschieben") : a->session.renaming ? ae_files_session_tr(&a->session,"Rename","Umbenennen") : ae_files_session_tr(&a->session,"Create","Anlegen"),a->submit.x+10,by+6,button-20,14,enabled ? fg : 0xa5b7d0);
}
struct palette { unsigned canvas,surface,raised,line,text,muted,selection,focus,icon; };
static struct palette colors(const struct app *a) {
    return a->session.contrast ? (struct palette){0,0,0,0xffffff,0xffffff,0xffffff,0x163e64,0xffffff,0xffffff} :
        (struct palette){0x252833,0x222630,0x2c303c,0x454b59,0xe9edf4,0xb3bdce,0x3d485e,0xc6d8ef,0xadbfdc};
}
static SDL_FRect rect(struct ae_files_rect r) { return (SDL_FRect){r.x,r.y,r.w,r.h}; }
static void line(struct app *a,float x,float y,float xx,float yy,unsigned rgb) {
    if (!SDL_SetRenderDrawColor(a->renderer,(Uint8)(rgb>>16),(Uint8)(rgb>>8),(Uint8)rgb,255) || !SDL_RenderLine(a->renderer,x,y,xx,yy)) fail(a,"line");
}
static void outline(struct app *a,SDL_FRect r,unsigned rgb) {
    if (!SDL_SetRenderDrawColor(a->renderer,(Uint8)(rgb>>16),(Uint8)(rgb>>8),(Uint8)rgb,255) || !SDL_RenderRect(a->renderer,&r)) fail(a,"outline");
}
static void rounded(struct app *a,SDL_FRect r,float radius,unsigned rgb) {
    float q=fminf(radius,fminf(r.w,r.h)/2);if (q<1) { fill(a,r,rgb);return; }
    fill(a,(SDL_FRect){r.x+q,r.y,r.w-2*q,r.h},rgb);
    for (int i=0;i<(int)ceilf(q);++i) {
        float dy=q-(float)i-.5f,dx=sqrtf(fmaxf(0,q*q-dy*dy));
        fill(a,(SDL_FRect){r.x+q-dx,r.y+(float)i,r.w-2*q+2*dx,1},rgb);
        fill(a,(SDL_FRect){r.x+q-dx,r.y+r.h-1-(float)i,r.w-2*q+2*dx,1},rgb);
    }
    fill(a,(SDL_FRect){r.x,r.y+q,r.w,r.h-2*q},rgb);
}
/* Original geometric line icons; no external font, bitmap or icon package. */
static void icon(struct app *a,unsigned kind,SDL_FRect r,unsigned rgb) {
    float x=r.x+r.w*.25f,y=r.y+r.h*.25f,w=r.w*.5f,h=r.h*.5f;
    if (kind==0) { /* folder */
        line(a,x,y+h*.2f,x+w*.4f,y+h*.2f,rgb);line(a,x+w*.4f,y+h*.2f,x+w*.55f,y+h*.4f,rgb);
        line(a,x+w*.55f,y+h*.4f,x+w,y+h*.4f,rgb);line(a,x+w,y+h*.4f,x+w,y+h,rgb);
        line(a,x+w,y+h,x,y+h,rgb);line(a,x,y+h,x,y+h*.2f,rgb);
    } else if (kind==1) { /* document */
        line(a,x+w*.15f,y,x+w*.65f,y,rgb);line(a,x+w*.65f,y,x+w*.9f,y+h*.25f,rgb);
        line(a,x+w*.9f,y+h*.25f,x+w*.9f,y+h,rgb);line(a,x+w*.9f,y+h,x+w*.15f,y+h,rgb);
        line(a,x+w*.15f,y+h,x+w*.15f,y,rgb);line(a,x+w*.3f,y+h*.5f,x+w*.7f,y+h*.5f,rgb);
        line(a,x+w*.3f,y+h*.7f,x+w*.7f,y+h*.7f,rgb);
    } else if (kind==2) { for (unsigned i=0;i<3;++i) line(a,x,y+(float)i*h*.45f,x+w,y+(float)i*h*.45f,rgb); }
    else if (kind==3) { for (unsigned i=0;i<4;++i) outline(a,(SDL_FRect){x+(float)(i%2)*w*.6f,y+(float)(i/2)*h*.6f,w*.4f,h*.4f},rgb); }
    else if (kind==4) { line(a,x+w*.6f,y,x+w*.1f,y+h*.5f,rgb);line(a,x+w*.1f,y+h*.5f,x+w*.6f,y+h,rgb); }
    else if (kind==5) { line(a,x,y,x+w,y+h,rgb);line(a,x+w,y,x,y+h,rgb); }
    else if (kind==6) { outline(a,(SDL_FRect){x,y,w,h},rgb);line(a,x+w*.3f,y+h*.7f,x+w*.7f,y+h*.3f,rgb); }
    else if (kind==7) { line(a,x+w*.5f,y,x+w*.5f,y+h,rgb);line(a,x,y+h*.5f,x+w,y+h*.5f,rgb); }
    else if (kind==8) { outline(a,(SDL_FRect){x,y,w,h},rgb);line(a,x+w*.5f,y+h*.4f,x+w*.5f,y+h*.8f,rgb); }
    else if (kind==9) { line(a,x,y+h*.5f,x+w*.35f,y+h,rgb);line(a,x+w*.35f,y+h,x+w,y,rgb); }
}
static void icon_button(struct app *a,SDL_FRect r,unsigned kind,bool selected,struct palette p) {
    rounded(a,r,5*a->session.scale,selected ? p.selection : p.raised);icon(a,kind,r,selected ? p.text : p.muted);
}

static void place(struct app *a,const char *label,unsigned depth,size_t snapshot,float y,struct palette p) {
    size_t n=a->session.place_count;if (n>=AE_FILES_DEPTH+1) return;
    SDL_FRect r={10*a->session.scale,y,a->session.layout.sidebar.w-20*a->session.scale,32*a->session.scale};
    a->places[n]=r;a->session.place_depth[n]=depth;a->session.place_snapshot[n]=snapshot;++a->session.place_count;
    if (snapshot==SIZE_MAX && depth==ae_files_depth(a->session.files)) rounded(a,r,5*a->session.scale,p.selection);
    icon(a,0,(SDL_FRect){r.x+2*a->session.scale,r.y,30*a->session.scale,r.h},p.icon);
    text(a,label,r.x+34*a->session.scale,r.y+8*a->session.scale,r.w-40*a->session.scale,12,p.text);
}
static void draw_places(struct app *a,struct palette p) {
    a->session.place_count=0;if (!a->session.layout.sidebar_visible) return;
    float s=a->session.scale;fill(a,rect(a->session.layout.sidebar),p.surface);
    line(a,a->session.layout.sidebar.w-1,a->session.layout.chrome.h,a->session.layout.sidebar.w-1,a->session.layout.status.y,p.line);
    text(a,ae_files_session_tr(&a->session,"Locations","Orte"),18*s,a->session.layout.chrome.h+18*s,a->session.layout.sidebar.w-36*s,10,p.muted);
    float y=a->session.layout.chrome.h+42*s,limit=a->session.layout.status.y-12*s;unsigned depth=ae_files_depth(a->session.files);
    place(a,a->session.root_label,0,SIZE_MAX,y,p);y+=36*s;
    if (depth) {
        unsigned slots=limit>y ? (unsigned)((limit-y)/(36*s)) : 0;
        unsigned start=depth>=slots && slots ? depth-slots+1 : 1;
        for (unsigned d=start;d<=depth && y+32*s<=limit;++d,y+=36*s) place(a,ae_files_ancestor_label(a->session.files,d),d,SIZE_MAX,y,p);
    } else {
        y+=18*s;
        for (size_t i=0;i<ae_files_total(a->session.files) && y+32*s<=limit;++i) {
            const struct ae_files_entry *e=ae_files_snapshot_entry(a->session.files,i);
            if (e && e->kind==AE_FILES_DIRECTORY && (a->session.show_hidden || e->name[0]!='.')) { place(a,e->label,0,i,y,p);y+=36*s; }
        }
    }
}
static void draw_information(struct app *a,SDL_FRect r,struct palette p) {
    float s=a->session.scale;fill(a,r,p.surface);line(a,r.x,r.y,r.x,r.y+r.h,p.line);
    const struct ae_files_entry *e=ae_files_entry(a->session.files,ae_files_selected(a->session.files));
    text(a,ae_files_session_tr(&a->session,"Information","Informationen"),r.x+14*s,r.y+14*s,r.w-28*s,12,p.text);
    if (ae_files_session_inline_edit(&a->session) && !a->session.renaming) {
        text(a,a->session.create_kind==AE_FILES_CREATE_DIRECTORY ? ae_files_session_tr(&a->session,"New folder","Neuer Ordner") : ae_files_session_tr(&a->session,"New file","Neue Datei"),r.x+14*s,r.y+48*s,r.w-28*s,13,p.text);
        text(a,ae_files_session_creating(&a->session) ? ae_files_session_tr(&a->session,"Waiting for result","Warten auf das Ergebnis") : ae_files_session_tr(&a->session,"Not created yet","Noch nicht angelegt"),r.x+14*s,r.y+78*s,r.w-28*s,11,p.muted);
        text(a,ae_files_session_folder_label(&a->session),r.x+14*s,r.y+110*s,r.w-28*s,11,p.muted);return;
    }
    if (!e) { text(a,ae_files_session_tr(&a->session,"No selection","Keine Auswahl"),r.x+14*s,r.y+48*s,r.w-28*s,12,p.muted);return; }
    SDL_FRect tile={r.x+14*s,r.y+44*s,r.w-28*s,72*s};rounded(a,tile,8*s,p.raised);
    icon(a,e->kind==AE_FILES_DIRECTORY ? 0u : 1u,(SDL_FRect){tile.x+(tile.w-64*s)/2,tile.y+4*s,64*s,64*s},p.icon);
    text(a,e->label,r.x+14*s,r.y+130*s,r.w-28*s,13,p.text);
    text(a,ae_files_session_kind_label(&a->session,e),r.x+14*s,r.y+154*s,r.w-28*s,11,p.muted);
    char bytes[64];snprintf(bytes,sizeof(bytes),"%llu bytes",(unsigned long long)e->size);
    if (e->kind==AE_FILES_REGULAR) text(a,bytes,r.x+14*s,r.y+180*s,r.w-28*s,11,p.muted);
    if (r.h>248*s) { text(a,ae_files_session_tr(&a->session,"Location","Ort"),r.x+14*s,r.y+218*s,r.w-28*s,10,p.muted);text(a,ae_files_session_folder_label(&a->session),r.x+14*s,r.y+238*s,r.w-28*s,11,p.text); }
}
static void draw_inline(struct app *a,SDL_FRect r,struct palette p) {
    float s=a->session.scale;bool card=a->session.grid && !a->session.layout.compact_cards;
    float field_x=r.x+(card ? 8 : 46)*s,field_y=r.y+(card ? 74 : 5)*s;
    float buttons_y=card ? r.y+112*s : a->session.grid ? r.y+r.h-30*s : field_y;
    float button=28*s,right=r.x+r.w-8*s;
    a->submit=(SDL_FRect){right-button,buttons_y,button,28*s};a->cancel=(SDL_FRect){right-2*button-6*s,buttons_y,button,28*s};
    float width=card || a->session.grid ? r.x+r.w-8*s-field_x : a->cancel.x-8*s-field_x;
    a->name_field=(SDL_FRect){field_x,field_y,width,28*s};
    rounded(a,a->name_field,4*s,p.surface);
    outline(a,a->name_field,a->session.name_bad ? 0xffbdad : a->session.name_focus==0 && !ae_files_session_creating(&a->session) ? p.focus : p.line);
    if (*a->session.name) name_text(a,field_x+5*s,field_y+3*s,width-10*s,p.text);
    else text(a,ae_files_session_tr(&a->session,"Name","Name"),field_x+5*s,field_y+5*s,width-10*s,12,p.muted);
    icon_button(a,a->cancel,5,a->session.name_focus==1,p);icon_button(a,a->submit,9,a->session.name_focus==2,p);
    if (!a->session.grid) text(a,ae_files_session_creating(&a->session) ? ae_files_session_tr(&a->session,"Working…","Wird ausgeführt…") : ae_files_session_tr(&a->session,"Enter confirm · Escape cancel","Enter bestätigen · Escape abbrechen"),r.x+46*s,r.y+43*s,r.w-56*s,10,p.muted);
}
static void control_record(struct app *a,const char *id,size_t row,SDL_FRect r) {
    if (r.w>0 && r.h>0) printf("CONTROL frame=%u id=%s row=%zu x=%.2f y=%.2f w=%.2f h=%.2f\n",a->session.frames,id,row,(double)r.x,(double)r.y,(double)r.w,(double)r.h);
}
static void controls(struct app *a) {
    if (!a->session.diagnostics) return;
    if (a->session.dialog) { control_record(a,"name",0,a->name_field);control_record(a,"cancel",0,a->cancel);control_record(a,"submit",0,a->submit);return; }
    if (a->session.info_overlay) { control_record(a,"info_close",0,a->info_close);return; }
    if (a->session.menu) { for (size_t i=0;i<3;++i) control_record(a,"menu",i,a->menu_rows[i]);return; }
    control_record(a,"back",0,a->back);control_record(a,"list",0,a->list_button);control_record(a,"grid",0,a->grid_button);
    control_record(a,"new_file",0,a->new_file);control_record(a,"new_folder",0,a->new_folder);control_record(a,"details",0,a->details_button);
    control_record(a,"more",0,a->more);control_record(a,"hidden",0,a->hidden);control_record(a,"search",0,a->search);
    control_record(a,"close",0,a->close_button);control_record(a,"fullscreen",0,a->fullscreen_button);
    for (size_t i=0;i<a->session.rows && a->session.first+i<ae_files_count(a->session.files);++i) { struct ae_files_rect r;if (ae_files_layout_cell(&a->session.layout,i,&r)) control_record(a,"entry",a->session.first+i,rect(r)); }
    for (size_t i=0;i<a->session.place_count;++i) control_record(a,"place",i,a->places[i]);
}
static void draw(struct app *a) {
    int w,h,pw,ph;if (!SDL_GetWindowSize(a->window,&w,&h) || !SDL_GetWindowSizeInPixels(a->window,&pw,&ph)) { fail(a,"size");return; }
    a->density=SDL_GetWindowPixelDensity(a->window);if (!isfinite(a->density) || a->density<.5f || a->density>4 || !SDL_SetRenderScale(a->renderer,(float)pw/(float)w,(float)ph/(float)h)) { fail(a,"density");return; }
    bool naming=ae_files_session_inline_edit(&a->session);
    if (!ae_files_layout_make(w,h,a->session.scale,a->session.grid,a->session.details,a->session.sidebar,naming,&a->session.layout)) { fail(a,"layout");return; }
    struct palette p=colors(a);float s=a->session.scale;struct ae_files_layout *l=&a->session.layout;
    if (!SDL_SetRenderDrawColor(a->renderer,(Uint8)(p.canvas>>16),(Uint8)(p.canvas>>8),(Uint8)p.canvas,255) || !SDL_RenderClear(a->renderer)) { fail(a,"clear");return; }
    a->session.rows=l->capacity;
    size_t count=ae_files_count(a->session.files),naming_index=a->session.renaming ? ae_files_selected(a->session.files) : count;
    size_t visual_count=count+(naming && !a->session.renaming ? 1u : 0u);
    a->session.first=ae_files_layout_reveal(l,visual_count,naming ? naming_index : ae_files_selected(a->session.files),a->session.first);
    a->back=rect(l->back);a->list_button=rect(l->list);a->grid_button=rect(l->grid);a->new_folder=rect(l->new_folder);a->new_file=rect(l->new_file);
    a->search=rect(l->search);a->details_button=rect(l->details);a->more=rect(l->more);a->close_button=rect(l->close);a->fullscreen_button=rect(l->fullscreen);
    icon(a,5,a->close_button,p.muted);icon(a,6,a->fullscreen_button,p.muted);
    text(a,ae_files_session_tr(&a->session,"Files","Dateien"),76*s,7*s,(float)w-150*s,12,p.text);
    icon_button(a,a->more,2,false,p);draw_places(a,p);fill(a,rect(l->toolbar),p.surface);
    icon_button(a,a->back,4,false,p);icon_button(a,a->list_button,2,!a->session.grid,p);icon_button(a,a->grid_button,3,a->session.grid,p);
    icon_button(a,a->new_folder,0,false,p);icon(a,7,(SDL_FRect){a->new_folder.x+14*s,a->new_folder.y+13*s,18*s,18*s},p.text);
    icon_button(a,a->new_file,1,false,p);icon(a,7,(SDL_FRect){a->new_file.x+14*s,a->new_file.y+13*s,18*s,18*s},p.text);
    rounded(a,a->search,5*s,a->session.query_focus ? p.selection : p.raised);
    if (a->session.query_focus) outline(a,a->search,a->session.query_bad ? 0xffbdad : p.focus);
    text(a,*ae_files_query(a->session.files) ? ae_files_query(a->session.files) : ae_files_session_tr(&a->session,"Search this folder","Ordner durchsuchen"),a->search.x+9*s,a->search.y+8*s,a->search.w-18*s,11,p.muted);
    icon_button(a,a->details_button,8,l->inspector_visible || a->session.info_overlay,p);
    if (!l->compact) {
        icon(a,0,(SDL_FRect){l->heading.x,l->heading.y+12*s,40*s,40*s},p.icon);
        text(a,ae_files_session_folder_label(&a->session),l->heading.x+48*s,l->heading.y+13*s,l->heading.w-48*s,17,p.text);
        char count_label[64];snprintf(count_label,sizeof(count_label),"%zu / %zu",ae_files_count(a->session.files),ae_files_total(a->session.files));
        text(a,count_label,l->heading.x+48*s,l->heading.y+37*s,l->heading.w-48*s,10,p.muted);
        if (!a->session.grid) { text(a,ae_files_session_tr(&a->session,"Name","Name"),l->content.x+10*s,l->content.y-19*s,l->content.w-20*s,10,p.muted);line(a,l->content.x,l->content.y-3*s,l->content.x+l->content.w,l->content.y-3*s,p.line); }
    } else text(a,ae_files_session_folder_label(&a->session),l->heading.x,l->heading.y+7*s,l->heading.w,12,p.text);
    for (size_t i=0;i<a->session.rows && a->session.first+i<visual_count;++i) {
        size_t index=a->session.first+i;const struct ae_files_entry *e=ae_files_entry(a->session.files,index);struct ae_files_rect cell;if (!ae_files_layout_cell(l,i,&cell)) break;
        SDL_FRect r=rect(cell);bool selected=index==(naming ? naming_index : ae_files_selected(a->session.files));
        rounded(a,r,6*s,selected ? p.selection : a->session.grid ? p.raised : p.canvas);
        if (selected) outline(a,r,a->session.query_focus ? p.line : p.focus);
        float ix=r.x+6*s,iy=r.y+(a->session.grid && !l->compact_cards ? 10 : 4)*s,iw=(a->session.grid && !l->compact_cards ? 58 : 36)*s;
        icon(a,(e ? e->kind==AE_FILES_DIRECTORY : a->session.create_kind==AE_FILES_CREATE_DIRECTORY) ? 0u : 1u,(SDL_FRect){ix,iy,iw,iw},p.icon);
        if (naming && index==naming_index) { draw_inline(a,r,p);continue; }
        if (!e) continue;
        if (a->session.grid && !l->compact_cards) {
            text(a,e->label,r.x+12*s,r.y+78*s,r.w-24*s,12,p.text);
            line(a,r.x+12*s,r.y+108*s,r.x+r.w-12*s,r.y+108*s,p.line);
            text(a,ae_files_session_kind_label(&a->session,e),r.x+12*s,r.y+119*s,r.w-24*s,10,p.muted);
        } else {
            text(a,e->label,r.x+48*s,r.y+5*s,r.w-58*s,12,p.text);
            char detail[96];snprintf(detail,sizeof(detail),"%s%s%llu%s",ae_files_session_kind_label(&a->session,e),e->kind==AE_FILES_REGULAR ? " · " : "",e->kind==AE_FILES_REGULAR ? (unsigned long long)e->size : 0,e->kind==AE_FILES_REGULAR ? " bytes" : "");
            text(a,e->kind==AE_FILES_REGULAR ? detail : ae_files_session_kind_label(&a->session,e),r.x+48*s,r.y+24*s,r.w-58*s,10,p.muted);
        }
    }
    if (!visual_count) text(a,*ae_files_query(a->session.files) ? ae_files_session_tr(&a->session,"No matching entries","Keine passenden Einträge") : ae_files_session_tr(&a->session,"This folder is empty","Dieser Ordner ist leer"),l->content.x+8*s,l->content.y+12*s,l->content.w-16*s,12,p.muted);
    if (l->inspector_visible) draw_information(a,rect(l->inspector),p);
    fill(a,rect(l->status),p.surface);line(a,0,l->status.y,(float)w,l->status.y,p.line);
    a->hidden=(SDL_FRect){(float)w-110*s,l->status.y+2*s,104*s,24*s};
    text(a,a->session.show_hidden ? ae_files_session_tr(&a->session,"Hidden: on","Versteckt: an") : ae_files_session_tr(&a->session,"Hidden: off","Versteckt: aus"),a->hidden.x+4*s,a->hidden.y+5*s,a->hidden.w-8*s,10,p.muted);
    const char *status=ae_files_session_unresolved(&a->session) ? ae_files_session_tr(&a->session,"File operation outcome unknown; do not retry","Ergebnis der Dateiaktion unklar; nicht wiederholen") : naming ? *a->session.create_message ? a->session.create_message : ae_files_session_tr(&a->session,"Enter confirm · Tab focus · Escape cancel","Enter bestätigen · Tab Fokus · Escape abbrechen") : *a->session.message ? a->session.message : a->session.clip_dir>=0 ? ae_files_session_tr(&a->session,"Move selected · Ctrl+V preview","Verschieben vorgemerkt · Strg+V Vorschau") : ae_files_session_tr(&a->session,"Ctrl+1/2 view · F9 details · F10 actions","Strg+1/2 Ansicht · F9 Details · F10 Aktionen");
    text(a,status,14*s,l->status.y+7*s,a->hidden.x-28*s,10,p.muted);
    if (a->session.dialog && !naming) draw_dialog(a,w,h,p.text);
    else if (a->session.menu) draw_menu(a,w,h,p.text);
    else if (a->session.info_overlay) {
        float width=fminf(300*s,(float)w-32*s);SDL_FRect r={((float)w-width)/2,8*s,width,(float)h-16*s};
        a->info_rect=r;draw_information(a,r,p);outline(a,r,p.focus);a->info_close=(SDL_FRect){r.x+r.w-32*s,r.y+2*s,30*s,30*s};icon(a,5,a->info_close,p.text);
    }
    if (!SDL_RenderPresent(a->renderer)) fail(a,"present");
    a->session.dirty=false;++a->session.frames;controls(a);
    if (a->session.diagnostics) printf("VIEW frame=%u count=%zu total=%zu selected=%zu top=%zu rows=%zu depth=%u scale=%.2f german=%d contrast=%d filter=%d rejected=%d child=%d width=%d height=%d dialog=%d name_bytes=%zu name_bad=%d name_focus=%u creating=%d unknown=%d renaming=%d stale=%d moving=%d clipboard=%d menu=%d menu_focus=%u grid=%d columns=%zu sidebar=%d inspector=%d info_overlay=%d inline=%d fullscreen=%d\n",a->session.frames,ae_files_count(a->session.files),ae_files_total(a->session.files),ae_files_selected(a->session.files),a->session.first,a->session.rows,ae_files_depth(a->session.files),(double)a->session.scale,a->session.german,a->session.contrast,a->session.query_focus,a->session.query_bad,a->session.child.pid,w,h,a->session.dialog,a->session.name_size,a->session.name_bad,a->session.name_focus,ae_files_session_creating(&a->session),ae_files_session_unresolved(&a->session),a->session.renaming,a->session.rename_stale,a->session.moving,a->session.clip_dir>=0,a->session.menu,a->session.menu_focus,a->session.grid,a->session.layout.columns,a->session.layout.sidebar_visible,a->session.layout.inspector_visible,a->session.info_overlay,ae_files_session_inline_edit(&a->session),(SDL_GetWindowFlags(a->window)&SDL_WINDOW_FULLSCREEN)!=0);
}
static bool inside(SDL_FRect r,float x,float y) { return r.w>0 && x>=r.x && y>=r.y && x<r.x+r.w && y<r.y+r.h; }
static unsigned target(struct app *a,float x,float y,size_t *row) {
    *row=0;
    if (a->session.dialog) { *row=0;if (inside(a->name_field,x,y)) return 7;if (inside(a->cancel,x,y)) return 8;if (inside(a->submit,x,y)) return 9;return 0; }
    if (a->session.info_overlay) return inside(a->info_close,x,y) ? 16u : inside(a->info_rect,x,y) ? 19u : 0u;
    if (a->session.menu) { for (unsigned i=0;i<3;++i) if (inside(a->menu_rows[i],x,y)) return 30+i;return 0; }
    if (inside(a->new_file,x,y)) return 5;
    if (inside(a->new_folder,x,y)) return 6;
    if (inside(a->more,x,y)) return 11;
    if (inside(a->list_button,x,y)) return 12;
    if (inside(a->grid_button,x,y)) return 13;
    if (inside(a->details_button,x,y)) return 14;
    if (inside(a->close_button,x,y)) return 17;
    if (inside(a->fullscreen_button,x,y)) return 18;
    for (size_t i=0;i<a->session.place_count;++i) if (inside(a->places[i],x,y)) { *row=i;return 20; }
    *row=0;if (inside(a->back,x,y)) return 1;if (inside(a->hidden,x,y)) return 2;if (inside(a->search,x,y)) return 3;
    for (size_t i=0;i<a->session.rows && a->session.first+i<ae_files_count(a->session.files);++i) { struct ae_files_rect r;if (ae_files_layout_cell(&a->session.layout,i,&r) && inside(rect(r),x,y)) { *row=a->session.first+i;return 4; } }return 0;
}
static SDL_HitTestResult hit_test(SDL_Window *window,const SDL_Point *p,void *data) {
    struct app *a=data;int w,h;if (!SDL_GetWindowSize(window,&w,&h) || (SDL_GetWindowFlags(window)&(SDL_WINDOW_MAXIMIZED|SDL_WINDOW_FULLSCREEN))) return SDL_HITTEST_NORMAL;
    bool l=p->x<8,r=p->x>=w-8,t=p->y<8,b=p->y>=h-8;
    if (t&&l) return SDL_HITTEST_RESIZE_TOPLEFT;
    if (t&&r) return SDL_HITTEST_RESIZE_TOPRIGHT;
    if (b&&l) return SDL_HITTEST_RESIZE_BOTTOMLEFT;
    if (b&&r) return SDL_HITTEST_RESIZE_BOTTOMRIGHT;
    if (l) return SDL_HITTEST_RESIZE_LEFT;
    if (r) return SDL_HITTEST_RESIZE_RIGHT;
    if (t) return SDL_HITTEST_RESIZE_TOP;
    if (b) return SDL_HITTEST_RESIZE_BOTTOM;
    return (float)p->y<30*a->session.scale && !a->session.dialog && !a->session.menu && !a->session.info_overlay && !inside(a->close_button,(float)p->x,(float)p->y) && !inside(a->fullscreen_button,(float)p->x,(float)p->y) && !inside(a->more,(float)p->x,(float)p->y) ? SDL_HITTEST_DRAGGABLE : SDL_HITTEST_NORMAL;
}
static void toggle_details(struct app *a) {
    if (a->session.details && (a->session.layout.inspector_visible || a->session.info_overlay)) { a->session.details=false;a->session.info_overlay=false; }
    else { int w,h;struct ae_files_layout candidate;
        if (!SDL_GetWindowSize(a->window,&w,&h) || !ae_files_layout_make(w,h,a->session.scale,a->session.grid,true,a->session.sidebar,false,&candidate)) { fail(a,"information layout");return; }
        a->session.details=true;a->session.info_overlay=!candidate.inspector_visible;
    }a->session.pressed=0;a->session.dirty=true;
}
static void fullscreen(struct app *a) {
    if (!SDL_SetWindowFullscreen(a->window,(SDL_GetWindowFlags(a->window)&SDL_WINDOW_FULLSCREEN)==0)) fail(a,"fullscreen");
}

static void dialog_key(struct app *a,SDL_Keycode key,SDL_Keymod mod) {
    if (ae_files_session_creating(&a->session)) return;
    if (key==SDLK_ESCAPE) ae_files_session_dialog_close(&a->session);
    else if (key==SDLK_TAB) {
        a->session.name_focus=ae_files_session_unresolved(&a->session) ? 1 : (a->session.name_focus+((mod&SDL_KMOD_SHIFT) ? 2u : 1u))%3;a->session.dirty=true;
    } else if (key==SDLK_RETURN) { if (a->session.name_focus==1) ae_files_session_dialog_close(&a->session);else ae_files_session_create_submit(&a->session); }
    else if (!ae_files_session_unresolved(&a->session) && !a->session.rename_stale && a->session.name_focus==0 && (mod&SDL_KMOD_CTRL) && key==SDLK_U) {
        a->session.name_bad=false;a->session.composing=false;*a->session.name=0;a->session.name_size=0;*a->session.create_message=0;a->session.dirty=true;
    } else if (!ae_files_session_unresolved(&a->session) && !a->session.rename_stale && a->session.name_focus==0 && !a->session.name_bad && key==SDLK_BACKSPACE && a->session.name_size) {
        size_t offsets[AE_FILES_NAME+1],count;
        if (ae_text_graphemes(a->session.name,a->session.name_size,offsets,AE_FILES_NAME+1,&count)) { a->session.name_size=offsets[count-2];a->session.name[a->session.name_size]=0;*a->session.create_message=0;a->session.dirty=true; }
    }
}
static void menu_key(struct app *a,SDL_Keycode key,SDL_Keymod mod) {
    if (key==SDLK_ESCAPE || key==SDLK_F10) { a->session.menu=false;a->session.dirty=true; }
    else if (key==SDLK_UP || key==SDLK_DOWN || key==SDLK_TAB) {
        unsigned delta=key==SDLK_UP || (key==SDLK_TAB && (mod&SDL_KMOD_SHIFT)) ? 2u : 1u;
        for (unsigned n=0;n<3;++n) { a->session.menu_focus=(a->session.menu_focus+delta)%3;if (ae_files_session_menu_enabled(&a->session,a->session.menu_focus)) break; }a->session.dirty=true;
    } else if (key==SDLK_RETURN || key==SDLK_SPACE) ae_files_session_menu_action(&a->session,a->session.menu_focus);
    else if ((mod&SDL_KMOD_CTRL) && key==SDLK_X) ae_files_session_menu_action(&a->session,0);
    else if ((mod&SDL_KMOD_CTRL) && key==SDLK_V) ae_files_session_menu_action(&a->session,1);
    else if (key==SDLK_F2) ae_files_session_menu_action(&a->session,2);
}
static void event(struct app *a,const SDL_Event *e) {
    if (e->type==SDL_EVENT_QUIT || e->type==SDL_EVENT_WINDOW_CLOSE_REQUESTED) ae_files_session_request_close(&a->session);
    else if (e->type==SDL_EVENT_KEY_DOWN && !e->key.repeat) {
        a->session.pressed=0;
        SDL_Keycode k=e->key.key;SDL_Keymod mod=e->key.mod;
        if ((mod&SDL_KMOD_CTRL) && k==SDLK_Q) ae_files_session_request_close(&a->session);
        else if (k==SDLK_F6) { a->session.german=!a->session.german;a->session.dirty=true; }
        else if (k==SDLK_F7) { a->session.contrast=!a->session.contrast;a->session.dirty=true; }
        else if (k==SDLK_F8) { a->session.scale=a->session.scale==1 ? 1.25f : a->session.scale==1.25f ? 1.5f : a->session.scale==1.5f ? 2 : 1;a->session.dirty=true; }
        else if (a->session.dialog) dialog_key(a,k,mod);
        else if (a->session.info_overlay) { if (k==SDLK_ESCAPE || k==SDLK_F9) { a->session.info_overlay=false;a->session.details=false;a->session.dirty=true; } }
        else if (a->session.menu) menu_key(a,k,mod);
        else if (k==SDLK_F9) toggle_details(a);
        else if (k==SDLK_F4) { a->session.sidebar=!a->session.sidebar;a->session.dirty=true; }
        else if (k==SDLK_F11) fullscreen(a);
        else if ((mod&SDL_KMOD_CTRL) && (k==SDLK_1 || k==SDLK_2)) { a->session.grid=k==SDLK_2;a->session.dirty=true; }
        else if (k==SDLK_F10) ae_files_session_menu_open(&a->session,a->more.x,a->more.y+a->more.h+4,true);
        else if ((mod&SDL_KMOD_CTRL) && k==SDLK_X && !a->session.query_focus) ae_files_session_cut_selected(&a->session);
        else if ((mod&SDL_KMOD_CTRL) && k==SDLK_V && !a->session.query_focus) ae_files_session_paste_open(&a->session);
        else if (k==SDLK_F2) ae_files_session_rename_open(&a->session);
        else if ((mod&SDL_KMOD_CTRL) && k==SDLK_N) ae_files_session_dialog_open(&a->session,(mod&SDL_KMOD_SHIFT) ? AE_FILES_CREATE_DIRECTORY : AE_FILES_CREATE_FILE);
        else if ((mod&SDL_KMOD_CTRL) && k==SDLK_R) ae_files_session_result(&a->session,ae_files_refresh(a->session.files));
        else if ((mod&SDL_KMOD_CTRL) && k==SDLK_H) { a->session.show_hidden=!a->session.show_hidden;ae_files_hidden(a->session.files,a->session.show_hidden);ae_files_session_reveal(&a->session);a->session.dirty=true; }
        else if ((mod&SDL_KMOD_CTRL) && (k==SDLK_L || (k==SDLK_U && a->session.query_focus))) { a->session.query_focus=true;a->session.query_bad=false;(void)ae_files_filter(a->session.files,"",0);a->session.dirty=true; }
        else if ((mod&SDL_KMOD_ALT) && k==SDLK_UP) ae_files_session_result(&a->session,ae_files_up(a->session.files));
        else if ((mod&SDL_KMOD_ALT) && k==SDLK_HOME) ae_files_session_result(&a->session,ae_files_ancestor(a->session.files,0));
        else if (k==SDLK_ESCAPE) { ae_files_session_clipboard_clear(&a->session);a->session.query_focus=false;a->session.query_bad=false;(void)ae_files_filter(a->session.files,"",0);a->session.first=0;a->session.dirty=true; }
        else if (k==SDLK_UP || k==SDLK_DOWN) ae_files_session_move(&a->session,(a->session.grid ? (int)a->session.layout.columns : 1)*(k==SDLK_UP ? -1 : 1));
        else if (a->session.grid && (k==SDLK_LEFT || k==SDLK_RIGHT)) ae_files_session_move(&a->session,k==SDLK_LEFT ? -1 : 1);
        else if (k==SDLK_PAGEUP || k==SDLK_PAGEDOWN) ae_files_session_move(&a->session,(int)(a->session.rows ? a->session.rows : 1)*(k==SDLK_PAGEUP ? -1 : 1));
        else if (k==SDLK_HOME || k==SDLK_END) { if (ae_files_count(a->session.files)) (void)ae_files_select(a->session.files,k==SDLK_HOME ? 0 : ae_files_count(a->session.files)-1);ae_files_session_reveal(&a->session);a->session.dirty=true; }
        else if (k==SDLK_RETURN) { a->session.query_focus=false;if (a->session.rows) ae_files_session_activate(&a->session); }
        else if (k==SDLK_BACKSPACE) { if (a->session.query_focus) { const char *q=ae_files_query(a->session.files);size_t n=strlen(q),points[AE_FILES_QUERY+1],count;if (!a->session.query_bad && n && ae_text_graphemes(q,n,points,AE_FILES_QUERY+1,&count)) (void)ae_files_filter(a->session.files,q,points[count-2]);a->session.dirty=true; }else ae_files_session_result(&a->session,ae_files_up(a->session.files)); }
    } else if (e->type==SDL_EVENT_TEXT_INPUT && a->session.dialog) { a->session.pressed=0;a->session.composing=false;ae_files_session_name_input(&a->session,e->text.text); }
    else if (e->type==SDL_EVENT_TEXT_EDITING && a->session.dialog) { a->session.pressed=0;a->session.composing=e->edit.text && *e->edit.text;a->session.dirty=true; }
    else if (e->type==SDL_EVENT_TEXT_INPUT && !a->session.menu && !a->session.info_overlay && a->session.query_focus) {
        a->session.pressed=0;if (a->session.query_bad) return;
        char q[AE_FILES_QUERY+1];size_t old=strlen(ae_files_query(a->session.files)),n=strlen(e->text.text);
        if (n<=AE_FILES_QUERY-old) { memcpy(q,ae_files_query(a->session.files),old);memcpy(q+old,e->text.text,n);if (ae_files_filter(a->session.files,q,old+n)) { a->session.first=0;a->session.dirty=true; }else { a->session.query_bad=true;ae_files_session_message(&a->session,ae_files_session_tr(&a->session,"Filter rejected; Ctrl+L clears","Filter abgewiesen; Strg+L löscht")); } }else { a->session.query_bad=true;ae_files_session_message(&a->session,ae_files_session_tr(&a->session,"Filter rejected; Ctrl+L clears","Filter abgewiesen; Strg+L löscht")); }
    } else if (e->type==SDL_EVENT_MOUSE_WHEEL) { a->session.pressed=0;if (!a->session.dialog && !a->session.menu && !a->session.info_overlay) ae_files_session_move(&a->session,(a->session.grid ? (int)a->session.layout.columns : 1)*(e->wheel.y>0 ? -3 : e->wheel.y<0 ? 3 : 0)); }
    else if (e->type==SDL_EVENT_MOUSE_BUTTON_DOWN && e->button.button==SDL_BUTTON_RIGHT) {
        a->session.pressed=0;
        if (!a->session.dialog && !a->session.info_overlay && !ae_files_session_creating(&a->session) && !a->session.closing) {
            if (a->session.menu) { a->session.menu=false;a->session.dirty=true; }
            else { size_t row;unsigned hit=target(a,e->button.x,e->button.y,&row);bool selected=hit==4 && ae_files_select(a->session.files,row);ae_files_session_menu_open(&a->session,e->button.x,e->button.y,selected); }
        }
    }
    else if (e->type==SDL_EVENT_MOUSE_BUTTON_DOWN && e->button.button==SDL_BUTTON_LEFT) {
        a->session.consume_release=false;
        a->session.pressed=target(a,e->button.x,e->button.y,&a->session.pressed_row);
        if (ae_files_session_inline_edit(&a->session) && !a->session.pressed) { ae_files_session_create_submit(&a->session);a->session.consume_release=true; }
    }
    else if (e->type==SDL_EVENT_MOUSE_BUTTON_UP && e->button.button==SDL_BUTTON_LEFT) {
        if (a->session.consume_release) { a->session.consume_release=false;a->session.pressed=0;return; }
        size_t row;unsigned hit=target(a,e->button.x,e->button.y,&row),pressed=a->session.pressed;a->session.pressed=0;if (hit!=pressed) return;
        if (a->session.dialog) {
            if (!ae_files_session_creating(&a->session)) { if (hit==7 && !ae_files_session_unresolved(&a->session)) { a->session.name_focus=0;a->session.dirty=true; }else if (hit==8) ae_files_session_dialog_close(&a->session);else if (hit==9) { a->session.name_focus=2;ae_files_session_create_submit(&a->session);a->session.dirty=true; } }
        } else if (a->session.info_overlay) { if (hit==0 || hit==16) { a->session.info_overlay=false;a->session.details=false;a->session.dirty=true; } }
        else if (a->session.menu) { if (hit>=30 && hit<33) ae_files_session_menu_action(&a->session,hit-30);else { a->session.menu=false;a->session.dirty=true; } }
        else if (hit==1) ae_files_session_result(&a->session,ae_files_up(a->session.files));
        else if (hit==2) { a->session.show_hidden=!a->session.show_hidden;ae_files_hidden(a->session.files,a->session.show_hidden);ae_files_session_reveal(&a->session);a->session.dirty=true; }
        else if (hit==3) { a->session.query_focus=true;a->session.dirty=true; }
        else if (hit==4 && row==a->session.pressed_row && ae_files_select(a->session.files,row)) { a->session.query_focus=false;a->session.dirty=true;if (e->button.clicks==2) ae_files_session_activate(&a->session); }
        else if (hit==5 || hit==6) ae_files_session_dialog_open(&a->session,hit==5 ? AE_FILES_CREATE_FILE : AE_FILES_CREATE_DIRECTORY);
        else if (hit==11) ae_files_session_menu_open(&a->session,a->more.x,a->more.y+a->more.h+4,true);
        else if (hit==12 || hit==13) { a->session.grid=hit==13;a->session.dirty=true; }
        else if (hit==14) toggle_details(a);
        else if (hit==17) ae_files_session_request_close(&a->session);
        else if (hit==18) fullscreen(a);
        else if (hit==20 && row==a->session.pressed_row) ae_files_session_navigate_place(&a->session,row);
    } else if (e->type==SDL_EVENT_WINDOW_FOCUS_LOST) { a->session.pressed=0;a->session.consume_release=false;if (a->session.menu) { a->session.menu=false;a->session.dirty=true; }
        if (a->session.dialog && a->session.composing) { a->session.composing=false;a->session.name_bad=true;snprintf(a->session.create_message,sizeof(a->session.create_message),"%s",ae_files_session_tr(&a->session,"Composition interrupted; Ctrl+U clears the name","Texteingabe unterbrochen; Strg+U löscht den Namen"));a->session.dirty=true; }
    }
    else if (e->type==SDL_EVENT_WINDOW_EXPOSED || e->type==SDL_EVENT_WINDOW_RESIZED || e->type==SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) { a->session.pressed=0;a->session.menu=false;a->session.dirty=true; }
}
int main(int argc,char **argv) {
    struct app a={.wake={-1,-1}};const char *root=NULL,*font=NULL;
    ae_files_session_init(&a.session,start_worker,join_worker,&a);
    for (int i=1;i<argc;++i) {
        const char **slot=!strcmp(argv[i],"--root") ? &root : !strcmp(argv[i],"--font") ? &font : !strcmp(argv[i],"--cache") ? &a.session.cache : !strcmp(argv[i],"--terminal") ? &a.session.terminal : !strcmp(argv[i],"--editor") ? &a.session.editor : !strcmp(argv[i],"--terminal-font") ? &a.session.terminal_font : NULL;
        if (slot && !*slot && i+1<argc) *slot=argv[++i];else if (!strcmp(argv[i],"--diagnostics") && !a.session.diagnostics) a.session.diagnostics=true;else return 2;
    }
    struct stat st;if (!root || !font || !a.session.cache || !a.session.terminal || !a.session.editor || !a.session.terminal_font || root[0]!='/' || getuid()==0 || getuid()!=geteuid() || getgid()!=getegid() || lstat(a.session.cache,&st) || !S_ISDIR(st.st_mode) || st.st_uid!=getuid() || (st.st_mode&07777)!=0700) return 2;
    setvbuf(stdout,NULL,_IOLBF,0);int fd=open(root,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);enum ae_files_result r;a.session.files=fd<0 ? NULL : ae_files_create(fd,&r);if (fd>=0) close(fd);if (!a.session.files) return 1;
    const char *end=root+strlen(root);while (end>root+1 && end[-1]=='/') --end;const char *leaf=end;while (leaf>root && leaf[-1]!='/') --leaf;
    if (leaf==end) snprintf(a.session.root_label,sizeof(a.session.root_label),"%s",ae_files_session_tr(&a.session,"Filesystem","Dateisystem"));
    else if (!ae_files_format_name(leaf,(size_t)(end-leaf),a.session.root_label)) { ae_files_destroy(a.session.files);return 2; }
    struct sigaction child={.sa_handler=SIG_DFL};sigemptyset(&child.sa_mask);
    if (sigaction(SIGCHLD,&child,NULL) || setenv("MESA_SHADER_CACHE_DIR",a.session.cache,1) || setenv("MESA_SHADER_CACHE_MAX_SIZE","32M",1) || !SDL_SetAppMetadata("Aetheri Files","0.0.1","org.aetheri.Files") || !SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"wayland") || !SDL_Init(SDL_INIT_VIDEO) || !TTF_Init()) { a.session.failed=true;goto done; }
    int fm,fn,fp,hm,hn,hp;TTF_GetFreeTypeVersion(&fm,&fn,&fp);TTF_GetHarfBuzzVersion(&hm,&hn,&hp);
    if (SDL_GetVersion()!=SDL_VERSION || TTF_Version()!=SDL_TTF_VERSION || fm!=2 || fn!=15 || fp!=0 || hm!=15 || hn!=0 || hp!=0) { a.session.failed=true;fputs("files: unreviewed SDL/text version\n",stderr);goto done; }
    a.window=SDL_CreateWindow("Aetheri Files",1000,680,SDL_WINDOW_RESIZABLE|SDL_WINDOW_HIGH_PIXEL_DENSITY);a.font=TTF_OpenFont(font,16);if (a.window) a.renderer=SDL_CreateRenderer(a.window,"opengles2");
    if (!a.window || !a.font || !a.renderer || !SDL_SetWindowMinimumSize(a.window,640,420) || !SDL_SetWindowHitTest(a.window,hit_test,&a) || !SDL_StartTextInput(a.window) || pipe2(a.wake,O_CLOEXEC|O_NONBLOCK)) { a.session.failed=true;goto done; }
    a.event=SDL_RegisterEvents(1);atomic_init(&a.active,true);atomic_init(&a.external_changed,false);atomic_init(&a.notification_failed,false);a.notifications=ae_files_notification_fd(a.session.files);signal_writer=a.wake[1];struct sigaction action={.sa_handler=signalled};sigemptyset(&action.sa_mask);
    if (!a.event || sigaction(SIGINT,&action,NULL) || sigaction(SIGTERM,&action,NULL) || !(a.worker=SDL_CreateThread(wake_worker,"files-signal",&a))) { a.session.failed=true;goto done; }
    if (a.session.diagnostics) puts("READY files wayland opengles2");
    while (a.session.running) {
        ae_files_session_create_update(&a.session);ae_files_session_child_update(&a.session);if (stopping) { stopping=0;ae_files_session_request_close(&a.session); }if (!a.session.running) break;
        if (atomic_load(&a.notification_failed) && !a.session.failed) fail(&a,"folder notifications");
        ae_files_session_directory_update(&a.session,atomic_exchange(&a.external_changed,false));
        if (a.session.dirty && !a.session.failed) draw(&a);
        if (!a.session.running) break;
        SDL_Event e;bool timed=ae_files_session_busy(&a.session) || ae_files_session_creating(&a.session);bool got=timed ? SDL_WaitEventTimeout(&e,200) : SDL_WaitEvent(&e);if (got) event(&a,&e);else if (!timed) fail(&a,"event wait");
    }
done:
    ae_files_session_join_worker(&a.session);
    ae_files_create_close(a.session.creation);ae_files_create_close(a.session.unknown);if (a.session.create_dir>=0) close(a.session.create_dir);
    ae_files_rename_close(a.session.rename_op);ae_files_rename_close(a.session.rename_unknown);if (a.session.rename_witness>=0) close(a.session.rename_witness);
    if (a.session.move_target>=0) close(a.session.move_target);
    ae_files_session_clipboard_clear(&a.session);
    if (a.worker) { atomic_store(&a.active,false);unsigned char b=1;(void)write(a.wake[1],&b,1);SDL_WaitThread(a.worker,NULL); }
    signal_writer=-1;for (unsigned i=0;i<2;++i) if (a.wake[i]>=0) close(a.wake[i]);
    if (a.session.child.pid>0) { if (!a.session.child.ended) { fputs("files: refusing to retire a live document child\n",stderr);return 125; }(void)ae_files_child_close(&a.session.child); }
    SDL_DestroyRenderer(a.renderer);SDL_DestroyWindow(a.window);TTF_CloseFont(a.font);TTF_Quit();SDL_Quit();ae_files_destroy(a.session.files);
    if (a.session.diagnostics) printf("CLOSED files frames=%u failed=%d\n",a.session.frames,a.session.failed);
    return a.session.failed ? 1 : 0;
}
