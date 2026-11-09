# Private Mesa runtime

The [runtime profile](graphics-runtime-inputs.json) supplies the actual renderer
behind the separate [SDL EGL/GLES dispatch SDK](sdl-sdk.md). It selects the
existing Mesa 26.2.5, LLVM 22.1.8 and associated binary inputs without installing
packages or changing the carrier. This is a private development prefix, not a
normal desktop promotion or a USB image.

## Inputs and ownership

Sixteen signed binary RPMs supply 50 original regular files and 86 original
aliases. These include Mesa EGL/GBM/Gallium, its DRI/GBM entry points, driver
defaults, LLVM, SPIR-V Tools, DRM, Expat and the required X11/XCB libraries.
The Mesa binaries have X11 dependencies even though Aetheri's SDL profile is
Wayland-only; this does not install an X server or enable SDL's X11 backend.
Keep the driver aliases, including Iris and Nouveau. Software rendering in the
lab is not a reason to remove the intended hardware paths.

Mesa's binary packages omit a consolidated license text. A separately verified
signed Mesa source RPM is retained as opaque source/notice data, including its
original source, spec and vendored source archives. `SOURCEPACKAGE=1` is required;
its i686 header architecture is not the target library ABI. Nothing in that
source RPM executes or is installed. The already reviewed original libdrm source
archive preserves its distributed per-file notices. Other selected packages
retain their original COPYING/LICENSE files. This is not an exhaustive release
license or security audit.

The prefix holds 52 regular inputs, 277,258,995 original bytes, its profile
receipt and 86 aliases. That development size includes the 69,191,486-byte Mesa
source RPM; it is not an estimate or decision about final installed image size.
GL dispatch, Wayland/FFI and the C++ runtime remain in their independently checked
SDK prefixes. Twelve explicit native carrier files are separately hash-bound.
The carrier's older DRM, Expat and C++ libraries are not silently selected.

## Preparation

Use an ordinary Linux build account and a fresh destination:

```sh
python3 tools/test_graphics_runtime.py
python3 tools/prepare_graphics_runtime.py prepare FRESH_RUNTIME --cache CACHE
python3 tools/prepare_graphics_runtime.py check FRESH_RUNTIME
python3 tools/test_graphics_runtime_native.py \
  FRESH_RUNTIME COMMON_SDK SDL_SDK TOOLCHAIN FRESH_PROOF
```

The preparer verifies exact compressed bytes, signatures, metadata and absence
of package scripts against the same sealed RPM FD. A first read checks original
archive classes, names, contents and aliases before output effects. A second
read copies only the selected original bodies into new regular files. Original
partial hardlink sets can supply a body but never create output hardlinks.
Unknown failures retain their partial output; no adoption, repair or cleanup is
performed. These are developer files, not trusted application or volume identity.

LLVM's 147,082,848-byte library exceeds the smaller SDK parser's member bound.
The separate streaming reader allows 160 MiB per member and 256 MiB per archive,
with 1 MiB read chunks and a bounded entry count. It never holds the decompressed
archive in memory. The old parsers' limits remain unchanged. The observed
rpm2cpio output uses 10-KiB blocks: accept at most 10,239 final zero bytes and
reject nonzero/truncated/excess data, including across short reads.

## Actual rendering proof

The native C17 consumer uses the real private GL dispatch and Mesa provider,
creates an EGL surfaceless pbuffer and GLES context, compiles/links shaders,
uploads triangle vertices, draws and reads every pixel. The run explicitly
selects llvmpipe and disables the shader disk cache. It does not override reported
GL versions. Vendor/renderer strings, pixel checks and actual process mappings
bind Mesa, Gallium, LLVM and the private C++ runtime. Every filesystem mapping
must have an explicit source in the selected runtime/SDK/base inventory.

A separate test-only executable retains the real context/shaders/background
clear but omits the draw call. It must fail the red/blue pixel gate specifically.
It is never an application or runtime dependency.

The [execution record](../brains/aetheri/journal/2026-11-10-graphics-runtime.md)
contains the actual successful prefixes, pixel results, negative control and
verified export. This establishes real software rendering only. Wayland windows,
the current wlroots/app build, DRM hardware rendering, normal admission,
physical GPUs/monitors and release acceptance remain separate work.
