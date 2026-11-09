# libinput 1.32 / Lua 5.5 build — 9 November 2026

## Decision and source evidence

The previous GUI loader check identifies the retained Lua 5.4 dependency in
libinput 1.31.3. Review the [build contract](../../../spec/libinput-build.md).
Lua implements optional input-event filters, not the ordinary input processing
core. The selected wlroots 0.20.2 source contains no `libinput_plugin_*` call;
both libinput versions default autoload off. Nevertheless, keep plugin, mtdev
and tablet support while reviewing an upgrade rather than dropping functionality
solely to remove a library.

The current listed stable upstream tag is 1.32.0, dated 17 September 2026. Compare
both official 1.31.3 and 1.32.0 archives as data. Both use the generic Lua dependency
lookup with version>=5.4, while the official plugin guide names 5.4. Choose a
private 1.32.0/Lua 5.5 candidate and require actual compatibility tests. No source
or soname is relabelled as Lua 5.4 and no normal provider is replaced.

Official archive SHA256:
`7dd6c1ca964c86eb6810ccd6639cda634e68bdca43bf3afec39e94e942fac8d4`,
1,193,963 bytes, 525 regular source files. Tag commit
`5cf833b572f5a33663888da321dce0ba1abd82a2`. Independent reconstruction of every
Git blob/tree matches upstream tag tree
`43b027c997068e990f016c49a4c802c1f8b5de13`. The official archive downloads succeed
even though several browser GitLab release/raw pages are unavailable. A Gitiles
mirror supplies the independent tag metadata; no unverified mirror archive is used.

