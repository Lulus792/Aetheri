#ifndef AETHERI_EDITOR_MODAL_H
#define AETHERI_EDITOR_MODAL_H
#include "document.h"
enum ae_modal_mode { AE_MODAL_NORMAL, AE_MODAL_INSERT };
enum ae_modal_action { AE_MODAL_ESCAPE, AE_MODAL_ENTER_INSERT, AE_MODAL_APPEND,
    AE_MODAL_LINE_INSERT, AE_MODAL_LINE_APPEND, AE_MODAL_LEFT, AE_MODAL_RIGHT,
    AE_MODAL_UP, AE_MODAL_DOWN, AE_MODAL_HOME, AE_MODAL_END, AE_MODAL_DELETE,
    AE_MODAL_BACKSPACE, AE_MODAL_NEWLINE, AE_MODAL_OPEN_BEFORE, AE_MODAL_OPEN_AFTER, AE_MODAL_DELETE_LINE };
struct ae_modal;
/* Caller owns the document and outlives this controller; no direct document
   mutation while attached. Single owner thread. Pure normal/insert actions,
   no terminal/key parser, path/FD, save/quit/plugin/process or clipboard effects.
   Vertical goal is a logical grapheme column, not a shaped/pixel/cell column.
   Read-only document views keep their usual mutation lifetime. */
struct ae_modal *ae_modal_create(struct ae_document *document);
void ae_modal_destroy(struct ae_modal *modal);
const struct ae_document *ae_modal_document(const struct ae_modal *modal);
enum ae_modal_mode ae_modal_mode(const struct ae_modal *modal);
enum ae_document_result ae_modal_action(struct ae_modal *modal,enum ae_modal_action action);
enum ae_document_result ae_modal_text(struct ae_modal *modal,const char *text,size_t size);
/* Undo/redo original document deltas; a changed result returns to Normal mode.
   This is content history, never a file/save acknowledgement or grant. */
enum ae_document_result ae_modal_history(struct ae_modal *modal,int direction,bool *changed);
/* Normal-only admitted grapheme cursor jump; resets the vertical goal/history
   group without changing text, revision, saved state or undo/redo entries. */
bool ae_modal_seek(struct ae_modal *modal,size_t position);
#endif
