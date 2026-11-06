# Private compositor bring-up

5 November 2026. `aetheri-compositor` is the first original C17 Wayland server in
Aetheri. Its input module now supplies a native seat and nested output mode.
It uses wlroots for protocol/scene/backend primitives and Pixman for
software rendering. This opt-in executable is not an installed desktop session,
app sandbox or replacement for the lifecycle authorities.

An additional opt-in [DRM development session](session.md) now drives the actual
virtual connector and PS/2 input through libinput/logind on the dedicated carrier.
It has a separate dependency/build profile and does not change the default
headless/nested selection or imply a trusted session, lock or resume policy.

## Implemented scope

The optional [private desktop connection](desktop.md) adds compositor-side dock/
search roles, work-area reservation and control of existing windows bound to
the original connection. A separate [SDL desktop](desktop-ui.md) now provides
dock/search and fixed development Settings launch; the protocol probe remains
independent. Neither global exists without an
explicit inherited shell endpoint. Existing no-shell modes remain available.

The explicit `--headless` mode creates three virtual outputs: 1920×1080,
2560×1440 and 3840×2160, arranged left to right in the scene. These buffers are
not sent to a physical display. New windows receive an initial configure and
are placed on successive outputs. Maximize, fullscreen on a selected output
and restore update configuration and position. Destruction and connection loss
retire scene content independently of other clients.

The server offers xdg-shell v5 and declares only maximize/fullscreen capabilities.
A native consumer checks v5 and a concurrent consumer negotiates v3. There is no
minimized-state claim. Frame callbacks, including commits without surface damage,
are served. An unchanged scene does not keep committing output buffers; an
explicit client frame request can require one new commit. This is an observed
idle-commit result, not a CPU/wakeup or hardware benchmark.