Primary references: [plugin documentation](https://wayland.freedesktop.org/libinput/doc/latest/lua-plugins.html),
[upstream tag source](https://gitlab.freedesktop.org/libinput/libinput/-/tree/1.32.0),
[tag mirror](https://chromium.googlesource.com/external/gitlab.freedesktop.org/libinput/libinput/+/refs/tags/upstream/1.32.0),
[Gentoo's dated advisory](https://security.gentoo.org/glsa/202608-08).
The advisory's fixed threshold includes the old 1.31.3 baseline; this is not a
claim that it lacks those fixes or an exhaustive current security clearance.

## Private SDK and actual build

The separate registered audio lab remains UUID
`033ac49e-68bb-47a3-9c9c-26a812dc8300`, QEMU 43164/controller 10491,
boot `32baadd1-a444-4aa8-8c32-56eb3ce22c21`, enforcing with restricted networking.
The original P0 carrier is stopped and untouched. Existing reviewed GCC 16.2.1
and glibc 2.43 toolchain is reused; no system package is installed.

Eleven Fedora-signed RPM snapshots provide only build headers, PC metadata,
Meson and Ninja. Complete source/license metadata, dependencies, empty script
lists and full signer/payload proofs are retained. Sysprof capture metadata is
needed by GLib's private PC requirements, not a new runtime profiler. The private
SDK copies independently checked existing library providers and the earlier
patched glibc Lua 5.5 SDK. Old libinput/Lua 5.4 cannot shadow the new tests there.

Fresh root-controlled mount namespaces/tmpfs run compiler/tests as UID/GID1000,
empty groups and NNP. All subprocesses are waited for before complete protected
exports; namespace exit retires scratch mounts. This is build scratch, not a
durable package publication or app recovery protocol.

Local evidence is `local/libinput-review-2026-11-09`:

- v1 correctly finds Lua 5.5 but cannot resolve GLib's sysprof-capture PC entry.
  Its failed output is retained; a separately signed SDK input snapshot adds only
  the missing build metadata/headers.
- v2 configures but compile finds `/usr` headers because pkgconf did not relocate
  the RPM prefixes by default. v3 adds explicit `--define-prefix` and rejects
  ambient/unrelocated include flags. No header dependency or tablet feature is
  disabled to make the build pass.
- v3 completes the full configured build, generic deviceless and utility/version
  tests and private `DESTDIR` staging. It remains separate historical evidence.
- v4 additionally tests matching-version bytecode admission and its real negative
  source counterexample. Original host controller 44928 exits0. Protected output:
  `/root/aetheri-libinput-build-3465b16f0db6481da0ae98a8a0958691`.

The v4 complete 2,465-file export SHA256 is
`3d61b64d9c4e4f122b3f70ff18230483b71c0a247022077793302c28641672b0`.
Every file length/hash and submitted public source/profile matches independently.
The final verifier streams the compressed archive once; repeated backward gzip
lookups are unnecessarily slow. Initial postprocessors also incorrectly expect
an explicit `#undef AUTOLOAD_PLUGINS`; Meson omits an unset key. Correct checks
observe no define and the actual introspected option false. No native result is
changed to repair those verification mistakes.

Candidate library: 637,664 bytes, SHA256
`38dc7da06bb33703b2843ca67731c0485e562a2e1aac6463d55489069f730b07`.
It links Lua 5.5, has no RPATH/RUNPATH, and retains all 309 versioned exported API
symbols from the selected 1.31.3 ELF. Full ABI and input behavior remain unproven.

## Executed tests and important limit

The verbose generic deviceless runner reports 44 pass, 0 skip/fail. Utilities
report 64 pass, 0 skip/fail, and the library-version test passes. Five host archive
controls reject changed bytes, traversal, an unexpected link and a wrong root.
These do not include upstream's full Lua suite: its `no_device` registration is
different from `deviceless` and may create uinput devices. Do not turn the generic
Meson success into a claim of complete plugin or physical-device acceptance.

The own C17 probe loads text containing an actual `_VERSION == 'Lua 5.5'` check,
logs its acceptance and observes zero device opens. Matching Lua 5.5 bytecode is
generated by the selected runtime and rejected by the plugin loader's text-only
mode. The unmodified upstream negative vector is precompiled 5.4, so rejection
of that vector alone could merely reflect a bytecode-version mismatch.

A separate copied source changes exactly `luaL_loadfilex(..., "t")` to unrestricted
mode and rebuilds its own library. The same probe now reports
`MATCHING_BYTECODE_ACCEPTED` and exits 1. That counter library is test-only, never
staged/installed or selected as the candidate. Both outcomes are independently
checked in the export. This establishes one guard, not complete sandbox safety.

## Next

Run the full upstream Lua/input cases in a new root-only disposable virtual-device
fixture with protected udev/rule/device/process lifetimes and no normal input
admission. Add sanitizer and actual compositor/input regressions before replacing
the old runtime library/notice or claiming a maintained whole-desktop Lua closure.
The PCRE2 review, normal service/session/device ownership, physical hardware and
the remainder of the OS/app/installer/experiment goal stay open.

Preserve all failed archives and exact completed receipts. The lab remains live
under its original controller for the next tests. Normal manifests, original
carrier, USB user documents and foreign planning/UI files remain untouched.
NexaUI handoff stays closed; the full goal remains active.

Independent same-boot inspection finds no executable/working directory under any
of the four build scratch roots, and enforcing remains1. All four foreign file
hashes match the initial snapshot. This is administrative build-process evidence,
not production app/descendant/cgroup/lease settlement.

## Explicit maintained runtime projection

After the separately recorded functional, memory and actual DRM input checks,
`spec/libinput-runtime-inputs.json` now independently selects one component for
subsequent desktop builds. The [runtime contract](../../spec/libinput-runtime.md)
and preparer bind63 regular files/1,124,239 bytes plus one soname alias: the actual
tested Release-v5 library, three matching Release helpers/two udev rules, all54
source quirks, Lua5.5 and both original MIT notices. No development tool/header,
test program, debug runner, interpreter or daemon enters this projection.

Both original Release-v4/v5 complete2,465-file exports are rechecked. Their equal
637,664-byte libraries have different hashes; select v5's previously exercised
`ca4472b257663239f9356e5ec07fcd1983193311cf755d325c7006c94cc69b9b` and matching
helper artifacts. Never substitute a merely equal version or the larger
debugoptimized/instrumented library. Source/archive and Lua SDK pins remain
independent of the component's own receipt. The existing Wacom database stays
with its original owner in the normal desktop manifest.

`prepare_libinput_runtime.py` verifies inputs before creating fresh output,
rejects aliases/overlap, preserves partial failures and refuses output adoption.
Its checker accounts for directories, exact aliases/modes, notices and receipt
origin. The derived desktop manifest removes55 original libinput/Lua files,
adds63 and preserves all1,371 unrelated entries exactly. It has1,434 files and
explicitly records source-build origins instead of those two removed RPMs.
The old manifest lacks libinput's standalone COPYING notice; the new component
includes it. This observation is not a complete release-compliance audit.

Desktop and regular-USB image builders now accept explicit `--input-runtime`.
With no argument the original manifest bytes/providers remain unchanged. After
candidate payload copy, a static readelf pass rejects retained old Lua linkage
and checks the selected input library's genuine5.5 dependency/no RPATH. It writes
the exact dependency audit. The complete image audit is wired but not executed
here: only the component's five actual ELFs and original old-ABI counter run.
No full new image or current whole-desktop build/boot is claimed.

Native work uses fresh `/run/a-lrt-cd92722c2cf3`, protected inputs at
`/root/aetheri-libinput-runtime-inputs-cd92722c2cf34024a2a876d7fe0744e6`.
The first host wrapper fails before dispatch at Windows' command-line limit;
the final wrapper carries its complete hash-bound input inventory through stdin
instead. It never replays an uncertain guest attempt. All234 input files match
their original selected host bundle.

All8 native projection tests pass without skips as UID1000, including actual
copy/overlay, unchanged unrelated owners, changed code/data/rules/notices/receipt,
alias/mode drift, rejected changed input before output, existing-tree refusal
and actual ELF old-Lua rejection. Preparation, manifest creation, selected-loader
listing and the existing independent Lua C probe all exit0. Loader initialization
actually selects both libraries from the candidate prefix; text reports Lua505
with zero device-open calls, and matching5.5 bytecode is rejected. This is a
library/loader probe, not an actual compositor run of the newly projected prefix.

The complete314-file export is checked:2,877,201 bytes, SHA256
`8cce0218b0844fa7c1e58743d2eb12c254f791a3efd392499f93a276c1d98910`, protected at
`/root/aetheri-libinput-runtime-evidence-43260c9ad53646dd99952ed66dcfc6f1/export.tar.gz`.
The alias is separately recorded and checked; every regular member and original
input matches. Independent end inspection finds no candidate mapping or test
unit. Original udev PID693/invocation remains unchanged and enforcing stays1.
The same original QEMU5548/controller15559/boot remains live. On Windows the three
pure profile tests pass with five Linux cases skipped; the existing USB6,
desktop-controller7, payload7 and Sound-runtime8 controls pass (one Linux Sound
case skipped). These host checks do not replace the8 native cases above.

The retained candidate at `/run/a-lrt-cd92722c2cf3/owned/candidate` is user-owned
development output, never privileged admission. Before any privileged consumption
select/protect/check its inputs independently. Next join an explicitly protected
projected prefix to actual compositor/session and connected Sound tests, then
stage the full native SDK/tool inputs and rebuild/test the current complete
desktop. The full image ELF audit, normal device/session ownership, PCRE2 review,
physical HP and broader OS/app/installer/experiment work remain open. Normal
manifests, old carrier, physical stick and foreign UI notes remain unchanged.

## Protected projection in actual DRM and shared Sound

The explicit projected input mode now passes actual DRM/PAM/logind input and
connected Sound. `check_protected` repeats the complete independent profile and
requires root ownership/non-writable-by-others parents. This is a lab prerequisite,
not service/volume admission or proof that chown retired old writers. Preparation
uses fresh root-created copies from the checked artifact export; the previous
user-owned prefix is never adopted. The original build mode remains supported.

Protected preparation v1 stops before services because raw Windows CRLF profile
bytes differ from original Linux receipt hashes. The locally normalized LF hash
matches the original receipt precisely. Preserve that failed tree and output;
do not relax the checker or alter its receipt. Fresh v2 copies canonical Git LF
code/profiles and passes all source/runtime checks. Its protected stage is
`/root/aetheri-projected-integration-5c87216edc4e4248b56169262b5a0b5b`, source
`/run/a-lrs-5c87216edc4e`, component `/run/a-lrp-5c87216edc4e`. The original GUI80
and audio artifact bytes remain independently checked and unchanged.

Native projection9, Sound-control8 and original-GUI-runtime9 tests pass without
skips. Additional actual selector checks accept the projected and legacy-default
modes, reject an unknown mode and reject the user-owned prepared prefix. The
default Sound environment/policy rejection controls execute without any bus effect.

Projected DRM controller74904 exits0. Original unit
`aetheri-input-c1e7eef222744dd9983ff91c3a8ff65a.service`, invocation
`074792cce7d64ab69c483b473bd7c8ad`, retires its original actors, service/scope/
session and input baseline. Actual key30 press/release produces97 (`a`); motion
and button272 press/release arrive through PS2/libinput/Wayland. Actual maps
select the component's Release input library and Lua5.5, with matching quirks and
no libinput error. The same independently checked normal keyboard-v3 compositor
artifact is used; this remains separate from a current full desktop rebuild.

Shared Sound controller26978 exits0. Its fixture root is
`/run/a-audio-8c4eb56adb7c`; compositor61487 and policy61462/invocation
`2657514032fa4852bc2b68a600e8ade9` actually map the same
`input/usr/lib64/liblua-5.5.so` within that root. Both raw mapping records are
exported and compared independently. The separately checked rebuilt module stays
in the policy prefix. GUI invocation is `5c9be43b2635457192d49850f0d46414`.
Pointer/keyboard volume control, raw stereo readback `[0.117649,0.117649]` for
UI0.489999995, mute/unmute and source-loss fencing pass. Native observer/input-FD,
original process/unit/cgroup/target/session retirement completes before fixture
unmount and unchanged temporary policy removal. No new MAC permission is added.

A separate no-option default RPM Sound run/controller48675 also exits0 and
retires completely. It uses original policy63015/invocation
`adc1f1dfd5114dfba9cd227670f20083`, GUI invocation
`591e520b75a444cba746f09c0129855c`, with the original Lua5.4 policy mapping checked
by the live controller. Its compositor raw maps are not exported by that default
branch; do not invent such evidence. Shared and default runs have identical
readback/mute/loss results and all12 corresponding raw capture hashes match.
These24 files are GUI images, not PCM recordings. All compressed/raw hashes and
image decoding are checked; the four shared primary states are actually viewed.

The complete727-file export is checked:3,356,270 bytes, SHA256
`522b4c07a1f4929b384d5b511ef3696aebdde1258aaa87d270d2d765ebcc1b23`, protected at
`/root/aetheri-projected-input-evidence-ea5f706d11be4bd68c319937b003c28b/export.tar.gz`.
Both original submitted input inventories match. The first host verifier wrongly
expects a raw compositor-map file in the default branch; inspect its actual
contract and verify the immutable existing export without replaying either run.
The corrected verifier uses the default controller's actual Lua observation and
the shared raw maps, preserving that evidence distinction.

Independent end inspection finds no selected mapping, test unit or PAM profile;
tty1 and the original udev PID693/invocation remain, enforcing stays1 and both
temporary policy modules are absent. The two Sound fixture mounts ended. Failed
and protected source/component/program inputs deliberately remain; never adopt
the failed v1 preparation. Four foreign UI note hashes remain unchanged. Original
QEMU5548/controller15559 stays live on boot8b915c0c-ab36-4cce-a8a3-e84e1c12627c.
Next stage the required full native SDK/tool inputs and rebuild/test the current
complete desktop and full image ELF closure. Normal image/provider selection,
PCRE2, production device/session ownership, physical hardware and broader OS work
remain open. The USB and old quarantined P0 remain untouched; NexaUI handoff stays closed.
