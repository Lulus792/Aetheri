# Current wlroots development SDK

The private SDK joins original package development files to the tested libinput
1.32.0/Lua 5.5 runtime. It lets wlroots 0.20.2 and future current app builds use
matching headers and libraries without installing packages on the carrier.
Normal builds and installed image pins remain separate.

## Contents

The [input profile](wlroots-sdk-inputs.json) describes two checked subdirectories:

- `rpm/`: 12 signed packages supply 94 original regular files, 4,291,579 bytes
  and 19 original aliases. They provide DRM 2.4.134, Pixman 0.46.2, libseat 0.9.3,
  libdisplay-info 0.3.0, hwdata 0.412 and systemd 259.10 headers/libraries.
  The seven selected pkg-config modules change only path assignments and keep their real versions;
  libudev/libsystemd advertise 259, not an invented 259.10 API version.
- `input/`: the original Release-v5 installed libinput header, generated PC file,
  original build receipt, selected libinput/Lua libraries and both notices.
  These seven files have independent original hashes. The PC prefix is relocated
  while version, requirements and compiler/linker metadata remain original.

The input runtime's exact SONAME link is retained. A fixed additional development
link lets the original PC's `-linput` resolve the selected library. It does not
rename or relabel another version. `notices/` holds the already reviewed libdrm
source archive because its binary package lacks a consolidated notice. Other
packages retain their original license files. No manuals, NSS modules, hardware
database beyond the required PNP IDs, package scripts or daemons are installed.
This is not exhaustive release-license or security acceptance.

The old full Release export is independently pinned by the input runtime profile.
Its original working namespace is gone; selected header/metadata/source/receipt
bytes are recovered from the fully verified 2,465-file export. Its legacy receipt
predates `build_type`/`sanitizers` fields: verify its original primary Meson Release
command and exact artifact instead of rewriting the record. The historical build
profile hash is distinct from later added instrumentation-test metadata; source
archive/commit and runtime artifact pins remain independently required.

## Preparing and building

Run as an ordinary Linux build account with fresh destinations:

```sh
python3 tools/test_wlroots_sdk.py
python3 tools/prepare_wlroots_sdk.py prepare FRESH_SDK \
  --cache CACHE --input-build ORIGINAL_RELEASE_INPUTS \
  --input-runtime CHECKED_INPUT_RUNTIME
python3 tools/prepare_wlroots_sdk.py check FRESH_SDK
python3 tools/build_graphics.py CACHE FRESH_BUILD --session \
  --common-sdk COMMON --session-sdk FRESH_SDK --sdl-sdk SDL \
  --toolchain TOOLS --original-runtime ORIGINAL_GUI_RUNTIME
```

The SDK preparer validates independent source hashes and the selected Release
artifact before effects. Its package half reuses the sealed-FD signature, metadata
and bounded archive checks. Outputs stay under a fresh developer directory;
partial failures are preserved without adoption, repair or cleanup. The separate
package-only profile can be checked with
`python3 tools/prepare_desktop_sdk.py check SDK/rpm --profile wlroots`;
the joined checker also requires the input half, source notice and receipts.

All five optional build inputs must be supplied together with `--session`.
Without them the existing carrier build path, including its 1.31.3 input check,
stays unchanged. The private path checks exact dependency versions, uses only
the selected PC directories, scanner and Meson, disables downloads and records
the actual prefixes/RPATH. Shared original runtime dependencies are checked
against their separate inventory. Neither old libinput nor Lua 5.4 is admitted
as a fallback for the new input provider.

The existing Pixman/dumb-buffer development profile is retained: DRM/libinput/
session enabled, no Xwayland, wlroots GL/Vulkan renderer or GBM allocator. SDL
clients have their separate real Mesa/GLES runtime. This SDK rebuild does not
establish accelerated compositing or physical GPU acceptance.

## Native proof

`test_wlroots_sdk_native.py` links the installed wlroots and original dependency
PC modules, checks runtime version/features, creates an unassigned libinput/udev
context and a real 64×64 headless output with Pixman/allocator, then tears them
down. The input callback must receive no device-open request. Direct Wayland
calls link Wayland explicitly instead of relying on a private transitive DSO.
Only known non-executable `rw-s` wlroots shared pixel buffers are separated from
filesystem library mappings; arbitrary deleted/executable mappings are rejected.

The separate ASan/UBSan build instruments wlroots and the consumer, checks actual
symbols/ELF dependencies and runs with leak detection and no suppression.
External prebuilt libraries are not thereby instrumented. The
[execution record](../brains/aetheri/journal/2026-11-10-wlroots-sdk.md) contains
actual successful prefixes and independent source/artifact/map checks. Headless
proofs do not establish current compositor/app integration, device/session
admission, DRM presentation or laptop acceptance.
