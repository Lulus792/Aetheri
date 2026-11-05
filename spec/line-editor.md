# Own shell line editor

5 November 2026. The graphical terminal now starts `aetheri-shell -i --edit`.
This adds our own C17 single-line editing and in-memory command history to the
existing [shell language and job runner](shell.md). It has no UI-toolkit types,
history file, background service or added runtime library. A full modal editor,
completion, search, undo, panes and persistent sessions remain work.

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
| Tab | Insert four spaces; completion is not yet implemented |
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
aliases, draft/history ownership, allocation failure and both history budgets.
The [native PTY test](../tools/test_shell_editor_native.py) has nine cases,
including actual child termios/flags/input, stop/fg/resize, rejection and exit
restoration. The [native Compose test](../tools/test_compose_provider.py) now has
ten cases including real whole-grapheme deletion, Home/Delete/End and history.

All 28 affected groups pass Release and ASan/UBSan. A separate real virtual-DRM
session verifies original keyboard-to-file effects and pixel-exact cancellation
at 100/200 percent. Original failures, negative controls and precise evidence
identities are in the [dated record](../brains/aetheri/journal/2026-11-05-line-editor.md).
These results do not establish physical hardware, assistive use, full editor/
terminal compatibility, normal admitted app/session launch or isolation. Undo,
completion/search, the requested own modal editor, panes and persistence remain
required; final bindings/design still need product review.

Primary references: [UAX #29 revision 49, Unicode 18](https://www.unicode.org/reports/tr29/tr29-49.html),
the exact [input URLs/hashes](unicode-inputs.json),
[Unicode license](https://www.unicode.org/license.txt) and
[Linux termios](https://man7.org/linux/man-pages/man3/termios.3.html).
