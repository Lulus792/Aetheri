#ifndef AETHERI_SHELL_EDITOR_H
#define AETHERI_SHELL_EDITOR_H
#include <stdbool.h>
#include <stddef.h>
enum { AE_LINE_BYTES=16383, AE_LINE_HISTORY=64, AE_LINE_HISTORY_BYTES=131072,
    AE_LINE_UNDO=64, AE_LINE_UNDO_BYTES=262144, AE_LINE_SEARCH_BYTES=1024 };
struct ae_line_editor;
enum ae_line_action { AE_LINE_LEFT,AE_LINE_RIGHT,AE_LINE_HOME,AE_LINE_END,
    AE_LINE_BACKSPACE,AE_LINE_DELETE,AE_LINE_KILL_START,AE_LINE_KILL_END,AE_LINE_WORD_BACK,AE_LINE_CLEAR,AE_LINE_YANK };
/* Single owner thread; no OS, terminal, rendering, execution or disk effects.
   Text is strict single-line UTF-8; cursor byte offsets are extended-grapheme
   boundaries. Borrowed text is valid until a successful mutation or destroy.
   Insertion/error is atomic. History is copied, private, bounded and in-memory.
   Creating no process or global state; callers own command admission. */
struct ae_line_editor *ae_line_create(void);
bool ae_line_valid_utf8(const char *text,size_t size);
void ae_line_destroy(struct ae_line_editor *editor);
void ae_line_clear(struct ae_line_editor *editor);
const char *ae_line_text(const struct ae_line_editor *editor,size_t *size,size_t *cursor);
const size_t *ae_line_boundaries(const struct ae_line_editor *editor,size_t *count);
bool ae_line_insert(struct ae_line_editor *editor,const char *text,size_t size);
/* One atomic edit over existing grapheme boundaries; breaks typed-insert grouping.
   OOM/invalid input preserves text/cursor/undo/redo. Never executes a candidate. */
bool ae_line_replace(struct ae_line_editor *editor,size_t begin,size_t end,const char *text,size_t size);
/* -1 undo, +1 redo; no available state is a successful no-op. Adjacent insert
   calls group until another edit/movement. History navigation/clear start a new
   undo lifetime; retained command history is never changed by undo. */
bool ae_line_undo(struct ae_line_editor *editor,int direction);
bool ae_line_edit(struct ae_line_editor *editor,enum ae_line_action action);
bool ae_line_history(struct ae_line_editor *editor,int direction);
struct ae_line_search_view {
    const char *query,*match;size_t query_size,match_size;bool found;
};
/* Search borrows retained history and leaves the original draft/cursor/undo and
   history navigation untouched until accept. While active, ordinary mutations
   refuse (clear may explicitly discard the whole draft). Query is bounded,
   literal/case-sensitive UTF-8; query backspace removes a whole grapheme.
   -1 older/+1 newer, no wrapping; no-match is view state, not an API failure.
   View spans expire on mutation. Cancel restores the untouched original draft;
   accept alone loads a match as history navigation, never records/executes it.
   These functions allocate nothing and have no OS effects. */
bool ae_line_search_begin(struct ae_line_editor *editor);
bool ae_line_search_active(const struct ae_line_editor *editor);
bool ae_line_search_view(const struct ae_line_editor *editor,struct ae_line_search_view *view);
bool ae_line_search_insert(struct ae_line_editor *editor,const char *text,size_t size);
bool ae_line_search_backspace(struct ae_line_editor *editor);
bool ae_line_search_clear_query(struct ae_line_editor *editor);
bool ae_line_search_next(struct ae_line_editor *editor,int direction);
bool ae_line_search_accept(struct ae_line_editor *editor);
void ae_line_search_cancel(struct ae_line_editor *editor);
/* Remember a deliberately accepted line; never executes or clears it. Initial
   ASCII space opts out, empty/adjacent-duplicate lines are not recorded. OOM
   preserves existing history and the draft. Caller may still execute its line. */
bool ae_line_remember(struct ae_line_editor *editor);
#endif
