# Native Settings development view

4 November 2026. The first app view uses SDL3 for its Wayland window, native
input and OpenGL ES presentation, SDL_ttf for text, FreeType for rasterization
and HarfBuzz for shaping.
It is an opt-in C17 development executable, not an installed/admitted app or a
complete Settings application. Final visual design remains with the user.

The separate [NexaUI integration probe](nexaui.md) now exercises this original
model on the Aetheri compositor. It is opt-in and does not replace this SDL view
or establish native accessibility/normal session acceptance.

## Behavior and responsibility

### Native Sound page

The optional NexaUI view now has a **Sound** page alongside the original three
pages below. It follows the accepted sound study's output list, thin volume
track and mute switch, using the current opaque material fallback. The normal
development executable has no admitted audio owner yet: its Sound page reports
unavailability and offers no working-looking device controls. No example device
is inserted into the normal application.

The shared display model accepts bounded copied audio observations. Each window
assigns monotonically allocated semantic IDs to complete connection/serial/node
references. Reordering retains an ID; node reuse, disappearance/reappearance or
a different connection cannot inherit a held press. Invalid observations retain
the old data marked stale and disable effects. Owner/backend loss permanently
fences this model. These copies and IDs authenticate no caller, device or service.

Output selection, volume and mute create one explicit intent, without changing
observed device state. Dragging edits a local draft and submits on release;
pointer cancellation/focus loss drops that draft. Keyboard and accessible value
changes submit directly. Pending work disables further effects. A submitted
change is reported as confirmed only after matching original-owner command
readback; an old observed command cannot confirm a new intent. Unknown effects
are never retried. Stale/busy/rejected submissions need a fresh user action.
Observed boosted levels are displayed but cannot trigger an automatic clamp
write. Mute preserves the observed volume and disables its slider.

