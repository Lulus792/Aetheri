# Own shell line editor

5 November 2026. The graphical terminal now starts `aetheri-shell -i --edit`.
This adds our own C17 single-line editing and in-memory command history to the
existing [shell language and job runner](shell.md). It has no UI-toolkit types,
history file, background service or added runtime library. Bounded undo/redo and
literal command/path completion are now available. A full modal editor, search,
advanced completion, panes and persistent sessions remain work.

## Use and controls

The explicit `-i --edit` profile requires an independently owned foreground
Linux N_TTY controlling terminal and an observed width of 16–4096 columns.
It uses the narrow cursor/erase profile understood by our [terminal](terminal.md),
independently of `TERM`; it does not advertise complete xterm compatibility.
Plain `-i` retains the earlier canonical kernel editor. Batch modes are unchanged.

| Control | Action in our line editor |
| --- | --- |
| Left / Right, Ctrl+B / Ctrl+F | Move by one extended grapheme |
| Home / End, Ctrl+A / Ctrl+E | Move to start/end |
| Backspace / Delete | Remove the preceding/following whole grapheme |
| Up / Down, Ctrl+P / Ctrl+N | Browse history; returning past newest restores the live draft and cursor |
| Ctrl+K / Ctrl+U | Cut from cursor to end / from start to cursor |
| Ctrl+W | Cut preceding ASCII-space-delimited text; this is not Unicode word segmentation |
| Ctrl+Y | Insert the retained cut text |
| Ctrl+L | Redraw the current line |
| Tab | Complete a literal command/path; list alternatives when the common prefix cannot grow |
| Ctrl+Shift+Z / Ctrl+Shift+Y in the graphical terminal | Undo / redo the current draft (PTY bytes 0x1f / 0x1e) |
| Enter | Accept the line, restore terminal modes, then parse/execute |
| Ctrl+C | Cancel the draft; retain history and report status 130 |
| Ctrl+D | End input if empty, otherwise delete the following grapheme |

History retains at most 64 copied entries and 131,072 allocated text bytes including
their NUL terminators. Empty and adjacent duplicate lines are skipped. An initial
ASCII space opts out of recording. This only affects this shell's history; it
does not conceal program arguments or file effects. Editing a recalled line does
not overwrite its stored entry. Allocation failure preserves the draft and old
history; the accepted command may still run without being recorded. A new copy is
allocated before eviction, temporarily adding at most 16,384 bytes. Exit wipes
owned editor/history buffers before freeing them. This is not a secure-memory or
whole-process forensic-erasure guarantee.

## Undo lifetime

Consecutive typing is one undo group, independently of input-read batching.
Cursor movement, deletion, cut/yank and completion establish separate edits.
Undo/redo restores original text and cursor without executing anything or
changing retained command history. New successful editing after undo discards
the old redo branch. Invalid input or allocation failure preserves it.

The combined undo/redo stacks retain at most 64 states and 262,144 requested
allocation bytes including snapshot headers and terminators. A pending snapshot
temporarily adds at most one complete line plus its header. Oldest reachable
history is evicted under these budgets. Allocation failure leaves text/cursor/
cut buffer and both stacks unchanged. The editor does not promise secure memory.

History traversal starts a fresh undo lifetime: undo can revert edits to a
recalled line but cannot traverse or rewrite command history itself. Returning
to the live draft preserves that draft and its cursor. Starting the next draft
after acceptance/cancellation, and whole-draft reset, clear undo/redo; these commands cannot revive a rejected
prefix. In the adapter an already blocked draft ignores undo/redo/Tab until its
existing explicit reset. Ctrl+Z retains its terminal job-stop meaning; the new
graphical chords are provisional and send control bytes to foreground programs
as well. They are not intercepted as editor commands by the terminal itself.

## Literal Tab completion

At a command word, Tab combines our builtins with executable regular files in
the shell's current PATH. Slash-containing commands use the selected directory.
Other arguments and redirection targets complete file/directory names; the first
`cd` argument suggests directories. Unset PATH uses `/usr/bin:/bin`; empty PATH
entries mean the current directory, matching execution. Duplicates are removed
and suggestions are byte-sorted. Hidden names require an explicitly typed dot.
Directory suggestions append `/`; a unique non-directory suggestion normally
adds a space. Multiple matches extend only their shared complete-grapheme prefix;
another Tab lists quoted alternatives when no further extension is possible.

Single/double quotes, escaped literals and an unfinished quote are supported.
The cursor must follow the token's literal content; closing quotes may follow
it, but a nonempty literal suffix is currently unsupported. Current-word variable
expansion, tilde expansion, globbing, plugins and programmable completion are not
implemented. Unsupported, unavailable or over-limit queries leave the draft
unchanged and explain the reason. No partial result is treated as unique.

