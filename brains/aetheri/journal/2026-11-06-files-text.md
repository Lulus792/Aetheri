# Native text access for Files fields

6 November 2026. Own NexaUI Files now uses actual SDK text fields so native
clients can read and edit their text, caret and selection. SDK 0.1.18/e71ee30,
the controller, original operations and SDL default remain unchanged.

## Model and publication

The old custom field painter/selection code is replaced by one SDK editor per
input, with history disabled. Aetheri keeps the validated name/query needed by
its operations. At frame end it checks the complete SDK candidate against the
existing 255-byte/content policy. Invalid or over-limit candidates restore the
prior bytes and mark the field invalid/read-only until explicit clear. The SDK's
65,536-byte transport limit is not Aetheri's accepted name/filter limit.

Changed candidates discard simultaneous old actions and rebuild once with fresh
scopes before rendering/native publication. A native text setter is not a
confirmation or file-operation result. Naming/filter Enter and dialog-control
Enter now follow that frame path too. A separate negative test found the old
direct Enter path reaching a controller start after focus loss; late keydown is
now ignored, and that regression requires no start attempt.

There is one editor rather than independent SDK/app caret and undo stacks. The
view's cached validated bytes are a reconciliation boundary, not a second editor.
The pinned field descriptor has no description member; the immediate named group
retains the localized rejection explanation. The actual entry owns native text
interfaces and validation/read-only state.

## Executed checks

The unchanged earlier view rejects the new native-set-value capability assertion.
Controlled C checks bind actual SDK changes to controller bytes, reject a queued
old confirmation accompanying a new native value, retain invalid input, refuse
writes until clear and expire a request after dialog replacement. Both font-density
suites retain editing, Unicode, geometry, modal and stale-action cases.

The independent C17/libatspi client links neither NexaUI nor the app model. Through
the actual private registry/compositor it checks Text/EditableText on the real
entry: Unicode value/character count, scalar caret versus grapheme boundaries,
selection add/change/remove, range delete/insert, defunct old targets and a
256-byte replacement retaining the previous Unicode draft as invalid/read-only.
An actual keyboard clear restores editing. Native Search replacement/clear is
joined to real filtered-entry counts. The ordinary file/create/rename/move/editor
workflow and the separate fault/owner groups still run.

The first native-client build used a non-existent role constant; the fixture's
headers require ATSPI_ROLE_GROUPING. C fixtures were also corrected to respect
the documented event/frame boundary after clear and the SDK's four-pixel input
inset. A filter-drift case now changes the authoritative model before the next
frame directly. The original failures and temporary diagnostics are retained;
they are not SDK Unicode failures, and no timeout/validation guard was relaxed.

Local evidence: `local/files-text-2026-11-06/`. Earlier seven-group runs at
`/home/aetheri/build/files-text-final-rauipsi8` pass Release 52.31s and ASan/UBSan
103.52s before the final Enter-path refinement. Final execution root:
`/home/aetheri/build/files-text-admission-h_zt38x6`. Its exact final results and
source/native/protected-state audit are retained separately.
All seven final groups pass Release 51.49s and ASan/UBSan 103.25s, without skips.
The collector verifies 366 host/snapshot/guest inputs, 263 exports including
180 native images, unchanged 28 frozen SQL/XML and nineteen protected hashes
without sidecars. The native Unicode rejection/query, long-name caret and German
minimum-size information images were inspected. The test font displays the emoji
as a missing-glyph box; semantic Unicode correctness is not font coverage.
All build/private-registry actors ended. The enforcing carrier is idle on tty1
without an Aetheri service or seat0 session. Closed DRM history and foreign files
are unchanged; no new DRM session was started.
The existing NexaUI catalogue received only this scoped adoption evidence and
the pinned description/styling limitation under existing L04/A04/A06/D01.
No SDK code, requirement IDs, other chat or replacement handoff was created.

## Remaining scope

This is a tested native text profile, not complete human screenreader or normal
desktop/session acceptance. Full native geometry/events, external IMEs, clipboard,
word shortcuts and field undo remain open. Final materials/physical hardware,
normal app/document admission/recovery/removal and the full OS/apps/image/
installer/updates/suspend/multimonitor/experiment-desktop objective remain required.
The old quarantined lifecycle attempt is unrelated and unresolved.
