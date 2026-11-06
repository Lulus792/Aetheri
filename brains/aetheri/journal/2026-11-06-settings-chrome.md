# Compact Settings chrome and native window gestures

6 November 2026. The own Settings presentation now has compact left-side window
controls and ordinary title dragging and border resizing. Shared pure geometry
keeps painting, button targets and edge/cursor hit tests consistent. Close,
Minimize and Maximize/Restore retain localized semantic names, visible symbols
and keyboard focus. Unsupported Minimize stays disabled on this compositor.

The native host retains a pending title press until the drag threshold, then
requests one native move through the SDK's original held input. Border presses
request one of eight resize directions. A caption-origin release cannot become
a button activation, and body-origin input cannot become a caption gesture.
Double-click requests work-area maximize/restore. The reviewed 0.1.18 SDK pin,
original model, frozen five-source owner profile and SDL default are unchanged.

Escape cancels either a pending caption gesture in the app or the original
compositor-owned move/resize. Repeats and release are consumed; subsequent
motion under the held press cannot resume the operation. Restoration requires
unchanged ordinary mode and size constraints. Below 320x240, a clipped hint
and Close replace the page; stale settings actions are absent and the original
model returns on enlargement. The pure boundary is tested down to 64x64.

## Evidence

All 56 affected app/desktop/compositor groups pass Release in 186.46 seconds and
ASan/UBSan in 323.22 seconds, without skipped or failed groups. The only later
change is a test-only refinement from any device enter to an actual replacement
pointer enter; both native Settings groups are checked separately per profile.
No compiled source or DRM image changes after the full regression.
The final two-group runs pass in 45.10 seconds (Release) and 119.29 seconds
(ASan/UBSan), without skips. Both native profiles observe zero app CPU ticks
during their bounded 1.2-second idle sample; this is not a hardware benchmark.

The existing layout suites retain 540 theme/language/text/viewport/page cases
split between 1x and 2x raster density. They now also check compact geometry,
all eight hit edges/cursors, button precedence, capability/mode restrictions
and below-minimum views without stale actions or lost model state.

Each native Settings run retains nineteen states on three virtual outputs.
Actual Wayland requests, configure observations and exact compositor-pixel
probes verify displacement, growth and return. Tests cover all eight edges,
pending/native Escape and repeat, caption/body ownership, genuine double-clicks,
device removal/rebinding, narrow keyboard reading, a 240x180 fallback and
restoration. The independent libatspi/real-registry action test and its
disabled-bridge negative remain. Missing font/display checks and bounded idle
checks remain enabled.

The expanded test initially exhausted the compositor's sixteen-capture budget
before the tiny-view image. Original failures are retained. Only the test
compositor now allows thirty-two captures; production stays at sixteen. The
device-replacement test waits for the actual new pointer enter rather than
assuming that a driver acknowledgement means the client has rebound it.

The fresh actual DRM development token `84a8ed394f664ab19cdc67b88f0eeaca` proves
the new own image/library selection, original-process reuse, retained Light
choice, fresh default model after close, exact Terminal-saved bytes and Files
coexistence. German/high-contrast/200-percent views maximize above the dock and
restore to 1000x636. All five roles and the Root controller finish with status
zero; retry returns unchanged historical closure. Twenty-four stable PNGs and
the original process snapshots are retained. These QMP/PS2 observations are
separate from the private native-pointer tests, not physical input evidence.

Local evidence is under `local/settings-chrome-2026-11-06`, with the original
guest checks under `/home/aetheri/build/settings-chrome-xa_pt6a0`. See
[the contract](../../../spec/nexaui.md#window-chrome-and-gestures).
The collector verifies 355 host/snapshot/guest source inputs, unchanged 28 frozen
SQL/XML and nineteen protected runtime hashes without SQLite sidecars. Its 142
exports include 114 native images. The enforcing carrier is idle on tty1 with
no build actors, running Aetheri service or seat0 session. Prior closed developer
history remains unchanged; old `running/recover-handoff` remains unresolved.

## Remaining work

The material is still opaque. Full glass, fullscreen entry, snapping/drag-out of
maximized windows, human visual/screenreader and physical monitor/input/driver
acceptance remain open. W07 in the existing NexaUI catalogue now records the
missing public fullscreen-entry request, independently checked against SDK main
`9416e2a`; maximize is not presented as fullscreen. No new chat handoff is needed.

Normal settings persistence/device services, other app views, normal session
admission, durable lifecycle/recovery/removal, installable image/installer,
hardware and the persistent resettable experiment desktop remain required.
The quarantined old runtime remains unresolved and must not be retried or retired
on the strength of UI tests. The full project goal remains active.
