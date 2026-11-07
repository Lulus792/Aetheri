# Own modal file editor

5 November 2026. The opt-in `aetheri-editor` executable joins our original C17
document, modal controller, local file adapter and terminal-cell view. It runs
inside the existing terminal as a foreground command. This remains a local
development profile; normal document grants/admission, durable recovery, block/
mouse selection, richer search, plugins and panes remain work. The existing
[shell line editor](line-editor.md) is a separate working component.
The standard [graphical Editor](graphical-editor.md) has a separate direct-entry
controller; it does not use this modal interaction or have a native window yet.
The new [Files development workflow](files.md) can also open a selected text file
in a dedicated Terminal. Its `--selected-fds` entry checks the held file witness
before reading; this is separate from ordinary document-grant admission.

## Try it

In the reviewed configured Linux build, enable `AETHERI_EDITOR=ON` and build
`aetheri-editor`. For example, inside the dedicated carrier:

```sh
cmake -S /home/aetheri/source -B /home/aetheri/build/session-release -DAETHERI_EDITOR=ON
cmake --build /home/aetheri/build/session-release --target aetheri-editor
```

Open Terminal through the [development desktop](development-session.md). In its
ordinary shell, select a file in a directory owned by the current user:

```sh
/home/aetheri/build/session-release/aetheri-editor --new notes.txt
/home/aetheri/build/session-release/aetheri-editor notes.txt
```

`--new` requires an absent name and creates no file until save. Opening an absent
file fails; it does not silently switch to creation. No path starts an untitled
document. The first save then asks for a new path. File-adapter restrictions below
still apply. Commands run with the development user's rights, without a sandbox
or a new authority. `--help` describes the CLI without taking a terminal.

| Control | Action |
| --- | --- |
| `i`, `a`, `I`, `A` in Normal | Insert before/after this grapheme or at line start/end |
| `o`, `O`; `x`; `dd` in Normal | Open a line, delete a grapheme, delete a line |
| Arrows / `h j k l`, Home/End / `0 $` | Move; Insert uses arrows/Home/End |
| Enter, Backspace, Delete in Insert | Newline or whole-grapheme edit |
| `u`, Ctrl+R in Normal | Undo or redo the original document history |
| `v`, `V` in Normal/selection | Select characters / whole lines; repeat to cancel, switch kind without losing the anchor |
| Motions during selection | Move the endpoint by whole graphemes; the anchor stays fixed |
| `y`, `d`/Delete, `c` during selection | Copy, cut or replace through the private editor register |
| `p`, `P` in Normal | Paste after/before the cursor or line from that register |
| Ctrl+Shift+Z/Y | Existing terminal chords for undo/redo; a changed result returns to Normal |
| `/`, `?` in Normal | Forward/backward literal search with an in-memory preview |
| Enter / Escape during search | Choose the preview match / restore the original cursor |
| `n`, `N` in Normal | Next match in the accepted direction / reverse direction; no wrap |
| Escape; Ctrl+C/G | Normal mode, or cancel the current interaction |
| Ctrl+S; `:w` | Save the current binding; ask for a new path when untitled |
| Ctrl+Q; `:q` | Close if settled/clean; otherwise show close choices |
| `:wq` | Save and close only after success |
| `:saveas NEW-PATH` | Create-only save under a new binding; never overwrite a named destination |
| `:export NEW-PATH` | Create-only copy; preserve the original dirty/uncertain state |
| `:q!` / close dialog's `d` | Ask for the literal word `DISCARD`; never immediately drop text |
| `:help`, then `j/k`, Escape | Read/scroll the keyboard guide and return to the document |
| Ctrl+Z, then shell `fg` | Suspend and resume the same buffer; background resume stops again |

Text after `saveas ` or `export ` is a literal path, including spaces. It is not
shell syntax: quotes and `$` have no expansion role. The command buffer is bounded
to 4,096 UTF-8 bytes. Invalid/control/overflow input blocks submission until
Escape or Ctrl+U, rather than accepting a truncated prefix. Escape sequences
have a bounded decoder and a 120ms deadline; lone Escape is distinguished from
arrows. This is not a general terminal-protocol or untrusted-input parser.
Escape followed promptly by a non-CSI/SS3 byte first cancels/enters Normal, then
processes that next key. Fast Escape→`u` or Escape→Ctrl+S must not lose the action.

