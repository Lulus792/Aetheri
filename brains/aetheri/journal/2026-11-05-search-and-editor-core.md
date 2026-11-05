# History search and modal editor core

5 November 2026. The actual shell now has bounded history search. Original
document/normal-insert libraries begin the own file editor; they have no
executable view or file I/O yet. See the [line editor](../../../spec/line-editor.md)
and [editor core](../../../spec/editor.md) for current contracts and limits.

## Scope

Search has an independently owned <=1,024-byte literal UTF-8 query. Preview never
changes the draft/cursor/undo or command history. Ctrl+R/S and decoded Up/Down
traverse matches without wrapping; no-match has no admissible old candidate.
Ctrl+G/lone Escape restores the original state. First Enter only accepts a match;
a further Enter submits. Query overflow/invalid input retains whole-draft
rejection. Search allocates nothing, reads no files and has no persistent index.

The document model owns a copied gap buffer, dynamic line offsets and monotonic
content revision. Its first limits are 8MiB, 65,536 logical lines and 65,536 content
bytes per line. Strict UTF-8/TAB/LF/CRLF are preserved, including mixed endings
and missing final LF. Grapheme-boundary edits/cursors handle joining clusters;
all allocation/size checks precede single-owner mutation. Borrowed split spans
are not C strings. A clean-generation acknowledgement is not a save or grant.

The separate controller has Normal/Insert modes, whole-grapheme edits, motions,
logical vertical goals and line actions. It borrows its caller-owned document.
Tests cover preserving text/mode/cursor after failed line growth. Neither library
opens/saves files, parses terminal keys, renders, invokes plugins or authorizes
storage. File undo, actual view/key interpretation, I/O/conflict/unknown-save
handling, unsaved-close UI, panes and normal app/session admission remain work.
No new runtime library, thread, service, policy or SQL/XML contract is added.

## Executed checks

Root `/home/aetheri/build/history-search-check-ji5_5ww9`:

- Final Release **31/31**, 67.19s; ASan/UBSan **31/31**, 91.85s. No skips. Includes
  original text/shell/terminal/desktop and relevant Compose/compositor groups,
  plus `editor.document` and `editor.modal`.
- Own-editor PTY group: **16 cases** per profile. Search adds two-stage accept/
  submission, both traversal directions/arrows, exact cancel draft/cursor,
  lone Escape, actual UTF-8 query/backspace, no-match stale-candidate rejection,
  overflow/reset and undo isolation. Previous raw/OOM/job/resize cases still run.
- Native provider group: **13 cases**, six raw images per profile. Real Ctrl+R,
  typed query and actual PTY cursor prove search rendering; the first native
  Enter leaves the old proof absent, a second Enter produces exact `abc`.
- Document model compares **4,000** deterministic edits with a separate flat
  byte reference. Actual8MiB, maximum-line/count, constructor/both-growth OOM,
  byte-exact endings, boundary/range and joined-cluster cases pass. Modal tests
  cover actual modes/motions, short-line goal retention, line actions/CRLF,
  cluster joining and failed-open state. These are core tests, not editor UI or
  file-save acceptance. No measured speed superiority is claimed.

Separate copied-source controls under
`/home/aetheri/build/history-model-negative-7gfqagb9` first pass unchanged tests.
The search mutant returns the selected line for execution on the first Enter:
the unchanged native case exits 1 and the unexpected `search-new` file contains
`new`. The index mutant omits shifted tail offsets; the unchanged document
reference assertion exits 1. Source copies, commands, licenses, test hashes and
positive/negative binaries/results are retained. No installed authority/profile
or protected catalogue is used for either mutation.

## Ordinary virtual DRM search

Original Debug desktop+Compose token `42da204e3d6c47d8ad5b08da8af2abc5`, boot
`124d6ce8-6ff6-4ed8-a8c8-51aeaa87f056`, PID95324, invocation
`e4eac3da49ab42868f07861548cc4066`, stdout stream
`0ae29bff9e6d4704a48aedcf32a398d7`, session-1588.scope.

Actual QMP PS/2 keys through logind/libinput/bochs-drm render query and selected
draft, preserve absent proof until a further Enter, then produce exact `history`.
Ctrl+G restores draft/caret pixels exactly; subsequent midline typing produces
`abc`. Search/selection also survives 200-percent resizing. Original compositor/
desktop/terminal exit 0 and the same controller attempt is closed. QMP remains
protected host-local AF_UNIX, SDL/gl-off/window-close-false. This is the existing
terminal plus new shell search, **not a graphical modal-editor/file-I/O run**.

Local results, stable images, original records, native matrix/negative controls
and read-only preservation audit are in `local/history-search-2026-11-05`.
The index verifies 688 copied native files/186 decompressed native images and 14
final console PNGs in seven stable pairs. Query images at 100/200 percent were
visually inspected. The read-only audit matches 309 host/snapshot/guest files,
28 frozen SQL/XML and 19 protected old hashes without SQLite sidecars. All 1,010
local Markdown targets resolve; original native/console binary digests agree.
The enforcing guest is idle on tty1 with no known native fixture/app, running
Aetheri unit or seat0 session after original-attempt retirement.
They do not establish physical keyboard/monitors, human accessibility or trusted
login/lock/resume/normal admitted applications. The quarantined old running/
recover-handoff intent remains outside this work and must not be settled by UI
tests. Next connect the tested modal core to explicit file I/O and terminal
presentation; the complete OS/apps/image/installer/experiment-desktop goal stays
required. NexaUI receives incremental catalogue updates, no renewed chat text.
