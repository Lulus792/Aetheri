# Graphical text editor

7 November 2026. The standard Editor is separate from the modal editor inside
Terminal. It uses direct text entry, visible document tabs, floating search and
explicit unsaved-document decisions. Extensions should eventually support an
IDE-like workspace; no extension ecosystem or plugin permission model is selected.
See the [accepted design reference](../brains/aetheri/knowledge/ui-design.md).

## Implemented: direct editing controller

`src/editor/direct.h` and `direct.c` provide toolkit-independent C17 editing over
our existing document model. The opt-in native window below uses this controller;
the normal desktop launch path and modal editor are unchanged.

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

## Implemented: document tabs, local saving and close review

`editor/workspace.h` now joins up to eight original documents, their direct-entry
controllers and the existing local file adapters. These are native backend tabs,
not rendered controls. Opening an unchanged already-open local source selects
its original tab with unsaved edits intact. Changed/replaced source conflicts
instead of silently reloading. Closing the last tab leaves an empty workspace.

An untitled document creates no file. Its first save uses an independently chosen
directory FD and literal leaf with create-only publication. A destination token
binds the original active document/revision, input sequence and current close
review. Edits, switching away/back or a new close review invalidate a delayed
picker result before file effects. Known failed first saves leave the document
untitled; UNKNOWN retains the original file adapter, name and recovery information.
Existing named saves keep that original binding. File preservation/restrictions
remain those of the [local file adapter](editor.md); no normal grant is implied.

Clean known tabs close immediately. Dirty or uncertain tabs become active and
freeze workspace edits/switches until an original ID/revision/review-serial
decision. Cancel keeps the document. Discard drops only its unsaved in-memory
tab, never its file. Save closes only after actual successful file saving; a
missing first destination keeps the same review pending. Stale/replayed close
decisions cannot close another tab. UNKNOWN blocks save/discard completion even
when the text happens to be clean. Recovery/export of that condition remains
separate work; cancellation can return to the retained document.

One synchronous owner manages documents and file operations. Read-only tab views
and the active input handle are borrowed only until the next workspace operation.
Review blocks new input handles; callers must not keep an old handle across it.
Normal UI shutdown must resolve all tabs first. Destruction only releases local
memory/FDs, never approves user close or removes file/recovery artifacts. The
responsive I/O host, normal file-grant admission and complete Save As/export
still need integration. The private local picker below does not confer a grant.

`editor.workspace` passes eight native file/tab/close workflows, including real
saves, no-replace conflicts, external changes, allocation/ENOSPC failures,
post-rename uncertainty, delayed picker rejection and stale decisions. All nine
editor groups pass in Release and ASan/UBSan without skips; see the
[workspace execution record](../brains/aetheri/journal/2026-11-07-editor-workspace.md).

## Opt-in native window

`aetheri-text-editor` now renders original document tabs, a workspace list,
line-number gutter, caret and selection through the pinned NexaUI 0.1.23 C API.
It uses the software renderer and an opaque theme. These are real document/file
effects, not the separate design studio's example data. The accepted compact
glass design, icons and remaining interactions still need faithful implementation.

The local picker borrows an admitted development root, navigates held folders and
hands the selected original regular-file FD witness to the workspace for both
new reads and duplicate-tab reuse. First saves remain create-only. Named saves
retain original-file conflict checks. File I/O and folder enumeration currently
run synchronously; responsive operation ownership is the next host work.

The document-first close review defaults to Cancel. Save keeps the original review
while asking for a first destination, then closes only after actual success.
Discard closes the in-memory tab without deleting its file. Closing the last tab
leaves a real empty window. Window close reviews all remaining documents.
Uncertain saves remain blocked; no new recovery/export implementation is implied.

Native events pass through the app one at a time, with a frame between them.
Do not route an unexamined remainder directly to the toolkit: direct document
input would bypass its controller and earlier focus changes. Rendering uses
read-only workspace selection even while close review blocks an input handle.
The loop blocks when idle and redraws on logical size or raster-density changes.

