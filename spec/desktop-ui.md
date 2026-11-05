# Native development desktop

5 November 2026. Aetheri now has an original SDL3 desktop client with a dock,
keyboard search, a current window list and fixed Settings/Terminal launch. It runs on the
private headless compositor and the carrier's actual DRM/logind development
session. Its layout is a provisional design for later user review.

## Run and use

Build with `AETHERI_DESKTOP=ON`, `AETHERI_COMPOSITOR=ON` and the existing reviewed
SDL/text profile. `AETHERI_SETTINGS=ON` adds the actual Settings consumer/tests;
the complete carrier desktop also builds `AETHERI_TERMINAL=ON` and `AETHERI_SHELL=ON`.
the native carrier session also requires `AETHERI_DRM_SESSION=ON`. Keep all normal
graphics options off unless deliberately building this development profile.
See [Settings inputs/build](settings.md) and the [session guide](development-session.md).

With the dedicated carrier already prepared, compiled and booted:

```sh
python tools/development_session.py start --desktop --profile release
python tools/development_session.py status
python tools/development_session.py stop
```

Use `--profile debug` for the reviewed ASan/UBSan build. A start without
`--desktop` retains the earlier Settings-only workflow. A desktop session starts
without app processes; launch Settings and Terminal independently from dock or search.

| Action | Behavior |
| --- | --- |
| Super+Space | Open/close search on the output under the pointer |
| Super+Up / Super+Down in an app | Maximize to its work area / restore normal size |
| Search button | Open search there, including moving it from another output |
| Type / Backspace | Append valid UTF-8 or remove the last scalar |
| Tab / Shift+Tab / Up / Down | Select a matching app/window result |
| Enter / result click | Request the selected fixed app or activate that current window |
| Escape / click outside the search card | Close search and restore app focus |
| F6 / F7 while search has focus | Session-local DE/EN or high-contrast view choice |
| Ctrl+Q while search has focus | Ask the owner to close this development desktop in order |

Settings Ctrl+Q closes Settings and leaves the desktop running. Reopening it
creates another original child after the previous process and output pipe have
fully ended. A launch request while Settings still runs retains that child and
raises its observed window where available. The app ID used for visual grouping
is descriptive; the launch owner independently selects its fixed executable.
The [terminal](terminal.md) behaves the same way, with Ctrl+Shift+Q for its own
close and its own PTY/shell owner. Both apps can run together. Ctrl+Shift+Q does
not alias the desktop's Ctrl+Q action while search has focus. The narrow dock
uses three controls; German Settings is shortened to “System” there.

Normal-window configure bounds now recommend the selected output's usable area,
including the dock and the provisional 40px placement margins on sufficiently
large outputs. The reviewed SDL client uses those initial bounds. On the actual
1280×800 carrier, Settings opens at 1100×636, maximizes to 1280×716 and restores
to 1100×636. Its footer remains above the dock. These are advisory xdg-shell v4+
bounds, not forced clipping or a promise that every client respects them. Legacy
v3 clients remain supported without that event. User resizing can exceed the
recommendation; general off-screen recovery/minimum-size policy remains work.

Super+Up/Down consumes both arrow events and acts only on the actual app keyboard
focus. Search focus, a pointer interaction or fullscreen blocks it. Restore does
not mean minimize or fullscreen exit. Compositor desired modes are separate from
client requests and committed protocol state; no optimistic state is reported as
an acknowledged client resize. Output changes follow the selected output rather
than a window's original placement output.

## Module and resource ownership

| Module | Responsibility |
| --- | --- |
| `src/desktop/model.h` and `model.c` | Bounded query, stable window/result selection and pending process observations; no SDL/NexaUI types or toolkit persistence |
| `src/desktop/main.c` | SDL windows/renderers, font/text presentation, native role adapter, controls, input and channel event translation |
| `src/compositor/desktop.c` | Original-connection admission to private roles/control, placement, focus, work area and current window objects |
| `tools/development_session.py` | Explicit carrier controller and ordinary-user owner of original compositor/desktop/Settings/Terminal Popen handles |