Normal-window v4+ bounds now exclude the dock and initial placement margins.
The reviewed SDL consumer follows them when first opening; arbitrary clients
are not forcibly clipped. Compositor desired maximize/fullscreen state is
separate from client requests and committed state. Provisional Super+Up/Down
maximizes/restores only the actual focused app outside shell focus, pointer grabs
and fullscreen. Both arrow events are consumed. Current-output lookup, including
explicit fullscreen selection, drives later work-area/output reconfiguration;
original placement is not used as a substitute. See [consumer behavior and native
evidence](desktop-ui.md). The [private desktop](desktop.md#surface-profile-and-focus)
now derives compact-dock visibility from committed fullscreen coverage and the
current stack, while retaining explicit search and pointer-sequence ownership.
General off-screen recovery, fullscreen workspace/animation, mixed-DPI and
physical acceptance remain open.

## Native input and nested presentation

The input module owns a wlroots seat/cursor and per-device listeners. Real backend
pointer/keyboard events use scene hit testing, native keymaps and seat delivery.
Click selects/raises a window; provisional Alt+Tab cycles mapped windows and
consumes both the shortcut press and release. Keyboard enter excludes consumed
keys. Unmapping the focused window selects another mapped window. Backend-provided
keymaps are retained; the fallback is US. Native tests also exercise German and
Shift/modifier state. User-facing layout/shortcut settings remain future UI work.

Client move/resize requires the same seat, the actual focused root surface and a
current single-button grab serial. Old/released/foreign-window requests cannot
select a target. Implicit grabs retain surface-relative coordinates outside the
window. Unmapping the origin does not redirect the held release to another app.
Escape now cancels the original focused native move/resize and consumes its
press/repeats/release. Captured geometry is restored only with unchanged ordinary
mode and size constraints; held pointer motion cannot resume the cancelled grab.
The [own Settings native tests](nexaui.md#window-chrome-and-gestures) verify actual
requests, sizes and moved/restored pixels separately from the DRM keyboard check.
Resize uses bounded geometry and client min/max constraints. Device removal
withdraws capabilities; a fresh keyboard does not inherit an old held Shift.
An optional [built-in Compose/dead-key provider](compose.md) now supplies native
v3 preedit/commit with original field/focus admission. It adds no external input
method or virtual-keyboard global. Multiple simultaneous keyboard aggregation,
touch/gestures and external IME routing remain work.
The private native clipboard boundary is described below; clipboard UI/consent and
physical Bluetooth/device recovery still need their own acceptance.

`--nested SOCKET CAPTURE_DIR_OR_- PARENT_SOCKET` connects explicitly to a named
parent socket in the same owned runtime directory, creating three ordinary native
parent windows. There is no display/backend fallback. Native nested pointers are
mapped to their declared output, rather than to the whole multi-output rectangle.
A supplied parent keyboard map is preserved. Parent resize is accepted within
64–3840 by 64–2160; this is a bounded development profile, not final output policy.
Closing a nested output ends this private session; losing the selected parent
exits with failure, removes the child socket and does not choose another display.
The parent connection is borrowed by wlroots and separately closed by the server.

The test-only server additionally consumes a connected inherited AF_UNIX
SOCK_SEQPACKET descriptor. It constructs actual wlroots keyboard/pointer devices
and feeds the same device event listeners. This driver is linked only into
`test_compositor_server`, not `aetheri-compositor`, and is never installed. No
virtual-input global or unauthenticated network listener is added to the desktop.
Native consumers observe actual seat events without linking the compositor.

## Boundaries and ownership

- Run as an ordinary user with equal real/effective UID/GID. Root is refused.
- Supply an owned, nonsymlink, 0700 `XDG_RUNTIME_DIR` and a bounded socket basename.
  Its ancestors and the same-UID development environment remain trusted
  prerequisites; this is not hostile-user or namespace admission.
- Only the explicitly selected backend and Pixman renderer are constructed.
  Headless/nested modes open no DRM session or physical input device. The separate
  DRM build/mode requests one named card and seat input through logind; there is
  no hardware/backend fallback or startup shell command. A parent display is used
  only by the explicit nested mode.
- The server owns display/backend/scene/allocator/renderer and signal sources;
  each window owns its listeners. The xdg scene helper retires its surface tree.
  Shutdown first destroys clients, then outputs and renderer resources.
- A maximum of 64 live toplevels is enforced. This is not a complete protocol
  resource quota or protection against malicious clients. Production access
  policy, sandbox, lock, clipboard mediation, popups, trusted shell roles, physical input,
  scaling/hotplug and GPU recovery remain work.
- No authority RPC, catalogue write, launch capability, lease or app identity is
  connected. A mapped window proves neither admitted readiness nor process drain.

SIGUSR2 prints live window/output-commit counts. SIGUSR1 requests an image of each
output if an explicit owned 0700 capture directory was supplied. Capture files
are exclusive and never overwritten; pending capture or render failure terminates
the private server. At most 16 requests are accepted. Capture has no client
protocol/global. SIGINT/SIGTERM shut down orderly. These diagnostics are not a
stable session-management API.

## Reviewed dependencies

[graphics-inputs.json](graphics-inputs.json) records source/RPM URLs, exact names,
sizes, SHA-256 and license declarations, separately from the base carrier inputs.
From the repository with Python 3.11 or newer:

```sh
python tools/prepare_vm.py fetch --profile graphics
python tools/prepare_vm.py verify --profile graphics
```

These commands download/verify only; they do not install or execute inputs.

| Component | Selected development version | Reason/boundary |
| --- | --- | --- |
| wlroots | 0.20.2, MIT, upstream archive | Accepted compositor family; unstable API reviewed against these headers/build files |
| Wayland/scanner | 1.26.0-1.fc44, MIT | Matching client/server/scanner |
| wayland-protocols | 1.49-1.fc44, MIT | Generated xdg-shell source; no daemon |
| Pixman | 0.46.2-3.fc44, MIT | Software path without GPU drivers |
| libdrm | 2.4.134-1.fc44, MIT | Required wlroots dependency even without its DRM backend |
| xkbcommon | 1.13.1-2.fc44, MIT/X11/MIT-CMU | Required wlroots dependency; not keyboard acceptance |
| Meson | 1.11.2-1.fc44, Apache-2.0 | Upstream build tool |
| Expat | 2.9.0-1.fc44, MIT | Scanner/development dependency needs the versioned ABI absent from the original carrier |

The standard Fedora wlroots-devel transaction would add 68 packages and update
two, including X11/Mesa/Vulkan. The source profile instead adds 16 packages and
updates Expat/libdrm. All 18 transaction RPMs are pinned. Existing carrier tools,
libxkbcommon/libffi/libpciaccess remain prerequisites. Preserve upstream/package
licenses; original Aetheri code remains MIT. No third-party source is vendored.

All RPM signatures were checked against the existing Fedora 44 fingerprint
`36F612DCF27F7D1A48A835E4DBFCF71C6D9F90A6`. The source archive is pinned from
upstream HTTPS; its hash is not an upstream signature. Install verified inputs
only into the dedicated carrier, using the pinned local RPM closure with repo
resolution disabled and local package signature checking enabled.

Cached repository metadata referenced retired Expat 2.8.5 bytes. The official
update directory supplied 2.9.0; its changes/signature were reviewed and exact
bytes pinned before installation. It preserves the selected `libexpat.so.1` ABI
and contains additional fixes. Upstream still tracks unresolved security work;
this review does not assert a vulnerability-free stack. The build uses pinned
trusted protocol XML. This private scope does not authorize exposure to hostile
clients or installation as a secure session.

## Build and test

Use a Linux checkout with the exact dependencies above and xkeyboard-config
2.47-1.fc44 (already present in the carrier). Set `graphics_cache` to the verified
archive cache and `graphics_work` to a new private workspace, then run:

```sh
python tools/build_graphics.py "$graphics_cache" "$graphics_work"
PKG_CONFIG_PATH="$graphics_work/prefix/lib/pkgconfig" \
  cmake -S . -B build/graphics-release -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DAETHERI_COMPOSITOR=ON
cmake --build build/graphics-release --target aetheri-compositor test_compositor_server test_compositor_client -j2
ctest --test-dir build/graphics-release -R '^compositor\.' --output-on-failure
```

The builder verifies the archive and reviewed patch bytes, original pointer source
and patched source digest, refuses an existing workspace, extracts only expected
regular files/directories and downloads nothing. It builds/installs into its own
user prefix and preserves the upstream license there. It enables Pixman, shared
memory, headless and nested Wayland; optional DRM/libinput/X11/session, GLES2/
Vulkan, GBM/udmabuf and color-management are disabled. No global install is made.

For sanitizer acceptance, use a different new workspace with `--sanitizers`;
configure Aetheri in a separate build directory with that prefix and
`-DCMAKE_BUILD_TYPE=Debug -DAETHERI_SANITIZERS=ON`. Verify the loaded libraries;
client flags alone do not instrument wlroots. Earlier unpatched dependency builds
and their evidence remain separate.

The narrow [wlroots pointer-enter patch](../patches/wlroots-0.20.2-pointer-enter.patch)
emits the real parent enter coordinates as an absolute backend motion event.
Unpatched 0.20.2 records the active pointer but does not emit those coordinates,
so an immediate click after switching outputs can use the previous position.
The native nested test deliberately crosses outputs and clicks without an extra
motion. The patch adds no input authority, event listener or dependency. The
archive remains unchanged; preimage/result and patch hashes are separately pinned.
This is an Aetheri development patch, not a claim of an upstream fix/release.

`AETHERI_COMPOSITOR` defaults OFF. Existing builds do not look up graphics
dependencies. There is no install rule, autostart unit, public capability or
permanent process. Tests launch only owned child handles in private directories,
close them, and retain logs/results. Successful captures are losslessly compressed
with original PPM SHA-256; failed original evidence is retained.

## Executed evidence and next work

The current Release suite passes 3/3 in 5.93 s; Debug ASan/UBSan, including the
separately instrumented patched wlroots, passes 3/3 in 9.83 s:

- `compositor.wayland`: 11 rendering/lifetime workflows, including three-output
  pixels, configure/frame replies, v5/v3 consumers and unchanged-window idle.
- `compositor.input`: 8 workflows for native keymaps/modifiers, German/US input,
  scrolling, click/Alt+Tab focus, serial-bound move/resize, foreign/stale rejection,
  implicit-grab routing, origin unmap and keyboard removal/replacement.
- `compositor.nested`: 3 checks for actual parent-composed output pixels, pointer/
  keyboard delivery across outputs and selected-parent loss without fallback.

All 42 captured images per profile agree byte-for-byte after lossless decoding;
loaded dependency paths, source patch/result hashes and copied licenses were
verified. Consumers use libwayland/xkbcommon, without wlroots/private server
linkage. Production rejects the fixture-FD form and has no input-driver symbols.
With the same final executables and the unpatched upstream library, the nested
input control fails at the immediate entry/click path as expected. Original
failure/control artifacts remain, rather than being relabelled as successful.

A collision check exposed backend listener teardown using an already-freed list;
listeners are now detached before backend destruction, while device listeners
survive until their devices finish. A no-motion-after-drag expectation was also
incorrect: unchanged surface-relative coordinates need no duplicate motion event.
Actual moved/resized output pixels remain the acceptance oracle. The replacement
keyboard test now waits for the actual client keymap/enter, not just a capability
announcement that precedes resource binding. Native release/press delivery is
checked after that observation. No timeout or sanitizer suppression was added.
Nested setup uses parent-managed refresh (zero), and named-output pointer mapping
is required; fixed headless refresh and whole-layout absolute mapping were wrong
for the nested backend.

Previous first-render checkpoint (ec32e0f): Release passed `compositor.wayland` in
4.68 s and Debug in 6.69 s, with 24 images per profile. That evidence remains
historical; the current suite above covers the expanded implementation.

Initial failures are retained: v3 server advertisement was incompatible with
setting v5 capability metadata, so the server now offers v5 and accepts v3
clients. A test incorrectly required zero commits for an explicit no-damage
frame request; wlroots schedules one. The corrected check bounds that request
to one commit and retains the separate zero-repeated-commit idle check.

The ordinary Release build and five existing owner/storage/fixture/broker/boot
checks also pass (0.24 s). Retained old Root v6/User v4 catalogue hashes and
inspected static fixture hashes remain unchanged. Their recovery is not settled.
Normal policy, SQL/XML and installed capabilities are unchanged.

Next integrate a real app view and session behavior, mixed scaling, popups and
trusted roles. Review SDL3/text/widget versions separately. Hardware displays, an original boot image and the
complete experiment desktop remain required, beyond these private pixel checks.

Primary references reviewed: [pinned wlroots source](https://gitlab.freedesktop.org/wlroots/wlroots/-/tree/0.20.2),
[headless API](https://wlroots.pages.freedesktop.org/wlroots/wlr/backend/headless.h.html),
[scene API](https://wlroots.pages.freedesktop.org/wlroots/wlr/types/wlr_scene.h.html),
[seat API](https://wlroots.pages.freedesktop.org/wlroots/wlr/types/wlr_seat.h.html),
[nested backend API](https://wlroots.pages.freedesktop.org/wlroots/wlr/backend/wayland.h.html),
[Wayland input protocol](https://wayland.freedesktop.org/docs/html/apa.html),
[Fedora package](https://packages.fedoraproject.org/pkgs/wlroots/wlroots/),
[Expat changes](https://github.com/libexpat/libexpat/blob/R_2_9_0/expat/Changes) and
[DNF cache semantics](https://dnf5.readthedocs.io/en/latest/dnf5.conf.5.html).
Live API pages may describe newer versions; pinned source/headers are authoritative.

## Native clipboard selection

4 November 2026. The private compositor now advertises `wl_data_device_manager`
version 3 and accepts clipboard selections through its seat input module.
This is real client-to-client Wayland/FD transport. It introduces no clipboard
history, persistence, privileged data-control protocol or resident clipboard
manager. The existing trusted same-UID private runtime-directory boundary still
applies; this is not normal admitted-app or secure-session clipboard policy.

The input module accepts publication or explicit clearing only from the actual
current keyboard-focus seat client, with a mapped current root surface and a
serial sent during that focus interval. wlroots first validates that the serial
was actually issued to that client and was not superseded. Aetheri separately
checks the focus interval, including when the current selection is empty. These
are bounded 32-bit Wayland serial observations, not durable user/app identity.
Keyboard removal invalidates the interval. Replacing a keyboard retains the
current logical window, sends fresh native focus and cannot reuse the old serial.

The producer owns content and serves requested bytes through the receiver's FD.
The compositor retains source/offer metadata and never reads, logs or caches the
payload. Replacement cancels the old source; clearing or producer departure
clears the current selection. Losing a window/focus does not erase a still-live
producer's clipboard. An old offer cannot start a new transfer without current
focus, including the interval when no window has focus. A transferred FD or bytes
already copied into another process cannot be revoked by destroying an offer.
App byte quotas, user paste consent, sensitive-content handling, session lock,
cross-account/sandbox admission and parent/nested clipboard bridging remain work.

The pinned wlroots 0.20.2 interface omits the requesting client from its selection
event. Its receive handler also lacks a current-focus check on an offer retained
during empty focus. The small [private integration patch](../patches/wlroots-0.20.2-clipboard-context.patch)
adds original client provenance to the event and binds each selection offer to
its live seat for receive-time focus checks. Existing offer replacement and
resource destruction still make the old resource inert.

The required private symbols `wlr_aetheri_selection_client_v1` and
`wlr_aetheri_reject_unstarted_drag_v1` deliberately prevent silently loading an
old incompatible library. The first is called at startup before opening the
socket. These symbols detect an old ABI, not trusted source/image provenance.
The second frees only an unstarted drag; the compositor separately
cancels a fresh rejected source while preserving its current clipboard source.
Drag-and-drop/grab/icon admission is not implemented by enabling clipboard.
Source-less and source-backed rejected drags must leave no leaked drag/grab.
This is an Aetheri-private library integration, not an upstream API claim.

The fresh [source builder](../tools/build_graphics.py) verifies the archive,
pointer-enter patch and clipboard patch, plus each affected file's exact original
and result hashes. Unified hunks require exact positions/context/counts, with no
fuzz, shell patch execution or fallback. New Release/Debug prefixes preserve
the prior pointer-only library and all original evidence. No new package family,
SQL/XML, authority capability, policy permission or installed app is introduced.

`compositor.clipboard` uses two independently connected libwayland clients and
actual seat focus. It verifies UTF-8 pipe bytes, replacement/cancellation, clear,
producer exit, unfocused and empty-focus reads, old/unknown serials, keyboard
replacement, selection retention through window unmap/remap and rejected drags.
It exercises native protocol objects/FDs, without linking the client to wlroots
or exposing an injection global in the ordinary compositor. Test payloads are
fixed non-private text. The exact executed scope/results are in STATE. MIME
families beyond that text corpus, clipboard app UI and physical devices still
need their own acceptance.

Primary references: [Wayland data-device protocol](https://wayland.freedesktop.org/docs/html/apa.html#protocol-spec-wl_data_device)
and [wlroots data-device API](https://wlroots.pages.freedesktop.org/wlroots/wlr/types/wlr_data_device.h.html).
The pinned 0.20.2 source, not a newer API page, defines the reviewed boundary.
