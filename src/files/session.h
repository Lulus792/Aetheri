/* Internal Files application state shared by native presentation adapters. */
#ifndef AETHERI_FILES_SESSION_H
#define AETHERI_FILES_SESSION_H
#include "model.h"
#include "launch.h"
#include "create.h"
#include "rename.h"
#include "layout.h"
#include <stdatomic.h>

struct ae_files_session;
/* Trusted original adapter callbacks. A false start guarantees no worker was
   started and no operation applied. A successful start owns exactly one worker;
   join returns only after that original worker has ended. These are not an SDK
   capability, filesystem grant or production recovery protocol. */
typedef bool (*ae_files_work_start)(void *,struct ae_files_session *);
typedef void (*ae_files_work_join)(void *);
struct ae_files_session {
    struct ae_files *files;struct ae_files_child child;
    const char *terminal,*editor,*terminal_font,*terminal_bold_font,*cache;char message[256];
    float scale;size_t first,rows;
    bool running,failed,closing,dirty,german,contrast,show_hidden,query_focus,query_bad,diagnostics,external_pending;
    unsigned frames,launches,pressed;size_t pressed_row;
    bool dialog,name_bad;unsigned name_focus,creations;int create_dir;uint64_t dialog_serial;
    enum ae_files_create_kind create_kind;char name[AE_FILES_NAME+1],folder[AE_FILES_LABEL+1],create_message[256];size_t name_size;
    struct ae_files_create *creation,*unknown;bool work_active;atomic_bool create_done;
    bool renaming,rename_stale;unsigned renames;int rename_witness;
    char original[AE_FILES_NAME+1],original_label[AE_FILES_LABEL+1];
    struct ae_files_rename *rename_op,*rename_unknown;
    bool moving,menu,menu_selection;unsigned moves,menu_focus;int move_target,clip_dir,clip_witness;
    char clip_name[AE_FILES_NAME+1],clip_label[AE_FILES_LABEL+1],clip_folder[AE_FILES_LABEL+1],from_folder[AE_FILES_LABEL+1];
    float menu_x,menu_y;struct ae_files_layout layout;
    bool grid,details,sidebar,info_overlay;
    unsigned place_depth[AE_FILES_DEPTH+1];size_t place_snapshot[AE_FILES_DEPTH+1],place_count;
    char root_label[AE_FILES_LABEL+1];bool consume_release,composing;
    ae_files_work_start work_start;ae_files_work_join work_join;void *work_context;
};
/* Initialize once, before any view or operation. The caller supplies/owns the
   independently selected files model and startup strings. Never copy live state.
   Ordinary methods run on its one UI owner thread. Only apply_worker runs on
   the single original worker; publication uses create_done release/acquire.
   Keep all state/model/callback storage alive until join. */
void ae_files_session_init(struct ae_files_session *,ae_files_work_start,ae_files_work_join,void *);
void ae_files_session_apply_worker(struct ae_files_session *);
void ae_files_session_join_worker(struct ae_files_session *);
bool ae_files_session_busy(const struct ae_files_session *a);
bool ae_files_session_creating(const struct ae_files_session *a);
bool ae_files_session_unresolved(const struct ae_files_session *a);
bool ae_files_session_inline_edit(const struct ae_files_session *a);
const char *ae_files_session_tr(const struct ae_files_session *a,const char *en,const char *de);
void ae_files_session_message(struct ae_files_session *a,const char *s);
void ae_files_session_result(struct ae_files_session *a,enum ae_files_result r);
void ae_files_session_reveal(struct ae_files_session *a);
void ae_files_session_move(struct ae_files_session *a,int amount);
void ae_files_session_clipboard_clear(struct ae_files_session *a);
void ae_files_session_dialog_close(struct ae_files_session *a);
void ae_files_session_request_close(struct ae_files_session *a);
void ae_files_session_dialog_open(struct ae_files_session *a,enum ae_files_create_kind kind);
void ae_files_session_rename_open(struct ae_files_session *a);
void ae_files_session_cut_selected(struct ae_files_session *a);
void ae_files_session_paste_open(struct ae_files_session *a);
void ae_files_session_rename_submit(struct ae_files_session *a);
void ae_files_session_create_submit(struct ae_files_session *a);
void ae_files_session_rename_update(struct ae_files_session *a);
void ae_files_session_create_update(struct ae_files_session *a);
void ae_files_session_activate(struct ae_files_session *a);
void ae_files_session_child_update(struct ae_files_session *a);
/* Coalesce invalidations; defer while a pointer press, menu/dialog, worker or
   uncertain operation owns the visible selection/input. */
void ae_files_session_directory_update(struct ae_files_session *a,bool changed);
bool ae_files_session_menu_enabled(const struct ae_files_session *a,unsigned item);
void ae_files_session_menu_open(struct ae_files_session *a,float x,float y,bool selected);
void ae_files_session_menu_action(struct ae_files_session *a,unsigned item);
const char *ae_files_session_kind_label(struct ae_files_session *a,const struct ae_files_entry *e);
const char *ae_files_session_folder_label(const struct ae_files_session *a);
void ae_files_session_navigate_place(struct ae_files_session *a,size_t index);
void ae_files_session_name_input(struct ae_files_session *a,const char *input);
#endif
