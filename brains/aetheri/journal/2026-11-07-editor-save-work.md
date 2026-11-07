# Original background Editor saves — 7 November 2026

Named and first saves now run in a bounded on-demand C17 worker while the native
Editor retains its editable original document. The worker owns copied text and
the original file adapter; it never reads live UI/workspace state. This advances
responsive file I/O without replacing the accepted full OS scope. File loading,
folder enumeration and startup file reads remain synchronous and are next.
Read the [contract](../../../spec/graphical-editor.md).

## Saved state and original ownership

`document.h` adds an owned private snapshot plus original history checkpoint.
Capture freezes coalescing at that saved boundary. Actual file success can later
acknowledge the captured state, preserving newer dirty edits and their adjacent
typing group. Undo can reach the saved state; an acknowledgement retry cannot
restore an older clean marker over a later one. This is a model correlation, not
external save evidence or a grant. Original owner lifetime and serialized commits
remain caller obligations.

`workspace.h` prepares one original save job. Named saves loan their original file
adapter. First saves own the original duplicated parent/literal leaf and borrow
fixed incumbent adapters for duplicate checks. Target/borrowed close and competing
filesystem operations are blocked; live editing, tab changes/new tabs and unrelated
unborrowed closes remain possible. A known failed start can abort before apply.
Started writes are never cancelled, replayed or treated as rolled back.

Apply runs once on the worker. Release/acquire done publishes its result, but the
UI joins the original thread before finish/free. Pipe wake is advisory. Finish
returns the adapter/UNKNOWN obligation only to the original retained tab and
acknowledges only actual successful saving. Destruction refuses active ownership.
A close waits for that job; Escape cancels the close intent. Review Save closes
only the exact still-pending original ID/revision/serial. Cancelling that review
keeps editing available while the write continues; a late completion cannot close
the changed context. Error status retains the original document name.

No dependency family/version, persistent thread/service, SQL/XML or installed
capability changes. The existing pinned NexaUI/graphics inputs are reused.
Snapshot copying is bounded synchronous memory work, not disk I/O or a universal
maximum-file latency guarantee. Full recovery after display/process failure,
unknown outcomes and normal grant/admission integration remain open.

## Executed evidence

The original Fedora44 Enforcing carrier/boot remains unchanged. UID1000 code runs
in separate bounded 768 MiB work and 256 MiB `/tmp` private mounts. The latter
avoids the already attributed foreign quota exhaustion; no foreign files or
global policy/packages are changed. The new native save target shares the app's
actual `graphical_main.c` and adds only a separate controlled fsync adapter.
The normal `aetheri-text-editor` has no pause/fault hook.

Full-matrix token `0a1c3ba1895c4251a6d3041dd146780b`, helper PID1036385,
host handle75846 terminal0. All 469 source inputs are bound in local
`intent-native-v4.json`. Source archive SHA256:
`6e4ad970aa1293768305e44bb971dfa3b8a9c798921dc7bdf212f1bdbe581353`.

| Executed check | Release | ASan/UBSan |
| --- | --- | --- |
| All ten editor groups, no skips | 2.982 s | 18.328 s |
| Existing native window workflows | 8.401 s | 22.710 s |
| Fourteen native clipboard cases | 10.003 s | 24.365 s |
| Nine controlled native save attempts | 2.591 s | 14.646 s |

The new `editor.save_work` group uses actual C17 threads and a controlled blocked
fsync inside the original file-save path. Main-thread edits, original-tab array
movement, retained file/close guards, real saved bytes, saved-state Undo/Redo,
wrong-owner/early finish, exact apply-once, pre-start abort/allocation failures,
create conflicts, review cancellation and post-rename uncertainty are checked.
Unknown is separately tested with clean text. Late acknowledgement also preserves
typing performed on both sides of completion as one newer undo group.

