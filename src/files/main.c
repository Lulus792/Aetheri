/* Local development view; normal app/document grants remain separate. */
#define _GNU_SOURCE
#include "model.h"
#include "launch.h"
#include "create.h"
#include "rename.h"
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
    SDL_FRect new_file,new_folder,name_field,cancel,submit;
    bool dialog,name_bad;unsigned name_focus,creations;int create_dir;
    enum ae_files_create_kind create_kind;char name[AE_FILES_NAME+1],folder[AE_FILES_LABEL+1],create_message[256];size_t name_size;
    struct ae_files_create *creation,*unknown;SDL_Thread *creator;atomic_bool create_done;
    bool renaming,rename_stale;unsigned renames;int rename_witness;SDL_FRect rename_button;
    char original[AE_FILES_NAME+1],original_label[AE_FILES_LABEL+1];
    struct ae_files_rename *rename_op,*rename_unknown;
    bool moving,menu,menu_selection;unsigned moves,menu_focus;int move_target,clip_dir,clip_witness;
    char clip_name[AE_FILES_NAME+1],clip_label[AE_FILES_LABEL+1],clip_folder[AE_FILES_LABEL+1],from_folder[AE_FILES_LABEL+1];
    SDL_FRect more,menu_rect,menu_rows[3];float menu_x,menu_y;
};
static volatile sig_atomic_t signal_writer=-1,stopping;
static void signalled(int number) { stopping=number;int saved=errno;unsigned char b=1;if (signal_writer>=0) (void)write(signal_writer,&b,1);errno=saved; }
static int wake_worker(void *data) {
    struct app *a=data;while (atomic_load(&a->active)) { struct pollfd p={a->wake[0],POLLIN,0};int n=poll(&p,1,-1);if (n<0 && errno==EINTR) continue;if (n<0) break;
        unsigned char bytes[64];while (read(a->wake[0],bytes,sizeof(bytes))>0) {}SDL_Event e={.type=a->event};(void)SDL_PushEvent(&e); }return 0;
}
static bool busy(const struct app *a) { return a->child.pid>0 && !a->child.ended; }
static bool creating(const struct app *a) { return a->creator!=NULL; }
static bool unresolved(const struct app *a) { return a->unknown!=NULL || a->rename_unknown!=NULL; }
static const char *tr(const struct app *a,const char *en,const char *de) { return a->german ? de : en; }
static void message(struct app *a,const char *s) { snprintf(a->message,sizeof(a->message),"%s",s);a->dirty=true; }
static void fail(struct app *a,const char *s) { fprintf(stderr,"files: %s: %s\n",s,SDL_GetError());a->failed=true;a->closing=true;if (!busy(a) && !creating(a)) a->running=false; }
static void result(struct app *a,enum ae_files_result r) {
    const char *s=r==AE_FILES_OK ? "" : r==AE_FILES_ROOT ? tr(a,"At the selected root","Am gewählten Ausgangsordner") : r==AE_FILES_CHANGED ? tr(a,"Folder or file changed; Ctrl+R refreshes","Ordner oder Datei geändert; Strg+R aktualisiert") : r==AE_FILES_LIMIT ? tr(a,"Folder/depth limit; view retained","Ordner-/Tiefenlimit; Ansicht bleibt erhalten") : r==AE_FILES_NOMEM ? tr(a,"Not enough memory; view retained","Nicht genug Speicher; Ansicht bleibt erhalten") : r==AE_FILES_UNSUPPORTED ? tr(a,"This entry cannot be opened here","Dieser Eintrag lässt sich hier nicht öffnen") : tr(a,"Cannot read this folder","Ordner kann nicht gelesen werden");
    message(a,s);if (r==AE_FILES_OK) { a->first=0;if (!*ae_files_query(a->files)) a->query_bad=false; }
}
static void reveal(struct app *a) { size_t selected=ae_files_selected(a->files);if (selected<a->first) a->first=selected;else if (a->rows && selected>=a->first+a->rows) a->first=selected-a->rows+1; }
static void move(struct app *a,int amount) {
    size_t count=ae_files_count(a->files),old=ae_files_selected(a->files);if (!count) return;
    size_t n=amount<0 ? (size_t)(-amount)>old ? 0 : old-(size_t)(-amount) : (size_t)amount>=count-old ? count-1 : old+(size_t)amount;
    (void)ae_files_select(a->files,n);a->query_focus=false;reveal(a);a->dirty=true;
}
static void clipboard_clear(struct app *a) {
    if (a->clip_dir>=0) close(a->clip_dir);
    if (a->clip_witness>=0) close(a->clip_witness);
    a->clip_dir=-1;a->clip_witness=-1;a->dirty=true;
}
static void dialog_close(struct app *a) {
    if (creating(a)) return;
    if (a->create_dir>=0) close(a->create_dir);
    if (a->rename_witness>=0) close(a->rename_witness);
    if (a->move_target>=0) close(a->move_target);
    a->move_target=-1;
    a->rename_witness=-1;
    a->create_dir=-1;a->dialog=false;a->pressed=0;a->dirty=true;
}
static void request_close(struct app *a) {
    a->closing=true;if (creating(a)) message(a,tr(a,"Waiting for the file operation result","Warten auf das Ergebnis der Dateiaktion"));
    else { dialog_close(a);if (busy(a)) message(a,tr(a,"Close the document window first; Files is waiting","Zuerst das Dokumentfenster schließen; Dateien wartet"));else a->running=false; }
}
static void dialog_open(struct app *a,enum ae_files_create_kind kind) {
    if (a->closing || creating(a)) return;
    a->pressed=0;a->query_focus=false;a->name_focus=0;a->name_bad=false;*a->create_message=0;
    if (unresolved(a)) {
        a->renaming=a->rename_unknown!=NULL;a->dialog=true;a->name_focus=1;snprintf(a->name,sizeof(a->name),"%s",a->renaming ? ae_files_rename_new(a->rename_unknown) : ae_files_create_leaf(a->unknown));a->name_size=strlen(a->name);
        snprintf(a->create_message,sizeof(a->create_message),"%s",tr(a,"Outcome unknown. Inspect; do not retry.","Ergebnis unklar. Prüfen; nicht wiederholen."));
    } else {
        if (a->creations>=256) { message(a,tr(a,"Creation limit reached; reopen Files","Limit für das Anlegen erreicht; Dateien neu öffnen"));return; }
        int fd=ae_files_directory(a->files);if (fd<0) { message(a,tr(a,"Cannot retain the target folder","Zielordner kann nicht festgehalten werden"));return; }
        if (a->create_dir>=0) close(a->create_dir);
        a->renaming=false;a->moving=false;a->rename_stale=false;a->create_dir=fd;a->create_kind=kind;a->name_size=0;*a->name=0;
        snprintf(a->folder,sizeof(a->folder),"%s",ae_files_folder(a->files));a->dialog=true;
    }
    a->dirty=true;
}
static void rename_open(struct app *a) {
    if (a->closing || creating(a)) return;
    if (unresolved(a)) { dialog_open(a,AE_FILES_CREATE_FILE);return; }
    if (a->child.pid>0) { message(a,tr(a,"Close the document before renaming","Dokument vor dem Umbenennen schließen"));return; }
    if (a->query_bad || !a->rows) return;
    if (a->renames>=256) { message(a,tr(a,"Rename limit reached; reopen Files","Umbenennen-Limit erreicht; Dateien neu öffnen"));return; }
    const struct ae_files_entry *e=ae_files_entry(a->files,ae_files_selected(a->files));if (!e) return;
    enum ae_files_result r=ae_files_hold_selected(a->files,&a->create_dir,&a->rename_witness,a->original);
    if (r!=AE_FILES_OK) { result(a,r);return; }
    snprintf(a->original_label,sizeof(a->original_label),"%s",e->label);snprintf(a->folder,sizeof(a->folder),"%s",ae_files_folder(a->files));
    a->name_size=ae_files_create_name(a->original,strlen(a->original)) ? strlen(a->original) : 0;
    memcpy(a->name,a->original,a->name_size);a->name[a->name_size]=0;
    a->renaming=true;a->moving=false;a->rename_stale=false;a->dialog=true;a->name_bad=false;a->name_focus=0;a->query_focus=false;a->pressed=0;*a->create_message=0;a->dirty=true;
}
static void cut_selected(struct app *a) {
    if (a->closing || creating(a) || unresolved(a) || a->query_bad || !a->rows || a->child.pid>0) {
        message(a,a->child.pid>0 ? tr(a,"Close the document before selecting a move","Dokument vor der Verschiebeauswahl schließen") : tr(a,"Move selection is unavailable here","Verschiebeauswahl hier nicht verfügbar"));return;
    }
    const struct ae_files_entry *e=ae_files_entry(a->files,ae_files_selected(a->files));if (!e) return;
    int dir=-1,witness=-1;char leaf[AE_FILES_NAME+1];enum ae_files_result r=ae_files_hold_selected(a->files,&dir,&witness,leaf);
    if (r!=AE_FILES_OK) { result(a,r);return; }
    clipboard_clear(a);a->clip_dir=dir;a->clip_witness=witness;
    memcpy(a->clip_name,leaf,strlen(leaf)+1);snprintf(a->clip_label,sizeof(a->clip_label),"%s",e->label);snprintf(a->clip_folder,sizeof(a->clip_folder),"%s",ae_files_folder(a->files));
    a->query_focus=false;message(a,tr(a,"Move selected; navigate to the target and press Ctrl+V","Verschieben vorgemerkt; Ziel öffnen und Strg+V drücken"));
}
static void paste_open(struct app *a) {
    if (a->closing || creating(a)) return;
    if (unresolved(a)) { dialog_open(a,AE_FILES_CREATE_FILE);return; }
    if (a->child.pid>0) { message(a,tr(a,"Close the document before moving","Dokument vor dem Verschieben schließen"));return; }
    if (a->clip_dir<0 || a->clip_witness<0) { message(a,tr(a,"Select a file or folder with Ctrl+X first","Zuerst Datei oder Ordner mit Strg+X auswählen"));return; }
    if (a->moves>=256) { message(a,tr(a,"Move limit reached; reopen Files","Verschieben-Limit erreicht; Dateien neu öffnen"));return; }
    int dir=fcntl(a->clip_dir,F_DUPFD_CLOEXEC,4),witness=fcntl(a->clip_witness,F_DUPFD_CLOEXEC,4),target=ae_files_directory(a->files);
    if (dir<0 || witness<0 || target<0) { if (dir>=0) close(dir);if (witness>=0) close(witness);if (target>=0) close(target);message(a,tr(a,"Cannot retain the move inputs","Verschiebeauswahl kann nicht festgehalten werden"));return; }
    a->create_dir=dir;a->rename_witness=witness;a->move_target=target;a->moving=true;a->renaming=true;a->rename_stale=false;
    memcpy(a->original,a->clip_name,strlen(a->clip_name)+1);snprintf(a->original_label,sizeof(a->original_label),"%s",a->clip_label);
    snprintf(a->from_folder,sizeof(a->from_folder),"%s",a->clip_folder);snprintf(a->folder,sizeof(a->folder),"%s",ae_files_folder(a->files));
    a->name_size=ae_files_create_name(a->original,strlen(a->original)) ? strlen(a->original) : 0;memcpy(a->name,a->original,a->name_size);a->name[a->name_size]=0;
    a->dialog=true;a->name_bad=false;a->name_focus=0;a->query_focus=false;a->pressed=0;*a->create_message=0;a->dirty=true;
}
static int create_worker(void *data) {
    struct app *a=data;if (a->renaming) (void)ae_files_rename_apply(a->rename_op);else (void)ae_files_create_apply(a->creation);
    atomic_store_explicit(&a->create_done,true,memory_order_release);
    SDL_Event e={.type=a->event};(void)SDL_PushEvent(&e);return 0;
}
static void rename_submit(struct app *a) {
    if (a->closing || creating(a) || unresolved(a) || a->rename_stale || a->name_bad || !ae_files_create_name(a->name,a->name_size) || (!a->moving && !strcmp(a->original,a->name))) return;
    enum ae_files_rename_result r;a->rename_op=a->moving ? ae_files_move_prepare(a->create_dir,a->rename_witness,a->original,strlen(a->original),a->move_target,a->name,a->name_size,&r) : ae_files_rename_prepare(a->create_dir,a->rename_witness,a->original,strlen(a->original),a->name,a->name_size,&r);
    if (!a->rename_op) {
        if (r==AE_FILES_RENAME_CHANGED || r==AE_FILES_RENAME_OVERLAP || r==AE_FILES_RENAME_LIMIT) a->rename_stale=true;
        if (r==AE_FILES_RENAME_CHANGED && a->moving) clipboard_clear(a);
        snprintf(a->create_message,sizeof(a->create_message),"%s",r==AE_FILES_RENAME_CHANGED ? tr(a,"Source changed. Cancel and refresh.","Quelle geändert. Abbrechen und aktualisieren.") : r==AE_FILES_RENAME_OVERLAP ? tr(a,"Cannot move a folder into itself or a child","Ordner nicht in sich oder einen Unterordner verschieben") : r==AE_FILES_RENAME_LIMIT ? tr(a,"Target ancestry limit; choose another folder","Ziel-Tiefenlimit; anderen Ordner wählen") : r==AE_FILES_RENAME_DENIED ? tr(a,"Private writable folders and owned entry required","Private Ordner und eigener Eintrag nötig") : tr(a,"Operation not prepared; nothing changed","Aktion nicht vorbereitet; nichts geändert"));a->dirty=true;return;
    }
    atomic_store(&a->create_done,false);a->creator=SDL_CreateThread(create_worker,"files-rename",a);
    if (!a->creator) { ae_files_rename_close(a->rename_op);a->rename_op=NULL;snprintf(a->create_message,sizeof(a->create_message),"%s",tr(a,"Worker unavailable; nothing changed","Worker nicht verfügbar; nichts geändert")); }
    else { if (a->moving) ++a->moves;else ++a->renames;snprintf(a->create_message,sizeof(a->create_message),"%s",a->moving ? tr(a,"Moving… Please wait","Wird verschoben… Bitte warten") : tr(a,"Renaming… Please wait","Wird umbenannt… Bitte warten")); }
    a->dirty=true;
}
static void create_submit(struct app *a) {
    if (a->renaming) { rename_submit(a);return; }
    if (a->closing || creating(a) || unresolved(a) || a->name_bad || !ae_files_create_name(a->name,a->name_size)) return;
    enum ae_files_create_result r;a->creation=ae_files_create_prepare(a->create_dir,a->create_kind,a->name,a->name_size,&r);
    if (!a->creation) {
        snprintf(a->create_message,sizeof(a->create_message),"%s",r==AE_FILES_CREATE_DENIED ? tr(a,"A private writable folder is required","Ein privater beschreibbarer Ordner ist nötig") : r==AE_FILES_CREATE_NOMEM ? tr(a,"Not enough memory; nothing created","Nicht genug Speicher; nichts angelegt") : tr(a,"Cannot prepare creation; nothing created","Anlegen nicht vorbereitet; nichts angelegt"));a->dirty=true;return;
    }
    atomic_store(&a->create_done,false);a->creator=SDL_CreateThread(create_worker,"files-create",a);
    if (!a->creator) { ae_files_create_close(a->creation);a->creation=NULL;snprintf(a->create_message,sizeof(a->create_message),"%s",tr(a,"Worker unavailable; nothing created","Worker nicht verfügbar; nichts angelegt")); }
    else { ++a->creations;snprintf(a->create_message,sizeof(a->create_message),"%s",tr(a,"Creating… Please wait","Wird angelegt… Bitte warten")); }
    a->dirty=true;
}
static void rename_update(struct app *a) {
    enum ae_files_rename_result r=ae_files_rename_status(a->rename_op);
    if (a->diagnostics) printf("%s attempt=%u result=%u\n",a->moving ? "MOVE" : "RENAME",a->moving ? a->moves : a->renames,(unsigned)r);
    if (r==AE_FILES_RENAME_DONE) {
        dialog_close(a);a->query_bad=false;(void)ae_files_filter(a->files,"",0);
        if (a->name[0]=='.') { a->show_hidden=true;ae_files_hidden(a->files,true); }
        if (ae_files_refresh(a->files)==AE_FILES_OK) {
            for (size_t i=0;i<ae_files_count(a->files);++i) if (!strcmp(ae_files_entry(a->files,i)->name,a->name)) { (void)ae_files_select(a->files,i);break; }
            reveal(a);message(a,a->moving ? tr(a,"Moved","Verschoben") : tr(a,"Renamed","Umbenannt"));
        } else message(a,tr(a,"Completed; refresh failed. Do not repeat.","Abgeschlossen; Ansicht nicht aktualisiert. Nicht wiederholen."));
        if (a->moving) clipboard_clear(a);
    } else if (r==AE_FILES_RENAME_UNKNOWN) {
        a->rename_unknown=a->rename_op;a->rename_op=NULL;a->name_focus=1;
        snprintf(a->create_message,sizeof(a->create_message),"%s",tr(a,"Outcome unknown. Inspect both names.","Ergebnis unklar. Beide Namen prüfen."));
        message(a,a->moving ? tr(a,"Move outcome unknown; new mutations blocked","Verschieben unklar; neue Dateiänderungen gesperrt") : tr(a,"Rename outcome unknown; creation and renaming blocked","Umbenennen unklar; Anlegen und Umbenennen gesperrt"));
    } else {
        if (r==AE_FILES_RENAME_CHANGED || r==AE_FILES_RENAME_OVERLAP || r==AE_FILES_RENAME_LIMIT) a->rename_stale=true;
        if (r==AE_FILES_RENAME_CHANGED && a->moving) clipboard_clear(a);
        snprintf(a->create_message,sizeof(a->create_message),"%s",r==AE_FILES_RENAME_CONFLICT ? tr(a,"Name already exists. Choose another name.","Name bereits vorhanden. Anderen Namen wählen.") : r==AE_FILES_RENAME_CHANGED ? tr(a,"Source changed. Cancel and refresh.","Quelle geändert. Abbrechen und aktualisieren.") : tr(a,"Not renamed. Inspect the source.","Nicht umbenannt. Quelle prüfen."));
    }
    ae_files_rename_close(a->rename_op);a->rename_op=NULL;a->dirty=true;if (a->closing) request_close(a);
}
static void create_update(struct app *a) {
    if (!creating(a) || !atomic_load_explicit(&a->create_done,memory_order_acquire)) return;
    SDL_WaitThread(a->creator,NULL);a->creator=NULL;
    if (a->renaming) { rename_update(a);return; }
    enum ae_files_create_result r=ae_files_create_status(a->creation);
    if (a->diagnostics) printf("CREATE attempt=%u result=%u kind=%u\n",a->creations,(unsigned)r,(unsigned)a->create_kind);
    if (r==AE_FILES_CREATE_DONE) {
        dialog_close(a);a->query_bad=false;(void)ae_files_filter(a->files,"",0);
        if (a->name[0]=='.') { a->show_hidden=true;ae_files_hidden(a->files,true); }
        enum ae_files_result refreshed=ae_files_refresh(a->files);
        if (refreshed==AE_FILES_OK) {
            for (size_t i=0;i<ae_files_count(a->files);++i) if (!strcmp(ae_files_entry(a->files,i)->name,a->name)) { (void)ae_files_select(a->files,i);break; }
            reveal(a);message(a,tr(a,"Created","Angelegt"));
        } else message(a,tr(a,"Created; folder refresh failed. Do not recreate.","Angelegt; Ansicht nicht aktualisiert. Nicht neu anlegen."));
    } else if (r==AE_FILES_CREATE_UNKNOWN) {
        a->unknown=a->creation;a->creation=NULL;a->name_focus=1;
        snprintf(a->create_message,sizeof(a->create_message),"%s",tr(a,"Outcome unknown. Inspect; do not retry.","Ergebnis unklar. Prüfen; nicht wiederholen."));
        message(a,tr(a,"Creation outcome unknown; further creation is blocked","Ergebnis des Anlegens unklar; weiteres Anlegen gesperrt"));
    } else {
        snprintf(a->create_message,sizeof(a->create_message),"%s",r==AE_FILES_CREATE_CONFLICT ? tr(a,"Name already exists. Choose another name.","Name bereits vorhanden. Anderen Namen wählen.") : r==AE_FILES_CREATE_DENIED ? tr(a,"Folder is no longer private and writable","Ordner nicht mehr privat und beschreibbar") : r==AE_FILES_CREATE_NOMEM ? tr(a,"Not enough memory; nothing created","Nicht genug Speicher; nichts angelegt") : tr(a,"Not created. Inspect the target folder.","Nicht angelegt. Zielordner prüfen."));
    }
    ae_files_create_close(a->creation);a->creation=NULL;a->dirty=true;
    if (a->closing) request_close(a);
}
static void activate(struct app *a) {
    if (a->closing || a->query_bad) return;
    const struct ae_files_entry *e=ae_files_entry(a->files,ae_files_selected(a->files));if (!e) return;
    if (e->kind==AE_FILES_DIRECTORY) { result(a,ae_files_enter(a->files));return; }
    if (a->rename_unknown) { message(a,tr(a,"Inspect the unknown rename before opening documents","Unklares Umbenennen vor dem Öffnen von Dokumenten prüfen"));return; }
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
        enum ae_files_result refreshed=ae_files_refresh(a->files);if (refreshed!=AE_FILES_OK) result(a,refreshed);else { a->first=0;message(a,status ? tr(a,"Document window ended with an error","Dokumentfenster mit Fehler beendet") : tr(a,"Document window closed","Dokumentfenster geschlossen")); }if (a->closing && !creating(a)) a->running=false; }
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
static void name_text(struct app *a,float x,float y,float width,unsigned fg) {
    /* Keep the editable tail visible without splitting a UTF-8 grapheme. */
    size_t offsets[AE_FILES_NAME+1],count=0,start=0;int pixels=0,mark=0;
    if (!TTF_SetFontSize(a->font,16*a->scale*a->density) || !TTF_GetStringSize(a->font,a->name,a->name_size,&pixels,NULL) ||
        !TTF_GetStringSize(a->font,"…",3,&mark,NULL) || !ae_text_graphemes(a->name,a->name_size,offsets,AE_FILES_NAME+1,&count)) { fail(a,"name measurement");return; }
    char tail[AE_FILES_NAME+4];
    while ((float)(pixels+(start ? mark : 0))>width*a->density && start+1<count) {
        ++start;if (!TTF_GetStringSize(a->font,a->name+offsets[start],a->name_size-offsets[start],&pixels,NULL)) { fail(a,"name tail");return; }
    }
    if (start) { memcpy(tail,"…",3);memcpy(tail+3,a->name+offsets[start],a->name_size-offsets[start]+1); }
    text(a,start ? tail : a->name,x,y,width,16,fg);
}
static bool menu_enabled(const struct app *a,unsigned item) {
    if (a->closing || creating(a) || unresolved(a) || a->child.pid>0) return false;
    if (item==1) return a->clip_dir>=0 && a->clip_witness>=0;
    const struct ae_files_entry *e=ae_files_entry(a->files,ae_files_selected(a->files));
    return a->menu_selection && !a->query_bad && a->rows && e && (e->kind==AE_FILES_REGULAR || e->kind==AE_FILES_DIRECTORY);
}
static void menu_open(struct app *a,float x,float y,bool selected) {
    a->menu=true;a->menu_x=x;a->menu_y=y;a->menu_selection=selected;a->menu_focus=0;a->pressed=0;a->query_focus=false;
    while (a->menu_focus<2 && !menu_enabled(a,a->menu_focus)) ++a->menu_focus;
    a->dirty=true;
}
static void menu_action(struct app *a,unsigned item) {
    if (!menu_enabled(a,item)) return;
    a->menu=false;a->pressed=0;a->dirty=true;
    if (item==0) cut_selected(a);else if (item==1) paste_open(a);else rename_open(a);
}
static void draw_menu(struct app *a,int w,int h,unsigned fg) {
    float width=fminf(300*a->scale,(float)w-32),rh=34*a->scale+16,height=3*rh+8;
    float x=fmaxf(16,fminf(a->menu_x,(float)w-width-16)),y=fmaxf(16,fminf(a->menu_y,(float)h-height-16));
    a->menu_rect=(SDL_FRect){x,y,width,height};fill(a,a->menu_rect,a->contrast ? 0xffffff : 0x6e91c2);
    for (unsigned i=0;i<3;++i) {
        SDL_FRect r={x+2,y+4+(float)i*rh,width-4,rh};a->menu_rows[i]=r;bool enabled=menu_enabled(a,i);
        fill(a,r,enabled && i==a->menu_focus ? 0x29466b : a->contrast ? 0 : 0x1b2230);
        const char *label=i==0 ? tr(a,"Cut","Ausschneiden") : i==1 ? tr(a,"Paste…","Einfügen…") : tr(a,"Rename…","Umbenennen…");
        text(a,label,r.x+12,r.y+8,r.w-100*a->scale-24,16,enabled ? fg : 0xa5b7d0);
        text(a,i==0 ? "Ctrl+X" : i==1 ? "Ctrl+V" : "F2",r.x+r.w-100*a->scale,r.y+8,100*a->scale-8,14,enabled ? fg : 0xa5b7d0);
    }
}
static void draw_dialog(struct app *a,int w,int h,unsigned fg) {
    float width=fminf((float)w-32,720),height=154*a->scale+80,x=((float)w-width)/2,y=((float)h-height)/2;
    fill(a,(SDL_FRect){x-2,y-2,width+4,height+4},a->contrast ? 0xffffff : 0x6e91c2);
    fill(a,(SDL_FRect){x,y,width,height},a->contrast ? 0 : 0x1b2230);
    text(a,unresolved(a) ? tr(a,"Inspect file operation","Dateiaktion prüfen") : a->moving ? tr(a,"Move","Verschieben") : a->renaming ? tr(a,"Rename","Umbenennen") : a->create_kind==AE_FILES_CREATE_DIRECTORY ? tr(a,"New folder","Neuer Ordner") : tr(a,"New file","Neue Datei"),x+16,y+12,width-32,20,fg);
    text(a,a->renaming ? a->original_label : a->folder,x+16,y+18+26*a->scale,width-32,14,0xa5b7d0);
    if (a->renaming) {
        float fy=y+18+40*a->scale;
        if (a->moving) {
            float half=(width-32)/2;
            text(a,tr(a,"From:","Von:"),x+16,fy,45*a->scale,10,0xa5b7d0);text(a,a->from_folder,x+16+45*a->scale,fy,half-45*a->scale-8,10,0xa5b7d0);
            text(a,tr(a,"To:","Nach:"),x+16+half,fy,45*a->scale,10,0xa5b7d0);text(a,a->folder,x+16+half+45*a->scale,fy,half-45*a->scale,10,0xa5b7d0);
        } else { text(a,tr(a,"Folder:","Ordner:"),x+16,fy,50*a->scale,10,0xa5b7d0);text(a,a->folder,x+16+50*a->scale,fy,width-32-50*a->scale,10,0xa5b7d0); }
    }
    a->name_field=(SDL_FRect){x+16,y+28+46*a->scale,width-32,24*a->scale+16};
    fill(a,a->name_field,a->name_bad ? 0x613538 : a->name_focus==0 && !unresolved(a) && !a->rename_stale ? 0x29466b : 0x10141c);
    if (*a->name) name_text(a,a->name_field.x+8,a->name_field.y+6,a->name_field.w-16,fg);
    else text(a,a->renaming ? tr(a,"New name","Neuer Name") : tr(a,"Name","Name"),a->name_field.x+8,a->name_field.y+6,a->name_field.w-16,16,0xa5b7d0);
    float status=a->name_field.y+a->name_field.h+8;
    text(a,*a->create_message ? a->create_message : tr(a,"Existing entries are never replaced","Vorhandene Einträge werden nie ersetzt"),x+16,status,width-32,12,a->name_bad || unresolved(a) || a->rename_stale ? 0xffbdad : fg);
    text(a,a->name_bad ? tr(a,"Ctrl+U clears the rejected name","Strg+U löscht den abgewiesenen Namen") : unresolved(a) ? tr(a,"Further creation/rename blocked in this window","Weiteres Anlegen/Umbenennen hier gesperrt") : a->rename_stale ? tr(a,"Cancel, then refresh the folder","Abbrechen, dann den Ordner aktualisieren") : a->moving ? tr(a,"Enter move · Tab focus · Escape cancel","Enter verschieben · Tab Fokus · Escape zurück") : a->renaming ? tr(a,"Ctrl+U name · Enter rename · Escape cancel","Strg+U Name · Enter OK · Escape zurück") : tr(a,"Enter create · Tab change focus · Escape cancel","Enter anlegen · Tab Fokus · Escape abbrechen"),x+16,status+16*a->scale+4,width-32,12,0xa5b7d0);
    const char *recovery=ae_files_create_recovery(a->unknown);
    if (recovery) text(a,recovery,x+16,status+32*a->scale+8,width-32,12,0xffbdad);
    float button=(width-40)/2,bh=22*a->scale+16,by=y+height-bh-16;
    a->cancel=(SDL_FRect){x+16,by,button,bh};a->submit=(SDL_FRect){x+24+button,by,button,bh};
    fill(a,a->cancel,a->name_focus==1 ? 0x29466b : 0x10141c);text(a,unresolved(a) ? tr(a,"Back","Zurück") : tr(a,"Cancel","Abbrechen"),a->cancel.x+10,by+6,button-20,14,fg);
    bool enabled=!creating(a) && !unresolved(a) && !a->rename_stale && !a->name_bad && ae_files_create_name(a->name,a->name_size) && !a->closing && (!a->renaming || a->moving || strcmp(a->name,a->original));
    fill(a,a->submit,enabled ? a->name_focus==2 ? 0x416b9e : 0x29466b : 0x10141c);text(a,a->moving ? tr(a,"Move","Verschieben") : a->renaming ? tr(a,"Rename","Umbenennen") : tr(a,"Create","Anlegen"),a->submit.x+10,by+6,button-20,14,enabled ? fg : 0xa5b7d0);
}
static void draw(struct app *a) {
    int w,h,pw,ph;if (!SDL_GetWindowSize(a->window,&w,&h) || !SDL_GetWindowSizeInPixels(a->window,&pw,&ph)) { fail(a,"size");return; }
    a->density=SDL_GetWindowPixelDensity(a->window);if (!isfinite(a->density) || a->density<.5f || a->density>4 || !SDL_SetRenderScale(a->renderer,(float)pw/(float)w,(float)ph/(float)h)) { fail(a,"density");return; }
    unsigned bg=a->contrast ? 0u : 0x10141cu,panel=a->contrast ? 0u : 0x1b2230u,fg=a->contrast ? 0xffffffu : 0xe8edf5u;
    if (!SDL_SetRenderDrawColor(a->renderer,(Uint8)(bg>>16),(Uint8)(bg>>8),(Uint8)bg,255) || !SDL_RenderClear(a->renderer)) { fail(a,"clear");return; }
    float header=32*a->scale+24,control=26*a->scale+16;a->top=header+2*control+12;a->bottom=(float)h-(48*a->scale+16);a->row_height=34*a->scale+18;
    a->rows=a->bottom>a->top ? (size_t)((a->bottom-a->top)/a->row_height) : 0;reveal(a);
    text(a,tr(a,"Files","Dateien"),20,12,(float)w-40,24,fg);
    a->rename_button=(SDL_FRect){(float)w-20-120*a->scale,8,120*a->scale,control-4};fill(a,a->rename_button,panel);
    text(a,tr(a,"Rename","Umbenennen"),a->rename_button.x+8,13,a->rename_button.w-16,14,fg);
    a->more=(SDL_FRect){a->rename_button.x-48*a->scale,8,40*a->scale,control-4};fill(a,a->more,panel);text(a,"…",a->more.x+8,13,a->more.w-16,16,fg);
    a->new_file=(SDL_FRect){(float)w-20-240*a->scale,header,110*a->scale,control-4};a->new_folder=(SDL_FRect){a->new_file.x+a->new_file.w+8,header,120*a->scale,control-4};
    text(a,ae_files_folder(a->files),20,header,a->new_file.x-36,14,0xa5b7d0);
    fill(a,a->new_file,panel);fill(a,a->new_folder,panel);
    text(a,tr(a,"New file","Neue Datei"),a->new_file.x+8,header+5,a->new_file.w-16,14,fg);text(a,tr(a,"New folder","Neuer Ordner"),a->new_folder.x+8,header+5,a->new_folder.w-16,14,fg);
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
    if (!ae_files_count(a->files) && a->rows) text(a,tr(a,"No matching entries","Keine passenden Einträge"),28,a->top,(float)w-56,16,fg);
    char footer[160];snprintf(footer,sizeof(footer),"%zu / %zu · %s",ae_files_count(a->files),ae_files_total(a->files),tr(a,"Enter open · Alt+Up back · Ctrl+R refresh","Enter öffnen · Alt+Hoch zurück · Strg+R neu"));
    text(a,unresolved(a) ? tr(a,"File operation outcome unknown; do not retry","Ergebnis der Dateiaktion unklar; nicht wiederholen") : a->closing && *a->message ? a->message : a->clip_dir>=0 ? tr(a,"Move selection retained · Ctrl+V preview · Escape clear","Verschiebeauswahl bleibt · Strg+V Vorschau · Escape löschen") : !a->rows ? tr(a,"Enlarge the window to browse","Fenster zum Navigieren vergrößern") : *a->message ? a->message : footer,20,a->bottom+4,(float)w-40,12,fg);text(a,tr(a,"F10 actions · F2 rename · F6 language · F7 contrast · F8 text · Ctrl+Q close","F10 Aktionen · F2 umbenennen · F6 Sprache · F7 Kontrast · F8 Text · Strg+Q schließen"),20,a->bottom+24*a->scale,(float)w-40,12,0xa5b7d0);
    if (a->dialog) draw_dialog(a,w,h,fg);
    else if (a->menu) draw_menu(a,w,h,fg);
    if (!SDL_RenderPresent(a->renderer)) fail(a,"present");
    a->dirty=false;++a->frames;
    if (a->diagnostics) printf("VIEW frame=%u count=%zu total=%zu selected=%zu top=%zu rows=%zu depth=%u scale=%.2f german=%d contrast=%d filter=%d rejected=%d child=%d width=%d height=%d dialog=%d name_bytes=%zu name_bad=%d name_focus=%u creating=%d unknown=%d renaming=%d stale=%d moving=%d clipboard=%d menu=%d menu_focus=%u\n",a->frames,ae_files_count(a->files),ae_files_total(a->files),ae_files_selected(a->files),a->first,a->rows,ae_files_depth(a->files),(double)a->scale,a->german,a->contrast,a->query_focus,a->query_bad,a->child.pid,w,h,a->dialog,a->name_size,a->name_bad,a->name_focus,creating(a),unresolved(a),a->renaming,a->rename_stale,a->moving,a->clip_dir>=0,a->menu,a->menu_focus);
}
static bool inside(SDL_FRect r,float x,float y) { return r.w>0 && x>=r.x && y>=r.y && x<r.x+r.w && y<r.y+r.h; }
static unsigned target(struct app *a,float x,float y,size_t *row) {
    *row=0;
    if (a->dialog) { *row=0;if (inside(a->name_field,x,y)) return 7;if (inside(a->cancel,x,y)) return 8;if (inside(a->submit,x,y)) return 9;return 0; }
    if (a->menu) { for (unsigned i=0;i<3;++i) if (inside(a->menu_rows[i],x,y)) return 30+i;return 0; }
    if (inside(a->new_file,x,y)) return 5;
    if (inside(a->new_folder,x,y)) return 6;
    if (inside(a->rename_button,x,y)) return 10;
    if (inside(a->more,x,y)) return 11;
    *row=0;if (inside(a->back,x,y)) return 1;if (inside(a->hidden,x,y)) return 2;if (inside(a->search,x,y)) return 3;
    if (y>=a->top && y<a->bottom && a->rows && x>=16) { size_t n=(size_t)((y-a->top)/a->row_height);if (n<a->rows && a->first+n<ae_files_count(a->files)) { *row=a->first+n;return 4; } }return 0;
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
    return p->y<44 && !a->dialog && !a->menu && !inside(a->rename_button,(float)p->x,(float)p->y) && !inside(a->more,(float)p->x,(float)p->y) ? SDL_HITTEST_DRAGGABLE : SDL_HITTEST_NORMAL;
}
static void name_input(struct app *a,const char *input) {
    if (unresolved(a) || a->rename_stale || creating(a) || a->name_bad || a->name_focus!=0) return;
    size_t n=strlen(input);char next[AE_FILES_NAME+1];
    if (n<=AE_FILES_NAME-a->name_size) {
        memcpy(next,a->name,a->name_size);memcpy(next+a->name_size,input,n+1);size_t size=a->name_size+n;
        if (ae_files_create_name(next,size) || (size==1 && next[0]=='.') || (size==2 && !memcmp(next,"..",2))) {
            memcpy(a->name,next,size+1);a->name_size=size;*a->create_message=0;a->dirty=true;return;
        }
    }
    a->name_bad=true;snprintf(a->create_message,sizeof(a->create_message),"%s",tr(a,"Name rejected; retained prefix cannot be used","Name abgewiesen; bisheriger Teil nicht verwendbar"));a->dirty=true;
}
static void dialog_key(struct app *a,SDL_Keycode key,SDL_Keymod mod) {
    if (creating(a)) return;
    if (key==SDLK_ESCAPE) dialog_close(a);
    else if (key==SDLK_TAB) {
        a->name_focus=unresolved(a) ? 1 : (a->name_focus+((mod&SDL_KMOD_SHIFT) ? 2u : 1u))%3;a->dirty=true;
    } else if (key==SDLK_RETURN) { if (a->name_focus==1) dialog_close(a);else create_submit(a); }
    else if (!unresolved(a) && !a->rename_stale && a->name_focus==0 && (mod&SDL_KMOD_CTRL) && key==SDLK_U) {
        a->name_bad=false;*a->name=0;a->name_size=0;*a->create_message=0;a->dirty=true;
    } else if (!unresolved(a) && !a->rename_stale && a->name_focus==0 && !a->name_bad && key==SDLK_BACKSPACE && a->name_size) {
        size_t offsets[AE_FILES_NAME+1],count;
        if (ae_text_graphemes(a->name,a->name_size,offsets,AE_FILES_NAME+1,&count)) { a->name_size=offsets[count-2];a->name[a->name_size]=0;*a->create_message=0;a->dirty=true; }
    }
}
static void menu_key(struct app *a,SDL_Keycode key,SDL_Keymod mod) {
    if (key==SDLK_ESCAPE || key==SDLK_F10) { a->menu=false;a->dirty=true; }
    else if (key==SDLK_UP || key==SDLK_DOWN || key==SDLK_TAB) {
        unsigned delta=key==SDLK_UP || (key==SDLK_TAB && (mod&SDL_KMOD_SHIFT)) ? 2u : 1u;
        for (unsigned n=0;n<3;++n) { a->menu_focus=(a->menu_focus+delta)%3;if (menu_enabled(a,a->menu_focus)) break; }a->dirty=true;
    } else if (key==SDLK_RETURN || key==SDLK_SPACE) menu_action(a,a->menu_focus);
    else if ((mod&SDL_KMOD_CTRL) && key==SDLK_X) menu_action(a,0);
    else if ((mod&SDL_KMOD_CTRL) && key==SDLK_V) menu_action(a,1);
    else if (key==SDLK_F2) menu_action(a,2);
}
static void event(struct app *a,const SDL_Event *e) {
    if (e->type==SDL_EVENT_QUIT || e->type==SDL_EVENT_WINDOW_CLOSE_REQUESTED) request_close(a);
    else if (e->type==SDL_EVENT_KEY_DOWN && !e->key.repeat) {
        a->pressed=0;
        SDL_Keycode k=e->key.key;SDL_Keymod mod=e->key.mod;
        if ((mod&SDL_KMOD_CTRL) && k==SDLK_Q) request_close(a);
        else if (k==SDLK_F6) { a->german=!a->german;a->dirty=true; }
        else if (k==SDLK_F7) { a->contrast=!a->contrast;a->dirty=true; }
        else if (k==SDLK_F8) { a->scale=a->scale==1 ? 1.25f : a->scale==1.25f ? 1.5f : a->scale==1.5f ? 2 : 1;a->dirty=true; }
        else if (a->dialog) dialog_key(a,k,mod);
        else if (a->menu) menu_key(a,k,mod);
        else if (k==SDLK_F10) menu_open(a,a->more.x,a->more.y+a->more.h+4,true);
        else if ((mod&SDL_KMOD_CTRL) && k==SDLK_X && !a->query_focus) cut_selected(a);
        else if ((mod&SDL_KMOD_CTRL) && k==SDLK_V && !a->query_focus) paste_open(a);
        else if (k==SDLK_F2) rename_open(a);
        else if ((mod&SDL_KMOD_CTRL) && k==SDLK_N) dialog_open(a,(mod&SDL_KMOD_SHIFT) ? AE_FILES_CREATE_DIRECTORY : AE_FILES_CREATE_FILE);
        else if ((mod&SDL_KMOD_CTRL) && k==SDLK_R) result(a,ae_files_refresh(a->files));
        else if ((mod&SDL_KMOD_CTRL) && k==SDLK_H) { a->show_hidden=!a->show_hidden;ae_files_hidden(a->files,a->show_hidden);reveal(a);a->dirty=true; }
        else if ((mod&SDL_KMOD_CTRL) && (k==SDLK_L || (k==SDLK_U && a->query_focus))) { a->query_focus=true;a->query_bad=false;(void)ae_files_filter(a->files,"",0);a->dirty=true; }
        else if ((mod&SDL_KMOD_ALT) && k==SDLK_UP) result(a,ae_files_up(a->files));
        else if ((mod&SDL_KMOD_ALT) && k==SDLK_HOME) result(a,ae_files_ancestor(a->files,0));
        else if (k==SDLK_ESCAPE) { clipboard_clear(a);a->query_focus=false;a->query_bad=false;(void)ae_files_filter(a->files,"",0);a->first=0;a->dirty=true; }
        else if (k==SDLK_UP || k==SDLK_DOWN) move(a,k==SDLK_UP ? -1 : 1);
        else if (k==SDLK_PAGEUP || k==SDLK_PAGEDOWN) move(a,(int)(a->rows ? a->rows : 1)*(k==SDLK_PAGEUP ? -1 : 1));
        else if (k==SDLK_HOME || k==SDLK_END) { if (ae_files_count(a->files)) (void)ae_files_select(a->files,k==SDLK_HOME ? 0 : ae_files_count(a->files)-1);reveal(a);a->dirty=true; }
        else if (k==SDLK_RETURN) { a->query_focus=false;if (a->rows) activate(a); }
        else if (k==SDLK_BACKSPACE) { if (a->query_focus) { const char *q=ae_files_query(a->files);size_t n=strlen(q),points[AE_FILES_QUERY+1],count;if (!a->query_bad && n && ae_text_graphemes(q,n,points,AE_FILES_QUERY+1,&count)) (void)ae_files_filter(a->files,q,points[count-2]);a->dirty=true; }else result(a,ae_files_up(a->files)); }
    } else if (e->type==SDL_EVENT_TEXT_INPUT && a->dialog) { a->pressed=0;name_input(a,e->text.text); }
    else if (e->type==SDL_EVENT_TEXT_INPUT && !a->menu && a->query_focus) {
        a->pressed=0;if (a->query_bad) return;
        char q[AE_FILES_QUERY+1];size_t old=strlen(ae_files_query(a->files)),n=strlen(e->text.text);
        if (n<=AE_FILES_QUERY-old) { memcpy(q,ae_files_query(a->files),old);memcpy(q+old,e->text.text,n);if (ae_files_filter(a->files,q,old+n)) { a->first=0;a->dirty=true; }else { a->query_bad=true;message(a,"Filter rejected; Ctrl+L clears"); } }else { a->query_bad=true;message(a,"Filter rejected; Ctrl+L clears"); }
    } else if (e->type==SDL_EVENT_MOUSE_WHEEL) { a->pressed=0;if (!a->dialog && !a->menu) move(a,e->wheel.y>0 ? -3 : e->wheel.y<0 ? 3 : 0); }
    else if (e->type==SDL_EVENT_MOUSE_BUTTON_DOWN && e->button.button==SDL_BUTTON_RIGHT) {
        a->pressed=0;
        if (!a->dialog && !creating(a) && !a->closing) {
            if (a->menu) { a->menu=false;a->dirty=true; }
            else { size_t row;unsigned hit=target(a,e->button.x,e->button.y,&row);bool selected=hit==4 && ae_files_select(a->files,row);menu_open(a,e->button.x,e->button.y,selected); }
        }
    }
    else if (e->type==SDL_EVENT_MOUSE_BUTTON_DOWN && e->button.button==SDL_BUTTON_LEFT) a->pressed=target(a,e->button.x,e->button.y,&a->pressed_row);
    else if (e->type==SDL_EVENT_MOUSE_BUTTON_UP && e->button.button==SDL_BUTTON_LEFT) {
        size_t row;unsigned hit=target(a,e->button.x,e->button.y,&row),pressed=a->pressed;a->pressed=0;if (hit!=pressed) return;
        if (a->dialog) {
            if (!creating(a)) { if (hit==7 && !unresolved(a)) { a->name_focus=0;a->dirty=true; }else if (hit==8) dialog_close(a);else if (hit==9) { a->name_focus=2;create_submit(a);a->dirty=true; } }
        } else if (a->menu) { if (hit>=30 && hit<33) menu_action(a,hit-30);else { a->menu=false;a->dirty=true; } }
        else if (hit==1) result(a,ae_files_up(a->files));
        else if (hit==2) { a->show_hidden=!a->show_hidden;ae_files_hidden(a->files,a->show_hidden);reveal(a);a->dirty=true; }
        else if (hit==3) { a->query_focus=true;a->dirty=true; }
        else if (hit==4 && row==a->pressed_row && ae_files_select(a->files,row)) { a->query_focus=false;a->dirty=true;if (e->button.clicks==2) activate(a); }
        else if (hit==5 || hit==6) dialog_open(a,hit==5 ? AE_FILES_CREATE_FILE : AE_FILES_CREATE_DIRECTORY);
        else if (hit==10) rename_open(a);
        else if (hit==11) menu_open(a,a->more.x,a->more.y+a->more.h+4,true);
    } else if (e->type==SDL_EVENT_WINDOW_FOCUS_LOST) { a->pressed=0;if (a->menu) { a->menu=false;a->dirty=true; } }
    else if (e->type==SDL_EVENT_WINDOW_EXPOSED || e->type==SDL_EVENT_WINDOW_RESIZED || e->type==SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) { a->pressed=0;a->menu=false;a->dirty=true; }
}
int main(int argc,char **argv) {
    struct app a={.child={.pid=-1,.pin=-1},.wake={-1,-1},.create_dir=-1,.rename_witness=-1,.move_target=-1,.clip_dir=-1,.clip_witness=-1,.scale=1,.running=true,.dirty=true};const char *root=NULL,*font=NULL;
    atomic_init(&a.create_done,false);
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
        create_update(&a);child_update(&a);if (stopping) { stopping=0;request_close(&a); }if (!a.running) break;
        if (a.dirty && !a.failed) draw(&a);
        if (!a.running) break;
        SDL_Event e;bool timed=busy(&a) || creating(&a);bool got=timed ? SDL_WaitEventTimeout(&e,200) : SDL_WaitEvent(&e);if (got) event(&a,&e);else if (!timed) fail(&a,"event wait");
    }
