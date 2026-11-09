# Private SDL and text SDK rebuild

The current Linux lab builds the existing SDL3 3.4.20, SDL3_ttf 3.2.2,
FreeType 2.15.0 and HarfBuzz 15.0.0 inputs without installing development
packages or replacing system libraries. This continues the
[common Wayland SDK](desktop-sdk.md) and [private toolchain](desktop-toolchain.md).
It is not a new desktop image or a normal runtime promotion.

## Additional inputs

The [SDL build profile](sdl-sdk-inputs.json) selects five signed binary RPMs:
libglvnd/EGL/GLES/development files 1.7.0-9 and the already reviewed Meson 1.11.2.
There are 256 original regular members, six explicit aliases and two path-only
pkg-config derivations. These provide the actual EGL/GLES interfaces and
headers, plus Meson's original Python modules. GLX, desktop OpenGL, GLES1,
bytecode caches and manuals are not selected for this existing Wayland/GLES2
build profile. No renderer implementation is provided by the dispatch library.

Meson's Apache notice is retained from its original dist-info license directory.
The libglvnd binary RPMs omit a consolidated license text. Keep their exact
signed source RPM as opaque source/notice data, including the original archive
and Fedora patch/spec. Header `SOURCEPACKAGE=1` is checked independently; its
`ARCH=i686` field does not select a target ABI. None of this source RPM's content
is installed, extracted or executed. The five target packages are x86-64/native
Python inputs. The resulting SDK has 257 regular files, 6,052,669 original bytes,
plus its profile receipt and six aliases. This is not exhaustive release-license
or security acceptance.

## Building

Run in the ordinary Linux build account, using fresh output directories:

```sh
python3 tools/test_desktop_sdk.py
python3 tools/prepare_desktop_sdk.py prepare FRESH_SDL_SDK \
  --profile sdl --cache REVIEWED_CACHE
python3 tools/prepare_desktop_sdk.py check SDL_SDK --profile sdl
python3 tools/build_ui.py SOURCE_CACHE FRESH_UI_WORK \
  --common-sdk COMMON_SDK --sdl-sdk SDL_SDK --toolchain BUILD_TOOLS \
  --font-runtime ORIGINAL_CHECKED_GUI_SNAPSHOT
python3 tools/test_ui_text_native.py UI_WORK COMMON_SDK SDL_SDK BUILD_TOOLS \
  FRESH_PROOF_DIRECTORY
```

The three dependency options are required together. Without them, the existing
installed-carrier dependency checks and header adapter remain the default.
The optional font snapshot supplies only its exact already reviewed font and
notice; its libraries are not added to the build's library path.

The selected SDKs fence pkg-config, scanner, Meson and C++ discovery. Generated
pkg-config versions and requirements remain original. CMake/Meson install
RPATHs explicitly name the private dependencies; their new ELF hashes must not
be substituted into old runtime/image pins. The existing source archive hashes,
three SDL clipboard/event/wakeup patches, feature options, offline Meson mode
and source notices remain required. No missing dependency is relabelled as an
installed RPM, and no library is copied into the carrier's `/usr`.

## Evidence and scope

The Release SDK builds successfully, retaining Wayland, OpenGL ES and shared
library support and the existing disabled X11/Vulkan/static options. The native
consumer uses the installed CMake/pkg-config interfaces and verifies the four
actual SDL/TTF/FreeType/HarfBuzz versions and mapped library files. It queues and
receives an event, then measures and renders four Latin/combining/ligature text
samples to real CPU surfaces with nonempty, correct-color glyph pixels.

HarfBuzz is compiled with C++, but this Release ELF does not require libstdc++.
The probe follows the actual dependency listing: a required C++ runtime must map
the selected private version; an unrequired extra runtime is rejected. It never
adds a dependency merely to satisfy a compiler-language assumption.

A separate `build_ui.py --sanitizers` output and
`test_ui_text_native.py --sanitizers` proof now pass the same native workflow.
The latter checks real ASan/UBSan references in all four libraries, instruments
the consumer and keeps leak detection enabled without suppressions. This
instrumented dependency graph does need C++, and actual maps select the
private libstdc++16.2.1. Both builds produce identical measured dimensions and
nonzero-ink counts for the four samples; no pixel-golden identity is inferred.

The [execution record](../brains/aetheri/journal/2026-11-09-sdl-sdk.md) retains
original controllers, the false mandatory-C++ verifier and an export-only loader
path correction. The verified 1,113-file export covers all 242 submitted inputs,
both 200-file installed artifact inventories, the complete 7,219-file source
inventory in each variant, four reviewed source changes and 20 dependency ELFs.
The host reconstructs both source inventories from the exact cached archives
and pinned final patch hashes. Native package inventory, original udev and
enforcing policy stay unchanged; no selected SDK/probe code remains mapped.

These are CPU text/event/ABI checks. They do not establish an EGL rendering
context, Mesa/DRI closure, actual Wayland window/input, normal session/device/app
admission, accessibility or physical HP acceptance. Complete those integrations
before selecting new normal runtime/image artifacts. The first-version USB gate
remains closed.