| Input | Action |
| --- | --- |
| Ctrl+N / Ctrl+O / Ctrl+S | New / local Open / Save |
| Ctrl+W / Ctrl+Q | Close current document / review window close |
| Ctrl+Tab / Ctrl+Shift+Tab | Next / previous original tab |
| Arrows, Home/End, Ctrl+Home/End | Move; Shift extends the selection |
| Ctrl+A / Ctrl+Z / Ctrl+Y | Select All / Undo / Redo |
| F6 | Move between document and toolbar focus |
| Escape | Cancel picker/review or clear document selection |

The host includes local copy/cut/paste adapters, but native clipboard transfer,
queued focus loss and cancellation still need asynchronous integration and their
own tests. The current synchronous paste path is provisional. It is not the
Terminal's already tested asynchronous transport. External IME/direct-editor
composition, search UI, plugins, scroll/drag refinements and page navigation
remain work.

Buttons expose names/roles/state; document text is a bounded read-only accessibility
snapshot, not a complete native editable-text interface. The native filename
field has a visible label. Full screenreader editing and human assistive acceptance
remain open. Text sizes 100/150/200 and dark/light/high-contrast CLI choices exist;
the tests below cover selected combinations, not the whole accessibility matrix.
Native title move/edge resize, minimization, minimum-window reflow and live theme
switching still need implementation/acceptance. Zoom is only a native request.

The pinned SDK exposes a sticky native `should_close` flag. This host latches its
first observation so Cancel cannot repeatedly reopen review. Repeated native
compositor-close and subsequent window-management behavior need a consumable SDK
request/cancellation contract; see catalogue W01. App-owned close controls remain
separate. Display/process failure still has no durable unsaved-document recovery.

## Build and native test boundary

On Linux, configure `AETHERI_GRAPHICAL_EDITOR=ON` with `AETHERI_NEXAUI_PREFIX`
pointing to the exact reviewed profile's installed SDK. Build `aetheri-text-editor`.
This consumer can build without selecting Settings or SDL. Existing SDK version
and source-revision guards still apply. Under an admitted private Wayland seat:

```sh
build/aetheri-text-editor UI_FONT --mono-font MONO_FONT --directory DOCUMENT_ROOT
```

Optional `--german`, `--light`, `--contrast`, `--text-percent 100|150|200` and
`--diagnostics` select development presentation. Trailing literal file names are
opened relative to that original root. This is ordinary developer access, not
installed app admission or brokered documents.

The explicit disposable-carrier test takes original test-server/app/font paths:

```sh
python3 tools/test_editor_graphical.py TEST_COMPOSITOR APP UI_FONT MONO_FONT BUILD_DIR
```

Build the test compositor from the same source with `AETHERI_COMPOSITOR=ON` and
reviewed graphics prefixes. Its private input-driver `Z output 100|200` command
commits integer density for the actual 4K test; it is absent from installed
compositor input and does not implement fractional scaling. Run with bounded
private temporary backing: pinned NexaUI allocates sparse Wayland buffers in
`/tmp`; known foreign UID quota exhaustion can otherwise SIGBUS. Never reclaim
foreign files or treat private tmpfs as the SDK allocation-failure fix.

Release and ASan/UBSan native workflows pass alongside all nine editor groups.
The selected cases cover real typed/saved bytes, replacement/history, tabs,
Save/Discard/Cancel, last-tab empty view, held-file open, external-change conflict,
F6, German 200% high contrast and light 150% on an actual reported 2x virtual
output. Idle CPU is measured, and native images are reviewed. See the
[execution record](../brains/aetheri/journal/2026-11-07-editor-window.md).
This is not normal desktop provisioning, complete native accessibility, physical
monitor/laptop acceptance or a bootable Aetheri image.