The model holds at most 64 observed windows, 256-byte title/app-ID values and a
255-byte UTF-8 query. Results borrow model/literal labels until the next model
mutation. Its 66-result capacity includes two fixed app choices. Selection stores
separate app and window tags; app enum values never overload compositor window IDs.
Each app has separate pending/process/error observations. Windows are updated by
current compositor IDs; disappearing results
cannot redirect an old activation to a replacement window.

Search matches ASCII case-insensitively and otherwise compares UTF-8 bytes.
Backspace removes one scalar, not a complete Unicode grapheme. Long queries show
their end with a leading ellipsis and a static caret. Full cursor/selection,
clipboard editing, Unicode folding, grapheme editing and IME composition remain
work. This is a small launch palette, not an editor/text-widget SDK.

One SDL process owns up to three custom-role windows. A closed dock uses only an
84-logical-pixel strip per output; search temporarily expands its selected view.
The adapter matches actual output names from its own registry and the private
control snapshot. SDL owns scale/viewport translation; the client does not attach
a second viewport or set its own buffer scale. Renderers are created after the
initial role configure, and subsequent configures update SDL's logical size.

The compositor now offers the standard wlroots viewporter primitive from its
existing pinned library. Initial EGL allocation before configure was observed
to submit 640×84 against the acknowledged 1920×84. The working adapter delays
renderer creation, uses SDL's viewport handling and schedules one bounded
follow-up repaint after configure: this carrier's EGL path can submit its old
allocation once during resize. This is a measured integration boundary, not a
claim that every SDL/driver version needs the same treatment.

Wayland buffers use premultiplied alpha. Transparent clear pixels have zero RGB;
the dark search backdrop uses premultiplied color. Original captures exposed
the previous doubled background color and transient missing dock; the final
native test checks the dock tile and background independently of Settings text.

An unchanged view waits for events. Query editing redraws only its active view;
window observations and theme/language changes invalidate the affected shared
presentation. There is no animation timer or polling renderer. A single SDL
worker waits on the launch channel and a signal self-pipe, then enqueues numeric
events. Only the UI thread mutates the model or touches views. Closing wakes and
joins that worker before freeing SDL resources. A failed enqueue terminates the
development client with failure rather than leaving an unobservable command.

The explicit shader cache remains inside the owned development token directory,
shared by these development views with the existing 32 MiB soft budget. It is
not an app DataEpoch, StorageRef, hard quota or permanent toolkit-owned service.

## Fixed owner channel

The supervisor independently inherits one private Wayland stream connection to
the desktop and creates a separate AF_UNIX SOCK_SEQPACKET command channel. Apps
inherit neither endpoint. All extra parent/child endpoint copies are closed.
The three-byte version2 packets carry no paths, argv, account IDs or incoming FDs.
`S` and `T` select only the fixed Settings and Terminal programs. Unknown selectors
and old version1 packets are rejected by the current desktop/owner pair.

| Direction | Packet | Meaning |
| --- | --- | --- |
| Desktop → owner | `L2S` / `L2T` | Request the selected fixed reviewed program |
| Owner → desktop | `A2S` / `A2T` | Original selected child exists; not app/window READY |
| Owner → desktop | `E2S` / `E2T` | No new launch effect: definite exec failure, busy old slot or per-app attempt limit |
| Owner → desktop | `X2S` / `X2T` | Previous selected process is reaped and its output pipe drained |
| Desktop → owner | `Q20` | Fence all new launches and begin ordered development shutdown |