The close dialog offers save/discard/cancel. An unknown save instead offers
copy/discard/cancel; it cannot rearm the original write. `DISCARD` confirms leaving
the editor with potential artifacts retained; uncertain exit has status 2.
An untitled save-and-close asks for a new path and only closes after that save.
Export is a recovery copy, not an acknowledgement that the original save settled.
Up to eight additional uncertain exports stay held; reaching that bound refuses
more binding attempts. General cross-process recovery and an artifact inspector
remain work.

## Selection and private register

Visual selection belongs to the modal model. `v` includes both endpoint
graphemes; `V` covers all touched lines and their existing endings. Reverse
selection and mixed UTF-8/endings retain original byte ranges. Escape returns
to Normal at the current endpoint. A selection kind can switch without losing
its anchor. Search first leaves selection at that endpoint; save/close choices
do not turn selection into file authority. Undo/redo changes return to Normal.

`y` copies to one private in-memory editor register without changing text,
revision, clean state or retained undo/redo entries. `d`/Delete cuts, and `c`
replaces the range then enters Insert. A linewise change leaves one empty line
with the final selected line's ending; its cursor and redo receipt stay on that
line. Each cut/change/paste is one document history entry; subsequent typing
keeps the existing separate typing-group rules. Failed validation/limit/
allocation preserves text, selection, the previous register and redo branch.

The register holds at most 8MiB of owned original text, wiped before replacement
or destruction. Wiping is not a secure-erasure guarantee. It lasts only for this
editor process: there is no disk history, daemon, platform clipboard read/write
or automatic command execution. Normal `x`/`dd` retain their earlier delete
behavior; they do not replace this register. An empty selection keeps the prior
register and says so. Block/mouse selection, named registers, platform clipboard
integration and final bindings remain later work.

Character paste inserts the copied bytes before/after the current grapheme.
Linewise paste inserts before/after the current line, adding the captured LF or
CRLF separator when required to keep whole lines separate. Existing copied
endings stay unchanged. Cutting the final unterminated line consumes the preceding
separator too, so it does not leave a phantom line; Undo restores those bytes.
Cutting a final empty line can likewise remove its preceding separator while
keeping the old register. Paste follows inserted text with Normal end-of-line
clamping. The view highlights the selected cells and paints a selected ending/
empty-line placeholder without inserting that placeholder into document bytes.
The mode and selection actions remain visible at the 30x8 minimum.

Register staging plus document growth/history admission precedes mutation. A
linewise paste can need an additional copied assembly buffer; old/new register,
document, index and undo allocations may coexist during preflight. The 8MiB
register limit is not a whole-process RSS ceiling. No permission, grant, file
baseline or uncertain-save outcome changes through selection or a transfer.
See [selection evidence](../brains/aetheri/journal/2026-11-05-editor-selection.md).

## Literal document search

The first search is case-sensitive literal UTF-8 within one line, up to 256 query
bytes. It has no regex, normalization, case folding, cross-line expression,
automatic wrapping or persistent index. Matches must begin and end on complete
document grapheme boundaries; searching `j` skips a combined `j́`, while the whole
combined query can match it. CRLF and gap-buffer storage do not change offsets.

`/` or `?` saves the original Normal cursor. Admitted query changes preview the
first eligible forward/backward match including that position, with an inverse
match highlight and keyboard focus on the query. No match or an empty/rejected
query restores the original cursor; Enter cannot accept an old prefix/result.
Enter chooses a valid match and remembers its query/direction. Escape restores
the original cursor and leaves the previous accepted query intact. Ctrl+U clears
the rejected/current query; Backspace removes a whole query grapheme. Invalid,
unsupported-control or overflow input blocks acceptance until clear/cancel.
Escape also cancels after an incomplete UTF-8 prefix. Save, close or suspension
cancels the preview before its ordinary workflow.

