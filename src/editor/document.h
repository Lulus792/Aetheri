#ifndef AETHERI_EDITOR_DOCUMENT_H
#define AETHERI_EDITOR_DOCUMENT_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
enum { AE_DOCUMENT_BYTES=8*1024*1024, AE_DOCUMENT_LINES=65536, AE_DOCUMENT_LINE_BYTES=65536 };
enum { AE_DOCUMENT_HISTORY=1024, AE_DOCUMENT_HISTORY_BYTES=32*1024*1024 };
enum ae_document_result { AE_DOCUMENT_OK, AE_DOCUMENT_INVALID, AE_DOCUMENT_LIMIT, AE_DOCUMENT_NOMEM, AE_DOCUMENT_EXHAUSTED };
struct ae_document;
struct ae_document_slice { const char *first,*second;size_t first_size,second_size; };
struct ae_document_line { size_t start,size,ending_size; };
struct ae_document_info { size_t size,lines,cursor;uint64_t revision;bool dirty; };
struct ae_document_history_info { size_t undo,redo,bytes; };
/* Single owner; pure copied text, no path, FD, file/permission/save or rendering
   effects. Strict UTF-8, TAB/LF/CRLF, no NUL/C0/C1/DEL. Preserve bytes, mixed
   endings and missing final LF. Limits are this first text profile, not a binary
   editor or final large-file policy. No normalization, shaping or cell widths.
   Views are borrowed until successful mutation/destruction; never C strings.
   Insertion and output buffers must not alias document storage or each other.
   Invalid/limit/allocation failure leaves document content/cursor/revision intact. */
struct ae_document *ae_document_create(const char *text,size_t size,enum ae_document_result *result);
void ae_document_destroy(struct ae_document *document);
bool ae_document_info(const struct ae_document *document,struct ae_document_info *info);
bool ae_document_slice(const struct ae_document *document,size_t start,size_t size,struct ae_document_slice *slice);
bool ae_document_copy(const struct ae_document *document,size_t start,size_t size,char *out,size_t capacity);
bool ae_document_line(const struct ae_document *document,size_t row,struct ae_document_line *line);
bool ae_document_row(const struct ae_document *document,size_t position,size_t *row);
bool ae_document_cursor(struct ae_document *document,size_t position);
bool ae_document_step(struct ae_document *document,int direction);
enum ae_document_result ae_document_replace(struct ae_document *document,size_t begin,size_t end,const char *text,size_t size);
/* Cursor insertion with adjacent same-line typing coalescing. Other replacements,
   explicit break, cursor actions, history traversal and clean acknowledgement
   end a typing group. All retained entries are copied byte deltas, not file
   snapshots. Failed edit/allocation preserves text, redo and retained entries. */
enum ae_document_result ae_document_type(struct ae_document *document,const char *text,size_t size);
void ae_document_history_break(struct ae_document *document);
bool ae_document_history_info(const struct ae_document *document,struct ae_document_history_info *info);
/* -1 undo / +1 redo. Monotonic content revision advances; the logical history
   state and original cursor restore. Returning to the acknowledged saved state
   becomes clean, but stale save revisions still fail. No file/save/grant effect.
   No entry is a successful unchanged result. History never allocates on traversal
   while the document's retained text/index capacities are not externally shrunk. */
enum ae_document_result ae_document_history(struct ae_document *document,int direction,bool *changed);
/* Only a local content-generation acknowledgement. Caller owes actual external
   save evidence; this neither writes nor proves durability/ownership. Stale
   generations fail, cursor motion does not change content generation. */
bool ae_document_mark_clean(struct ae_document *document,uint64_t revision);
#endif
