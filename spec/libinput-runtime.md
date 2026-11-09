# Maintained input runtime

The explicit `libinput-1.32.0-lua55-runtime` profile prepares one private input
component for subsequent desktop-image integration. It selects the exact Release
artifact exercised by the [device and DRM tests](libinput-build.md), genuine
Lua 5.5.1 and matching device data. It is not a normal provider update, package
installer, publisher signature, durable publication or physical acceptance.

## Contents and origin

[libinput-runtime-inputs.json](libinput-runtime-inputs.json) independently binds
the original desktop manifest, build-input profile, Lua SDK profile and precise
native artifacts. Its 63 regular files total 1,124,239 bytes, plus one soname alias:

- Release libinput 1.32.0 and the reviewed patched Lua 5.5.1 shared library;
- all 54 source-version-matched device quirks;
- three upstream udev helpers and their two rules;
- original MIT notices for libinput and Lua.

The helper/rule files come from the same verified Release-v5 export as the tested
library, rather than the debugoptimized test runner. Keep the original source
and build records; equal versions do not authorize another build's different bytes.
The profile includes no compiler, headers, test executable, Lua interpreter,
diagnostic CLI or extra daemon. Existing Wacom data remains independently owned
by the unchanged desktop manifest. These contents do not establish complete
redistribution/source obligations or clear other dependency release reviews.

## Prepare and check

Use the ordinary native Linux build account and independently selected inputs:

```sh
python3 tools/prepare_libinput_runtime.py prepare \
  REVIEWED_RELEASE_BUILD REVIEWED_LUA55_SDK LIBINPUT_132_SOURCE_ARCHIVE \
  FRESH_INPUT_RUNTIME
python3 tools/prepare_libinput_runtime.py check FRESH_INPUT_RUNTIME
python3 tools/test_libinput_runtime.py \
  REVIEWED_RELEASE_BUILD REVIEWED_LUA55_SDK LIBINPUT_132_SOURCE_ARCHIVE \
  INDEPENDENT_ORIGINAL_LUA54_LINKED_LIBINPUT
```

The build input supplies `stage/usr/...`; the Lua SDK supplies its pinned library
and COPYRIGHT. The complete reviewed source archive supplies quirks/COPYING.
The preparer checks all selected bytes before output creation, rejects aliased
sources and overlaps, then creates a new tree without overwriting anything.
Failures retain partial output; retry never adopts it. The checker derives every
file, alias, mode and parent from the reviewed profile, including notice coverage.
Unexplained empty directories, altered receipts, old/counter libraries and changed
data cannot pass. `inputs.json` records origin; it cannot authorize different bytes.

This user-owned development output is not protected runtime authority. Do not
execute its helpers as root or use it to admit a privileged service. A later
privileged consumer still requires independently checked protected inputs. Copy
verification is not a power-loss/durable-publication protocol.

## Explicit desktop selection

```sh
python3 tools/prepare_libinput_runtime.py manifest \
  FRESH_INPUT_RUNTIME NEW_DESKTOP_MANIFEST_JSON
# Existing ordinary-user image command, additionally:
python3 tools/build_desktop_probe.py \
  --boot-image REVIEWED_BOOT_IMAGE --packages REVIEWED_PACKAGE_CACHE \
  --output FRESH_DESKTOP_IMAGE --input-runtime FRESH_INPUT_RUNTIME
```

`build_usb_desktop.py` accepts the same optional `--input-runtime` argument for a
new **regular image**, not a physical writer. Without the argument both builders
retain the original manifest bytes and providers. The selection never follows a
directory name, an untrusted receipt or a guessed newer library.

The derived manifest removes the original 55 libinput/Lua package files and adds
the selected 63. All 1,371 unrelated entries remain identical, including ownership
and origin. The result has 1,434 regular payload entries; its package inventory
removes those two RPM providers and records the two source-build origins instead.
The original file is unchanged. The new component supplies an explicit libinput
COPYING notice; the preceding desktop manifest did not contain that notice.

After copying the selected payload into the fresh image tree, the builder uses
`readelf` to inspect every selected ELF without executing it. Any remaining Lua
ABI other than 5.5 rejects the image; the selected libinput must need the genuine
5.5 soname and have no RPATH/RUNPATH. The output retains the dependency audit and
exact derived manifest. This is a static compatibility gate, not complete runtime
dependency resolution, security review or device acceptance.

The component's native static audit and original old-ABI counter pass. The full
1,434-entry image audit and current complete desktop rebuild remain to execute
with the required SDK/tool inputs. Do not describe the selection option, manifest
or component probe as a newly booted desktop image. Test the projected runtime
with the actual compositor/session and connected Sound before normal selection.
Production device ownership, PCRE2 review and physical HP testing remain separate.