The native UI test pauses real fsync while actual Wayland keyboard/buttons drive
continued typing, F6, new/other tabs and pending/review close cancellation. Actual
saved bytes exclude later edits and those edits remain dirty. It checks successful
original review close, first-save name collision followed by an explicit different
destination, an externally changed file, and clean-text UNKNOWN with disabled
discard. This is controlled scheduling/fault injection, not physical slow-disk,
power-loss, production recovery or general app/lease-drain acceptance.

The final UNKNOWN actor is pidfd-administratively killed only after native worker
join/finish and retained exact file bytes; it is not a successful normal close.
Core tests separately retire owned C memory after joined work. Do not infer an
LSan clean normal shutdown for that intentionally killed UI variant. Ordinary
window/clipboard applications complete their normal shutdown checks separately.

Actual saving-state images were inspected: later text/caret remain visible while
the original tab/footer show saving. The worker is paused before publication;
the driver independently verifies the old destination bytes. This does not claim
final glass/icon design, complete accessibility or physical hardware acceptance.

All 375 full-matrix evidence files are independently host-hashed before the original
root-protected token/source/archive acknowledgement, then exact private mounts/
control retire. Final archive SHA256:
`4554e583747017b3f7b8ae797c34e114e64672ebea7eafda04d957c5f74fbf30`
(4,671,018 compressed bytes). Originals, verified manifest, dispatch, source and
images remain in `local/editor-save-2026-11-07`.

Final API review also rejects acknowledgement after accidental modification of
the private snapshot body. File-save clean marking is permitted; changing its
content revision cannot mark the original captured history state as saved.
The negative test leaves original bytes and dirty state intact. Token
`bfc78df055fe4197808ac9729321355e` reruns all ten editor groups and the nine native
save attempts in Release (3.304 s / 2.564 s) and ASan/UBSan (18.735 s / 14.426 s).
All pass. Unchanged window/clipboard suites retain their v4 attribution above.
All 279 v5 evidence members and ten changed source/build/test inputs are
independently rechecked against the original manifest; its exact retirement is
recorded. Source SHA256:
`1ef13308dbb83a6667bcd25bd67fe2ee74be9a08e55f1e7e821e49feee039aa1`.
Evidence SHA256:
`d93b7403cfcd1ba49e3200b878ac2b19cb1bdff29293db399334e8b1447d8728`.

## Earlier boundaries and tooling gap

- The separate core-v1 run passes both native profiles/all ten groups before UI
  integration; its original 251-file export is verified and retired.
- Native-v1 strict warnings reject a diagnostic potentially bypassed by the
  small-window fallback. The first scope correction in v2 is misplaced and also
  rejected. V3 initializes it in the correct frame scope and passes the full
  matrix. No warning suppression is used; both failed exports remain attributed.
- Final review adds the late-ack/newer-typing grouping check and conditional
  coalescing behavior. V4 above reruns the affected complete matrix successfully.
- ThreadSanitizer's GCC linker script exists, but its actual carrier runtime
  `/usr/lib64/libtsan.so.2.0.0` is missing. The explicit TSan configure cannot link
  even CMake's empty compiler probe; project code is not run. Ten original files
  are verified/retired under token `cab3e2eb46f740b3a93df3b7126a5b26`, export SHA256
  `702f7597f4e0c0f80d431916668a7cd5097322bf83bcc2009bf6063dd5a5c045`.
  No TSan/race-detector acceptance or global runtime/policy change is claimed.

The read-only audit preserves 445 normal guest source inputs, 31 frozen SQL/XML,
19 quarantine hashes, 52 normal P0 files/controller, old SDK0.1.18/SDL04 and exact
original CLOSED DRM history. Four foreign planning/design files are excluded.
The prepared developer disk and live foreign work are untouched. No new NexaUI
catalogue requirement or handoff is generated.

Responsive original file loads and folder navigation/scan remain necessary,
preserving their held selection and original-result admission. The next delivery
priority is the own-image boot path toward the requested laptop alpha; app polish
does not need to precede the first serial boot proof. Save As/export,
abnormal/UNKNOWN recovery, normal app/session admission and the full OS/apps/
image/installer/hardware/experiment product remain required. Goal remains active.
