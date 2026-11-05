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

### Current source inspection and SDK handoff — 5 November 2026

A fresh read-only `git ls-remote origin refs/heads/main` selected
`88aeef97f0cd9b90f335e8520cae65570994ba09`. Relevant committed headers,
examples, the requirement map and adoption assessment were read with `git show`.
The local primary checkout remains at
`5f7680f7d00a950f759d17d98520405901f9419d` with existing work; it was not
checked out, built, reset or adopted.

This advances the earlier `120384b` inspection. Public installed Vulkan and
Wayland software views, installed Settings, application-descriptor polling,
a correlated short-lived Settings worker, DE/EN example strings and borrowed
Wayland host presentation/polling now exist. The source map includes all 103
IDs, including D11. Do not repeat those implementations or request the earlier
102-to-103 ID repair again. The preliminary statuses still require individual
acceptance review against the exact candidate and each concrete workflow.

The user-requested full handoff is
[D:/NexaUI/NEXT_AETHERI_UI.md](D:/NexaUI/NEXT_AETHERI_UI.md): **103 stable IDs
and 13 app/system surface groups**, with priorities, source-status boundaries,
observable acceptance, concrete integration cases, delivery packets and a
copyable implementation request. It covers every currently agreed Aetheri UI,
including installer/recovery and the persistent full-desktop experiment VM.
New product choices extend that baseline without renumbering existing IDs.

First reuse the public package/renderer/wakeup work and close remaining Settings
and service-integration gates. Then exercise the actual Aetheri compositor and
private shell adapter, including input/semantic/AT-SPI composition. The borrowed
host example submits an ordinary surface on private Weston; it does not prove
private-role admission, dock/search, three outputs, physical presentation or
normal session authorization. Dense terminal/editor, Files, content-engine and
VM interfaces retain their own acceptance cases. Aetheri's separate fixed
Terminal/Settings development-owner contracts must survive a view migration.

This update is a source/document review and planning handoff, not fresh SDK
build/runtime/hardware acceptance. Original reports keep their stated candidate
and environment. No SDK code, consumer pin, existing edits, other-chat message
or publication was changed. Engines, process supervision, app models and
privileged backends retain their separate responsibilities; working SDL3 views
continue until an individually accepted NexaUI scope is available.

### Primary references inspected

- [SDL3 overview](https://wiki.libsdl.org/SDL3/FrontPage): platform/input/graphics
  scope, C API and zlib licensing; text/font helpers are separate libraries.
- [SDL3 Wayland notes](https://wiki.libsdl.org/SDL3/README-wayland): compositor
  capability boundaries, configure/initial-buffer handling and scaling limits.
- [NexaUI earlier pinned README](https://github.com/Lulus792/NexaUI/blob/120384bcb9d80f9b5a958bab337fbfecbb30ca7a/README.md),
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

Latest inspected sources: [public host contract](https://github.com/Lulus792/NexaUI/blob/88aeef97f0cd9b90f335e8520cae65570994ba09/include/nexaui/wayland_host.h),
[adoption assessment](https://github.com/Lulus792/NexaUI/blob/88aeef97f0cd9b90f335e8520cae65570994ba09/docs/quality/aetheri-adoption-assessment.md)
and [requirement map](https://github.com/Lulus792/NexaUI/blob/88aeef97f0cd9b90f335e8520cae65570994ba09/docs/quality/aetheri-requirement-map.md).