`n`/`N` recomputes from the current document, strictly beyond/before the current
cursor; it never reuses a stored match offset after edits or undo. At the search
boundary it reports no further match and keeps the cursor. Search never edits
text, advances content revision, changes clean state, clears undo/redo entries,
writes files or acknowledges uncertain saves. Enter chooses only the match and
cannot execute a command. The modal owner resets vertical-motion goals on jumps.
Ending a typing group is separate from changing retained history entries.

The read-only `ae_document_find` uses an original prefix-table/KMP byte scan and
one grapheme segmentation per relevant line. Overlapping candidates remain
eligible. Two monotonic boundary cursors admit whole matches without rescanning
a line for each candidate. For N scanned bytes, L line records, M query bytes and
R fixed Unicode property ranges, the bound is O(N log R + L + M) time and
O(maximum line bytes + maximum line grapheme count + M) scratch, with no heap
allocation or new process. Backward lookup scans forwards to retain the last
eligible match. Repeated query changes repeat that bounded synchronous scan;
this is not a responsiveness or speed-superiority claim for maximum-size files.
Viewport painting remains the existing terminal-cell fallback, not shaping or
an accessible-text document interface.

The earlier 200-percent no-match capture was misread: its query glyphs are `zerr`,
not `zero`. The corrected native-input check observes printable commits and
requires the actual `zero` query plus all four highlighted document cells; the
same editor binary passes the ordinary desktop check and manual image review.
See the [input record](../brains/aetheri/journal/2026-11-05-native-key-input.md).
This closes that specific counterexample, without proving physical input,
assistive technology or normal admitted-session acceptance.

## View and terminal lifetime

The view uses the existing terminal's colors, glyph/cell profile, native Compose
and text scaling. Header, line numbers and status distinguish Normal/Insert,
modified text, current location and SAVE UNKNOWN. The close dialog moves the
terminal caret to its choices; cancelling restores document focus. Long lines
pan by whole graphemes, with clipping markers and a visible caret. Tabs use four
logical cells. Unsupported/invisible/complex clusters have an explicit ASCII
fallback; bytes and edit boundaries remain unchanged. Arbitrary filename bytes
cannot inject terminal commands. This is not shaping, bidi or accessible geometry.
The header puts the filename before its parent path so long paths cannot displace
the short basename at ordinary/minimum widths; very long names remain clipped.

The first minimum is 30 columns × 8 rows, maximum 512 × 200. Below the minimum,
the view says editing is paused and refuses invisible edits/commands until resize.
The shell's original screen is retained using the terminal's alternate screen.
Input drains bounded bursts without reading beyond a completed quit, leaving
subsequent bytes for the original shell. The app has its own nonblocking
`/dev/tty` open-file description and a foreground/session check; it never changes
the shell/command descriptors' flags. It restores modes and screen before stopping
and only resumes raw mode after foreground ownership returns.

