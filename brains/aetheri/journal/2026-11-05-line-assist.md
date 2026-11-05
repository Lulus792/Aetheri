# Bounded undo and literal shell completion

5 November 2026. Continued the original editor rather than changing UI toolkits.
The [line-editor guide](../../../spec/line-editor.md) documents current controls,
ownership, limits and private C contracts. No new runtime library, daemon,
production service/policy or SQL/XML contract is introduced.

## Result

The toolkit-free editor now groups consecutive typing, records separate cursor/
edit/completion boundaries and restores text/cursor through undo/redo. A new edit
invalidates redo; allocation/validation failure preserves it. The combined stacks
retain at most 64 snapshots/262,144 requested bytes including headers. History
navigation, reset and the next draft delimit their lifetime. Undo never changes
command history, executes a command or revives a rejected input prefix. The
graphical terminal sends 0x1f / 0x1e for provisional Ctrl+Shift+Z/Y. Ctrl+Z remains
job stop; native preedit/paste/selection routing keeps its existing precedence.

The separate Linux completion component proposes builtins/PATH executables,
slash-containing commands, argument/redirection paths and directories for the
first `cd` argument. It never executes candidates, opens their contents, changes
cwd/environment or mutates the editor. Literal quoting uses our actual grammar;
the independent production parser verifies metacharacter round trips. The adapter
applies one whole quoted edit to the still-owned synchronous draft. Common-prefix
extension stops at a complete grapheme; alternatives are sorted/deduplicated.

One query has explicit candidate/name/directory/entry budgets and a cooperative
250ms clock/cancellation boundary. An individual remote/FUSE/filesystem syscall
can still block: this is not a hard latency guarantee or fully asynchronous
provider. Permission/type results are hints, never authority for later execution
or storage access. Unsupported dynamic words and literal suffixes remain intact.
Advanced programmable completion, environment/tilde expansion, history search,
the modal editor, panes, persistence and accessible text remain work.

## Native evidence

Check root `/home/aetheri/build/line-assist-check-s3t_2jap`:

- Release **29/29**, 67.21s; final Debug ASan/UBSan **29/29**, 93.22s. No skips.
  Includes all text/shell/terminal/desktop and relevant Compose/native compositor
  groups. The earlier Debug 96.03s run passed before a header-only precondition
  clarification. The 70.24/88.71s and 66.09/92.64s checkpoints predate the
  final seven-path allocation guard; their original logs are retained. Release binary hashes remained identical after rebuild; Debug
  shell/completion test identities changed, so the Debug matrix was repeated.
  These are distinct results; no stale hash was assigned to the final console.
- `shell.editor`: grouped typing, cursor restoration, redo invalidation, failed
  allocation preserving redo, cut/yank, history/reset isolation and both undo
  eviction budgets, in addition to the previous Unicode/history checks.
- `shell.completion`: actual ordinary-user directory fixtures, executable/PATH/
  duplicate/hidden/directory filters, quotes/escaped syntax, closed/unfinished
  words, pipes/redirections, whole-grapheme prefix, cancellation, 128-candidate
  overflow, unchanged small-output buffers, no candidate execution and no FD
  count leak. All replacement words round-trip through the separate real parser.
- Actual own-editor PTY group: **13 cases** per profile. Added undo/redo plus
  completion/prefix-list ownership, executable candidate waiting for Enter,
  metacharacter/space filenames, nested directory tokens and blocked-draft
  rejection that undo/redo/Tab cannot bypass. Previous job/mode/resize/input and
  signal cases still run.
- Final review found a real adapter failure: raw Backspace/Ctrl+D/K/U/W ignored
  the editor's new allocation-failure result. The test-only normal-shell producer
  [shell_editor_allocation.c](../../../tests/shell_editor_allocation.c) fails
  exactly the second editor checkpoint using link-time malloc wrapping. With
  the original adapter, `touch failed-edit` executed after the failed Backspace;
  retained root `session-release/shell-interactive-_4be65b4` contains the actual
  file and transcript, and `allocation-before.log` records the unchanged failure.
  The adapter now blocks the entire draft on each failed raw deletion/cut and completion edit,
  including Ctrl+U; a subsequent explicit reset remains allocation-free. One
  native case tests seven raw/CSI/completion paths and successful fresh input afterward.
  The fault producer is a separate test target, never part of the normal shell.