Every replacement is encoded as a complete literal in our actual shell grammar,
including embedded quotes, dollar signs, spaces and metacharacters. Completion
never executes a candidate, changes cwd/environment, opens file contents or
authorizes a future filesystem operation. Metadata and permission checks are
advisory; a later explicit Enter still uses the normal parser/runner and may
observe changed files. Symlinks are followed for these hints, not trusted as app
or storage identity. Names whose followed metadata is unavailable are skipped,
including broken symlinks. Unrepresentable names follow the line profile's rejection
policy; this is not a complete arbitrary-byte filename editor.

One query retains at most 128 candidates/65,536 name bytes and checks at most 64
directories/32,768 entries. It uses a 250ms cooperative monotonic budget and polls
the existing signal/owner boundary between steps. Cancellation closes the owned
directory stream and leaves input untouched. Individual filesystem calls can
still block, particularly on remote/FUSE mounts; this is not a hard latency bound.
There is no completion daemon, thread, process, persistent cache or shell-script
evaluation. A fully nonblocking remote-filesystem provider remains later work.

## Rejected input

The line limit is 16,383 UTF-8 bytes. Invalid UTF-8, unsupported controls, invalid
key sequences and overflow block the entire draft: Enter cannot execute the
accepted prefix. Ctrl+U clears a blocked/partial draft; Ctrl+C cancels it.
Ordinary Ctrl+U retains its cut-to-start behavior. Valid multi-scalar input is
preserved as its original bytes, without normalization. LS/PS, C0/C1/DEL and
explicit bidi-formatting controls are rejected in this command profile.

## Components and ownership

| Component | Responsibility |
| --- | --- |
| [text/grapheme.h](../src/text/grapheme.h), [implementation](../src/text/grapheme.c) | Strict UTF-8 and Unicode 18 default extended-grapheme boundaries; no allocation, width, shaping or OS effects |
| [shell/editor.h](../src/shell/editor.h), [implementation](../src/shell/editor.c) | Single-owner draft/cursor, atomic edits, cut buffer and copied history; no terminal, execution or disk effects |
| [shell/readline.h](../src/shell/readline.h), [implementation](../src/shell/readline.c) | Bounded key decoder, viewport and acceptance/cancellation |
| [shell/jobs.c](../src/shell/jobs.c) | Original controlling-terminal ownership, private editor FD, checked mode transitions and signal/job events |
| [shell/completion.h](../src/shell/completion.h), [implementation](../src/shell/completion.c) | Read-only advisory lookup and quoted result snapshots; no editor mutation or command effects |

`ae_utf8_next` accepts a borrowed immutable span, including embedded NUL at this
generic layer. Failure preserves the offset and scalar output. `ae_text_graphemes`
returns byte boundaries including zero and the end; an empty span has one offset.
NULL output with capacity0 measures. Malformed input fails with count0 and no
output writes; insufficient capacity fails with the required count and no writes.
Input, output and count must not overlap. Keep input immutable across both passes.

The editor's borrowed text/boundary views expire on mutation or destruction.
Insertion validates a complete candidate before changing text. The cursor remains
on a grapheme boundary even when inserted text joins an adjacent cluster. The
fixed line/draft/cut buffers and worst-case boundary array are retained per editor;
only bounded history uses additional allocations. Property lookup is binary
search over 1,641 constant combined ranges. Segmentation uses two linear scans
with O(log 1641) property lookup per scalar. An edit rebuilds the bounded line;
there is no claim of an incremental large-document data structure.
`ae_line_replace` changes an existing grapheme-bounded range as one undo edit.
`ae_line_undo(-1/+1)` applies undo/redo; absent states are successful no-ops.
`ae_line_valid_utf8` checks the command-line text profile without OS effects.
The completion query borrows immutable, nonoverlapping line/PATH input, owns its
bounded result and clears results on error. Callbacks observe cancellation only;
they cannot mutate inputs or recurse. `ae_shell_quote` requires disjoint input,
output and required-size storage; capacity/validation failure writes no output
bytes. The adapter applies a result only to its still-owned synchronous draft.

The adapter retains ISIG while disabling canonical input, echo and software flow
control. It opens a separate nonblocking `/dev/tty` file description and joins
its actual device/session/foreground to the retained original. It does not set
O_NONBLOCK on the descriptors inherited by commands. Every mode transition checks
expected ownership/current modes and confirms the result. Before command dispatch
or normal editor return, original modes are restored. Foreign ownership/mode
drift causes failure rather than a takeover or fallback.

