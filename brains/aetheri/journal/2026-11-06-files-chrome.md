# Files window gestures

6 November 2026. Own NexaUI Files now follows the tested Settings title/edge
gesture behavior. The view keeps its current design, controller, SDK 0.1.18 and
SDL default. No compositor or production lifecycle interface changes.

## Behavior

The host retains an original primary title press and requests one native move
after four logical pixels. A double click within 450 milliseconds/six pixels
requests work-area maximize/restore, including the maximized title. Unsupported
requests retain the existing safe result handling; a maximized drag does not
invent restore-and-move behavior. All eight border directions remain distinct.

Pending gestures recheck the view's current modal admission. Opening a dialog
while holding the caption prevents a later move or transferred release. Escape
cancels a pending caption and consumes its repeats/release. During an admitted
move/resize, the original compositor grab handles cancellation and restores
geometry only under its existing ordinary-mode/constraint checks. Further held
motion stays inert. A title-origin release cannot activate Close; a body-origin
press cannot be adopted as a title drag. Focus/capture loss, resize and mode
changes reset pending click state, and keyboard/other-button input breaks the
double-click sequence. SDK native-press/serial checks remain unchanged.

These interactions never cancel a filesystem worker, infer rollback, end an app
lease or settle the old quarantined execution attempt.

## Verification

The new C hit tests reject the unchanged earlier view at the maximized-title
assertion. Positive cases cover all eight edges, controls, ordinary/maximized/
fullscreen/unsupported modes and modal blocking. Both font-density tests retain
their previous editing/layout/stale-action checks.

The actual private Wayland workflow checks title double-click geometry, sixteen
edge out/back resize operations, pending-title Escape with a retained filter,
native move/resize cancellation, dialog opening during a held caption,
title-to-Close/body-to-title ownership and fresh pointer use after device loss.
The independent AT-SPI client and real create/rename/move/editor workflow still
run afterward. Separate fault/owner cases retain unknown outcomes, original
worker/document close obligations and bound fresh app starts.

Initial seven-group runs pass Release 42.32s and ASan/UBSan 81.91s, without
skips. The final test-only refinement adds a captured move before cancellation
and exact background/position/bounds pixels. Its first selected sidebar pixel
was actually the highlighted root item; retained raw RGB and view observations
identify the incorrect fixture expectation. The corrected probe uses the free
five-pixel sidebar margin. No product change, timeout increase or weakened
geometry check follows from that correction. Original failed runs are preserved.
The two final native groups pass Release 31.33s and ASan/UBSan 57.05s. The
snapshot comparison confirms that only the Python driver differs from the
seven-group runs; product and C test bytes are unchanged.

Evidence is local under `local/files-chrome-2026-11-06/`; the original guest root
is `/home/aetheri/build/files-chrome-5vb66dhr`. Native test input and software
pixels do not establish physical monitors, final material quality, normal
session or human screenreader acceptance. Full accessible text, the remaining
apps/services/image/installer/hardware and experiment-desktop goal remain open.

The final collector verifies 366 host/snapshot/guest inputs, 244 exports including
168 native images, unchanged 28 frozen contracts and nineteen protected hashes
without sidecars. Moved/restored position and held-caption dialog images were
inspected. All build/private-registry actors ended; the enforcing carrier is idle
on tty1 without an Aetheri service or seat0 session. Closed DRM history and the
foreign files are unchanged. No new DRM session was started, and the protected
old `running/recover-handoff` attempt remains unresolved.