[`settings/audio.h`](../src/settings/audio.h) connects the model to the existing
[original-session control handle](audio-session.md#shared-session-control-handle).
An admitted owner calls its pump on relevant notifications and after input.
It reads original observations before consuming an intent once, forwards the
exact revision/ref and reads back after submission. It creates no service,
timer, thread, socket selection or reconnect. This is a programmatic binding;
the regular executable does not yet create an owner or dispatch those events.
Normal session/service/device provisioning remains separate work.

The view exposes radio, slider and switch semantics, keyboard focus and
English/German labels. Both raster densities, dark/light/high contrast,
100/150/200% text, narrow layouts, actual native window/navigation and stale
held-row behavior are tested. Model/owner binding uses controlled adapters;
the populated page's rendered captures use explicit test data. No actual joined
Sound-UI-to-HDA/physical playback or screenreader task is accepted here. Input
devices, per-app mixing and the full final materials/layout remain work. See the
[execution record](../brains/aetheri/journal/2026-11-09-settings-sound.md).

### Original SDL pages

`src/settings/model.*` contains transient observations and selection without
SDL, font or renderer types. Display IDs are session-local; they never enter a
catalogue or become device/permission identity. A snapshot is validated before
replacement. Failed or oversized snapshots retain the old observation marked
stale; selection follows the same display through reordering and falls back to
the first remaining display on removal. There is a deliberate 16-display bound.

`src/settings/main.c` owns the window, renderer, font, bounded control
geometry, event loop and presentation. It reads actual SDL display names/current
modes/content scale. An absent refresh rate is shown as unavailable. It does not
invent a physical display/GPU identity from headless output observations.

The three pages are:

- **Displays:** observed displays, selection, current mode/scale/refresh and refresh.
  There is no apply action before a real backend exists.
- **Appearance:** dark, light or high-contrast palette and 100/150/200% text.
- **Language:** English or German labels.

Appearance/language choices affect this window until it closes. There is no
system mutation, app configuration file, resident helper, network request, catalogue
write or authority RPC. The selected Mesa driver does create a shader cache.
Development launches must supply `--cache` with an existing owned, nonsymlink
0700 directory; the app selects that cache path before graphics initialization
and requests a 32 MiB driver cache budget. This is not a hard disk quota or
StorageRef admission; same-UID development code and trusted ancestors remain
prerequisites. The eventual admitted app needs its managed cache allocation.
Tests keep shader data in their own retained artifact root rather than adding
unexplained files to the account-wide cache. App models/commands/persistence
remain outside the toolkit so NexaUI views can replace SDL incrementally.

Tab/Shift-Tab select controls; Enter/Space activate; F5 refreshes displays;
Page Up/Down and the wheel scroll; Ctrl+Q or Close exits. Focus follows controls
through reflow and automatically scrolls into view. The provisional client
header and borders issue native Wayland move/resize requests through SDL hit
testing. Content is clipped to its scrolling viewport. The minimum logical
window is 760 × 480; the initial size is 1100 × 760. These are development
dimensions, not a user-approved final design.

There are no animations or transparent materials yet. Opaque palettes preserve
readability without depending on blur support. The main thread waits for native
events and redraws changed/exposed views; it has no continuous frame/timer loop.
`--diagnostics` prints observation counts, non-private numeric display facts,
frame/state/control geometry and orderly close. It grants no input-injection API.

## Reviewed dependency profile

[ui-inputs.json](ui-inputs.json) pins four upstream archives, three matching
signed Fedora toolchain RPMs and the separately reviewed EGL/Mesa inputs, separately from the base/graphics profiles. This
review selected exact release/build/license inputs; it is not a comprehensive
security audit or approval of arbitrary fonts/content engines.

| Component | Version/license | Purpose |
| --- | --- | --- |
| SDL3 | 3.4.20, Zlib | Wayland client window/input, OpenGL ES presentation |
| SDL_ttf | 3.2.2, Zlib | Font/text wrapper around the selected rasterizer/shaper |
| FreeType | 2.15.0, FTL selected from its dual license | TrueType font rasterization |
| HarfBuzz | 15.0.0, MIT-Modern-Variant | Text shaping; C++ is dependency-internal, Aetheri remains C17 |
| Liberation Sans | 2.1.5-15.fc44, OFL-1.1-RFN | Existing guest font, exact regular-face hash pinned |
| GCC C++/libstdc++ development/runtime | 16.2.1-2.fc44, GCC licenses/runtime exception | Matching HarfBuzz build tools; existing C compiler remains unchanged |

SDL3's release notes include Wayland/window fixes. FreeType/HarfBuzz release
notes include later parser/lifetime fixes than the original guest versions;
the selected copies therefore live in a private prefix instead of linking those
older system text libraries. The complete Fedora SDL development transaction
would have installed 123 packages and upgraded 16, including unrelated desktop
families. The three reviewed GCC-family RPMs were installed, followed by 17 signed EGL/
Mesa runtime packages (about 205 MiB additional installed size). The runtime
selection includes libglvnd 1.7.0-9.fc44, Mesa 26.2.5-1.fc44, LLVM 22.1.8-4.fc44
and their pinned transitive dependencies. Fedora Mesa includes X11 client
libraries even though SDL uses only Wayland; no X server is installed or used.
The separate libglvnd-devel RPM is read as header data, never installed. Exact
SPDX declarations, versions and hashes are in the input manifest; retain the
installed Fedora licenses, including LGPL obligations for lm_sensors-libs.
The app links dynamically to these libraries; original Aetheri code remains MIT.

The source builder enables Wayland and OpenGL ES, disabling SDL X11, libdecor,
desktop OpenGL, Vulkan/GPU, audio, camera, joystick/haptics/HID and sensor
subsystems. The private test explicitly selects Mesa llvmpipe and two worker
threads in its child environment, without exposing a physical GPU. This is a
software implementation of the GL API, not SDL's `software` window renderer or
physical-driver acceptance. Normal app launches do not set those test variables.

An initial pure-software SDL build could not create a Wayland window framebuffer:
SDL's Wayland backend has no direct window-framebuffer callbacks and falls back
to an accelerated renderer for window surfaces. Disabling all such renderers was
therefore unsuitable. That original failure is retained. A temporary build-check
patch was discarded. An earlier license-copy path mistake is retained separately
from the final fresh build.

The real consumer also exposed a separate SDL 3.4.20 close-time null dereference:
`keyboard_handle_leave` unconditionally reads `seat->data_device->selection_offer`
when the compositor supplies no optional clipboard manager. The retained core
dump identifies that instruction and null base. The narrow
[optional-data-device patch](../patches/sdl3-3.4.20-optional-data-device.patch) guards
the absent object while retaining existing cleanup when it exists. The builder
checks archive/patch/preimage/result hashes. No clipboard protocol is advertised
merely to avoid the client bug, and clipboard/offer acceptance remains separate.
The later queue/arm and Linux wake-descriptor work is recorded below; these
are separate corrections, not a reason to advertise missing protocols.

## Native wakeup correction

The private builder also applies an exact reviewed SDL 3.4.20 queue/arm
correction and Linux Wayland wake descriptor patch. A queued worker event is
checked again after native wakeup is armed. The Linux backend uses one
nonblocking, close-on-exec eventfd instead of allocating a Wayland sync callback
for each wakeup. The main thread retains event dispatch; app workers must still
end before SDL device teardown. This adds no timer, background process or public
API/ABI. Other platform backends are unchanged and are not accepted by this test.

The builder verifies archive/patch/source preimages and results. GNU patch
2.8-4.fc44 is a pinned, signature-checked private build tool extracted from an
RPM without installation or scriptlets. Its GPL-3.0-or-later license is retained
in the build workspace; it is not linked into or required by the application.
See [UI inputs](ui-inputs.json) and the
[executed investigation](../brains/aetheri/journal/2026-11-06-sdl-wakeup.md).

On the unprivileged disposable Linux carrier, the controlled regression is:

```sh
python3 tools/test_sdl_wakeup.py CACHE REVIEWED_UI_WORK FRESH_TEST_WORK TEST_COMPOSITOR
```

The tool checks the reviewed complete UI workspace, builds two private SDKs with
the same Linux backend and one-shot gate, and requires the original queue logic
to miss the queued event while the corrected logic delivers it. It separately
checks repeated descriptor retirement and injected eventfd creation failure.
The gated SDKs are test fixtures, never application profiles. Actual app-suite
and physical acceptance are separate; use the journal's latest executed result.

HarfBuzz builds core shaping/FreeType integration, without utilities/tests/docs,
subset/raster/vector/GPU or optional integrations. FreeType disables optional
compression/image helpers and its HarfBuzz integration to avoid a dependency
cycle; SDL_ttf invokes the separately required HarfBuzz shaper. This is a narrow
trusted-TTF development profile, not general untrusted font-format acceptance.
SDL_ttf's optional SVG support is disabled.

`build_ui.py` requires a fresh unprivileged Linux workspace, validates cached
archives, safely extracts them, builds into its own prefix and retains commands,
logs, library hashes, font and licenses. Its build-local SDK reads only EGL/GLES2/
KHR headers from the pinned devel RPM and creates explicit links to the independently
installed libglvnd runtime. It executes no package scriptlets. It never runs a package manager or
downloads a missing source. An interrupted build remains for inspection.
The app and CMake reject mismatched selected dependency
versions rather than silently accepting a different runtime.

This software is based in part on the work of the FreeType Team. Third-party
licenses are copied into the private prefix; original Aetheri code remains MIT.
Preserve those notices when packaging the eventual app/system.

Primary references: [SDL release](https://github.com/libsdl-org/SDL/releases/tag/release-3.4.20),
[SDL_ttf release](https://github.com/libsdl-org/SDL_ttf/releases/tag/release-3.2.2),
[FreeType](https://freetype.org/),
[HarfBuzz release](https://github.com/harfbuzz/harfbuzz/releases/tag/15.0.0),
[SDL event wait](https://wiki.libsdl.org/SDL3/SDL_WaitEvent),
[Mesa 26.2.5](https://docs.mesa3d.org/relnotes/26.2.5.html),
[llvmpipe](https://docs.mesa3d.org/drivers/llvmpipe.html) and
[libglvnd](https://github.com/NVIDIA/libglvnd) and
[Mesa cache controls](https://docs.mesa3d.org/envvars.html#mesa-shader-cache-dir). Exact pinned source
headers, CMake/Meson files and included license/change files are authoritative for
the selected profile.

## Build and native checks

On the host, cache/verify only:

```sh
python tools/prepare_vm.py fetch --profile ui
python tools/prepare_vm.py verify --profile ui
```

Transfer the exact archives and source snapshot to the dedicated development
guest. Review/signature-check the exact GCC-family and 17-package EGL/Mesa runtime
transactions there before installing them. Keep the libglvnd-devel header RPM in
the cache for read-only extraction; do not install it. Do not install this profile
on the Windows host or shared WSL.
In an unprivileged guest account, with the reviewed graphics prefix already built:

```sh
python tools/build_ui.py /path/to/cache /path/to/fresh-ui-work
export UI_PREFIX=/path/to/fresh-ui-work/prefix
export GRAPHICS_PREFIX=/path/to/reviewed-wlroots/prefix
export PKG_CONFIG_PATH="$UI_PREFIX/lib/pkgconfig:$GRAPHICS_PREFIX/lib/pkgconfig"
export LD_LIBRARY_PATH="$UI_PREFIX/lib:$GRAPHICS_PREFIX/lib"
cmake -S . -B /path/to/app-build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DAETHERI_SETTINGS=ON -DAETHERI_COMPOSITOR=ON \
  -DCMAKE_PREFIX_PATH="$UI_PREFIX" \
  -DAETHERI_SETTINGS_TEST_FONT="$UI_PREFIX/share/aetheri-fonts/LiberationSans-Regular.ttf"
cmake --build /path/to/app-build -j2
ctest --test-dir /path/to/app-build -R '^settings\.' --output-on-failure
```

Both Settings/compositor options default OFF. The combined native check requires
the explicitly selected reviewed font. Run the app against an already running
private Wayland session:

```sh
mkdir -m 700 /path/to/private-settings-cache
WAYLAND_DISPLAY=private-socket /path/to/app-build/aetheri-settings \
  --font "$UI_PREFIX/share/aetheri-fonts/LiberationSans-Regular.ttf" \
  --cache /path/to/private-settings-cache
```

The model tests cover snapshot rejection, stale-state preservation, selection
across reordering/removal and safe self-refresh. The native integration uses the
separate inherited-FD compositor test executable, actual Wayland seat events,
real SDL buffers and composed-output captures. Native resize waits for the
actual client resize request in the private server wire trace before moving the
grab; a driver input ACK alone is not acknowledgement of that later request.
The initial incorrectly ordered test and its original failure are retained. It checks three real virtual
modes, selection/refresh, keyboard and pointer control, loaded private libraries,
themes, German/200% text, native minimum-size resize, focus scrolling, idle CPU/
frame counts, clean close, failed-font cleanup, explicit cache input and refusal
of unavailable-display fallback. It does
not call SDL_PushEvent or expose a control socket in the ordinary app/compositor.
All screenshots/logs are test artifacts, not the final product design.

Executed results are recorded in [STATE](../brains/aetheri/STATE.md); intended
checks above are not evidence until their selected binaries actually pass.

## Remaining acceptance

Native semantics/AT-SPI and a screenreader workflow are still required. SDL3 and
SDL_ttf do not supply a complete accessible widget toolkit; this view must not
be represented as a daily-use accessible Settings app. IME/text editing,
clipboard editing UI, full settings backends/async failure handling, session admission,
persistence/removal, mixed-density physical monitors, hardware GPU, suspend and
render/display recovery remain work. Current scale controls change text, not a
physical monitor or system scale. Real native 2x output evidence is separate
from 200% text. Normal authority gates/capabilities and the retained old execution
intent remain unchanged. Graphics/UI evidence cannot settle that old lifecycle.
