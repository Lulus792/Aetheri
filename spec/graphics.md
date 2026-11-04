# Private compositor bring-up

4 November 2026. `aetheri-compositor` is the first original C17 Wayland server in
Aetheri. Its input module now supplies a native seat and nested output mode.
It uses wlroots for protocol/scene/backend primitives and Pixman for
software rendering. This opt-in executable is not an installed desktop session,
app sandbox or replacement for the lifecycle authorities.

## Implemented scope

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
Resize uses bounded geometry and client min/max constraints. Device removal
withdraws capabilities; a fresh keyboard does not inherit an old held Shift.
Multiple simultaneous keyboard aggregation, touch/gestures, IME, clipboard and
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
- Only the explicitly selected headless or nested Wayland backend and Pixman
  renderer are constructed. There is no hardware autodetection, fallback or startup
  shell command. A parent display is used only by the explicit nested mode; no
  DRM session or physical input device is opened directly.
- The server owns display/backend/scene/allocator/renderer and signal sources;
  each window owns its listeners. The xdg scene helper retires its surface tree.
  Shutdown first destroys clients, then outputs and renderer resources.
- A maximum of 64 live toplevels is enforced. This is not a complete protocol
  resource quota or protection against malicious clients. Production access
  policy, sandbox, lock, clipboard, popups, trusted shell roles, physical input,
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
