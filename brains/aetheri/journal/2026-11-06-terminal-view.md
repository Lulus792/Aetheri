# Own NexaUI terminal presenter

6 November 2026. `aetheri-terminal-nexaui` draws the actual terminal/editor with
NexaUI 0.1.18/e71ee30, retaining the original SDL Wayland host and shared C17
session. This is an explicit executable, not a default desktop-owner migration.

## Implementation

The conditional presenter replaces rectangle/label/grid painting and clipping
in the original host. NexaUI parses the regular/bold fonts, shapes bounded cell
clusters with real glyph advances/mark offsets, paints the original cell styles
and rasterizes a complete RGBA8 canvas. SDL uploads that canvas through the
existing renderer. SDL_ttf remains for original host cell metrics. No second
window/connection/parser/input queue is introduced. Composition, clipboard and
paste review still use their existing original context/epoch/foreground checks.
The real selected-FD editor remains a separate foreground child and owns its
Save/Drop/Back choices. The original worker and child/stream cleanup paths remain.

The view owns bounded graphics/cache resources, not the terminal model or PTY.
Destroying it settles no process. Complete visible-row read-only semantic text
preserves large Unicode grids without the SDK's per-object length truncation;
row identity comes from the original content token. The host supplies whether
a live input caret is visible. Native AT-SPI transport/controls are not connected
yet, and clipped selections are not advertised as a complete text selection.

The bold face comes from the same reviewed Liberation Mono RPM as the existing
regular face; actual file/license identity is added to ui-inputs. No new library
family, SDK version, source-pinning permission or public API is introduced.

## Checks

The first strict build identified a missing cell-grid header and a misleading
one-line conditional; both were corrected without relaxing warnings. Focused
pixel/font/style/density checks then pass Release/ASan/UBSan. The initial five
native groups at `/home/aetheri/build/terminal-view-native-x_e19sy5` pass Release
53.48s and ASan/UBSan 71.62s. Those results precede the later row-semantics and
explicit native-presenter/SDK/font evidence checks; their exact source snapshot
and original source copies remain in `local/terminal-view-2026-11-06/`.

The final suite runs the original real workflows rather than a fixture-owned
terminal: native shell/output/selection/scale, whole paste review/copy and source
loss, IME/preedit/commit and the Files selected-FD editor/save/close workflow.
The wrapper verifies actual NexaUI-frame logs and ordinary binary symbols,
records the selected SDK library/builder metadata and checks font/license pins.
It preserves the original driver result and output. The final pixel test also
checks 100 complete semantic rows containing over 65,536 aggregate UTF-8 bytes,
with the original model unchanged. All 70 final app/UI groups pass Release
279.58s and ASan/UBSan 468.75s, without skips, at
`/home/aetheri/build/terminal-view-final-pdn6_7my`. No code/test changes follow
this run. The full original baseline and the five new presenter groups are
executed together; no installed/P0 lifecycle acceptance follows from this suite.

Four separately compiled Release/NDEBUG counterexamples omit grid rasterization,
remove the selection color, truncate a row and lower the canvas limit below the
valid dense workload. Each rejects at its expected pixel/row/budget check; the
unchanged control passes. Exact commands, sources, ELF hashes and logs remain at
`/home/aetheri/build/terminal-view-counterexamples-e17izj3k`. Release native ANSI
output, 200-percent German review and combining preedit images were inspected.

The final collector joins 373 host/snapshot/guest source inputs, 668 exports and
456 native images, unchanged 28 frozen SQL/XML and nineteen protected hashes
without sidecars. Normal SDL Terminal has no NexaUI dependency; only the new
presenter has its own adapter and SDK linkage, with no event/write-fault hooks.
Selected SDK library/metadata, actual presenter-frame logs and font/license
hashes join the native results. These metadata checks are not publisher or loader
provenance attestation. Actual mapped-image/default-owner admission remains work.

At 32-point text size, each native terminal sample records zero new frames and
zero CPU ticks during its two-second idle window; observed RSS is 164,440 KiB
Release and 331,864 KiB instrumented. These are scoped carrier observations,
not paired hardware or final resource/performance acceptance. The actual dirty
editor Save/Drop/Back on output 1 was inspected too.

All build/private-registry actors ended. The enforcing carrier is idle on tty1
with no Aetheri service or seat0 session. Closed development records and foreign
files remain unchanged; no new DRM session was started. The existing NexaUI
catalogue receives only this scoped adoption result, with all 103 IDs unchanged;
no SDK code, version update, other-project publication or new chat handoff.

## Remaining scope

The current opaque geometry is retained for this first presenter. The accepted
compact chrome/glass design, truthful startup system-information mark, tabs and
panes, native assistive host transport, compositor/backend optimization, physical
input/performance and full SDK adoption remain work. Missing glyphs retain .notdef;
this does not add broad font coverage or a general terminal bidi/shaping engine.
The complete OS/apps/image/installer/update/hardware/experiment desktop objective
remains active. Preserve the old quarantined lifecycle runtime.
