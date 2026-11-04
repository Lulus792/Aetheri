# NexaUI suitability

## Current decision — 4 November 2026

The user prefers a practical foundation such as SDL3 for early Aetheri UI, with
later NexaUI migration where useful. Missing NexaUI features must not hold up the
OS or applications. NexaUI may still be used per application when the necessary
scope is ready; full toolkit completion is not an adoption prerequisite.

Use SDL3 as the preferred platform foundation for early application surfaces,
subject to an exact dependency/build/security/license review before adding it.
This selects a direction, not an installed package/version or a proven speed gain.
The compositor remains our wlroots-based Wayland server. SDL3 handles client
windows, input and graphics integration; it is not a widget toolkit, compositor,
terminal emulator, shell, browser engine or complete accessibility solution.

Keep each app's data, commands, service clients, validation and persistence free
of SDL/NexaUI types. Put toolkit handles and event/render translation in its UI
module. Do not build a speculative universal widget abstraction or duplicate a
large toolkit in anticipation of migration. A later migration replaces views
incrementally while retaining those application contracts and data formats.

Text layout/shaping, IME, editing, accessibility semantics and keyboard focus
remain explicit work. An SDL-only rectangle/text demo does not satisfy them.
Choose the needed text/widget foundations by evidence; preserve reduced motion,
scaling and unsupported-capability behavior. Prefer event-driven idle behavior
and measure CPU/wakeups, frame response and actual memory on the target session.
The user's final visual design remains a separate decision.

### Current source inspection

GitHub main was read with `git ls-remote` and resolved to
`05e28bb16c1e60ac339cd9428ff2d3afbafc21f6`. The local primary checkout is separately
at `5f7680f7d00a950f759d17d98520405901f9419d`; its local main ref is older still.
Inspect committed objects at the confirmed remote revision, not an arbitrary
checkout or an uncommitted worktree candidate. No NexaUI code/build/pin was changed
or tested by this inspection.

| Area | Source/document evidence at 05e28bb | Aetheri consequence |
| --- | --- | --- |
| Widgets and core | README/header expose layout, widgets, text editing and renderer-neutral commands | Already a substantial candidate; do not request reimplementation of these features |
| Native Linux windows | Wayland/X11, window management and text-input protocols exist | Aetheri compositor compatibility still needs its own native test |
| Dependency profile | CMakeLists.txt:29 unconditionally requires X11 for Linux; Wayland/xkbcommon/scanner also required | Request a tested Wayland-only build/package profile before adopting it into a Wayland-only base |
| Linux native evidence | Private Weston report exercises actual Wayland surfaces on a controlled Xvfb/Weston fixture | Useful prior evidence; not Aetheri/wlroots, physical mixed-DPI or hardware acceptance |
| Text and accessibility | README retains IME/clipboard and complete assistive-coverage limits; current AT-SPI multiple selection is published | Select exact workflows and test those; do not call the whole toolkit absent or finished |
| Stability and release | README identifies early 0.1.0 development and changing pre-v1 APIs; other local worktrees contain separate candidate work | Pin an accepted source revision for adoption; do not use an uncommitted global checkout |

The user subsequently requested the full requirement list for NexaUI to support
all agreed Aetheri UI. The authorized `NEXT_AETHERI_UI.md` in the local NexaUI
folder now contains 102 identified requirements and 12 app/shell surface groups,
with priority, source-evidence status and observable acceptance. It covers build/
package, native Wayland windows, rendering/content surfaces, text/editing/IME,
layouts/controls, input, accessibility, themes/localization/host seams, performance/
lifetime and adoption evidence. Existing API families are not labelled missing.

The implementation order still starts with the confirmed Wayland-only consumer
gap and a real Settings view, then app interaction, desktop/content adapters and
full hardware/quality acceptance. Missing native Aetheri evidence is recorded
separately from missing SDK implementation. The catalog does not transfer the
OS compositor, app supervision, engines or security broker into NexaUI. It is a
user-requested project/chat handoff, not an automatic message dispatch. No NexaUI
code, consumer pin, existing local edits or publication was changed.

### Primary references inspected

- [SDL3 overview](https://wiki.libsdl.org/SDL3/FrontPage): platform/input/graphics
  scope, C API and zlib licensing; text/font helpers are separate libraries.
- [SDL3 Wayland notes](https://wiki.libsdl.org/SDL3/README-wayland): compositor
  capability boundaries, configure/initial-buffer handling and scaling limits.
- [NexaUI pinned README](https://github.com/Lulus792/NexaUI/blob/05e28bb16c1e60ac339cd9428ff2d3afbafc21f6/README.md),
  [build profile](https://github.com/Lulus792/NexaUI/blob/05e28bb16c1e60ac339cd9428ff2d3afbafc21f6/CMakeLists.txt),
  [native Wayland report](https://github.com/Lulus792/NexaUI/blob/05e28bb16c1e60ac339cd9428ff2d3afbafc21f6/docs/quality/wayland-private-compositor-report.md)
  and [window contract](https://github.com/Lulus792/NexaUI/blob/05e28bb16c1e60ac339cd9428ff2d3afbafc21f6/docs/concepts/window-management.md).

## Earlier inspection — 25 October 2026

Inspected 25 October 2026. This is a source/document inventory, not a fresh build,
benchmark, hardware test or security acceptance.

## Evidence

NexaUI is an original C17 toolkit with layout, widgets, text/font handling, themes,
input and accessibility semantics. It has software rendering and Linux Vulkan code.
Its Linux dispatcher creates Wayland or X11 client windows. The inspected build
requires X11, Wayland and xkbcommon development files; Vulkan is a separate option.

This makes it a candidate for Settings, Files, a monitor, editor chrome and other
original app UI. It is not a compositor, browser engine, terminal emulator or shell.

The README describes early version 0.1.0 and remaining text, IME, accessibility,
API and hardware acceptance. Some local documents describe older feature states.
The working checkout has local/untracked changes; do not infer complete coverage
or a releasable commit from one document. Inspected revisions are in [sources](../SOURCES.md).

## Relation to the user's answer

The user accepts NexaUI if suitable and would consider another toolkit for a
significant demonstrated benefit. OS-specific code is not automatically faster.
A matching theme and a thin Aetheri integration layer may meet the product need
without replacing layout, text, widgets and accessibility.

Recommendation: first measure an agreed NexaUI commit in the intended userspace.
Exercise a text field, list, dialog, clipboard, focus, scaling and semantics.
Measure idle CPU/wakeups, input-to-frame latency, frame timing, allocation behavior
and memory on the target laptop. Test CPU and GPU paths separately.

Wayland-only Aetheri would need a deliberate review of the current X11 build
requirement. Supporting our own compositor needs server work beyond the client toolkit.
No change has been made to NexaUI or its other consumers.

The user now explicitly intends Physim to migrate to NexaUI when ready. Record
text/editor, rendering, accessibility and app-specific requirements as migration
gates. Until that separate work is implemented and tested, Physim's SDL3/OpenGL
needs still apply. Do not use the future migration as evidence those dependencies
are already gone.
