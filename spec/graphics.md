# Private compositor bring-up

4 November 2026. `aetheri-compositor` is the first original C17 Wayland server in
Aetheri. It uses wlroots for protocol/scene/backend primitives and Pixman for
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

## Boundaries and ownership

- Run as an ordinary user with equal real/effective UID/GID. Root is refused.
- Supply an owned, nonsymlink, 0700 `XDG_RUNTIME_DIR` and a bounded socket basename.
  Its ancestors and the same-UID development environment remain trusted
  prerequisites; this is not hostile-user or namespace admission.
- Only the explicit headless backend and Pixman renderer are constructed. There
  is no hardware autodetection, fallback or startup shell command. No host
  display, DRM session or physical input device is selected.
- The server owns display/backend/scene/allocator/renderer and signal sources;
  each window owns its listeners. The xdg scene helper retires its surface tree.
  Shutdown first destroys clients, then outputs and renderer resources.
- A maximum of 64 live toplevels is enforced. This is not a complete protocol
  resource quota or protection against malicious clients. Production access
  policy, sandbox, lock, clipboard, popups, seat/input, trusted shell roles,
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

Use a Linux checkout with the exact dependencies above. Verify the source archive
SHA-256 `972c7ac44b17828f4702bfae7cd8347346a3fb5b2c1076cfa2c3fcedac5ec343`,
then extract into a fresh private dependency workspace. Set `graphics_work` to
that workspace and run:

```sh
meson setup "$graphics_work/wlroots-build" "$graphics_work/wlroots-0.20.2" \
  --prefix="$graphics_work/wlroots-prefix" --libdir=lib --buildtype=release \
  --wrap-mode=nodownload -Dauto_features=disabled -Dexamples=false \
  -Dbackends=[] -Drenderers=[] -Dallocators=[] -Dsession=disabled \
  -Dxwayland=disabled -Dcolor-management=disabled
meson compile -C "$graphics_work/wlroots-build" -j2
meson install -C "$graphics_work/wlroots-build"
```

Installation stays in that user prefix without root. Empty optional arrays still
include wlroots' always-built Pixman, shared-memory allocation, headless and
nested Wayland. DRM/libinput/X11/session, GLES2/Vulkan, GBM/udmabuf and color
management are disabled. Meson downloads no fallback dependencies. Physical
backends require a separate reviewed profile.

From the Linux Aetheri source directory:

```sh
PKG_CONFIG_PATH="$graphics_work/wlroots-prefix/lib/pkgconfig" \
  cmake -S . -B build/graphics-release -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DAETHERI_COMPOSITOR=ON
cmake --build build/graphics-release --target aetheri-compositor test_compositor_client -j2
ctest --test-dir build/graphics-release -R '^compositor.wayland$' --output-on-failure
```

For sanitizer acceptance, build wlroots in a separate workspace/prefix with
`--buildtype=debugoptimized -Db_sanitize=address,undefined`. Configure Aetheri in
another build directory with `-DCMAKE_BUILD_TYPE=Debug -DAETHERI_SANITIZERS=ON`
and that prefix's pkg-config path. Verify that both compositor and consumer load
their intended libraries; client flags alone do not instrument wlroots.

`AETHERI_COMPOSITOR` defaults OFF. Existing builds do not look up graphics
dependencies. There is no install rule, autostart unit, public capability or
permanent process. Tests launch only owned child handles in private directories,
close them, and retain logs/results. Successful captures are losslessly compressed
with original PPM SHA-256; failed original evidence is retained.

## Executed evidence and next work

Release passes `compositor.wayland` in 4.68 s; Debug ASan/UBSan with separately
instrumented wlroots passes in 6.69 s. Eleven workflow checks include eight
three-output captures, configure/frame replies, v5 capabilities, a concurrent
v3 client's abrupt disconnect, other-client preservation and idle stability.
The consumer links only libwayland, not wlroots or private server interfaces.
Pixel probes sample composed buffers, not just client paint memory. Runtime/
socket argument refusal and socket collision are also tested.

Initial failures are retained: v3 server advertisement was incompatible with
setting v5 capability metadata, so the server now offers v5 and accepts v3
clients. A test incorrectly required zero commits for an explicit no-damage
frame request; wlroots schedules one. The corrected check bounds that request
to one commit and retains the separate zero-repeated-commit idle check.

The ordinary Release build and five existing owner/storage/fixture/broker/boot
checks also pass (0.32 s). Retained old Root v6/User v4 catalogue hashes and
inspected static fixture hashes remain unchanged. Their recovery is not settled.
Normal policy, SQL/XML and installed capabilities are unchanged.

Next add pointer/keyboard/focus and a visible nested development output, then app
UI/session integration, mixed scaling, popups and trusted roles. Review SDL3/text/
widget versions separately. Hardware displays, an original boot image and the
complete experiment desktop remain required, beyond these private pixel checks.

Primary references reviewed: [pinned wlroots source](https://gitlab.freedesktop.org/wlroots/wlroots/-/tree/0.20.2),
[headless API](https://wlroots.pages.freedesktop.org/wlroots/wlr/backend/headless.h.html),
[scene API](https://wlroots.pages.freedesktop.org/wlroots/wlr/types/wlr_scene.h.html),
[Fedora package](https://packages.fedoraproject.org/pkgs/wlroots/wlroots/),
[Expat changes](https://github.com/libexpat/libexpat/blob/R_2_9_0/expat/Changes) and
[DNF cache semantics](https://dnf5.readthedocs.io/en/latest/dnf5.conf.5.html).
Live API pages may describe newer versions; pinned source/headers are authoritative.