done:
    if (a.creator) { SDL_WaitThread(a.creator,NULL);a.creator=NULL; }
    ae_files_create_close(a.creation);ae_files_create_close(a.unknown);if (a.create_dir>=0) close(a.create_dir);
    ae_files_rename_close(a.rename_op);ae_files_rename_close(a.rename_unknown);if (a.rename_witness>=0) close(a.rename_witness);
    if (a.move_target>=0) close(a.move_target);
    clipboard_clear(&a);
    if (a.worker) { atomic_store(&a.active,false);unsigned char b=1;(void)write(a.wake[1],&b,1);SDL_WaitThread(a.worker,NULL); }
    signal_writer=-1;for (unsigned i=0;i<2;++i) if (a.wake[i]>=0) close(a.wake[i]);
    if (a.child.pid>0) { if (!a.child.ended) { fputs("files: refusing to retire a live document child\n",stderr);return 125; }(void)ae_files_child_close(&a.child); }
    SDL_DestroyRenderer(a.renderer);SDL_DestroyWindow(a.window);TTF_CloseFont(a.font);TTF_Quit();SDL_Quit();ae_files_destroy(a.files);
    if (a.diagnostics) printf("CLOSED files frames=%u failed=%d\n",a.frames,a.failed);
    return a.failed ? 1 : 0;
}
