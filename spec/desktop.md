# Private desktop connection

5 November 2026. The compositor now has a separate development-shell connection
and native role/focus integration. The separate [SDL3 development desktop](desktop-ui.md)
now provides dock/search, observed tasks and fixed Settings launch. The solid-color
protocol consumer remains an independent test. Normal session/app admission and
the finished product design remain work.

## Responsibilities

The compositor owns scene ordering, output placement, work-area reservation,
keyboard focus and activation of existing windows. The desktop client will own
the dock/search views and toolkit-independent UI state. A separate development
supervisor will launch the fixed reviewed app binaries. The compositor does not
execute commands, resolve app names to programs or grant filesystem access.

Ordinary xdg toplevels alone cannot express the reserved dock and exclusive search
role. Exposing a general shell-management global to every connection would grant
ordinary clients those controls. Use a deliberately inherited connection and a
small private control contract alongside the existing wlroots layer-shell instead.

## Connection ownership

The optional final CLI pair is `--shell-fd FD`, after the existing headless,
nested or DRM arguments. It must refer to an already connected AF_UNIX
SOCK_STREAM endpoint, numbered at least 3. The compositor validates its type and
peer family, sets close-on-exec and transfers it to `wl_client_create` only on
success. The future supervisor must retain the other endpoint for the desktop
only and close all extra copies. Normal app processes must not inherit it.

The exact original `wl_client` may discover and bind the layer-shell v5 global
and `aetheri_development_desktop_v1`. A display-wide filter checks advertisement
and binding; guessing an actual global ID does not bypass it. The filter is
installed before the compositor publishes its ordinary socket. Other globals
retain their ordinary behavior. Without the shell FD, neither private global is
created and existing headless/nested/session use remains available.

This endpoint is a development connection capability. It is not independently
authenticated process identity: socketpair peer credentials can describe the
creating supervisor before fork. The same-UID developer environment and supplied
FD are trusted prerequisites. This is not a hostile-account boundary, secure
lock screen, installed Aetheri authority or normal application admission.

The control binds once. Unexpected control destruction or peer loss fails this
development compositor without replacement or reconnection. For orderly close,
the original client requests `retire`, waits for `retired`, then disconnects.
That departure ends the development compositor successfully. These messages
settle no app operation, lease, grant or stored generation. SIGTERM still provides
ordinary administrative compositor shutdown.

## Surface profile and focus

One `aetheri.desktop` layer surface is accepted per selected actual output, up to
three outputs. The current development profile uses overlay layer, zero margins,
an 84-logical-pixel bottom exclusive zone and explicit bottom exclusive edge.
Outputs must remain between 320×320 and 3840×2160. This does not establish a
general mixed-DPI/hotplug policy or a hostile-client buffer-memory limit.

| State | Anchors and requested size | Keyboard | Placement |
| --- | --- | --- | --- |
| Dock | bottom/left/right, width 0, height 84 | none | output bottom |
| Search | all four edges, width 0, height 0 | exclusive | whole selected output |

Only one mapped search is exclusive at a time. Unapproved state combinations,
duplicate output roles, unspecified outputs and popups are rejected. Attached
logical surface dimensions must match the acknowledged configure. Positioning
and configure submission are separate from painting; unchanged buffer commits do
not issue another configure or keep the scene redrawing. Unmap releases the
work-area reservation; remap requires a fresh configure and reserves it again.
Actual output-size requests reconfigure dock/search and maximized/fullscreen
windows together; the native test resizes and restores a real headless output.

Ordinary windows live in a separate scene tree beneath shell surfaces. Maximized
windows exclude a mapped dock; fullscreen geometry remains the entire output.
The compact dock is hidden when the frontmost intersecting ordinary window has
committed fullscreen state, is selected for that output and its effective window
geometry covers the output. The scene stack and current coordinates determine
this, not original placement or the last keyboard owner. Raising/moving a normal
window over that output reveals its dock; moving it away or raising the original
fullscreen window hides it again. Other output docks are unaffected. This is the
current fixed-scale development policy, not a fullscreen workspace/lock contract.

An explicitly expanded search stays visible above fullscreen. Hiding the compact
scene does not unmap its surface, change the role, free its reserved work area or
steal keyboard focus. Commit/map/unmap/stack/output and interactive-move changes
refresh that presentation. No polling loop or new service is introduced. Search
temporarily takes actual seat keyboard focus while the prior logical app choice
is retained. Pointer clicks on the dock do not clear app keyboard focus. Search
collapse restores a still-mapped app or empty focus, with a fresh clipboard focus
interval. App mapping/unmapping and keyboard replacement cannot bypass the
exclusive search. Alt+Tab is consumed while it is exclusive.

Scene changes also re-evaluate the surface and local coordinates at the existing
cursor position, so an invisible old dock cannot receive a new stationary click.
The keyboard owner does not change through that recheck. An original held button
or interactive move/resize keeps its existing sequence until release; a new press
then follows the current scene. Identical target/coordinates and absent pointer
devices cause no new pointer notification. This is not pointer confinement or
production secure-input admission. Executed source/binary/pixel boundaries and
the two retained negatives are in the [fullscreen record](../brains/aetheri/journal/2026-11-05-fullscreen-dock.md).

