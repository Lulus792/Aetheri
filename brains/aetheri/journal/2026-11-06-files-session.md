# Shared Files application controller

6 November 2026. The working SDL Files view now uses an original C17 application
controller in `src/files/session.c` and `session.h`. Twenty-nine existing
application functions moved out of the window implementation: navigation feedback,
held naming/rename/move state, single-worker completion, uncertain outcomes and
the document-child close fence. Rendering, toolkit events and native window/font
resources remain in the SDL adapter. The lower Files model, create, rename/move
and launch implementations are unchanged.

This is concrete preparation for a NexaUI view sharing the same behavior. There
is no NexaUI Files executable yet, no change to the selected SDK/default view and
no new daemon or background thread. `aetheri_files_session` links the existing
model without an SDL/NexaUI dependency; the Release controller test loads only
libc and libm. The internal mutable state is not an installed SDK, permission
grant, durable session or production authority.

## Worker and ownership boundary

The UI owner initializes the state once and retains it, its model, startup strings
and original adapter context. A successful callback start owns one worker; a
false return promises that none started and no operation applied. The existing
SDL adapter keeps its original thread handles, operation-specific thread names
and event notification. Worker input stays immutable until the owner observes
release/acquire completion and joins that exact worker before consuming results.
Close during work waits. Unknown effects remain preserved and block new mutation;
view disposal does not undo or remove them. Live documents remain independent
close obligations.

The now separately callable selection helper handles the full signed-int range
without negating INT_MIN. Out-of-range menu action selectors are refused. No
filesystem durability, grant, child-observation or uncertainty rule is relaxed.
See [the application contract](../../../spec/files.md#application-controller-and-presentation).

## Verification

All 57 affected app/desktop/compositor groups pass Release in 191.35 seconds and
ASan/UBSan in 318.76 seconds, without skips. Each native Files run has 61 cases
and 48 retained images, with zero app CPU ticks in its bounded one-second idle
sample. This is not a physical performance benchmark.

The new headless `files.session` case checks a failed worker start without effects,
deferred execution, original join, close while pending, real rename/move with exact
bytes, an actual mkdir whose acknowledgement is lost, rejected name prefixes and
the document-close fence. Controlled callback/PID state is not claimed as process
admission. The unchanged native Files suite separately covers actual SDL threads,
native input, original document/editor lifetimes and fault injection.

The new actual DRM development token `4003d80b2a464ae3af7ce2dcbcec7a3d` passes
inline file/folder creation, direct document edit/save with exact bytes, list/grid
switching, an inline directory rename retaining its inode, German 200-percent
layout, information overlay, fullscreen and return. A fresh Files lifetime sees
the saved workspace. Its original Files/desktop/compositor and Root all end zero.
Sixteen stable PNGs and original Files/Terminal/editor image/process snapshots
are retained under `local/files-session-2026-11-06/drm`. The source/binary checks
are developer consistency evidence, not production sealed-code admission.

The guest checks are retained under `/home/aetheri/build/files-session-5ux9p51a`;
local reports are under `local/files-session-2026-11-06`. Prior protected runtime
state and unrelated project edits must remain untouched.
The audit verifies 358 host/snapshot/guest inputs, unchanged 28 frozen SQL/XML
and nineteen protected hashes without SQLite sidecars. Its 108 exports contain
96 native images. The enforcing guest is idle on tty1 with no build actors,
running Aetheri service or seat0 session. Original closed developer history is
unchanged. Ninja's generated commands record C17/strict warnings and both build
profiles; the tested controller executable's dynamic imports are checked separately.

## Next

Implement the own NexaUI Files presentation against this shared controller and
the accepted studio: list/grid, held locations, filter, inline naming, menus and
read-only information. Preserve existing rejected-input, pending/unknown-operation,
document-close and selection behavior. The existing W07 fullscreen-entry SDK gap
remains a migration gate; do not silently substitute maximize for fullscreen or
drop working SDL behavior. No new handoff/catalogue is required for this extraction.

Real file grants, normal app/session admission, durable restart recovery/removal,
full glass, human assistive/physical acceptance and the broader OS/apps/image/
installer/hardware/experiment-desktop goal remain required. The quarantined old
`running/recover-handoff` intent is unrelated and remains unresolved.
