/* Direct-entry editing for the graphical editor; independent of a toolkit. */
#ifndef AETHERI_EDITOR_DIRECT_H
#define AETHERI_EDITOR_DIRECT_H
#include "document.h"
struct ae_direct;
enum ae_direct_move { AE_DIRECT_LEFT, AE_DIRECT_RIGHT, AE_DIRECT_UP, AE_DIRECT_DOWN,
    AE_DIRECT_HOME, AE_DIRECT_END, AE_DIRECT_FIRST, AE_DIRECT_LAST };
struct ae_direct_selection { size_t begin,end;bool active; };
/* Process-local input correlation only. Never a clipboard, document-grant or
   persistence capability. Caller still owes original window/focus/transfer
   admission and must cancel async work before destroying its owner. */
struct ae_direct_token { uint64_t identity,interaction,revision;size_t cursor,anchor;bool selecting; };
/* Borrows the document until destroy; one owner thread, no outside mutations
   of content/cursor/history while attached. A synchronous original-file save
   may acknowledge clean state after cancel_input. Unlike the modal controller, direct entry is always enabled
   and the cursor may sit at EOF. Constructor does not alter the document.
   Returned identities never repeat within the process, including after free.
   No path, FD, file/save/close, key spelling, shaping or clipboard effects. */
struct ae_direct *ae_direct_create(struct ae_document *document);
void ae_direct_destroy(struct ae_direct *);
const struct ae_document *ae_direct_document(const struct ae_direct *);
bool ae_direct_selection(const struct ae_direct *,struct ae_direct_selection *);
bool ae_direct_token(const struct ae_direct *,struct ae_direct_token *);
/* Half-open selection between whole grapheme boundaries. Non-extending Left/
   Right collapses a selection without moving an extra character. Vertical
   movement preserves a logical grapheme goal across short/empty lines; it is
   not a visual/pixel/word column. Seek supports caller-admitted pointer hits. */
bool ae_direct_seek(struct ae_direct *,size_t position,bool extend);
bool ae_direct_move(struct ae_direct *,enum ae_direct_move,bool extend);
bool ae_direct_select_all(struct ae_direct *);
bool ae_direct_clear_selection(struct ae_direct *);
/* Cancel original async input on focus/tab/file-operation changes while keeping
   the visible selection and vertical goal. No clipboard/process effect. */
bool ae_direct_cancel_input(struct ae_direct *);
/* Strict nonempty UTF-8/TAB/LF/CRLF, under the document's existing limits.
   Replaces a selection in one undo step. Plain adjacent typing uses existing
   document coalescing. Delete/backspace operate on whole graphemes or CRLF.
   Newline follows the current line's ending, then the previous line, else LF.
   Failure preserves text/cursor/history/selection and input correlation. */
enum ae_document_result ae_direct_text(struct ae_direct *,const char *,size_t);
enum ae_document_result ae_direct_delete(struct ae_direct *,bool backwards);
enum ae_document_result ae_direct_newline(struct ae_direct *);
enum ae_document_result ae_direct_history(struct ae_direct *,int direction,bool *changed);
/* Exact original controller + gesture + document revision/cursor/selection.
   Every accepted editing/movement gesture consumes prior tokens, even if it
   hits an edge; failure consumes none. Successful paste is one edit/history
   group and invalidates the token before any later use. Never replays a stale
   receive after focus/tab changes: host must also call cancel_input (or
   another accepted gesture) when cancelling its original input context. */
enum ae_document_result ae_direct_paste(struct ae_direct *,const struct ae_direct_token *,const char *,size_t);
#endif