Super+Space sends a toggle event for the actual output under the cursor. Its
Space press/release pair is consumed when the private control is available;
there is no shortcut claim if it is unavailable. Actual view expansion/collapse
remains the desktop client's responsibility. The product client must implement
query editing, Escape, accessible focus and truthful loading/error states.

## Window observations

The private XML emits output names, mapped window metadata, closure, search
toggle, activation result and the end of the initial snapshot. Every toplevel
gets a monotonically allocated nonzero uint32 ID, never reused in this compositor
lifetime. Exhaustion refuses another window. A stale/unknown ID cannot select a
replacement window. Activation operates on the actual current mapped object and
is denied while search has exclusive focus.

Titles/app IDs are descriptive. Each is bounded to 256 UTF-8 bytes without
splitting a valid scalar; controls become spaces and invalid input becomes `?`.
They are never publisher, installation, process, permission or launch evidence.
IDs are not portable across compositor restart. Event observation and a successful
activation are not proof of application readiness or a settled start operation.

The [two XML files](../protocols/README.md) generate private server/client bindings.
The upstream layer protocol is byte-identical to the already pinned wlroots
0.20.2 source; CMake checks its pinned hash. No library family, ordinary authority
profile, lifecycle SQL or D-Bus contract changes for this integration.

## Initial protocol acceptance checkpoint

Build the existing compositor test targets, then run:

```sh
ctest --test-dir BUILD -R '^(compositor\.|settings\.)' --output-on-failure
```

The added `compositor.desktop` uses separate native C17 Wayland consumers and the
existing inherited input-device test driver. It runs unprivileged on the private
headless compositor, without a host display or input-injection global. Assertions
remain active in Release. The existing controller/session is not provisioned with
a shell and the normal installed capabilities remain empty.

Actual final Release passes 8/8 selected tests in 13.69 seconds. Debug with
ASan/UBSan passes the same 8/8 in 21.81 seconds. The SDK/Mesa Release libraries
retain their earlier instrumentation boundary. The tests cover:

- Three real dock surfaces, search over a maximized app, reserved geometry,
  collapse, unmap/remap, output-size changes and actual composed pixels.
- Ordinary same-UID discovery denial and forged binding to both known private
  global IDs; unexpected departure, repeated control binding, invalid exclusive
  zone and a non-stream inherited FD fail without replacement.
- Native Super+Space, exclusive search typing, Alt+Tab fencing, keyboard
  replacement, activation, stale ID rejection, metadata bounds, never-reused IDs,
  apps closing during search and orderly peer retirement.
- A buffer-only repaint produces exactly one additional probe paint, with no
  configure loop; an unchanged scene produces no further output commits.
- Existing Settings, native input, clipboard, nested and ordinary compositor
  checks remain green. Seventy-eight composed image hashes per profile agree,
  including twenty-four new desktop-probe images; all 156 gzip originals verify.

The unchanged native consumer and test reject a separately compiled mutation
that allows private globals on every non-null connection, at the precise hidden-
global assertion. The original sources/binaries are retained. Two initial
counterexample harness errors (relocated include path and overlong AF_UNIX runtime
path) were corrected in the harness; they are not SDK failure evidence.
The first resize/restoration probe also exposed a stale test-counter wait: it
could capture before the newly restored frame. The original failed pixels/logs
remain; the final test waits for additional app and shell frames from that action.

Private evidence is retained under `local/desktop-connection-2026-11-05`, with
exact native report locations, original image hashes, filter counterexample and
protected-state verification. One original composed probe image was also viewed.
These diagnostic color blocks are not a product design or visual/accessibility
acceptance. No physical monitor, screenreader, suspend/resume or production
shell/process confinement is established by this result.

## Consumer integration

The separate [SDL3 desktop client](desktop-ui.md) now uses the reviewed UI dependency
profile for a compact dock, keyboard search, observed tasks and current-window
activation. Its model/commands have no SDL/NexaUI types. The later view migration
still requires suitable NexaUI gates. Actual custom SDL role/configure tests and
an explicit `--desktop` development-controller mode now exist.

Its separate bounded inherited channel requests only fixed Settings launch and
development shutdown. No arbitrary argv/path or compositor-side spawning is
accepted. Settings close returns to the desktop, and a live original child is
reused. The [consumer guide](desktop-ui.md) records keyboard/pixel/error/idle and
ordered-shutdown evidence separately from this original protocol checkpoint.

The full original desktop/apps, normal admitted session, lock/resume, persistent
experiment desktop VM, original bootable image/installer and physical hardware
acceptance remain required. This protocol checkpoint does not complete them.

## Sources

- [Wayland server API](https://wayland.freedesktop.org/docs/html/apc.html):
  `wl_display_set_global_filter` applies to advertisement and binding, and
  `wl_client_create` takes FD ownership only on success. Read 5 November 2026.
- Pinned wlroots 0.20.2 `wlr_layer_shell_v1.h`, layer implementation and scene
  helper source were inspected. The configure function always submits a new
  configure; the compositor therefore tracks requested dimensions itself.
- [Native test](../tools/test_desktop.py), [consumer](../tests/test_desktop_client.c),
  [compositor integration](../src/compositor/desktop.c) and
  [reviewed inputs](graphics-inputs.json).