- Actual native Compose/SDL/PTY provider: **12 cases**, six raw images per profile.
  Added real Ctrl+Shift+Z/Y without shell stop and actual Tab-to-file effects.
  Existing DE/EN composition, focus/keymap/device, selection/clipboard and owner
  cases remain exercised in their respective groups.

Two copied-source controls under
`/home/aetheri/build/line-assist-negative-gj06ot__` first pass unchanged tests.
The quote mutant returns bare text only for a semicolon-containing filename;
the unchanged production-parser round-trip assertion exits 1. The redo mutant
retains the old redo branch after a new edit; the unchanged state assertion
exits 1. Positive/negative binaries, exact commands, source copies and hashes are
retained. These controls do not dispatch app code or touch installed inputs.

## Ordinary virtual DRM session

Final original Debug desktop+Compose token `8a7c4ef7d19f42219d13dbd31882869b`, boot
`124d6ce8-6ff6-4ed8-a8c8-51aeaa87f056`, PID90469, invocation
`3feb04e060074da491423e2d3c6c96d8`, stdout stream
`506bbee5ecd9496b9e2cc3c0f9e3679e`, session-1527.scope.

Actual QMP PS/2 keys through logind/libinput/bochs-drm undo/redo a midline edit;
redo restores exact text/caret pixels, then explicit Enter writes `abc`. Tab
quotes an absolute filename containing a space, and original `cat` writes exact
`completed` bytes. Previous Home/Delete/End/history and whole-grapheme deletion
still work; Compose cancellation restores exact 100/200-percent baselines.
The first final screenshot pair straddled actual Delete/End presentation (old
`abxc` then `abc`), and the next file probe raised before the file appeared. Both
original images/probe records remain. Re-observed the same live unit; continued
its unsubmitted draft, then independently observed the exact committed `abc`
file before proceeding without re-executing that effect. Capture now observes
stable pixels and expected change/restoration within the existing bounded wait;
the file predicate distinguishes pending from an existing empty file. Assertions
and expected bytes/pixels remain unchanged. This is fixture synchronization,
not a product rendering fix or a new session. The earlier successful token
`a6e6f162be054c45a42c7fe57d0b3813` is separate pre-allocation-guard evidence.

Final compositor/desktop/terminal exit 0 and the controller closes that same token.
QMP remains protected host-local AF_UNIX with SDL/gl-off/window-close-false.

Local evidence is under `local/line-assist-2026-11-05`: native logs/JUnit/results,
negative controls, original start/stop/journal, stable PNG pairs and hash indexes.
The index verifies 673 copied files and 186 decompressed native images. The 22
console PNGs form eleven stable pairs; undo/redo and completed-token images were
visually inspected. The final read-only audit matches 303 host/snapshot/guest
sources, 28 frozen SQL/XML and 19 protected old hashes without SQLite sidecars.
All 983 local Markdown targets resolve, and the enforcing carrier is idle on tty1
without a known native app/test, running Aetheri unit or seat0 session.
The read-only final audit verifies original native/console binary agreement,
current sources, unchanged frozen/protected data and an idle enforcing carrier.
This is virtual software/native input evidence; physical devices, assistive use,
normal admitted sessions and trusted lock/resume are not accepted by these tests.

Continue toward history search and the own modal editor/panes/session workflows,
while retaining the separate OS image/installer, normal admission/recovery,
remaining apps, hardware and early full experiment-desktop requirements. The
quarantined old running/recover-handoff attempt remains unchanged. The existing
NexaUI list receives incremental requirements only; no new chat text is needed.
