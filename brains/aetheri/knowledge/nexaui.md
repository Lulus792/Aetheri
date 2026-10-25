# NexaUI suitability

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