Only one launch response per app may be pending. Repeated activation while pending does
not submit again. Reply loss, malformed/truncated packets, ancillary FDs or a lost
channel fail the development session without reconnect/replay. The owner keeps one
slot and at most 128 fresh attempts per app/session. A live child's original
Popen handle is reused; an exited slot is not replaced before its pipe is closed.
An error after a real spawn, such as failed pipe registration, retains that child
and fails the session rather than returning a definite no-effect reply.
Neither a process acknowledgement nor metadata proves code admission or readiness.

On Ctrl+Q the desktop waits for its owner rather than retiring the display beneath
a running app. The owner stops/reaps Terminal, then Settings, then signals the desktop and waits for
its original retirement/departure and ends the compositor. The signal self-pipe
wakes the UI even while it sleeps. Every role's output pipe must end before the
next is terminated. The same order applies after an exception. Cleanup/sanitizer
failures remain failures. A hung PTY job or retained stdout writer keeps later
roles alive; there is no automatic force-kill or fabricated descendant/lease drain.
The root controller retains PID1's original result before explicit unit retirement.
The root development record remains format1. New desktop records add
`launch_protocol: 2`, five executable digests and two Mono font/license digests.
Historical desktop records without that field retain protocol1 meaning; records
without `desktop` retain Settings-only meaning. Closed records never control a
newer session. No frozen SQL/XML management contract changes.

This is trusted same-account development infrastructure, not normal session/app
authorization, hostile-client isolation, app lifetime recovery or lease release.
The production authorities/capabilities, protected old execution intent and frozen
SQL/D-Bus contracts remain unchanged. The Python supervisor is not selected as
the production session manager.

## Acceptance and remaining work

The two-app checkpoint passes `desktop.model`, `desktop.native` and
`desktop.owner` initially in Release (16.94 s) and Debug ASan/UBSan (21.65 s). The first two
retain the existing controlled launch-owner cases, now with tagged choices,
version2, 320px layout and the distinct close chords. The new owner integration
runs the actual supervisor loop and all real programs with a test-only headless
compositor/notify/log adapter; it does not mock app launching or retirement.
It covers both launch routes, reuse, task activation, terminal close/reopen,
an actual hangup-ignoring shell job and a retained real stdout writer after the
terminal process exits. A build-local owner omitting pipe waiting is rejected by
the unchanged behavioral assertion. Test /proc-FD retention and force cleanup
belong only to the independently pinned fixture, never production admission.
It also makes the actual compositor fail before desktop creation. The preserved
old owner hangs on its unused parent endpoints; stopping now closes those copies
and does not start the first UI after the stop fence. The final fixture exercises
the failure without a root controller, PAM session or installed service.

The final two native suites provide 33 hashed images per profile across three
virtual outputs. Actual DRM/logind sessions in Release and Debug also launch both
apps, run an own-shell file operation, reuse the original Terminal and shut down
all four direct children with status0. The bootstrap-corrected owner repeats both
native workflows; sixteen QMP PNGs across four runs are virtual display
evidence, not Windows host interaction or physical GPU acceptance. Sources/logs/
binary hashes, original fixture failures and negative control are retained under
`local/desktop-terminal-2026-11-05`.

The subsequent source review found that the older synthetic supervisor fixture
still sent protocol1. Actual execution rejected it; its client now sends protocol2
and its record guards check strict version types while retaining legacy meaning.
It is registered as `desktop.supervisor` in this explicit carrier profile. Final
four-group runs pass Release (16.55 s) and Debug ASan/UBSan (20.96 s), including
all eleven controller/file/synthetic-process subtests without skips. Native app
binaries remain hash-identical to the reviewed DRM sessions. Final logs and
native-result references are retained in `final-regressions` under the evidence
folder above; this is no wider normal session or lifecycle acceptance.

### Earlier consumer and work-area checkpoints

`desktop.model` exercises bounded UTF-8, stable selection, invalid input, window
capacity and duplicate/uncorrelated launch observations. `desktop.native` uses
actual SDL/Wayland buffers and native keyboard/pointer device events, plus a
controlled test launch owner that starts the real Settings binary. It covers
three outputs, pointer transfer of search, keyboard editing/selection/activation,
normal/error/empty states, transparency, 320px reflow, idle, close fencing and
lost-channel/ancillary-FD denials. There is no SDL event injection test shortcut.

