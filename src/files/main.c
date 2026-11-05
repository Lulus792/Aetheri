/* Local development view; normal app/document grants remain separate. */
#define _GNU_SOURCE
#include "model.h"
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
    SDL_Window *window;SDL_Renderer *renderer;TTF_Font *font;SDL_Thread *worker;struct ae_files *files;struct ae_files_child child;
    const char *terminal,*editor,*terminal_font,*cache;char message[256];
    float scale,density;size_t first,rows;SDL_FRect back,hidden,search;
    bool running,failed,closing,dirty,german,contrast,show_hidden,query_focus,query_bad,diagnostics;unsigned frames,launches,pressed;size_t pressed_row;
    int wake[2];uint32_t event;atomic_bool active;
    SDL_FRect new_file,new_folder,name_field,cancel,submit;
    bool dialog,name_bad;unsigned name_focus,creations;int create_dir;
    enum ae_files_create_kind create_kind;char name[AE_FILES_NAME+1],folder[AE_FILES_LABEL+1],create_message[256];size_t name_size;
    struct ae_files_create *creation,*unknown;SDL_Thread *creator;atomic_bool create_done;
    bool renaming,rename_stale;unsigned renames;int rename_witness;
    char original[AE_FILES_NAME+1],original_label[AE_FILES_LABEL+1];
    struct ae_files_rename *rename_op,*rename_unknown;
    bool moving,menu,menu_selection;unsigned moves,menu_focus;int move_target,clip_dir,clip_witness;
    char clip_name[AE_FILES_NAME+1],clip_label[AE_FILES_LABEL+1],clip_folder[AE_FILES_LABEL+1],from_folder[AE_FILES_LABEL+1];
    SDL_FRect more,menu_rect,menu_rows[3];float menu_x,menu_y;
    struct ae_files_layout layout;
    bool grid,details,sidebar,info_overlay;
    SDL_FRect list_button,grid_button,details_button,close_button,fullscreen_button,info_close;
    SDL_FRect places[AE_FILES_DEPTH+1];unsigned place_depth[AE_FILES_DEPTH+1];size_t place_snapshot[AE_FILES_DEPTH+1],place_count;
    char root_label[AE_FILES_LABEL+1];
    bool consume_release,composing;SDL_FRect info_rect;
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
static bool inline_edit(const struct app *a) { return a->dialog && !a->moving && !unresolved(a) && !a->rename_stale; }
static const char *tr(const struct app *a,const char *en,const char *de) { return a->german ? de : en; }
static void message(struct app *a,const char *s) { snprintf(a->message,sizeof(a->message),"%s",s);a->dirty=true; }
static void fail(struct app *a,const char *s) { fprintf(stderr,"files: %s: %s\n",s,SDL_GetError());a->failed=true;a->closing=true;if (!busy(a) && !creating(a)) a->running=false; }
static void result(struct app *a,enum ae_files_result r) {
    a->pressed=0;
    const char *s=r==AE_FILES_OK ? "" : r==AE_FILES_ROOT ? tr(a,"At the selected root","Am gewählten Ausgangsordner") : r==AE_FILES_CHANGED ? tr(a,"Folder or file changed; Ctrl+R refreshes","Ordner oder Datei geändert; Strg+R aktualisiert") : r==AE_FILES_LIMIT ? tr(a,"Folder/depth limit; view retained","Ordner-/Tiefenlimit; Ansicht bleibt erhalten") : r==AE_FILES_NOMEM ? tr(a,"Not enough memory; view retained","Nicht genug Speicher; Ansicht bleibt erhalten") : r==AE_FILES_UNSUPPORTED ? tr(a,"This entry cannot be opened here","Dieser Eintrag lässt sich hier nicht öffnen") : tr(a,"Cannot read this folder","Ordner kann nicht gelesen werden");
    message(a,s);if (r==AE_FILES_OK) { a->first=0;if (!*ae_files_query(a->files)) a->query_bad=false; }
}
static void reveal(struct app *a) {
    if (a->layout.capacity) a->first=ae_files_layout_reveal(&a->layout,ae_files_count(a->files),ae_files_selected(a->files),a->first);
}
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
    a->pressed=0;a->query_focus=false;a->name_focus=0;a->name_bad=false;a->composing=false;*a->create_message=0;
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
    a->renaming=true;a->moving=false;a->rename_stale=false;a->dialog=true;a->name_bad=false;a->name_focus=0;a->query_focus=false;a->pressed=0;a->composing=false;*a->create_message=0;a->dirty=true;
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
    a->dialog=true;a->name_bad=false;a->name_focus=0;a->query_focus=false;a->pressed=0;a->composing=false;*a->create_message=0;a->dirty=true;
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
    if (a->composing) return;
    if (inline_edit(a) && a->renaming && !creating(a) && !a->closing && !a->name_bad &&
        ae_files_create_name(a->name,a->name_size) && !strcmp(a->name,a->original)) { dialog_close(a);return; }
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
    if (r) { a->pressed=0;int status=a->child.status;if (a->diagnostics) printf("DOCUMENT_END status=%d\n",status);(void)ae_files_child_close(&a->child);
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
struct palette { unsigned canvas,surface,raised,line,text,muted,selection,focus,icon; };
static struct palette colors(const struct app *a) {
    return a->contrast ? (struct palette){0,0,0,0xffffff,0xffffff,0xffffff,0x163e64,0xffffff,0xffffff} :
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
    rounded(a,r,5*a->scale,selected ? p.selection : p.raised);icon(a,kind,r,selected ? p.text : p.muted);
}
static const char *kind_label(struct app *a,const struct ae_files_entry *e) {
    return e->kind==AE_FILES_DIRECTORY ? tr(a,"Folder","Ordner") : e->kind==AE_FILES_REGULAR ? tr(a,"File","Datei") : e->kind==AE_FILES_LINK ? tr(a,"Link (not followed)","Link (nicht gefolgt)") : tr(a,"Unavailable here","Hier nicht verfügbar");
}
static const char *folder_label(const struct app *a) { return ae_files_depth(a->files) ? ae_files_folder(a->files) : a->root_label; }
static void place(struct app *a,const char *label,unsigned depth,size_t snapshot,float y,struct palette p) {
    size_t n=a->place_count;if (n>=AE_FILES_DEPTH+1) return;
    SDL_FRect r={10*a->scale,y,a->layout.sidebar.w-20*a->scale,32*a->scale};
    a->places[n]=r;a->place_depth[n]=depth;a->place_snapshot[n]=snapshot;++a->place_count;
    if (snapshot==SIZE_MAX && depth==ae_files_depth(a->files)) rounded(a,r,5*a->scale,p.selection);
    icon(a,0,(SDL_FRect){r.x+2*a->scale,r.y,30*a->scale,r.h},p.icon);
    text(a,label,r.x+34*a->scale,r.y+8*a->scale,r.w-40*a->scale,12,p.text);
}
static void draw_places(struct app *a,struct palette p) {
    a->place_count=0;if (!a->layout.sidebar_visible) return;
    float s=a->scale;fill(a,rect(a->layout.sidebar),p.surface);
    line(a,a->layout.sidebar.w-1,a->layout.chrome.h,a->layout.sidebar.w-1,a->layout.status.y,p.line);
    text(a,tr(a,"Locations","Orte"),18*s,a->layout.chrome.h+18*s,a->layout.sidebar.w-36*s,10,p.muted);
    float y=a->layout.chrome.h+42*s,limit=a->layout.status.y-12*s;unsigned depth=ae_files_depth(a->files);
    place(a,a->root_label,0,SIZE_MAX,y,p);y+=36*s;
    if (depth) {
        unsigned slots=limit>y ? (unsigned)((limit-y)/(36*s)) : 0;
        unsigned start=depth>=slots && slots ? depth-slots+1 : 1;
        for (unsigned d=start;d<=depth && y+32*s<=limit;++d,y+=36*s) place(a,ae_files_ancestor_label(a->files,d),d,SIZE_MAX,y,p);
    } else {
        y+=18*s;
        for (size_t i=0;i<ae_files_total(a->files) && y+32*s<=limit;++i) {
            const struct ae_files_entry *e=ae_files_snapshot_entry(a->files,i);
            if (e && e->kind==AE_FILES_DIRECTORY && (a->show_hidden || e->name[0]!='.')) { place(a,e->label,0,i,y,p);y+=36*s; }
        }
    }
}
static void draw_information(struct app *a,SDL_FRect r,struct palette p) {
    float s=a->scale;fill(a,r,p.surface);line(a,r.x,r.y,r.x,r.y+r.h,p.line);
    const struct ae_files_entry *e=ae_files_entry(a->files,ae_files_selected(a->files));
    text(a,tr(a,"Information","Informationen"),r.x+14*s,r.y+14*s,r.w-28*s,12,p.text);
    if (inline_edit(a) && !a->renaming) {
        text(a,a->create_kind==AE_FILES_CREATE_DIRECTORY ? tr(a,"New folder","Neuer Ordner") : tr(a,"New file","Neue Datei"),r.x+14*s,r.y+48*s,r.w-28*s,13,p.text);
        text(a,creating(a) ? tr(a,"Waiting for result","Warten auf das Ergebnis") : tr(a,"Not created yet","Noch nicht angelegt"),r.x+14*s,r.y+78*s,r.w-28*s,11,p.muted);
        text(a,folder_label(a),r.x+14*s,r.y+110*s,r.w-28*s,11,p.muted);return;
    }
    if (!e) { text(a,tr(a,"No selection","Keine Auswahl"),r.x+14*s,r.y+48*s,r.w-28*s,12,p.muted);return; }
    SDL_FRect tile={r.x+14*s,r.y+44*s,r.w-28*s,72*s};rounded(a,tile,8*s,p.raised);
    icon(a,e->kind==AE_FILES_DIRECTORY ? 0u : 1u,(SDL_FRect){tile.x+(tile.w-64*s)/2,tile.y+4*s,64*s,64*s},p.icon);
    text(a,e->label,r.x+14*s,r.y+130*s,r.w-28*s,13,p.text);
    text(a,kind_label(a,e),r.x+14*s,r.y+154*s,r.w-28*s,11,p.muted);
    char bytes[64];snprintf(bytes,sizeof(bytes),"%llu bytes",(unsigned long long)e->size);
    if (e->kind==AE_FILES_REGULAR) text(a,bytes,r.x+14*s,r.y+180*s,r.w-28*s,11,p.muted);
    if (r.h>248*s) { text(a,tr(a,"Location","Ort"),r.x+14*s,r.y+218*s,r.w-28*s,10,p.muted);text(a,folder_label(a),r.x+14*s,r.y+238*s,r.w-28*s,11,p.text); }
}
static void draw_inline(struct app *a,SDL_FRect r,struct palette p) {
    float s=a->scale;bool card=a->grid && !a->layout.compact_cards;
    float field_x=r.x+(card ? 8 : 46)*s,field_y=r.y+(card ? 74 : 5)*s;
    float buttons_y=card ? r.y+112*s : a->grid ? r.y+r.h-30*s : field_y;
    float button=28*s,right=r.x+r.w-8*s;
    a->submit=(SDL_FRect){right-button,buttons_y,button,28*s};a->cancel=(SDL_FRect){right-2*button-6*s,buttons_y,button,28*s};
    float width=card || a->grid ? r.x+r.w-8*s-field_x : a->cancel.x-8*s-field_x;
    a->name_field=(SDL_FRect){field_x,field_y,width,28*s};
    rounded(a,a->name_field,4*s,p.surface);
    outline(a,a->name_field,a->name_bad ? 0xffbdad : a->name_focus==0 && !creating(a) ? p.focus : p.line);
    if (*a->name) name_text(a,field_x+5*s,field_y+3*s,width-10*s,p.text);
    else text(a,tr(a,"Name","Name"),field_x+5*s,field_y+5*s,width-10*s,12,p.muted);
    icon_button(a,a->cancel,5,a->name_focus==1,p);icon_button(a,a->submit,9,a->name_focus==2,p);
    if (!a->grid) text(a,creating(a) ? tr(a,"Working…","Wird ausgeführt…") : tr(a,"Enter confirm · Escape cancel","Enter bestätigen · Escape abbrechen"),r.x+46*s,r.y+43*s,r.w-56*s,10,p.muted);
}
static void control_record(struct app *a,const char *id,size_t row,SDL_FRect r) {
    if (r.w>0 && r.h>0) printf("CONTROL frame=%u id=%s row=%zu x=%.2f y=%.2f w=%.2f h=%.2f\n",a->frames,id,row,(double)r.x,(double)r.y,(double)r.w,(double)r.h);
}
static void controls(struct app *a) {
    if (!a->diagnostics) return;
    if (a->dialog) { control_record(a,"name",0,a->name_field);control_record(a,"cancel",0,a->cancel);control_record(a,"submit",0,a->submit);return; }
    if (a->info_overlay) { control_record(a,"info_close",0,a->info_close);return; }
    if (a->menu) { for (size_t i=0;i<3;++i) control_record(a,"menu",i,a->menu_rows[i]);return; }
    control_record(a,"back",0,a->back);control_record(a,"list",0,a->list_button);control_record(a,"grid",0,a->grid_button);
    control_record(a,"new_file",0,a->new_file);control_record(a,"new_folder",0,a->new_folder);control_record(a,"details",0,a->details_button);
    control_record(a,"more",0,a->more);control_record(a,"hidden",0,a->hidden);control_record(a,"search",0,a->search);
    control_record(a,"close",0,a->close_button);control_record(a,"fullscreen",0,a->fullscreen_button);
    for (size_t i=0;i<a->rows && a->first+i<ae_files_count(a->files);++i) { struct ae_files_rect r;if (ae_files_layout_cell(&a->layout,i,&r)) control_record(a,"entry",a->first+i,rect(r)); }
    for (size_t i=0;i<a->place_count;++i) control_record(a,"place",i,a->places[i]);
}
static void draw(struct app *a) {
    int w,h,pw,ph;if (!SDL_GetWindowSize(a->window,&w,&h) || !SDL_GetWindowSizeInPixels(a->window,&pw,&ph)) { fail(a,"size");return; }
    a->density=SDL_GetWindowPixelDensity(a->window);if (!isfinite(a->density) || a->density<.5f || a->density>4 || !SDL_SetRenderScale(a->renderer,(float)pw/(float)w,(float)ph/(float)h)) { fail(a,"density");return; }
    bool naming=inline_edit(a);
    if (!ae_files_layout_make(w,h,a->scale,a->grid,a->details,a->sidebar,naming,&a->layout)) { fail(a,"layout");return; }
    struct palette p=colors(a);float s=a->scale;struct ae_files_layout *l=&a->layout;
    if (!SDL_SetRenderDrawColor(a->renderer,(Uint8)(p.canvas>>16),(Uint8)(p.canvas>>8),(Uint8)p.canvas,255) || !SDL_RenderClear(a->renderer)) { fail(a,"clear");return; }
    a->rows=l->capacity;
    size_t count=ae_files_count(a->files),naming_index=a->renaming ? ae_files_selected(a->files) : count;
    size_t visual_count=count+(naming && !a->renaming ? 1u : 0u);
    a->first=ae_files_layout_reveal(l,visual_count,naming ? naming_index : ae_files_selected(a->files),a->first);
    a->back=rect(l->back);a->list_button=rect(l->list);a->grid_button=rect(l->grid);a->new_folder=rect(l->new_folder);a->new_file=rect(l->new_file);
    a->search=rect(l->search);a->details_button=rect(l->details);a->more=rect(l->more);a->close_button=rect(l->close);a->fullscreen_button=rect(l->fullscreen);
    icon(a,5,a->close_button,p.muted);icon(a,6,a->fullscreen_button,p.muted);
    text(a,tr(a,"Files","Dateien"),76*s,7*s,(float)w-150*s,12,p.text);
    icon_button(a,a->more,2,false,p);draw_places(a,p);fill(a,rect(l->toolbar),p.surface);
    icon_button(a,a->back,4,false,p);icon_button(a,a->list_button,2,!a->grid,p);icon_button(a,a->grid_button,3,a->grid,p);
    icon_button(a,a->new_folder,0,false,p);icon(a,7,(SDL_FRect){a->new_folder.x+14*s,a->new_folder.y+13*s,18*s,18*s},p.text);
    icon_button(a,a->new_file,1,false,p);icon(a,7,(SDL_FRect){a->new_file.x+14*s,a->new_file.y+13*s,18*s,18*s},p.text);
    rounded(a,a->search,5*s,a->query_focus ? p.selection : p.raised);
    if (a->query_focus) outline(a,a->search,a->query_bad ? 0xffbdad : p.focus);
    text(a,*ae_files_query(a->files) ? ae_files_query(a->files) : tr(a,"Search this folder","Ordner durchsuchen"),a->search.x+9*s,a->search.y+8*s,a->search.w-18*s,11,p.muted);
    icon_button(a,a->details_button,8,l->inspector_visible || a->info_overlay,p);
    if (!l->compact) {
        icon(a,0,(SDL_FRect){l->heading.x,l->heading.y+12*s,40*s,40*s},p.icon);
        text(a,folder_label(a),l->heading.x+48*s,l->heading.y+13*s,l->heading.w-48*s,17,p.text);
        char count_label[64];snprintf(count_label,sizeof(count_label),"%zu / %zu",ae_files_count(a->files),ae_files_total(a->files));
        text(a,count_label,l->heading.x+48*s,l->heading.y+37*s,l->heading.w-48*s,10,p.muted);
        if (!a->grid) { text(a,tr(a,"Name","Name"),l->content.x+10*s,l->content.y-19*s,l->content.w-20*s,10,p.muted);line(a,l->content.x,l->content.y-3*s,l->content.x+l->content.w,l->content.y-3*s,p.line); }
    } else text(a,folder_label(a),l->heading.x,l->heading.y+7*s,l->heading.w,12,p.text);
    for (size_t i=0;i<a->rows && a->first+i<visual_count;++i) {
        size_t index=a->first+i;const struct ae_files_entry *e=ae_files_entry(a->files,index);struct ae_files_rect cell;if (!ae_files_layout_cell(l,i,&cell)) break;
        SDL_FRect r=rect(cell);bool selected=index==(naming ? naming_index : ae_files_selected(a->files));
        rounded(a,r,6*s,selected ? p.selection : a->grid ? p.raised : p.canvas);
        if (selected) outline(a,r,a->query_focus ? p.line : p.focus);
        float ix=r.x+6*s,iy=r.y+(a->grid && !l->compact_cards ? 10 : 4)*s,iw=(a->grid && !l->compact_cards ? 58 : 36)*s;
        icon(a,(e ? e->kind==AE_FILES_DIRECTORY : a->create_kind==AE_FILES_CREATE_DIRECTORY) ? 0u : 1u,(SDL_FRect){ix,iy,iw,iw},p.icon);
        if (naming && index==naming_index) { draw_inline(a,r,p);continue; }
        if (!e) continue;
        if (a->grid && !l->compact_cards) {
            text(a,e->label,r.x+12*s,r.y+78*s,r.w-24*s,12,p.text);
            line(a,r.x+12*s,r.y+108*s,r.x+r.w-12*s,r.y+108*s,p.line);
            text(a,kind_label(a,e),r.x+12*s,r.y+119*s,r.w-24*s,10,p.muted);
        } else {
            text(a,e->label,r.x+48*s,r.y+5*s,r.w-58*s,12,p.text);
            char detail[96];snprintf(detail,sizeof(detail),"%s%s%llu%s",kind_label(a,e),e->kind==AE_FILES_REGULAR ? " · " : "",e->kind==AE_FILES_REGULAR ? (unsigned long long)e->size : 0,e->kind==AE_FILES_REGULAR ? " bytes" : "");
            text(a,e->kind==AE_FILES_REGULAR ? detail : kind_label(a,e),r.x+48*s,r.y+24*s,r.w-58*s,10,p.muted);
        }
    }
    if (!visual_count) text(a,*ae_files_query(a->files) ? tr(a,"No matching entries","Keine passenden Einträge") : tr(a,"This folder is empty","Dieser Ordner ist leer"),l->content.x+8*s,l->content.y+12*s,l->content.w-16*s,12,p.muted);
    if (l->inspector_visible) draw_information(a,rect(l->inspector),p);
    fill(a,rect(l->status),p.surface);line(a,0,l->status.y,(float)w,l->status.y,p.line);
    a->hidden=(SDL_FRect){(float)w-110*s,l->status.y+2*s,104*s,24*s};
    text(a,a->show_hidden ? tr(a,"Hidden: on","Versteckt: an") : tr(a,"Hidden: off","Versteckt: aus"),a->hidden.x+4*s,a->hidden.y+5*s,a->hidden.w-8*s,10,p.muted);
    const char *status=unresolved(a) ? tr(a,"File operation outcome unknown; do not retry","Ergebnis der Dateiaktion unklar; nicht wiederholen") : naming ? *a->create_message ? a->create_message : tr(a,"Enter confirm · Tab focus · Escape cancel","Enter bestätigen · Tab Fokus · Escape abbrechen") : *a->message ? a->message : a->clip_dir>=0 ? tr(a,"Move selected · Ctrl+V preview","Verschieben vorgemerkt · Strg+V Vorschau") : tr(a,"Ctrl+1/2 view · F9 details · F10 actions","Strg+1/2 Ansicht · F9 Details · F10 Aktionen");
    text(a,status,14*s,l->status.y+7*s,a->hidden.x-28*s,10,p.muted);
    if (a->dialog && !naming) draw_dialog(a,w,h,p.text);
    else if (a->menu) draw_menu(a,w,h,p.text);
    else if (a->info_overlay) {
        float width=fminf(300*s,(float)w-32*s);SDL_FRect r={((float)w-width)/2,8*s,width,(float)h-16*s};
        a->info_rect=r;draw_information(a,r,p);outline(a,r,p.focus);a->info_close=(SDL_FRect){r.x+r.w-32*s,r.y+2*s,30*s,30*s};icon(a,5,a->info_close,p.text);
    }
    if (!SDL_RenderPresent(a->renderer)) fail(a,"present");
    a->dirty=false;++a->frames;controls(a);
    if (a->diagnostics) printf("VIEW frame=%u count=%zu total=%zu selected=%zu top=%zu rows=%zu depth=%u scale=%.2f german=%d contrast=%d filter=%d rejected=%d child=%d width=%d height=%d dialog=%d name_bytes=%zu name_bad=%d name_focus=%u creating=%d unknown=%d renaming=%d stale=%d moving=%d clipboard=%d menu=%d menu_focus=%u grid=%d columns=%zu sidebar=%d inspector=%d info_overlay=%d inline=%d fullscreen=%d\n",a->frames,ae_files_count(a->files),ae_files_total(a->files),ae_files_selected(a->files),a->first,a->rows,ae_files_depth(a->files),(double)a->scale,a->german,a->contrast,a->query_focus,a->query_bad,a->child.pid,w,h,a->dialog,a->name_size,a->name_bad,a->name_focus,creating(a),unresolved(a),a->renaming,a->rename_stale,a->moving,a->clip_dir>=0,a->menu,a->menu_focus,a->grid,a->layout.columns,a->layout.sidebar_visible,a->layout.inspector_visible,a->info_overlay,inline_edit(a),(SDL_GetWindowFlags(a->window)&SDL_WINDOW_FULLSCREEN)!=0);
}
static bool inside(SDL_FRect r,float x,float y) { return r.w>0 && x>=r.x && y>=r.y && x<r.x+r.w && y<r.y+r.h; }
static unsigned target(struct app *a,float x,float y,size_t *row) {
    *row=0;
    if (a->dialog) { *row=0;if (inside(a->name_field,x,y)) return 7;if (inside(a->cancel,x,y)) return 8;if (inside(a->submit,x,y)) return 9;return 0; }
    if (a->info_overlay) return inside(a->info_close,x,y) ? 16u : inside(a->info_rect,x,y) ? 19u : 0u;
    if (a->menu) { for (unsigned i=0;i<3;++i) if (inside(a->menu_rows[i],x,y)) return 30+i;return 0; }
    if (inside(a->new_file,x,y)) return 5;
    if (inside(a->new_folder,x,y)) return 6;
    if (inside(a->more,x,y)) return 11;
    if (inside(a->list_button,x,y)) return 12;
    if (inside(a->grid_button,x,y)) return 13;
    if (inside(a->details_button,x,y)) return 14;
    if (inside(a->close_button,x,y)) return 17;
    if (inside(a->fullscreen_button,x,y)) return 18;
    for (size_t i=0;i<a->place_count;++i) if (inside(a->places[i],x,y)) { *row=i;return 20; }
    *row=0;if (inside(a->back,x,y)) return 1;if (inside(a->hidden,x,y)) return 2;if (inside(a->search,x,y)) return 3;
    for (size_t i=0;i<a->rows && a->first+i<ae_files_count(a->files);++i) { struct ae_files_rect r;if (ae_files_layout_cell(&a->layout,i,&r) && inside(rect(r),x,y)) { *row=a->first+i;return 4; } }return 0;
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
    return (float)p->y<30*a->scale && !a->dialog && !a->menu && !a->info_overlay && !inside(a->close_button,(float)p->x,(float)p->y) && !inside(a->fullscreen_button,(float)p->x,(float)p->y) && !inside(a->more,(float)p->x,(float)p->y) ? SDL_HITTEST_DRAGGABLE : SDL_HITTEST_NORMAL;
}
static void toggle_details(struct app *a) {
    if (a->details && (a->layout.inspector_visible || a->info_overlay)) { a->details=false;a->info_overlay=false; }
    else { int w,h;struct ae_files_layout candidate;
        if (!SDL_GetWindowSize(a->window,&w,&h) || !ae_files_layout_make(w,h,a->scale,a->grid,true,a->sidebar,false,&candidate)) { fail(a,"information layout");return; }
        a->details=true;a->info_overlay=!candidate.inspector_visible;
    }a->pressed=0;a->dirty=true;
}
static void fullscreen(struct app *a) {
    if (!SDL_SetWindowFullscreen(a->window,(SDL_GetWindowFlags(a->window)&SDL_WINDOW_FULLSCREEN)==0)) fail(a,"fullscreen");
}
static void navigate_place(struct app *a,size_t index) {
    if (index>=a->place_count) return;
    if (a->place_snapshot[index]==SIZE_MAX) { result(a,ae_files_ancestor(a->files,a->place_depth[index]));return; }
    if (ae_files_depth(a->files)) return;
    const struct ae_files_entry *e=ae_files_snapshot_entry(a->files,a->place_snapshot[index]);if (!e || e->kind!=AE_FILES_DIRECTORY) return;
    char name[AE_FILES_NAME+1];memcpy(name,e->name,strlen(e->name)+1);dev_t device=e->device;ino_t inode=e->inode;
    if (!ae_files_filter(a->files,"",0)) return;
    a->query_bad=false;a->query_focus=false;
    for (size_t i=0;i<ae_files_count(a->files);++i) { e=ae_files_entry(a->files,i);
        if (!strcmp(e->name,name) && e->device==device && e->inode==inode) { (void)ae_files_select(a->files,i);result(a,ae_files_enter(a->files));return; }
    }result(a,AE_FILES_CHANGED);
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
        a->name_bad=false;a->composing=false;*a->name=0;a->name_size=0;*a->create_message=0;a->dirty=true;
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
        else if (a->info_overlay) { if (k==SDLK_ESCAPE || k==SDLK_F9) { a->info_overlay=false;a->details=false;a->dirty=true; } }
        else if (a->menu) menu_key(a,k,mod);
        else if (k==SDLK_F9) toggle_details(a);
        else if (k==SDLK_F4) { a->sidebar=!a->sidebar;a->dirty=true; }
        else if (k==SDLK_F11) fullscreen(a);
        else if ((mod&SDL_KMOD_CTRL) && (k==SDLK_1 || k==SDLK_2)) { a->grid=k==SDLK_2;a->dirty=true; }
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
        else if (k==SDLK_UP || k==SDLK_DOWN) move(a,(a->grid ? (int)a->layout.columns : 1)*(k==SDLK_UP ? -1 : 1));
        else if (a->grid && (k==SDLK_LEFT || k==SDLK_RIGHT)) move(a,k==SDLK_LEFT ? -1 : 1);
        else if (k==SDLK_PAGEUP || k==SDLK_PAGEDOWN) move(a,(int)(a->rows ? a->rows : 1)*(k==SDLK_PAGEUP ? -1 : 1));
        else if (k==SDLK_HOME || k==SDLK_END) { if (ae_files_count(a->files)) (void)ae_files_select(a->files,k==SDLK_HOME ? 0 : ae_files_count(a->files)-1);reveal(a);a->dirty=true; }
        else if (k==SDLK_RETURN) { a->query_focus=false;if (a->rows) activate(a); }
        else if (k==SDLK_BACKSPACE) { if (a->query_focus) { const char *q=ae_files_query(a->files);size_t n=strlen(q),points[AE_FILES_QUERY+1],count;if (!a->query_bad && n && ae_text_graphemes(q,n,points,AE_FILES_QUERY+1,&count)) (void)ae_files_filter(a->files,q,points[count-2]);a->dirty=true; }else result(a,ae_files_up(a->files)); }
    } else if (e->type==SDL_EVENT_TEXT_INPUT && a->dialog) { a->pressed=0;a->composing=false;name_input(a,e->text.text); }
    else if (e->type==SDL_EVENT_TEXT_EDITING && a->dialog) { a->pressed=0;a->composing=e->edit.text && *e->edit.text;a->dirty=true; }
    else if (e->type==SDL_EVENT_TEXT_INPUT && !a->menu && !a->info_overlay && a->query_focus) {
        a->pressed=0;if (a->query_bad) return;
        char q[AE_FILES_QUERY+1];size_t old=strlen(ae_files_query(a->files)),n=strlen(e->text.text);
        if (n<=AE_FILES_QUERY-old) { memcpy(q,ae_files_query(a->files),old);memcpy(q+old,e->text.text,n);if (ae_files_filter(a->files,q,old+n)) { a->first=0;a->dirty=true; }else { a->query_bad=true;message(a,tr(a,"Filter rejected; Ctrl+L clears","Filter abgewiesen; Strg+L löscht")); } }else { a->query_bad=true;message(a,tr(a,"Filter rejected; Ctrl+L clears","Filter abgewiesen; Strg+L löscht")); }
    } else if (e->type==SDL_EVENT_MOUSE_WHEEL) { a->pressed=0;if (!a->dialog && !a->menu && !a->info_overlay) move(a,(a->grid ? (int)a->layout.columns : 1)*(e->wheel.y>0 ? -3 : e->wheel.y<0 ? 3 : 0)); }
    else if (e->type==SDL_EVENT_MOUSE_BUTTON_DOWN && e->button.button==SDL_BUTTON_RIGHT) {
        a->pressed=0;
        if (!a->dialog && !a->info_overlay && !creating(a) && !a->closing) {
            if (a->menu) { a->menu=false;a->dirty=true; }
            else { size_t row;unsigned hit=target(a,e->button.x,e->button.y,&row);bool selected=hit==4 && ae_files_select(a->files,row);menu_open(a,e->button.x,e->button.y,selected); }
        }
    }
    else if (e->type==SDL_EVENT_MOUSE_BUTTON_DOWN && e->button.button==SDL_BUTTON_LEFT) {
        a->consume_release=false;
        a->pressed=target(a,e->button.x,e->button.y,&a->pressed_row);
        if (inline_edit(a) && !a->pressed) { create_submit(a);a->consume_release=true; }
    }
    else if (e->type==SDL_EVENT_MOUSE_BUTTON_UP && e->button.button==SDL_BUTTON_LEFT) {
        if (a->consume_release) { a->consume_release=false;a->pressed=0;return; }
        size_t row;unsigned hit=target(a,e->button.x,e->button.y,&row),pressed=a->pressed;a->pressed=0;if (hit!=pressed) return;
        if (a->dialog) {
            if (!creating(a)) { if (hit==7 && !unresolved(a)) { a->name_focus=0;a->dirty=true; }else if (hit==8) dialog_close(a);else if (hit==9) { a->name_focus=2;create_submit(a);a->dirty=true; } }
        } else if (a->info_overlay) { if (hit==0 || hit==16) { a->info_overlay=false;a->details=false;a->dirty=true; } }
        else if (a->menu) { if (hit>=30 && hit<33) menu_action(a,hit-30);else { a->menu=false;a->dirty=true; } }
        else if (hit==1) result(a,ae_files_up(a->files));
        else if (hit==2) { a->show_hidden=!a->show_hidden;ae_files_hidden(a->files,a->show_hidden);reveal(a);a->dirty=true; }
        else if (hit==3) { a->query_focus=true;a->dirty=true; }
        else if (hit==4 && row==a->pressed_row && ae_files_select(a->files,row)) { a->query_focus=false;a->dirty=true;if (e->button.clicks==2) activate(a); }
        else if (hit==5 || hit==6) dialog_open(a,hit==5 ? AE_FILES_CREATE_FILE : AE_FILES_CREATE_DIRECTORY);
        else if (hit==11) menu_open(a,a->more.x,a->more.y+a->more.h+4,true);
        else if (hit==12 || hit==13) { a->grid=hit==13;a->dirty=true; }
        else if (hit==14) toggle_details(a);
        else if (hit==17) request_close(a);
        else if (hit==18) fullscreen(a);
        else if (hit==20 && row==a->pressed_row) navigate_place(a,row);
    } else if (e->type==SDL_EVENT_WINDOW_FOCUS_LOST) { a->pressed=0;a->consume_release=false;if (a->menu) { a->menu=false;a->dirty=true; }
        if (a->dialog && a->composing) { a->composing=false;a->name_bad=true;snprintf(a->create_message,sizeof(a->create_message),"%s",tr(a,"Composition interrupted; Ctrl+U clears the name","Texteingabe unterbrochen; Strg+U löscht den Namen"));a->dirty=true; }
    }
    else if (e->type==SDL_EVENT_WINDOW_EXPOSED || e->type==SDL_EVENT_WINDOW_RESIZED || e->type==SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) { a->pressed=0;a->menu=false;a->dirty=true; }
}
int main(int argc,char **argv) {
    struct app a={.child={.pid=-1,.pin=-1},.wake={-1,-1},.create_dir=-1,.rename_witness=-1,.move_target=-1,.clip_dir=-1,.clip_witness=-1,.scale=1,.running=true,.dirty=true,.details=true,.sidebar=true};const char *root=NULL,*font=NULL;
    atomic_init(&a.create_done,false);
    for (int i=1;i<argc;++i) {
        const char **slot=!strcmp(argv[i],"--root") ? &root : !strcmp(argv[i],"--font") ? &font : !strcmp(argv[i],"--cache") ? &a.cache : !strcmp(argv[i],"--terminal") ? &a.terminal : !strcmp(argv[i],"--editor") ? &a.editor : !strcmp(argv[i],"--terminal-font") ? &a.terminal_font : NULL;
        if (slot && !*slot && i+1<argc) *slot=argv[++i];else if (!strcmp(argv[i],"--diagnostics") && !a.diagnostics) a.diagnostics=true;else return 2;
    }
    struct stat st;if (!root || !font || !a.cache || !a.terminal || !a.editor || !a.terminal_font || root[0]!='/' || getuid()==0 || getuid()!=geteuid() || getgid()!=getegid() || lstat(a.cache,&st) || !S_ISDIR(st.st_mode) || st.st_uid!=getuid() || (st.st_mode&07777)!=0700) return 2;
    setvbuf(stdout,NULL,_IOLBF,0);int fd=open(root,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);enum ae_files_result r;a.files=fd<0 ? NULL : ae_files_create(fd,&r);if (fd>=0) close(fd);if (!a.files) return 1;
    const char *end=root+strlen(root);while (end>root+1 && end[-1]=='/') --end;const char *leaf=end;while (leaf>root && leaf[-1]!='/') --leaf;
    if (leaf==end) snprintf(a.root_label,sizeof(a.root_label),"%s",tr(&a,"Filesystem","Dateisystem"));
    else if (!ae_files_format_name(leaf,(size_t)(end-leaf),a.root_label)) { ae_files_destroy(a.files);return 2; }
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