Input reads one byte at a time so accepting LF cannot consume the following
program input. Printable bytes are accumulated in a bounded buffer between
model/redraw operations. Escape decoding holds at most 32 bytes under an absolute
120ms deadline, not a deadline extended by each arriving byte. A lone expired
Escape is discarded; an expired partial key sequence blocks the whole draft.
Event-driven output uses the private FD, poll and the
existing signal channel; backpressure does not require a periodic polling loop.
Ctrl+C flushes only the still-owned input queue. Infrastructure failure exits
through the runner's existing shutdown path, reporting125 when appropriate.

## Graphemes and display are different contracts

Editing follows the Unicode 18 extended-grapheme rules, including Indic linker,
regional-indicator and emoji ZWJ sequences. This does not imply that the current
font/terminal can shape those sequences. Display uses the terminal's existing
glibc `wcwidth` cell profile. Leading/unrepresentable marks, ZWJ/variation forms,
too many combining marks or oversized clusters receive explicit `[U+…]` ASCII
fallbacks; very narrow space can use `?`. Original command bytes stay intact.

A horizontal viewport keeps the caret visible and marks omitted content with
`<`/`>`. It can skip an entire wide indivisible cluster to show the cursor.
Accepted lines are rendered through the same display policy, then submitted as
the original text. This is not font fallback, full bidi layout, accessible-text
integration or a complete multilingual editor.

## Reviewed data and reproduction

The original algorithm uses reviewed Unicode data rather than adding ICU or
another runtime Unicode library for this bounded segmentation task. This keeps
the chosen implementation and dependencies inspectable; it is not a claim of
superior performance or a plan to replace shaping/font libraries.

[Unicode inputs](unicode-inputs.json) pins official 18.0.0 grapheme properties,
Indic properties, emoji properties, all official boundary tests and the complete
license by size and SHA-256. These are locally observed HTTPS-content hashes,
not upstream digital signatures. The generated property/vector headers are
committed; ordinary builds need no network or generator. To check reproduction:

```text
python tools/prepare_vm.py fetch --profile unicode
python tools/prepare_vm.py verify --profile unicode
python tools/generate_graphemes.py downloads --check
```

Omit `--check` only to deliberately regenerate from those same pinned inputs.
The complete [Unicode License V3](../src/text/UNICODE-LICENSE.txt) accompanies
derived data; preserve it in source and binary/image distributions. It is
separate from the MIT license on original Aetheri code. No production packaging
is claimed by this prototype.

## Executed checks and remaining work

The [grapheme test](../tests/test_grapheme.c) checks all 853 official Unicode 18
vectors plus malformed UTF-8, capacity and unchanged-output failures. The
[editor test](../tests/test_shell_editor.c) covers real combining/ZWJ/flag edits,
aliases, draft/history ownership, allocation failure, undo/redo branch/lifetime
and count/byte eviction. The [completion test](../tests/test_shell_completion.c)
round-trips actual metacharacter filenames through the independent production
parser and checks PATH/type/hidden filters, cancellation, limits, no candidate
execution and unchanged FD counts.
The [native PTY test](../tools/test_shell_editor_native.py) has thirteen cases,
including actual child termios/flags/input, stop/fg/resize, rejection and exit
restoration. The [native Compose test](../tools/test_compose_provider.py) now has
twelve cases including real whole-grapheme deletion, Home/Delete/End, history,
graphical undo/redo chords and Tab.
The separate [allocation fault producer](../tests/shell_editor_allocation.c)
uses the same shell/adapter with a test-only malloc wrapper. Seven failed raw/CSI
delete/cut/completion paths must block execution until reset; the production target contains
no fault hook. A real pre-fix file effect is retained in the newer dated record.

All 29 affected groups pass Release and ASan/UBSan. A separate real virtual-DRM
session verifies original keyboard-to-file effects and pixel-exact cancellation
at 100/200 percent. Original failures, negative controls and precise evidence
identities are in the [dated record](../brains/aetheri/journal/2026-11-05-line-editor.md).
These results do not establish physical hardware, assistive use, full editor/
terminal compatibility, normal admitted app/session launch or isolation. Search,
advanced completion, the requested own modal editor, panes and persistence remain
required; final bindings/design still need product review. The
[undo/completion record](../brains/aetheri/journal/2026-11-05-line-assist.md)
records the newer candidate; the earlier record retains its original results.

Primary references: [UAX #29 revision 49, Unicode 18](https://www.unicode.org/reports/tr29/tr29-49.html),
the exact [input URLs/hashes](unicode-inputs.json),
[Unicode license](https://www.unicode.org/license.txt) and
[Linux termios](https://man7.org/linux/man-pages/man3/termios.3.html).