With diagnostics enabled, each view retains at most one Wayland frame callback
and a copied render-state description. `DRAWN` is emitted after the selected
compositor's frame-done event; replacing/closing a view retires that observation.
The native test waits for every latest submitted view, plus the requested locale/
theme/window state, before capture. Earlier comparisons exposed captures of an
old dock count and the frame before a theme key was handled. This observation is
not a presentation-time, physical scanout or app READY contract. The ordinary
development startup notification still means the first submitted layout.

The initial SDL desktop suite ran in Release and ASan/UBSan.
Those checkpoint runs passed 10/10 in 22.34 seconds and 10/10 in 39.59 seconds respectively.
At that checkpoint, all 99 native image hashes per profile agreed; all 198
compressed originals were decompressed and checked against their recorded hashes.
Separate Linux supervisor tests exercise actual generated child programs and
verify one live app and Settings → desktop → compositor stop order. Their
unchanged order test rejects a deliberately reversed shutdown implementation.

Actual native DRM sessions additionally exercise the real controller, private
channel, Settings close/reopen/reuse and ordered/user-requested shutdown. Exact
profiles, original unit records, journals, guest PNGs, loaded-library/credential
observations and test reports are retained under `local/desktop-ui-2026-11-05`.
The final `*-reviewed-*` Release/Debug sessions use the frame-observation build;
their actual DRM PNGs are byte-identical. Release closes through the controller,
Debug through the desktop's owner request while Settings still runs. Both original
supervisor results are status0 and all three native children end successfully.

The subsequent work-area tests reproduce the original overflow using the old
compositor and real Settings. Updated native tests cover initial bounds, keyboard
maximize/restore, suppression during search/grabs/fullscreen and resizing a
fullscreen window on a different selected output. All ten checks pass in Release
across focused runs and in ASan/UBSan's complete 10/10 run (36.88 s), followed by
the extended selected-output test in both profiles. All 108 image hashes per
profile agree and all 216 compressed originals verify. Actual DRM sessions repeat
normal/maximized/restored states and owner-ordered shutdown. All three Release/
Debug captures match, and normal/restored images are also byte-identical. These
later original results are under `local/workarea-2026-11-05`.
The existing PID1 sd-pam helper is separately observed bookkeeping, not an app
with an asserted identical execution context. No new PAM/policy configuration is
installed. The carrier's previously observed optional pam_lastlog2 loader warning
remains; this does not establish a finished login/PAM profile.

The design is provisional. Screenreader/AT-SPI semantics, complete text/IME,
200% text and mixed physical DPI, final design/materials/assets, arbitrary app
catalogue/launch admission, general off-screen/minimum-size handling, fullscreen
dock policy, trusted lock/resume and physical drivers remain work. The full
terminal/editor/shell, browser, Files, device/software/VM tools, original image/
installer and persistent experiment desktop are still required. A working carrier
desktop does not complete those goals or establish a security sandbox.

The model/commands can be retained during a later per-view NexaUI migration.
The user's [NexaUI handoff](../brains/aetheri/knowledge/nexaui.md) remains the
adoption baseline; no SDK or consumer pin is changed by this SDL implementation.

## Sources

- [SDL Wayland/custom-role guide](https://wiki.libsdl.org/SDL3/README-wayland),
  read 5 November 2026: roleless surfaces, logical configure sizes and SDL-owned
  scaling/viewport handling. The pinned SDL 3.4.20 implementation/example were
  inspected separately; current wiki behavior alone is not version acceptance.
- [Private role/control contract](desktop.md), [reviewed UI inputs](ui-inputs.json),
  [native UI test](../tools/test_desktop_ui.py) and
  [supervisor tests](../tools/test_development_session.py).