Signals are consumed through signalfd, without allocation/file work in async
handlers. SIGTERM/SIGHUP or an observed terminal failure attempts a private
create-only `.aetheri-editor-recovery-…` copy in the current directory if text is
dirty or a save is uncertain, then exits. Failure is reported; this is best effort,
not autosave, recovery after SIGKILL/power loss, or a durable journal. The current
directory must meet the same local directory rules. No background process,
periodic history file or new runtime library is added. See
[termios](https://man7.org/linux/man-pages/man3/termios.3.html) and
[signalfd](https://man7.org/linux/man-pages/man2/signalfd.2.html).
Once the signal reader has decided to exit, it drains its remaining queued
managed signals without another copy, cancellation, suspension or exit-code rewrite.
This does not stop SIGKILL, guarantee recovery storage, or settle arbitrary
future signal/terminal/OS failure schedules.

Keyboard/virtual scaling evidence does not establish screenreader acceptance.
AT-SPI document semantics, external IME/bidi/complex shaping, light-theme design,
localised editor chrome, physical hardware and final UI remain open. These keys
and layout are the first reversible engineering profile.

## Text and ownership

| Component | Owns |
| --- | --- |
| [document.h](../src/editor/document.h), [implementation](../src/editor/document.c) | Copied text, gap storage, line offsets, byte cursor and content revision |
| [modal.h](../src/editor/modal.h), [implementation](../src/editor/modal.c) | Normal/insert state, logical vertical goal and semantic edit actions; borrows the caller-owned document |
| [file.h](../src/editor/file.h), [implementation](../src/editor/file.c) | Independently held local directory/file, baseline bytes/metadata and synchronous save outcome; development profile only |
| [view.h](../src/editor/view.h), [implementation](../src/editor/view.c) | Bounded cell projection, viewport and reusable frame scratch; no I/O or model mutation |
| [application](../src/editor/main.c) | Literal keys/commands, explicit save/close decisions, own TTY/signal lifetime and additional unsettled export holders |
| Future integration | Accessible text, durable recovery and normal brokered file access |

The first text profile accepts strict UTF-8, TAB, LF and CRLF. It preserves mixed
endings and missing final LF without normalization. NUL, other C0/C1 controls,
DEL, malformed UTF-8 and lone CR are rejected. This is not an arbitrary-byte or
binary-file profile. Unicode formatting characters remain document data; a
view must render unsupported/invisible content explicitly rather than
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
The separate file adapter must supply actual save evidence before acknowledging.
An old asynchronous save result cannot mark newer edits clean. No backing file,
path, app grant, StorageRef or account identity exists in this model.

## Undo, redo and saved content

The document owns copied forward/inverse byte deltas and the original before/
after cursor. It retains at most 1,024 whole undo steps across both directions
and 32MiB of requested entry/header storage. The fixed pointer table and allocator
overhead are separate; preflight allocations can temporarily exceed the retained
budget. Oldest complete entries are evicted only after a successful new edit.
No history file, new service or whole-document snapshot per keystroke is added.

`type` coalesces adjacent same-line insertions with geometric entry capacity.
Newlines, other replacements and explicit interaction/cursor/history/save
boundaries end that typing group. Delete/backspace/line actions are separate
steps. Control actions delimit groups even when they make no text change.
Undo/redo from the modal controller returns a changed document to Normal mode;
the original valid cursor restores, then receives the normal end-of-line clamp.
This is a bounded linear history, not exact Vim compatibility or an undo tree.

All required text, line-index and history allocation precedes mutation. Failed
text admission/allocation leaves text, cursor, revision, history and redo intact.
A successful new edit discards the old redo branch; failed edits cannot do so.
Undo/redo uses the retained grow-only text/index capacity and allocates no new
history or document storage. Each traversal still moves/copies/validates relevant
text and updates the flat line index; there is no constant-time or measured
speed-superiority claim. New edits charge copied removed/inserted bytes plus
entry capacity; copying a large replacement can be expensive.

An edit can join graphemes, so reversing an original byte delta may begin inside
the resulting cluster. Only the private history path may do this, bound to its
exact original logical content state. Ordinary edits still require grapheme
boundaries. Recorded canonical text/endings restore byte-for-byte; history is
not a general raw-byte editing API.

Every content change, undo and redo advances the monotonic revision. A separate
document-only logical state identifies the current history position. Successful
`mark_clean(current_revision)` acknowledges that state; returning to it by undo/
redo becomes clean without rewinding the revision. Stale save acknowledgements
still fail. Manually recreating identical bytes on a different branch need not
become clean. These IDs have no component/run/storage/grant meaning.

Cleanliness only describes the acknowledged document state. Undo/redo never
touches a file, acknowledges another save, reopens an UNKNOWN binding or drops
unsettled export holders. Export still leaves the original document history/
dirty/uncertain state alone, apart from ending the current typing group.

The [history tests](../tests/test_history.c) exercise grouped typing, saved-state
return/stale acknowledgements, redo branching and allocation failures, Unicode
combining/Prepend/flag joins and CRLF, actual count/32MiB eviction, allocation-free
8MiB traversal and 5,000 independent snapshot-reference transitions. The reference
requires substantial coverage of edits, undo, redo, cursor changes and save marks.

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

These remain internal semantic actions. The executable maps the first keys and
commands above; richer repeat/count handling and editing are still work.
No modal action executes a shell command, opens a file or requests privileged work.

## Local development file adapter

The caller selects a directory FD and one filename. The adapter holds its own
directory descriptor, admits a current-user-owned directory without group/other
write access, and rechecks that boundary before saving. Root use is refused.
Existing files must be current-user-owned regular single-link files without
set-ID/sticky bits and within the text profile. A no-follow O_PATH pin admits the
type before opening the same object through trusted `/proc/self/fd`. Symlinks,
FIFOs, devices, directories and oversized objects are not opened as content.
Bounded content that fails the text profile is rejected without exposing a document.
No privileged authority, path grant, app sandbox or StorageRef is created.

Loading copies the exact bytes and readable attributes between repeated held/
named metadata observations. The file object retains a private baseline; the
caller separately owns the document. Read-only files may load, but saving them
is refused. A new binding creates only an empty document until explicit save;
an already existing name is a conflict. Filenames and recovery leaves are data,
never shell syntax or safe-to-print terminal strings.

Saving serializes the current content generation, rechecks the original inode,
timestamps, owner/group/mode/link count/size and actual baseline bytes, then writes
an exclusively created random `.aetheri-save-…` file in the held directory.
Short writes and EINTR are handled; zero writes and errors cannot publish a prefix.
Existing ordinary permissions/GID and readable xattrs are restored and checked.
There are at most 64 attributes, 16KiB of names and 64KiB of values in this first
profile. Capability attributes, unavailable/oversized metadata and metadata that
cannot be preserved are refused rather than silently dropped. New files use 0600.
Hidden attributes omitted by the kernel are outside this readable-metadata profile.

The adapter flushes the staged file and directory, repeats original-file checks,
then renames. A new file uses RENAME_NOREPLACE. Existing-file replacement is atomic
visibility, **not an atomic compare-and-swap**: another program can change the
name/content after the last check. This development path assumes an independently
selected local Linux filesystem and no hostile same-UID writer. It does not solve
general cross-app save coordination or remote-filesystem failure semantics.

After rename, the directory is flushed, the held/named object and exact committed
bytes are checked, and the writer is closed before the content generation can be
acknowledged clean. A close error is not retried on its now-released FD number.
Only a fully acknowledged save updates the private baseline and calls mark_clean.
This is kernel/filesystem acknowledgement, not tested hardware power-loss recovery.
See [rename](https://man7.org/linux/man-pages/man2/rename.2.html),
[fsync](https://man7.org/linux/man-pages/man2/fsync.2.html) and
[xattr enumeration](https://man7.org/linux/man-pages/man2/flistxattr.2.html).

A failure before rename cleans only the adapter's staged file and leaves the
target unchanged. A possibly applied rename, failed later flush/observation/close
or uncertain cleanup returns UNKNOWN and fences subsequent saves on that object.
It never retries, restores an old file or assumes rollback. An available recovery
leaf is exposed as a *potential* artifact, not proof that the name still refers to
an owned file. Closing the adapter releases memory/FDs without deleting uncertain
artifacts. The process-local fence is not a durable recovery journal.

Failure leaves the document's clean generation unchanged. Edited content therefore
stays dirty; a previously clean document can still have an UNKNOWN save outcome.
The executable blocks ordinary close independently on uncertainty and offers
create-only export or explicit discard. Full reconciliation/inspection and
cross-process/restart recovery and brokered saves remain work.

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
The [file tests](../tests/test_editor_file.c) separately exercise actual local
load/save, maximum-size bytes, attributes, conflicts, FD cleanup and injected
write/flush/rename/close failures. Their evidence and limits are in the
[file-adapter record](../brains/aetheri/journal/2026-11-05-editor-file.md).
Current executable/view/PTY/provider evidence, initial failures and actual
development-desktop limits are in the
[application record](../brains/aetheri/journal/2026-11-05-editor-app.md).
Bounded document history, one-shot queued-signal exit, fast Escape handling and
the final current matrix/desktop evidence are in the
[history record](../brains/aetheri/journal/2026-11-05-editor-history.md).
The [search record](../brains/aetheri/journal/2026-11-05-editor-search.md)
tracks the literal-search model, query focus, minimum-size rendering and actual
native/desktop evidence separately from richer search and accessibility work.
