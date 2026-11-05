# Native development desktop

5 November 2026. Aetheri now has an original SDL3 desktop client with a dock,
keyboard search, a current window list and fixed Settings launch. It runs on the
private headless compositor and the carrier's actual DRM/logind development
session. Its layout is a provisional design for later user review.

## Run and use

Build with `AETHERI_DESKTOP=ON`, `AETHERI_COMPOSITOR=ON` and the existing reviewed
SDL/text profile. `AETHERI_SETTINGS=ON` adds the actual Settings consumer/tests;
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
without a Settings process; launch it from the dock or search.

| Action | Behavior |
| --- | --- |
| Super+Space | Open/close search on the output under the pointer |
| Search button | Open search there, including moving it from another output |
| Type / Backspace | Append valid UTF-8 or remove the last scalar |
| Tab / Shift+Tab / Up / Down | Select a matching app/window result |
| Enter / result click | Request fixed Settings launch or activate that current window |
| Escape / click outside the search card | Close search and restore app focus |
| F6 / F7 while search has focus | Session-local DE/EN or high-contrast view choice |
| Ctrl+Q while search has focus | Ask the owner to close this development desktop in order |

Settings Ctrl+Q closes Settings and leaves the desktop running. Reopening it
creates another original child after the previous process and output pipe have
fully ended. A launch request while Settings still runs retains that child and
raises its observed window where available. The app ID used for visual grouping
is descriptive; the launch owner independently selects its fixed executable.

## Module and resource ownership

| Module | Responsibility |
| --- | --- |
| `src/desktop/model.h` and `model.c` | Bounded query, stable window/result selection and pending process observations; no SDL/NexaUI types or toolkit persistence |
| `src/desktop/main.c` | SDL windows/renderers, font/text presentation, native role adapter, controls, input and channel event translation |
| `src/compositor/desktop.c` | Original-connection admission to private roles/control, placement, focus, work area and current window objects |
| `tools/development_session.py` | Explicit carrier controller and ordinary-user owner of original compositor/desktop/Settings Popen handles |

The model holds at most 64 observed windows, 256-byte title/app-ID values and a
255-byte UTF-8 query. Results borrow model/literal labels until the next model
mutation. Windows are updated by current compositor IDs; disappearing results
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
the desktop and creates a separate AF_UNIX SOCK_SEQPACKET command channel. Settings
inherits neither endpoint. All extra parent/child endpoint copies are closed.
The two-byte version1 packets carry no paths, argv, account IDs or incoming FDs.

| Direction | Packet | Meaning |
| --- | --- | --- |
| Desktop → owner | `L1` | Request the fixed reviewed Settings program |
| Owner → desktop | `A1` | Original Settings child exists; not app/window READY |
| Owner → desktop | `E1` | No new launch effect: definite exec failure, busy old slot or spawn-attempt limit |
| Owner → desktop | `X1` | Previous tracked Settings process is reaped and its output pipe drained |
| Desktop → owner | `Q1` | Fence new launches and begin ordered development shutdown |

Only one launch response may be pending. Repeated activation while pending does
not submit again. Reply loss, malformed/truncated packets, ancillary FDs or a lost
channel fail the development session without reconnect/replay. The owner keeps one
Settings slot and at most 128 fresh spawn attempts per session. A live child's
Popen handle is reused; a terminal slot is not replaced before its pipe is closed.
Neither a process acknowledgement nor metadata proves code admission or readiness.

On Ctrl+Q the desktop waits for its owner rather than retiring the display beneath
a running app. The owner stops/reaps Settings, signals the desktop, then waits for
its original retirement/departure and ends the compositor. The signal self-pipe
wakes the UI even while it sleeps. Cleanup/sanitizer failures remain failures.
The root controller retains PID1's original result before explicit unit retirement.
The optional `desktop` field is additive to the development version1 record;
historical records without it retain their Settings-only meaning.

This is trusted same-account development infrastructure, not normal session/app
authorization, hostile-client isolation, app lifetime recovery or lease release.
The production authorities/capabilities, protected old execution intent and frozen
SQL/D-Bus contracts remain unchanged. The Python supervisor is not selected as
the production session manager.

## Acceptance and remaining work

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

The complete Settings/compositor/desktop suite runs in Release and ASan/UBSan.
The final runs pass 10/10 in 22.34 seconds and 10/10 in 39.59 seconds respectively.
All 99 native image hashes per profile agree; all 198 compressed originals were
decompressed and checked against their recorded raw-byte hashes.
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
The existing PID1 sd-pam helper is separately observed bookkeeping, not an app
with an asserted identical execution context. No new PAM/policy configuration is
installed. The carrier's previously observed optional pam_lastlog2 loader warning
remains; this does not establish a finished login/PAM profile.

The design is provisional. Screenreader/AT-SPI semantics, complete text/IME,
200% text and mixed physical DPI, final design/materials/assets, arbitrary app
catalogue/launch admission, free-window initial work-area fitting, fullscreen
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
