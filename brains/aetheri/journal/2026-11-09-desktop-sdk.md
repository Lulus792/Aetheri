# Common SDK and current NexaUI rebuild — 9 November 2026

Continue the complete current desktop rebuild after the
[private toolchain](2026-11-09-desktop-toolchain.md). Read the
[SDK contract](../../../spec/desktop-sdk.md). Keep the original P0 runtime stopped
and quarantined, normal runtime defaults unchanged and the first-version USB gate
closed. No NexaUI handoff or catalogue change is required by this Aetheri build fix.

## Inputs and implementation

Host inputs/evidence are under `local/full-desktop-build-2026-11-09`.
`sdk-review-intent-v1.json` selects the ten exact original SDK/runtime RPMs.
The native signature/metadata/inventory query exits zero at
`/root/aetheri-desktop-sdk-review-6f9525ad6cf0457aa06fe268e71494d0`.
libxkbcommon 1.13.1-2 and libffi 3.5.2-2 are fetched from signed Fedora Koji
objects at the same versions as the existing GUI. Their library bytes are
independently selected from signed RPM bodies; no installed package is replaced.
The lab's existing D-Bus runtime matches the pinned 1:1.16.2-1 package.

The common SDK has 169 original regular inputs (2,808,395 bytes), 12 original
aliases and nine exact path-only pkg-config derivations. Protocol XML, header,
library and scanner bytes retain their package hashes. Native code checks each
RPM through one sealed descriptor before output effects. Eight controls pass on
host/native; three additional model-snapshot controls pass on both platforms.

`build_nexaui.py --dependency-sdk` explicitly selects that SDK, protocol generator
and private install RPATH. The source remains the previously reviewed 0.1.23
archive/commit. The current Settings model's two-file copy is incomplete: it now
needs `audio/types.h` and `ae_utf8_next`. An additive six-file `model_support`
snapshot, original notices and a pure-model adapter source/include extension
fix the build. The frozen two-key `model` metadata remains separate. No upstream
NexaUI source, existing normal ownership validator or selected build path changes.

## Original failed attempts and successful Release

Native v1/controller 2984 exits one after preparation: pkg-config returns
`//home/.../usr/share/wayland-protocols` because its original metadata prefixes
`${pc_sysrootdir}`. The lexical-only assertion rejects the valid Linux path.
The probe now compares resolved paths to the independently selected SDK; the
original metadata expression is retained. v1 remains at `/run/a-dsk-14bdf0ca3a6d`.

Fresh v2/controller 29982 exits one at the upstream consumer compilation after
the common probe and NexaUI SDK itself build successfully. The current model's
missing `audio/types.h` is the actual compiler error. Do not adopt or replay
that build; retain `/run/a-dsk-ca591a15a434` and its work directories.

Fresh v3/controller 58498 exits zero with all seven rows: SDK controls, model
snapshot controls, SDK prepare, native probe, complete NexaUI/example build,
model test and text-wrap test. Source: `/run/a-dsk-c2d8d3d0c244`.
Common SDK: `/home/aetheri/deps/common-wayland-sdk-c2d8d3d0c244`.
Release work: `/home/aetheri/deps/nexaui-desktop-release-c2d8d3d0c244`.
Release library SHA256:
`918891d0b9f0089b4a82f1c5ccd038927d451405a4099f79c8bfa6072ddc1272`.

The common probe has 43 successful rows. It verifies six pkg-config versions,
their selected files/prefixes, protocol location and scanner version; generates
and compiles xdg-shell, both text-input revisions, decoration, fractional scale
and viewporter; then compiles/runs a real Wayland/XKB/FFI join. The probe is not
normal app/device admission or complete keyboard-layout management.

## Instrumented build and independent audit

Debug work: `/home/aetheri/deps/nexaui-desktop-debug-bf29bf228030`.
Its original record is
`/root/aetheri-nexaui-debug-bf29bf228030462dbe63993ea15316d4`.
Library SHA256:
`33888419b9c6bc5f46e1c43ae25d404e20ab5f0591e6f11355533cc928a8f2b0`.

Original controller 36215 reports zero for build, model test and text-wrap test,
then exits one in a post-test verifier. That verifier incorrectly expects a
library's own path in its dependency listing. This is not a failed native model
or sanitizer test. Preserve its actual exitcode and original submitted script.
The separate read-only audit exits zero at
`/root/aetheri-sdk-runtime-audit-944e8c80b0d74db3b8051bccbb3997fa`.
It checks both build/artifact inventories, original model/support copies, real
ASan/UBSan ELF references, loader selections, clean test stderr and 12 distinct
dependency ELF hashes. No native test is replayed or failed work recreated.

Debug uses `detect_leaks=1:abort_on_error=1` and UBSan halt/stack traces. Native
package/sanitizer files stay fixed; no instrumentation or policy is weakened.
The pinned Liberation Sans font is the unchanged original GUI file with SHA256
`76d04c18ea243f426b7de1f3ad208e927008f961dc5945e5aad352d0dfde8ee8`.
The tests exercise headless model semantics/layout and wrapping, not graphical
presentation, a physical laptop or an entire screenreader/session workflow.

## Complete evidence and next work

Export: 3,091 files, 30,459,688 bytes, SHA256
`359dc3f8782f4bf4ffff9bcec08f2c403df53d7c5bee3c66fa2fd4f7456ff794`.
Protected copy:
`/root/aetheri-desktop-sdk-evidence-38e5e1f46f4941cbbeac99f4151fe22f/export.tar.gz`.
Host copy: `sdk-evidence-v1.tar.gz`; final receipt:
`sdk-evidence-verified-v2.json` in the host review directory. Every exported file,
all three submitted source inventories, original source archive, all 1,106 source
files in both builds, actual consumer/core compile commands, generated protocols,
SDK bodies/aliases, notices and final artifact hashes are checked. One additional
host-only verification correction uses POSIX paths for Linux compile-command
comparison; it does not rerun guest work or alter the export.

Carrier package inventory is unchanged; SELinux stays enforcing and udev retains
PID 693/invocation `1f3734ab799e47ab91ea4d27abbac917`. Independent map inspection
finds no selected SDK or new NexaUI code still mapped after synchronous tests.
All four unrelated UI-memory files remain untouched. Reobserve VM state before
dispatch: original audio lab UUID `033ac49e-68bb-47a3-9c9c-26a812dc8300`, QEMU 5548/
controller 15559, boot `8b915c0c-ab36-4cce-a8a3-e84e1c12627c`, restricted network.

Next select the genuine SDL/GLES and full wlroots development inputs, rebuild
those SDKs and current Aetheri apps, then run normal GUI/native regressions and
dependency closure. The private candidates are not normal provisioning, device/
service ownership, image or first-version acceptance. No USB build or flash.
