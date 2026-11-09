# Private common SDK and current NexaUI rebuild

The common SDK supplies the original Wayland, XKB and FFI development files
needed to rebuild NexaUI and the later desktop stack in the separate Linux lab.
It is not a complete graphics SDK or a standalone product runtime. Nothing is
installed into the carrier's system directories.

The [input profile](desktop-sdk-inputs.json) selects ten signed Fedora packages:
Wayland client/server/cursor/EGL and development files 1.26.0, Wayland protocols
1.49, libxkbcommon and its development files 1.13.1, and libffi plus development
files 3.5.2. These retain the already reviewed desktop component families. The
newly fetched runtime RPMs match the existing GUI's XKB/FFI versions; exact
metadata, requirements and signature results are retained in the execution record.

There are 169 original regular inputs, 2,808,395 bytes and 12 original relative
library aliases. Nine pkg-config files receive a path-only derivation. Only
literal `/usr` assignments for prefix, exec_prefix, includedir, libdir, bindir
and datarootdir change. Version numbers, requirements, descriptions, XKB data
paths and other bytes remain original. The profile carries each original
pkg-config body and hash so the checker can reproduce the exact derivation.

The scope includes headers, protocol XML, scanner, needed libraries and notices.
It excludes static libraries, manuals and the unused xkbregistry interface.
This does not remove anything from a normal Aetheri runtime or claim a complete
keyboard-layout management backend. Native libc/expat and the reviewed compiler
environment remain prerequisites; this is not a hermetic SDK.

## Build commands

Use an ordinary Linux build account and the
[private build tools](desktop-toolchain.md). Output and proof directories must
be fresh; existing partial output is preserved rather than adopted or repaired.

```sh
python3 tools/test_desktop_sdk.py
python3 tools/test_nexaui_build_inputs.py
python3 tools/prepare_desktop_sdk.py prepare FRESH_COMMON_SDK --cache RPM_CACHE
python3 tools/prepare_desktop_sdk.py check COMMON_SDK
python3 tools/build_nexaui.py PINNED_NEXAUI_TAR FRESH_NEXAUI_WORK \
  --dbus-rpm PINNED_DBUS_DEVEL_RPM --dependency-sdk COMMON_SDK
```

The preparation tool shares the sealed-RPM and bounded newc reader with the
private toolchain. Signature, metadata, scriptlet inspection and payload parsing
use the same immutable descriptor. All selected bodies validate before output
creation; archive symlinks are checked as data and only explicit SDK aliases are
created. No package script is executed. The resulting ordinary-user prefix is a
developer input, not privileged code admission or a secure package installer.

The optional `--dependency-sdk` fences pkg-config discovery to this SDK plus the
builder's separate D-Bus headers. It selects the actual pinned scanner and gives
the new NexaUI library an explicit SDK RPATH. The existing carrier dependency
selection remains the default when this option is absent. The source version
stays NexaUI 0.1.23 at commit `97a42647480bb05167cee5e4ae08cedd9147d80d`.
No upstream source or normal development-build path is changed.

## Current Settings model closure

The current pure Settings model also includes copied audio observation types
and calls our UTF-8 reader. The old two-file snapshot no longer builds the
retained upstream example. `model_support` now records the audio type header,
grapheme implementation/headers/generated data and original MIT/Unicode notices.
The adapter adds the source include root and the existing grapheme source to
the model target. It does not add an audio backend or toolkit types to the model.

The original two `model` keys stay separate for the frozen development-profile
shape. New support hashes are recorded independently, and the text-wrap test
binary joins the build artifact inventory. Both own-code and Unicode notices
are retained beside the consumer. Missing support fails before copying a partial
model snapshot. Normal app/session profile admission is not extended by this
build fix; these fresh private candidates are not automatically selected there.

## Executed boundary

Eight SDK controls and three model-snapshot controls pass on Windows and Linux.
The native common-SDK probe verifies pkg-config versions and paths, generates
and compiles six required Wayland protocols, and actually joins Wayland server,
XKB context and FFI calls. Its 43 command rows pass.

The current NexaUI SDK and complete example build in Release and Debug with
ASan/UBSan. Both configurations actually run the model/semantic-layout and text
wrap tests with the original pinned font. Debug keeps leak detection enabled;
no suppression or policy change is used. Compile commands, ELF instrumentation,
loader selection and all 1,106 original source files are independently checked.
This is not a graphical compositor/session or complete accessibility acceptance.

The [execution record](../brains/aetheri/journal/2026-11-09-desktop-sdk.md)
preserves the early path-check failure, missing-model-dependency build and later
post-test loader-verifier failure separately. The latter is closed by a read-only
audit; no successful native tests are replayed to rewrite its original exitcode.
All 3,091 exported files, source/support copies, artifacts, aliases and input
inventories are verified. Carrier packages, udev and enforcing policy stay fixed.

Next rebuild SDL/text and wlroots against explicit reviewed SDK inputs, then the
current Aetheri apps and their GUI/native regressions. New RPATHs change artifact
hashes; old runtime/image pins must not be silently reused or promoted. The full
desktop, device/service integration and image closure remain open. The user's
first-version USB gate continues to forbid the next stick image build or flash.
