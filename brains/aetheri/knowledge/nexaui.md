# NexaUI suitability

## Current decision — 4 November 2026

The user prefers a practical foundation such as SDL3 for early Aetheri UI, with
later NexaUI migration where useful. Missing NexaUI features must not hold up the
OS or applications. NexaUI may still be used per application when the necessary
scope is ready; full toolkit completion is not an adoption prerequisite.

Use SDL3 as the preferred platform foundation for early application surfaces.
The first exact dependency/build/license profile and native execution evidence
are now recorded in [Settings](../../../spec/settings.md): SDL3/SDL_ttf plus a private
FreeType/HarfBuzz stack, OpenGL ES and explicit Mesa software rendering in the VM.
The attempted renderer-free SDL Wayland path was unsuitable. This is a measured
development integration, not a proven speed gain over NexaUI or physical-driver
acceptance.
The compositor remains our wlroots-based Wayland server. SDL3 handles client
windows, input and graphics integration; it is not a widget toolkit, compositor,
terminal emulator, shell, browser engine or complete accessibility solution.

Keep each app's data, commands, service clients, validation and persistence free
of SDL/NexaUI types. Put toolkit handles and event/render translation in its UI
module. Do not build a speculative universal widget abstraction or duplicate a
large toolkit in anticipation of migration. A later migration replaces views
incrementally while retaining those application contracts and data formats.

The first [SDL3 desktop consumer](../../../spec/desktop-ui.md) now follows this
boundary: its C17 query/window model has no toolkit types, while the adapter owns
custom Wayland roles, painting and input translation. A fixed development owner
handles actual app processes independently. This is working SDL evidence, not a
completed NexaUI adoption gate or permission to modify its SDK/consumer pins.

Text layout/shaping, IME, editing, accessibility semantics and keyboard focus
remain explicit work. An SDL-only rectangle/text demo does not satisfy them.
Choose the needed text/widget foundations by evidence; preserve reduced motion,
scaling and unsupported-capability behavior. Prefer event-driven idle behavior
and measure CPU/wakeups, frame response and actual memory on the target session.
The user's final visual design remains a separate decision.

### Current source inspection

On 5 November, GitHub main was read with `git ls-remote` and resolved to
`120384bcb9d80f9b5a958bab337fbfecbb30ca7a`, superseding the earlier
`05e28bb16c1e60ac339cd9428ff2d3afbafc21f6` source baseline. Relevant files were
read directly from that committed object. The local primary checkout is separately
at `5f7680f7d00a950f759d17d98520405901f9419d`; its local main ref is older still.
Inspect committed objects at the confirmed remote revision, not an arbitrary
checkout or an uncommitted worktree candidate. No NexaUI code/build/pin was changed
or tested by this inspection.

| Area | Source/document evidence at 120384b | Aetheri consequence |
| --- | --- | --- |
| Widgets and core | README/header expose layout, widgets, text editing and renderer-neutral commands | Already a substantial candidate; do not request reimplementation of these features |
| Native Linux windows | Wayland/X11, window management and text-input protocols exist | Aetheri compositor compatibility still needs its own native test |
| Dependency profile | `NEXAUI_ENABLE_X11=OFF` selects a Wayland-only profile; package/subdirectory probes exist | Reuse the implementation; close exact consumer/profile gates instead of requesting B01 again |
| Linux native evidence | Adoption/scaling reports describe private Weston, software Sway and native input/assistive checks for their stated candidates | Useful prior evidence; not a fresh test of 120384b, Aetheri/wlroots, physical mixed-DPI or hardware acceptance |
| Text and accessibility | Settings and native text/IME/AT-SPI paths have expanded, with remaining limits in their reports | Select exact workflows and test those; do not call the whole toolkit absent or finished |
| Settings and async work | Example has app-owned pending/completion/error/retry with a deterministic local timer backend | Preserve it; genuine worker wakeup and Aetheri service mediation remain separate work |
| Consumer packaging | Core installed consumers exist; Settings still uses a private build-tree Vulkan adapter | Close a documented installed renderer/host contract for actual application adoption |
| Stability and release | README identifies early 0.1.1 development; linked-version/build-feature/builder-revision queries exist | Pin and independently verify accepted artifacts; metadata alone is not provenance or live capability |

The user subsequently requested the full requirement list for NexaUI to support
all agreed Aetheri UI. The authorized `NEXT_AETHERI_UI.md` in the local NexaUI
folder now contains 103 identified requirements and 13 app/system surface groups,
with priority, source-evidence status and observable acceptance. It covers build/
package, native Wayland windows, rendering/content surfaces, text/editing/IME,
layouts/controls, input, accessibility, themes/localization/host seams, performance/
lifetime and adoption evidence, including installation, first-run and recovery
presentation. Existing API families are not labelled missing. Source records are
mapped in the handoff; new product decisions extend it without renumbering IDs.

The refreshed handoff preserves all 103 IDs and adds concrete integration cases
and delivery packets. The SDK's preliminary source map still lists 102 IDs and
omits installer/recovery D11; its reconciliation is explicitly requested. New
implementation must not repeat the now-existing Wayland-only profile or Settings
example. Next close installed renderer/host packaging, worker wakeup and actual
Aetheri consumer gates, then app interaction, desktop/content adapters and full
hardware/quality acceptance. Missing native Aetheri evidence is recorded
separately from missing SDK implementation. The catalog does not transfer the
OS compositor, app supervision, engines or security broker into NexaUI. It is a
user-requested project/chat handoff, not an automatic message dispatch. No NexaUI
code, consumer pin, existing local edits or publication was changed.

This refresh changes the local handoff and this planning note only. No SDK build,
runtime acceptance, consumer-pin update, other-chat message or publication was
performed. Existing terminal source work in Aetheri is preserved independently.

### Primary references inspected

- [SDL3 overview](https://wiki.libsdl.org/SDL3/FrontPage): platform/input/graphics
  scope, C API and zlib licensing; text/font helpers are separate libraries.
- [SDL3 Wayland notes](https://wiki.libsdl.org/SDL3/README-wayland): compositor
  capability boundaries, configure/initial-buffer handling and scaling limits.
- [NexaUI pinned README](https://github.com/Lulus792/NexaUI/blob/120384bcb9d80f9b5a958bab337fbfecbb30ca7a/README.md),
  [build profile](https://github.com/Lulus792/NexaUI/blob/120384bcb9d80f9b5a958bab337fbfecbb30ca7a/CMakeLists.txt),
  [adoption assessment](https://github.com/Lulus792/NexaUI/blob/120384bcb9d80f9b5a958bab337fbfecbb30ca7a/docs/quality/aetheri-adoption-assessment.md),
  [requirement map](https://github.com/Lulus792/NexaUI/blob/120384bcb9d80f9b5a958bab337fbfecbb30ca7a/docs/quality/aetheri-requirement-map.md)
  and [Settings example](https://github.com/Lulus792/NexaUI/blob/120384bcb9d80f9b5a958bab337fbfecbb30ca7a/examples/18_settings/README.md).

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
