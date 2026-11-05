# Aetheri-owned NexaUI Settings

6 November 2026. The three implemented Settings sections now have an Aetheri-owned
C17 NexaUI presentation. It retains the original toolkit-free model and selected
0.1.18 SDK at `e71ee304f1e6c23fa562d9e4b11f7e8e12152079`; no SDK source,
consumer pin, production service or default app selection changes.

## Implemented boundary

The sidebar, content hierarchy, muted blue-violet greys and grouped cards take
their direction from the accepted local studio at
`F:/Aetheri-UI-Konzepte/02-designwerkstatt/index.html`. The implementation exposes
display observations/selection/refresh, local themes/text size and local DE/EN.
Only these implemented sections appear. It does not simulate device settings,
updates or successful privileged operations. This is the opaque material fallback;
full glass, final visual acceptance and the other designed sections remain work.

`src/settings/nexaui_view.c/.h` owns layouts, semantic controls, bounded cached
labels and scroll/focus presentation. Model choices survive view replacement;
they remain process-local. Failed display snapshots retain their previous values
and disable selection. Selection follows actual observation IDs, not row order.
Overlong paint runs wrap at grapheme boundaries, with original semantic names.
Narrow navigation keeps three accessible icon controls; focus follows content.
Page Up/Down and Home/End expose read-only help without changing a selection.
Circular marks correspond to native radio roles/checked states.

`nexaui_main.c` adapts the selected upstream event/observation scaffold. Its MIT
notice is retained in `src/settings/NEXAUI-NOTICE.txt` and copied into the explicit
guest source snapshot. The native host retains the original model/font lifetime,
app ID, signal pipe and event-driven wait. Close and supported maximize/restore
requests are checked separately from observed state; unsupported minimize remains
disabled. `SETTLED` diagnostics distinguish a rendered stable model from the
earlier frame that accepted its change. No authority receipt follows from either.

The optional `AETHERI_NEXAUI_PREFIX` requires the exact installed 0.1.18 package;
the ordinary build gains no mandatory SDK dependency. It adds
`aetheri-settings-nexaui` beside the existing SDL executable. The private
development owner's `--settings-ui nexaui` still selects its previously bound
upstream consumer. A separate owned-view binding and actual DRM owner/lifetime
exercise are the next integration step, before changing any selection.
The later [owned-profile checkpoint](2026-11-06-owned-settings-owner.md) now
records that distinct binding and actual DRM exercise; the upstream option and
SDL default remain unchanged.

## Verification and retained corrections

Final regression: all 55 selected text/editor/shell/Files/Terminal/Settings/desktop/
compositor groups pass without skips, Release in 169.42 seconds and Debug ASan/UBSan
in 288.74 seconds. The four new Settings groups are included in both. Actual Ninja
C17 commands retain strict warnings and sanitizer instrumentation. The independent
peer's ELF dependencies contain libatspi and no NexaUI. Native view tests verify
the actual loaded SDK library against the unchanged reviewed build metadata.

The final audit joins 355 host/snapshot/guest source inputs, unchanged 28 frozen
SQL/XML files and nineteen protected old-runtime hashes without SQLite sidecars.
SELinux remains enforcing; the carrier is idle on tty1 with no build/test actors,
running Aetheri unit or seat0 session. The export index verifies 148 retained
files, including 108 native output images across both profiles and native/AT-SPI
runs. Representative native dark/light/minimum-size/reading images were inspected;
this is not final design or human assistive acceptance. No DRM session was started
for this new owned view.

`settings.view` and `settings.view_hidpi` together exercise 540 combinations of
three themes, two languages, three text sizes, five viewports (320x240 through
3840x2160), three pages and two controlled text raster densities. They also check
keyboard order/reverse traversal, focused-control visibility, read-only scrolling,
opaque text/focus contrast, view replacement, stable display IDs, stale refusal,
refresh consumption, unavailable window actions and long-name semantic bytes.
This matrix is controlled scene/raster evidence, not native mixed-DPI hardware.

`settings.view_native` drives real Wayland pointer/keyboard input on Aetheri's
compositor. It verifies actual display observations, page/theme/language changes,
200-percent text, own maximize/restore requests, 320x240 configure and focus
following, keyboard reading, idle frame/CPU observations, native close and missing
font/display refusal. Each native run retains nine captures on three outputs.
The test-only driver's output-height floor now admits 240 for this standalone
minimum-size case; production desktop output policy is unchanged.

`settings.view_atspi` repeats that workflow with a real private registry and
separately compiled C17/libatspi peer with no NexaUI/model linkage. It checks
native section actions, localized radio roles/state, actual display selection,
window geometry and Close. The same peer rejects a visible disabled-bridge app.
Verified private headers/runtime are reused; no installed bus/service changes.
Application and window can share a name: the independent peer now queries the
actual native child window's Component rather than the name-matching application.

Earlier per-attempt logs/images remain in the carrier's `settings-view-*` folders.
The first native fixture wrongly expected focus on unsupported minimize, sampled
an empty high-contrast corner, and requested 240 below its old driver limit.
The peer initially selected the identically named application for geometry;
its subsequent positive/negative run then met a harness cleanup expectation that
incorrectly required a daemon's SIGTERM exit to be zero. These test assumptions
were corrected without changing the SDK. Stable-frame diagnostics also prevent
capturing the old page immediately after accepting a model change.

The original combined Debug matrix hit its 30-second CTest boundary. A bounded
repeat completed all 540 cases in 29.782 seconds, exit0, with no sanitizer output
and 488744 KiB recorded peak RSS. The two raster densities are now separate test
groups, retaining the full matrix and the original 30-second bound per group.
The original timeout XML and measurement remain; no app performance acceptance
is inferred from an instrumented scene test.

See `local/settings-view-2026-11-06` for final regression, source/artifact binding,
images and protected-runtime audit. Normal session accessibility, human
screenreader/visual review, full window chrome/input, physical GPU/monitor/suspend
and end-to-end performance remain open. Real settings backends, other apps,
lifecycle/recovery/removal, OS image/installer/hardware and the complete persistent
resettable experiment desktop remain required. The old quarantined runtime stays
unresolved; keep the full project goal active.
