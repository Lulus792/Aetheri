# Own modal editor core

5 November 2026. Original C17 document and normal/insert controllers now exist.
They are tested libraries, **not yet an executable file editor**. Terminal
presentation, file loading/saving, document undo, search/selection, command mode,
plugins and panes still need their adapters and tests. The existing
[shell line editor](line-editor.md) is a separate working component.

## Text and ownership

| Component | Owns |
| --- | --- |
| [document.h](../src/editor/document.h), [implementation](../src/editor/document.c) | Copied text, gap storage, line offsets, byte cursor and content revision |
| [modal.h](../src/editor/modal.h), [implementation](../src/editor/modal.c) | Normal/insert state, logical vertical goal and semantic edit actions; borrows the caller-owned document |
| Future app/view | Keyboard interpretation, rendering/cell geometry, accessible text, file permissions and actual I/O |

The first text profile accepts strict UTF-8, TAB, LF and CRLF. It preserves mixed
endings and missing final LF without normalization. NUL, other C0/C1 controls,
DEL, malformed UTF-8 and lone CR are rejected. This is not an arbitrary-byte or
binary-file profile. Unicode formatting characters remain document data; a
future view must render unsupported/invisible content explicitly rather than
silently issuing terminal control sequences.

Initial limits are 8MiB of text, 65,536 logical lines and 65,536 content bytes per
line. A final newline creates an empty last logical line. These are reviewed
engineering limits for this first profile, not final large-file requirements.
Text capacity starts at 4KiB when needed and grows geometrically; line capacity
starts at 64 offsets. Capacity stays allocated after deletion until destruction.
There is no disk history, persistent cache, new runtime dependency or service.

One owner mutates the model. Borrowed ranges have one or two explicit byte spans
around the gap, expire on successful mutation/destruction and are never assumed
to be C strings. Arbitrary byte ranges are allowed for reading; they need not be
grapheme-aligned. Insert and output buffers must not alias model storage or each
other. Views can read only their needed lines; there is no required whole-buffer
copy per frame. All text, line indexes and buffers are released on destruction;
owned text storage is wiped first. This is not a secure-memory/forensic-erasure
guarantee for scratch copies, allocator state or the OS.

## Editing and revisions

Replace endpoints and cursor positions must be extended-grapheme boundaries
within line content or at the next line start. A cursor cannot split UTF-8,
CRLF, combining sequences, flags or supported default Unicode 18 clusters.
An edit can join formerly separate clusters; its cursor snaps forward to the
new valid boundary. Model movement treats a complete line ending as one step.
Glyph shaping, bidi and pixel/cell width belong to the view, not these offsets.

Before mutation, replacement checks UTF-8, range, joined-line and total limits,
new cursor boundaries and both required allocations. If text or line-index
growth fails, the old bytes, cursor, revision and borrowed storage remain valid.
After successful admission, the single-owner commit has no further allocation.
Content revisions increase without wrapping; cursor movement does not change
them. Empty insertion into an empty range is a no-op. A nonempty replacement is
an edit even when its bytes happen to match the old text.

`mark_clean(revision)` accepts only the current content generation. It is a
local state acknowledgement, **not a file write, durable receipt or permission**.
The future file adapter must supply actual save evidence before acknowledging.
An old asynchronous save result cannot mark newer edits clean. No backing file,
path, app grant, StorageRef or account identity exists in this model.

## Modal controller

The controller starts in Normal mode. Its semantic actions cover entering Insert
before/after the current grapheme or at line start/end, whole-grapheme deletion,
Insert backspace/newline, opening a line before/after, line deletion and motions.
Escape returns from Insert to the preceding grapheme in that line. Normal mode
stays on an existing grapheme in nonempty lines; Insert may stand after the end.
Vertical movement retains a logical grapheme goal across short lines. This is
not yet visual-column motion for tabs, wide glyphs or wrapped views.

Newline/open actions choose the current line ending, or the preceding ending at
an unterminated last line, falling back to LF. Unrelated existing endings remain
unchanged. Deleting a final unterminated line also removes its preceding separator;
this is the current semantic action, not a promise of exact Vim compatibility.
Text insertion requires Insert mode. Failed growth while opening a line leaves
the old text, cursor and Normal mode intact. The caller retains the document,
keeps it alive and does not mutate it independently while a controller is attached.

These are internal semantic actions. Physical keys, repeat/count handling,
commands such as `:w`, file-save conflicts and unsaved-close UI are still work.
No action executes a shell command, opens a file or requests privileged work.

## Cost and checks

The original gap-buffer implementation moves bytes only when editing away from
the gap or growing storage. The separate flat line index still needs offset
updates after an edit. For file bytes N, affected-line bytes B, inserted bytes T,
line count L and gap travel D, replacement can cost O(N+L+B log R+T+D), with the
N term on growth and R=1,641 fixed Unicode property ranges. Local edits avoid the
whole-file move when existing gap capacity suffices; they still validate affected
lines and adjust following line offsets. Cursor/motion scans its bounded line.
Read-only row lookup is O(log L), line metadata and split-range lookup are O(1),
and range copying costs its byte length. This is not a measured speed comparison
or a balanced large-document tree. The standard gap concept is described in the
[GNU manual](https://www.gnu.org/software/emacs/manual/pdf/elisp.pdf).

[Document tests](../tests/test_document.c) compare 4,000 deterministic edits with
an independent flat byte model, check actual 8MiB/line/count limits, CRLF/missing
LF, Unicode/joined-cluster cursors and constructor/text/index allocation failures.
[Modal tests](../tests/test_modal.c) cover mode transitions, motions/vertical
goals, line actions, preserved endings, joined clusters and failed-open atomicity.
Both run in Release and ASan/UBSan. Executed integration scope and remaining app
work are in the [dated record](../brains/aetheri/journal/2026-11-05-search-and-editor-core.md).
