# Graphical text editor

7 November 2026. The standard Editor is separate from the modal editor inside
Terminal. It uses direct text entry, visible document tabs, floating search and
explicit unsaved-document decisions. Extensions should eventually support an
IDE-like workspace; no extension ecosystem or plugin permission model is selected.
See the [accepted design reference](../brains/aetheri/knowledge/ui-design.md).

## Implemented: direct editing controller

`src/editor/direct.h` and `direct.c` provide toolkit-independent C17 editing over
our existing document model. There is **no graphical Editor window yet**. The
new `aetheri_direct` library is not connected to the desktop or modal editor.

Unlike the modal controller, it always accepts direct text entry and allows the
caret at the end of a line or file. It supports:

- Whole-grapheme movement/deletion, including combining characters and joined
  emoji. CRLF is crossed/deleted as one line ending.
- Half-open selections through extended movement or an admitted pointer hit.
  Left/right without extension collapses a selection to its edge without moving
  another character. Select All and selection cancellation are explicit.
- Up/down with a remembered logical grapheme column across short or empty lines,
  plus line/document beginning and end. Pixel positions, soft wrapping, word
  navigation and page geometry remain integration work.
- Selection replacement as one edit, adjacent typing groups and the document's
  bounded undo/redo. Returning to its acknowledged saved state becomes clean;
  editing alone never acknowledges a file save.
- Newline insertion using the current line's ending, then the previous line's
  ending, then LF. Existing mixed endings and missing final endings are preserved.
- Original-input tokens for asynchronous paste. Each binds the process-local
  controller identity, interaction sequence, document revision, caret and anchor.
  Successful paste is a separate undo group and consumes the old token.

## Ownership and failures

The application retains its original document and file adapter. The controller
borrows that document until destruction; content/caret/history mutations must go
through the controller while attached. An original synchronous file save may
acknowledge clean state after input cancellation. Rendering gets read-only views
with the document's existing mutation lifetime. Destroying the controller does
not destroy its document or close/delete a file.

Document limits remain 8 MiB, 65,536 lines and 65,536 bytes per line, with existing
bounded history. The controller adds no heap text copy or background process. It
uses temporary line/grapheme scratch space for navigation. These are development
limits, not final large-file or performance acceptance.

Invalid UTF-8, allocation failure, invalid pointer hits and stale paste preserve
content, caret, history, selection and input correlation. Empty text/paste is
rejected rather than silently deleting a selection.

Controller identities never repeat within the process, even if an address is
reused. They are not persisted. Accepted gestures invalidate previous tokens even
at an unchanged document edge. The host must also call `ae_direct_cancel_input`
on focus/tab/transfer/file-operation changes; this keeps the selection and
vertical goal while invalidating old input. A token is never clipboard provenance,
focus, permission, native document transport or a bearer capability. Actual
window/transfer admission and cancellation remain host duties.

## Native evidence and next integration

`editor.direct` exercises seven editing workflows: Unicode, selection replacement,
saved-state undo, short/empty-line navigation, CRLF, stale/duplicate/cross-controller
paste and allocation/validation failures. Release and ASan/UBSan both pass.
Seven existing document/modal/history/search/selection/file/view groups also pass
in each profile. See the [executed record](../brains/aetheri/journal/2026-11-07-direct-editor.md).

Next connect this controller to the native graphical Editor: document-tab
lifetimes, existing file adapter, approved unsaved-change review, then actual
window/input/rendering and accessibility evidence. Closing the last tab must
leave the approved empty Editor view. Do not replace the modal Terminal editor
or its fixed development launch path to simulate that integration. Search UI,
file pickers, extensions, normal grants and production admission/recovery remain
unfinished.
