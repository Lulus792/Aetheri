#define _GNU_SOURCE
#include "session.h"
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

void ae_files_session_init(struct ae_files_session *a,ae_files_work_start start,ae_files_work_join join,void *context) {
    *a=(struct ae_files_session){.child={.pid=-1,.pin=-1},.create_dir=-1,.rename_witness=-1,
        .move_target=-1,.clip_dir=-1,.clip_witness=-1,.scale=1,.running=true,.dirty=true,.details=true,.sidebar=true,
        .dialog_serial=1,.work_start=start,.work_join=join,.work_context=context};
    atomic_init(&a->create_done,false);
}
/* This entry belongs only to a successful original adapter start, once. */
void ae_files_session_apply_worker(struct ae_files_session *a) {
    if (a->renaming) (void)ae_files_rename_apply(a->rename_op);else (void)ae_files_create_apply(a->creation);
    atomic_store_explicit(&a->create_done,true,memory_order_release);
}
static bool start_worker(struct ae_files_session *a) {
    if (!a->work_start || !a->work_join) return false;
    a->work_active=true;
    if (a->work_start(a->work_context,a)) return true;
    a->work_active=false;return false;
}
void ae_files_session_join_worker(struct ae_files_session *a) {
    if (!a->work_active) return;
    a->work_join(a->work_context);a->work_active=false;
}
bool ae_files_session_busy(const struct ae_files_session *a) { return a->child.pid>0 && !a->child.ended; }
bool ae_files_session_creating(const struct ae_files_session *a) { return a->work_active; }
bool ae_files_session_unresolved(const struct ae_files_session *a) { return a->unknown!=NULL || a->rename_unknown!=NULL; }
bool ae_files_session_inline_edit(const struct ae_files_session *a) { return a->dialog && !a->moving && !ae_files_session_unresolved(a) && !a->rename_stale; }
const char *ae_files_session_tr(const struct ae_files_session *a,const char *en,const char *de) { return a->german ? de : en; }
void ae_files_session_message(struct ae_files_session *a,const char *s) { snprintf(a->message,sizeof(a->message),"%s",s);a->dirty=true; }
void ae_files_session_result(struct ae_files_session *a,enum ae_files_result r) {
    a->pressed=0;
    const char *s=r==AE_FILES_OK ? "" : r==AE_FILES_ROOT ? ae_files_session_tr(a,"At the selected root","Am gewählten Ausgangsordner") : r==AE_FILES_CHANGED ? ae_files_session_tr(a,"Folder or file changed; Ctrl+R refreshes","Ordner oder Datei geändert; Strg+R aktualisiert") : r==AE_FILES_LIMIT ? ae_files_session_tr(a,"Folder/depth limit; view retained","Ordner-/Tiefenlimit; Ansicht bleibt erhalten") : r==AE_FILES_NOMEM ? ae_files_session_tr(a,"Not enough memory; view retained","Nicht genug Speicher; Ansicht bleibt erhalten") : r==AE_FILES_UNSUPPORTED ? ae_files_session_tr(a,"This entry cannot be opened here","Dieser Eintrag lässt sich hier nicht öffnen") : ae_files_session_tr(a,"Cannot read this folder","Ordner kann nicht gelesen werden");
    ae_files_session_message(a,s);if (r==AE_FILES_OK) { a->first=0;if (!*ae_files_query(a->files)) a->query_bad=false; }
}
void ae_files_session_reveal(struct ae_files_session *a) {
    if (a->layout.capacity) a->first=ae_files_layout_reveal(&a->layout,ae_files_count(a->files),ae_files_selected(a->files),a->first);
}
void ae_files_session_move(struct ae_files_session *a,int amount) {
    size_t count=ae_files_count(a->files),old=ae_files_selected(a->files);if (!count) return;
    size_t step=amount<0 ? (size_t)(-(int64_t)amount) : (size_t)amount;
    size_t n=amount<0 ? step>old ? 0 : old-step : step>=count-old ? count-1 : old+step;
    (void)ae_files_select(a->files,n);a->query_focus=false;ae_files_session_reveal(a);a->dirty=true;
}
void ae_files_session_clipboard_clear(struct ae_files_session *a) {
    if (a->clip_dir>=0) close(a->clip_dir);
    if (a->clip_witness>=0) close(a->clip_witness);
    a->clip_dir=-1;a->clip_witness=-1;a->dirty=true;
}
void ae_files_session_dialog_close(struct ae_files_session *a) {
    if (ae_files_session_creating(a)) return;
    if (a->dialog && a->dialog_serial<UINT64_MAX) ++a->dialog_serial;
    if (a->create_dir>=0) close(a->create_dir);
    if (a->rename_witness>=0) close(a->rename_witness);
    if (a->move_target>=0) close(a->move_target);
    a->move_target=-1;
    a->rename_witness=-1;
    a->create_dir=-1;a->dialog=false;a->pressed=0;a->dirty=true;
}
void ae_files_session_request_close(struct ae_files_session *a) {
    a->closing=true;if (ae_files_session_creating(a)) ae_files_session_message(a,ae_files_session_tr(a,"Waiting for the file operation result","Warten auf das Ergebnis der Dateiaktion"));
    else { ae_files_session_dialog_close(a);if (ae_files_session_busy(a)) ae_files_session_message(a,ae_files_session_tr(a,"Close the document window first; Files is waiting","Zuerst das Dokumentfenster schließen; Dateien wartet"));else a->running=false; }
}
void ae_files_session_dialog_open(struct ae_files_session *a,enum ae_files_create_kind kind) {
    if (a->closing || ae_files_session_creating(a) || a->dialog_serial==UINT64_MAX) return;
    a->pressed=0;a->query_focus=false;a->name_focus=0;a->name_bad=false;a->composing=false;*a->create_message=0;
    if (ae_files_session_unresolved(a)) {
        a->renaming=a->rename_unknown!=NULL;a->dialog=true;a->name_focus=1;snprintf(a->name,sizeof(a->name),"%s",a->renaming ? ae_files_rename_new(a->rename_unknown) : ae_files_create_leaf(a->unknown));a->name_size=strlen(a->name);
        snprintf(a->create_message,sizeof(a->create_message),"%s",ae_files_session_tr(a,"Outcome unknown. Inspect; do not retry.","Ergebnis unklar. Prüfen; nicht wiederholen."));
    } else {
        if (a->creations>=256) { ae_files_session_message(a,ae_files_session_tr(a,"Creation limit reached; reopen Files","Limit für das Anlegen erreicht; Dateien neu öffnen"));return; }
        int fd=ae_files_directory(a->files);if (fd<0) { ae_files_session_message(a,ae_files_session_tr(a,"Cannot retain the target folder","Zielordner kann nicht festgehalten werden"));return; }
        if (a->create_dir>=0) close(a->create_dir);
        a->renaming=false;a->moving=false;a->rename_stale=false;a->create_dir=fd;a->create_kind=kind;a->name_size=0;*a->name=0;
        snprintf(a->folder,sizeof(a->folder),"%s",ae_files_folder(a->files));a->dialog=true;
    }
    ++a->dialog_serial;a->dirty=true;
}
void ae_files_session_rename_open(struct ae_files_session *a) {
    if (a->closing || ae_files_session_creating(a) || a->dialog_serial==UINT64_MAX) return;
    if (ae_files_session_unresolved(a)) { ae_files_session_dialog_open(a,AE_FILES_CREATE_FILE);return; }
    if (a->child.pid>0) { ae_files_session_message(a,ae_files_session_tr(a,"Close the document before renaming","Dokument vor dem Umbenennen schließen"));return; }
    if (a->query_bad || !a->rows) return;
    if (a->renames>=256) { ae_files_session_message(a,ae_files_session_tr(a,"Rename limit reached; reopen Files","Umbenennen-Limit erreicht; Dateien neu öffnen"));return; }
    const struct ae_files_entry *e=ae_files_entry(a->files,ae_files_selected(a->files));if (!e) return;
    enum ae_files_result r=ae_files_hold_selected(a->files,&a->create_dir,&a->rename_witness,a->original);
    if (r!=AE_FILES_OK) { ae_files_session_result(a,r);return; }
    snprintf(a->original_label,sizeof(a->original_label),"%s",e->label);snprintf(a->folder,sizeof(a->folder),"%s",ae_files_folder(a->files));
    a->name_size=ae_files_create_name(a->original,strlen(a->original)) ? strlen(a->original) : 0;
    memcpy(a->name,a->original,a->name_size);a->name[a->name_size]=0;
    a->renaming=true;a->moving=false;a->rename_stale=false;a->dialog=true;a->name_bad=false;a->name_focus=0;a->query_focus=false;a->pressed=0;a->composing=false;*a->create_message=0;++a->dialog_serial;a->dirty=true;
}
void ae_files_session_cut_selected(struct ae_files_session *a) {
    if (a->closing || ae_files_session_creating(a) || ae_files_session_unresolved(a) || a->query_bad || !a->rows || a->child.pid>0) {
        ae_files_session_message(a,a->child.pid>0 ? ae_files_session_tr(a,"Close the document before selecting a move","Dokument vor der Verschiebeauswahl schließen") : ae_files_session_tr(a,"Move selection is unavailable here","Verschiebeauswahl hier nicht verfügbar"));return;
    }
    const struct ae_files_entry *e=ae_files_entry(a->files,ae_files_selected(a->files));if (!e) return;
    int dir=-1,witness=-1;char leaf[AE_FILES_NAME+1];enum ae_files_result r=ae_files_hold_selected(a->files,&dir,&witness,leaf);
    if (r!=AE_FILES_OK) { ae_files_session_result(a,r);return; }
    ae_files_session_clipboard_clear(a);a->clip_dir=dir;a->clip_witness=witness;
    memcpy(a->clip_name,leaf,strlen(leaf)+1);snprintf(a->clip_label,sizeof(a->clip_label),"%s",e->label);snprintf(a->clip_folder,sizeof(a->clip_folder),"%s",ae_files_folder(a->files));
    a->query_focus=false;ae_files_session_message(a,ae_files_session_tr(a,"Move selected; navigate to the target and press Ctrl+V","Verschieben vorgemerkt; Ziel öffnen und Strg+V drücken"));
}
void ae_files_session_paste_open(struct ae_files_session *a) {
    if (a->closing || ae_files_session_creating(a) || a->dialog_serial==UINT64_MAX) return;
    if (ae_files_session_unresolved(a)) { ae_files_session_dialog_open(a,AE_FILES_CREATE_FILE);return; }
    if (a->child.pid>0) { ae_files_session_message(a,ae_files_session_tr(a,"Close the document before moving","Dokument vor dem Verschieben schließen"));return; }
    if (a->clip_dir<0 || a->clip_witness<0) { ae_files_session_message(a,ae_files_session_tr(a,"Select a file or folder with Ctrl+X first","Zuerst Datei oder Ordner mit Strg+X auswählen"));return; }
    if (a->moves>=256) { ae_files_session_message(a,ae_files_session_tr(a,"Move limit reached; reopen Files","Verschieben-Limit erreicht; Dateien neu öffnen"));return; }
    int dir=fcntl(a->clip_dir,F_DUPFD_CLOEXEC,4),witness=fcntl(a->clip_witness,F_DUPFD_CLOEXEC,4),target=ae_files_directory(a->files);
    if (dir<0 || witness<0 || target<0) { if (dir>=0) close(dir);if (witness>=0) close(witness);if (target>=0) close(target);ae_files_session_message(a,ae_files_session_tr(a,"Cannot retain the move inputs","Verschiebeauswahl kann nicht festgehalten werden"));return; }
    a->create_dir=dir;a->rename_witness=witness;a->move_target=target;a->moving=true;a->renaming=true;a->rename_stale=false;
    memcpy(a->original,a->clip_name,strlen(a->clip_name)+1);snprintf(a->original_label,sizeof(a->original_label),"%s",a->clip_label);
    snprintf(a->from_folder,sizeof(a->from_folder),"%s",a->clip_folder);snprintf(a->folder,sizeof(a->folder),"%s",ae_files_folder(a->files));
    a->name_size=ae_files_create_name(a->original,strlen(a->original)) ? strlen(a->original) : 0;memcpy(a->name,a->original,a->name_size);a->name[a->name_size]=0;
    a->dialog=true;a->name_bad=false;a->name_focus=0;a->query_focus=false;a->pressed=0;a->composing=false;*a->create_message=0;++a->dialog_serial;a->dirty=true;
}
void ae_files_session_rename_submit(struct ae_files_session *a) {
    if (a->closing || ae_files_session_creating(a) || ae_files_session_unresolved(a) || a->rename_stale || a->name_bad || !ae_files_create_name(a->name,a->name_size) || (!a->moving && !strcmp(a->original,a->name))) return;
    enum ae_files_rename_result r;a->rename_op=a->moving ? ae_files_move_prepare(a->create_dir,a->rename_witness,a->original,strlen(a->original),a->move_target,a->name,a->name_size,&r) : ae_files_rename_prepare(a->create_dir,a->rename_witness,a->original,strlen(a->original),a->name,a->name_size,&r);
    if (!a->rename_op) {
        if (r==AE_FILES_RENAME_CHANGED || r==AE_FILES_RENAME_OVERLAP || r==AE_FILES_RENAME_LIMIT) a->rename_stale=true;
        if (r==AE_FILES_RENAME_CHANGED && a->moving) ae_files_session_clipboard_clear(a);
        snprintf(a->create_message,sizeof(a->create_message),"%s",r==AE_FILES_RENAME_CHANGED ? ae_files_session_tr(a,"Source changed. Cancel and refresh.","Quelle geändert. Abbrechen und aktualisieren.") : r==AE_FILES_RENAME_OVERLAP ? ae_files_session_tr(a,"Cannot move a folder into itself or a child","Ordner nicht in sich oder einen Unterordner verschieben") : r==AE_FILES_RENAME_LIMIT ? ae_files_session_tr(a,"Target ancestry limit; choose another folder","Ziel-Tiefenlimit; anderen Ordner wählen") : r==AE_FILES_RENAME_DENIED ? ae_files_session_tr(a,"Private writable folders and owned entry required","Private Ordner und eigener Eintrag nötig") : ae_files_session_tr(a,"Operation not prepared; nothing changed","Aktion nicht vorbereitet; nichts geändert"));a->dirty=true;return;
    }
    atomic_store(&a->create_done,false);a->work_active=start_worker(a);
    if (!a->work_active) { ae_files_rename_close(a->rename_op);a->rename_op=NULL;snprintf(a->create_message,sizeof(a->create_message),"%s",ae_files_session_tr(a,"Worker unavailable; nothing changed","Worker nicht verfügbar; nichts geändert")); }
    else { if (a->moving) ++a->moves;else ++a->renames;snprintf(a->create_message,sizeof(a->create_message),"%s",a->moving ? ae_files_session_tr(a,"Moving… Please wait","Wird verschoben… Bitte warten") : ae_files_session_tr(a,"Renaming… Please wait","Wird umbenannt… Bitte warten")); }
    a->dirty=true;
}
void ae_files_session_create_submit(struct ae_files_session *a) {
    if (a->composing) return;
    if (ae_files_session_inline_edit(a) && a->renaming && !ae_files_session_creating(a) && !a->closing && !a->name_bad &&
        ae_files_create_name(a->name,a->name_size) && !strcmp(a->name,a->original)) { ae_files_session_dialog_close(a);return; }
    if (a->renaming) { ae_files_session_rename_submit(a);return; }
    if (a->closing || ae_files_session_creating(a) || ae_files_session_unresolved(a) || a->name_bad || !ae_files_create_name(a->name,a->name_size)) return;
    enum ae_files_create_result r;a->creation=ae_files_create_prepare(a->create_dir,a->create_kind,a->name,a->name_size,&r);
    if (!a->creation) {
        snprintf(a->create_message,sizeof(a->create_message),"%s",r==AE_FILES_CREATE_DENIED ? ae_files_session_tr(a,"A private writable folder is required","Ein privater beschreibbarer Ordner ist nötig") : r==AE_FILES_CREATE_NOMEM ? ae_files_session_tr(a,"Not enough memory; nothing created","Nicht genug Speicher; nichts angelegt") : ae_files_session_tr(a,"Cannot prepare creation; nothing created","Anlegen nicht vorbereitet; nichts angelegt"));a->dirty=true;return;
    }
    atomic_store(&a->create_done,false);a->work_active=start_worker(a);
    if (!a->work_active) { ae_files_create_close(a->creation);a->creation=NULL;snprintf(a->create_message,sizeof(a->create_message),"%s",ae_files_session_tr(a,"Worker unavailable; nothing created","Worker nicht verfügbar; nichts angelegt")); }
    else { ++a->creations;snprintf(a->create_message,sizeof(a->create_message),"%s",ae_files_session_tr(a,"Creating… Please wait","Wird angelegt… Bitte warten")); }
    a->dirty=true;
}
void ae_files_session_rename_update(struct ae_files_session *a) {
    enum ae_files_rename_result r=ae_files_rename_status(a->rename_op);
    if (a->diagnostics) printf("%s attempt=%u result=%u\n",a->moving ? "MOVE" : "RENAME",a->moving ? a->moves : a->renames,(unsigned)r);
    if (r==AE_FILES_RENAME_DONE) {
        ae_files_session_dialog_close(a);a->query_bad=false;(void)ae_files_filter(a->files,"",0);
        if (a->name[0]=='.') { a->show_hidden=true;ae_files_hidden(a->files,true); }
        if (ae_files_refresh(a->files)==AE_FILES_OK) {
            for (size_t i=0;i<ae_files_count(a->files);++i) if (!strcmp(ae_files_entry(a->files,i)->name,a->name)) { (void)ae_files_select(a->files,i);break; }
            ae_files_session_reveal(a);ae_files_session_message(a,a->moving ? ae_files_session_tr(a,"Moved","Verschoben") : ae_files_session_tr(a,"Renamed","Umbenannt"));
        } else ae_files_session_message(a,ae_files_session_tr(a,"Completed; refresh failed. Do not repeat.","Abgeschlossen; Ansicht nicht aktualisiert. Nicht wiederholen."));
        if (a->moving) ae_files_session_clipboard_clear(a);
    } else if (r==AE_FILES_RENAME_UNKNOWN) {
        a->rename_unknown=a->rename_op;a->rename_op=NULL;a->name_focus=1;
        snprintf(a->create_message,sizeof(a->create_message),"%s",ae_files_session_tr(a,"Outcome unknown. Inspect both names.","Ergebnis unklar. Beide Namen prüfen."));
        ae_files_session_message(a,a->moving ? ae_files_session_tr(a,"Move outcome unknown; new mutations blocked","Verschieben unklar; neue Dateiänderungen gesperrt") : ae_files_session_tr(a,"Rename outcome unknown; creation and renaming blocked","Umbenennen unklar; Anlegen und Umbenennen gesperrt"));
    } else {
        if (r==AE_FILES_RENAME_CHANGED || r==AE_FILES_RENAME_OVERLAP || r==AE_FILES_RENAME_LIMIT) a->rename_stale=true;
        if (r==AE_FILES_RENAME_CHANGED && a->moving) ae_files_session_clipboard_clear(a);
        snprintf(a->create_message,sizeof(a->create_message),"%s",r==AE_FILES_RENAME_CONFLICT ? ae_files_session_tr(a,"Name already exists. Choose another name.","Name bereits vorhanden. Anderen Namen wählen.") : r==AE_FILES_RENAME_CHANGED ? ae_files_session_tr(a,"Source changed. Cancel and refresh.","Quelle geändert. Abbrechen und aktualisieren.") : ae_files_session_tr(a,"Not renamed. Inspect the source.","Nicht umbenannt. Quelle prüfen."));
    }
    ae_files_rename_close(a->rename_op);a->rename_op=NULL;a->dirty=true;if (a->closing) ae_files_session_request_close(a);
}
void ae_files_session_create_update(struct ae_files_session *a) {
    if (!ae_files_session_creating(a) || !atomic_load_explicit(&a->create_done,memory_order_acquire)) return;
    ae_files_session_join_worker(a);
    if (a->renaming) { ae_files_session_rename_update(a);return; }
    enum ae_files_create_result r=ae_files_create_status(a->creation);
    if (a->diagnostics) printf("CREATE attempt=%u result=%u kind=%u\n",a->creations,(unsigned)r,(unsigned)a->create_kind);
    if (r==AE_FILES_CREATE_DONE) {
        ae_files_session_dialog_close(a);a->query_bad=false;(void)ae_files_filter(a->files,"",0);
        if (a->name[0]=='.') { a->show_hidden=true;ae_files_hidden(a->files,true); }
        enum ae_files_result refreshed=ae_files_refresh(a->files);
        if (refreshed==AE_FILES_OK) {
            for (size_t i=0;i<ae_files_count(a->files);++i) if (!strcmp(ae_files_entry(a->files,i)->name,a->name)) { (void)ae_files_select(a->files,i);break; }
            ae_files_session_reveal(a);ae_files_session_message(a,ae_files_session_tr(a,"Created","Angelegt"));
        } else ae_files_session_message(a,ae_files_session_tr(a,"Created; folder refresh failed. Do not recreate.","Angelegt; Ansicht nicht aktualisiert. Nicht neu anlegen."));
    } else if (r==AE_FILES_CREATE_UNKNOWN) {
        a->unknown=a->creation;a->creation=NULL;a->name_focus=1;
        snprintf(a->create_message,sizeof(a->create_message),"%s",ae_files_session_tr(a,"Outcome unknown. Inspect; do not retry.","Ergebnis unklar. Prüfen; nicht wiederholen."));
        ae_files_session_message(a,ae_files_session_tr(a,"Creation outcome unknown; further creation is blocked","Ergebnis des Anlegens unklar; weiteres Anlegen gesperrt"));
    } else {
        snprintf(a->create_message,sizeof(a->create_message),"%s",r==AE_FILES_CREATE_CONFLICT ? ae_files_session_tr(a,"Name already exists. Choose another name.","Name bereits vorhanden. Anderen Namen wählen.") : r==AE_FILES_CREATE_DENIED ? ae_files_session_tr(a,"Folder is no longer private and writable","Ordner nicht mehr privat und beschreibbar") : r==AE_FILES_CREATE_NOMEM ? ae_files_session_tr(a,"Not enough memory; nothing created","Nicht genug Speicher; nichts angelegt") : ae_files_session_tr(a,"Not created. Inspect the target folder.","Nicht angelegt. Zielordner prüfen."));
    }
    ae_files_create_close(a->creation);a->creation=NULL;a->dirty=true;
    if (a->closing) ae_files_session_request_close(a);
}
void ae_files_session_activate(struct ae_files_session *a) {
    if (a->closing || a->query_bad) return;
    const struct ae_files_entry *e=ae_files_entry(a->files,ae_files_selected(a->files));if (!e) return;
    if (e->kind==AE_FILES_DIRECTORY) { ae_files_session_result(a,ae_files_enter(a->files));return; }
    if (a->rename_unknown) { ae_files_session_message(a,ae_files_session_tr(a,"Inspect the unknown rename before opening documents","Unklares Umbenennen vor dem Öffnen von Dokumenten prüfen"));return; }
    if (a->launches>=256) { ae_files_session_message(a,ae_files_session_tr(a,"Document-start limit reached; reopen Files","Dokumentstart-Limit erreicht; Dateien neu öffnen"));return; }
    if (ae_files_session_busy(a) || a->child.pid>0) { ae_files_session_message(a,ae_files_session_tr(a,"One document window at a time","Nur ein Dokumentfenster gleichzeitig"));return; }
    int directory=-1,witness=-1;char leaf[AE_FILES_NAME+1];enum ae_files_result r=ae_files_open_selected(a->files,&directory,&witness,leaf);
    if (r!=AE_FILES_OK) { ae_files_session_result(a,r);return; }
    ++a->launches;int spawned=ae_files_launch(&a->child,a->terminal,a->editor,a->terminal_font,a->cache,directory,witness,leaf,a->diagnostics);close(directory);close(witness);
    if (spawned) ae_files_session_message(a,ae_files_session_tr(a,"Could not start the document window","Dokumentfenster konnte nicht gestartet werden"));
    else { ae_files_session_message(a,ae_files_session_tr(a,"Document window requested","Dokumentfenster angefordert"));if (a->diagnostics) printf("OPEN pid=%d pin=%d\n",a->child.pid,a->child.pin>=0); }
}
void ae_files_session_child_update(struct ae_files_session *a) {
    if (a->child.pid<=0) return;
    int r=ae_files_child_observe(&a->child);if (r<0) { a->failed=true;ae_files_session_message(a,"Document observation unresolved; new starts and close remain blocked");return; }
    if (r) { a->pressed=0;int status=a->child.status;if (a->diagnostics) printf("DOCUMENT_END status=%d\n",status);(void)ae_files_child_close(&a->child);
        enum ae_files_result refreshed=ae_files_refresh(a->files);if (refreshed!=AE_FILES_OK) ae_files_session_result(a,refreshed);else { a->first=0;ae_files_session_message(a,status ? ae_files_session_tr(a,"Document window ended with an error","Dokumentfenster mit Fehler beendet") : ae_files_session_tr(a,"Document window closed","Dokumentfenster geschlossen")); }if (a->closing && !ae_files_session_creating(a)) a->running=false; }
}
bool ae_files_session_menu_enabled(const struct ae_files_session *a,unsigned item) {
    if (item>=3 || a->closing || ae_files_session_creating(a) || ae_files_session_unresolved(a) || a->child.pid>0) return false;
    if (item==1) return a->clip_dir>=0 && a->clip_witness>=0;
    const struct ae_files_entry *e=ae_files_entry(a->files,ae_files_selected(a->files));
    return a->menu_selection && !a->query_bad && a->rows && e && (e->kind==AE_FILES_REGULAR || e->kind==AE_FILES_DIRECTORY);
}
void ae_files_session_menu_open(struct ae_files_session *a,float x,float y,bool selected) {
    a->menu=true;a->menu_x=x;a->menu_y=y;a->menu_selection=selected;a->menu_focus=0;a->pressed=0;a->query_focus=false;
    while (a->menu_focus<2 && !ae_files_session_menu_enabled(a,a->menu_focus)) ++a->menu_focus;
    a->dirty=true;
}
void ae_files_session_menu_action(struct ae_files_session *a,unsigned item) {
    if (!ae_files_session_menu_enabled(a,item)) return;
    a->menu=false;a->pressed=0;a->dirty=true;
    if (item==0) ae_files_session_cut_selected(a);else if (item==1) ae_files_session_paste_open(a);else ae_files_session_rename_open(a);
}
const char *ae_files_session_kind_label(struct ae_files_session *a,const struct ae_files_entry *e) {
    return e->kind==AE_FILES_DIRECTORY ? ae_files_session_tr(a,"Folder","Ordner") : e->kind==AE_FILES_REGULAR ? ae_files_session_tr(a,"File","Datei") : e->kind==AE_FILES_LINK ? ae_files_session_tr(a,"Link (not followed)","Link (nicht gefolgt)") : ae_files_session_tr(a,"Unavailable here","Hier nicht verfügbar");
}
const char *ae_files_session_folder_label(const struct ae_files_session *a) { return ae_files_depth(a->files) ? ae_files_folder(a->files) : a->root_label; }
void ae_files_session_navigate_place(struct ae_files_session *a,size_t index) {
    if (index>=a->place_count) return;
    if (a->place_snapshot[index]==SIZE_MAX) { ae_files_session_result(a,ae_files_ancestor(a->files,a->place_depth[index]));return; }
    if (ae_files_depth(a->files)) return;
    const struct ae_files_entry *e=ae_files_snapshot_entry(a->files,a->place_snapshot[index]);if (!e || e->kind!=AE_FILES_DIRECTORY) return;
    char name[AE_FILES_NAME+1];memcpy(name,e->name,strlen(e->name)+1);dev_t device=e->device;ino_t inode=e->inode;
    if (!ae_files_filter(a->files,"",0)) return;
    a->query_bad=false;a->query_focus=false;
    for (size_t i=0;i<ae_files_count(a->files);++i) { e=ae_files_entry(a->files,i);
        if (!strcmp(e->name,name) && e->device==device && e->inode==inode) { (void)ae_files_select(a->files,i);ae_files_session_result(a,ae_files_enter(a->files));return; }
    }ae_files_session_result(a,AE_FILES_CHANGED);
}
void ae_files_session_name_input(struct ae_files_session *a,const char *input) {
    if (ae_files_session_unresolved(a) || a->rename_stale || ae_files_session_creating(a) || a->name_bad || a->name_focus!=0) return;
    size_t n=strlen(input);char next[AE_FILES_NAME+1];
    if (n<=AE_FILES_NAME-a->name_size) {
        memcpy(next,a->name,a->name_size);memcpy(next+a->name_size,input,n+1);size_t size=a->name_size+n;
        if (ae_files_create_name(next,size) || (size==1 && next[0]=='.') || (size==2 && !memcmp(next,"..",2))) {
            memcpy(a->name,next,size+1);a->name_size=size;*a->create_message=0;a->dirty=true;return;
        }
    }
    a->name_bad=true;snprintf(a->create_message,sizeof(a->create_message),"%s",ae_files_session_tr(a,"Name rejected; retained prefix cannot be used","Name abgewiesen; bisheriger Teil nicht verwendbar"));a->dirty=true;
}
